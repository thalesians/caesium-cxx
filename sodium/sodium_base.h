// $Id$

#ifndef _SODIUM_BASE_H_
#define _SODIUM_BASE_H_

#include <sodium/config.h>
#include <sodium/light_ptr.h>
#include <sodium/count_set.h>
#include <sodium/lock_pool.h>
#include <boost/optional.hpp>
#include <boost/intrusive_ptr.hpp>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <forward_list>
#if defined(SODIUM_NO_EXCEPTIONS)
#include <stdlib.h>
#else
#include <stdexcept>
#endif
#include <vector>

#define SODIUM_CONSTANT_OPTIMIZATION

namespace sodium {

    template <typename A> class stream;
    template <typename A> class cell;
    template <typename A> class cell_sink;
    template <typename A> class cell_loop;
    template <typename A> class stream_loop;
    template <typename A> class stream_sink;
    template <typename A, typename Selector> class router;
    template <typename A, typename B>
    cell<B> apply(const cell<std::function<B(const A&)>>& bf,
                  const cell<A>& ba);
    template <typename A>
    stream<A> filter_optional(const stream<boost::optional<A>>& input);
    template <typename A> stream<A> split(const stream<std::list<A>>& e);
    template <typename A> stream<A> switch_s(const cell<stream<A>>& bea);
    template <typename T> cell<typename T::time> clock(const T& t);

    template <typename A> class lazy {
    private:
        std::function<A()> f;

    public:
        lazy(const A& a) : f([a]() -> A { return a; }) {}
        lazy(const std::function<A()>& f_) : f(f_) {}
        A operator()() const { return f(); }

        template <typename Fn>
        lazy<typename std::result_of<Fn(A)>::type> map(const Fn& fn) const {
            typedef typename std::result_of<Fn(A)>::type B;
            const auto& a(*this);
            return lazy<B>([fn, a]() { return fn(a()); });
        }

        template <typename B, typename Fn>
        lazy<typename std::result_of<Fn(A, B)>::type> lift(const lazy<B>& b,
                                                           const Fn& fn) const {
            typedef typename std::result_of<Fn(A, B)>::type C;
            const auto& a(*this);
            return lazy<C>([fn, a, b]() { return fn(a(), b()); });
        }

        template <typename B, typename C, typename Fn>
        lazy<typename std::result_of<Fn(A, B, C)>::type> lift(
            const lazy<B>& b, const lazy<C>& c, const Fn& fn) const {
            typedef typename std::result_of<Fn(A, B, C)>::type D;
            const auto& a(*this);
            return lazy<D>([fn, a, b, c]() { return fn(a(), b(), c()); });
        }

        template <typename B, typename C, typename D, typename Fn>
        lazy<typename std::result_of<Fn(A, B, C, D)>::type> lift(
            const lazy<B>& b, const lazy<C>& c, const lazy<D>& d,
            const Fn& fn) const {
            typedef typename std::result_of<Fn(A, B, C, D)>::type E;
            const auto& a(*this);
            return lazy<E>(
                [fn, a, b, c, d]() { return fn(a(), b(), c(), d()); });
        }
    };

    namespace impl {
        class holder;
        class node;
        class transaction_impl;
        template <typename Allocator>
        struct listen_impl_func {
            typedef std::function<std::function<void()>*(
                transaction_impl*,
                const std::shared_ptr<impl::node>&,
                const SODIUM_SHARED_PTR<holder>&,
                bool)> closure;
            listen_impl_func(closure* func_)
                : func(func_) {}
            ~listen_impl_func()
            {
                assert(cleanups.begin() == cleanups.end() && func == NULL);
            }
            count_set counts;
            closure* func;
            SODIUM_FORWARD_LIST<std::function<void()>*> cleanups;
            inline void update_and_unlock(spin_lock* l) {
                if (func && !counts.active()) {
                    counts.inc_strong();
                    l->unlock();
                    for (auto it = cleanups.begin(); it != cleanups.end(); ++it) {
                        (**it)();
                        delete *it;
                    }
                    cleanups.clear();
                    delete func;
                    func = NULL;
                    l->lock();
                    counts.dec_strong();
                }
                if (!counts.alive()) {
                    l->unlock();
                    delete this;
                }
                else
                    l->unlock();
            }
        };

        struct H_STREAM {};
        struct H_STRONG {};
        struct H_NODE {};

        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_STREAM>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_STREAM>* p);
        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p);
        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p);

        inline bool alive(const boost::intrusive_ptr<listen_impl_func<H_STRONG> >& li) {
            return li && li->func != NULL;
        }

        inline bool alive(const boost::intrusive_ptr<listen_impl_func<H_STREAM> >& li) {
            return li && li->func != NULL;
        }
    
        struct listen_handler
        {
            listen_handler() {}
            virtual ~listen_handler() {}
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a) = 0;
        };

        class cell_;
        struct cell_impl;

        class stream_ {
            friend class cell_;
            friend class switch_entry;
            friend class switch_s_handler;
            friend class switch_c_handler;
            template <typename A> friend class sodium::stream;
            template <typename A> friend class sodium::stream_loop;
            template <typename A> friend class sodium::cell;
            friend cell_ switch_c(transaction_impl* trans, const cell_& bba);
            friend SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0,
                                                     const light_ptr& initValue,
                                                     const stream_& input);
            friend SODIUM_SHARED_PTR<cell_impl> hold_lazy(
                transaction_impl* trans0,
                const std::function<light_ptr()>& initValue,
                const stream_& input);
            template <typename A, typename B>
            friend cell<B> sodium::apply(
                const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
            friend cell_ apply(transaction_impl* trans0, const cell_& bf,
                               const cell_& ba);
            friend stream_ map_(
                transaction_impl* trans,
                listen_handler* impl,
                const stream_& ev);
            friend cell_ map_(
                transaction_impl* trans,
                const std::function<light_ptr(const light_ptr&)>& f,
                const cell_& beh);
            friend stream_ switch_s(transaction_impl* trans, const cell_& bea);
            template <typename A>
            friend stream<A> sodium::split(const stream<std::list<A>>& e);
            friend stream_ filter_optional_(
                transaction_impl* trans, const stream_& input,
                std::function<boost::optional<light_ptr>(const light_ptr&)> f);
            template <typename A, typename Selector>
            friend class sodium::router;

        protected:
            boost::intrusive_ptr<listen_impl_func<H_STREAM>> p_listen_impl;

        public:
            stream_();
            stream_(
                boost::intrusive_ptr<listen_impl_func<H_STREAM>> p_listen_impl_)
                : p_listen_impl(std::move(p_listen_impl_)) {}

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            bool is_never() const { return !impl::alive(p_listen_impl); }
#endif

        protected:
            /*!
             * listen to streams.
             */
            std::function<void()>* listen_raw(
                transaction_impl* trans,
                const SODIUM_SHARED_PTR<impl::node>& target,
                listen_handler* handle,
                bool suppressEarlierFirings) const;

            std::function<void()>* listen_raw(
                transaction_impl* trans,
                const SODIUM_SHARED_PTR<impl::node>& target,
                bool suppressEarlierFirings) const;

            /*!
             * This is far more efficient than add_cleanup because it modifies
             * the stream in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup) {
                boost::intrusive_ptr<listen_impl_func<H_STRONG>> li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(
                        p_listen_impl.get()));
                if (cleanup != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup);
                    else {
                        (*cleanup)();
                        delete cleanup;
                    }
                }
                return *this;
            }

            /*!
             * This is far more efficient than add_cleanup because it modifies
             * the stream in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1,
                                       std::function<void()>* cleanup2) {
                boost::intrusive_ptr<listen_impl_func<H_STRONG>> li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(
                        p_listen_impl.get()));
                if (cleanup1 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup1);
                    else {
                        (*cleanup1)();
                        delete cleanup1;
                    }
                }
                if (cleanup2 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup2);
                    else {
                        (*cleanup2)();
                        delete cleanup2;
                    }
                }
                return *this;
            }

            /*!
             * This is far more efficient than add_cleanup because it modifies
             * the stream in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1,
                                       std::function<void()>* cleanup2,
                                       std::function<void()>* cleanup3) {
                boost::intrusive_ptr<listen_impl_func<H_STRONG>> li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(
                        p_listen_impl.get()));
                if (cleanup1 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup1);
                    else {
                        (*cleanup1)();
                        delete cleanup1;
                    }
                }
                if (cleanup2 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup2);
                    else {
                        (*cleanup2)();
                        delete cleanup2;
                    }
                }
                if (cleanup3 != NULL) {
                    if (alive(li))
                        li->cleanups.push_front(cleanup3);
                    else {
                        (*cleanup3)();
                        delete cleanup3;
                    }
                }
                return *this;
            }

            /*!
             * Create a new stream that is like this stream but has an extra
             * cleanup.
             */
            stream_ add_cleanup_(transaction_impl* trans,
                                 std::function<void()>* cleanup) const;
            cell_ hold_(transaction_impl* trans, const light_ptr& initA) const;
            cell_ hold_lazy_(transaction_impl* trans,
                             const std::function<light_ptr()>& initA) const;
            stream_ once_(transaction_impl* trans) const;
            stream_ merge_(transaction_impl* trans, const stream_& other) const;
            template <typename A, typename F>
            stream_ coalesce_(transaction_impl* trans, F combine) const;
            stream_ last_firing_only_(transaction_impl* trans) const;
            stream_ snapshot_(transaction_impl* trans, cell_ beh,
                              std::function<light_ptr(const light_ptr&,
                                                      const light_ptr&)>
                                  combine) const;
            stream_ filter_(
                transaction_impl* trans,
                std::function<bool(const light_ptr&)> pred) const;

            std::function<void()>* listen_impl(
                transaction_impl* trans,
                const SODIUM_SHARED_PTR<impl::node>& target,
                SODIUM_SHARED_PTR<holder> h,
                bool suppressEarlierFirings) const {
                boost::intrusive_ptr<listen_impl_func<H_STRONG>> li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(
                        p_listen_impl.get()));
                if (alive(li))
                    return (*li->func)(trans, target, h,
                                       suppressEarlierFirings);
                else
                    return NULL;
            }
        };

        template <typename A, typename F>
        light_ptr detype(F f, light_ptr a)
        {
            typedef typename std::result_of<F(A)>::type B;
            return light_ptr::create<B>(f(*a.cast_ptr<A>(NULL)));
        }

        template <typename A, typename B, typename F>
        light_ptr detype2(F f, light_ptr a, light_ptr b)
        {
            typedef typename std::result_of<F(A,B)>::type C;
            return light_ptr::create<C>(f(*a.cast_ptr<A>(NULL), *b.cast_ptr<B>(NULL)));
        }

#define SODIUM_DETYPE_FUNCTION1_OLD(A, B, f)                  \
    [f](const light_ptr& a) -> light_ptr {                    \
        return impl::detype<A>(f, a); \
    }

#if 1
        stream_ map_(transaction_impl* trans,
                     listen_handler* impl,
                     const std::function<light_ptr(const light_ptr&)>& f,
                     const stream_& ca);
#endif

        stream_ map_(transaction_impl* trans,
                     listen_handler* impl,
                     const stream_& ca);

        /*!
         * Function to push a value into an stream
         */
        void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans,
                  const light_ptr& ptr);

        /*!
         * Creates an stream, that values can be pushed into using impl::send().
         */
        SODIUM_TUPLE<stream_, SODIUM_SHARED_PTR<node>> unsafe_new_stream();

        struct cell_impl {
            cell_impl();
            cell_impl(const stream_& updates,
                      const SODIUM_SHARED_PTR<cell_impl>& parent);
            virtual ~cell_impl();

            virtual const light_ptr& sample() const = 0;
            virtual const light_ptr& newValue() const = 0;

            stream_ updates;  // Having this here allows references to cell to
                              // keep the underlying stream's cleanups alive,
                              // and provides access to the underlying stream,
                              // for certain primitives.

            std::function<void()>* kill;
            SODIUM_SHARED_PTR<cell_impl> parent;

            std::function<std::function<void()>(
                transaction_impl*, const SODIUM_SHARED_PTR<node>&,
                const std::function<void(transaction_impl*,
                                         const light_ptr&)>&)>
            listen_value_raw() const;
        };

        SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0,
                                          const light_ptr& initValue,
                                          const stream_& input);
        SODIUM_SHARED_PTR<cell_impl> hold_lazy(
            transaction_impl* trans0,
            const std::function<light_ptr()>& initValue, const stream_& input);

        struct cell_impl_constant : cell_impl {
            cell_impl_constant(light_ptr k_) : k(std::move(k_)) {}
            light_ptr k;
            virtual const light_ptr& sample() const { return k; }
            virtual const light_ptr& newValue() const { return k; }
        };

        template <typename state_t> struct cell_impl_concrete : cell_impl {
            cell_impl_concrete(const stream_& updates_, state_t&& state_,
                               const SODIUM_SHARED_PTR<cell_impl>& parent_)
                : cell_impl(updates_, parent_), state(std::move(state_)) {}
            state_t state;

            virtual const light_ptr& sample() const { return state.sample(); }
            virtual const light_ptr& newValue() const {
                return state.newValue();
            }
        };

        struct cell_impl_loop : cell_impl {
            cell_impl_loop(
                const stream_& updates_,
                const SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl>>& pLooped_,
                const SODIUM_SHARED_PTR<cell_impl>& parent_)
                : cell_impl(updates_, parent_), pLooped(pLooped_) {}
            SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl>> pLooped;

            void assertLooped() const {
                if (!*pLooped)
                    SODIUM_THROW("cell_loop sampled before it was looped");
            }

            virtual const light_ptr& sample() const {
                assertLooped();
                return (*pLooped)->sample();
            }
            virtual const light_ptr& newValue() const {
                assertLooped();
                return (*pLooped)->newValue();
            }
        };

        struct cell_state {
            cell_state(const light_ptr& initA) : current(initA) {}
            light_ptr current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const { return current; }
            const light_ptr& newValue() const {
                return update ? update.get() : current;
            }
            void finalize() {
                current = update.get();
                update = boost::optional<light_ptr>();
            }
        };

        struct cell_state_lazy {
                private:
            // Don't allow copying because we have no valid implementation for
            // that given our use of a pointer in pInitA.
            cell_state_lazy(const cell_state_lazy&) {}
            cell_state_lazy& operator=(const cell_state_lazy&) { return *this; }

                public:
            cell_state_lazy(const std::function<light_ptr()>& initA)
                : pInitA(new std::function<light_ptr()>(initA)) {}
            cell_state_lazy(cell_state_lazy&& other)
                : pInitA(other.pInitA),
                  current(other.current),
                  update(other.update) {
                other.pInitA = NULL;
            }
            ~cell_state_lazy() { delete pInitA; }
            std::function<light_ptr()>* pInitA;
            boost::optional<light_ptr> current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const {
                if (!current) {
                    const_cast<cell_state_lazy*>(this)->current =
                        boost::optional<light_ptr>((*pInitA)());
                    delete pInitA;
                    const_cast<cell_state_lazy*>(this)->pInitA = NULL;
                }
                return current.get();
            }
            const light_ptr& newValue() const {
                return update ? update.get() : sample();
            }
            void finalize() {
                current = update;
                update = boost::optional<light_ptr>();
            }
        };

        class cell_ {
            friend impl::stream_ underlying_stream(const cell_& beh);

        public:
            cell_();
            cell_(cell_impl* impl);
            cell_(SODIUM_SHARED_PTR<cell_impl> impl);
            cell_(light_ptr a);
            SODIUM_SHARED_PTR<impl::cell_impl> impl;

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            /*!
             * For optimization, if this cell is a constant, then return its
             * value.
             */
            boost::optional<light_ptr> get_constant_value() const;
#endif

            stream_ value_(transaction_impl* trans) const;
            const stream_& updates_() const { return impl->updates; }
        };

        cell_ map_(transaction_impl* trans,
                   const std::function<light_ptr(const light_ptr&)>& f,
                   const cell_& beh);

        template <typename A, typename F>
        struct map_handler : listen_handler
        {
            map_handler(F f_) : f(std::move(f_)) {}
            F f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                typedef typename std::result_of<F(A)>::type B;
                auto b = light_ptr::create<B>(f(*a.cast_ptr<A>(nullptr)));
                send(target, trans, b);
            }
        };

        struct map_cell_handler : listen_handler
        {
            map_cell_handler(std::function<light_ptr(const light_ptr&)> f_)
            : f(std::move(f_))
            {
            }
            std::function<light_ptr(const light_ptr&)> f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                send(target, trans, f(a));
            }
        };

        template <typename A, typename F>
        listen_handler* mk_map_handler(F f_)
        {
            return new map_handler<A, F>(std::move(f_));
        }

        template <typename A, typename F>
        struct listener_handler : listen_handler
        {
            listener_handler(F f_) : f(std::move(f_)) {}
            F f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                f(*a.cast_ptr<A>(NULL));
            }
        };

        template <typename A, typename S, typename Fn>
        struct collect_handler : listen_handler
        {
            collect_handler(SODIUM_SHARED_PTR<lazy<S>> pState_, Fn f_)
            : pState(std::move(pState_)),
              f(std::move(f_))
            {
            }
            SODIUM_SHARED_PTR<lazy<S>> pState;
            Fn f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                typedef typename std::tuple_element<
                    0, typename std::result_of<Fn(A, S)>::type>::type B;
/*
                            new std::function<void(const SODIUM_SHARED_PTR<impl::node>&,
                                       impl::transaction_impl*,
                                       const light_ptr&)>(
                    [pState, f](const SODIUM_SHARED_PTR<impl::node>& target,
                                impl::transaction_impl* trans2,
                                const light_ptr& ptr) {
*/
                auto outsSt = f(*a.cast_ptr<A>(NULL), (*pState)());
                const S& new_s = SODIUM_TUPLE_GET<1>(outsSt);
                *pState = lazy<S>(new_s);
                send(target, trans,
                     light_ptr::create<B>(std::get<0>(outsSt)));
            }
        };

        template <typename A, typename B>
        struct accum_s_handler : listen_handler
        {
            accum_s_handler(
                SODIUM_SHARED_PTR<lazy<B>> pState_,
                std::function<B(const A&, const B&)> f_
            )
            : pState(std::move(pState_)),
              f(std::move(f_))
            {
            }
            SODIUM_SHARED_PTR<lazy<B>> pState;
            std::function<B(const A&, const B&)> f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                B b = f(*a.cast_ptr<A>(NULL), (*pState)());
                *pState = lazy<B>(b);
                send(target, trans, light_ptr::create<B>(b));
            }
        };

        inline void kill_once(SODIUM_SHARED_PTR<std::function<void()>*> ppKill)
        {
            std::function<void()>* pKill = *ppKill;
            if (pKill != NULL) {
                *ppKill = NULL;
                (*pKill)();
                delete pKill;
            }
        }

        struct once_handler : listen_handler
        {
            once_handler(SODIUM_SHARED_PTR<std::function<void()>*> ppKill_)
            : ppKill(std::move(ppKill_))
            {
            }
            SODIUM_SHARED_PTR<std::function<void()>*> ppKill;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                if (*ppKill) {
                    send(target, trans, a);
                    kill_once(ppKill);
                }
            }
        };

        struct merge_handler : listen_handler
        {
            merge_handler(SODIUM_SHARED_PTR<impl::node> right_)
            : right(std::move(right_))
            {
            }
            SODIUM_SHARED_PTR<impl::node> right;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                send(right, trans, a);
            }
        };

        struct snapshot_handler : listen_handler
        {
            snapshot_handler(cell_ beh_,
                std::function<light_ptr(const light_ptr&, const light_ptr&)> combine_)
            : beh(std::move(beh_)),
              combine(std::move(combine_))
            {
            }
            cell_ beh;
            std::function<light_ptr(const light_ptr&, const light_ptr&)> combine;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                send(target, trans, combine(a, beh.impl->sample()));
            }
        };

        struct filter_handler : listen_handler
        {
            filter_handler(std::function<bool(const light_ptr&)> pred_)
            : pred(std::move(pred_))
            {
            }
            std::function<bool(const light_ptr&)> pred;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                if (pred(a)) send(target, trans, a);
            }
        };

        struct filter_optional_handler : listen_handler
        {
            filter_optional_handler(std::function<boost::optional<light_ptr>(const light_ptr&)> f_)
            : f(std::move(f_))
            {
            }
            std::function<boost::optional<light_ptr>(const light_ptr&)> f;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& poa)
            {
                boost::optional<light_ptr> oa = f(poa);
                if (oa) impl::send(target, trans, oa.get());
            }
        };

    }  // end namespace impl

}  // end namespace sodium

#endif
