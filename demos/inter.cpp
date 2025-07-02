#include <sodium/sodium.h>
#include <thread>
#include <chrono>
#include <iostream>

int main() {
    sodium::stream_sink<int> sA; sA.listen([](int){});


    static sodium::partition other_part;
    sodium::impl::transaction_impl::part = &other_part;
    sodium::stream_sink<int> sB; sB.listen([](int){});
    sodium::impl::transaction_impl::part = nullptr;  

    auto push = [](sodium::stream_sink<int>& st){
        for (int i = 0; i < 100000; ++i) st.send(i);
    };

    auto t0 = std::chrono::high_resolution_clock::now();
    std::thread th1(push, std::ref(sA));
    std::thread th2(push, std::ref(sB));
    th1.join(); th2.join();
    double ms = std::chrono::duration<double,std::milli>(
                  std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "two partitions elapsed_ms=" << ms << '\n';
}