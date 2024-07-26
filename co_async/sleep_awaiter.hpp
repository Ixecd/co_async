#pragma once
#include <chrono>
#include <thread>
#include "scheduler.hpp"
#include "task.hpp"

using namespace std::chrono_literals;

namespace co_async {

struct SleepAwaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const {
        // std::this_thread::sleep_until(mExpireTime);
        // getLoop()->addTask(coroutine);
        // 如果getLoop设置为static,具有文件作用域,只能在当前文件访问,所以staitc要去掉
        getLoop()->addTimer(mExpireTime, coroutine);
        return std::noop_coroutine();
    }

    void await_resume() const { }

    std::chrono::system_clock::time_point mExpireTime;
};

struct SleepAwatiable {
    SleepAwaiter operator co_await() {
        return SleepAwaiter();
    }
};

// sleep需要知道loop,到时间还要塞回队列
Task<void> sleep_until(std::chrono::system_clock::time_point expireTime) {
    co_await SleepAwaiter(expireTime);
    co_return;
}

Task<void> sleep_for(std::chrono::system_clock::duration duration) {
    co_await sleep_until(std::chrono::system_clock::now() + duration);
    co_return;
}

}