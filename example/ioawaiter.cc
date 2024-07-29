#include <co_async/task.hpp>
#include <co_async/ioLoop.hpp>
#include <utilities/qc.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/epoll.h>

using namespace co_async;
using namespace std::chrono_literals;

IoLoop loop;

Task<std::string> reader() {
    // 这里就相当于添加了一次事件,这行代码执行完,promise就寄了,直接从epoll中删除事件
    co_await wait_file(loop, 0, EPOLLIN);
    std::string s;
    while (1) {
        char c;
        int len = read(0, &c, 1);
        if (len == -1) {
            if (errno != EAGAIN) [[unlikely]] {
                throw std::system_error(errno, std::system_category());
            }
            // 读完了
            break;
        }
        s.push_back(c);
    }
    co_return s;
}

Task<void> async_main() {
    while (1) {
        // 这个co_await 是 Task中自定义的
        // 先创建一个协会实例Promise,之后再co_awiat
        auto s = co_await reader();
        PRINT(s);
        if (s == "q\n") break;
    }
}
// 编写协程函数 --> 生成Promsie对象 --> 要么手动resume --> 

int main() {

    int attr = 1;
    ioctl(0, FIONBIO, &attr);

    int var = 100;

    PRINT(var);

    auto t = async_main();
    t.mCoroutine.resume();
    while (!t.mCoroutine.done()) loop.tryRun();

    return 0;
}