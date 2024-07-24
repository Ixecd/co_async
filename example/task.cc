#include <chrono>
#include <coroutine>
#include <co_async/qc.hpp>
#include <co_async/task.hpp>
#include <co_async/concepts.hpp>
#include <co_async/uninitialized.hpp>
#include <co_async/non_void_helper.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;

// 这是一个协程体, 编译器看见co_return 就会认为这是一个协程函数
// hello() 才是一个任务
// 协程的近亲就是函数
Task<int> hello() {

    co_return 42;
}

int func() {
    int x = 100;
    PRINT(x);
    return 1;
}


int main() {
    // 只是生成一个任务
    auto t1 = hello();
    while (!t1.mCoroutine.done()) 
        t1.mCoroutine.resume();
    PRINT(t1.mCoroutine.promise().result());

    return 0;
}
