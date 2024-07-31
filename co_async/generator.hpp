/**
 * @file generator.hpp
 * @author qc
 * @brief 生成器, 里面可以不停yield
 * @version 0.1
 * @date 2024-07-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <optional>
#include <coroutine>
#include <exception>
#include <utility>
#include <utilities/uninitialized.hpp>
#include "previous_awaiter.hpp"

namespace co_async {

template <class T>
struct GeneratorPromise {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        mException = std::current_exception();
        mFinal = true;
    }

    // 相较于其他的promise,对于生成器而言,其特点就是可以yield
    auto yield_value(T const& val) noexcept {
        mResult.putValue(val);
        return PreviousAwaiter(mPrevious);
    }

    auto yield_value(T && val) noexcept {
        mResult.putValue(std::move(val));
        return PreviousAwaiter(mPrevious);
    }

    void return_void() {
        mFinal = true;
    }

    bool isfinal() {
        if (mFinal) {
            if (mException) [[unlikely]]
                std::rethrow_exception(mException);
        }
        return mFinal;
    }

    T result() {
        return mResult.moveValue();
    }

    auto get_return_object() {
        return std::coroutine_handle<GeneratorPromise>::from_promise(*this);
    }

    GeneratorPromise& operator= (GeneratorPromise&&) = delete;

    Uninitialized<T> mResult;
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
    bool mFinal = false;
};

// 特化引用类型
template <class T>
struct GeneratorPromise<T &> {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() {
        mException = std::current_exception();
        mResult = nullptr;
    }

    auto yield_value(T &ret) {
        mResult = std::addressof(ret);
        return PreviousAwaiter(mPrevious);
    }

    void return_void() {
        mResult = nullptr;
    }

    bool isfinal() {
        if (!mResult) {
            if (mException) [[unlikely]]
                std::rethrow_exception(mException);
            return true;
        }
        return false;
    }

    T& result () {
        return *mResult;
    }

    auto get_return_object() {
        return std::coroutine_handle<GeneratorPromise>::from_this(*this);
    }

    GeneratorPromise& operator= (GeneratorPromise &&) = delete;

    T *mResult;
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};

// Task
template <class T, class P = GeneratorPromise<T>>
struct [[nodiscard]] Generator {
    using promise_type = P;

    Generator(std::coroutine_handle<promise_type> coroutine = nullptr) noexcept {
        mCoroutine = coroutine;
    }

    Generator(Generator &&that) noexcept : mCoroutine(that.mCoroutine) {
        that.mCoroutine = nullptr;
    }

    Generator& operator = (Generator &&that) noexcept {
        std::swap(mCoroutine, that.mCoroutine);
    }

    ~Generator() {
        if (mCoroutine)
            mCoroutine.destroy();
    }

    struct GeneratorAwaiter {
        bool await_ready() const noexcept { return false; }
        
        std::coroutine_handle<promise_type>
        await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        std::optional<T> await_resume() const {
            if (mCoroutine.promise().isfinal()) {
                return std::nullopt;
            }
            return mCoroutine.promise().result();
        }

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() const noexcept {
        return GeneratorAwaiter(mCoroutine);
    }

    operator std::coroutine_handle<promise_type>() const noexcept {
        return mCoroutine;
    }

    std::coroutine_handle<promise_type> mCoroutine{};

};

#if 0
template <class T, class A, class LoopRef>
struct GeneratorIterator {
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    explicit GeneratorIterator(A awaiter, LoopRef loop) noexcept
        : mAwaiter(awaiter),
          mLoop(loop) {
        ++*this;
    }

    bool operator!=(std::default_sentinel_t) const noexcept {
        return mResult.has_value();
    }

    bool operator==(std::default_sentinel_t) const noexcept {
        return !(*this != std::default_sentinel);
    }

    friend bool operator==(std::default_sentinel_t,
                           GeneratorIterator const &it) noexcept {
        return it == std::default_sentinel;
    }

    friend bool operator!=(std::default_sentinel_t,
                           GeneratorIterator const &it) noexcept {
        return it == std::default_sentinel;
    }

    GeneratorIterator &operator++() {
        mAwaiter.mCoroutine.resume();
        mLoop.run();
        mResult = mAwaiter.await_resume();
        return *this;
    }

    GeneratorIterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    T &operator*() noexcept {
        return *mResult;
    }

    T *operator->() noexcept {
        return mResult.operator->();
    }

private:
    A mAwaiter;
    LoopRef mLoop;
    std::optional<T> mResult;
};

template <class Loop, class T, class P>
auto run_generator(Loop &loop, Generator<T, P> const &g) {
    using Awaiter = typename Generator<T, P>::Awaiter;

    struct GeneratorRange {
        explicit GeneratorRange(Awaiter awaiter, Loop &loop)
            : mAwaiter(awaiter),
              mLoop(loop) {
            mAwaiter.await_suspend(std::noop_coroutine());
        }

        auto begin() const noexcept {
            return GeneratorIterator<T, Awaiter, Loop &>(mAwaiter, mLoop);
        }

        std::default_sentinel_t end() const noexcept {
            return {};
        }

    private:
        Awaiter mAwaiter;
        Loop &mLoop;
    };

    return GeneratorRange(g.operator co_await(), loop);
};
#endif

}