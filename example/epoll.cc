#include <utilities/qc.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <co_async/task.hpp>
#include <co_async/ioLoop.hpp>


int origin() {
    int epfd = epoll_create1(0);
    // int flag = fcntl(0, F_GETFL);
    // flag |= O_NONBLOCK;
    // fcntl(0, F_SETFL);
    int attr = 1; // Good!!
    ioctl(0, FIONBIO);


    struct epoll_event epevent;
    epevent.events = EPOLLIN;
    epevent.data.fd = 0;
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &epevent);

    while (true) {
        struct epoll_event ebuf[10];
        // 这里如果epoll超时了不会再输出下面的After1sNoEvent
        int rt = epoll_wait(epfd, ebuf, 10, 1000);
        if (rt < 0) {
            PRINT(rt);
            break;
        }
        if (rt == 0) {
            PRINT_S(After1sNoEvent);
        }
        for (int i = 0; i < rt; ++i) {
            PRINT_S(GetEvent!);
            int fd = ebuf[i].data.fd;
            char c;
            while (1) {
                PRINT_S(INLOOP!!!);
                int len = read(fd, &c, 1);
                if (len <= 0) { // len 永远不会等于0 因为有结束标志
                // EAGAIN == EWOULDBLOCK
                    if (errno == EAGAIN) {
                        PRINT_S(没东西可读喽~);
                        break;
                    }
                    PRINT(len);
                    PRINT_S(readError);
                    break;
                }
                PRINT(c);
            }
        }
    }
    return 0;
}




int main() {


    return 0;
}