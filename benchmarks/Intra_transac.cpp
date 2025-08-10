#include <sodium/sodium.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

//busy() 
static void busy() {
    volatile double x = 0;
    for(int i = 0; i < 500000;++i) x += std::sin(i);
}

int main() {
    //number of OS threads to spin up
    int T = 1;
    if(auto* tenv = std::getenv("SODIUM_THREADS"))
        T = std::max(1, std::atoi(tenv));

    //number listeners/tasks to fan out
    const int M = 10000;  

    // per-thread counters
    std::vector<std::atomic<size_t>> counts(T);
    for(auto& c: counts) c.store(0,std::memory_order_relaxed);
    
    //one shared source
    sodium::stream_sink<int> src;

    static std::atomic<size_t> next_idx{0};

    // install M listeners: each will run busy() and bump its thread's counter
    for(int i = 0; i < M; ++i) {
        src.listen([&,T](int){
            thread_local size_t idx= std::numeric_limits<size_t>::max();
            if(idx== std::numeric_limits<size_t>::max()){
                size_t k=next_idx.fetch_add(1,std::memory_order_relaxed);
                idx=k%static_cast<size_t>(T);
            }
            busy();
            counts[idx].fetch_add(1,std::memory_order_relaxed);
        });
    }

    using clock=std::chrono::steady_clock;
    auto t0 = clock::now();

    {
        sodium::transaction txn;
        src.send(42);
    }  //on close, T threads will race on M tasks

    double ms= std::chrono::duration<double,std::milli>(clock::now() - t0).count();
    double ev_s = M / (ms/1000.0);

    //how many tasks each thread got
    size_t total = 0, active=0;
    for(auto& c : counts) {
        size_t v = c.load(std::memory_order_relaxed);
        if(v>0) ++active;
        total += v;
    }

    std::cout
      << "Threads active: " << active << "\n"
      << "Total tasks run: " << total  << "\n"
      << "Elapsed: " << ms << " ms\n"
      << "Throughput:" << ev_s << " every secondd\n";

    if(total != static_cast<size_t>(M)){
        std::cerr << "Mismatch,expected " <<M<< " number of tasks but saw "<<total<< "\n";
        return 2;
    }

    if(active < 2) {
        std::cerr << "No intra‐transact concurrency\n";
        return 1;
    }
    return 0;
}
