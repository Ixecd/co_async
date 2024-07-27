#pragma once
#include <chrono>
#include <thread>

#include "task.hpp"
#include "scheduler.hpp"
#include "timerLoop.hpp"

using namespace std::chrono_literals;

namespace co_async {

struct SleepUntilPromise;
struct TimerLoop;

struct SleepAwaiter {
    bool await_ready() const noexcept { return false; }

    // 光荣退休!
    // std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const {
    //     // std::this_thread::sleep_until(mExpireTime);
    //     // getLoop()->addTask(coroutine);
    //     Loop::TimerEntry te(mExpireTime, coroutine);
    //     getLoop()->addTimer(te);
    //     return std::noop_coroutine();
    // }

    void await_suspend(std::coroutine_handle<SleepUntilPromise> coroutine) {
        auto &promise = coroutine.promise();
        promise.mExpireTime = mExpireTime;
        mRbTreeTimer.addTimer(promise);
    }

    void await_resume() const noexcept { }

    TimerLoop &mRbTreeTimer;
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