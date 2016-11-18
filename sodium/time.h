#ifndef _SODIUM_TIME_AT_H_
#define _SODIUM_TIME_AT_H_

#include <sodium/sodium.h>
#include <memory>
#include <boost/optional.hpp>
#include <set>

namespace sodium {
    namespace impl {
        // Note: Smallest value is considered to be highest priority, which is
        // the opposite to std::thread_safe_priority_queue.
        template <typename T>
        class thread_safe_priority_queue
        {
            private:
#if !defined(SODIUM_SINGLE_THREADED)
                std::mutex lock;
#endif
                std::set<T> c;

            public:
                void push(const T& value) {
#if !defined(SODIUM_SINGLE_THREADED)
                    lock.lock();
#endif
                    c.insert(value);
#if !defined(SODIUM_SINGLE_THREADED)
                    lock.unlock();
#endif
                }
                template <class Fn>
                boost::optional<T> pop_if(Fn f) {
#if !defined(SODIUM_SINGLE_THREADED)
                    lock.lock();
#endif
                    if (!c.empty() && f(*c.begin())) {
                        boost::optional<T> ret(*c.begin());
                        c.erase(c.begin());
#if !defined(SODIUM_SINGLE_THREADED)
                        lock.unlock();
#endif
                        return ret;
                    }
#if !defined(SODIUM_SINGLE_THREADED)
                    lock.unlock();
#endif
                    return boost::optional<T>();
                }
                bool remove(const T& value) {
#if !defined(SODIUM_SINGLE_THREADED)
                    lock.lock();
#endif
                    auto it = c.find(value);
                    if (it == c.end()) {
#if !defined(SODIUM_SINGLE_THREADED)
                        lock.unlock();
#endif
                        return false;
                    }
                    else {
                        c.erase(it);
#if !defined(SODIUM_SINGLE_THREADED)
                        lock.unlock();
#endif
                        return true;
                    }
                }
        };
    };

    template <typename T>
    struct timer_system_impl {
        /*!
         * Set a timer that will execute the specified callback at the specified time.
         * @return A function that can be used to cancel the timer.
         */
        virtual std::function<void()> set_timer(T t, std::function<void()> callback) = 0;

        /**
         * Return the current clock time.
         */
        virtual T now() = 0;
    };

    namespace impl {

        extern long long next_seq;

        template <typename T>
        struct event {
            event(T t_, stream_sink<T> sAlarm_snk_)
                : t(t_), sAlarm_snk(sAlarm_snk_)
            {
                // This is called inside a listen() handler and this is under a global
                // transaction lock, so there can't be any race conditions.
                seq = ++next_seq;
            }
            bool operator < (const event& other) const {
                if (t < other.t) return true;
                if (t > other.t) return false;
                return seq < other.seq;
            }
            bool operator == (const event& other) const {
                return seq == other.seq;
            }
            T t;
            stream_sink<T> sAlarm_snk;
            long long seq;  // Used to guarantee uniqueness
        };

        template <typename T>
        class timer_system_base {
        public:
            timer_system_base(
                cell<T> time_,
                SODIUM_SHARED_PTR<timer_system_impl<T>> impl_,
                SODIUM_SHARED_PTR<thread_safe_priority_queue<event<T>>> event_queue_
            ) : time(time_), impl(impl_), event_queue(event_queue_)
            {}

        private:
            struct at_state {
                at_state() {}
                boost::optional<event<T>> current;
                boost::optional<std::function<void()>> cancel_current;
                boost::optional<T> tAl;
                void do_cancel(const SODIUM_SHARED_PTR<thread_safe_priority_queue<event<T>>>& event_queue)
                {
                    if (this->cancel_current) {
                        this->cancel_current.get()();
                        event_queue->remove(this->current.get());
                    }
                    this->cancel_current = boost::optional<std::function<void()>>();
                    this->current = boost::optional<event<T>>();
                }
            };

        public:
            stream<T> at(cell<boost::optional<T>> tAlarm) const
            {
                transaction trans0;

                stream_sink<T> sAlarm_snk;

                SODIUM_SHARED_PTR<at_state> state(new at_state);
                const auto& impl_(this->impl);
                const auto& event_queue_(this->event_queue);
                auto kill = tAlarm.value().listen(
                        [state, impl_, event_queue_, tAlarm, sAlarm_snk] (const boost::optional<T>& o_tAl) {
                    state->do_cancel(event_queue_);
                    if (o_tAl) {
                        const auto& tAl = o_tAl.get();
                        state->current = boost::make_optional(event<T>(tAl, sAlarm_snk));
                        event_queue_->push(state->current.get());
                        state->cancel_current = impl_->set_timer(tAl, [] () {
                                    // Open and close a transaction to trigger queued
                                    // events to run.
                                    transaction trans;
                                    trans.close();
                                });
                    }
                });
                auto sa = sAlarm_snk.add_cleanup([kill, state, event_queue_] () {
                    kill();
                    state->do_cancel(event_queue_);
                });
                trans0.close();
                return sa;
            }
            cell<T> time;
        private:
            SODIUM_SHARED_PTR<timer_system_impl<T>> impl;
            SODIUM_SHARED_PTR<thread_safe_priority_queue<event<T>>> event_queue;
        };
    }

    template <typename T>
    class timer_system : public impl::timer_system_base<T>
    {
    public:
        timer_system(SODIUM_SHARED_PTR<timer_system_impl<T>> impl_)
            : impl::timer_system_base<T>(construct(std::move(impl_)))
        {
        }
    private:
        static impl::timer_system_base<T> construct(SODIUM_SHARED_PTR<timer_system_impl<T>> impl) {
            transaction trans0;
            cell_sink<T> time_snk(impl->now());
            SODIUM_SHARED_PTR<impl::thread_safe_priority_queue<impl::event<T>>> event_queue(
                new impl::thread_safe_priority_queue<impl::event<T>>);
            SODIUM_SHARED_PTR<std::mutex> lock(new std::mutex);
            trans0.on_start([impl, time_snk, event_queue, lock] () {
                T t = impl->now();
                while (true) {
                    boost::optional<impl::event<T>> o_event = event_queue->pop_if(
                        [t] (const impl::event<T>& e) { return e.t <= t; });
                    if (o_event) {
                        const auto& e = o_event.get();
                        // Two separate transactions
                        time_snk.send(e.t);
                        e.sAlarm_snk.send(e.t);
                    }
                    else
                        break;
                    lock->lock();
                }
                time_snk.send(t);
            });
            return impl::timer_system_base<T>(time_snk, impl, event_queue);
        }
    };

    template <typename T>
    stream<T> periodic_timer(const timer_system<T>& sys, const cell<boost::optional<T>>& period) {
        transaction trans;
        using namespace boost;
        cell_loop<optional<T>> tAlarm;
        stream<T> sAlarm = sys.at(tAlarm);
        cell<T> t_zero = sAlarm.hold(sys.time.sample());
        tAlarm.loop(
            t_zero.lift(period, [] (T t0, const boost::optional<T>& o_per) {
                return o_per ? boost::optional<T>(t0 + o_per.get())
                             : boost::optional<T>();
            }));
        return sAlarm;
    }
}  // end namespace sodium
#endif
