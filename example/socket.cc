#include <arpa/inet.h> // for inet_pton
#include <chrono>
#include <co_async/ioLoop.hpp>
#include <co_async/task.hpp>
#include <netdb.h>      // 通过域名获取ip
#include <netinet/in.h> // 具体类型sockaddr
#include <sys/socket.h>
#include <sys/un.h>     // 通过path代表地址
#include <utilities/qc.hpp>

using namespace std::chrono_literals;
using namespace co_async;

// 互联网ipv4地址其实是一个32位无符号uint类型 ipv6->128位
// 进程间通信需要一个path name, 其也是基于TCP/UDP AF_LOCAL
int main() {
    // AF_LOCAL == AF_UNIX --> 本地通信 sockaddr_un
    // SOCK_STREAM 面向有连接 流
    // SOCK_DGRAM 面向无连接 包
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // struct sockaddr addr; //相当于一个纯虚基类 之后要类型转换为这个
    struct sockaddr_in iaddr; // 代表一个ipv4地址

    char const *dns = "142857.red";

    // 如果第二个参数是一个域名,就会返回0, hostent *hent = gethostbyname(DNS);
    // hent->h_name, hent->m_length, ... memcpy(&addr.sin_addr,
    // hent->h_addr_list[0], sizeof(iaddr.sin_addr)); ipv6 gethostbyname2(DSN,
    // AF_INET6) 小端0x01..7F, ip地址是以小端存储
    if (checkError(inet_pton(AF_INET, dns, &iaddr.sin_addr)) == 0) {
        hostent *hent = gethostbyname2(dns, AF_INET);
        memcpy(&iaddr, hent->h_addr_list[0], sizeof(iaddr.sin_addr));
    }
    // iaddr.sin_addr = { 127 << 24 | 1};
    iaddr.sin_port =
        htons(8080); // port用大端存, x86上字节序是小端,所以port要转换为大端,
                     // ip地址是一个例外
    // 规定网络相关的用大端来存储, 大端更适合人类理解
    // int port = ntohs(iaddr.sin_port);
    iaddr.sin_family = AF_INET;

    // struct sockaddr_in6 iaddr6; // 代表一个ipv6地址
    PRINT_S(beginConn);
    checkError(connect(sock, (sockaddr const *)&iaddr, sizeof(iaddr)));
    PRINT_S(connectSucc);

    return 0;
}
