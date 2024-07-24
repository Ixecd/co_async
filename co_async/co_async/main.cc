#include "qc.hpp"
#include <chrono>
#include <coroutine>
#include <deque>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <variant>
#include <vector>

using namespace std::chrono_literals;

/* 所有和标准库相关的使用下划线+小写命名法, 自己的函数使用驼峰命名法,
 * 自己的类使用大写驼峰命名法 */
/* 协程C++20有(编译器)硬编码的名字(xxx_xxx) */
namespace co_async {

// 所有具有await_ready/await_suspend/await_resume的都叫 awaiter /
// awaitable(可以不具有这仨,但是里面有成员函数重载co_await 返回awaiter)
// 相当于智能指针中 awaiter(原始指针) / awaitable(operator->)
struct RepeatAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if (coroutine.done()) {
            return std::noop_coroutine(); // 如果直接返回std::noop_coroutine(),一步一停顿
                                          // 和 直接返回void是一样的
        } else {
            return coroutine;
        }
    }

    void await_resume() const noexcept {}
};

struct RepeatAwaitable {
    RepeatAwaiter operator co_await() {
        return RepeatAwaiter();
    }
};

struct SuspendAlways {
    // 挂起状态,如果准备好了,直接resume
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if (coroutine.done()) {
            return std::noop_coroutine();
        }
        return coroutine;
    }

    void await_resume() const noexcept {}
};

struct PreviousAwaiter {
    std::coroutine_handle<> m_Previous;

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<>) const noexcept {
        if (m_Previous) {
            return m_Previous;
        } else {
            return std::noop_coroutine();
        }
    }

    void await_resume() const noexcept {}
};

template <class T>
struct Promise {
    auto initial_suspend() { // 刚构建的时候
        return std::suspend_always();
    }

    auto final_suspend() noexcept { // co_return 的时候返回一下
        return PreviousAwaiter(mPrevious);
        // std::suspend_always();
    }

    void unhandled_exception() {
        DEBUG(UNHANDLED_EXCEPTION);
        // 异常的指针(从哪里抛出来的?)
        // mResult.emplace<0>(std::current_exception());
        mExceptionPtr = std::current_exception();
        // std::rethrow_exception(std::current_exception());
        // throw; // 简写跟上面效果一样
    }

    // return_void 和 return_value 只能二选一
    // void return_void() {}
    // void return_value(int val) {
    //     mResult = val;
    // }

    // void return_void() {}
    void return_value(T ret) {
        // mResult.emplace<1>(ret);
        // new (&mResult) T(std::move(ret));
        std::construct_at(&mResult, std::move(ret));
    }

    auto yield_value(T ret) {
        // new (&mResult) T(std::move(ret));
        std::construct_at(&mResult, std::move(ret));
        // mResult.emplace<1>(ret);
        return std::suspend_always();
        // PreviousAwaiter(mPrevious);
        //  RepeatAwaitable(); // 只返回最后一个结果
    }

    T result() const { // 一层一层往上传,遇到调用result的时候才会真正的抛出异常
        // int *retValue = (int *)std::get_if<1>(&mResult);
        if (mExceptionPtr) [[unlikely]] {
            std::rethrow_exception(mExceptionPtr);
        }

        T ret = std::move(mResult);
        std::destroy_at(&mResult);
        return ret;
    }

    // 从promise的引用转换为一个协程的句柄, 后面再发生一次隐式转换
    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    // 异常和返回值只能发生发一个,让这俩共用同一个地址空间
    // std::optional<T> mResult{};
    // std::variant<std::exception_ptr, int> mResult{};
    std::exception_ptr mExceptionPtr{};
    std::coroutine_handle<> mPrevious = nullptr;

    union {
        T mResult;
    };

    Promise() noexcept {}

    Promise(Promise &&) = delete;

    ~Promise() {} // 析构函数自动 noexcept
};

template <>
struct Promise<void> {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        mException = std::current_exception();
    }

    void return_void() noexcept {}

    void result() {
        if (mException) [[unlikely]] {
            std::rethrow_exception(mException);
        }
    }

    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};

    Promise() = default;
    Promise(Promise &&) = delete;
    ~Promise() = default;
};

// ---------- 调度器 -----------
struct Loop {
    // Loop() = default;
    // Loop(Loop &&) = delete;
    Loop &
    operator=(Loop &&) = delete; // 一次性删除五个函数 除了默认构造函数不删

    void addTask(std::coroutine_handle<> task) {
        // mLock.lock();
        mReadyQueue.push_back(task);
        // mLock.unlock();
    }

    struct TimerEntry {
        std::chrono::system_clock::time_point expireTime;
        std::coroutine_handle<> coroutine;

        TimerEntry(std::chrono::system_clock::time_point ms,
                   std::coroutine_handle<> task) {
            expireTime = ms;
            coroutine = task;
        }

        // TimerEntry(TimerEntry const& that) noexcept:
        // expireTime(that.expireTime), coroutine(that.coroutine) {
        // }

        // TimerEntry(TimerEntry &&that) noexcept:
        // expireTime(std::move(that.expireTime)),
        // coroutine(std::move(that.coroutine)) {
        //     that.coroutine = nullptr;
        // }

        struct MyCompareTimer {
            bool operator()(TimerEntry const &lhs,
                            TimerEntry const &rhs) const noexcept {
                return lhs.expireTime > rhs.expireTime;
            }
        };

        bool operator<(TimerEntry const &that) const noexcept {
            return expireTime > that.expireTime;
        }
    };

    /**
     * @brief 添加定时器
     *
     * @param ms 激活时间
     * @param task 激活协程,谁调用激活谁
     */
    void addTimer(std::chrono::system_clock::time_point ms,
                  std::coroutine_handle<> task) {
        mTimerHeap.push({ms, task});
    }

    void process() {
        while (!mTimerHeap.empty() || !mReadyQueue.empty()) {
            while (!mReadyQueue.empty()) {
                // mLock.lock();
                auto readyTask = mReadyQueue.front();
                mReadyQueue.pop_front();
                // mLock.unlock();
                readyTask.resume();
            }
            if (!mTimerHeap.empty()) {
                // auto it = std::upper_bound(timerQueue.begin(),
                // timerQueue.end(), std::chrono::system_clock::now());
                // mExpiredList.assign(timerQueue.begin(), it);
                // timerQueue.erase(timerQueue.begin(), it);
                // for (size_t i = 0; i < mExpiredList.size(); ++i)
                // mReadyQueue.push_front(mExpiredList[i].coroutine);
                auto nowTime = std::chrono::system_clock::now();
                TimerEntry timer =
                    mTimerHeap.top(); // priority_queue.top() -> const_reference
                                      // -> 拷贝构造函数(移动没有意义) ->
                                      // 不能修改绑定的对象
                // auto const& timer = mTimerHeap.top(); //
                // mTimerHeap里的东西马上就会被清除,所以不能使用const& 来接收
                if (timer.expireTime < nowTime) {
                    mTimerHeap.pop();
                    timer.coroutine.resume();
                } else {
                    std::this_thread::sleep_until(timer.expireTime);
                }
            }
        }
    }

    // std::unique_lock<std::mutex> mLock;
    std::deque<std::coroutine_handle<>> mReadyQueue;
    std::deque<std::coroutine_handle<>> mWaitingQueue;
    // std::vector<TimerEntry> mExpiredList;
    // std::set<TimerEntry, TimerEntry::MyCompareTimer> timerQueue;
    std::priority_queue<TimerEntry> mTimerHeap;
};

// 单例模式
static Loop *getLoop() {
    static Loop loop; // static很智能,只能构造一次,多线程安全的,是因为static
    return &loop;
}

struct SleepAwaiter {
    bool await_ready() const {
        return std::chrono::system_clock::now() >= mExpireTime;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        // 目前是一个同步的
        // std::this_thread::sleep_until(mExpireTime);
        // 如果要等的话直接return noop::coroutine()返回到主线程中
        getLoop()->addTimer(mExpireTime, coroutine);
        return std::noop_coroutine();
    }

    void await_resume() const noexcept {}

    std::chrono::system_clock::time_point mExpireTime;
};

template <class T = void>
struct Task {
    using promise_type = Promise<T>;

    Task(std::coroutine_handle<promise_type> coroutine)
        : mCoroutine(coroutine) {}

    // 定义了析构函数就要三五
    Task(Task &&) = delete;

    ~Task() {
        mCoroutine.destroy();
    }

    struct Awaiter {
        bool await_ready() const noexcept {
            return false;
        }

        // 参数里的coroutine 是调用这个协程的协程, mCoroutine是自己
        // std::coroutine_handle<> 类型擦除,可以接受<T>会转换为void
        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            // 挂起
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        // resume的时候的返回值,如果有 int i = co_await world();
        T await_resume() const {
            return mCoroutine.promise().result();
        }

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() const noexcept {
        return Awaiter(mCoroutine);
    }

    // Task 必须可以隐式转换为coroutine_handle();
    operator std::coroutine_handle<>() const noexcept {
        return mCoroutine;
    }

    // 协程句柄
    std::coroutine_handle<promise_type> mCoroutine;
};

// 针对when_all的promise和task
struct ReturnPreviousPromise {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() {
        throw;
    }

    void return_value(std::coroutine_handle<> previous) noexcept {
        mPrevious = previous;
    }

    auto get_return_object() {
        return std::coroutine_handle<ReturnPreviousPromise>::from_promise(
            *this);
    }

    std::coroutine_handle<> mPrevious = nullptr;

    ReturnPreviousPromise &operator=(ReturnPreviousPromise &&) = delete;
};

struct ReturnPreviousTask {
    using promise_type = ReturnPreviousPromise;

    ReturnPreviousTask(std::coroutine_handle<promise_type> coroutine) noexcept
        : mCoroutine(coroutine) {}

    ~ReturnPreviousTask() {
        mCoroutine.destroy();
    }

    operator std::coroutine_handle<>() {
        return mCoroutine;
    }

    std::coroutine_handle<promise_type> mCoroutine;
};

struct WhenAllCounterBlock {
    WhenAllCounterBlock operator++(int) {
        auto tmp = *this;
        mCount += 1;
        return tmp;
    }

    WhenAllCounterBlock &operator++() {
        mCount += 1;
        return *this;
    }

    WhenAllCounterBlock operator--(int) {
        auto tmp = *this;
        mCount -= 1;
        return tmp;
    }

    WhenAllCounterBlock &operator--() {
        mCount -= 1;
        return *this;
    }

    size_t mCount = 0;
    std::coroutine_handle<> mPrevious = nullptr;
    std::exception_ptr mException{};
};

ReturnPreviousTask whenAllHelper(Task<void> const &t,
                                 WhenAllCounterBlock &counter) {
    co_await t;
    counter--;
    std::coroutine_handle<> previous = nullptr;
    if (counter.mCount == 0) {
        previous = counter.mPrevious;
    }
    co_return previous;
}

struct WhenAllAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        counter.mPrevious = coroutine;
        // 这里t1和t2执行完要通知一下,
        // coroutine是when_all的给丢了,所以没有返回42 getLoop()->addTask(t1);
        getLoop()->addTask(t2.mCoroutine);
        return t1.mCoroutine; // 更高效
    }

    void await_resume() const noexcept {}

    WhenAllCounterBlock &counter;
    ReturnPreviousTask const &t1;
    ReturnPreviousTask const &t2;
};

// **
Task<void> when_all(Task<void> const &t1, Task<void> const &t2) {
    WhenAllCounterBlock counter;
    counter.mCount = 2;
    co_await WhenAllAwaiter(
        counter, whenAllHelper(t1, counter),
        whenAllHelper(t2, counter)); // t1 和 t2都执行完才会执行下面的
}

} // namespace co_async

using namespace co_async;
#include <string>

Task<std::string> baby() {
    DEBUG(baby);
    co_return "aaa";
}

Task<double> world() {
    DEBUG(world);

    // 任务里面可以抛出异常
    // throw std::runtime_error("world失败"); //
    // 被promise里的unhandled_exception()接收到

    // co_yield 3.14; // 也就是说yield会返回到main线程,co_return
    // 可以返回给调用者而不是main线程 co_yield 1.22;
    co_return 3.56;
}

Task<int> hello() {
    // auto ret = co_await baby();
    // PRINT(ret);

    // // DEBUG(正在构建worldTask);
    // // Task<double> worldTask = world();
    // // DEBUG(构建完了worldTask);
    // // double ret = co_await worldTask; // 这里world执行完毕会返回到main中
    // double i = co_await world();
    // DEBUG(hello得到world结果为);
    // PRINT(i);
    // co_return i + 1;

    // PRINT_S(hello得到world返回值为:);
    // PRINT(ret);

    // PRINT(worldTask.mCoroutine.promise().result());

    // co_await worldTask;

    // PRINT(worldTask.mCoroutine.promise().result());

    // DEBUG(world又一次执行完毕);

    // // C++20编译器会找一些固定的名字(硬编码),找不到就报错
    // DEBUG(hello 42);
    // co_yield 42; //
    // 编译器看到这个就会认为这个函数是协程,协程函数的返回类型必须是具有promise_type成员的类,promise_type
    // -> Promise这个类又必须有写main写的五个标准成员函数 DEBUG(hello 199);
    // co_yield 199;
    // DEBUG(hello 12);
    // co_yield 12; // co_return 表示任务已经执行完了, 使用co_yield
    co_return 42;
}

// sleep需要知道loop,到时间还要塞回队列
Task<void> sleep_until(std::chrono::system_clock::time_point expireTime) {
    co_await SleepAwaiter(expireTime);
    co_return;
}

Task<void> sleep_for(std::chrono::system_clock::duration duration) {
    co_await sleep_until(std::chrono::system_clock::now() + duration);
    co_return;
}

Task<void> hello1() {
    DEBUG(hello1开始睡1s);
    co_await sleep_for(std::chrono::seconds(1));
    DEBUG(hello1睡完了1s);
    co_return;
}

// using namespace std::chrono_literals;
Task<void> hello2() {
    DEBUG(hello2开始睡2s);
    co_await sleep_for(2s); // using std::chrono_literals
    DEBUG(hello2睡完了2s);
    co_return;
}

Task<int> taskWhenAll() {
    DEBUG(taskWhenALL);
    co_await when_all(hello1(), hello2());
    DEBUG(hello1Andhello2Awake);
    co_return 21;
}

void testTimer() {
    auto t1 = hello1();
    auto t2 = hello2();
    DEBUG(createTaskSucc);
    getLoop()->addTask(t1);
    getLoop()->addTask(t2);
    DEBUG(addTaskSucc);
    getLoop()->process();
    // cout << void ; // err
    // PRINT(t1.mCoroutine.promise().result());
    // PRINT(t2.mCoroutine.promise().result());
}

void testWhenall() {
    auto t = taskWhenAll();
    getLoop()->addTask(t);
    getLoop()->process();
    DEBUG(t.mCoroutine.promise().result());
}

int main() { // suspend_always 一步一停顿

    // Task t = a();
    // DEBUG(createTask)
    // getLoop()->addTask(t);
    // DEBUG(addTaskSucc);
    // getLoop()->process();

    testWhenall();

    // while (1) {}

    // int const& a= 100;
    // int b = std::move(a); // 对于POD来说ok,对于自定义struct没意义,常量引用
    // cosnt& -> const * const不能修改其绑定的对象 PRINT(b);

    return 0;
}
