#pragma once

#include <coroutine>
#include <tuple>
#include <span>
#include "task.hpp"
#include "concepts.hpp"
#include "uninitialized.hpp"
#include "return_previous.hpp"

namespace co_async {

struct WhenAllCtlBlock {
    std::size_t mCount;
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};

struct WhenAllAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        if (mTasks.empty())
            return coroutine;
        // 记录调用者
        mControl.mPrevious = coroutine;
        for (auto const &t: mTasks.subspan(0, mTasks.size() - 1))
            t.mCoroutine.resume();
        return mTasks.back().mCoroutine;
    }

    void await_resume() const {
        if (mControl.mException) [[unlikely]] {
            std::rethrow_exception(mControl.mException);
        }
    }
    
    WhenAllCtlBlock &mControl;
    // span有一个特殊的构造函数可以将数组转移到这里
    std::span<ReturnPreviousTask const> mTasks;
};

template <class T>
ReturnPreviousTask whenAllHelper(auto &&t, WhenAllCtlBlock &control, Uninitialized<T> &result) {
    try {
        result.putValue(co_await std::forward<decltype(t)>(t));
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mCount;
    if (control.mCount == 0) {
        co_return control.mPrevious;
    }
    co_return std::noop_coroutine();
}

template<class = void>
ReturnPreviousTask whenAllHelper (auto &&t, WhenAllCtlBlock &control, Uninitialized<void> &) {
    try {
        co_await std::forward<decltype(t)>(t);
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mCount;
    if (control.mCount == 0) 
        co_return control.mPrevious;
    co_return std::noop_coroutine();
}
// 下面生成的整数序列可以直接直接使用std::index_sequence<Ts...>展开
template<std::size_t... Is, class... Ts>
Task<std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>>
whenAllImpl(std::index_sequence<Is...>, Ts &&...ts) {
    // Ts长度就是任务的数量,给mCount赋值
    WhenAllCtlBlock control{sizeof...(Ts)};
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
    ReturnPreviousTask taskArray[] {
        whenAllHelper(ts, control, std::get<Is>(result))...};
    
    co_await WhenAllAwaiter(control, taskArray);
    co_return std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>(std::get<Is>(result).moveValue()...);
}

// 实现结构化绑定, 可以接受任意类型的任务,甚至是其他人写的task类(只要满足概念)
// 强制Ts为可等待类型, 并且任务数量不为0
template <Awaitable... Ts> requires(sizeof...(Ts) != 0)
auto when_all(Ts &&... ts) {
    // 生成Ts长的整数序列
    return whenAllImpl(std::make_index_sequence<sizeof...(Ts)>{}, std::forward<Ts>(ts)...);
}


}
