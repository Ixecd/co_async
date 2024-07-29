/**
 * @file timerLoop.hpp
 * @author qc
 * @brief 定时器单独的调度器,for when_any 使用set顶层的红黑树方便删除对应的coroutine_handle
 * @details 单独为定时器任务封装一个promise
 * @version 0.1
 * @date 2024-07-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <coroutine>
#include <optional>
#include <chrono>
#include <thread>

#include <utilities/qc.hpp>
#include <utilities/rbtree.hpp>
#include "task.hpp"
#include "scheduler.hpp"

using namespace std::chrono_literals;
namespace co_async {

// 定时器不需要返回任何value 直接继承Promise<void>即可
// 这里Pomise直接继承 RbNode称为其儿子,RbNode从树中删除,对应的Promise也被删除
struct SleepUntilPromise : RbTree<SleepUntilPromise>::RbNode, Promise<void> {
    // 记录触发时间点,Promise<T>中不会记录
    std::chrono::system_clock::time_point mExpireTime;

    auto get_return_object() {
        return std::coroutine_handle<SleepUntilPromise>::from_promise(*this);
    }

    SleepUntilPromise& operator=(SleepUntilPromise &&) = delete;
private:
    bool operator > (SleepUntilPromise const& that) {
        return this->mExpireTime > that.mExpireTime;
    }

    friend bool operator < (SleepUntilPromise const& lhs, SleepUntilPromise const& rhs) noexcept {
        return lhs.mExpireTime < rhs.mExpireTime;
    }

};

struct TimerLoop {
    // weak RbTree,只保留一个引用指向真正的Promise
    // 这里澄清一下
    // Task是协程任务 eg Task<void> task { ... co_return }
    // auto t = task; 这个表示生成一个协程任务实例(Promise),但是还没有执行
    // 之后将这个t添加到调度器中,通过调度器t->mCoroutine.resume() 才表示真正执行协程函数.
    RbTree<SleepUntilPromise> mRbTimer;

    bool hasTimer() const noexcept {
        return !mRbTimer.empty();
    }

    void addTimer(SleepUntilPromise &promise) {
        mRbTimer.insert(promise);
    }

    std::chrono::system_clock::duration getNext() const noexcept {
        // 在这里设置epoll_wait的TIMEOUT, min(3, 下一个定时器)
        if (!hasTimer()) return 3s;
        else return mRbTimer.front().mExpireTime - std::chrono::system_clock::now();
    }

    std::optional<std::chrono::system_clock::duration> run() {

        // 这里如果如果不停,会先执行coroutine,还没来得及加进RbTree就下去了
        // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        while (!mRbTimer.empty()) {
            auto nowTime = std::chrono::system_clock::now();
            auto &promise = mRbTimer.front();
            if (promise.mExpireTime < nowTime) {
                mRbTimer.erase(promise);
                std::coroutine_handle<SleepUntilPromise>::from_promise(promise).resume();
            } else return promise.mExpireTime - nowTime;
        }
        return std::nullopt;
    }

    void process() {
        while (true) {
            auto ret = run();
            if (ret == std::nullopt) break;
            else std::this_thread::sleep_for(ret.value());
        }
    }

    TimerLoop& operator=(TimerLoop &&) = delete;
};

TimerLoop& getTimerLoop() {
    static TimerLoop loop;
    return loop;
}

struct SleepAwaiter {
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<SleepUntilPromise> coroutine) {
        auto &promise = coroutine.promise();
        promise.mExpireTime = mExpireTime;
        mLoop.addTimer(promise);
    }

    void await_resume() const noexcept { }

    TimerLoop &mLoop;
    std::chrono::system_clock::time_point mExpireTime;
};

// task可以指定到特定的红黑树上
template <class Clock, class Dur>
inline Task<void, SleepUntilPromise>
sleep_until(TimerLoop &loop, std::chrono::time_point<Clock, Dur> expireTime) {
    co_await SleepAwaiter(loop, std::chrono::time_point_cast<std::chrono::system_clock::duration>(expireTime));
}

template <class Rep, class Period>
inline Task<void, SleepUntilPromise>
sleep_for(TimerLoop &loop, std::chrono::duration<Rep, Period> duration) {
    auto d = std::chrono::duration_cast<std::chrono::system_clock::duration>(duration);
    if (d.count() > 0) 
        co_await SleepAwaiter(loop, std::chrono::system_clock::now() + d);
}

// Task<void, SleepUntilPromise> sleep_until(std::chrono::system_clock::time_point expireTime) {
//     auto &loop = getTimerLoop();
//     co_await SleepAwaiter(loop, expireTime);
// }

// Task<void, SleepUntilPromise> sleep_for(std::chrono::system_clock::duration duration) {
//     auto &loop = getTimerLoop();
//     co_await SleepAwaiter(loop, std::chrono::system_clock::now() + duration);
// }

}
