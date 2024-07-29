/**
 * @file make_awaitable.hpp
 * @author qc
 * @brief make_awiatbale
 * @details 对于任意一个class如果其不是Awaitable那就将其作为value返回一个Task
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

namespace co_async {

template <Awaitable A>
A &&make_awaitable(A && a) {
    return std::forward<A>(a);
}
// Task 可以co_await 转换为Awaiter
template <class A> requires (!Awaitable<A>) 
Task<A> make_awaitable(A && a) {
    co_return std::forward<A>(a);
}

}