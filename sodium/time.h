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
            event(T t_, SODIUM_SHARED_PTR<impl::node> sAlarm_)
                : t(t_), sAlarm(sAlarm_)
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
            SODIUM_SHARED_PTR<impl::node> sAlarm;
            long long seq;  // Used to guarantee uniqueness
        };

        template <typename T>
        class timer_system_base {
        public:
            timer_system_base(
                cell<T> time_,
                std::shared_ptr<timer_system_impl<T>> impl_,
                std::shared_ptr<priority_queue<event<T>>> event_queue_
            ) : time(time_), impl(impl_), event_queue(event_queue_) {}

        private:
            struct at_state {
                at_state() : sampled(false) {}
                boost::optional<event<T>> current;
                boost::optional<std::function<void()>> cancel_current;
                bool sampled;
                boost::optional<T> tAl;
                void do_cancel(const std::shared_ptr<priority_queue<event<T>>>& event_queue)
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

                SODIUM_TUPLE<stream_,SODIUM_SHARED_PTR<node> > p = unsafe_new_stream();

                SODIUM_SHARED_PTR<node> sAlarm = std::get<1>(p);

                std::shared_ptr<at_state> state(new at_state);
                const auto& impl_(this->impl);
                const auto& event_queue_(this->event_queue);
                auto updateTimer = [state, impl_, event_queue_, tAlarm, sAlarm] (sodium::impl::transaction_impl*) {
                    state->do_cancel(event_queue_);
                    if (!state->sampled) {
                        state->sampled = true;
                        state->tAl = tAlarm.sample();
                    }
                    if (state->tAl) {
                        const auto& tAl_ = state->tAl.get();
                        state->current = boost::make_optional(event<T>(tAl_, sAlarm));
                        event_queue_->push(state->current.get());
                        state->cancel_current = impl_->set_timer(tAl_, [] () {
                                    // Open and close a transaction to trigger queued
                                    // events to run.
                                    transaction trans;
                                    trans.close();
                                });
                    }
                };

                auto kill = tAlarm.updates().listen_raw(trans0.impl(), sAlarm,
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [state, updateTimer, event_queue_] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans1, const light_ptr& ptr) {
                            state->tAl = *ptr.cast_ptr<boost::optional<T>>(nullptr);
                            state->sampled = true;
                            updateTimer(trans1);
                        }), false);

                trans0.prioritized(sAlarm, updateTimer);
                auto sa = SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill,
                    new std::function<void()>([state, event_queue_] () {
                        state->do_cancel(event_queue_);
                    }));
                trans0.close();
                return sa;
            }
            cell<T> time;
        private:
            std::shared_ptr<timer_system_impl<T>> impl;
            std::shared_ptr<priority_queue<event<T>>> event_queue;
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
                    if (o_event) {
                        const auto& e = o_event.get();
                        // Two separate transactions
                        time_snk.send(e.t);
                        {
                            transaction trans1;
                            send(e.sAlarm, trans1.impl(), light_ptr::create<T>(e.t));
                        }
                    }
                    else
                        break;
                }
                time_snk.send(t);
            });
            return impl::timer_system_base<T>(time_snk, impl, q);
        }
    };

    template <typename T>
    stream<T> periodic_timer(const timer_system<T>& ts, const cell<T>& period) {
        transaction trans;
        using namespace boost;
        cell_loop<optional<T>> tAlarm;
        stream<T> sAlarm = ts.at(tAlarm);
        tAlarm.loop(
            sAlarm.snapshot(
                period,
                [] (const T& t, const T& p) {
                    return optional<T>(t + p);
                })
            .hold_lazy(ts.time.sample_lazy().map([] (const T& t) { return optional<T>(t); })));
        return sAlarm;
    }
}  // end namespace sodium
#endif
