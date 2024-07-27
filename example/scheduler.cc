#include <chrono>
#include <coroutine>
#include <co_async/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/concepts.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/scheduler.hpp>
#include <co_async/uninitialized.hpp>
#include <co_async/non_void_helper.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;
using namespace std::chrono_literals;

// Task<void> sleep_until(std::chrono::system_clock::time_point expiredTime) {
//     co_await SleepAwaiter(expiredTime);
//     co_return;
// }

// Task<void> sleep_for(std::chrono::system_clock::duration duration) {
//     // 同步方法
//     auto t = sleep_until(std::chrono::system_clock::now() + duration);
//     co_await t;
//     co_return;
// }

Task<void> testSleep() {
    DEBUG(testSleep()...);
    auto t = sleep_for(1s);
    co_await t;
    DEBUG(sleep_for1send...);
    co_return;
}

Task<void> hello1() {
    DEBUG(hello1开始睡1s);
    co_await sleep_for(std::chrono::seconds(1));
    DEBUG(hello1睡完了1s);
    co_return;
}

// using namespace std::chrono_literals;
Task<void> hello2() {
    DEBUG(hello2开始睡2s);
    co_await sleep_for(2s); // using std::chrono_literals
    DEBUG(hello2睡完了2s);
    co_return;
}

void testTimer() {
    auto t1 = hello1();
    auto t2 = hello2();
    DEBUG(createTaskSucc);
    // getLoop()->addTask(hello1()); //err
    // 协程句柄默认是以引用的方式传入,不能使用临时变量
    getLoop()->addTask(t1);
    getLoop()->addTask(t2);
    DEBUG(addTaskSucc);
    getLoop()->process();
    // cout << void ; // err
    // PRINT(t1.mCoroutine.promise().result());
    // PRINT(t2.mCoroutine.promise().result());
}

int main() {

    testTimer();
    // DEBUG(begin);
    // getLoop()->addTask(std::move(testSleep()));
    // getLoop()->process();

    return 0;
}