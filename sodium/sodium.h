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
    cell<B> apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
    template <class A>
    stream<A> filter_optional(const stream<boost::optional<A>>& input);
    template <class A>
    stream<A> split(const stream<std::list<A>>& e);
    template <class A>
    stream<A> switch_s(const cell<stream<A>>& bea);
    template <class T>
    cell<typename T::time> clock(const T& t);

    template <class A>
    class lazy {
    private:
        std::function<A()> f;
    public:
        lazy(const A& a) : f([a] () -> A { return a; }) {}
        lazy(const std::function<A()>& f_) : f(f_) {}
        A operator () () const { return f(); }

        template <typename Fn>
        lazy<typename std::result_of<Fn(A)>::type> map(const Fn& fn) const {
            typedef typename std::result_of<Fn(A)>::type B;
            const auto& a(*this);
            return lazy<B>([fn, a] () { return fn(a()); });
        }

        template <typename B, typename Fn>
        lazy<typename std::result_of<Fn(A,B)>::type> lift(const lazy<B>& b, const Fn& fn) const {
            typedef typename std::result_of<Fn(A,B)>::type C;
            const auto& a(*this);
            return lazy<C>([fn, a, b] () { return fn(a(), b()); });
        }

        template <typename B, typename C, typename Fn>
        lazy<typename std::result_of<Fn(A,B,C)>::type> lift(const lazy<B>& b, const lazy<C>& c, const Fn& fn) const {
            typedef typename std::result_of<Fn(A,B,C)>::type D;
            const auto& a(*this);
            return lazy<D>([fn, a, b, c] () { return fn(a(), b(), c()); });
        }

        template <typename B, typename C, typename D, typename Fn>
        lazy<typename std::result_of<Fn(A,B,C,D)>::type> lift(const lazy<B>& b, const lazy<C>& c, const lazy<D>& d, const Fn& fn) const {
            typedef typename std::result_of<Fn(A,B,C,D)>::type E;
            const auto& a(*this);
            return lazy<E>([fn, a, b, c, d] () { return fn(a(), b(), c(), d()); });
        }
    };

    namespace impl {

        class cell_;
        struct cell_impl;

        class stream_ {
        friend class cell_;
        template <class A> friend class sodium::stream;
        template <class A> friend class sodium::stream_loop;
        template <class A> friend class sodium::cell;
        friend cell_ switch_c(transaction_impl* trans, const cell_& bba);
        friend SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_& input);
        friend SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_& input);
        template <class A, class B>
        friend cell<B> sodium::apply(const cell<std::function<B(const A&)>>& bf, const cell<A>& ba);
        friend cell_ apply(transaction_impl* trans0, const cell_& bf, const cell_& ba);
        friend stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ev);
        friend cell_ map_(transaction_impl* trans,
            const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh);
        friend stream_ switch_s(transaction_impl* trans, const cell_& bea);
        template <class A>
        friend stream<A> sodium::split(const stream<std::list<A>>& e);
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
            std::function<void()>* listen_raw(
                        transaction_impl* trans0,
                        const SODIUM_SHARED_PTR<impl::node>& target,
                        std::function<void(const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&)>* handle,
                        bool suppressEarlierFirings) const;

            /*!
             * This is far more efficient than add_cleanup because it modifies the stream
             * in place.
             */
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup)
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
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2)
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
            stream_ unsafe_add_cleanup(std::function<void()>* cleanup1, std::function<void()>* cleanup2, std::function<void()>* cleanup3)
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
            stream_ add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const;
            cell_ hold_(transaction_impl* trans, const light_ptr& initA) const;
            cell_ hold_lazy_(transaction_impl* trans, const std::function<light_ptr()>& initA) const;
            stream_ once_(transaction_impl* trans) const;
            stream_ merge_(transaction_impl* trans, const stream_& other) const;
            stream_ coalesce_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
            stream_ last_firing_only_(transaction_impl* trans) const;
            stream_ snapshot_(transaction_impl* trans, const cell_& beh, const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine) const;
            stream_ filter_(transaction_impl* trans, const std::function<bool(const light_ptr&)>& pred) const;

            std::function<void()>* listen_impl(
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
        #define SODIUM_DETYPE_FUNCTION1(A,B,f) \
                   [f] (const light_ptr& a) -> light_ptr { \
                        return light_ptr::create<B>(f(*a.cast_ptr<A>(NULL))); \
                   }
        stream_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f, const stream_& ca);

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

            std::function<void()>* kill;
            SODIUM_SHARED_PTR<cell_impl> parent;

            std::function<std::function<void()>(transaction_impl*, const SODIUM_SHARED_PTR<node>&,
                             const std::function<void(transaction_impl*, const light_ptr&)>&)> listen_value_raw() const;
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
                    SODIUM_THROW("cell_loop sampled before it was looped");
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

        cell_ map_(transaction_impl* trans, const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh);
    }  // end namespace impl

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
        friend cell<BB> apply(const cell<std::function<BB(const AA&)>>& bf, const cell<AA>& ba);
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

            lazy<A> sample_lazy() const {
                const SODIUM_SHARED_PTR<impl::cell_impl>& impl_(this->impl);
                return lazy<A>([impl_] () -> A {
                    transaction trans;
                    return *impl_->sample().template cast_ptr<A>(NULL);
                });
            }

            /*!
             * Returns a new cell with the specified cleanup added to it, such that
             * it will be executed when no copies of the new cell are referenced.
             */
            cell<A> add_cleanup(const std::function<void()>& cleanup) const {
                transaction trans;
                return updates().add_cleanup(cleanup).hold(sample());
            }

            /*!
             * Map a function over this cell to modify the output value.
             */
            template <typename Fn>
            cell<typename std::result_of<Fn(A)>::type> map(const Fn& f) const {
                typedef typename std::result_of<Fn(A)>::type B;
                transaction trans;
                return cell<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Lift a binary function into cells.
             */
            template <class B, class Fn>
            cell<typename std::result_of<Fn(A,B)>::type> lift(const cell<B>& bb, const Fn& f) const
            {
                typedef typename std::result_of<Fn(A,B)>::type C;
                std::function<std::function<C(const B&)>(const A&)> fa(
                    [f] (const A& a) -> std::function<C(const B&)> {
                        return [f, a] (const B& b) -> C { return f(a, b); };
                    }
                );
                transaction trans;
                return apply<B, C>(map(fa), bb);
            }

            /*!
             * Lift a ternary function into cells.
             */
            template <class B, class C, class Fn>
            cell<typename std::result_of<Fn(A,B,C)>::type> lift(
                const cell<B>& bb,
                const cell<C>& bc,
                const Fn& f
            ) const
            {
                typedef typename std::result_of<Fn(A,B,C)>::type D;
                std::function<std::function<std::function<D(const C&)>(const B&)>(const A&)> fa(
                    [f] (const A& a) -> std::function<std::function<D(const C&)>(const B&)> {
                        return [f, a] (const B& b) -> std::function<D(const C&)> {
                            return [f, a, b] (const C& c) -> D {
                                return f(a,b,c);
                            };
                        };
                    }
                );
                return apply(apply(map(fa), bb), bc);
            }

            /*!
             * Lift a quaternary function into cells.
             */
            template <class B, class C, class D, class Fn>
            cell<typename std::result_of<Fn(A,B,C,D)>::type> lift(
                const cell<B>& bb,
                const cell<C>& bc,
                const cell<D>& bd,
                const Fn& f
            ) const
            {
                typedef typename std::result_of<Fn(A,B,C,D)>::type E;
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
                return apply(apply(apply(map(fa), bb), bc), bd);
            }

            /*!
             * Lift a 5-argument function into cells.
             */
            template <class B, class C, class D, class E, class Fn>
            cell<typename std::result_of<Fn(A,B,C,D,E)>::type> lift(
                const cell<B>& bb,
                const cell<C>& bc,
                const cell<D>& bd,
                const cell<E>& be,
                const Fn& f
            ) const
            {
                typedef typename std::result_of<Fn(A,B,C,D,E)>::type F;
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
                return apply(apply(apply(apply(map(fa), bb), bc), bd), be);
            }
        
            /*!
             * Lift a 6-argument function into cells.
             */
            template <class B, class C, class D, class E, class F, class Fn>
            cell<typename std::result_of<Fn(A,B,C,D,E,F)>::type> lift(
                const cell<A>& ba,
                const cell<B>& bb,
                const cell<C>& bc,
                const cell<D>& bd,
                const cell<E>& be,
                const cell<F>& bf,
                const Fn& fn
            ) const
            {
                typedef typename std::result_of<Fn(A,B,C,D,E,F)>::type G;
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
                return apply(apply(apply(apply(apply(map(fa), bb), bc), bd), be), bf);
            }

            /*!
             * Lift a 7-argument function into cells.
             */
            template <class B, class C, class D, class E, class F, class G, class Fn>
            cell<typename std::result_of<Fn(A,B,C,D,E,F,G)>::type> lift(
                const cell<A>& ba,
                const cell<B>& bb,
                const cell<C>& bc,
                const cell<D>& bd,
                const cell<E>& be,
                const cell<F>& bf,
                const cell<G>& bg,
                const Fn& fn
            ) const
            {
                typedef typename std::result_of<Fn(A,B,C,D,E,F,G)>::type H;
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
                return apply(apply(apply(apply(apply(apply(map(fa), bb), bc), bd), be), bf), bg);
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
                return stream<A>(value_(trans.impl())).coalesce([] (const A&, const A& b) { return b; });
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
                return stream<A>(value_(trans.impl())).coalesce([] (const A&, const A& b) { return b; }).listen(handle);
            }

            /**
             * Transform a cell with a generalized state loop (a mealy machine). The function
             * is passed the input and the old state and returns the new state and output value.
             *
             * The supplied function should have the signature std::tuple<B, S>(A, S), where B
             * is the return cell's type, and S is the state type.
             */
            template <class S, class Fn>
            cell<typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type> collect_lazy(
                const lazy<S>& initS,
                const Fn& f
            ) const
            {
                typedef typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type B;
                transaction trans1;
                auto ea = updates().coalesce([] (const A&, const A& snd) -> A { return snd; });
                lazy<A> za_lazy = sample_lazy();
                std::function<SODIUM_TUPLE<B,S>()> zbs = [za_lazy, initS, f] () -> SODIUM_TUPLE<B,S> {
                    return f(za_lazy(), initS());
                };
                SODIUM_SHARED_PTR<lazy<S> > pState(new lazy<S>([zbs] () -> S {
                    return SODIUM_TUPLE_GET<1>(zbs());
                }));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                auto kill = updates().listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            SODIUM_TUPLE<B,S> outsSt = f(*ptr.cast_ptr<A>(NULL), (*pState)());
                            const S& new_s = SODIUM_TUPLE_GET<1>(outsSt);
                            *pState = lazy<S>(new_s);
                            send(target, trans2, light_ptr::create<B>(SODIUM_TUPLE_GET<0>(outsSt)));
                        }), false);
                return stream<B>(SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill)).hold_lazy(
                    lazy<B>([zbs] () -> B {
                        return SODIUM_TUPLE_GET<0>(zbs());
                    }
                ));
            }

            /**
             * Transform a cell with a generalized state loop (a mealy machine). The function
             * is passed the input and the old state and returns the new state and output value.
             *
             * The supplied function should have the signature std::tuple<B, S>(A, S), where B
             * is the return cell's type, and S is the state type.
             */
            template <class S, class Fn>
            cell<typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type> collect(
                const S& initS,
                const Fn& f
            ) const
            {
                return collect_lazy<S, Fn>(lazy<S>(initS), f);
            }

    };  // end class cell

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
            std::function<void()> listen(const std::function<void(const A&)>& handle) const {
                transaction trans1;
                std::function<void()>* pKill = listen_raw(trans1.impl(),
                    SODIUM_SHARED_PTR<impl::node>(new impl::node(SODIUM_IMPL_RANK_T_MAX)),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [handle] (const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr& ptr) {
                            handle(*ptr.cast_ptr<A>(NULL));
                        }), false);
                if (pKill != NULL) {
                    std::function<void()> kill(*pKill);
                    delete pKill;
                    return kill;
                }
                else
                    return [] () {};
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
            template <typename Fn>
            stream<typename std::result_of<Fn(A)>::type> map(const Fn& f) const {
                typedef typename std::result_of<Fn(A)>::type B;
                transaction trans;
                return stream<B>(impl::map_(trans.impl(), SODIUM_DETYPE_FUNCTION1(A,B,f), *this));
            }

            /*!
             * Map a function over this stream that always outputs a constant value.
             */
            template <class B>
            stream<B> map_to(const B& value) {
                return map([value] (const A&) -> B { return value; });
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
             * Merge a stream of unary functions with a signature like Arg(Arg), in the
             * simultaneous case, composing them, with the function on the left going first.
             */
            template <class Arg>
            stream<A> merge_functions(const stream<A>& s) const {
                return merge(s, [] (const A& f, const A& g) -> A {
                    return [f, g] (Arg a) -> Arg {
                        return g(f(a));
                    };
                });
            }

        private:
            /*!
             * If there's more than one firing in a single transaction, combine them into
             * one using the specified combining function.
             *
             * If the stream firings are ordered, then the first will appear at the left
             * input of the combining function. In most common cases it's best not to
             * make any assumptions about the ordering, and the combining function would
             * ideally be commutative.
             */
            stream<A> coalesce(const std::function<A(const A&, const A&)>& combine) const
            {
                transaction trans;
                return stream<A>(coalesce_(trans.impl(),
                    [combine] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                        return light_ptr::create<A>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<A>(NULL)));
                    }
                ));
            }

        public:
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
            stream<A> merge(const stream<A>& s, const std::function<A(const A&, const A&)>& f) const
            {
                transaction trans;
                return stream<A>(merge_(trans.impl(), s)).coalesce(f);
            }

            /*!
             * Filter this stream based on the specified predicate, passing through values
             * where the predicate returns true.
             */
            stream<A> filter(const std::function<bool(const A&)>& pred) const
            {
                transaction trans;
                return stream<A>(filter_(trans.impl(),
                    [pred] (const light_ptr& a) {
                        return pred(*a.cast_ptr<A>(NULL));
                    }
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

            cell<A> hold_lazy(const lazy<A>& initA) const
            {
                transaction trans;
                return cell<A>(hold_lazy_(trans.impl(), [initA] () -> light_ptr { return light_ptr::create<A>(initA()); }));
            }

            /*!
             * Sample the cell's value as at the transaction before the
             * current one, i.e. no changes from the current transaction are
             * taken.
             */
            template <class B, class Fn>
            stream<typename std::result_of<Fn(A,B)>::type> snapshot(const cell<B>& beh, const Fn& combine) const
            {
                typedef typename std::result_of<Fn(A,B)>::type C;
                transaction trans;
                return stream<C>(snapshot_(trans.impl(), beh,
                    [combine] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                        return light_ptr::create<C>(combine(*a.cast_ptr<A>(NULL), *b.cast_ptr<B>(NULL)));
                    }
                ));
            }

            template <class B, class C, class Fn>
            stream<typename std::result_of<Fn(A,B,C)>::type> snapshot(
                const cell<B>& bc, const cell<C>& cc, const Fn& f) const
            {
                typedef typename std::result_of<Fn(A,B,C)>::type D;
                return snapshot(bc, [cc, f] (const A& a, const B& b) {
                    return f(a, b, cc.sample());
                });
            }

            template <class B, class C, class D, class Fn>
            stream<typename std::result_of<Fn(A,B,C,D)>::type> snapshot(
                const cell<B>& bc, const cell<C>& cc, const cell<D>& cd,
                const Fn& f) const
            {
                typedef typename std::result_of<Fn(A,B,C,D)>::type E;
                return snapshot(bc, [cc, cd, f] (const A& a, const B& b) {
                    return f(a, b, cc.sample(), cd.sample());
                });
            }

            template <class B, class C, class D, class E, class Fn>
            stream<typename std::result_of<Fn(A,B,C,D,E)>::type> snapshot(
                const cell<B>& bc, const cell<C>& cc, const cell<D>& cd, const cell<E>& ce,
                const Fn& f) const
            {
                typedef typename std::result_of<Fn(A,B,C,D,E)>::type F;
                return snapshot(bc, [cc, cd, ce, f] (const A& a, const B& b) {
                    return f(a, b, cc.sample(), cd.sample(), ce.sample());
                });
            }

            template <class B, class C, class D, class E, class F, class Fn>
            stream<typename std::result_of<Fn(A,B,C,D,E,F)>::type> snapshot(
                const cell<B>& bc, const cell<C>& cc, const cell<D>& cd, const cell<E>& ce, const cell<F>& cf,
                const Fn& f) const
            {
                typedef typename std::result_of<Fn(A,B,C,D,E,F)>::type G;
                return snapshot(bc, [cc, cd, ce, cf, f] (const A& a, const B& b) {
                    return f(a, b, cc.sample(), cd.sample(), ce.sample(), cf.sample());
                });
            }

            /*!
             * Sample the cell's value as at the transaction before the
             * current one, i.e. no changes from the current transaction are
             * taken.
             */
            template <class B>
            stream<B> snapshot(const cell<B>& beh) const
            {
                return snapshot(beh,
                    [] (const A&, const B& b) { return b; }
                    );
            }

            /*!
             * Allow streams through only when the cell's value is true.
             */
            stream<A> gate(const cell<bool>& g) const
            {
                transaction trans;
                return filter_optional<A>(snapshot(
                    g,
                    [] (const A& a, const bool& gated) {
                        return gated ? boost::optional<A>(a) : boost::optional<A>();
                    }
                ));
            }

            /*!
             * Adapt an stream to a new stream statefully.  Always outputs one output for each
             * input.
             *
             * The supplied function should have the signature std::tuple<B, S>(A, S), where B
             * is the return cell's type, and S is the state type.
             */
            template <class S, class Fn>
            stream<typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type> collect_lazy(
                const lazy<S>& initS,
                const Fn& f
            ) const
            {
                typedef typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type B;
                transaction trans1;
                SODIUM_SHARED_PTR<lazy<S> > pState(new lazy<S>(initS));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                auto kill = listen_raw(trans1.impl(), std::get<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            auto outsSt = f(*ptr.cast_ptr<A>(NULL), (*pState)());
                            const S& new_s = SODIUM_TUPLE_GET<1>(outsSt);
                            *pState = lazy<S>(new_s);
                            send(target, trans2, light_ptr::create<B>(std::get<0>(outsSt)));
                        }), false);
                return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
            }

            /*!
             * Adapt an stream to a new stream statefully.  Always outputs one output for each
             * input.
             *
             * The supplied function should have the signature std::tuple<B, S>(A, S), where B
             * is the return cell's type, and S is the state type.
             */
            template <class S, class Fn>
            stream<typename std::tuple_element<0,typename std::result_of<Fn(A,S)>::type>::type> collect(
                const S& initS,
                const Fn& f
            ) const
            {
                return collect_lazy<S,Fn>(lazy<S>(initS), f);
            }

            template <class B>
            stream<B> accum_s_lazy(
                const lazy<B>& initB,
                const std::function<B(const A&, const B&)>& f
            ) const
            {
                transaction trans1;
                SODIUM_SHARED_PTR<lazy<B> > pState(new lazy<B>(initB));
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                auto kill = listen_raw(trans1.impl(), SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                        [pState, f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            B b = f(*ptr.cast_ptr<A>(NULL), (*pState)());
                            *pState = lazy<B>(b);
                            send(target, trans2, light_ptr::create<B>(b));
                        })
                    , false);
                return stream<B>(SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill));
            }

            template <class B>
            stream<B> accum_s(
                const B& initB,
                const std::function<B(const A&, const B&)>& f
            ) const
            {
                return accum_s_lazy<B>(lazy<B>(initB), f);
            }

            /*!
             * Renamed to accum_s.
             */
            template <class B>
            stream<B> accum_e(
                const B& initB,
                const std::function<B(const A&, const B&)>& f
            ) const __attribute__ ((deprecated));

            template <class B>
            cell<B> accum(
                const B& initB,
                const std::function<B(const A&, const B&)>& f
            ) const
            {
                return accum_s(initB, f).hold(initB);
            }

            /*!
             * Variant of accum_s that accumulates a stream of functions.
             */
            template<class B>
            stream<B> accum_s(const B& initA) const
            {
                return this->accum_s<B>(initA, [] (const A& f, const B& a) -> B {
                    return f(a);
                });
            }

            /*!
             * Variant of accum_s that accumulates a stream of functions - lazy variant.
             */
            template<class B>
            stream<B> accum_s_lazy(const lazy<B>& initA) const
            {
                return this->accum_s_lazy<B>(initA, [] (const A& f, const B& a) -> B {
                    return f(a);
                });
            }

            /*!
             * Variant of accum_s that accumulates a stream of functions.
             */
            template<class B>
            cell<B> accum(const B& initA) const
            {
                transaction trans;
                return this->accum_s<B>(initA).hold(initA);
            }

            /*!
             * Variant of accum_s that accumulates a stream of functions.
             */
            template<class B>
            cell<B> accum_lazy(const lazy<B>& initA) const
            {
                transaction trans;
                return this->accum_s_lazy<B>(initA).hold_lazy(initA);
            }

            cell<int> count() const
            {
                return accum<int>(0,
                    [] (const A&, const int& total) -> int {  return total+1; }
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
            stream<A> defer() const
            {
                return split<A>(map(
                        [] (const A& a) -> std::list<A> { return { a }; }
                    ));
            }

            /*!
             * Delays each stream occurrence by putting it into a new transaction, using
             * the same method as split.
             * Renamed to defer();
             */
            stream<A> delay() const __attribute__ ((deprecated))
            {
                return this->defer();
            }

            /*!
             * Add a clean-up operation to be performed when this stream is no longer
             * referenced.
             */
            stream<A> add_cleanup(const std::function<void()>& cleanup) const
            {
                transaction trans;
                return stream<A>(add_cleanup_(trans.impl(),
                    new std::function<void()>(cleanup)
                ));
            }
    };  // end class stream

    template <class A>
    template <class B>
    stream<B> stream<A>::accum_e(
        const B& initB,
        const std::function<B(const A&, const B&)>& f
    ) const
    {
        return accum_s<B>(initB, f);
    }

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
                *static_cast<stream<A>*>(this) = stream<A>(impl.construct()).coalesce(
                    [] (const A&, const A& b) { return b; });
            }

            stream_sink(const std::function<A(const A&, const A&)>& f) {
                *static_cast<stream<A>*>(this) = stream<A>(impl.construct()).coalesce(f);
            }

            void send(const A& a) const {
                transaction trans;
                if (trans.impl()->inCallback > 0)
                    SODIUM_THROW("You are not allowed to use send() inside a Sodium callback");
                impl.send(trans.impl(), light_ptr::create<A>(a));
            }

            void send(A&& a) const {
                transaction trans;
                if (trans.impl()->inCallback > 0)
                    SODIUM_THROW("You are not allowed to use send() inside a Sodium callback");
                impl.send(trans.impl(), light_ptr::create<A>(std::move(a)));
            }
    };

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
    };

    /*!
     * Apply a function contained in a cell to a cell value. This is the primitive
     * for all lifting of functions into cells.
     */
    template <class A, class B>
    cell<B> apply(
        const cell<std::function<B(const A&)>>& bf,
        const cell<A>& ba)
    {
        transaction trans;
        return cell<B>(impl::apply(
            trans.impl(),
            impl::map_(trans.impl(),
                [] (const light_ptr& pf) -> light_ptr {
                    const std::function<B(const A&)>& f = *pf.cast_ptr<std::function<B(const A&)>>(NULL);
                    return light_ptr::create<std::function<light_ptr(const light_ptr&)> >(
                            SODIUM_DETYPE_FUNCTION1(A, B, f)
                        );
                },
                bf),
            ba
        ));
    }

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
                    const SODIUM_SHARED_PTR<std::function<void()>*>& pKill_
                )
                : pKill(pKill_), looped(false)
                {
                }
                SODIUM_SHARED_PTR<impl::node> target;
                SODIUM_SHARED_PTR<std::function<void()>*> pKill;
                bool looped;
            };
            SODIUM_SHARED_PTR<info> i;

        private:
            stream_loop(const impl::stream_& ev, const SODIUM_SHARED_PTR<info>& i_) : stream<A>(ev), i(i_) {}

        public:
            stream_loop()
            {
                SODIUM_SHARED_PTR<std::function<void()>*> pKill(
                    new std::function<void()>*(new std::function<void()>(
                        [] () {
                        }
                    ))
                );
                SODIUM_SHARED_PTR<info> i_(new info(pKill));

                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                i_->target = SODIUM_TUPLE_GET<1>(p);
                *this = stream_loop<A>(
                    SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                        new std::function<void()>(
                            [pKill] () {
                                std::function<void()>* kill = *pKill;
                                if (kill)
                                    (*kill)();
                                delete kill;
                            }
                        )
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
                    SODIUM_THROW("stream_loop looped back more than once");
#endif
                }
            }
    };

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

    template <class A, class B, class C>
    cell<C> lift(const std::function<C(const A&, const B&)>& f, const cell<A>& ba, const cell<B>& bb)
     __attribute__ ((deprecated));

    /*!
     * Lift a binary function into cells.
     */
    template <class A, class B, class C>
    cell<C> lift(const std::function<C(const A&, const B&)>& f, const cell<A>& ba, const cell<B>& bb)
    {
        std::function<std::function<C(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<C(const B&)> {
                return [f, a] (const B& b) -> C { return f(a, b); };
            }
        );
        transaction trans;
        return apply<B, C>(ba.map(fa), bb);
    }

    template <class A, class B, class C, class D>
    cell<D> lift(const std::function<D(const A&, const B&, const C&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc
    )  __attribute__ ((deprecated));

    /*!
     * Lift a ternary function into cells.
     */
    template <class A, class B, class C, class D>
    cell<D> lift(const std::function<D(const A&, const B&, const C&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc
    )
    {
        std::function<std::function<std::function<D(const C&)>(const B&)>(const A&)> fa(
            [f] (const A& a) -> std::function<std::function<D(const C&)>(const B&)> {
                return [f, a] (const B& b) -> std::function<D(const C&)> {
                    return [f, a, b] (const C& c) -> D {
                        return f(a,b,c);
                    };
                };
            }
        );
        return apply(apply(ba.map(fa), bb), bc);
    }

    template <class A, class B, class C, class D, class E>
    cell<E> lift(const std::function<E(const A&, const B&, const C&, const D&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd
    ) __attribute__ ((deprecated));

    /*!
     * Lift a quaternary function into cells.
     */
    template <class A, class B, class C, class D, class E>
    cell<E> lift(const std::function<E(const A&, const B&, const C&, const D&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd
    )
    {
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
        return apply(apply(apply(ba.map(fa), bb), bc), bd);
    }

    template <class A, class B, class C, class D, class E, class F>
    cell<F> lift(const std::function<F(const A&, const B&, const C&, const D&, const E&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be
    ) __attribute__ ((deprecated));

    /*!
     * Lift a 5-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F>
    cell<F> lift(const std::function<F(const A&, const B&, const C&, const D&, const E&)>& f,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be
    )
    {
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
        return apply(apply(apply(apply(ba.map(fa), bb), bc), bd), be);
    }

    template <class A, class B, class C, class D, class E, class F, class G>
    cell<G> lift(const std::function<G(const A&, const B&, const C&, const D&, const E&, const F&)>& fn,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf
    ) __attribute__ ((deprecated));

    /*!
     * Lift a 6-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F, class G>
    cell<G> lift(const std::function<G(const A&, const B&, const C&, const D&, const E&, const F&)>& fn,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf
    )
    {
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
        return apply(apply(apply(apply(apply(ba.map(fa), bb), bc), bd), be), bf);
    }

    template <class A, class B, class C, class D, class E, class F, class G, class H>
    cell<H> lift(const std::function<H(const A&, const B&, const C&, const D&, const E&, const F&, const G&)>& fn,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf,
        const cell<G>& bg
    ) __attribute__ ((deprecated));

    /*!
     * Lift a 7-argument function into cells.
     */
    template <class A, class B, class C, class D, class E, class F, class G, class H>
    cell<H> lift(const std::function<H(const A&, const B&, const C&, const D&, const E&, const F&, const G&)>& fn,
        const cell<A>& ba,
        const cell<B>& bb,
        const cell<C>& bc,
        const cell<D>& bd,
        const cell<E>& be,
        const cell<F>& bf,
        const cell<G>& bg
    )
    {
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
        return apply(apply(apply(apply(apply(apply(ba.map(fa), bb), bc), bd), be), bf), bg);
    }

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

