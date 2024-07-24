#include <chrono>
#include <coroutine>
#include <co_async/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/concepts.hpp>
#include <co_async/uninitialized.hpp>
#include <co_async/non_void_helper.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;

Task<int> fiber1() {
    int fiber1Var = 2;
    PRINT(fiber1Var);
    co_return 42;
}

Task<int> fiber2() {
    DEBUG(fiber2Begin);
    auto t1 = fiber1();
    co_await t1;
    PRINT(t1.mCoroutine.promise().result());
    co_return 100;
}

int main() {

    auto t = fiber2();

    while (!t.mCoroutine.done()) 
        t.mCoroutine.resume();
    PRINT(t.mCoroutine.promise().result());

    return 0;
}