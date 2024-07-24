#include <coroutine>

struct ReturnPreviousPromise {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) {}

    void await_resume() const noexcept {}

    std::coroutine_handle<> mPrevious = nullptr;

    ReturnPreviousPromise &operator=(ReturnPreviousPromise &&) = delete;
};

struct ReturnPreviousTask {};
