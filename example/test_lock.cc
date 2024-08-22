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
using ll = long long;
using namespace std;

int Count = 0;
std::mutex mtx;

// 对下面的协程函数添加锁毛用没有,得在线程上加锁才有用啊
Task<void> fiber_2() {
    // std::unique_lock<std::mutex> glk(mtx);
    Count += 2;
    // glk.unlock();
    co_return;
}

Task<void> fiber_1() {
    Count += 1;
    co_await fiber_2();
    Count += 1;
    co_return;
}

// 多线程下执行相同的函数,反而协程成了共享资源了
void thread_1() {
    auto t = fiber_1();
    while (!t.mCoroutine.done()) {
        std::unique_lock<std::mutex> glk(mtx);
        t.mCoroutine.resume();
        glk.unlock();
    }
}

int main() {
    thread t1(thread_1);
    thread t2(thread_1);

    std::this_thread::sleep_for(300ms);

    cout << Count << endl; // 第一次 0 ?? 第二次 8 (调试情况下)
    // ok 十分需要加锁

    // 放到sleep前面就不需要sleep了
    t1.join();
    t2.join();

    return 0;
}
