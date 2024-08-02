/**
 * @file task.hpp
 * @author qc
 * @brief 任务封装
 * @version 0.1
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <exception> // for std::exception_ptr
#include <coroutine>
#include <utility>
#include <utilities/qc.hpp>
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>
#include "concepts.hpp"
#include "previous_awaiter.hpp"

/// @brief C++20协程中有很多硬编码函数xxx_xxx() -> 涉及到concept
namespace co_async {

template <class T>
struct Promise {
    auto initial_suspend() noexcept{
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        // 这里实现了递归调用协程,保存一个mPrevious协程句柄,等待co_return的时候执行mPrevious
        // 如果返回std::suspend_always()就会回到主线程
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        // 如果协程抛出异常会来到这里,这里只是记录一下,并不会真正抛出
        mException = std::current_exception();
    }
    // co_return 设置返回值
    void return_value(T &&ret) {
        // 右值作为参数传递进来会退化成const_ref
        mResult.putValue(std::move(ret));
    }

    void return_value(T const& ret) {
        mResult.putValue(ret);
    }
    // co_yield 设置返回值
    auto yield_value(T && ret) {
        mResult.putValue(std::move(ret));
        return std::suspend_always(); //继续 Awaitable对象
    }

    auto yield_value(T const& ret) {
        mResult.putValue(ret);
        return std::suspend_always(); //继续
    }

    // co_return 的结果
    T result() {
        // 调用结果的时候再抛出异常,将异常一层一层往上传递
        // [[unlikely]] 会使CPU预取指令到另一个分支
        if (mException) [[unlikely]] {
            std::rethrow_exception(mException);
        } else return mResult.moveValue();
    }

    auto get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    // 作为一个promise,要记录我的调用者是谁,我的异常,我的协程返回值
    // 这里的mPreivous是通过Task中的Awaiter->await_suspend传入的
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
    // union {
    //     T mResult;
    // };
    Uninitialized<T> mResult; // 底层是union

    // 作为一个Promise,不应该移动和拷贝
    // 除了构造函数其它五个也删除了,小技巧
    Promise& operator=(Promise &&) = delete;
};

// 给返回类型为void的单独整一份
template <>
struct Promise<void> {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        // 这里实现了递归调用协程,保存一个mPrevious协程句柄,等待co_return的时候执行mPrevious
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        // 如果协程抛出异常会来到这里,这里只是记录一下,并不会真正抛出
        mException = std::current_exception();
    }

    // 在一个Promise中return_void和return_value只能二存一
    void return_void() noexcept {}
    // void return_value() { }

    // void 是数据成员的类型,既然是void就不可能再yield的时候存数据

    // 还有可能要抛出异常
    // [[unlikely]]涉及到编译器优化,干预CUP预取指令,直接预取到[[unlikely]]的下一个分支
    void result() {
        if (mException) [[unlikely]] {
            std::rethrow_exception(mException);
        }
    }

    auto get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    // 作为一个promise,要记录我的调用者是谁,我的异常,我的协程返回值
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};

    // 作为一个Promise,不应该移动和拷贝
    // 除了构造函数其它五个也删除了,小技巧
    Promise& operator=(Promise &&) = delete;
};

// 针对于返回类型为void的任务只需要单独设置一份Promise即可,不需要特化Task
template <class T = void, class P = Promise<T>>
struct Task {
    // 对于一个Task类,必须有promise_type -> Promise<T> 类型
    using promise_type = P;

    using returnType = AwaitableTraits<T>::Type;

    // coroutine_handle<T>中的T表示的是协程句柄承诺返回的类型
    // 一个为 T 类型的Task 其promise_type也得是T表返回类型
    Task(std::coroutine_handle<promise_type> coroutine) noexcept : mCoroutine(coroutine) { }

    Task(Task &&that) noexcept : mCoroutine(that.mCoroutine) {
        that.mCoroutine = nullptr;
    }

    Task &operator=(Task &&that) noexcept {
        std::swap(mCoroutine, that.mCoroutine);
        return *this;
    }

    ~Task() {
        if (mCoroutine) {
            mCoroutine.destroy();
        }
    }

    // Task里面有一个Awaiter, 是为了co_await 另一个协程
    // 使用了C++17的 CTAD
    // 比如:Task<int> 中 co_await了Task<double>, 生成的Awaiter是Task<double>的->Promise<double>
    struct Awaiter {
        bool await_ready() const noexcept { return false; }
        // 如果没有准备好,就来下面,挂起,保存previous,继续执行自己
        // 这里的std::coroutine_handle<void> 类型擦除相当于std::any
        std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> coroutine) {
            // 如果存在递归调用,并且没有记录上一个协程句柄,那么当前协程执行完就会返回到主线程
            // x86架构是将返回地址推入栈,这里是直接记录
            promise_type &promise = mCoroutine.promise();
            promise.mPrevious = coroutine;
            return mCoroutine;
        }
        // 子协程执行完父协程会调用这个函数,如果子协程有一样,那么在这里会rethrow
        // 之后如果父协程还有有父父协程就会再次设置自己的mExcpetion
        // 直到出了协程之后,调用promise().result(),此时就会真正抛出异常
        T await_resume() const {
            return mCoroutine.promise().result();
        }
        // 任务协程句柄必须要有promise_type
        // 注意 Awaiter中的mCoroutine 和 Task 中的 mCoroutine不是一个
        // Awaiter 中的mCoroutine是被调用者的协程句柄
        std::coroutine_handle<promise_type> mCoroutine;
    };
    // 调用co_await 就返回一个等待者对象,等mCoroutine执行完
    // 谁调用co_await await_suspend中的参数就是谁
    auto operator co_await() const noexcept {
        return Awaiter(mCoroutine);// 将自己作为参数传入,被调用的协程执行Awaiter, 返回Awaiter中的mCoroutine
    }

    // 任务可以转换为协程句柄
    operator std::coroutine_handle<>() const noexcept {
        return mCoroutine;
    }

    std::coroutine_handle<promise_type> mCoroutine;

};

template <class Loop, class T, class P>
T run_task(Loop &loop, Task<T, P> const& t) {
    auto a = t.operator co_await();
    a.await_suspend(std::noop_coroutine()).resume();
    loop.process();
    return a.await_resume();
}

}