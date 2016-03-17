/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_SODIUM_H_
#define _SODIUM_SODIUM_H_

#include <sodium/light_ptr.h>
#include <sodium/transaction.h>
#include <functional>
#include <boost/optional.hpp>
#include <memory>
#include <list>
#include <set>
#if defined(SODIUM_NO_EXCEPTIONS)
#include <stdlib.h>
#else
#include <stdexcept>
#endif
#include <vector>

#define SODIUM_CONSTANT_OPTIMIZATION

// TO DO:
// the sample_lazy() mechanism is not correct yet. The lazy value needs to be
// fixed at the end of the transaction.

namespace sodium {
    template <class A> class stream;
    template <class A> class cell;
    template <class A> class cell_sink;
    template <class A> class cell_loop;
    template <class A> class stream_loop;
    template <class A, class B>
#if defined(SODIUM_NO_CXX11)
    cell<B> apply(const cell<lambda1<B,const A&>>& bf, const cell<A>& ba);
#else
    cell<B> apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
#endif
    template <class A>
    stream<A> filter_optional(const stream<boost::optional<A>>& input);
    template <class A>
    stream<A> split(const stream<std::list<A>>& e);
    template <class A>
    stream<A> switch_s(const cell<stream<A>>& bea);
    template <class T>
    cell<typename T::time> clock(const T& t);

    namespace impl {

        class cell_;
        class cell_impl;

        class stream_ {
        friend class cell_;
        template <class A> friend class sodium::stream;
        template <class A> friend class sodium::stream_loop;
        template <class A> friend class sodium::cell;
        friend cell_ switch_c(transaction_impl* trans, const cell_& bba);
        friend SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_& input);
        friend SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_& input);
        template <class A, class B>
#if defined(SODIUM_NO_CXX11)
        friend cell<B> sodium::apply(const cell<lambda1<B, const A&>>& bf, const cell<A>& ba);
#else
        friend cell<B> sodium::apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
#endif
        friend cell_ apply(transaction_impl* trans0, const cell_& bf, const cell_& ba);
#if defined(SODIUM_NO_CXX11)
        friend stream_ map_(transaction_impl* trans, const lambda1<light_ptr, const light_ptr&>& f, const stream_& ev);
#else
        friend stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ev);
#endif
        friend cell_ map_(transaction_impl* trans,
#if defined(SODIUM_NO_CXX11)
            const lambda1<light_ptr, const light_ptr&>& f,
#else
            const std::function<light_ptr(const light_ptr&)>& f,
#endif
            const cell_& beh);
        friend stream_ switch_s(transaction_impl* trans, const cell_& bea);
        template <class A>
        friend stream<A> sodium::split(const stream<std::list<A>>& e);
#if defined(SODIUM_NO_CXX11)
        friend struct switch_s_task;
        friend struct switch_c_handler;
#endif
        friend stream_ filter_optional_(transaction_impl* trans, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f);

        protected:
            boost::intrusive_ptr<listen_impl_func<H_STREAM> > p_listen_impl;

        public:
            stream_();
            stream_(const boost::intrusive_ptr<listen_impl_func<H_STREAM> >& p_listen_impl_)
                : p_listen_impl(p_listen_impl_) {}

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            bool is_never() const { return !impl::alive(p_listen_impl); }
#endif

        protected:

            /*!
             * listen to streams.
             */
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* listen_raw(
#else
            std::function<void()>* listen_raw(
#endif
                        transaction_impl* trans0,
                        const SODIUM_SHARED_PTR<impl::node>& target,
#if defined(SODIUM_NO_CXX11)
                        lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>* handle,
#else
                        std::function<void(const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&)>* handle,
#endif
                        bool suppressEarlierFirings) const;

            /*!
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
#if defined(SODIUM_NO_CXX11)
            stream_ unsafe_add_cleanup(lambda0<void>* cleanup)
#else
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup)
#endif
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
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
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
#if defined(SODIUM_NO_CXX11)
            stream_ unsafe_add_cleanup(lambda0<void>* cleanup1, lambda0<void>* cleanup2)
#else
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2)
#endif
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
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
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
#if defined(SODIUM_NO_CXX11)
            stream_ unsafe_add_cleanup(lambda0<void>* cleanup1, lambda0<void>* cleanup2, lambda0<void>* cleanup3)
#else
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2, std::function<void()>* cleanup3)
#endif
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
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
             * Create a new stream that is like this stream but has an extra cleanup.
             */
#if defined(SODIUM_NO_CXX11)
            stream_ add_cleanup_(transaction_impl* trans, lambda0<void>* cleanup) const;
#else
            stream_ add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const;
#endif
            cell_ hold_(transaction_impl* trans, const light_ptr& initA) const;
            cell_ hold_lazy_(transaction_impl* trans, const std::function<light_ptr()>& initA) const;
            stream_ once_(transaction_impl* trans) const;
            stream_ merge_(transaction_impl* trans, const stream_& other) const;
#if defined(SODIUM_NO_CXX11)
            stream_ coalesce_(transaction_impl* trans, const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine) const;
#else
            stream_ coalesce_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
#endif
            stream_ last_firing_only_(transaction_impl* trans) const;
#if defined(SODIUM_NO_CXX11)
            stream_ snapshot_(transaction_impl* trans, const cell_& beh, const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine) const;
            stream_ filter_(transaction_impl* trans, const lambda1<bool, const light_ptr&>& pred) const;
#else
            stream_ snapshot_(transaction_impl* trans, const cell_& beh, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
            stream_ filter_(transaction_impl* trans, const std::function<bool(const light_ptr&)>& pred) const;
#endif

#if defined(SODIUM_NO_CXX11)
            lambda0<void>* listen_impl(
#else
            std::function<void()>* listen_impl(
#endif
                transaction_impl* trans,
                const SODIUM_SHARED_PTR<impl::node>& target,
                SODIUM_SHARED_PTR<holder> h,
                bool suppressEarlierFirings) const
            {
                boost::intrusive_ptr<listen_impl_func<H_STRONG> > li(
                    reinterpret_cast<listen_impl_func<H_STRONG>*>(p_listen_impl.get()));
                if (alive(li))
                    return (*li->func)(trans, target, h, suppressEarlierFirings);
                else
                    return NULL;
            }
        };
#if defined(SODIUM_NO_CXX11)
        template <class A, class B>
        class _de_type : public i_lambda1<light_ptr, const light_ptr&>
        {
        private:
            lambda1<B,const A&> f;
        public:
            _de_type(const lambda1<B,const A&>& f) : f(f) {}
            virtual light_ptr operator () (const light_ptr& a) const
            {
                return light_ptr::create<B>(f(*a.cast_ptr<A>(NULL)));
            }
        };
        #define SODIUM_DETYPE_FUNCTION1(A,B,f) new sodium::impl::_de_type<A,B>(f)
        stream_ map_(transaction_impl* trans, const lambda1<light_ptr, const light_ptr&>& f, const stream_& ca);
#else
        #define SODIUM_DETYPE_FUNCTION1(A,B,f) \
                   [f] (const light_ptr& a) -> light_ptr { \
                        return light_ptr::create<B>(f(*a.cast_ptr<A>(NULL))); \
                   }
        stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ca);
#endif

        /*!
         * Function to push a value into an stream
         */
        void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans, const light_ptr& ptr);

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        SODIUM_TUPLE<
                stream_,
                SODIUM_SHARED_PTR<node>
            > unsafe_new_stream();

        struct cell_impl {
            cell_impl();
            cell_impl(
                const stream_& updates,
                const SODIUM_SHARED_PTR<cell_impl>& parent);
            virtual ~cell_impl();

            virtual const light_ptr& sample() const = 0;
            virtual const light_ptr& newValue() const = 0;

            stream_ updates;  // Having this here allows references to cell to keep the
                             // underlying stream's cleanups alive, and provides access to the
                             // underlying stream, for certain primitives.

#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill;
#else
            std::function<void()>* kill;
#endif
            SODIUM_SHARED_PTR<cell_impl> parent;

#if defined(SODIUM_NO_CXX11)
            lambda3<lambda0<void>, transaction_impl*, const SODIUM_SHARED_PTR<node>&,
                             const lambda2<void, transaction_impl*, const light_ptr&>&> listen_value_raw() const;
#else
            std::function<std::function<void()>(transaction_impl*, const SODIUM_SHARED_PTR<node>&,
                             const std::function<void(transaction_impl*, const light_ptr&)>&)> listen_value_raw() const;
#endif
        };

        SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0,
                            const light_ptr& initValue,
                            const stream_& input);
        SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0,
                            const std::function<light_ptr()>& initValue,
                            const stream_& input);

        struct cell_impl_constant : cell_impl {
            cell_impl_constant(const light_ptr& k_) : k(k_) {}
            light_ptr k;
            virtual const light_ptr& sample() const { return k; }
            virtual const light_ptr& newValue() const { return k; }
        };

        template <class state_t>
        struct cell_impl_concrete : cell_impl {
            cell_impl_concrete(
                const stream_& updates_,
                const state_t& state_,
                const SODIUM_SHARED_PTR<cell_impl>& parent_)
            : cell_impl(updates_, parent_),
              state(state_)
            {
            }
            state_t state;

            virtual const light_ptr& sample() const { return state.sample(); }
            virtual const light_ptr& newValue() const { return state.newValue(); }
        };

        struct cell_impl_loop : cell_impl {
            cell_impl_loop(
                const stream_& updates_,
                const SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl> >& pLooped_,
                const SODIUM_SHARED_PTR<cell_impl>& parent_)
            : cell_impl(updates_, parent_),
              pLooped(pLooped_)
            {
            }
            SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<cell_impl> > pLooped;

            void assertLooped() const {
                if (!*pLooped)
                    throw std::runtime_error("cell_loop sampled before it was looped");
            }

            virtual const light_ptr& sample() const { assertLooped(); return (*pLooped)->sample(); }
            virtual const light_ptr& newValue() const { assertLooped(); return (*pLooped)->newValue(); }
        };

        struct cell_state {
            cell_state(const light_ptr& initA) : current(initA) {}
            light_ptr current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const { return current; }
            const light_ptr& newValue() const { return update ? update.get() : current; }
            void finalize() {
                current = update.get();
                update = boost::optional<light_ptr>();
            }
        };

        struct cell_state_lazy {
            cell_state_lazy(const std::function<light_ptr()>& initA)
            : pInitA(new std::function<light_ptr()>(initA)) {}
            std::function<light_ptr()>* pInitA;
            boost::optional<light_ptr> current;
            boost::optional<light_ptr> update;
            const light_ptr& sample() const {
                if (!current) {
                    const_cast<cell_state_lazy*>(this)->current = boost::optional<light_ptr>((*pInitA)());
                    delete pInitA;
                    const_cast<cell_state_lazy*>(this)->pInitA = NULL;
                }
                return current.get();
            }
            const light_ptr& newValue() const { return update ? update.get() : sample(); }
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
                cell_(const SODIUM_SHARED_PTR<cell_impl>& impl);
                cell_(const light_ptr& a);
                SODIUM_SHARED_PTR<impl::cell_impl> impl;

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
                /*!
                 * For optimization, if this cell is a constant, then return its value.
                 */
                boost::optional<light_ptr> get_constant_value() const;
#endif

                stream_ value_(transaction_impl* trans) const;
                const stream_& updates_() const { return impl->updates; }
        };

#if defined(SODIUM_NO_CXX11)
        cell_ map_(transaction_impl* trans, const lambda1<light_ptr, const light_ptr&>& f,
            const cell_& beh);
#else
        cell_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh);
#endif

        template <class S>
        struct collect_state {
            collect_state(const std::function<S()>& s_lazy_) : s_lazy(s_lazy_) {}
            std::function<S()> s_lazy;
        };

#if defined(SODIUM_NO_CXX11)
        template <class A, class S, class B>
        struct collect_handler {
            collect_handler(const SODIUM_SHARED_PTR<collect_state<S> >& pState,
                            const lambda2<SODIUM_TUPLE<B, S>, const A&, const S&>& f)
            : pState(pState), f(f) {}
            SODIUM_SHARED_PTR<collect_state<S> > pState;
            lambda2<SODIUM_TUPLE<B, S>, const A&, const S&> f;
            virtual void operator () (const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans,
                                      const light_ptr& ptr) {
                SODIUM_TUPLE<B,S> outsSt = f(*ptr.cast_ptr<A>(NULL), pState->s);
                pState->s = SODIUM_TUPLE_GET<1>(outsSt);
                send(target, trans, light_ptr::create<B>(SODIUM_TUPLE_GET<0>(outsSt)));
            }
        };
#endif
    }  // end namespace impl

#if defined(SODIUM_NO_CXX11)
    template <class A, class B>
    struct fst_arg : i_lambda2<A,const A&, const B&>  {
        virtual A operator () (const A& a, const B&) const { return a; }
    };
    template <class A, class B>
    struct snd_arg : i_lambda2<B,const A&, const B&> {
        virtual B operator () (const A&, const B& b) const { return b; }
    };
#endif

    template <class A>
    class stream;

    /*!
     * A like an stream, but it tracks the input stream's current value and causes it
     * always to be output once at the beginning for each listener.
     */
    template <class A>
    class cell : protected impl::cell_ {
        template <class AA> friend class stream;
        template <class AA> friend class cell;
        template <class AA> friend class cell_loop;
        template <class AA, class BB>
#if defined(SODIUM_NO_CXX11)
        friend cell<BB> apply(const cell<lambda1<BB, const AA&>>& bf, const cell<AA>& ba);
#else
        friend cell<BB> apply(const cell<std::function<BB(const AA&)>>& bf, const cell<AA>& ba);
#endif
        template <class AA>
        friend cell<AA> switch_c(const cell<cell<AA>>& bba);
        template <class AA>
        friend stream<AA> switch_s(const cell<stream<AA>>& bea);
        template <class TT>
        friend cell<typename TT::time> clock(const TT& t);
        private:
            cell(const SODIUM_SHARED_PTR<impl::cell_impl>& impl_)
                : impl::cell_(impl_)
            {
            }

        protected:
            cell() {}
            cell(const impl::cell_& beh) : impl::cell_(beh) {}

        public:
            /*!
             * Constant value.
             */
            cell(const A& a)
                : impl::cell_(light_ptr::create<A>(a))
            {
            }

            cell(A&& a)
                : impl::cell_(light_ptr::create<A>(std::move(a)))
            {
            }

            /*!
             * Sample the value of this cell.
             */
            A sample() const {
                transaction trans;
                return *impl->sample().template cast_ptr<A>(NULL);
            }

            std::function<A()> sample_lazy() const {
                const SODIUM_SHARED_PTR<impl::cell_impl>& impl_(this->impl);
                return [impl_] () -> A {
                    transaction trans;
                    return *impl_->sample().template cast_ptr<A>(NULL);
                };
            }

            /*!
             * Returns a new cell with the specified cleanup added to it, such that
             * it will be executed when no copies of the new cell are referenced.
             */
#if defined(SODIUM_NO_CXX11)
            cell<A> add_cleanup(const lambda0<void>& cleanup) const {
#else
            cell<A> add_cleanup(const std::function<void()>& cleanup) const {
#endif
                transaction trans;
                return updates().add_cleanup(cleanup).hold(sample());
            }

            /*!
             * Map a function over this behaviour to modify the output value.
             */
            template <class B>
#if defined(SODIUM_NO_CXX11)
            cell<B> map(const lambda1<B, const A&>& f) const {
#else
            cell<B> map(const std::function<B(const A&)>& f) const {
#endif
                transaction trans;
                return cell<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Map a function over this behaviour to modify the output value.
             *
             * g++-4.7.2 has a bug where, under a 'using namespace std' it will interpret
             * b.template map<A>(f) as if it were std::map. If you get this problem, you can
             * work around it with map_.
             */
            template <class B>
#if defined(SODIUM_NO_CXX11)
            cell<B> map_(const lambda1<B, const A&>& f) const {
#else
            cell<B> map_(const std::function<B(const A&)>& f) const {
#endif
                transaction trans;
                return cell<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Returns an stream giving the updates to a cell. If this cell was created
             * by a hold, then this gives you back an stream equivalent to the one that was held.
             */
            stream<A> updates() const {
                return stream<A>(impl->updates);
            }

            /*!
             * Returns an stream describing the value of a cell, where there's an initial stream
             * giving the current value.
             */
            stream<A> value() const {
                transaction trans;
                return stream<A>(value_(trans.impl())).coalesce();
            }

            /*!
             * Listen for updates to the value of this cell. This is the observer pattern. The
             * returned {@link Listener} has a {@link Listener#unlisten()} method to cause the
             * listener to be removed. This is an OPERATIONAL mechanism is for interfacing between
             * the world of I/O and for FRP.
             * @param action The handler to execute when there's a new value.
             *   You should make no assumptions about what thread you are called on, and the
             *   handler should not block. You are not allowed to use {@link CellSink#send(Object)}
             *   or {@link StreamSink#send(Object)} in the handler.
             *   An exception will be thrown, because you are not meant to use this to create
             *   your own primitives.
             */
            std::function<void()> listen(const std::function<void(const A&)>& handle) const {
                transaction trans;
                return stream<A>(value_(trans.impl())).coalesce().listen(handle);
            }

            /**
             * Transform a cell with a generalized state loop (a mealy machine). The function
             * is passed the input and the old state and returns the new state and output value.
             */
            template <class S, class B>
            cell<B> collect_lazy(
                const std::function<S()>& initS,
#if defined(SODIUM_NO_CXX11)
                const lambda2<SODIUM_TUPLE<B, S>, const A&, const S&>& f
#else
                const std::function<std::tuple<B, S>(const A&, const S&)>& f
#endif
            ) const
            {
                transaction trans1;
#if defined(SODIUM_NO_CXX11)
                stream<A> ea = updates().coalesce(lambda2<A,A,A>(new snd_arg<A,A>));
#else
                auto ea = updates().coalesce([] (const A&, const A& snd) -> A { return snd; });
#endif
                std::function<A()> za_lazy = sample_lazy();
                std::function<SODIUM_TUPLE<B,S>()> zbs = [za_lazy, initS, f] () -> SODIUM_TUPLE<B,S> {
                    return f(za_lazy(), initS());
                };
                SODIUM_SHARED_PTR<impl::collect_state<S> > pState(new impl::collect_state<S>([zbs] () -> S {
                    return SODIUM_TUPLE_GET<1>(zbs());
                }));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
                lambda0<void>* kill = updates().listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&>(
                        new impl::collect_handler<A,S,B>(pState, f)
                    ), false);
#else
                auto kill = updates().listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            SODIUM_TUPLE<B,S> outsSt = f(*ptr.cast_ptr<A>(NULL), pState->s_lazy());
                            const S& new_s = SODIUM_TUPLE_GET<1>(outsSt);
                            pState->s_lazy = [new_s] () { return new_s; };
                            send(target, trans2, light_ptr::create<B>(SODIUM_TUPLE_GET<0>(outsSt)));
                        }), false);
#endif
                return stream<B>(SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill)).hold_lazy([zbs] () -> B {
                    return SODIUM_TUPLE_GET<0>(zbs());
                });
            }

            /**
             * Transform a cell with a generalized state loop (a mealy machine). The function
             * is passed the input and the old state and returns the new state and output value.
             */
            template <class S, class B>
            cell<B> collect(
                const S& initS,
#if defined(SODIUM_NO_CXX11)
                const lambda2<SODIUM_TUPLE<B, S>, const A&, const S&>& f
#else
                const std::function<std::tuple<B, S>(const A&, const S&)>& f
#endif
            ) const
            {
                return collect_lazy<S, B>([initS] () -> S { return initS; }, f);
            }

    };  // end class cell

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A>
        struct listen_wrap : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&> {
            listen_wrap(const lambda1<void, const A&>& handle) : handle(handle) {}
            lambda1<void, const A&> handle;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl* trans, const light_ptr& ptr) const {
                handle(*ptr.cast_ptr<A>(NULL));
            }
        };
        struct null_action : i_lambda0<void> {
            virtual void operator () () const {}
        };
        template <class A>
        struct detype_combine : i_lambda2<light_ptr, const light_ptr&, const light_ptr&> {
            detype_combine(const lambda2<A, const A&, const A&>& combine) : combine(combine) {}
            lambda2<A, const A&, const A&> combine;
            virtual light_ptr operator () (const light_ptr& a, const light_ptr& b) const {
                return light_ptr::create<A>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<A>(NULL)));
            }
        };
        template <class A>
        struct detype_pred : i_lambda1<bool, const light_ptr&> {
            detype_pred(const lambda1<bool, A>& pred) : pred(pred) {}
            lambda1<bool, A> pred;
            virtual bool operator () (const light_ptr& a) const {
                return pred(*a.cast_ptr<A>(NULL));
            }
        };
        template <class A, class B, class C>
        struct detype_snapshot : i_lambda2<light_ptr, const light_ptr&, const light_ptr&> {
            detype_snapshot(const lambda2<C, const A&, const B&>& combine) : combine(combine) {}
            lambda2<C, const A&, const B&> combine;
            virtual light_ptr operator () (const light_ptr& a, const light_ptr& b) const {
                return light_ptr::create<C>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<B>(NULL)));
            }
        };
        template <class A>
        struct gate_handler : i_lambda2<boost::optional<A>, const A&, const bool&> {
            virtual boost::optional<A> operator () (const A& a, const bool& gated) {
                return gated ? boost::optional<A>(a) : boost::optional<A>();
            }
        };
        template <class A, class B>
        struct accum_handler : i_lambda3<void, const SODIUM_SHARED_PTR<node>&, transaction_impl*, const light_ptr&> {
            accum_handler(
                const SODIUM_SHARED_PTR<collect_state<B> >& pState,
                const lambda2<B, const A&, const B&>& f)
            : pState(pState), f(f) {}
            SODIUM_SHARED_PTR<collect_state<B> > pState;
            lambda2<B, const A&, const B&> f;

            virtual void operator () (const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& ptr) const {
                pState->s = f(*ptr.cast_ptr<A>(NULL), pState->s);
                send(target, trans, light_ptr::create<B>(pState->s));
            }
        };
        template <class A>
        struct count_handler : i_lambda2<int,const A&,int> {
            virtual int operator () (const A&, int total) const {
                return total+1;
            }
        };
        template <class A>
        struct delay_handler : i_lambda1<std::list<A>, const A&> {
            virtual std::list<A> operator () (const A& a) const {
                std::list<A> as;
                as.push_back(a);
                return as;
            }
        };
    }
#endif

    template <class A>
    class stream : protected impl::stream_ {
        template <class AA> friend class stream;
        template <class AA> friend class stream_sink;
        template <class AA> friend class cell;
        template <class AA> friend class cell_sink;
        template <class AA> friend class cell_loop;
        template <class AA> friend class stream_sink;
        template <class AA> friend stream<AA> filter_optional(const stream<boost::optional<AA>>& input);
        template <class AA> friend stream<AA> switch_s(const cell<stream<AA>>& bea);
        template <class AA> friend stream<AA> split(const stream<std::list<AA>>& e);
        template <class AA> friend class sodium::stream_loop;
        public:
            /*!
             * The 'never' stream (that never fires).
             */
            stream() {}
        protected:
            stream(const impl::stream_& ev) : impl::stream_(ev) {}
        public:
            /*!
             * High-level interface to obtain an stream's value.
             */
#if defined(SODIUM_NO_CXX11)
            lambda0<void> listen(const lambda1<void, const A&>& handle) const {
#else
            std::function<void()> listen(const std::function<void(const A&)>& handle) const {
#endif
                transaction trans1;
#if defined(SODIUM_NO_CXX11)
                lambda0<void>* pKill = listen_raw(trans1.impl(),
#else
                std::function<void()>* pKill = listen_raw(trans1.impl(),
#endif
                    SODIUM_SHARED_PTR<impl::node>(new impl::node(SODIUM_IMPL_RANK_T_MAX)),
#if defined(SODIUM_NO_CXX11)
                    new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&>(
                        new impl::listen_wrap<A>(handle)
                    ), false);
#else
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [handle] (const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr& ptr) {
                            handle(*ptr.cast_ptr<A>(NULL));
                        }), false);
#endif
                if (pKill != NULL) {
#if defined(SODIUM_NO_CXX11)
                    lambda0<void> kill(*pKill);
#else
                    std::function<void()> kill(*pKill);
#endif
                    delete pKill;
                    return kill;
                }
                else
#if defined(SODIUM_NO_CXX11)
                    return new impl::null_action();
#else
                    return [] () {};
#endif
            };

            /*!
             * A variant of listen that handles the first stream and then
             * automatically deregisters itself. This is useful for implementing things that
             * work like promises.
             */
            std::function<void()> listen_once(const std::function<void(const A&)>& handle) const {
                std::shared_ptr<std::function<void()>> lRef(new std::function<void()>);
                *lRef = listen([handle, lRef] (const A& a) {
                    handle(a);
                    (*lRef)();
                });
                return *lRef;
            }

            /*!
             * Map a function over this stream to modify the output value. The function must be
             * pure (referentially transparent), that is, it must not have effects.
             */
            template <class B>
#if defined(SODIUM_NO_CXX11)
            stream<B> map(const lambda1<B, const A&>& f) const {
#else
            stream<B> map(const std::function<B(const A&)>& f) const {
#endif
                transaction trans;
                return stream<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Map a function over this stream to modify the output value. Effects are allowed.
             */
            template <class B>
#if defined(SODIUM_NO_CXX11)
            stream<B> map_effectful(const lambda1<B, const A&>& f) const {
#else
            stream<B> map_effectful(const std::function<B(const A&)>& f) const {
#endif
                return this->template map_<B>(f);  // Same as map() for now but this may change!
            }

            /*!
             * Map a function over this stream to modify the output value.
             *
             * g++-4.7.2 has a bug where, under a 'using namespace std' it will interpret
             * b.template map<A>(f) as if it were std::map. If you get this problem, you can
             * work around it with map_.
             */
            template <class B>
#if defined(SODIUM_NO_CXX11)
            stream<B> map_(const lambda1<B, const A&>& f) const {
#else
            stream<B> map_(const std::function<B(const A&)>& f) const {
#endif
                transaction trans;
                return stream<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Map a function over this stream that always outputs a constant value.
             */
            template <class B>
            stream<B> map_to(const B& value) {
                return map<B>([value] (const A&) { return value; });
            }

            /*!
             * Merge two streams of the same type into one, so that streams on either input appear
             * on the returned stream.
             * <p>
             * In the case where two streams are simultaneous (i.e. both
             * within the same transaction), the stream from <em>s</em> will take precedence, and
             * the stream from <em>this</em> will be dropped.
             * If you want to specify your own combining function, use {@link Stream#merge(Stream, Lambda2)}.
             * merge(s) is equivalent to merge(s, (l, r) -&gt; r).
             *
             * DEPRECATED: Please replace a.merge(b) with b.or_else(a) - NOTE THE SWAPPED ARGUMENTS.
             */
            stream<A> merge(const stream<A>& s) const __attribute__ ((deprecated)) {
                return merge(s, [] (const A& l, const A& r) { return r; });
            }

            /*!
             * Variant of merge that merges two streams and will drop an stream
             * in the simultaneous case.
             * <p>
             * In the case where two streams are simultaneous (i.e. both
             * within the same transaction), the stream from <em>this</em> will take precedence, and
             * the stream from <em>s</em> will be dropped.
             * If you want to specify your own combining function, use {@link Stream#merge(Stream, Lambda2)}.
             * s1.orElse(s2) is equivalent to s1.merge(s2, (l, r) -&gt; l).
             * <p>
             * The name orElse() is used instead of merge() to make it really clear that care should
             * be taken, because streams can be dropped.
             */
            stream<A> or_else(const stream<A>& s) const {
                return merge(s, [] (const A& l, const A& r) { return l; });
            }

            /*!
             * If there's more than one firing in a single transaction, combine them into
             * one using the specified combining function.
             *
             * If the stream firings are ordered, then the first will appear at the left
             * input of the combining function. In most common cases it's best not to
             * make any assumptions about the ordering, and the combining function would
             * ideally be commutative.
             */
#if defined(SODIUM_NO_CXX11)
            stream<A> coalesce(const lambda2<A, const A&, const A&>& combine) const
#else
            stream<A> coalesce(const std::function<A(const A&, const A&)>& combine) const
#endif
            {
                transaction trans;
                return stream<A>(coalesce_(trans.impl(),
#if defined(SODIUM_NO_CXX11)
                    new impl::detype_combine<A>(combine)
#else
                    [combine] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                        return light_ptr::create<A>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<A>(NULL)));
                    }
#endif
                ));
            }

            /*!
             * If there's more than one firing in a single transaction, keep only the latest one.
             */
            stream<A> coalesce() const
            {
                return coalesce(
#if defined(SODIUM_NO_CXX11)
                        new snd_arg<A,A>
#else
                        [] (const A&, const A& snd) -> A { return snd; }
#endif
                    );
            }

            /*!
             * A variant of {@link merge(Stream)} that uses the specified function to combine simultaneous
             * streams.
             * <p>
             * If the streams are simultaneous (that is, one stream from this and one from <em>s</em>
             * occurring in the same transaction), combine them into one using the specified combining function
             * so that the returned stream is guaranteed only ever to have one stream per transaction.
             * The stream from <em>this</em> will appear at the left input of the combining function, and
             * the stream from <em>s</em> will appear at the right.
             * @param f Function to combine the values. It may construct FRP logic or use
             *    {@link Cell#sample()}. Apart from this the function must be <em>referentially transparent</em>.
             */
#if defined(SODIUM_NO_CXX11)
            stream<A> merge(const stream<A>& s, const lambda2<A, const A&, const A&>& f) const
#else
            stream<A> merge(const stream<A>& s, const std::function<A(const A&, const A&)>& f) const
#endif
            {
                transaction trans;
                return stream<A>(merge_(trans.impl(), s)).coalesce(f);
            }

            /*!
             * Filter this stream based on the specified predicate, passing through values
             * where the predicate returns true.
             */
#if defined(SODIUM_NO_CXX11)
            stream<A> filter(const lambda1<bool, const A&>& pred) const
#else
            stream<A> filter(const std::function<bool(const A&)>& pred) const
#endif
            {
                transaction trans;
                return stream<A>(filter_(trans.impl(),
#if defined(SODIUM_NO_CXX11)
                    new impl::detype_pred<A>(pred)
#else
                    [pred] (const light_ptr& a) {
                        return pred(*a.cast_ptr<A>(NULL));
                    }
#endif
                  ));
            }

            /*!
             * Create a cell that holds at any given time the most recent value
             * that has arrived from this stream. Since cells must always have a current
             * value, you must supply an initial value that it has until the first stream
             * occurrence updates it.
             */
            cell<A> hold(const A& initA) const
            {
                transaction trans;
                return cell<A>(hold_(trans.impl(), light_ptr::create<A>(initA)));
            }

            cell<A> hold(A&& initA) const
            {
                transaction trans;
                return cell<A>(hold_(trans.impl(), light_ptr::create<A>(std::move(initA))));
            }

            cell<A> hold_lazy(const std::function<A()>& initA) const
            {
                transaction trans;
                return cell<A>(hold_lazy_(trans.impl(), [initA] () -> light_ptr { return light_ptr::create<A>(initA()); }));
            }

            /*!
             * Sample the cell's value as at the transaction before the
             * current one, i.e. no changes from the current transaction are
             * taken.
             */
            template <class B, class C>
#if defined(SODIUM_NO_CXX11)
            stream<C> snapshot(const cell<B>& beh, const lambda2<C, const A&, const B&>& combine) const
#else
            stream<C> snapshot(const cell<B>& beh, const std::function<C(const A&, const B&)>& combine) const
#endif
            {
                transaction trans;
                return stream<C>(snapshot_(trans.impl(), beh,
#if defined(SODIUM_NO_CXX11)
                    new impl::detype_snapshot<A,B,C>(combine)
#else
                    [combine] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                        return light_ptr::create<C>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<B>(NULL)));
                    }
#endif
                ));
            }

            /*!
             * Sample the cell's value as at the transaction before the
             * current one, i.e. no changes from the current transaction are
             * taken.
             */
            template <class B>
            stream<B> snapshot(const cell<B>& beh) const
            {
                return snapshot<B, B>(beh,
#if defined(SODIUM_NO_CXX11)
                    new snd_arg<A,B>
#else
                    [] (const A&, const B& b) { return b; }
#endif
                    );
            }

            /*!
             * Allow streams through only when the cell's value is true.
             */
            stream<A> gate(const cell<bool>& g) const
            {
                transaction trans;
                return filter_optional<A>(snapshot<bool, boost::optional<A>>(
                    g,
#if defined(SODIUM_NO_CXX11)
                    new impl::gate_handler<A>()
#else
                    [] (const A& a, const bool& gated) {
                        return gated ? boost::optional<A>(a) : boost::optional<A>();
                    }
#endif
                ));
            }

            /*!
             * Adapt an stream to a new stream statefully.  Always outputs one output for each
             * input.
             */
            template <class S, class B>
            stream<B> collect_lazy(
                const std::function<S()>& initS,
#if defined(SODIUM_NO_CXX11)
                const lambda2<SODIUM_TUPLE<B, S>, const A&, const S&>& f
#else
                const std::function<SODIUM_TUPLE<B, S>(const A&, const S&)>& f
#endif
            ) const
            {
                transaction trans1;
                SODIUM_SHARED_PTR<impl::collect_state<S> > pState(new impl::collect_state<S>(initS));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
                lambda0<void>* kill = listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new impl::collect_handler<A,S,B>(pState, f), false);
#else
                auto kill = listen_raw(trans1.impl(), std::get<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            auto outsSt = f(*ptr.cast_ptr<A>(NULL), pState->s_lazy());
                            const S& new_s = SODIUM_TUPLE_GET<1>(outsSt);
                            pState->s_lazy = [new_s] () { return new_s; };
                            send(target, trans2, light_ptr::create<B>(std::get<0>(outsSt)));
                        }), false);
#endif
                return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
            }

            /*!
             * Adapt an stream to a new stream statefully.  Always outputs one output for each
             * input.
             */
            template <class S, class B>
            stream<B> collect(
                const S& initS,
#if defined(SODIUM_NO_CXX11)
                const lambda2<SODIUM_TUPLE<B, S>, const A&, const S&>& f
#else
                const std::function<SODIUM_TUPLE<B, S>(const A&, const S&)>& f
#endif
            ) const
            {
                return collect_lazy<S,B>([initS] () -> S { return initS; }, f);
            }

            template <class B>
            stream<B> accum_e_lazy(
                const std::function<B()>& initB,
#if defined(SODIUM_NO_CXX11)
                const lambda2<B, const A&, const B&>& f
#else
                const std::function<B(const A&, const B&)>& f
#endif
            ) const
            {
                transaction trans1;
                SODIUM_SHARED_PTR<impl::collect_state<B> > pState(new impl::collect_state<B>(initB));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
                lambda0<void>* kill = listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new impl::accum_handler<A,B>(pState, f)
#else
                auto kill = listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            B b = f(*ptr.cast_ptr<A>(NULL), pState->s_lazy());
                            pState->s_lazy = [b] () { return b; };
                            send(target, trans2, light_ptr::create<B>(b));
                        })
#endif
                    , false);
                return stream<B>(SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill));
            }

            template <class B>
            stream<B> accum_e(
                const B& initB,
#if defined(SODIUM_NO_CXX11)
                const lambda2<B, const A&, const B&>& f
#else
                const std::function<B(const A&, const B&)>& f
#endif
            ) const
            {
                return accum_e_lazy<B>([initB] () -> B { return initB; }, f);
            }

            template <class B>
            cell<B> accum(
                const B& initB,
#if defined(SODIUM_NO_CXX11)
                const lambda2<B, const A&, const B&>& f
#else
                const std::function<B(const A&, const B&)>& f
#endif
            ) const
            {
                return accum_e(initB, f).hold(initB);
            }

            cell<int> count() const
            {
                return accum<int>(0,
#if defined(SODIUM_NO_CXX11)
                    new impl::count_handler<A>
#else
                    [] (const A&, const int& total) -> int {  return total+1; }
#endif
                );
            }

            stream<A> once() const
            {
                transaction trans;
                return stream<A>(once_(trans.impl()));
            }

            /*!
             * Delays each stream occurrence by putting it into a new transaction, using
             * the same method as split.
             */
            stream<A> delay()
            {
                return split<A>(map_<std::list<A> >(
#if defined(SODIUM_NO_CXX11)
                        new impl::delay_handler<A>
#else
                        [] (const A& a) -> std::list<A> { return { a }; }
#endif
                    ));
            }

            /*!
             * Add a clean-up operation to be performed when this stream is no longer
             * referenced.
             */
#if defined(SODIUM_NO_CXX11)
            stream<A> add_cleanup(const lambda0<void>& cleanup) const
#else
            stream<A> add_cleanup(const std::function<void()>& cleanup) const
#endif
            {
                transaction trans;
                return stream<A>(add_cleanup_(trans.impl(),
#if defined(SODIUM_NO_CXX11)
                    new lambda0<void>(cleanup)
#else
                    new std::function<void()>(cleanup)
#endif
                ));
            }
    };  // end class stream

    namespace impl {
        struct stream_sink_impl {
            stream_sink_impl();
            stream_ construct();
            void send(transaction_impl* trans, const light_ptr& ptr) const;
            SODIUM_SHARED_PTR<impl::node> target;
        };
    }

    namespace impl {
        template <class A, class L>
        stream<A> merge(const L& sas, size_t start, size_t end, const std::function<A(const A&, const A&)>& f) {
            size_t len = end - start;
            if (len == 0) return stream<A>(); else
            if (len == 1) return sas[start]; else
            if (len == 2) return sas[start].merge(sas[start+1], f); else {
                int mid = (start + end) / 2;
                return merge<A,L>(sas, start, mid, f).merge(merge<A,L>(sas, mid, end, f), f);
            }
        }
    }

    /*!
     * Variant of merge that merges a collection of streams.
     */
    template <class A, class L>
    stream<A> merge(const L& sas) {
        return impl::merge<A, L>(sas, 0, sas.size(), [] (const A& l, const A& r) { return r; });
    }

    /*!
     * Variant of merge that merges a collection of streams.
     */
    template <class A, class L>
    stream<A> merge(const L& sas, const std::function<A(const A&, const A&)>& f) {
        return impl::merge<A, L>(sas, 0, sas.size(), f);
    }

    /*!
     * An stream with a send() method to allow values to be pushed into it
     * from the imperative world.
     */
    template <class A>
    class stream_sink : public stream<A>
    {
        private:
            impl::stream_sink_impl impl;
            stream_sink(const impl::stream_& e) : stream<A>(e) {}

        public:
            stream_sink()
            {
                *static_cast<stream<A>*>(this) = stream<A>(impl.construct()).coalesce();
            }

            stream_sink(const std::function<A(const A&, const A&)>& f) {
                *static_cast<stream<A>*>(this) = stream<A>(impl.construct()).coalesce(f);
            }

            void send(const A& a) const {
                transaction trans;
                impl.send(trans.impl(), light_ptr::create<A>(a));
            }

            void send(A&& a) const {
                transaction trans;
                impl.send(trans.impl(), light_ptr::create<A>(std::move(a)));
            }
    };

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A>
        struct filter_optional_handler : public i_lambda3<void,const SODIUM_SHARED_PTR<node>&, transaction_impl*, const light_ptr&> {
            virtual void operator () (const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& poa) const {
                const boost::optional<A>& oa = *poa.cast_ptr<boost::optional<A> >(NULL);
                if (oa) send(target, trans, light_ptr::create<A>(oa.get()));
            }
        };
    }
#endif

    namespace impl {
        stream_ filter_optional_(transaction_impl* trans, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f);
    }

    /*!
     * Filter an stream of optionals, keeping only the defined values.
     */
    template <class A>
    stream<A> filter_optional(const stream<boost::optional<A>>& input)
    {
        transaction trans;
        return impl::filter_optional_(trans.impl(), input, [] (const light_ptr& poa) -> boost::optional<light_ptr> {
            const boost::optional<A>& oa = *poa.cast_ptr<boost::optional<A>>(NULL);
            if (oa)
                return boost::optional<light_ptr>(light_ptr::create<A>(oa.get()));
            else
                return boost::optional<light_ptr>();
        });
    }

    /*!
     * A cell with a send() method to allow its value to be changed
     * from the imperative world.
     */
    template <class A>
    class cell_sink : public cell<A>
    {
        private:
            stream_sink<A> e;

            cell_sink(const cell<A>& beh) : cell<A>(beh) {}

        public:
            cell_sink(const A& initA)
            {
                transaction trans;
                this->impl = SODIUM_SHARED_PTR<impl::cell_impl>(hold(trans.impl(), light_ptr::create<A>(initA), e));
            }

            cell_sink(A&& initA)
            {
                transaction trans;
                this->impl = SODIUM_SHARED_PTR<impl::cell_impl>(hold(trans.impl(), light_ptr::create<A>(std::move(initA)), e));
            }

            void send(const A& a) const
            {
                e.send(a);
            }

            void send(A&& a) const
            {
                e.send(std::move(a));
            }
    };

    namespace impl {
        /*!
         * Returns an stream describing the changes in a cell.
         */
        inline impl::stream_ underlying_stream(const impl::cell_& beh) {return beh.impl->updates;}
    };

    namespace impl {
        cell_ apply(transaction_impl* trans, const cell_& bf, const cell_& ba);
        
#if defined(SODIUM_NO_CXX11)
        template <class A, class B>
        struct apply_handler : i_lambda1<light_ptr, const light_ptr&> {
            virtual light_ptr operator () (const light_ptr& pf) const {
                const lambda1<B, const A&>& f = *pf.cast_ptr<lambda1<B, const A&> >(NULL);
                return light_ptr::create<lambda1<light_ptr, const light_ptr&> >(
                        SODIUM_DETYPE_FUNCTION1(A, B, f)
                    );
            }
        };
#endif
    };

    /*!
     * Apply a function contained in a cell to a cell value. This is the primitive
     * for all lifting of functions into cells.
     */
    template <class A, class B>
    cell<B> apply(
#if defined(SODIUM_NO_CXX11)
        const cell<lambda1<B, const A&>>& bf,
#else
        const cell<std::function<B(const A&)>>& bf,
#endif
        const cell<A>& ba)
    {
        transaction trans;
        return cell<B>(impl::apply(
            trans.impl(),
            impl::map_(trans.impl(),
#if defined(SODIUM_NO_CXX11)
                new impl::apply_handler<A,B>,
#else
                [] (const light_ptr& pf) -> light_ptr {
                    const std::function<B(const A&)>& f = *pf.cast_ptr<std::function<B(const A&)>>(NULL);
                    return light_ptr::create<std::function<light_ptr(const light_ptr&)> >(
                            SODIUM_DETYPE_FUNCTION1(A, B, f)
                        );
                },
#endif
                bf),
            ba
        ));
    }

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        struct stream_non_looped_kill : i_lambda0<void> {
            virtual void operator () () const {
            }
        };
        struct stream_loop_kill : i_lambda0<void> {
            stream_loop_kill(const SODIUM_SHARED_PTR<lambda0<void>*>& pKill) : pKill(pKill) {}
            SODIUM_SHARED_PTR<lambda0<void>*> pKill;
            virtual void operator () () const {
                lambda0<void>* kill = *pKill;
                if (kill)
                    (*kill)();
                delete kill;
            }
        };
    }
#endif

    /*!
     * Enable the construction of stream loops, like this. This gives the ability to
     * forward reference an stream.
     *
     *   stream_loop<A> ea;
     *   auto ea_out = do_something(ea);
     *   ea.loop(ea_out);  // ea is now the same as ea_out
     *
     * TO DO: Loops do not yet get deallocated properly.
     */
    template <class A>
    class stream_loop : public stream<A>
    {
        private:
            struct info {
                info(
#if defined(SODIUM_NO_CXX11)
                    const SODIUM_SHARED_PTR<lambda0<void>*>& pKill_
#else
                    const SODIUM_SHARED_PTR<std::function<void()>*>& pKill_
#endif
                )
                : pKill(pKill_), looped(false)
                {
                }
                SODIUM_SHARED_PTR<impl::node> target;
#if defined(SODIUM_NO_CXX11)
                SODIUM_SHARED_PTR<lambda0<void>*> pKill;
#else
                SODIUM_SHARED_PTR<std::function<void()>*> pKill;
#endif
                bool looped;
            };
            SODIUM_SHARED_PTR<info> i;

        private:
            stream_loop(const impl::stream_& ev, const SODIUM_SHARED_PTR<info>& i_) : stream<A>(ev), i(i_) {}

        public:
            stream_loop()
            {
#if defined(SODIUM_NO_CXX11)
                SODIUM_SHARED_PTR<lambda0<void>*> pKill(
                    new lambda0<void>*(new lambda0<void>(new impl::stream_non_looped_kill))
                );
#else
                SODIUM_SHARED_PTR<std::function<void()>*> pKill(
                    new std::function<void()>*(new std::function<void()>(
                        [] () {
                        }
                    ))
                );
#endif
                SODIUM_SHARED_PTR<info> i_(new info(pKill));

                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                i_->target = SODIUM_TUPLE_GET<1>(p);
                *this = stream_loop<A>(
                    SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
#if defined(SODIUM_NO_CXX11)
                        new lambda0<void>(new impl::stream_loop_kill(pKill))
#else
                        new std::function<void()>(
                            [pKill] () {
                                std::function<void()>* kill = *pKill;
                                if (kill)
                                    (*kill)();
                                delete kill;
                            }
                        )
#endif
                    ),
                    i_
                );
            }

            void loop(const stream<A>& e)
            {
                if (!i->looped) {
                    transaction trans;
                    SODIUM_SHARED_PTR<impl::node> target(i->target);
                    *i->pKill = e.listen_raw(trans.impl(), target, NULL, false);
                    i->looped = true;
                }
                else {
#if defined(SODIUM_NO_EXCEPTIONS)
                    abort();
#else
                    throw std::runtime_error("stream_loop looped back more than once");
#endif
                }
            }
    };

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        struct cell_non_looped_sample : i_lambda0<light_ptr> {
            virtual light_ptr operator () () const {
#if defined(SODIUM_NO_EXCEPTIONS)
                abort();
                return light_ptr();
#else
                throw std::runtime_error("cell_loop sampled before it was looped");
#endif
            }
        };
    }
#endif

    /*!
     * Enable the construction of cell loops, like this. This gives the ability to
     * forward reference a cell.
     *
     *   cell_loop<A> ba;
     *   auto ba_out = do_something(ea);
     *   ea.loop(ba_out);  // ba is now the same as ba_out
     *
     * TO DO: Loops do not yet get deallocated properly.
     */
    template <class A>
    class cell_loop : public cell<A>
    {
        private:
            stream_loop<A> elp;
            SODIUM_SHARED_PTR<SODIUM_SHARED_PTR<impl::cell_impl> > pLooped;

        public:
            cell_loop()
                : cell<A>(impl::cell_()),
                  pLooped(new SODIUM_SHARED_PTR<impl::cell_impl>)
            {
                this->impl = SODIUM_SHARED_PTR<impl::cell_impl>(new impl::cell_impl_loop(
                    elp,
                    pLooped,
                    SODIUM_SHARED_PTR<impl::cell_impl>()));
            }

            void loop(const cell<A>& b)
            {
                elp.loop(b.updates());
                *pLooped = b.impl;
                // TO DO: This keeps the memory allocated in a loop. Figure out how to
                // break the loop.
                this->impl->parent = b.impl;
            }
    };

    namespace impl {
        stream_ switch_s(transaction_impl* trans, const cell_& bea);
    }

    /*!
     * Flatten a cell that contains an stream to give an stream that reflects
     * the current state of the cell. Note that when an stream is updated,
     * due to cell's delay semantics, stream occurrences for the new
     * stream won't come through until the following transaction.
     */
    template <class A>
    stream<A> switch_s(const cell<stream<A>>& bea)
    {
        transaction trans;
        return stream<A>(impl::switch_s(trans.impl(), bea));
    }

    template <class A>
    stream<A> switch_e(const cell<stream<A>>& bea) __attribute__ ((deprecated));

    /*!
     * Deprecated old name.
     */
    template <class A>
    stream<A> switch_e(const cell<stream<A>>& bea)
    {
        return switch_s<A>(bea);
    }

    namespace impl {
        cell_ switch_c(transaction_impl* trans, const cell_& bba);
    }

    /*!
     * Cell variant of switch.
     */
    template <class A>
    cell<A> switch_c(const cell<cell<A>>& bba)
    {
        transaction trans;
        return cell<A>(impl::switch_c(trans.impl(), bba));
    }

    template <class A>
    cell<A> switch_b(const cell<cell<A>>& bba) __attribute__ ((deprecated));

    /*!
     * Cell variant of switch - deprecated old name.
     */
    template <class A>
    cell<A> switch_b(const cell<cell<A>>& bba)
    {
        return switch_c<A>(bba);
    }

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A, class B, class C>
        struct lift2_handler2 : i_lambda1<C, const B&> {
            lift2_handler2(const lambda2<C, const A&, const B&>& f, const A& a) : f(f), a(a) {}
            lambda2<C, const A&, const B&> f;
            A a;
            virtual C operator () (const B& b) const {
                return f(a, b);
            }
        };
        template <class A, class B, class C>
        struct lift2_handler1 : i_lambda1<lambda1<C, const B&>, const A&> {
            lift2_handler1(const lambda2<C, const A&, const B&>& f) : f(f) {}
            lambda2<C, const A&, const B&> f;
            virtual lambda1<C, const B&> operator () (const A& a) const {
                return new lift2_handler2<A, B, C>(f, a);
            }
        };
    }
#endif

    /*!
     * Lift a binary function into cells.
     */
    template <class A, class B, class C>
#if defined(SODIUM_NO_CXX11)
    cell<C> lift(const lambda2<C, const A&, const B&>& f, const cell<A>& ba, const cell<B>& bb)
#else
    cell<C> lift(const std::function<C(const A&, const B&)>& f, const cell<A>& ba, const cell<B>& bb)
#endif
    {
#if defined(SODIUM_NO_CXX11)
        lambda1<lambda1<C, const B&>, const A&> fa(
            new impl::lift2_handler1<A,B,C>(f)
        );
#else
        std::function<std::function<C(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<C(const B&)> {
                return [f, a] (const B& b) -> C { return f(a, b); };
            }
        );
#endif
        transaction trans;
        return apply<B, C>(ba.map_(fa), bb);
    }

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A, class B, class C, class D>
        struct lift3_handler3 : i_lambda1<C, const B&> {
            lift3_handler3(const lambda3<D, const A&, const B&, const C&>& f, const A& a, const B& b)
                : f(f), a(a), b(b) {}
            lambda3<D, const A&, const B&, const C&> f;
            A a;
            B b;
            virtual D operator () (const C& c) const {
                return f(a, b, c);
            }
        };
        template <class A, class B, class C, class D>
        struct lift3_handler2 : i_lambda1<lambda1<D, const C&>, const B&> {
            lift3_handler2(const lambda3<D, const A&, const B&, const C&>& f, const A& a) : f(f), a(a) {}
            lambda3<D, const A&, const B&, const C&> f;
            A a;
            virtual lambda1<D, const C&> operator () (const B& b) const {
                return new lift3_handler3<A, B, C, D>(f, a, b);
            }
        };
        template <class A, class B, class C, class D>
        struct lift3_handler1 : i_lambda1<lambda1<lambda1<D, const C&>, const B&>, const A&> {
            lift3_handler1(const lambda3<D, const A&, const B&, const C&>& f) : f(f) {}
            lambda3<D, const A&, const B&, const C&> f;
            virtual lambda1<lambda1<D, const C&>, const B&> operator () (const A& a) const {
                return new lift3_handler2<A, B, C, D>(f, a);
            }
        };
    }
#endif

    /*!
     * Lift a ternary function into cells.
     */
    template <class A, class B, class C, class D>
#if defined(SODIUM_NO_CXX11)
    cell<D> lift(const lambda3<D, const A&, const B&, const C&>& f,
#else
    cell<D> lift(const std::function<D(const A&, const B&, const C&)>& f,
#endif
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc
    )
    {
#if defined(SODIUM_NO_CXX11)
        lambda1<lambda1<lambda1<D, const C&>, const B&>, const A&> fa(
            new impl::lift3_handler1<A, B, C, D>(f)
        );
#else
        std::function<std::function<std::function<D(const C&)>(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<std::function<D(const C&)>(const B&)> {
                return [f, a] (const B& b) -> std::function<D(const C&)> {
                    return [f, a, b] (const C& c) -> D {
                        return f(a,b,c);
                    };
                };
            }
        );
#endif
        return apply(apply(ba.map_(fa), bb), bc);
    }

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A, class B, class C, class D, class E>
        struct lift4_handler4 : i_lambda1<E, const D&> {
            lift4_handler4(const lambda4<E, const A&, const B&, const C&, const D&>& f, const A& a, const B& b, const C& c)
                : f(f), a(a), b(b), c(c) {}
            lambda4<E, const A&, const B&, const C&, const D&> f;
            A a;
            B b;
            C c;
            virtual E operator () (const D& d) const {
                return f(a, b, c, d);
            }
        };
        template <class A, class B, class C, class D, class E>
        struct lift4_handler3 : i_lambda1<lambda1<E, const D&>, const C&> {
            lift4_handler3(const lambda4<E, const A&, const B&, const C&, const D&>& f, const A& a, const B& b)
                : f(f), a(a), b(b) {}
            lambda4<E, const A&, const B&, const C&, const D&> f;
            A a;
            B b;
            virtual E operator () (const C& c) const {
                return new lift4_handler4<A, B, C, D, E>(f, a, b, c);
            }
        };
        template <class A, class B, class C, class D, class E>
        struct lift4_handler2 : i_lambda1<lambda1<lambda1<E, const D&>, const C&>, const B&> {
            lift4_handler2(const lambda4<E, const A&, const B&, const C&, const D&>& f, const A& a) : f(f), a(a) {}
            lambda4<E, const A&, const B&, const C&, const D&> f;
            A a;
            virtual lambda1<D, const C&> operator () (const B& b) const {
                return new lift4_handler3<A, B, C, D, E>(f, a, b);
            }
        };
        template <class A, class B, class C, class D, class E>
        struct lift4_handler1 : i_lambda1<lambda1<lambda1<lambda1<E, const D&>, const C&>, const B&>, const A&> {
            lift4_handler1(const lambda4<E, const A&, const B&, const C&, const D&>& f) : f(f) {}
            lambda4<E, const A&, const B&, const C&, const D&> f;
            virtual lambda1<lambda1<D, const C&>, const B&> operator () (const A& a) const {
                return new lift4_handler2<A, B, C, D, E>(f, a);
            }
        };
    }
#endif

    /*!
     * Lift a quaternary function into cells.
     */
    template <class A, class B, class C, class D, class E>
#if defined(SODIUM_NO_CXX11)
    cell<E> lift(const lambda4<E, const A&, const B&, const C&, const D&>& f,
#else
    cell<E> lift(const std::function<E(const A&, const B&, const C&, const D&)>& f,
#endif
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd
    )
    {
#if defined(SODIUM_NO_CXX11)
        lambda1<lambda1<lambda1<lambda1<E, const D&>, const C&>, const B&>, const A&> fa(
            new impl::lift4_handler1<A,B,C,D,E>(f)
        );
#else
        std::function<std::function<std::function<std::function<E(const D&)>(const C&)>(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<std::function<std::function<E(const D&)>(const C&)>(const B&)> {
                return [f, a] (const B& b) -> std::function<std::function<E(const D&)>(const C&)> {
                    return [f, a, b] (const C& c) -> std::function<E(const D&)> {
                        return [f, a, b, c] (const D& d) -> E {
                            return f(a,b,c,d);
                        };
                    };
                };
            }
        );
#endif
        return apply(apply(apply(ba.map_(fa), bb), bc), bd);
    }

    /*!
     * Lift a 5-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F>
#if defined(SODIUM_NO_CXX11)
    cell<F> lift(const lambda5<F, const A&, const B&, const C&, const D&, const E&>& f,
#else
    cell<F> lift(const std::function<F(const A&, const B&, const C&, const D&, const E&)>& f,
#endif
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be
    )
    {
#if defined(SODIUM_NO_CXX11)
        lambda1<lambda1<lambda1<lambda1<lambda1<lambda1<F, const E&>, const D&>, const C&>, const B&>, const A&>> fa(
            new impl::lift5_handler1<A,B,C,D,E,F>(f)
        );
#else
        std::function<std::function<std::function<std::function<std::function<F(const E&)>(const D&)>(const C&)>(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<std::function<std::function<std::function<F(const E&)>(const D&)>(const C&)>(const B&)> {
                return [f, a] (const B& b) -> std::function<std::function<std::function<F(const E&)>(const D&)>(const C&)> {
                    return [f, a, b] (const C& c) -> std::function<std::function<F(const E&)>(const D&)> {
                        return [f, a, b, c] (const D& d) -> std::function<F(const E&)> {
                            return [f, a, b, c, d] (const E& e) -> F {
                                return f(a,b,c,d,e);
                            };
                        };
                    };
                };
            }
        );
#endif
        return apply(apply(apply(apply(ba.map_(fa), bb), bc), bd), be);
    }

    /*!
     * Lift a 6-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F, class G>
#if defined(SODIUM_NO_CXX11)
    cell<G> lift(const lambda6<G, const A&, const B&, const C&, const D&, const E&, const F&>& fn,
#else
    cell<G> lift(const std::function<G(const A&, const B&, const C&, const D&, const E&, const F&)>& fn,
#endif
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf
    )
    {
#if defined(SODIUM_NO_CXX11)
        *** TO DO
#else
        std::function<std::function<std::function<std::function<std::function<std::function<G(const F&)>(const E&)>(const D&)>(const C&)>(const B&)>(const A&)> fa(
            [fn] (const A& a) -> std::function<std::function<std::function<std::function<std::function<G(const F&)>(const E&)>(const D&)>(const C&)>(const B&)> {
                return [fn, a] (const B& b) -> std::function<std::function<std::function<std::function<G(const F&)>(const E&)>(const D&)>(const C&)> {
                    return [fn, a, b] (const C& c) -> std::function<std::function<std::function<G(const F&)>(const E&)>(const D&)> {
                        return [fn, a, b, c] (const D& d) -> std::function<std::function<G(const F&)>(const E&)> {
                            return [fn, a, b, c, d] (const E& e) -> std::function<G(const F&)> {
                                return [fn, a, b, c, d, e] (const F& f) -> G {
                                    return fn(a,b,c,d,e,f);
                                };
                            };
                        };
                    };
                };
            }
        );
#endif
        return apply(apply(apply(apply(apply(ba.map_(fa), bb), bc), bd), be), bf);
    }

    /*!
     * Lift a 7-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F, class G, class H>
#if defined(SODIUM_NO_CXX11)
    cell<H> lift(const lambda7<H, const A&, const B&, const C&, const D&, const E&, const F&, const G&>& fn,
#else
    cell<H> lift(const std::function<H(const A&, const B&, const C&, const D&, const E&, const F&, const G&)>& fn,
#endif
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf,
        const cell<G>& bg
    )
    {
#if defined(SODIUM_NO_CXX11)
        *** TO DO
#else
        std::function<std::function<std::function<std::function<std::function<std::function<std::function<H(const G&)>(const F&)>(const E&)>(const D&)>(const C&)>(const B&)>(const A&)> fa(
            [fn] (const A& a) -> std::function<std::function<std::function<std::function<std::function<std::function<H(const G&)>(const F&)>(const E&)>(const D&)>(const C&)>(const B&)> {
                return [fn, a] (const B& b) -> std::function<std::function<std::function<std::function<std::function<H(const G&)>(const F&)>(const E&)>(const D&)>(const C&)> {
                    return [fn, a, b] (const C& c) -> std::function<std::function<std::function<std::function<H(const G&)>(const F&)>(const E&)>(const D&)> {
                        return [fn, a, b, c] (const D& d) -> std::function<std::function<std::function<H(const G&)>(const F&)>(const E&)> {
                            return [fn, a, b, c, d] (const E& e) -> std::function<std::function<H(const G&)>(const F&)> {
                                return [fn, a, b, c, d, e] (const F& f) -> std::function<H(const G&)> {
                                    return [fn, a, b, c, d, e, f] (const G& g) {
                                        return fn(a,b,c,d,e,f,g);
                                    };
                                };
                            };
                        };
                    };
                };
            }
        );
#endif
        return apply(apply(apply(apply(apply(apply(ba.map_(fa), bb), bc), bd), be), bf), bg);
    }

#if defined(SODIUM_NO_CXX11)
    namespace impl {
        template <class A>
        struct split_post : i_lambda0<void> {
            split_post(const std::list<A>& la, const SODIUM_SHARED_PTR<impl::node>& target)
            : la(la), target(target) {}
            std::list<A> la;
            SODIUM_SHARED_PTR<impl::node> target;
            virtual void operator () () const {
                for (typename std::list<A>::iterator it = la.begin(); it != la.end(); ++it) {
                    transaction trans;
                    send(target, trans.impl(), light_ptr::create<A>(*it));
                }
            }
        };
        template <class A>
        struct split_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&> {
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& ptr) const {
                const std::list<A>& la = *ptr.cast_ptr<std::list<A> >(NULL);
                trans->part->post(new split_post<A>(la, target));
            }
        };
    }
#endif

    /*!
     * Take each list item and put it into a new transaction of its own.
     *
     * An example use case of this might be a situation where we are splitting
     * a block of input data into frames. We obviously want each frame to have
     * its own transaction so that state is updated separately for each frame.
     */
    template <class A>
    stream<A> split(const stream<std::list<A>>& e)
    {
        SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
        transaction trans1;
#if defined(SODIUM_NO_CXX11)
        lambda0<void>* kill = e.listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
            new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&>(
                new impl::split_handler<A>
            )
#else
        auto kill = e.listen_raw(trans1.impl(), std::get<1>(p),
            new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                [] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                    const std::list<A>& la = *ptr.cast_ptr<std::list<A>>(NULL);
                    trans2->part->post([la, target] () {
                        for (auto it = la.begin(); it != la.end(); ++it) {
                            transaction trans3;
                            send(target, trans3.impl(), light_ptr::create<A>(*it));
                        }
                    });
                })
#endif
            , false);
        return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
    }

    // New type names:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    template <class A>
    struct event : stream<A> {
        event() {}
        event(const stream<A>& other) : stream<A>(other) {}
    } __attribute__ ((deprecated));
    template <class A>
    struct event_sink : stream_sink<A> {
        event_sink() {}
        event_sink(const std::function<A(const A&, const A&)>& f) : stream_sink<A>(f) {}
        event_sink(const stream_sink<A>& other) : stream_sink<A>(other) {}
    } __attribute__ ((deprecated));
    template <class A>
    struct event_loop : stream_loop<A> {
        event_loop() {}
        event_loop(const stream_loop<A>& other) : event_loop<A>(other) {}
    } __attribute__ ((deprecated));
    template <class A>
    struct behavior : cell<A> {
        behavior(const A& initValue) : cell<A>(initValue) {}
        behavior(const cell<A>& other) : cell<A>(other) {}
    } __attribute__ ((deprecated));
    template <class A>
    struct behavior_sink : cell_sink<A> {
        behavior_sink(const A& initValue) : cell_sink<A>(initValue) {}
    } __attribute__ ((deprecated));
    template <class A>
    struct behavior_loop : cell_loop<A> {
        behavior_loop() {}
    } __attribute__ ((deprecated));
#pragma GCC diagnostic pop
}  // end namespace sodium
#endif

