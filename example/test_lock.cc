#include <bits/stdc++.h>
#include <chrono>
#include <co_async/concepts.hpp>
#include <co_async/previous_awaiter.hpp>
#include <co_async/return_previous.hpp>
#include <co_async/scheduler.hpp>
#include <co_async/task.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/when_all.hpp>
#include <co_async/when_any.hpp>
#include <coroutine>
#include <tuple>
#include <utilities/non_void_helper.hpp>
#include <utilities/qc.hpp>
#include <utilities/uninitialized.hpp>
#include <variant>

using namespace co_async;
using namespace std::chrono_literals;

int Count = 0;
std::mutex mtx;

Task<void> fiber_2() {
    std::unique_lock<std::mutex> glk(mtx);
    // Count += 2;
    for (int i = 0; i < 20000; ++i) Count++;
    glk.unlock();
    co_return;
}

Task<void> fiber_1() {
    std::unique_lock<std::mutex> glk(mtx);
    // Count += 1;
    for (int i = 0; i < 10000; ++i) Count++;
    glk.unlock();
    co_await fiber_2();
    glk.lock();
    for (int i = 0; i < 10000; ++i) Count++;
    // Count += 1;
    glk.unlock();
    co_return;
}

void thread_1() {
    auto t = fiber_1();
    while (!t.mCoroutine.done()) {
        // 将协程本身当作一种共享资源?这样临界区太大,不好,锁还是在协程函数里加比较好
        // std::unique_lock<std::mutex> glk(mtx);
        t.mCoroutine.resume();
        // glk.unlock();
    }
}

int main() {
    std::thread t1(thread_1);
    std::thread t2(thread_1);

    t1.join();
    t2.join();

    // std::this_thread::sleep_for(300ms);

    std::cout << Count << std::endl; // 覆盖写操作,预期80000,实际62731
    // 单线程下执行最终结果是正确的 40000 ok
    return 0;
}
