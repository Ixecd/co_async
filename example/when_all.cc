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
using namespace std::chrono_literals;

Task<void> hello1() {
    DEBUG(hello1开始睡1s);
    co_await sleep_for(getTimerLoop(), std::chrono::seconds(1));
    DEBUG(hello1睡完了1s);
    co_return;
}

// using namespace std::chrono_literals;
Task<void> hello2() {
    DEBUG(hello2开始睡2s);
    co_await sleep_for(getTimerLoop(), 2s); // using std::chrono_literals
    DEBUG(hello2睡完了2s);
    co_return;
}

Task<int> fiber1() {
    DEBUG(fiber1begin);
    int fiber1Var = 100;
    PRINT(fiber1Var);
    co_return fiber1Var + 100;
}

Task<double> fiber2() {
    DEBUG(fiber2begin);
    double fiber2Var = 314;
    PRINT(fiber2Var);
    co_return fiber2Var * 2;
}

Task<void> fiber3() {
    DEBUG(fiber3);

    co_return;
}


void testHello() {
    auto t1 = hello1();
    auto t2 = hello2();
    auto t = when_all(t1, t2);
    getLoop()->addTask(t);
    getLoop()->process();
}

Task<void> testVar() {
    auto t1 = fiber1();
    auto t2 = fiber2();
    auto t3 = fiber3();
    // tuple结构化绑定
    // when_all本质上是一个模板类型为tuple的Task
    // 
    auto [x, y, z] = co_await when_all(t1, t2, t3);
    PRINT(x);
    PRINT(y);

    co_return;
}


int main() {

    auto t = testVar();
    getLoop()->addTask(t);
    getLoop()->process();

    return 0;
}