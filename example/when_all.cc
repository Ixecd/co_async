#include <chrono>
#include <coroutine>
#include <co_async/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/when_all.hpp>
#include <co_async/concepts.hpp>
#include <co_async/uninitialized.hpp>
#include <co_async/sleep_awaiter.hpp>
#include <co_async/non_void_helper.hpp>
#include <co_async/return_previous.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;
using namespace std::chrono_literals;

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


int main() {

    auto t1 = hello1();
    auto t2 = hello2();
    auto t = when_all(t1, t2);
    getLoop()->addTask(t);
    getLoop()->process();

    return 0;
}