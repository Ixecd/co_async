#include <chrono>
#include <coroutine>
#include <utilities/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/concepts.hpp>
#include <co_async/timerLoop.hpp>
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;
using namespace std::chrono_literals;

// Task<void> sleep_until(std::chrono::system_clock::time_point expiredTime) {
//     co_await SleepAwaiter(expiredTime);
// }

// Task<void> sleep_for(std::chrono::system_clock::duration duration) {
//     // 同步方法
//     auto t = sleep_until(std::chrono::system_clock::now() + duration);
//     co_await t;
// }

Task<void> testSleep() {
    DEBUG(testSleep()...);
    auto t = sleep_for(1s);
    co_await t;
    DEBUG(sleep_for1send...);

    co_return;
}


int main() {

    auto t = testSleep();

    while(!t.mCoroutine.done()) 
        t.mCoroutine.resume();

    return 0;
}