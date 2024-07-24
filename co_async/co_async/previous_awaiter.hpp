#pragma once

#include <coroutine>

namespace co_async {

struct PreviousAwaiter {

    std::coroutine_handle<> mPrevious{};

    bool await_ready() const noexcept { return false; }
    // 这里返回类型设置为coroutine_handle是为了避免手动再次resume
    // 如果一个协程yield,并且await_ready为false就会执行下面这个函数,如果这个函数返回void,就表示
    // 直接将这个协程挂起,将控制权交给协程调用者,也可以设置返回值执行其他协程
    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
        if (mPrevious) return mPrevious;
        else return std::noop_coroutine(); // void 一样的效果(一次一停顿,需要调用者不断resume)
    }

    void await_resume() const noexcept { }

};

struct PreviousAwaitable {
    PreviousAwaiter operator co_await() {
        return PreviousAwaiter();
    }
};


}