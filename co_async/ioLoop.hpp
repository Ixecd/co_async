/**
 * @file ioLoop.hpp
 * @author qc
 * @brief epoll是单线程且同步非阻塞API,轮询方式,不能异步 --> 结合协程实现单线程异步
 * @details epoll中的epoll_event的 data 是一个union 里面包含fd,ptr... 加的时候用这个
 *          自定义的 fd 是 int类型不够大 -> 用 u64 或者 u32(42.9亿) -> 保存自定义编号
 *          使用文件描述符API来接收数据read / write->操作系统提供
 *          epoll中加入的fd要设置为非阻塞fcntl,这样read就算没有读完,下一次循环还是会读剩下的.
 *          为啥用fcntl?因为在这之前已经用open设置好文件属性了,只能更改
 *          fcntl还是不行,最佳解决方案是sys/ioctl -> ioctl(fd,FIONBIO, &attr)
 * @version 0.1
 * @date 2024-07-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <co_async/task.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/when_any.hpp>
#include <co_async/when_all.hpp>
#include <co_async/and_then.hpp>
#include <system_error>
#include <cerrno>


namespace co_async {

auto checkError(auto res) {
    if (res == -1) [[unlikely]] 
        throw std::system_error(errno, std::system_category());
    return res;
}

struct IoFilePromise : Promise<void> {


    auto get_return_object() {
        return std::coroutine_handle<IoFilePromise>::from_promise(*this);
    }

    ~IoFilePromise();

    IoFilePromise& operator=(IoFilePromise&&) = delete;

    // Promise和IoLoop相互依赖,所以这里用指针,并且析构函数要放下实现,还要记得+inline
    struct IoLoop *mLoop;
    int mFd;
    uint32_t mEvents;
};

// 一个loop对应一个epoll
struct IoLoop {
    void addListener(IoFilePromise &promise) {
        struct epoll_event event;
        // EPOLLONSHOT 只触发一次
        event.events = promise.mEvents;
        event.data.ptr = &promise;
        checkError(epoll_ctl(mEpfd, EPOLL_CTL_ADD, promise.mFd, &event));
    }

    void removeListener(int fd) {
        checkError(epoll_ctl(mEpfd, EPOLL_CTL_DEL, fd, nullptr));
    }

    void tryRun(int timeout = 1000) {
        struct epoll_event ebuf[10];
        int rt = checkError(epoll_wait(mEpfd, ebuf, 10, timeout));
        if (rt > 0) {
            PRINT_S(getEvent!);
            for (int i = 0; i < rt; ++i) {
                auto &event = ebuf[i];
                auto &promise = *(IoFilePromise *)ebuf[i].data.ptr;
                //checkError(epoll_ctl(mEpfd, EPOLL_CTL_DEL, promise.mFd, nullptr));
                // 下面promise 对应的协程句柄执行完就会析构掉promise,也就会析构掉事件
                std::coroutine_handle<IoFilePromise>::from_promise(promise).resume();
            }
        }
        
    }

    // 只保留默认构造函数
    IoLoop& operator = (IoLoop&&) = delete;

    ~IoLoop() {
        close(mEpfd);
    }

    // C++11 直接在结构体中初始化一个变量
    int mEpfd = checkError(epoll_create1(0));

    struct epoll_event mEventBuf[64];
};

inline
IoFilePromise::~IoFilePromise() {
    mLoop->removeListener(mFd);
}

// 大头在这里
struct IoAwaiter {

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<IoFilePromise> coroutine) const {
        auto &promise = coroutine.promise();
        promise.mLoop = &mLoop;
        promise.mFd = mFd;
        promise.mEvents = mEvents;
        mLoop.addListener(promise);
    }

    void await_resume() const noexcept { }

    using ClockType = std::chrono::system_clock;

    IoLoop &mLoop;
    int mFd;
    uint32_t mEvents;
};
// 这里就不会resume了,因为是io操作,一个wait_file就是向epoll中监听一个文件描述符
Task<void, IoFilePromise>
wait_file(IoLoop &loop, int fd, uint32_t events) {
    co_await IoAwaiter(loop, fd, events | EPOLLONESHOT);
}


}