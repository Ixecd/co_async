#pragma once

#include <unistd.h>
#include <termios.h>
#include "task.hpp"
#include "ioLoop.hpp"

namespace co_async {

inline AsyncFile asyncStdFile(int fileNo) {
    // 返回一个新的文件描述符（可用文件描述符的最小值）newfd，并且新的文件描述符newfd指向oldfd所指向的文件表项
    AsyncFile file(checkError(dup(fileNo)));
    file.setNonblock();
    return file;
}
// 所谓的异步stdin 和 stdout 就是将其封装成Asyncfile
inline AsyncFile async_stdin(bool noCanon = false, bool noEcho = false) {
    AsyncFile file = asyncStdFile(STDIN_FILENO);
    if ((noCanon || noEcho) && isatty(file.fileNo())) {
        struct termios tc;
        tcgetattr(file.fileNo(), &tc);
        if (noCanon) tc.c_lflag &= ~ICANON;
        if (noEcho) tc.c_lflag &= ~ECHO; // 不要回显
        tcsetattr(file.fileNo(), TCSANOW, &tc);
    }
    return file;
}

inline AsyncFile async_stdout() {
    return asyncStdFile(STDOUT_FILENO);
}

inline AsyncFile async_stderr() {
    return asyncStdFile(STDERR_FILENO);
}

}