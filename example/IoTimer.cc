#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <system_error>
#include <cerrno>
#include <chrono>
#include <tuple>
#include <variant>
#include <coroutine>
#include <utilities/qc.hpp>
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>
#include <co_async/task.hpp>
#include <co_async/and_then.hpp>
#include <co_async/when_any.hpp>
#include <co_async/when_all.hpp>
#include <co_async/concepts.hpp>
#include <co_async/scheduler.hpp>
#include <co_async/timerLoop.hpp>
#include <co_async/ioLoop.hpp>
#include <co_async/return_previous.hpp>
#include <co_async/previous_awaiter.hpp>

using namespace co_async;
using namespace std::chrono_literals;

IoLoop epollLoop;
TimerLoop timerLoop;

Task<std::string> reader() {
    auto which = co_await when_any(wait_file(epollLoop, 0, EPOLLIN), sleep_for(timerLoop, 1s));
    if (which.index() == 1) co_return "超过1s无任何输入";
    // 执行完32行,Promise还在,因为要等事件触发,只有之间触发之后,才会调用Promise的resume(),resume完就析构了
    std::string s;
    // 下面是对缓冲区的优化, 基于MSVC
    size_t chunk = 15;
    while (1) {
        size_t exist = s.size();
        s.resize(exist + chunk);
        ssize_t len = read(0, s.data() + exist, chunk);
        if (len == -1) {
            if (errno != EAGAIN) [[unlikely]] 
                throw std::system_error(errno, std::system_category());
            break;
        }
        if (len != chunk) {
            s.resize(exist + len);
            break;
        }
        if (chunk < 65536) {
            chunk *= 3;
        }
    }
    co_return s;
}

Task<void> async_main() {
    while(1) {
        auto s = co_await reader();
        PRINT(s);
        if (s == "quit\n") break;
    }
}

int main() {
    int attr = 1;
    ioctl(0, FIONBIO, &attr);

    auto t = async_main();
    t.mCoroutine.resume();
    while (!t.mCoroutine.done()) {
        if (auto delay = timerLoop.run()) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(*delay).count();
            PRINT(ms);
            epollLoop.tryRun(ms);
        } else {
            epollLoop.tryRun(-1);
        }
    }


    return 0;
}