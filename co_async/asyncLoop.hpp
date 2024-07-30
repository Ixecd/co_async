/**
 * @file asyncLoop.hpp
 * @author qc
 * @brief 将定时器循环和io事件循环结合起来
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "ioLoop.hpp"
#include "timerLoop.hpp"
#include <thread>

namespace co_async {

struct AsyncLoop {

    void process() {
        while (1) {
            auto timeout = mTimerLoop.getNext();
            if (mIoLoop.hasEvent()) 
                mIoLoop.tryRun(timeout);
            else if (timeout) {
                std::this_thread::sleep_for(*timeout);
            } else break;
        }
    }

    operator TimerLoop &() {
        return mTimerLoop;
    }

    operator IoLoop &() {
        return mIoLoop;
    }

private:
    IoLoop mIoLoop;
    TimerLoop mTimerLoop;
};


}