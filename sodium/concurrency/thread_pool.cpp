#include "thread_pool.h"   
using namespace sodium::conc;

thread_pool::thread_pool(unsigned n) {
    if (n == 0) n = 1;
    workers.reserve(n);
    for (unsigned i = 0; i < n; ++i)
        workers.emplace_back([this]{ worker(); });
}
thread_pool::~thread_pool() {
    done = true;
    cv.notify_all();
    for (auto& t : workers) t.join();
}
void thread_pool::submit(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(mx);
        q.push(std::move(fn));
        ++in_flight;
    }
    cv.notify_one();
}
void thread_pool::barrier() {
    while (in_flight.load(std::memory_order_acquire) != 0)
        std::this_thread::yield();
}
void thread_pool::worker() {
    while (!done) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(mx);
            cv.wait(lk, [this]{ return done || !q.empty(); });
            if (done && q.empty()) return;
            job = std::move(q.front()); q.pop();
        }
        job();
        in_flight.fetch_sub(1, std::memory_order_release);
    }
}