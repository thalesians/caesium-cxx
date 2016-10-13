/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_TRANSACTION_H_
#define _SODIUM_TRANSACTION_H_

#include <sodium/config.h>
#include <sodium/count_set.h>
#include <sodium/light_ptr.h>
#include <sodium/lock_pool.h>
#include <boost/optional.hpp>
#include <boost/intrusive_ptr.hpp>
#include <sodium/unit.h>
#include <map>
#include <set>
#include <list>
#include <memory>
#include <pthread.h>
#include <forward_list>
#include <tuple>

namespace sodium {

#if !defined(SODIUM_SINGLE_THREADED)
    class mutex
    {
    private:
        pthread_mutex_t mx;
        // ensure we don't copy or assign a mutex by value
        mutex(const mutex&) {}
        mutex& operator = (const mutex&) { return *this; }
    public:
        mutex();
        ~mutex();
        void lock()
        {
            pthread_mutex_lock(&mx);
        }
        void unlock()
        {
            pthread_mutex_unlock(&mx);
        }
    };
#endif

    struct partition {
        partition();
        ~partition();
#if !defined(SODIUM_SINGLE_THREADED)
        mutex mx;
#endif
        int depth;
#if !defined(SODIUM_SINGLE_THREADED)
        pthread_key_t key;
#endif
        bool processing_post;
        std::list<std::function<void()>> postQ;
        void post(const std::function<void()>& action);
        void process_post();
    };

    /*!
     * The default partition which gets chosen when you don't specify one.
     */
    struct def_part {
        static partition* part();
    };

    namespace impl {
        struct transaction_impl;

        typedef unsigned long rank_t;
        #define SODIUM_IMPL_RANK_T_MAX ULONG_MAX

        class holder;

        class node;
        template <class Allocator>
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

        class holder {
            public:
                holder(
                    std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>* handler_
                ) : handler(handler_) {}
                ~holder() {
                    delete handler;
                }
                void handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& value) const;

            private:
                std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>* handler;
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

            private:
                bool ensure_bigger_than(std::set<node*>& visited, rank_t limit);
        };
    }
}

namespace sodium {
    namespace impl {

        template <class A>
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

        struct prioritized_entry {
            prioritized_entry(const SODIUM_SHARED_PTR<node>& target_,
                              const std::function<void(transaction_impl*)>& action_)
                : target(target_), action(action_)
            {
            }
            SODIUM_SHARED_PTR<node> target;
            std::function<void(transaction_impl*)> action;
        };

        struct transaction_impl {
            transaction_impl(partition* part);
            ~transaction_impl();
            partition* part;
            entryID next_entry_id;
            std::map<entryID, prioritized_entry> entries;
            std::multiset<std::pair<rank_t, entryID>> prioritizedQ;
            std::list<std::function<void()>> lastQ;
            bool to_regen;
            int inCallback;

            void prioritized(const SODIUM_SHARED_PTR<impl::node>& target,
                             const std::function<void(impl::transaction_impl*)>& action);
            void last(const std::function<void()>& action);

            void check_regen();
            void process_transactional();
        };
    };

    class policy {
    public:
        policy() {}
        virtual ~policy() {}

        static policy* get_global();
        static void set_global(policy* policy);

        /*!
         * Get the current thread's active transaction for this partition, or NULL
         * if none is active.
         */
        virtual impl::transaction_impl* current_transaction(partition* part) = 0;

        virtual void initiate(impl::transaction_impl* impl) = 0;

        /*!
         * Dispatch the processing for this transaction according to the policy.
         * Note that post() will delete impl, so don't reference it after that.
         */
        virtual void dispatch(impl::transaction_impl* impl,
            const std::function<void()>& transactional,
            const std::function<void()>& post) = 0;
    };

    namespace impl {
        class transaction_ {
        private:
            transaction_impl* impl_;
            transaction_(const transaction_&) {}
            transaction_& operator = (const transaction_&) { return *this; };
        public:
            transaction_(partition* part);
            ~transaction_();
            impl::transaction_impl* impl() const { return impl_; }
        protected:
            void close();
        };
    };

    class transaction : public impl::transaction_
    {
        private:
            // Disallow copying
            transaction(const transaction&) : impl::transaction_(def_part::part()) {}
            // Disallow copying
            transaction& operator = (const transaction&) { return *this; };
        public:
            transaction() : impl::transaction_(def_part::part()) {}
            /*!
             * The destructor will close the transaction, so normally close() isn't needed.
             * But, in some cases you might want to close it earlier, and close() will do this for you.
             */
            inline void close() { impl::transaction_::close(); }
    };

    class simple_policy : public policy
    {
    public:
        simple_policy();
        virtual ~simple_policy();
        virtual impl::transaction_impl* current_transaction(partition* part);
        virtual void initiate(impl::transaction_impl* impl);
        virtual void dispatch(impl::transaction_impl* impl,
            const std::function<void()>& transactional,
            const std::function<void()>& post);
    };
}  // end namespace sodium

#endif
