/**
 * @file stream_base.hpp
 * @author qc
 * @brief 对输入流和输出流添加缓冲区
 * @version 0.1
 * @date 2024-07-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <memory>
#include "task.hpp"

namespace co_async {

struct EOFException {};

/// @brief 输入流
template <class Reader>
struct IStreamBase {

    explicit IStreamBase(std::size_t bufferSize = 8192) : mBuffer(std::make_unique<char[]>(bufferSize)), mBufSize(bufferSize) { }

    IStreamBase(IStreamBase &&) = default;
    IStreamBase &operator = (IStreamBase &&) = default;

    Task<char> getChar() {
        if (bufferEmpty()) {
            co_await fillBuffer();
        }
        char c = mBuffer[mIndex++];
        co_return c;
    }

    Task<std::string> getLine(char eol = '\n') {
        std::string s;
        while (true) {
            char c = co_await getChar();
            if (c == eol) break;
            s.push_back(c);
        }
        co_return s;
    }

    Task<std::string> getLine(std::string_view eol) {
        std::string s;
        while (true) {
            char c = co_await getchar();
            if (c == eol[0]) {
                std::size_t i;
                for (i = 1; i < eol.size(); ++i) {
                    char c = co_await getchar();
                    if (c != eol[i]) {
                        break;
                    }
                }
                if (i == eol.size()) {
                    break;
                }
                for (std::size_t j = 0; j < i; ++j) {
                    s.push_back(eol[j]);
                }
                continue;
            }
            s.push_back(c);
        }
        co_return s;
    }

    Task<std::string> getN(std::size_t n) {
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            char c = co_await getChar();
            s.push_back(c);
        }
        co_return s;
    }

    bool bufferEmpty() const noexcept {
        return mIndex == mEnd;
    }

    Task<void> fillBuffer() {
        auto *that = static_cast<Reader *>(this);
        mIndex = 0;
        mEnd = co_await that->read(std::span(mBuffer.get(), mBufSize));
        if (mEnd == 0) [[unlikely]] 
            throw EOFException();
    }

private:
    std::unique_ptr<char[]> mBuffer;
    std::size_t mIndex = 0;
    std::size_t mEnd = 0;
    std::size_t mBufSize = 0;
};

/// @brief 输出流
template <class Writer>
struct OStreamBase {

    explicit OStreamBase(size_t bufferSize = 8192) : mBuffer(std::make_unique<char[]>(bufferSize)), mBufSize(bufferSize) {}

    OStreamBase(OStreamBase &&) = default;
    OStreamBase& operator = (OStreamBase &&) = default;

    Task<void> putChar(char c) {
        if (bufferFull()) 
            co_await flush();
        mBuffer[mIndex++] = c;
    }

    Task<void> puts(std::string_view s) {
        for (char c : s) 
            co_await putChar(c);
    }

    bool bufferFull() const noexcept {
        return mIndex = mBufSize;
    }

    Task<void> flush() {
        if (mIndex) [[likely]] {
            auto *that = static_cast<Writer*>(this);
            auto buf = std::span(mBuffer.get(), mIndex);
            auto len = co_await that->write(buf);
            while (len != buf.size()) [[unlikely]] {
                buf = buf.subspan(len);
                len = co_await that->write(buf);
            }
            if (len == 0) [[unlikely]] 
                throw EOFException();
            mIndex = 0;
        }
    }

private:
    std::unique_ptr<char[]> mBuffer;
    size_t mIndex = 0;
    size_t mEnd = 0;
    size_t mBufSize = 0;
};


template <class StreamBuf>
struct IOStreamBase : IStreamBase<StreamBuf>, OStreamBase<StreamBuf> {
    explicit IOStreamBase(size_t bufferSize = 8192) : IStreamBase<StreamBuf>(bufferSize), OStreamBase<StreamBuf>(bufferSize) {}
};

/**
 * @brief IOStream
 * 
 * @tparam StreamBuf IOStream<StreamBuf>其作为一个class里面必须有协程函数read和write,里面调用read_file和write_file协程向epoll中添加事件
 *         static_cast
 */
template <class StreamBuf>
struct [[nodiscard]] IOStream : IOStreamBase<IOStream<StreamBuf>>, StreamBuf {
    template <class... Args>
        requires std::constructible_from<StreamBuf, Args...>
    explicit IOStream(Args &&...args)
        : IOStreamBase<IOStream<StreamBuf>>(),
          StreamBuf(std::forward<Args>(args)...) {}

    IOStream() = default;
};

template <class StreamBuf>
struct [[nodiscard]] OStream : OStreamBase<OStream<StreamBuf>>, StreamBuf {
    template <class... Args>
        requires std::constructible_from<StreamBuf, Args...>
    explicit OStream(Args &&...args)
        : OStreamBase<OStream<StreamBuf>>(),
          StreamBuf(std::forward<Args>(args)...) {}

    OStream() = default;
};

template <class StreamBuf>
struct [[nodiscard]] IStream : IStreamBase<OStream<StreamBuf>>, StreamBuf {
    template <class... Args>
        requires std::constructible_from<StreamBuf, Args...>
    explicit IStream(Args &&...args)
        : IStreamBase<IStream<StreamBuf>>(),
          StreamBuf(std::forward<Args>(args)...) {}

    IStream() = default;
};

}