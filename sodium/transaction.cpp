/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#include <sodium/sodium.h>
#include <runtime/dag.h>     
#include <map>
#include <iostream>    // for std::cerr
#include <cassert>       
#include <vector>

using caesium::runtime::Graph;
using caesium::runtime::Node;
using caesium::runtime::init_pending;
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
#include <pthread.h>
#endif

using namespace std;
using namespace boost;

namespace sodium {

#if defined(SODIUM_SINGLE_THREADED)
    static impl::transaction_impl* global_current_transaction;
#elif defined(SODIUM_USE_PTHREAD_SPECIFIC)
    static pthread_key_t current_transaction_key;
#else
    static thread_local impl::transaction_impl* global_current_transaction;
#endif

    namespace impl {

        std::vector<size_t> transaction_impl::all_txn_sizes;
        partition* transaction_impl::part;

        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_STREAM>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.inc_stream();
            l->unlock();
        }

        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_STREAM>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.dec_stream();
            p->update_and_unlock(l);
        }

        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.inc_strong();
            l->unlock();
        }
        
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.dec_strong();
            p->update_and_unlock(l);
        }

        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.inc_node();
            l->unlock();
        }

        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p)
        {
            spin_lock* l = spin_get_and_lock(p);
            p->counts.dec_node();
            p->update_and_unlock(l);
        }

        void holder::handle(const SODIUM_SHARED_PTR<node>& target, transaction_impl* trans, const light_ptr& value) const
        {
            if (mi)
                mi->handle(target, trans, value);
            else
                send(target, trans, value);
        }

    }

    partition::partition()
        : n_threads(::getenv("SODIUM_THREADS")
                       ? std::max(1, std::atoi(::getenv("SODIUM_THREADS")))
                       : int(std::thread::hardware_concurrency())),
          depth(0),
          processing_post(false),
          processing_on_start_hooks(false),
          shutting_down(false),
          pool(n_threads)

    {
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
        pthread_key_create(&current_transaction_key, NULL);
#endif
    }
                            
    partition::~partition()
    {
        shutting_down = true;
        on_start_hooks.clear();
    }

    void partition::post(std::function<void()> action)
    {
#if !defined(SODIUM_SINGLE_THREADED)
        mx.lock();
#endif
        postQ.push_back(std::move(action));
#if !defined(SODIUM_SINGLE_THREADED)
        mx.unlock();
#endif
    }

    void partition::on_start(std::function<void()> action)
    {
#if !defined(SODIUM_SINGLE_THREADED)
        mx.lock();
#endif
        on_start_hooks.push_back(std::move(action));
#if !defined(SODIUM_SINGLE_THREADED)
        mx.unlock();
#endif
    }

    void partition::process_post()
    {
#if !defined(SODIUM_SINGLE_THREADED)
        // Prevent it running on multiple threads at the same time, so posts
        // will be handled in order for the partition.
        if (!processing_post) {
            processing_post = true;
#endif
#if !defined(SODIUM_NO_EXCEPTIONS)
            try {
#endif
                while (postQ.begin() != postQ.end()) {
                    std::function<void()> action = *postQ.begin();
                    postQ.erase(postQ.begin());
#if !defined(SODIUM_SINGLE_THREADED)
                    mx.unlock();
#endif
                    action();
#if !defined(SODIUM_SINGLE_THREADED)
                    mx.lock();
#endif
                }
                processing_post = false;
#if !defined(SODIUM_NO_EXCEPTIONS)
            }
            catch (...) {
                processing_post = false;
                throw;
            }
#endif
#if !defined(SODIUM_SINGLE_THREADED)
        }
#endif
    }

    namespace impl {

        node::node() : rank(0) {}
        node::node(rank_t rank_) : rank(rank_) {}
        node::~node()
        {
            for (SODIUM_FORWARD_LIST<node::target>::iterator it = targets.begin(); it != targets.end(); it++) {
                SODIUM_SHARED_PTR<node> targ = it->n;
                if (targ) {
                    boost::intrusive_ptr<listen_impl_func<H_STREAM> > li(
                        reinterpret_cast<listen_impl_func<H_STREAM>*>(listen_impl.get()));
                    targ->sources.remove(li);
                }
            }
        }

        bool node::link(void* holder, const SODIUM_SHARED_PTR<node>& targ)
        {
            bool changed;
            if (targ) {
                std::set<node*> visited;
                changed = targ->ensure_bigger_than(visited, rank);
                boost::intrusive_ptr<listen_impl_func<H_STREAM> > li(
                    reinterpret_cast<listen_impl_func<H_STREAM>*>(listen_impl.get()));
                targ->sources.push_front(li);
            }
            else
                changed = false;
            targets.push_front(target(holder, targ));
            return changed;
        }

        void node::unlink(void* holder)
        {
            SODIUM_FORWARD_LIST<node::target>::iterator this_it;
            for (SODIUM_FORWARD_LIST<node::target>::iterator last_it = targets.before_begin(); true; last_it = this_it) {
                this_it = last_it;
                ++this_it;
                if (this_it == targets.end())
                    break;
                if (this_it->h == holder) {
                    SODIUM_SHARED_PTR<node> targ = this_it->n;
                    targets.erase_after(last_it);
                    if (targ) {
                        boost::intrusive_ptr<listen_impl_func<H_STREAM> > li(
                            reinterpret_cast<listen_impl_func<H_STREAM>*>(listen_impl.get()));
                        targ->sources.remove(li);
                    }
                    break;
                }
            }
        }

        void node::unlink_by_target(const SODIUM_SHARED_PTR<node>& targ)
        {
            SODIUM_FORWARD_LIST<node::target>::iterator this_it;
            for (SODIUM_FORWARD_LIST<node::target>::iterator last_it = targets.before_begin(); true; last_it = this_it) {
                this_it = last_it;
                ++this_it;
                if (this_it == targets.end())
                    break;
                if (this_it->n == targ) {
                    targets.erase_after(last_it);
                    if (targ) {
                        boost::intrusive_ptr<listen_impl_func<H_STREAM> > li(
                            reinterpret_cast<listen_impl_func<H_STREAM>*>(listen_impl.get()));
                        targ->sources.remove(li);
                    }
                    break;
                }
            }
        }

        bool node::ensure_bigger_than(std::set<node*>& visited, rank_t limit)
        {
            if (rank > limit || visited.find(this) != visited.end())
                return false;
            else {
                visited.insert(this);
                rank = limit + 1;
                for (SODIUM_FORWARD_LIST<node::target>::iterator it = targets.begin(); it != targets.end(); ++it)
                    if (it->n)
                        it->n->ensure_bigger_than(visited, rank);
                return true;
            }
        }

        rank_t rankOf(const SODIUM_SHARED_PTR<node>& target)
        {
            if (target.get() != NULL)
                return target->rank;
            else
                return SODIUM_IMPL_RANK_T_MAX;
        }

        transaction_impl::transaction_impl()
            : prioritized_single(nullptr),
              to_regen(false),
              inCallback(0)
        {
            if (part == nullptr)
                part = new partition;
        }

        void transaction_impl::check_regen() {
            if (to_regen) {
                to_regen = false;
                prioritizedQ.clear();
                for (std::map<entryID, prioritized_entry*>::iterator it = entries.begin(); it != entries.end(); ++it)
                    prioritizedQ.insert(pair<rank_t, entryID>(rankOf(it->second->target), it->first));
            }
        }

        transaction_impl::~transaction_impl()
        {
        }
        
        void coalesce_entry::process(transaction_impl* trans,
                          std::vector<prioritized_entry*>& out_single,
                          std::vector<prioritized_entry*>& out_queued)
        {
            send(target, trans, coalesce->oValue.get());
            coalesce->oValue = boost::optional<light_ptr>();
        }

        void send_entry::process(transaction_impl* trans,
                         std::vector<prioritized_entry*>& out_single,
                          std::vector<prioritized_entry*>& out_queued)
        {
            trans->inCallback++;
            try {
                ((holder*)f->h)->handle(f->n, trans, a);
                trans->inCallback--;
            }
            catch (...) {
                trans->inCallback--;
                throw;
            }
        }

        void firing_entry::process(transaction_impl* trans,
                          std::vector<prioritized_entry*>& out_single,
                         std::vector<prioritized_entry*>& out_queued)
        {
            for (SODIUM_FORWARD_LIST<light_ptr>::const_iterator it = firings.begin(); it != firings.end(); it++)
                h->handle(target, trans, *it);
        }

        void switch_entry::process(transaction_impl* trans,
                         std::vector<prioritized_entry*>& out_single,
                         std::vector<prioritized_entry*>& out_queued)
        {
            if (*pKillInner == NULL)
                *pKillInner = bea.impl->sample().cast_ptr<stream_>(NULL)->listen_raw(trans, target, false);
        }

        void apply_entry::process(transaction_impl* trans,
                         std::vector<prioritized_entry*>& out_single,
                          std::vector<prioritized_entry*>& out_queued)
        {
            auto f = *state->f.get().cast_ptr<std::function<light_ptr(const light_ptr&)>>(NULL);
            send(target, trans, f(state->a.get()));
            state->fired = false;
        }
        //tried intra-concurrency but currently crashes, next steps look at each node
        /*
        void transaction_impl::process_transactional()
        {
            while (true) {
                check_regen();

        
                std::vector<prioritized_entry*> batch;
                rank_t this_rank;

                // pop first entry (old logic)
                prioritized_entry* e;
                if (prioritized_single != nullptr) {
                    e = prioritized_single;
                    prioritized_single = nullptr;
                } else {
                    auto pit = prioritizedQ.begin();
                    if (pit == prioritizedQ.end()) break;      
                    auto eit = entries.find(pit->second);
                    e   = eit->second;
                    prioritizedQ.erase(pit);
                    entries.erase(eit);
                }
                this_rank = rankOf(e->target);
                batch.push_back(e);

                // collect any more entries of the same rank
                while (!prioritizedQ.empty() &&
                    prioritizedQ.begin()->first == this_rank)
                {
                    auto pit = prioritizedQ.begin();
                    auto eit = entries.find(pit->second);
                    batch.push_back(eit->second);
                    prioritizedQ.erase(pit);
                    entries.erase(eit);
                }


        #ifndef SERIAL_SODIUM  // parallel path
                for (auto* ent : batch)
                    part->pool.submit([ent,this]{ ent->process(this); });
                part->pool.barrier();   // wait for siblings
        #else           // fallback: old serial behaviour
                for (auto* ent : batch)
                    ent->process(this);
        #endif
                for (auto* ent : batch)  
                    delete ent;
            }


            while (!lastQ.empty()) {
                (*lastQ.begin())();
                lastQ.erase(lastQ.begin());
            }
        }
        */
        
        // In transaction_impl (transaction.cpp)

        void transaction_impl::last(const std::function<void()>& action)
        {
            // Enqueue post‐transaction hooks in the partition (thread‐safe)
            part->post(action);
        }

        void transaction_impl::process_transactional() {
        //records how many entries this transaction will process
        all_txn_sizes.push_back((prioritized_single ? 1 : 0) + entries.size());

        std::vector<prioritized_entry*> dummy_single, dummy_queued;

        //Rank‐batch scheduler (serial or parallel)
        while (prioritized_single || !prioritizedQ.empty()) {
            check_regen();

            //bail out if no work remains
            if (!prioritized_single && prioritizedQ.empty())
                break;

            //find the current lowest rank
            rank_t current_rank = std::numeric_limits<rank_t>::max();
            if (prioritized_single)
                current_rank = std::min(current_rank, rankOf(prioritized_single->target));
            if (!prioritizedQ.empty())
                current_rank = std::min(current_rank, prioritizedQ.begin()->first);

            //collect the full batch at that rank
            std::vector<prioritized_entry*> batch;
            if (prioritized_single && rankOf(prioritized_single->target) == current_rank) {
                batch.push_back(prioritized_single);
                prioritized_single = nullptr;
            }
            while (!prioritizedQ.empty() && prioritizedQ.begin()->first == current_rank) {
                auto pit = prioritizedQ.begin();
                auto eit = entries.find(pit->second);
                batch.push_back(eit->second);
                entries.erase(eit);
                prioritizedQ.erase(pit);
            }
            assert(!batch.empty());

            //process this batch, in parallel if >1 worker & >1 entry
            int n_threads = part->n_threads;
            if (n_threads > 1 && batch.size() > 1) {
                std::vector<std::vector<prioritized_entry*>> stage_single(n_threads),
                                                stage_queued(n_threads);
                std::atomic<size_t> next_idx{0};

                auto worker = [&](size_t tid) {
                    size_t i;
                    while ((i = next_idx.fetch_add(1, std::memory_order_relaxed)) < batch.size()) {
                        batch[i]->process(this, stage_single[tid], stage_queued[tid]);
                    }
                };

                for (int w = 0; w < n_threads; ++w) {
                    part->pool.submit([w,&worker]{ worker(w); });
                }
                part->pool.barrier();

                for (int tid = 0; tid < n_threads; ++tid) {
                    for (auto* e : stage_single[tid]) {
                        if (!prioritized_single)
                            prioritized_single = e;
                        else {
                            entries[next_entry_id] = e;
                            prioritizedQ.insert({rankOf(e->target), next_entry_id});
                            next_entry_id = next_entry_id.succ();
                        }
                    }
                    for (auto* e : stage_queued[tid]) {
                        entries[next_entry_id] = e;
                        prioritizedQ.insert({rankOf(e->target), next_entry_id});
                        next_entry_id = next_entry_id.succ();
                    }
                }
            } else {
                for (auto* ent : batch) {
                    ent->process(this, dummy_single, dummy_queued);
                }
            }

            //clean up processed entries
            for (auto* ent : batch) {
                delete ent;
            }
        }

        //run any post‐transaction hooks
        while (!lastQ.empty()) {
            (*lastQ.begin())();
            lastQ.erase(lastQ.begin());
        }
    }














        transaction_::transaction_()
        {
            if (transaction_impl::part == nullptr)
                transaction_impl::part = new partition;
            impl_ = current_transaction();
            if (impl_ == NULL) {
#if !defined(SODIUM_SINGLE_THREADED)
                transaction_impl::part->mx.lock();
#endif
                if (!transaction_impl::part->processing_on_start_hooks) {
                    transaction_impl::part->processing_on_start_hooks = true;
                    try {
                        if (!transaction_impl::part->shutting_down) {
                            for (auto it = transaction_impl::part->on_start_hooks.begin();
                                   it != transaction_impl::part->on_start_hooks.end(); ++it)
                                (*it)();
                        }
                        transaction_impl::part->processing_on_start_hooks = false;
                    }
                    catch (...) {
                        transaction_impl::part->processing_on_start_hooks = false;
                        throw;
                    }
                }
                impl_ = new transaction_impl;
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
                pthread_setspecific(current_transaction_key, impl_);
#else
                global_current_transaction = impl_;
#endif
            }
            transaction_impl::part->depth++;
        }
        
        transaction_::~transaction_()
        {
            close();
        }

        /*static*/ transaction_impl* transaction_::current_transaction()
        {
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
            return reinterpret_cast<transaction_impl*>(pthread_getspecific(current_transaction_key));
#else
            return global_current_transaction;
#endif
        }

        void transaction_::close()
        {
            impl::transaction_impl* impl__(this->impl_);
            if (impl__) {
                this->impl_ = NULL;
                partition* part = transaction_impl::part;
                if (part->depth == 1) {
                    try {
                        impl__->process_transactional();
                        part->depth--;
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
                        pthread_setspecific(current_transaction_key, NULL);
#else
                        global_current_transaction = NULL;
#endif
                        delete impl__;
                    }
                    catch (...) {
                        part->depth--;
#if !defined(SODIUM_SINGLE_THREADED) && defined(SODIUM_USE_PTHREAD_SPECIFIC)
                        pthread_setspecific(current_transaction_key, NULL);
#else
                        global_current_transaction = NULL;
#endif
                        delete impl__;
#if !defined(SODIUM_SINGLE_THREADED)
                        part->mx.unlock();
#endif
                        throw;
                    }
                    part->process_post();
#if !defined(SODIUM_SINGLE_THREADED)
                    part->mx.unlock();
#endif
                }
                else
                    part->depth--;
            }
        }
    };  // end namespace impl

};  // end namespace sodium
