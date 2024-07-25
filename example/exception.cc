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

    throw std::runtime_error("fiber1异常");

    co_yield 12.21; // -> Promise : yield_value -> 必须返回一个Awaiter
    DEBUG(fiber1again);
    co_return 42.24;
}

Task<int> fiber2() {
    DEBUG(fiber2Begin);
    auto t1 = fiber1();
    // 使用co_await会调用子协程,子协程执行完毕会调用Promise中的await_resume, 返回Promise中的result()
    // 这时候fiber1已经抛出了异常将mException设值, 由于上面的result当前fiber2会rethrow_exception,
    // 跑到fiber2的Promise中的unhandled_exception,再设置fiber2
    // 的promise中的mException,之后到了main函数调用result,这时候main线程不是协程会直接rethrow出来
    double d = co_await t1; // 将fiber2作为参数传递给Awaiter,记录previous之后返回t1协程句柄执行
    // 如果fiber1抛出了异常那么下面就会rethrow
    PRINT(t1.mCoroutine.promise().result());
    DEBUG(infiber1again);
    co_await t1;
    PRINT(t1.mCoroutine.promise().result());
    co_return 100;
}


Task<void> fiber3() {
    DEBUG(fiber3);
    co_await fiber2();
}


int main() {

    auto t = fiber3();

    while (!t.mCoroutine.done()) 
        t.mCoroutine.resume();

    t.mCoroutine.promise().result();

    return 0;
}