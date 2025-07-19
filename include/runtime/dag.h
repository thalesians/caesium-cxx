// file: include/runtime/dag.hpp
#pragma once
#include <vector>
#include <atomic>
#include <functional>
#include <deque>
#include <algorithm>
#include <memory>

namespace caesium {
namespace runtime {

struct Node {
    size_t id;
    std::vector<Node*> succ;
    std::atomic<int> pending{0};
    std::function<void()> work;
};
using Graph = std::vector<std::unique_ptr<Node>>;

inline void init_pending(Graph& dag){
    for (auto& n :dag)
        n->pending.store(int(n->succ.size()),std::memory_order_relaxed);
}

}  // namespace: runtime
}  //namespace:caesium
