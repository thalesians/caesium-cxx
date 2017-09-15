// $Id$

#ifndef _SODIUM_ROUTER_H_
#define _SODIUM_ROUTER_H_

#include <sodium/sodium.h>
#include <map>

namespace sodium {
    namespace impl {
        template <typename Selector>
        struct routing_table {
            routing_table(const impl::stream_& stream_, SODIUM_SHARED_PTR<impl::node> node_)
            : node(node_),
              kill(NULL)
            {}
            ~routing_table() {
                if (kill != NULL) {
                    (*kill)();
                    delete kill;
                }
            }
            SODIUM_SHARED_PTR<impl::node> node;
            std::function<void()>* kill;
            std::multimap<Selector, SODIUM_SHARED_PTR<impl::node>> table;
        };

        template <typename A, typename Selector>
        struct router_impl {
            SODIUM_SHARED_PTR<routing_table<Selector>> table;
            std::vector<std::tuple<stream_loop<A>, Selector>> queued;
        };

        template <typename A, typename Selector>
        struct router_handler : listen_handler
        {
            router_handler(
                std::function<Selector(const A&)> f_,
                SODIUM_SHARED_PTR<routing_table<Selector>> table_)
            : f(std::move(f_)),
              table(std::move(table_))
            {
            }
            std::function<Selector(const A&)> f;
            SODIUM_SHARED_PTR<routing_table<Selector>> table;
            virtual void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& a)
            {
                Selector sel = f(*a.cast_ptr<A>(NULL));
                std::vector<SODIUM_SHARED_PTR<impl::node>> targets;
                for (auto it = table->table.lower_bound(sel);
                        it != table->table.end() && it->first == sel;
                        ++it)
                    targets.push_back(it->second);
                for (auto it = targets.begin(); it != targets.end(); ++it)
                    send(*it, trans, a);
            }
        };
    }  // end namespace impl

    template <typename A, typename Selector>
    class router_loop;

    /*!
     * If you have stream events that you want to route to different parts of your code,
     * you could filter for each recipient, like this:
     *
     *    stream<A> s = ...;
     *    stream<A> r1 = s.filter([] (A a) { return a.recipient == 1; });
     *    stream<A> r2 = s.filter([] (A a) { return a.recipient == 2; });
     *    stream<A> r3 = s.filter([] (A a) { return a.recipient == 3; });
     *
     * But if your number of recipients N is large, then it takes N comparisons to
     * route each message. At O(N) this is clearly inefficient.
     *
     * router allows you to re-write the above code like this:
     *
     *    stream<A> s = ...;
     *    router<A, int> r(s, [] (const A& a) { return a.recipient; });
     *    stream<A> r1 = r.filter_equals(1);
     *    stream<A> r2 = r.filter_equals(2);
     *    stream<A> r3 = r.filter_equals(3);
     *
     * It is then far more efficient because the routing decision is implemented as
     * a map look-up - O(logN)
     */
    template <typename A, typename Selector>
    class router
    {
        friend class router_loop<A, Selector>;
        protected:
            SODIUM_SHARED_PTR<impl::router_impl<A, Selector>> impl;

            router()
            : impl(new impl::router_impl<A, Selector>)
            {}

        public:
            router(stream<A> in, std::function<Selector(const A&)> f)
            : impl(new impl::router_impl<A, Selector>)
            {
                SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                auto stream = std::get<0>(p);
                auto target = std::get<1>(p);
                transaction trans1;
                impl->table = SODIUM_SHARED_PTR<impl::routing_table<Selector>>(new impl::routing_table<Selector>(stream, target));
                auto table(impl->table);
                table->kill = in.listen_raw(trans1.impl(), target,
                    new impl::router_handler<A, Selector>(f, table), false);
            }

            stream<A> filter_equals(Selector sel) const {
                if (impl->table) {
                    SODIUM_TUPLE<impl::stream_,SODIUM_SHARED_PTR<impl::node> > p = impl::unsafe_new_stream();
                    auto target = std::get<1>(p);
                    auto table(impl->table);
    
                    transaction trans1;
    
                    auto it = table->table.insert(std::make_pair(sel, target));
                    if (table->node->link(NULL, target))
                        trans1.impl()->to_regen = true;
    
                    stream<A> out(stream<A>(std::get<0>(p)).unsafe_add_cleanup(
                        new std::function<void()>([table, it] () {
                            impl::transaction_ trans2;
                            trans2.impl()->last([table, it] () {
                                auto target2 = it->second;
                                table->table.erase(it);
                                table->node->unlink_by_target(target2);
                            });
                        })));
                    trans1.close();
                    return out;
                }
                else {
                    stream_loop<A> out;
                    impl->queued.push_back(std::make_tuple(out, sel));
                    return out;
                }
            }
    };

    template <typename A, typename Selector>
    class router_loop : public router<A, Selector>
    {
        public:
            router_loop() {}

            void loop(const router<A, Selector>& r) const {
                sodium::transaction trans;
                if (this->impl->table) {
#if defined(SODIUM_NO_EXCEPTIONS)
                    abort();
#else
                    SODIUM_THROW("router_loop looped back more than once");
#endif
                }
                this->impl->table = r.impl->table;
                for (auto it = this->impl->queued.begin(); it != this->impl->queued.end(); ++it)
                    std::get<0>(*it).loop(this->filter_equals(std::get<1>(*it)));
                this->impl->queued.clear();
            }
    };
}

#endif
