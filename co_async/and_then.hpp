/**
 * @file and_then.hpp
 * @author qc
 * @brief 
 * @version 0.1
 * @date 2024-07-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <utility>
#include "task.hpp"
#include "concepts.hpp"
#include "make_awaitable.hpp"


namespace co_async {

template <Awaitable A, std::invocable<typename AwaitableTraits<A>::RetType> F>
    requires(!std::same_as<void, typename AwaitableTraits<A>::RetType>) 
Task<typename AwaitableTraits<std::invoke_result_t<F, typename AwaitableTraits<A>::RetType>>::Type>
and_then(A && a, F && f) {
    co_return co_await make_awaitable(std::forward<F>(f)(co_await std::forward<A>(a)));
}


// 如果F是一个lambda函数
template <Awaitable A, std::invocable<> F>
    requires(std::same_as<void, typename AwaitableTraits<A>::RetType>)
Task<typename AwaitableTraits<std::invoke_result_t<F>>::Type> and_then(A && a, F && f) {
    co_await std::forward<A>(a);
    // F 是一个 lambda 函数, co_await后面必须是 Task 或者 Awaiter
    co_return co_await make_awaitable(std::forward<F>(f)());
}

template <Awaitable A, Awaitable F>
    requires(!std::invocable<F> && !std::invocable<F, typename AwaitableTraits<A>::RetType>)
auto and_then(A && a, F && f) {
    co_await std::forward<A>(a); // 先执行a
    co_return co_await std::forward<F>(f);
}

}