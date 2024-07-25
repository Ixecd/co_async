/**
 * @file repeat_awaiter.hpp
 * @author qc
 * @brief 忽视一切yield
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <coroutine>
#include "concepts.hpp"

namespace co_async {

struct RepeatAwaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        // return coroutine;// 这里不能直接返回,要判断当前协程是否执行完毕
        if (coroutine.done()) return std::noop_coroutine();
        else return coroutine;
    }

    void await_resume() const noexcept {}

    std::coroutine_handle<> mCoroutine;
};

struct RepeatAwaitable {
    RepeatAwaiter operator co_await() {
        return RepeatAwaiter();
    }
};

}

