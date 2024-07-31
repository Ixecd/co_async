#include "task.hpp"
#include "ioLoop.hpp"
#include <sys/socket.h>
#include <netinet/in.h> // 具体类型sockaddr
#include <sys/un.h> // 通过path代表地址
#include <chrono>

using namespace std::chrono_literals;
using namespace co_async;

// 互联网ipv4地址其实是一个32位无符号uint类型 ipv6->128位
// 进程间通信需要一个path name, 其也是基于TCP/UDP AF_LOCAL 
int test() {
    // AF_LOCAL == AF_UNIX --> 本地通信 sockaddr_un
    // SOCK_STREAM 面向有连接 流
    // SOCK_DGRAM 面向无连接 包
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr addr; //相当于一个纯虚基类 之后要类型转换为这个
    struct sockaddr_in iaddr; // 代表一个ipv4地址

    iaddr.sin_addr = {};

    struct sockaddr_in6 iaddr6; // 代表一个ipv6地址
    checkError(connect(sock, (const sockaddr *)&iaddr, sizeof(iaddr)));

    return 0;
}