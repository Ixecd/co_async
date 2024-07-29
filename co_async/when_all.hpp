/**
 * @file when_all.hpp
 * @author qc
 * @brief 封装when_all协程函数,其可以接收任意多个满足Awaiter概念的任务,所有任务执行完才返回
 *        使用tuple接收所有返回值.
 * @version 0.1
 * @date 2024-07-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <coroutine>
#include <tuple>
#include <span>
#include <utilities/qc.hpp>
#include <utilities/uninitialized.hpp>
#include "task.hpp"
#include "concepts.hpp"
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
        // 直接执行留下一个
        // 可以留也可以不留,可以用ReturnPreviousTask也可以不用
        // 如果不用的话必须在当前的Awaiter中执行完所有协程,之后return coroutine
        // 这样也不需要在control中记录任务数量
        for (auto const &t: mTasks.subspan(0, mTasks.size()))
            t.mCoroutine.resume();
        // 返回的是ReturnPreviousTask中的协程
        // return mTasks.back().mCoroutine;
        return //mControl.mPrevious;
        std::noop_coroutine();
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

template <class T> // Task 只有在 co_await之后才会被转换为Awaiter, 这里传入的是Awaiter,怎么转化为Task?
ReturnPreviousTask whenAllHelper(auto &&t, WhenAllCtlBlock &control, Uninitialized<T> &result) {
    try {
        // t 作为一个 只要符合概念的任务, 是更加底层的Task
        // 如果没有就去下面那一个void版本
        // co_await后面 如果co_await 返回为void, 则result.putValue 为 NonVoidHelper<void>{}
        result.putValue((co_await std::forward<decltype(t)>(t), NonVoidHelper<>()));
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

// 注意上面如果用了 result.putValue((co_await std::forward<decltype(t)>(t), NonVoidHelper<>())); 
// 那么下面的就完全可以省略.
template<class = void>
// template <> 如果这里要特化
// co_await 后面跟的是Awaiter或者是Task, 如果是Task返回一个Awaiter,在awaiter的参数中记录协程句柄信息,
// 所以这里类型是Awaiter, 所以不能使用Task作为参数.只能用Awaiter,由于Task可以转化为Awaiter,而Awaiter
// 目前无法转化为Task,所以这里特化之后无法推断出来 t 的类型
// template <>
// ReturnPreviousTask whenAllHelper<void> (Task<void, Promise<void>> &&t, WhenAllCtlBlock &control, Uninitialized<void> &) {
ReturnPreviousTask whenAllHelper (auto &&t, WhenAllCtlBlock &control, Uninitialized<void> &) {
    try {
        // 完美转发,相当于co_await std::move(t); t 是一个Awaiter
        // 这里的t的类型是外部的Task类型,这个Task内部有一个Awaiter, 这里co_await的是Task内部的Awaiter
        co_await std::forward<decltype(t)>(t);
        // co_await t;
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mCount;
    if (control.mCount == 0) 
        co_return control.mPrevious; // 执行完就返回到Task中
    co_return std::noop_coroutine();
}


// 下面生成的整数序列可以直接直接使用std::index_sequence<Ts...>展开
// whenAllEmpl的Promise类型是Promise<std::tuple<>...>
template<std::size_t... Is, class... Ts>
Task<std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>>
whenAllImpl(std::index_sequence<Is...>, Ts &&...ts) {
    // Ts长度就是任务的数量,给mCount赋值
    WhenAllCtlBlock control{sizeof...(Ts)};
    // Q : 这里为什么要RetType
    // A : 因为这里需要Uninitialized,如果是NonVoidRetType就会Uninitialized<NonVoidHelper>
    // 那怎么调用moveValue() ? 他会跑到<class T>中认为有值而移动, 所以必须是void的类型
    // 才能在这个函数的最后一行代码中调用moveValue() 来返回一个NonVoidHelper类表示空
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
    // auto size = std::tuple_size<decltype(result)>::value; 
    // PRINT(size); // 3
    // 子任务->子协程->子Promise->要记录Task协程句柄
    ReturnPreviousTask taskArray[] {
        whenAllHelper(ts, control, std::get<Is>(result))...};
    
    // 返回一个ReturnPreviousTask中的mCoroutine->由这里来执行这个协程
    // 这个协程会return_value()->将Promise中的mPrevious设置为当前Task
    // 这里不用接收返回值所以在WhenAllAwaiter中的await_resume不需要返回值
    co_await WhenAllAwaiter(control, taskArray);
    // 将tuple move到Promise<std::tuple<>>中的mResult中
    // Q : 为什么这里又是NonVoidRetType?
    // A : 由于result中的Type被Uninitialized修饰,所以result中如果有一个是Uninitialized<void>
    // 那么这个void调用moveValue()就会返回一个NonVoidHelper对象,而不是void(),也不能是void
    // 所以下面的tuple必须能够接收这种类型
    co_return std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>(std::get<Is>(result).moveValue()...);
}

// 实现结构化绑定, 可以接受任意类型的任务,甚至是其他人写的task类(只要满足概念)
// 强制Ts为可等待类型, 并且任务数量不为0
template <Awaitable... Ts> requires(sizeof...(Ts) != 0)
auto when_all(Ts &&... ts) {
    // 生成Ts长的整数序列 Implement->实现
    return whenAllImpl(std::make_index_sequence<sizeof...(Ts)>{}, std::forward<Ts>(ts)...);
}


}
