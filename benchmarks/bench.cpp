#include <sodium/sodium.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <atomic>
#include <cassert> 

static void busy() {
    volatile double x = 0;
    for (int i = 0; i < 10000; ++i) x += std::sin(i);
}

static inline double millis_since(
        const std::chrono::high_resolution_clock::time_point& t0)
{
    using namespace std::chrono;
    return duration<double, std::milli>(high_resolution_clock::now() - t0).count();
}

enum class topo { FANOUT, PIPELINE, JOIN };

static topo parse_topo(const char* arg) {
    if (!arg) return topo::FANOUT;
    std::string s(arg);
    if (s == "pipeline") return topo::PIPELINE;
    if (s == "join")     return topo::JOIN;
    return topo::FANOUT;
}

int main(int argc, char** argv)
{
    topo mode   = parse_topo(argc > 1 ? argv[1] : nullptr);
    int  events = argc > 2 ? std::atoi(argv[2]) : 100000;
    int  param  = argc > 3 ? std::atoi(argv[3]) : 8; 

    sodium::stream_sink<int> src;
    std::atomic<long long> checksum{0};
    auto work = [](int v) { busy(); return v; };

    if (mode == topo::FANOUT) {     
        for (int i = 0; i < param; ++i)
            src.map(work).listen([&](int v){ checksum += v; });
    }
    else if (mode == topo::PIPELINE) { 
        auto s = src.map(work);
        for (int i = 1; i < param; ++i)
            s = s.map(work);
        s.listen([&](int v){ checksum += v; });
    }
    else {                              
        auto a = src.map(work);
        auto b = src.map(work);
        a.merge(b, [](const int&, const int& r){ return r; })
         .listen([&](int v){ checksum += v; });
    }

    std::cerr << ">>> starting sends\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < events; ++n) {
        src.send(n);
    }
    std::cerr << ">>> finished sends\n";

    double ms = millis_since(t0);
    const char* name = (mode == topo::FANOUT   ? "fanout" :
                        mode == topo::PIPELINE ? "pipeline" :
                                                  "join");
    std::cout << "topology=" << name
              << " param="    << param
              << " events="   << events
              << " elapsed_ms="<< ms
              << " checksum=" << checksum.load()
              << '\n';

    long long expected;
    if (mode == topo::FANOUT)
        expected = static_cast<long long>(param) * events * (events - 1LL) / 2;
    else
        expected = static_cast<long long>(events) * (events - 1LL) / 2;

    assert(checksum.load() == expected && "checksum mismatch -- ordering broken!");
}
