#ifndef _SODIUM_TIME_AT_H_
#define _SODIUM_TIME_AT_H_

#include <sodium/sodium.h>
#include <memory>
#include <boost/optional.hpp>
#include <queue>

namespace sodium {
    namespace impl {
        template <typename T>
        class priority_queue : public std::priority_queue<T, std::vector<T>>
        {
            public:
                // To do: Improve performance
                bool remove(const T& value) {
                    auto it = std::find(this->c.begin(), this->c.end(), value);
                    if (it != this->c.end()) {
                        this->c.erase(it);
                        return true;
                    }
                    else
                        return false;
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
                seq = ++next_seq;
            }
            // Note: This is really a greater-than operation in terms of the numbers.
            // So the "greatest" element is the earliest time.
            // This means that std::priority_queue will give us the earliest alarm
            // for top() and pop().
            bool operator < (const event& other) const {
                if (t > other.t) return true;
                if (t < other.t) return false;
                return seq > other.seq;
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
                std::shared_ptr<timer_system_impl<T>> impl_,
                std::shared_ptr<priority_queue<event<T>>> event_queue_
            ) : time(time_), impl(impl_), event_queue(event_queue_), lock(new mutex) {}

        private:
            struct at_state {
                at_state() {}
                boost::optional<event<T>> current;
                boost::optional<std::function<void()>> cancel_current;
                boost::optional<T> tAl;
                void do_cancel(const std::shared_ptr<priority_queue<event<T>>>& event_queue,
                               const std::shared_ptr<mutex>& lock)
                {
                    if (this->cancel_current) {
                        this->cancel_current.get()();
                        lock->lock();
                        event_queue->remove(this->current.get());
                        lock->unlock();
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

                std::shared_ptr<at_state> state(new at_state);
                const auto& impl_(this->impl);
                const auto& event_queue_(this->event_queue);
                const auto& lock_(this->lock);
                auto kill = tAlarm.value().listen(
                        [state, impl_, event_queue_, lock_, tAlarm, sAlarm_snk] (const boost::optional<T>& o_tAl) {
                    state->do_cancel(event_queue_, lock_);
                    if (o_tAl) {
                        const auto& tAl = o_tAl.get();
                        state->current = boost::make_optional(event<T>(tAl, sAlarm_snk));
                        lock_->lock();
                        event_queue_->push(state->current.get());
                        lock_->unlock();
                        state->cancel_current = impl_->set_timer(tAl, [] () {
                                    // Open and close a transaction to trigger queued
                                    // events to run.
                                    transaction trans;
                                    trans.close();
                                });
                    }
                });
                auto sa = sAlarm_snk.add_cleanup([kill, state, event_queue_, lock_] () {
                    kill();
                    state->do_cancel(event_queue_, lock_);
                });
                trans0.close();
                return sa;
            }
            cell<T> time;
        private:
            std::shared_ptr<timer_system_impl<T>> impl;
            std::shared_ptr<priority_queue<event<T>>> event_queue;
            std::shared_ptr<mutex> lock;
        };
    }

    template <typename T>
    class timer_system : public impl::timer_system_base<T>
    {
    public:
        timer_system(std::shared_ptr<timer_system_impl<T>> impl_)
            : impl::timer_system_base<T>(construct(std::move(impl_)))
        {
        }
    private:
        static impl::timer_system_base<T> construct(std::shared_ptr<timer_system_impl<T>> impl) {
            transaction trans0;
            cell_sink<T> time_snk(impl->now());
            SODIUM_SHARED_PTR<impl::priority_queue<impl::event<T>>> q(new impl::priority_queue<impl::event<T>>);
            trans0.on_start([impl, time_snk, q] () {
                T t = impl->now();
                lock->lock();
                while (true) {
                    boost::optional<impl::event<T>> o_event;
                    if (!q->empty()) {
                        const impl::event<T>& e = q->top();
                        if (e.t <= t) {
                            o_event = boost::optional<impl::event<T>>(e);
                            q->pop();
                            // TO DO: Detect infinite loops!
                        }
                    }
                    lock->unlock();
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
            return impl::timer_system_base<T>(time_snk, impl, q);
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
