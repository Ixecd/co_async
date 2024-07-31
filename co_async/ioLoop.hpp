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
#include <source_location>
#include <co_async/task.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/when_any.hpp>
#include <co_async/when_all.hpp>
#include <co_async/and_then.hpp>
#include <system_error>
#include <span>
#include <cerrno>
#include <termios.h>

// Q : 遇到一个非常棘手的问题,在同一个头文件下几个类高度依赖,如何安排他们的顺序?
// A : 1. 将类中对象改为指针.解耦合
//     2. 将类中的成员函数放到后面实现.延迟实现

namespace co_async {

// wait_file 要返回已经触发的事件
using IoEventMask = std::uint32_t;


// static void disable_canon() {
//     struct termios tc;
//     tcgetattr(STDIN_FILENO, &tc);
//     // 输入流回显缓冲区全部关掉
//     tc.c_lflag &= ~ICANON;
//     tc.c_lflag &= ~ECHO;
//     tcsetattr(STDIN_FILENO, TCSANOW, &tc);
// }


auto checkError(auto res, std::source_location const& loc = std::source_location::current()) {
    if (res == -1) [[unlikely]]
        // system_error cpp13 从错误码构造,底部调用strerror获取真正报错信息
        // source_location -> 调用者的位置
        throw std::system_error(errno, std::system_category(),
                                (std::string)loc.file_name() + ":" + std::to_string(loc.line()));
    return res;
}

struct IoFilePromise : Promise<IoEventMask> {

    auto get_return_object() {
        return std::coroutine_handle<IoFilePromise>::from_promise(*this);
    }

    ~IoFilePromise();

    IoFilePromise& operator=(IoFilePromise&&) = delete;

    // Promise和IoLoop相互依赖,所以这里用指针,并且析构函数要放下实现,还要记得+inline
    // struct IoLoop *mLoop;
    // int mFd;
    // uint32_t mEvents;

    // 直接记录Awaiter,由Awaiter记录信息
    struct IoFileAwaiter *mAwaiter{};
};

// 对文件描述符进行封装
// [[nodiscard]] 如果没有co_await会警告
struct [[nodiscard("no co_await")]] AsyncFile {
    AsyncFile() : mFd(-1) {}

    explicit AsyncFile(int fd) noexcept: mFd(fd) {}

    AsyncFile(AsyncFile const& that) noexcept: mFd(that.mFd) {}

    AsyncFile(AsyncFile && that) noexcept {
        mFd = std::move(that.mFd);
        that.mFd = -1;
    }

    AsyncFile& operator= (AsyncFile const& that) noexcept {
        if (this == &that) return *this;
        mFd = that.mFd;
        return *this;
    }

    AsyncFile& operator= (AsyncFile && that) noexcept {
        if (this == &that) return *this;
        mFd = std::move(that.mFd);
        that.mFd = -1;
        return  *this;
    }

    int fileNo() const noexcept {
        return mFd;
    }

    int releaseOwnership() noexcept {
        int rt = mFd;
        mFd = -1;
        return rt;
    }

    void setNonblock() const {
        int attr = 1;
        checkError(ioctl(fileNo(), FIONBIO, &attr));
    }

    ~AsyncFile() { mFd = -1; }

private:
    int mFd;
};

// 一个loop对应一个epoll
struct IoLoop {
    bool addListener(IoFilePromise &promise, int op);

    void removeListener(AsyncFile &file) {
        checkError(epoll_ctl(mEpfd, EPOLL_CTL_DEL, file.fileNo(), nullptr));
        --mCount;
    }

    bool hasEvent() const noexcept { return mCount != 0; }

    bool tryRun(std::optional<std::chrono::system_clock::duration> timeout = std::nullopt);

    void process() {
        while (1) {
            bool rt = tryRun(1s);
            if (rt == false) break;
        }
    }

    // 只保留默认构造函数
    IoLoop& operator = (IoLoop&&) = delete;

    ~IoLoop() {
        close(mEpfd);
    }

    // C++11 直接在结构体中初始化一个变量
    int mEpfd = checkError(epoll_create1(0));

    std::size_t mCount = 0;

    struct epoll_event mEventBuf[64];
};
// 保存的所有东西都丢到这里,Promise中只保存一个Awaiter对象即可
struct IoFileAwaiter {

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<IoFilePromise> coroutine) {
        auto &promise = coroutine.promise();
        promise.mAwaiter = this;
        if (!mLoop.addListener(promise, op)) {
            // 添加失败
            promise.mAwaiter = nullptr;
            coroutine.resume();
        }
    }

    IoEventMask await_resume() const noexcept {
        return mResumeEvents;
    }

    using ClockType = std::chrono::system_clock;

    IoLoop &mLoop;
    // int mFd;
    AsyncFile &mFd;
    IoEventMask mEvents;
    IoEventMask mResumeEvents = 0;
    int op = EPOLL_CTL_ADD;
};

inline
IoFilePromise::~IoFilePromise() {
    mAwaiter->mLoop.removeListener(mAwaiter->mFd);
}

inline bool 
IoLoop::addListener(IoFilePromise &promise, int op) {
    struct epoll_event event;
    // EPOLLONSHOT 只触发一次 -> 避免对同一个fd重复添加相同event
    event.events = promise.mAwaiter->mEvents;
    event.data.ptr = &promise;
    // 服务器一旦运行起来,如果你下面直接抛出异常了,那还服务啥??
    // checkError(epoll_ctl(mEpfd, EPOLL_CTL_ADD, promise.mAwaiter->mFd, &event));
    int rt = epoll_ctl(mEpfd, op, promise.mAwaiter->mFd.fileNo(), &event);
    if (rt == -1) return false;
    if (op == EPOLL_CTL_ADD) mCount++;
    return true;
}

inline bool 
IoLoop::tryRun(std::optional<std::chrono::system_clock::duration> timeout) {
    if (mCount == 0) {
        PRINT_S(没事件);
        return false;
    }
    int timeoutInMs = -1;
    if (timeout) timeoutInMs = std::chrono::duration_cast<std::chrono::milliseconds>(*timeout).count();
    int rt = checkError(epoll_wait(mEpfd, mEventBuf, 10, timeoutInMs));
    if (rt > 0) {
        // PRINT_S(getEvent!);
        for (int i = 0; i < rt; ++i) {
            auto &event = mEventBuf[i];
            auto &promise = *(IoFilePromise *)mEventBuf[i].data.ptr;
            //checkError(epoll_ctl(mEpfd, EPOLL_CTL_DEL, promise.mFd, nullptr));
            // 下面promise 对应的协程句柄执行完就会析构掉promise,也就会析构掉事件
            // 这个promise就剩下一个省略的co_return了
            // std::coroutine_handle<IoFilePromise>::from_promise(promise).resume();
            // 先开香槟,后触发,再返回
            promise.mAwaiter->mResumeEvents = event.events;
        }

        // 所有promise已就绪
        for (int i = 0; i < rt; ++i) {
            auto &event = mEventBuf[i];
            auto &promise = *(IoFilePromise *)event.data.ptr;
            std::coroutine_handle<IoFilePromise>::from_promise(promise).resume();
        }
    }
    return true;
}


// wait_file 调用成功之后返回 触发了哪些事件,所以类型为 uint32_t
Task<IoEventMask, IoFilePromise>
wait_file_event(IoLoop &loop, AsyncFile &file, IoEventMask events) {
    uint32_t resumeEvents = co_await IoFileAwaiter(loop, file, events | EPOLLONESHOT);
    co_return resumeEvents;
}

std::size_t readFileSync(AsyncFile &file, std::span<char> buffer) {
    // span == ary + size()
    return checkError(read(file.fileNo(), buffer.data(), buffer.size()));
}

std::size_t writeFileSync(AsyncFile &file, std::span<char const> buffer) {
    return checkError(write(file.fileNo(), buffer.data(), buffer.size()));
}

Task<std::size_t> read_file(IoLoop &loop, AsyncFile &file, std::span<char> buffer) {
    co_await wait_file_event(loop, file, EPOLLIN | EPOLLRDHUP); //LT
    // readFileSync是普通函数,非异步
    // 这里是LT模式,因为buffer不是无限大,如果有数据就会触发,再到下面来读取数据
    auto len = readFileSync(file, buffer);
    co_return len;
}

Task<std::size_t> write_file(IoLoop &loop, AsyncFile &file, std::span<char const> buffer) {
    co_await wait_file_event(loop, file, EPOLLOUT | EPOLLHUP);
    auto len = writeFileSync(file, buffer);
    co_return len;
}

inline
Task<std::string> read_string(IoLoop &loop, AsyncFile &file) {
    // 如果是一次性读完缓冲区中所有数据那么,就是用ET模式更高效
    uint32_t triggeredEvent = co_await wait_file_event(loop, file, EPOLLIN | EPOLLET);
    if (triggeredEvent == EPOLLIN) PRINT_S(triggeredEpollIn);
    std::string s; // 基于MSVC对string进行优化
    ssize_t chunk = 15;
    while (1) {
        size_t exist = s.size();
        s.resize(exist + chunk);
        ssize_t len = read(0, s.data() + exist, chunk);
        if (len == -1) {
            if (errno != EAGAIN) [[unlikely]] 
                throw std::system_error(errno, std::system_category());
            break;
        }
        // 说明所有的都读完了
        if (len != chunk) {
            s.resize(exist + len);
            break;
        }
        if (chunk < 65536) {//64K
            chunk *= 3;
        }
    }
    co_return s;
}

}