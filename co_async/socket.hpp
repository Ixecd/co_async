/**
 * @file socket.hpp
 * @author qc
 * @brief 封装套接字,为了解决大小端问题
 * @version 0.1
 * @date 2024-08-02
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include "task.hpp"
#include "ioLoop.hpp"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h> // 具体类型sockaddr
#include <sys/un.h> // 通过path代表地址
#include <arpa/inet.h> // for inet_pton
#include <chrono>
#include <netdb.h> // 通过域名获取ip

using namespace std::chrono_literals;


namespace co_async {

// DEBUG 版本
#if !defined(NDEBUG)
auto chechErrorNonBlock(auto res, int blockres = 0, int blockerr = EAGAIN, std::source_location const& loc = std::source_location::current()) {
    if (res == -1) {
        if (errno != blockerr) [[unlikely]] {
            throw std::system_error(errno, std::system_category(), (std::string)loc.file_name() + ":" + std::to_string(loc.line()));
        }
        res = blockres;
    }
    return res;
}
#else
// 非DEBUG 版本
auto chechErrorNonBlock(auto res, int blockres = 0, int blockerr = EAGAIN) {
    if (res == -1) {
        if (errno != blockerr) [[unlikely]] {
            throw std::system_error(errno, std::system_category());
        }
        res = blockres;
    }
    return res;
}
#endif

struct IpAddress {
    IpAddress() noexcept { }

    IpAddress(in_addr addr) noexcept : mAddr(addr) {}

    IpAddress(in6_addr addr6) noexcept : mAddr(addr6) {}

    std::variant<in_addr, in6_addr> mAddr{};
};

inline
IpAddress ip_address(char const* ip) {
    in_addr addr = {};
    in6_addr addr6 = {};
    /**
     * inet_pton(int af, const char* restrict src, void *restrict dst);
     * succ : 1
     * src mismatch to af : 0
     * af is not one of AF_INET/AF_INET6 : -1
     */
    if (checkError(inet_pton(AF_INET, ip, &addr))) {
        return addr;
    }
    if (checkError(inet_pton(AF_INET6, ip, &addr6))) {
        return addr6;
    }
    // dns, 一个ip可能对应多个ip
    hostent *hent = gethostbyname(ip);
    for (int i = 0; hent->h_addr_list[i]; ++i) {
        if (hent->h_addrtype == AF_INET) {
            std::memcpy(&addr, hent->h_addr_list[i], sizeof(addr));
            return addr;
        } else if (hent->h_addrtype == AF_INET6) {
            std::memcpy(&addr, hent->h_addr_list[i], sizeof(addr6));
            return addr6;
        }
    }
    // 既不是ipv4也不是ipv6还不是dns域名,直接throw
    throw std::invalid_argument("invalid domain name or ip address");

}

struct SocketAddress {
    SocketAddress() = default;
    // 用于本机之间进程通信
    SocketAddress(char const* path) {
        sockaddr_un saddr = {};
        saddr.sun_family = AF_UNIX;
        std::strncpy(saddr.sun_path, path, sizeof(saddr.sun_path) - 1);
        std::memcpy(&mAddr, &saddr, sizeof(saddr));
        mAddrLen = sizeof(saddr);
    }

    SocketAddress(in_addr host, int port) {
        sockaddr_in saddr = {};
        saddr.sin_family = AF_INET;
        std::memcpy(&saddr.sin_addr, &host, sizeof(saddr.sin_addr));
        saddr.sin_port = htons(port);
        std::memcpy(&mAddr, &saddr, sizeof(saddr));
        mAddrLen = sizeof(saddr);
    }

    SocketAddress(in6_addr host, int port) {
        sockaddr_in6 saddr = {};
        saddr.sin6_family = AF_INET6;
        std::memcpy(&saddr.sin6_addr, &host, sizeof(saddr.sin6_addr));
        saddr.sin6_port = htons(port);
        std::memcpy(&mAddr, &saddr, sizeof(saddr));
        mAddrLen = sizeof(saddr);
    }
    // 只是作为字符串保存下来,后面再到外面用的时候类型转换一下就行
    sockaddr_storage mAddr;
    socklen_t mAddrLen;
};


inline
AsyncFile create_udp_socket(SocketAddress const& addr) {
    AsyncFile sock(socket(addr.mAddr.ss_family, SOCK_DGRAM, 0));
    return sock;
}

inline
SocketAddress socket_address(IpAddress ip, int port) {
    return std::visit([&](auto const& addr) { return SocketAddress(addr, port); }, ip.mAddr);
}

/// @brief 通过socket得到ip地址
inline
SocketAddress socketGetAddress(AsyncFile &sock) {
    SocketAddress sa;
    sa.mAddrLen = sizeof(sa.mAddr);
    // sys/socket.h 
    checkError(getsockname(sock.fileNo(), (sockaddr *)&sa.mAddr, &sa.mAddrLen));
    return sa;
}

template <class T>
inline T socketGetOption(AsyncFile &sock, int level, int optId) {
    T optVal;
    socklen_t optLen = sizeof(optVal);
    checkError(getsockopt(sock.fileNo(), level, optId, (sockaddr*)&optVal, &optLen));
    return optVal;
}

// 最常用的就是socket bind的时候设置SO_REUSEADDR属性
template <class T>
inline void socketSetOption(AsyncFile &sock, int level, int opt, T const &optVal) {
    checkError(setsockopt(sock.fileNo(), level, opt, &optVal, sizeof(optVal)));
}

inline Task<void> socketConnect(IoLoop &loop, AsyncFile &sock, SocketAddress const& addr) {
    sock.setNonblock();
    int res = chechErrorNonBlock(connect(sock.fileNo(), (sockaddr *)&addr.mAddr, addr.mAddrLen), -1, EINPROGRESS);

    // -1 并且errno 为 EINPROGRESS表示正在连接,添加写事件,等待触发,之后判断
    if (res == -1) [[likely]] {
        co_await wait_file_event(loop, sock, EPOLLOUT);
        // 检测SO_ERROR 如果没有错误就返回0
        int err = socketGetOption<int>(sock, SOL_SOCKET, SO_ERROR);
        if (err != 0) [[unlikely]] {
            throw std::system_error(err, std::system_category(), "connect");
        }
    }
}

inline
Task<AsyncFile> create_tcp_client(IoLoop &loop, SocketAddress const& addr) {
    AsyncFile sock(socket(addr.mAddr.ss_family, SOCK_STREAM, 0));
    co_await socketConnect(loop, sock, addr);
    co_return sock; // return_val中以val的方式传递,或者右值引用
}

inline
void socket_listen(AsyncFile &sock, int backlog = SOMAXCONN) {
    checkError(listen(sock.fileNo(), backlog));
}

// 
inline
Task<void> socketBind(IoLoop &loop, AsyncFile &sock, SocketAddress const& addr, int backlog = SOMAXCONN) {
    sock.setNonblock();
    // 一般绑定的时候不判断EINPROGRESS
    checkError(bind(sock.fileNo(), (sockaddr const*)&addr.mAddr, addr.mAddrLen));
    PRINT_S(绑定成功);
    
    // co_await wait_file_event(loop, sock, EPOLLOUT);
    // int err = socketGetOption<int>(sock, SOL_SOCKET, SO_ERROR);
    // if (err != 0) [[unlikely]] {
    //     throw std::system_error(err, std::system_category(), "bind");
    // }
    // 设置非阻塞
    int optVal = 0;
    socketSetOption<int>(sock, SOL_SOCKET, SO_REUSEADDR, optVal);
    
    PRINT_S(开始监听);
    socket_listen(sock, backlog);

    // co_await wait_file_event(loop, sock, EPOLLIN);

    co_return;
}

inline
Task<AsyncFile> create_tcp_server(IoLoop &loop, SocketAddress const& addr) {
    AsyncFile sock(socket(addr.mAddr.ss_family, SOCK_STREAM, 0));
    co_await socketBind(loop, sock, addr);
    co_return sock;
}


inline
void socket_shutdown(AsyncFile &sock, int flags = SHUT_RDWR) {
    checkError(shutdown(sock.fileNo(), flags));
}

template <class AddrType>
inline Task<std::tuple<AsyncFile, AddrType>> socket_accept(IoLoop &loop, AsyncFile &sock) {
    AddrType addr;
    socklen_t addrLen = sizeof(addr.mAddrLen);
    // 这里是触发了事件才返回的,要不然就一直阻塞, 这行的下一行才触发任务的,怎么返回??
    // co_await wait_file_event(loop, sock, EPOLLIN);
    int rt = checkError(accept(sock.fileNo(), (sockaddr *)&addr.mAddr, &addrLen));

    co_return {AsyncFile(rt), addr};
}

}
