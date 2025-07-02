#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace sodium { namespace conc {

class thread_pool {
public:
    explicit thread_pool(unsigned n = std::thread::hardware_concurrency());
    ~thread_pool();

    void submit(std::function<void()> fn);
    void barrier(); // wait for all tasks queued so far

private:
    void worker();
    std::vector<std::thread>  workers;
    std::queue<std::function<void()>> q;
    std::mutex     mx;
    std::condition_variable     cv;
    std::atomic<bool>         done{false};
    std::atomic<int>     in_flight{0};
};

} }