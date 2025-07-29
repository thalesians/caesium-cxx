#include <sodium/sodium.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

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
    
    //one shared source
    sodium::stream_sink<int> src;

    // install M listeners: each will run busy() and bump its thread's counter
    for(int i = 0; i < M; ++i) {
        src.listen([&](int){
            busy();
            auto id = std::this_thread::get_id();
            size_t idx = std::hash<std::thread::id>{}(id) % T;
            counts[idx].fetch_add(1, std::memory_order_relaxed);
        });
    }

    //fire exactly one event in a single transaction
    auto t0 = std::chrono::high_resolution_clock::now();
    {
        sodium::transaction txn;
        src.send(42);
    }  //on close, T threads will race on M tasks

    double ms= std::chrono::duration<double,std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
    double ev_s = M / (ms/1000.0);

    //how many tasks each thread got
    size_t total = 0, active=0;
    for(size_t c : counts) {
        if(c>0) ++active;
        total += c;
    }

    std::cout
      << "Threads active: " << active << "\n"
      << "Total tasks run: " << total  << "\n"
      << "Elapsed: " << ms << " ms\n"
      << "Throughput:" << ev_s << " every secondd\n";

    if(active < 2) {
        std::cerr << "No intra‐transact concurrency\n";
        return 1;
    }
    return 0;
}
