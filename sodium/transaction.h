/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_TRANSACTION_H_
#define _SODIUM_TRANSACTION_H_

#include <sodium/concurrency/thread_pool.h>
#include <sodium/config.h>
#include <sodium/light_ptr.h>
#include <sodium/sodium_base.h>
#include <boost/optional.hpp>
#include <boost/intrusive_ptr.hpp>
#include <sodium/unit.h>
#include <map>
#include <set>
#include <list>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>
#include <algorithm>

namespace sodium {

    struct partition {
        partition();
        ~partition();
#if !defined(SODIUM_SINGLE_THREADED)
        std::recursive_mutex mx;
#endif
        int n_threads; //number of wokrer threads
        conc::thread_pool pool;

        int depth;

        bool processing_post;
        std::list<std::function<void()>> postQ;
        void post(std::function<void()> action);
        void process_post();
        std::list<std::function<void()>> on_start_hooks;
        bool processing_on_start_hooks;
        void on_start(std::function<void()> action);
        bool shutting_down;
    };

    namespace impl {
        struct transaction_impl;

        typedef unsigned long rank_t;
        #define SODIUM_IMPL_RANK_T_MAX ULONG_MAX

        class holder {
            public:
                holder() : mi(nullptr) {}
                holder(
                    listen_handler* mi_
                ) : mi(mi_)
                {
                }
                ~holder() {
                    delete mi;
                }
                void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& value) const;

            private:
                std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>* handler;
                listen_handler* mi;
        };

        class node
        {
            public:
                struct target {
                    target(
                        void* h_,
                        const SODIUM_SHARED_PTR<node>& n_
                    ) : h(h_),
                        n(n_) {}

                    void* h;
                    SODIUM_SHARED_PTR<node> n;
                };

            public:
                node();
                node(rank_t rank);
                ~node();

                rank_t rank;
                SODIUM_FORWARD_LIST<node::target> targets;
                SODIUM_FORWARD_LIST<light_ptr> firings;
                SODIUM_FORWARD_LIST<boost::intrusive_ptr<listen_impl_func<H_STREAM> > > sources;
                boost::intrusive_ptr<listen_impl_func<H_NODE> > listen_impl;

                bool link(void* holder, const SODIUM_SHARED_PTR<node>& target);
                void unlink(void* holder);
                void unlink_by_target(const SODIUM_SHARED_PTR<node>& target);

            private:
                bool ensure_bigger_than(std::set<node*>& visited, rank_t limit);
        };
    }
}

namespace sodium {
    namespace impl {

        template <typename A>
        struct ordered_value {
            ordered_value() : tid(-1) {}
            long long tid;
            boost::optional<A> oa;
        };

        struct entryID {
            entryID() : id(0) {}
            entryID(rank_t id_) : id(id_) {}
            rank_t id;
            entryID succ() const { return entryID(id+1); }
            inline bool operator < (const entryID& other) const { return id < other.id; }
        };

        rank_t rankOf(const SODIUM_SHARED_PTR<node>& target);

        struct applicative_state {
            applicative_state() : fired(false) {}
            bool fired;
            boost::optional<light_ptr> f;
            boost::optional<light_ptr> a;
        };

        struct prioritized_entry {
            prioritized_entry(SODIUM_SHARED_PTR<node> target_)
                : target(std::move(target_))
            {
            }
            virtual ~prioritized_entry() {}
            virtual void process(transaction_impl* trans,
                                 std::vector<prioritized_entry*>& out_single,
                                 std::vector<prioritized_entry*>& out_queued) = 0;

            SODIUM_SHARED_PTR<node> target;
        };

        struct coalesce_state {
            coalesce_state() = default;
            ~coalesce_state() = default;
            std::mutex mtx;
            boost::optional<light_ptr> oValue;
        
        };

        struct coalesce_entry : prioritized_entry
        {
            coalesce_entry(SODIUM_SHARED_PTR<node> target_,
                           std::shared_ptr<coalesce_state> coalesce_)
                : prioritized_entry(std::move(target_)), coalesce(std::move(coalesce_))
            {
            }
            virtual void process(transaction_impl* trans,
                                 std::vector<prioritized_entry*>& out_single,
                                 std::vector<prioritized_entry*>& out_queued);

            std::shared_ptr<coalesce_state> coalesce;
        };

        struct send_entry : prioritized_entry
        {
            send_entry(SODIUM_SHARED_PTR<node> target_,
                       node::target* f_,
                       light_ptr a_)
                : prioritized_entry(std::move(target_)), f(std::move(f_)),
                  a(std::move(a_))
            {
            }
            virtual void process(transaction_impl* trans,
                                std::vector<prioritized_entry*>& out_single,
                                std::vector<prioritized_entry*>& out_queued);

            node::target* f;
            light_ptr a;
        };

        struct firing_entry : prioritized_entry
        {
            firing_entry(SODIUM_SHARED_PTR<node> target_,
                         SODIUM_SHARED_PTR<holder> h_,
                         SODIUM_FORWARD_LIST<light_ptr> firings_)
                : prioritized_entry(std::move(target_)), h(std::move(h_)),
                  firings(std::move(firings_))
            {
            }
            virtual void process(transaction_impl* trans,
                                 std::vector<prioritized_entry*>& out_single,
                                std::vector<prioritized_entry*>& out_queued);

            SODIUM_SHARED_PTR<holder> h;
            SODIUM_FORWARD_LIST<light_ptr> firings;
        };

        struct switch_entry : prioritized_entry
        {
            switch_entry(SODIUM_SHARED_PTR<node> target_,
                         std::shared_ptr<std::function<void()>*> pKillInner_,
                         impl::cell_ bea_)
                : prioritized_entry(std::move(target_)), pKillInner(std::move(pKillInner_)),
                  bea(std::move(bea_))
            {
            }
            virtual void process(transaction_impl* trans,
                                std::vector<prioritized_entry*>& out_single,
                                 std::vector<prioritized_entry*>& out_queued);

            std::shared_ptr<std::function<void()>*> pKillInner;
            cell_ bea;
        };

        struct apply_entry : prioritized_entry
        {
            apply_entry(SODIUM_SHARED_PTR<node> target_,
                        SODIUM_SHARED_PTR<applicative_state> state_)
                : prioritized_entry(std::move(target_)), state(std::move(state_))
            {
            }
            virtual void process(transaction_impl* trans,
                                 std::vector<prioritized_entry*>& out_single,
                                 std::vector<prioritized_entry*>& out_queued);

            SODIUM_SHARED_PTR<applicative_state> state;
        };

        struct transaction_impl {
            static std::vector<size_t> all_txn_sizes;
            transaction_impl();
            ~transaction_impl();
            static partition* part;
            entryID next_entry_id;
            prioritized_entry* prioritized_single;
            std::map<entryID, prioritized_entry*> entries;
            std::multiset<std::pair<rank_t, entryID>> prioritizedQ;
            std::list<std::function<void()>> lastQ;
            bool to_regen;
            int inCallback;

            void prioritized(prioritized_entry* e)
            {
                if (entries.empty()) {
                    if (prioritized_single == nullptr) {
                        // Lightweight handling of common case of a single entry.
                        prioritized_single = e;
                        return;
                    }
                    // Otherwise switch to the (heavyweight) general case.
                    entryID id = next_entry_id;
                    next_entry_id = next_entry_id.succ();
                    prioritizedQ.insert(std::pair<rank_t, entryID>(rankOf(prioritized_single->target), id));
                    entries.insert(std::pair<entryID, prioritized_entry*>(id, prioritized_single));
                    prioritized_single = nullptr;
                }
                // Handle general case.
                entryID id = next_entry_id;
                next_entry_id = next_entry_id.succ();
                prioritizedQ.insert(std::pair<rank_t, entryID>(rankOf(e->target), id));
                entries.insert(std::pair<entryID, prioritized_entry*>(id, e));
            }
            void last(const std::function<void()>& action);

            void check_regen();
            void process_transactional();
        };

        class transaction_ {
        private:
            transaction_impl* impl_;
            transaction_(const transaction_&) {}
            transaction_& operator = (const transaction_&) { return *this; };
        public:
            transaction_();
            ~transaction_();
            impl::transaction_impl* impl() const { return impl_; }
        protected:
            void close();
            static transaction_impl* current_transaction();
        };
    };

    class transaction : public impl::transaction_
    {
        private:
            // Disallow copying
            transaction(const transaction&) {}
            // Disallow copying
            transaction& operator = (const transaction&) { return *this; };
        public:
            transaction() {}
            /*!
             * The destructor will close the transaction, so normally close() isn't needed.
             * But, in some cases you might want to close it earlier, and close() will do this for you.
             */
            inline void close() { impl::transaction_::close(); }

            void prioritized(impl::prioritized_entry* e)
            {
                impl()->prioritized(e);
            }

            void post(std::function<void()> f) {
                impl()->part->post(std::move(f));
            }

            static void on_start(std::function<void()> f) {
                transaction trans;
                trans.impl()->part->on_start(std::move(f));
            }
    };
}  // end namespace sodium

#endif
