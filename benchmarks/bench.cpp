#include <sodium/sodium.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

static void busy() {
    volatile double x = 0;
    for (int i = 0; i < 10000; ++i) x += std::sin(i);
}

int main(int argc,char** argv)
{
    int fanout =(argc > 1) ? std::atoi(argv[1]) : 8;
    int events = (argc > 2) ? std::atoi(argv[2]) : 100000;

    sodium::stream_sink<int> src;

    //create one listener, use its exact type for the vector
    auto first = src.map([](int){ busy(); return 0; })
                    .listen([](int){});
    using token_t =decltype(first);

    std::vector<token_t> subs;
    subs.reserve(fanout);
    subs.push_back(first);// keep first listener alive

    for (int i = 1; i <fanout; ++i)
        subs.push_back(
            src.map([](int){ busy();return 0;})
               .listen([](int){})
        );

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < events; ++n) src.send(n);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "listeners="<< fanout << " events="<< events << " elapsed_ms="<< ms<< '\n';
}
