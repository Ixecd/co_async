#include <chrono>
#include <tuple>
#include <variant>
#include <coroutine>
#include <utilities/qc.hpp>
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>
#include <co_async/task.hpp>
#include <co_async/when_any.hpp>
#include <co_async/when_all.hpp>
#include <co_async/concepts.hpp>
#include <co_async/scheduler.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/return_previous.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;
using namespace std::chrono_literals;


Task<int> hello1() {
    DEBUG(hello1BeginSleep);
    co_await sleep_for(getTimerLoop(), 1s);
    // co_await sleep_for<std::chrono::steady_clock, std::chrono::seconds>(loop, 1s); // 1s 等价于 std::chrono::seconds(1)
    DEBUG(hello1EndSleep);
    co_return 1;
}

Task<int> hello2() {
    DEBUG(hello2BeginSleep);
    co_await sleep_for(getTimerLoop(), 2s); // 2s 等价于 std::chrono::seconds(2)
    DEBUG(hello2EndSleep);
    co_return 2;
}

Task<int> hello() {
    auto t1 = hello1();
    auto t2 = hello2();
    auto v = co_await when_any(t1, t2);
    /* co_await hello1(); */
    /* co_await hello2(); */
    //co_return 42;
    PRINT(std::get<0>(v));
    co_return 100;
}

int main() {

    //auto t = hello();
    //getTimerLoop().process(t);
    auto t = hello();
    getLoop()->addTask(t);
    getLoop()->process(); // 目前是单线程,getLoop中process必然将两个定时器加到TimerLoop中才会执行下面的
    getTimerLoop().process();
    return 0;
}