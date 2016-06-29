/**
 * Run with
 * valgrind --leak-check=full memory/promise-memory
 */
#include <sodium/promise.h>
#include <string>
#include <iostream>

void launch(sodium::stream_sink<std::string> s)
{
    sodium::promise<std::string> p(s);
    p.then_do([] (const std::string& text) {
        std::cout << text << std::endl;
    });
}

int main(int argc, char* argv[])
{
    sodium::stream_sink<std::string> s;
    launch(s);
    s.send("banana");
}
