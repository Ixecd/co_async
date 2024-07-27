#pragma once

#include <coroutine>
#include <span>
#include <tuple>
#include <variant> // 多选一 不能是void
#include <type_traits>
#include <utilities/uninitialized.hpp>
#include "task.hpp"
#include "concepts.hpp"
#include "return_previous.hpp"

using namespace std::chrono_literals;

namespace co_async {

struct WhenAnyCtlBlock {
    static constexpr std::size_t kNullIndex = std::size_t(-1);

    std::size_t mIndex{kNullIndex};
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};

struct WhenAnyAwaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        if (mTasks.empty()) return coroutine;
        mControl.mPrevious = coroutine;
        for (auto const &t : mTasks.subspan(0, mTasks.size() - 1)) 
            t.mCoroutine.resume();
        return mTasks.back().mCoroutine;
    }

    void await_resume() const {
        if (mControl.mException) [[unlikely]] 
            std::rethrow_exception(mControl.mException);
    }

    WhenAnyCtlBlock &mControl;
    // 其参数是一个引用,在使用arry转换为span的时候不需要知道数组的长度,这里可以自动推算出来
    std::span<ReturnPreviousTask const> mTasks;
};

// T 决定了 ts 就决定了
// Q : 这里模板参数用 Awaiter 还是 T ?
// A : 为了区分void和普通类型,由于通过T可以推导出Awaiter所以使用T更好
// 如果用Awaiter,那么如何特化void版本呢?
// template <Awaiter Ts>
// ReturnPreviousTask whenAnyHelper(Ts &&ts, WhenAnyCtlBlock &control, Uninitialized<typename AwaitableTraits<Ts>::RetType> &result, std::size_t index) 
template <class T>
ReturnPreviousTask whenAnyHelper(auto &&t, WhenAnyCtlBlock &control, Uninitialized<T> &result, std::size_t index) {
    try {
        result.putValue((co_await std::forward<decltype(t)>(t), NonVoidHelper<>()));
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mIndex = index;
    co_return control.mPrevious;
}

// template <class = void>
// ReturnPreviousTask whenAnyHelper(auto &&t, WhenAnyCtlBlock &control, Uninitialized<void> &, std::size_t index) {
//     try {
//         result.putValue((co_await std::forward<decltype(t)>(t), NonVoidHelper<>()));
//     } catch (...) {
//         control.mException = std::current_exception();
//         co_return control.mPrevious;
//     }
//     --control.mIndex = index;
//     co_return control.mPrevious;
// }


template <std::size_t... Is, class... Ts>
Task<std::variant<typename AwaitableTraits<Ts>::NonVoidRetType...>>
whenAnyImpl(std::index_sequence<Is...>, Ts &&... ts) {
    // CtlBlock
    WhenAnyCtlBlock control{};
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
    ReturnPreviousTask taskArray[] {
        whenAnyHelper(ts, control, std::get<Is>(result), Is)...
    };
    co_await WhenAnyAwaiter(control, taskArray);

    Uninitialized<std::variant<typename AwaitableTraits<Ts>::NonVoidRetType...>> varResult;

    ((control.mIndex == Is && (varResult.putValue(std::in_place_index<Is>, std::get<Is>(result).moveValue()), 0)),...);
    co_return varResult.moveValue();
}


// when_any 协程实现 本质上是Task + tuple
template <Awaitable... Ts>
    requires(sizeof...(Ts) != 0)
auto when_any(Ts &&... ts) {
    return whenAnyImpl(std::make_index_sequence<sizeof...(ts)>{}, std::forward<Ts>(ts)...);
}

}