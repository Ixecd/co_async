#include <chrono>
#include <tuple>
#include <coroutine>
#include <utilities/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/when_all.hpp>
#include <co_async/concepts.hpp>
#include <co_async/timerLoop.hpp>
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>
#include <co_async/return_previous.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;


Task<double> fiber2() {
    DEBUG(fiber2);
    double pai = 3.14;
    PRINT(pai);
    co_return pai * 2;
}

Task<int> fiber1() {
    DEBUG(fiber1);
    int var = 100;
    PRINT(var);
    auto t = fiber2();
    // 下面是简略分析
    // co_await能被r接收是因为Task中有Awaiter->await_resume()->子协程返回自己的result()

    // 下面是详细分析(为了方便理解后面的when_all)
    // co_await 是编译器硬编码关键字,t是一个协程,调用co_await t; 会等待t执行完,t执行
    // ->Promise-> 先执行 -> 再挂起(一定会,不管有没有yield,没有yield就在最后挂起) -> Awaiter(子协程await_suspend) -> 设置mPrevious
    // -> 设置ok, 返回当前t继续执行 -> return_value -> t中mResult设值(参数由用户给比如
    // co_return 42 就给42) -> 设置完 -> Promise(final_suspend)返回父协程(return 
    // PreviousAwaiter) -> mPrevious在挂起的时候就已经设置好了 -> 有父就返回父执行父(没有返
    // 回noop..) -> 返回父开始执行fiber1 -> 父协程调用子协程的await_resume()-> 由t接收返回结果
    auto r = co_await t;
    PRINT(r);
    co_return 100;
}

int main() {
    auto t = fiber1();
    getLoop()->addTask(t);
    getLoop()->process();

    return 0;
}