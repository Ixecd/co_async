#pragma once

#include <exception>
#include <coroutine>
#include "task.hpp"

namespace co_async {

// mCoroutine.promise()->mPrevious
// Promise是协程句柄的成员
struct ReturnPreviousPromise {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return //std::suspend_always();
        PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() {
        throw;
    }
    
    // 这里是和Task中Promise的主要区别
    // 测试了一下没必要,因为在WhenAllAwaite中已经记录了调用者的信息
    // 为啥参数是协程句柄??
    // 又测试了一下很有必要,要不然可以调用但是不能结构化绑定,为啥啊??(没回到Task)
    // 下面是详细解释
    // 作为一个ReturnPreviousTask类就是为了返回previous所以必须记录previous
    // co_return control.mPrevious;

    // Q :记得为什么要执行span的时候为什么要留一个吗?
    // A1:就是为了return 到Task中的时候 让Task执行这个mCoroutine,执行的时候一定会使
    // contorl中的count减为0,好让co_return control.mPrevious, 设置returnPromise
    // 的previous为 control.mPrevious 之后ReturnPreviousPromise中调用final_suspend->回到Task

    // A2:ReturnPreviousTask中的co_return control.mPrevious 是为了在对应的Promise中
    // 设置自己的previous为 Task 仅此而已,之后这最后一个任务执行完会调用自己的Promise中
    // 的final_suspend() -> PreviousAwaiter(mPrevious) -> 才真正返回到Task中.
    // 如果在ReturnPreviousPromise中不记录的话,那么就回不到Task

    // A2:其实也可以不用留,将所有事情都交给Awaiter即可,Awaiter中传入的是调用者的协程句柄
    // Awaiter中的await_suspend()->处理完所有的子协程->顺序一个一个resume->之后返回coroutine参数就行
    // 这样在ReturnPreviousTask中也不需要判断count是否减为0,直接返回std::noop_coroutine即可
    // ReturnPreviousTask只负责将得到的任务协程的结果putValue进result就行.

    // Q :又测试了一下发现不留也能回去为啥??
    // A : 因为在Task中调用co_await已经将当前协程句柄记录在CtlBlock中了,就算在子协程直接返回std::noop_coroutine()
    // 也一定会触发co_return control.mPrevious也就一定会回到Task中
    void return_value(std::coroutine_handle<> previous) noexcept {
        mPrevious = previous;
    }

    auto get_return_object() {
        return std::coroutine_handle<ReturnPreviousPromise>::from_promise(
            *this);
    }

    std::coroutine_handle<> mPrevious;

    ReturnPreviousPromise &operator=(ReturnPreviousPromise &&) = delete;
};

struct ReturnPreviousTask {
    using promise_type = ReturnPreviousPromise;

    ReturnPreviousTask(std::coroutine_handle<promise_type> coroutine) noexcept
        : mCoroutine(coroutine) {}

    ReturnPreviousTask(ReturnPreviousTask &&) = delete;

    ~ReturnPreviousTask() {
        mCoroutine.destroy();
    }

    // 如果要加入任务队列就得隐式转换
    operator std::coroutine_handle<> () const noexcept {
        return mCoroutine;
    }

    std::coroutine_handle<promise_type> mCoroutine;
};

}