/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#include <sodium/sodium.h>

using namespace std;
using namespace boost;


namespace sodium {

    namespace impl {

        stream_::stream_()
        {
        }

        /*!
         * listen to streams.
         */
#if defined(SODIUM_NO_CXX11)
        lambda0<void>* stream_::listen_raw(
#else
        std::function<void()>* stream_::listen_raw(
#endif
                    transaction_impl* trans,
                    const SODIUM_SHARED_PTR<impl::node>& target,
#if defined(SODIUM_NO_CXX11)
                    lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>* handler,
#else
                    std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>* handler,
#endif
                    bool suppressEarlierFirings) const
        {
            SODIUM_SHARED_PTR<holder> h(new holder(handler));
            return listen_impl(trans, target, h, suppressEarlierFirings);
        }

        cell_ stream_::hold_(transaction_impl* trans, const light_ptr& initA) const
        {
            return cell_(
                SODIUM_SHARED_PTR<impl::cell_impl>(impl::hold(trans, initA, *this))
            );
        }

        cell_ stream_::hold_lazy_(transaction_impl* trans, const std::function<light_ptr()>& initA) const
        {
            return cell_(
                SODIUM_SHARED_PTR<impl::cell_impl>(impl::hold_lazy(trans, initA, *this))
            );
        }

#if defined(SODIUM_NO_CXX11)
        #define KILL_ONCE(ppKill) \
            do { \
                lambda0<void>* pKill = *ppKill; \
                if (pKill != NULL) { \
                    *ppKill = NULL; \
                    (*pKill)(); \
                    delete pKill; \
                } \
            } while (0)

        struct once_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            once_handler(const SODIUM_SHARED_PTR<lambda0<void>*>& ppKill) : ppKill(ppKill) {}
            SODIUM_SHARED_PTR<lambda0<void>*> ppKill;

            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) const {
                if (*ppKill) {
                    send(target, trans, ptr);
                    KILL_ONCE(ppKill);
                }
            }
        };
        struct once_killer : i_lambda0<void> {
            once_killer(const SODIUM_SHARED_PTR<lambda0<void>*>& ppKill) : ppKill(ppKill) {}
            SODIUM_SHARED_PTR<lambda0<void>*> ppKill;

            virtual void operator () () const {
                KILL_ONCE(ppKill);
            }
        };
#else
        #define KILL_ONCE(ppKill) \
            do { \
                function<void()>* pKill = *ppKill; \
                if (pKill != NULL) { \
                    *ppKill = NULL; \
                    (*pKill)(); \
                    delete pKill; \
                } \
            } while (0)
#endif

        stream_ stream_::once_(transaction_impl* trans1) const
        {
#if defined(SODIUM_NO_CXX11)
            SODIUM_SHARED_PTR<lambda0<void>*> ppKill(new lambda0<void>*(NULL));
#else
            SODIUM_SHARED_PTR<function<void()>*> ppKill(new function<void()>*(NULL));
#endif

            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            *ppKill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
#if defined(SODIUM_NO_CXX11)
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new once_handler(ppKill)
                ),
#else
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [ppKill] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                        if (*ppKill) {
                            send(target, trans2, ptr);
                            KILL_ONCE(ppKill);
                        }
                    }),
#endif
                false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
#if defined(SODIUM_NO_CXX11)
                new lambda0<void>(new once_killer(ppKill))
#else
                new std::function<void()>([ppKill] () {
                    KILL_ONCE(ppKill);
                })
#endif
            );
        }

#if defined(SODIUM_NO_CXX11)
        struct send_task : i_lambda1<void, transaction_impl*> {
            send_task(const SODIUM_SHARED_PTR<impl::node>& target, const light_ptr& value)
            : target(target), value(value) {}
            SODIUM_SHARED_PTR<impl::node> target;
            light_ptr value;
            virtual void operator () (transaction_impl* trans_impl) const {
                sodium::impl::send(target, trans_impl, value);
            }
        };
        struct merge_listen : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& a) const {
                trans->prioritized(target, new send_task(target, a));
            }
        };
#endif

        stream_ stream_::merge_(transaction_impl* trans1, const stream_& other) const {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            SODIUM_SHARED_PTR<impl::node> left(new impl::node);
            const SODIUM_SHARED_PTR<impl::node>& right = SODIUM_TUPLE_GET<1>(p);
            char* h = new char;
            if (left->link(h, right))
                trans1->to_regen = true;
            // defer right side to make sure merge is left-biased
#if defined(SODIUM_NO_CXX11)
*** TO DO
#else
            auto kill1 = this->listen_raw(trans1, left,
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [right] (const std::shared_ptr<impl::node>&, impl::transaction_impl* trans2, const light_ptr& a) {
                        send(right, trans2, a);
                    }), false);
            auto kill2 = other.listen_raw(trans1, right, NULL, false);
            auto kill3 = new std::function<void()>([left, h] () {
                left->unlink(h);
                delete h;
            });
#endif
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill1, kill2, kill3);
        }

        struct coalesce_state {
            coalesce_state() {}
            ~coalesce_state() {}
            boost::optional<light_ptr> oValue;
        };
#if defined(SODIUM_NO_CXX11)
        struct coalesce_prioritized : i_lambda1<void, transaction_impl*> {
            coalesce_prioritized(const SODIUM_SHARED_PTR<impl::node>& target,
                                 const SODIUM_SHARED_PTR<coalesce_state>& pState)
            : target(target), pState(pState) {}
            SODIUM_SHARED_PTR<impl::node> target;
            SODIUM_SHARED_PTR<coalesce_state> pState;
            virtual void operator () (transaction_impl* trans) const {
                if (pState->oValue) {
                    send(target, trans, pState->oValue.get());
                    pState->oValue = boost::optional<light_ptr>();
                }
            }
        };
        struct coalesce_listen : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            coalesce_listen(
                const SODIUM_SHARED_PTR<coalesce_state>& pState,
                const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine)
            : pState(pState), combine(combine) {}
            SODIUM_SHARED_PTR<coalesce_state> pState;
            lambda2<light_ptr, const light_ptr&, const light_ptr&> combine;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& ptr) const {
                if (!pState->oValue) {
                    pState->oValue = boost::optional<light_ptr>(ptr);
                    trans->prioritized(target, new coalesce_prioritized(target, pState));
                }
                else
                    pState->oValue = make_optional(combine(pState->oValue.get(), ptr));
            }
        };
#endif

        stream_ stream_::coalesce_(transaction_impl* trans1,
#if defined(SODIUM_NO_CXX11)
                const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine
#else
                const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine
#endif
            ) const
        {
            SODIUM_SHARED_PTR<coalesce_state> pState(new coalesce_state);
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new coalesce_listen(pState, combine)
                ), false);
#else
            auto kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pState, combine] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                        if (!pState->oValue) {
                            pState->oValue = boost::optional<light_ptr>(ptr);
                            trans2->prioritized(target, [target, pState] (transaction_impl* trans3) {
                                if (pState->oValue) {
                                    send(target, trans3, pState->oValue.get());
                                    pState->oValue = boost::optional<light_ptr>();
                                }
                            });
                        }
                        else
                            pState->oValue = make_optional(combine(pState->oValue.get(), ptr));
                    }), false);
#endif
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        stream_ stream_::last_firing_only_(transaction_impl* trans) const
        {
#if defined(SODIUM_NO_CXX11)
            return coalesce_(trans, new snd_arg<light_ptr,light_ptr>);
#else
            return coalesce_(trans, [] (const light_ptr& fst, const light_ptr& snd) {
                return snd;
            });
#endif
        }

#if defined(SODIUM_NO_CXX11)
        struct snapshot_listen : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            snapshot_listen(
                const cell_& beh,
                const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine)
            : beh(beh), combine(combine) {}
            cell_ beh;
            lambda2<light_ptr, const light_ptr&, const light_ptr&> combine;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& a) const {
                send(target, trans, combine(a, beh.impl->sample()));
            }
        };
#endif

        /*!
         * Sample the cell's value as at the transaction before the
         * current one, i.e. no changes from the current transaction are
         * taken.
         */
        stream_ stream_::snapshot_(transaction_impl* trans1, const cell_& beh,
#if defined(SODIUM_NO_CXX11)
                const lambda2<light_ptr, const light_ptr&, const light_ptr&>& combine
#else
                const std::function<light_ptr(const light_ptr&, const light_ptr&)>& combine
#endif
            ) const
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new snapshot_listen(beh, combine)
                ), false);
#else
            auto kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [beh, combine] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& a) {
                        send(target, trans2, combine(a, beh.impl->sample()));
                    }), false);
#endif
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

#if defined(SODIUM_NO_CXX11)
        struct filter_listen : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            filter_listen(const lambda1<bool, const light_ptr&>& pred)
            : pred(pred) {}
            lambda1<bool, const light_ptr&> pred;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& ptr) const {
                if (pred(ptr)) send(target, trans, ptr);
            }
        };
#endif

        /*!
         * Filter this stream based on the specified predicate, passing through values
         * where the predicate returns true.
         */
        stream_ stream_::filter_(transaction_impl* trans1,
#if defined(SODIUM_NO_CXX11)
                const lambda1<bool, const light_ptr&>& pred
#else
                const std::function<bool(const light_ptr&)>& pred
#endif
            ) const
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new filter_listen(pred)
                ), false);
#else
            auto kill = listen_raw(trans1, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [pred] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            if (pred(ptr)) send(target, trans2, ptr);
                        }), false);
#endif
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

#if defined(SODIUM_NO_CXX11)
        struct cell_const_sample : i_lambda0<light_ptr> {
            cell_const_sample(const light_ptr& a) : a(a) {}
            light_ptr a;
            light_ptr operator () () const { return a; }
        };
#endif

        cell_impl::cell_impl()
            : updates(stream_()),
              kill(NULL)
        {
        }

        cell_impl::cell_impl(
            const stream_& updates_,
            const SODIUM_SHARED_PTR<cell_impl>& parent_)
            : updates(updates_), kill(NULL), parent(parent_)
        {
        }

        cell_impl::~cell_impl()
        {
            if (kill) {
                (*kill)();
                delete kill;
            }
        }
        
#if defined(SODIUM_NO_CXX11)
        struct clear_firings : i_lambda0<void> {
            clear_firings(const SODIUM_SHARED_PTR<node>& n) : n(n) {}
            SODIUM_SHARED_PTR<node> n;
            virtual void operator () () const {
                n->firings.clear();
            }
        };
#endif

        /*!
         * Function to push a value into an stream
         */
        void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans1, const light_ptr& a)
        {
            if (n->firings.begin() == n->firings.end())
#if defined(SODIUM_NO_CXX11)
                trans1->last(new clear_firings(n));
#else
                trans1->last([n] () {
                    n->firings.clear();
                });
#endif
            n->firings.push_front(a);
            SODIUM_FORWARD_LIST<node::target>::iterator it = n->targets.begin();
            while (it != n->targets.end()) {
                node::target* f = &*it;
                trans1->prioritized(f->n, [f, a] (transaction_impl* trans2) {
                    ((holder*)f->h)->handle(f->n, trans2, a);
                });
                it++;
            }
        }

#if defined(SODIUM_NO_CXX11)
        struct unregister : i_lambda0<void> {
            unregister(partition* part,
                       const SODIUM_WEAK_PTR<node>& n_weak,
                       holder* h)
            : part(part), n_weak(n_weak), h(h) {}
            partition* part;
            SODIUM_WEAK_PTR<node> n_weak;
            holder* h;
            virtual void operator () () const {
                SODIUM_SHARED_PTR<node> n = n_weak.lock();
                if (n) {
#if !defined(SODIUM_SINGLE_THREADED)
                    part->mx.lock();
#endif
                    n->unlink(h);
#if !defined(SODIUM_SINGLE_THREADED)
                    part->mx.unlock();
#endif
                }
                delete h;
            }
        };
        struct listen_impl : i_lambda4<lambda0<void>*,
                transaction_impl*,
                const SODIUM_SHARED_PTR<impl::node>&,
                const SODIUM_SHARED_PTR<holder>&,
                bool> {
            listen_impl(const SODIUM_WEAK_PTR<node>& n_weak) : n_weak(n_weak) {}
            SODIUM_WEAK_PTR<node> n_weak;
            virtual lambda0<void>* operator () (transaction_impl* trans,
                        const SODIUM_SHARED_PTR<node>& target,
                        const SODIUM_SHARED_PTR<holder>& h,
                        bool suppressEarlierFirings) const {  // Register listener
                SODIUM_SHARED_PTR<node> n = n_weak.lock();
                if (n) {
                    SODIUM_FORWARD_LIST<light_ptr> firings;
                    holder* h = new holder(handler);   *** TO DO: BRING THIS UP-TO-DATE relative to C++11
                    {
#if !defined(SODIUM_SINGLE_THREADED)
                        trans->part->mx.lock();
#endif
                        if (n->link(h, target))
                            trans->to_regen = true;
#if !defined(SODIUM_SINGLE_THREADED)
                        trans->part->mx.unlock();
#endif
                        firings = n->firings;
                    }
                    if (!suppressEarlierFirings && firings.begin() != firings.end())
                        for (SODIUM_FORWARD_LIST<light_ptr>::iterator it = firings.begin(); it != firings.end(); it++)
                            h->handle(target, trans, *it);
                    return new lambda0<void>(new unregister(trans->part, n_weak, h));
                }
                else {
                    delete handler;
                    return NULL;
                }
            }
        };
#endif

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        SODIUM_TUPLE<stream_, SODIUM_SHARED_PTR<node> > unsafe_new_stream()
        {
            SODIUM_SHARED_PTR<node> n1(new node);
            SODIUM_WEAK_PTR<node> n_weak(n1);
            boost::intrusive_ptr<listen_impl_func<H_STRONG> > impl(
#if defined(SODIUM_NO_CXX11)
                new listen_impl_func<H_STRONG>(new listen_impl_func<H_STRONG>::closure(new listen_impl(n_weak)))
            );
#else
                new listen_impl_func<H_STRONG>(new listen_impl_func<H_STRONG>::closure([n_weak] (transaction_impl* trans1,
                        const SODIUM_SHARED_PTR<node>& target,
                        const SODIUM_SHARED_PTR<holder>& h,
                        bool suppressEarlierFirings) -> std::function<void()>* {  // Register listener
                    SODIUM_SHARED_PTR<node> n2 = n_weak.lock();
                    if (n2) {
#if !defined(SODIUM_SINGLE_THREADED)
                        trans1->part->mx.lock();
#endif
                        if (n2->link(h.get(), target))
                            trans1->to_regen = true;
#if !defined(SODIUM_SINGLE_THREADED)
                        trans1->part->mx.unlock();
#endif
                        if (!suppressEarlierFirings && n2->firings.begin() != n2->firings.end()) {
                            SODIUM_FORWARD_LIST<light_ptr> firings = n2->firings;
                            trans1->prioritized(target, [target, h, firings] (transaction_impl* trans2) {
                                for (SODIUM_FORWARD_LIST<light_ptr>::const_iterator it = firings.begin(); it != firings.end(); it++)
                                    h->handle(target, trans2, *it);
                            });
                        }
                        partition* part = trans1->part;
                        SODIUM_SHARED_PTR<holder>* h_keepalive = new SODIUM_SHARED_PTR<holder>(h);
                        return new std::function<void()>([part, n_weak, h_keepalive] () {  // Unregister listener
                            impl::transaction_ trans2(part);
                            trans2.impl()->last([n_weak, h_keepalive] () {
                                std::shared_ptr<node> n3 = n_weak.lock();
                                if (n3)
                                    n3->unlink((*h_keepalive).get());
                                delete h_keepalive;
                            });
                        });
                    }
                    else
                        return NULL;
                }))
            );
#endif
            n1->listen_impl = boost::intrusive_ptr<listen_impl_func<H_NODE> >(
                reinterpret_cast<listen_impl_func<H_NODE>*>(impl.get()));
            boost::intrusive_ptr<listen_impl_func<H_STREAM> > li_stream(
                reinterpret_cast<listen_impl_func<H_STREAM>*>(impl.get()));
            return SODIUM_MAKE_TUPLE(stream_(li_stream), n1);
        }

        stream_sink_impl::stream_sink_impl()
        {
        }

        stream_ stream_sink_impl::construct()
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            this->target = SODIUM_TUPLE_GET<1>(p);
            return SODIUM_TUPLE_GET<0>(p);
        }

        void stream_sink_impl::send(transaction_impl* trans, const light_ptr& value) const
        {
            sodium::impl::send(target, trans, value);
        }

#if defined(SODIUM_NO_CXX11)
        struct hold_update_task : i_lambda0<void> {
            hold_update_task(const SODIUM_SHARED_PTR<cell_state>& state) : state(state) {}
            SODIUM_SHARED_PTR<cell_state> state;
            virtual void operator () () const {
                state->finalize();
            }
        };
        struct hold_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            hold_handler(const SODIUM_SHARED_PTR<cell_state>& state) : state(state) {}
            SODIUM_SHARED_PTR<cell_state> state;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) const {
                bool first = !state->update;
                state->update = boost::optional<light_ptr>(ptr);
                if (first)
                    trans->last(new hold_update_task(state));
                send(target, trans, ptr);
            }
        };
        struct hold_update_task_lazy : i_lambda0<void> {
            hold_update_task(const SODIUM_SHARED_PTR<cell_state_lazy>& state) : state(state) {}
            SODIUM_SHARED_PTR<cell_state_lazy> state;
            virtual void operator () () const {
                state->finalize();
            }
        };
        struct hold_handler_lazy : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            hold_handler(const SODIUM_SHARED_PTR<cell_state>& state) : state(state) {}
            SODIUM_SHARED_PTR<cell_state> state;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) const {
                bool first = !state->update;
                state->update = boost::optional<light_ptr>(ptr);
                if (first)
                    trans->last(new hold_update_task_lazy(state));
                send(target, trans, ptr);
            }
        };
#endif

        SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_& input)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            if (input.is_never())
                return SODIUM_SHARED_PTR<cell_impl>(new cell_impl_constant(initValue));
            else {
#endif
                cell_state state(initValue);
                SODIUM_SHARED_PTR<cell_impl_concrete<cell_state> > impl(
                    new cell_impl_concrete<cell_state>(input, state, std::shared_ptr<cell_impl>())
                );
                impl->kill =
                    input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
#if defined(SODIUM_NO_CXX11)
                    new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                        new hold_handler(state)
                    )
#else
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [impl] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) {
                            bool first = !impl->state.update;
                            impl->state.update = boost::optional<light_ptr>(ptr);
                            if (first)
                                trans->last([impl] () { impl->state.finalize(); });
                            send(target, trans, ptr);
                        })
#endif
                    , false);
                return static_pointer_cast<cell_impl, cell_impl_concrete<cell_state>>(impl);
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_& input)
        {
            cell_state_lazy state(initValue);
            SODIUM_SHARED_PTR<cell_impl_concrete<cell_state_lazy> > impl(
                new cell_impl_concrete<cell_state_lazy>(input, state, std::shared_ptr<cell_impl>())
            );
            impl->kill =
                input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
#if defined(SODIUM_NO_CXX11)
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new hold_handler_lazy(state)
                )
#else
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [impl] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& ptr) {
                        bool first = !impl->state.update;
                        impl->state.update = boost::optional<light_ptr>(ptr);
                        if (first)
                            trans->last([impl] () { impl->state.finalize(); });
                        send(target, trans, ptr);
                    })
#endif
                , false);
            return static_pointer_cast<cell_impl, cell_impl_concrete<cell_state_lazy>>(impl);
        }

        cell_::cell_()
        {
        }

        cell_::cell_(cell_impl* impl_)
            : impl(impl_)
        {
        }

        cell_::cell_(const SODIUM_SHARED_PTR<cell_impl>& impl_)
            : impl(impl_)
        {
        }

        cell_::cell_(const light_ptr& a)
            : impl(new cell_impl_constant(a))
        {
        }

        stream_ cell_::value_(transaction_impl* trans) const
        {
            SODIUM_TUPLE<stream_,SODIUM_SHARED_PTR<node> > p = unsafe_new_stream();
            const stream_& eSpark = std::get<0>(p);
            const SODIUM_SHARED_PTR<node>& node = std::get<1>(p);
            send(node, trans, light_ptr::create<unit>(unit()));
            stream_ eInitial = eSpark.snapshot_(trans, *this,
                [] (const light_ptr& a, const light_ptr& b) -> light_ptr {
                    return b;
                }
            );
            return eInitial.merge_(trans, impl->updates).last_firing_only_(trans);
        }

#if defined(SODIUM_CONSTANT_OPTIMIZATION)
        /*!
         * For optimization, if this cell is a constant, then return its value.
         */
        boost::optional<light_ptr> cell_::get_constant_value() const
        {
            return impl->updates.is_never() ? boost::optional<light_ptr>(impl->sample())
                                            : boost::optional<light_ptr>();
        }
#endif

        struct applicative_state {
            applicative_state() : fired(false) {}
            bool fired;
            boost::optional<light_ptr> f;
            boost::optional<light_ptr> a;
        };

#if defined(SODIUM_NO_CXX11)
        struct apply_const_a : i_lambda1<light_ptr, const light_ptr&> {
            apply_const_a(const light_ptr& a) : a(a) {}
            light_ptr a;
            virtual light_ptr operator () (const light_ptr& pf) const {
                const lambda1<light_ptr, const light_ptr&>& f =
                    *pf.cast_ptr<lambda1<light_ptr, const light_ptr&> >(NULL);
                return f(a);
            }
        };
#endif

        cell_ apply(transaction_impl* trans0, const cell_& bf, const cell_& ba)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ocf = bf.get_constant_value();
            if (ocf) { // function is constant
#if defined(SODIUM_NO_CXX11)
                lambda1<light_ptr, const light_ptr&> f = *ocf.get().cast_ptr<lambda1<light_ptr, const light_ptr&> >(NULL);
#else
                auto f = *ocf.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
#endif
                return impl::map_(trans0, f, ba);  // map optimizes to a constant where ba is constant
            }
            else {
                boost::optional<light_ptr> oca = ba.get_constant_value();
                if (oca) {  // 'a' value is constant but function is not
                    const light_ptr& a = oca.get();
#if defined(SODIUM_NO_CXX11)
                    return impl::map_(trans0, new apply_const_a(a), bf);
#else
                    return impl::map_(trans0, [a] (const light_ptr& pf) -> light_ptr {
                        const std::function<light_ptr(const light_ptr&)>& f =
                            *pf.cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                        return f(a);
                    }, bf);
#endif
                }
                else {
#endif
                    // Non-constant case
                    SODIUM_SHARED_PTR<applicative_state> state(new applicative_state);

                    SODIUM_SHARED_PTR<impl::node> in_target(new impl::node);
                    SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                    const SODIUM_SHARED_PTR<impl::node>& out_target = SODIUM_TUPLE_GET<1>(p);
                    char* h = new char;
                    if (in_target->link(h, out_target))
                        trans0->to_regen = true;
#if defined(SODIUM_NO_CXX11)
   *** // TO DO
#else
                    auto output = [state, out_target] (transaction_impl* trans) {
                        auto f = *state->f.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                        send(out_target, trans, f(state->a.get()));
                        state->fired = false;
                    };
                    auto kill1 = bf.value_(trans0).listen_raw(trans0, in_target,
                            new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                                [state, out_target, output] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& f) {
                                    state->f = f;
                                    if (state->a) {
                                        if (state->fired) return;
                                        state->fired = true;
                                        trans->prioritized(out_target, output);
                                    }
                                }
                            ), false);
                    auto kill2 = ba.value_(trans0).listen_raw(trans0, in_target,
                            new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                                [state, out_target, output] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& a) {
                                    state->a = a;
                                    if (state->f) {
                                        if (state->fired) return;
                                        state->fired = true;
                                        trans->prioritized(out_target, output);
                                    }
                                }
                            ), false);
                    auto kill3 = new std::function<void()>([in_target, h] () {
                        in_target->unlink(h);
                        delete h;
                    });
#endif
                    return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill1, kill2, kill3).hold_lazy_(
                        trans0, [bf, ba] () -> light_ptr {
                            auto f = *bf.impl->sample().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                            return f(ba.impl->sample());
                        }
                    );
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
                }
            }
#endif
        }

#if defined(SODIUM_NO_CXX11)
        struct send_wrapper : i_lambda3<void, const SODIUM_SHARED_PTR<node>&, transaction_impl*, const light_ptr&> {
            virtual void operator () (const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans, const light_ptr& ptr) const {
                send(n, trans, ptr);
            }
        };
#endif

#if defined(SODIUM_NO_CXX11)
        stream_ stream_::add_cleanup_(transaction_impl* trans, lambda0<void>* cleanup) const
#else
        stream_ stream_::add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const
#endif
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = listen_raw(trans, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new send_wrapper
                ),
#else
            auto kill = listen_raw(trans, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(send),
#endif
                    false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill, cleanup);
        }

#if defined(SODIUM_NO_CXX11)
        struct map_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            map_handler(const lambda1<light_ptr, const light_ptr&>& f)
            : f(f) {}
            lambda1<light_ptr, const light_ptr&> f;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& ptr) const {
                send(target, trans, f(ptr));
            }
        };
#endif

        /*!
         * Map a function over this stream to modify the output value.
         */
        stream_ map_(transaction_impl* trans1,
#if defined(SODIUM_NO_CXX11)
            const lambda1<light_ptr, const light_ptr&>& f,
#else
            const std::function<light_ptr(const light_ptr&)>& f,
#endif
            const stream_& ev)
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = ev.listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new map_handler(f)), false);
#else
            auto kill = ev.listen_raw(trans1, std::get<1>(p),
                    new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                        [f] (const std::shared_ptr<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& ptr) {
                            send(target, trans2, f(ptr));
                        }), false);
#endif
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        cell_ map_(transaction_impl* trans,
#if defined(SODIUM_NO_CXX11)
            const lambda1<light_ptr, const light_ptr&>& f,
#else
            const std::function<light_ptr(const light_ptr&)>& f,
#endif
            const cell_& beh) {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ca = beh.get_constant_value();
            if (ca)
                return cell_(f(ca.get()));
            else {
#endif
                auto impl = beh.impl;
                return map_(trans, f, beh.updates_()).hold_lazy_(trans, [f, impl] () -> light_ptr {
                    return f(impl->sample());
                });
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

#if defined(SODIUM_NO_CXX11)
        struct switch_s_task : public i_lambda0<void> {
            switch_s_task(const SODIUM_SHARED_PTR<lambda0<void>*>& pKillInner,
                          const stream_& ea,
                          impl::transaction_impl* trans1,
                          const SODIUM_SHARED_PTR<impl::node>& target)
            : pKillInner(pKillInner), ea(ea), trans1(trans1), target(target) {}
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner;
            stream_ ea;
            impl::transaction_impl* trans1;
            SODIUM_SHARED_PTR<impl::node> target;
            virtual void operator () () const {
                KILL_ONCE(pKillInner);
                *pKillInner = ea.listen_raw(trans1, target, NULL, true);
            }
        };
        struct switch_s_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            switch_s_handler(const SODIUM_SHARED_PTR<lambda0<void>*>& pKillInner) : pKillInner(pKillInner) {}
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans1, const light_ptr& pea) const {
                const stream_& ea = *pea.cast_ptr<stream_>(NULL);
                trans1->last(new switch_s_task(pKillInner, ea, trans1, target));
            }
        };
        struct switch_s_kill : i_lambda0<void> {
            switch_s_kill(const SODIUM_SHARED_PTR<lambda0<void>*>& pKillInner) : pKillInner(pKillInner) {}
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner;
            virtual void operator () () const {
                KILL_ONCE(pKillInner);
            }
        };
#endif

        stream_ switch_s(transaction_impl* trans0, const cell_& bea)
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            const SODIUM_SHARED_PTR<impl::node>& target1 = SODIUM_TUPLE_GET<1>(p);
#if defined(SODIUM_NO_CXX11)
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner(new lambda0<void>*(NULL));
#else
            std::shared_ptr<function<void()>*> pKillInner(new function<void()>*(NULL));
#endif
            trans0->prioritized(target1, [pKillInner, bea, target1] (transaction_impl* trans) {
                if (*pKillInner == NULL)
                    *pKillInner = bea.impl->sample().cast_ptr<stream_>(NULL)->listen_raw(trans, target1, NULL, false);
            });

#if defined(SODIUM_NO_CXX11)
            lambda0<void>* killOuter = bea.updates_().listen_raw(trans0, target1,
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new switch_s_handler(pKillInner)
                ),
#else
            auto killOuter = bea.updates_().listen_raw(trans0, target1,
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pKillInner] (const std::shared_ptr<impl::node>& target2, impl::transaction_impl* trans1, const light_ptr& pea) {
                        const stream_& ea = *pea.cast_ptr<stream_>(NULL);
                        trans1->last([pKillInner, ea, target2, trans1] () {
                            KILL_ONCE(pKillInner);
                            *pKillInner = ea.listen_raw(trans1, target2, NULL, true);
                        });
                    }),
#endif
                false
            );
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
#if defined(SODIUM_NO_CXX11)
                new lambda0<void>(new switch_s_kill(pKillInner))
#else
                new std::function<void()>([pKillInner] {
                    KILL_ONCE(pKillInner);
                })
#endif
                , killOuter);
        }

#if defined(SODIUM_NO_CXX11)
        struct switch_c_handler : i_lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&> {
            switch_c_handler(const SODIUM_SHARED_PTR<lambda0<void>*>& pKillInner) : pKillInner(pKillInner) {}
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner;
            virtual void operator () (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans, const light_ptr& pa) const {
                // Note: If any switch takes place during a transaction, then the
                // value().listen will always cause a sample to be fetched from the
                // one we just switched to. The caller will be fetching our output
                // using value().listen, and value() throws away all firings except
                // for the last one. Therefore, anything from the old input behaviour
                // that might have happened during this transaction will be suppressed.
                KILL_ONCE(pKillInner);
                const cell_& ba = *pa.cast_ptr<cell_>(NULL);
                *pKillInner = ba.value_(trans).listen_raw(trans, target, NULL, false);
            }
        };
#endif

        cell_ switch_c(transaction_impl* trans0, const cell_& bba)
        {
            auto za = [bba] () -> light_ptr { return bba.impl->sample().cast_ptr<cell_>(NULL)->impl->sample(); };
#if defined(SODIUM_NO_CXX11)
            SODIUM_SHARED_PTR<lambda0<void>*> pKillInner(new lambda0<void>*(NULL));
#else
            SODIUM_SHARED_PTR<function<void()>*> pKillInner(new function<void()>*(NULL));
#endif
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            auto out_target = SODIUM_TUPLE_GET<1>(p);
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* killOuter =
#else
            auto killOuter =
#endif
                bba.value_(trans0).listen_raw(trans0, out_target,
#if defined(SODIUM_NO_CXX11)
                new lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>(
                    new switch_c_handler(pKillInner)
                )
#else
                new std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>(
                    [pKillInner] (const std::shared_ptr<impl::node>& target, transaction_impl* trans, const light_ptr& pa) {
                        // Note: If any switch takes place during a transaction, then the
                        // value().listen will always cause a sample to be fetched from the
                        // one we just switched to. The caller will be fetching our output
                        // using value().listen, and value() throws away all firings except
                        // for the last one. Therefore, anything from the old input behaviour
                        // that might have happened during this transaction will be suppressed.
                        KILL_ONCE(pKillInner);
                        const cell_& ba = *pa.cast_ptr<cell_>(NULL);
                        *pKillInner = ba.value_(trans).listen_raw(trans, target, NULL, false);
                    })
#endif
                , false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
#if defined(SODIUM_NO_CXX11)
                new lambda0<void>(new switch_s_kill(pKillInner))
#else
                new std::function<void()>([pKillInner] {
                    KILL_ONCE(pKillInner);
                })
#endif
                , killOuter).hold_lazy_(trans0, za);
        }

        stream_ filter_optional_(transaction_impl* trans1, const stream_& input,
            const std::function<boost::optional<light_ptr>(const light_ptr&)>& f)
        {
            auto p = impl::unsafe_new_stream();
#if defined(SODIUM_NO_CXX11)
            lambda0<void>* kill = input.listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new lambda3<void, const boost::shared_ptr<sodium::impl::node>&, sodium::impl::transaction_impl*, const sodium::light_ptr&>(
                    new impl::filter_optional_handler<A>
                )
#else
            auto kill = input.listen_raw(trans1, std::get<1>(p),
                new std::function<void(const SODIUM_SHARED_PTR<impl::node>&, impl::transaction_impl*, const light_ptr&)>(
                    [f] (const SODIUM_SHARED_PTR<impl::node>& target, impl::transaction_impl* trans2, const light_ptr& poa) {
                        boost::optional<light_ptr> oa = f(poa);
                        if (oa) impl::send(target, trans2, oa.get());
                    })
#endif
                , false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

    };  // end namespace impl
};  // end namespace sodium
