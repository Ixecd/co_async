#include <chrono>
#include <coroutine>
#include <co_async/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/concepts.hpp>
#include <co_async/uninitialized.hpp>
#include <co_async/non_void_helper.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;

Task<double> fiber1() {
    int fiber1Var = 2;
    PRINT(fiber1Var);
    co_yield 12.21; // -> Promise : yield_value -> 必须返回一个Awaiter
    DEBUG(fiber1again);
    co_return 42.24;
}

Task<int> fiber2() {
    DEBUG(fiber2Begin);
    auto t1 = fiber1();
    co_await t1; // 将fiber2作为参数传递给Awaiter,记录previous之后返回t1协程句柄执行
    PRINT(t1.mCoroutine.promise().result());
    DEBUG(infiber1again);
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