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
        std::function<void()>* stream_::listen_raw(
            transaction_impl* trans,
            const SODIUM_SHARED_PTR<impl::node>& target,
            listen_handler* handler,
            bool suppressEarlierFirings) const
        {
            SODIUM_SHARED_PTR<holder> h(new holder(handler));
            return listen_impl(trans, target, h, suppressEarlierFirings);
        }

        std::function<void()>* stream_::listen_raw(
            transaction_impl* trans,
            const SODIUM_SHARED_PTR<impl::node>& target,
            bool suppressEarlierFirings) const
        {
            SODIUM_SHARED_PTR<holder> h(new holder);
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

        stream_ stream_::once_(transaction_impl* trans1) const
        {
            SODIUM_SHARED_PTR<function<void()>*> ppKill(new function<void()>*(NULL));

            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            *ppKill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new once_handler(ppKill),
                false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([ppKill] () {
                    kill_once(ppKill);
                })
            );
        }

        stream_ stream_::merge_(transaction_impl* trans1, const stream_& other) const {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            SODIUM_SHARED_PTR<impl::node> left(new impl::node);
            const SODIUM_SHARED_PTR<impl::node>& right = SODIUM_TUPLE_GET<1>(p);
            char* h = new char;
            if (left->link(h, right))
                trans1->to_regen = true;
            // defer right side to make sure merge is left-biased
            auto kill1 = this->listen_raw(trans1, left, new merge_handler(right), false);
            auto kill2 = other.listen_raw(trans1, right, false);
            auto kill3 = new std::function<void()>([left, h] () {
                left->unlink(h);
                delete h;
            });
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill1, kill2, kill3);
        }

        stream_ stream_::last_firing_only_(transaction_impl* trans) const
        {
            SODIUM_SHARED_PTR<coalesce_state> pState(new coalesce_state);
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            auto kill = listen_raw(trans, SODIUM_TUPLE_GET<1>(p),
                new impl::last_firing_only_handler(pState),
                false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        /*!
         * Sample the cell's value as at the transaction before the
         * current one, i.e. no changes from the current transaction are
         * taken.
         */
        stream_ stream_::snapshot_(transaction_impl* trans1, cell_ beh,
                std::function<light_ptr(const light_ptr&, const light_ptr&)> combine
            ) const
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            auto kill = listen_raw(trans1, SODIUM_TUPLE_GET<1>(p),
                new snapshot_handler(std::move(beh), std::move(combine)), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        /*!
         * Filter this stream based on the specified predicate, passing through values
         * where the predicate returns true.
         */
        stream_ stream_::filter_(transaction_impl* trans1,
                std::function<bool(const light_ptr&)> pred
            ) const
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            auto kill = listen_raw(trans1, std::get<1>(p), new filter_handler(std::move(pred)), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

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
        
        /*!
         * Function to push a value into an stream
         */
        void send(const SODIUM_SHARED_PTR<node>& n, transaction_impl* trans1, const light_ptr& a)
        {
            if (n->firings.begin() == n->firings.end())
                trans1->last([n] () {
                    n->firings.clear();
                });
            n->firings.push_front(a);
            SODIUM_FORWARD_LIST<node::target>::iterator it = n->targets.begin();
            while (it != n->targets.end()) {
                node::target* f = &*it;
                trans1->prioritized(new impl::send_entry(f->n, f, a));
                it++;
            }
        }

        /*!
         * Creates an stream, that values can be pushed into using impl::send(). 
         */
        SODIUM_TUPLE<stream_, SODIUM_SHARED_PTR<node> > unsafe_new_stream()
        {
            SODIUM_SHARED_PTR<node> n1(new node);
            SODIUM_WEAK_PTR<node> n_weak(n1);
            boost::intrusive_ptr<listen_impl_func<H_STRONG> > impl(
                new listen_impl_func<H_STRONG>(new listen_impl_func<H_STRONG>::closure([n_weak] (transaction_impl* trans1,
                        const SODIUM_SHARED_PTR<node>& target,
                        const SODIUM_SHARED_PTR<holder>& h,
                        bool suppressEarlierFirings) -> std::function<void()>* {  // Register listener
                    SODIUM_SHARED_PTR<node> n2 = n_weak.lock();
                    if (n2) {
#if !defined(SODIUM_SINGLE_THREADED)
                        transaction_impl::part->mx.lock();
#endif
                        if (n2->link(h.get(), target))
                            trans1->to_regen = true;
#if !defined(SODIUM_SINGLE_THREADED)
                        transaction_impl::part->mx.unlock();
#endif
                        if (!suppressEarlierFirings && n2->firings.begin() != n2->firings.end()) {
                            SODIUM_FORWARD_LIST<light_ptr> firings = n2->firings;
                            trans1->prioritized(new impl::firing_entry(target, h, firings));
                        }
                        SODIUM_SHARED_PTR<holder>* h_keepalive = new SODIUM_SHARED_PTR<holder>(h);
                        return new std::function<void()>([n_weak, h_keepalive] () {  // Unregister listener
                            impl::transaction_ trans2;
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


        SODIUM_SHARED_PTR<cell_impl> hold(transaction_impl* trans0, const light_ptr& initValue, const stream_& input)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            if (input.is_never())
                return SODIUM_SHARED_PTR<cell_impl>(new cell_impl_constant(initValue));
            else {
#endif
                SODIUM_SHARED_PTR<cell_impl_concrete<cell_state> > impl(
                    new cell_impl_concrete<cell_state>(input, cell_state(initValue), std::shared_ptr<cell_impl>())
                );
                SODIUM_WEAK_PTR<cell_impl_concrete<cell_state> > impl_weak(impl);
                impl->kill =
                    input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
                    new hold_handler(impl_weak), false);
                return impl;
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        SODIUM_SHARED_PTR<cell_impl> hold_lazy(transaction_impl* trans0, const std::function<light_ptr()>& initValue, const stream_& input)
        {
            SODIUM_SHARED_PTR<cell_impl_concrete<cell_state_lazy> > impl(
                new cell_impl_concrete<cell_state_lazy>(input, cell_state_lazy(initValue), std::shared_ptr<cell_impl>())
            );
            SODIUM_WEAK_PTR<cell_impl_concrete<cell_state_lazy> > w_impl(impl);
            impl->kill =
                input.listen_raw(trans0, SODIUM_SHARED_PTR<node>(new node(SODIUM_IMPL_RANK_T_MAX)),
                new hold_lazy_handler(w_impl), false);
            return static_pointer_cast<cell_impl, cell_impl_concrete<cell_state_lazy>>(impl);
        }

        cell_::cell_()
        {
        }

        cell_::cell_(cell_impl* impl_)
            : impl(impl_)
        {
        }

        cell_::cell_(SODIUM_SHARED_PTR<cell_impl> impl_)
            : impl(std::move(impl_))
        {
        }

        cell_::cell_(light_ptr a)
            : impl(new cell_impl_constant(std::move(a)))
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

        cell_ apply(transaction_impl* trans0, const cell_& bf, const cell_& ba)
        {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ocf = bf.get_constant_value();
            if (ocf) { // function is constant
                auto f = *ocf.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                return impl::map_(trans0, f, ba);  // map optimizes to a constant where ba is constant
            }
            else {
                boost::optional<light_ptr> oca = ba.get_constant_value();
                if (oca) {  // 'a' value is constant but function is not
                    const light_ptr& a = oca.get();
                    return impl::map_(trans0, [a] (const light_ptr& pf) -> light_ptr {
                        const std::function<light_ptr(const light_ptr&)>& f =
                            *pf.cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
                        return f(a);
                    }, bf);
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
                    auto kill1 = bf.value_(trans0).listen_raw(trans0, in_target,
                        new apply_f_handler(out_target, state), false);
                    auto kill2 = ba.value_(trans0).listen_raw(trans0, in_target,
                        new apply_a_handler(out_target, state), false);
                    auto kill3 = new std::function<void()>([in_target, h] () {
                        in_target->unlink(h);
                        delete h;
                    });
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

        stream_ stream_::add_cleanup_(transaction_impl* trans, std::function<void()>* cleanup) const
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            auto kill = listen_raw(trans, std::get<1>(p), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill, cleanup);
        }

        /*!
         * Map a function over this stream to modify the output value.
         */
        stream_ map_(transaction_impl* trans,
                     listen_handler* impl,
                     const stream_& s)
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
            auto kill = s.listen_raw(trans, std::get<1>(p),
                    impl, false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

        cell_ map_(transaction_impl* trans,
            const std::function<light_ptr(const light_ptr&)>& f,
            const cell_& beh) {
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            boost::optional<light_ptr> ca = beh.get_constant_value();
            if (ca)
                return cell_(f(ca.get()));
            else {
#endif
                auto impl = beh.impl;
                return
                    map_(trans, new map_cell_handler(f), beh.updates_())
                    .hold_lazy_(trans, [f, impl] () -> light_ptr {
                        return f(impl->sample());
                    });
#if defined(SODIUM_CONSTANT_OPTIMIZATION)
            }
#endif
        }

        stream_ switch_s(transaction_impl* trans0, const cell_& bea)
        {
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            const SODIUM_SHARED_PTR<impl::node>& target1 = SODIUM_TUPLE_GET<1>(p);
            std::shared_ptr<function<void()>*> pKillInner(new function<void()>*(NULL));
            trans0->prioritized(new impl::switch_entry(target1, pKillInner, bea));

            auto killOuter = bea.updates_().listen_raw(trans0, target1,
                new switch_s_handler(pKillInner), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([pKillInner] {
                    kill_once(pKillInner);
                })
                , killOuter);
        }

        cell_ switch_c(transaction_impl* trans0, const cell_& bba)
        {
            auto za = [bba] () -> light_ptr { return bba.impl->sample().cast_ptr<cell_>(NULL)->impl->sample(); };
            SODIUM_SHARED_PTR<function<void()>*> pKillInner(new function<void()>*(NULL));
            SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = unsafe_new_stream();
            auto out_target = SODIUM_TUPLE_GET<1>(p);
            auto killOuter =
                bba.value_(trans0).listen_raw(trans0, out_target,
                new switch_c_handler(pKillInner), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(
                new std::function<void()>([pKillInner] {
                    kill_once(pKillInner);
                })
                , killOuter).hold_lazy_(trans0, za);
        }

        stream_ filter_optional_(transaction_impl* trans1, const stream_& input,
            std::function<boost::optional<light_ptr>(const light_ptr&)> f)
        {
            auto p = impl::unsafe_new_stream();
            auto kill = input.listen_raw(trans1, std::get<1>(p),
                new filter_optional_handler(std::move(f)), false);
            return SODIUM_TUPLE_GET<0>(p).unsafe_add_cleanup(kill);
        }

    };  // end namespace impl
};  // end namespace sodium
