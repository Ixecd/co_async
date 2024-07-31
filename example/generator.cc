#include <chrono>
#include <co_async/generator.hpp>
#include <co_async/task.hpp>
#include <utilities/qc.hpp>

using namespace co_async;
using namespace std::chrono_literals;

Generator<int> gen() {
    for (int i = 0; i < 5; ++i) {
        co_yield i + 1;
    }
    co_return;
}

Task<void> amain() {
    auto g = gen();
    while (auto i = co_await g) {
        PRINT(*(i));
    }
    co_return;
}

int main() {
    auto t = amain();
    while (!t.mCoroutine.done()) {
        t.mCoroutine.resume();
    }

    return 0;
}
