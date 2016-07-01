#ifndef _SODIUM_TIME_AT_H_
#define _SODIUM_TIME_AT_H_

#include <sodium/sodium.h>
#include <memory>
#include <boost/optional.hpp>

namespace sodium {
    /*!
     * A timer that fires at the specified time.
     * The T class provides the implementation of a timer system.
     */
    template <class T>
    stream<typename T::time> at(const T& timeSys, const cell<boost::optional<typename T::time>>& tAlarm) {
        using namespace boost;
        std::shared_ptr<optional<typename T::timer>> current(new optional<typename T::timer>);
        stream_sink<typename T::time> eOut;
        auto kill = tAlarm.value().listen([current, eOut, timeSys] (const optional<typename T::time>& ot) {
            if (*current)
                timeSys.cancel_timer(current->get());
            if (ot) {
                typename T::time t = ot.get();
                *current = optional<typename T::timer>(
                                timeSys.set_timer(ot.get(), [eOut, t] () { eOut.send(t); })
                            );
            }
            else
                *current = optional<typename T::timer>();
        });
        return eOut.add_cleanup(kill);
    }

    namespace impl {
        // TO DO: Revisit.
        // Ideally the clock time would get frozen at the start of the transaction.
        template <class T>
        struct cell_impl_time : cell_impl {
            cell_impl_time(const T& timeSys) : timeSys(timeSys), now(NULL) {}
            ~cell_impl_time() { delete now; }
            T timeSys;
            light_ptr* now;
            virtual const light_ptr& sample() const {
                auto me = const_cast<cell_impl_time<T>*>(this);
                if (now == NULL)
                    me->now = new light_ptr(light_ptr::create<typename T::time>(timeSys.now()));
                else
                    *me->now = light_ptr::create<typename T::time>(timeSys.now());
                return *now;
            }
            virtual const light_ptr& newValue() const { return sample(); }
        };
    }

    /*!
     * A cell that always has the current clock time.
     */
    template <class T>
    cell<typename T::time> clock(const T& t)
    {
        return cell<typename T::time>(SODIUM_SHARED_PTR<impl::cell_impl>(
            new impl::cell_impl_time<T>(t)
        ));
    }

    template <class T>
    stream<typename T::time> periodic_timer(const T& t, const cell<typename T::time>& period) {
        transaction trans;
        using namespace boost;
        cell_loop<optional<typename T::time>> tAlarm;
        stream<typename T::time> eAlarm = at(t, tAlarm);
        cell<typename T::time> now = clock(t);
        tAlarm.loop(
            eAlarm.snapshot(
                period,
                [] (const typename T::time& t, const typename T::time& p) {
                    return optional<typename T::time>(t + p);
                })
            .hold(optional<typename T::time>(now.sample())));
        return eAlarm;
    }
}  // end namespace sodium
#endif
