#pragma once

#include <set>
#include <coroutine>
#include <queue>
#include <vector>
#include <thread>
#include <utilities/qc.hpp>
#include <utilities/rbtree.hpp>
#include "task.hpp"
#include "timerLoop.hpp"


namespace co_async {

// 在异步中使用Loop(黑话)->scheduler
struct Loop {

    // struct TimerEntry {
    //     std::chrono::system_clock::time_point mExpireTime;
    //     std::coroutine_handle<> mCoroutine;

    //     TimerEntry(std::chrono::system_clock::time_point expireTime, std::coroutine_handle<> coroutine) : mExpireTime(expireTime), mCoroutine(coroutine) {}

    //     // 类中的Comparation必须是const属性
    //     bool operator< (TimerEntry const& that) const noexcept {
    //         // priority_queue默认大顶堆,优先级高的放前面
    //         // 如果 mExpireTimer < that.mExpireTime; 优先级是从小到大,所以mExpir大的在前面
    //         // 所以这里要 > 
    //         return mExpireTime > that.mExpireTime;
    //     }
    // };

    // RbTree<TimerEntry> mRbTimer{};

    // void addTimer(std::chrono::system_clock::time_point expireTime, std::coroutine_handle<> task) {
    //     mTimerQueue.push({expireTime, task});
    // }

    // void addTimer(TimerEntry &te) {
    //     mRbTimer.insert(te);
    // }


    // 在Task中必须可以类型转换为协程句柄
    void addTask(std::coroutine_handle<> task) {
        mReadyQueue.push_back(task);
    }

    void process() {
        while (!mReadyQueue.empty()) {
            while (!mReadyQueue.empty()) {
                // DEBUG(mReadyQueueisNotEmpty);
                // PRINT(mReadyQueue.size());
                auto t = mReadyQueue.front();
                mReadyQueue.pop_front();
                t.resume();
            }
        }
    }

    // void run(std::coroutine_handle<> coroutine) {
    //     while (!coroutine.done()) {
    //         coroutine.resume();
    //         while (!mRbTimer.empty()) {
    //             if (!mRbTimer.empty()) {
    //                 auto nowTime = std::chrono::system_clock::now();
    //                 auto &te = mRbTimer.front();
    //                 if (te.mExpireTime < nowTime) {
    //                     mRbTimer.erase(te);
    //                     te.mCoroutine.resume();
    //                 } else std::this_thread::sleep_until(te.mExpireTime);
    //             }
    //         }
    //     }
    // }

    std::deque<std::coroutine_handle<>> mReadyQueue;
    // when_any的时候定时器要从这里面删除掉
    // 优先队列只要头的话很高效,不能删除,所以改用set的weak版本->RbTree
    // std::priority_queue<TimerEntry> mTimerQueue;
    // std::set<int> mSet;

    // 单例要删除,下面这个直接构造函数,赋值函数都删了,只保留了默认构造
    Loop& operator=(Loop &&) = delete;

};
// static 保证只会构造一次,是多线程安全的
// static在cc文件中具有文件作用域
// static Loop* getLoop(); 使得每个包含这个头文件的源文件都有一份这个函数
// 函数中的static Loop loop; 生命周期为整个程序,作用域是getLoop()
// 懒汉式,只有调用这个函数的时候才会初始化,函数前面的static为了减小程序大小可以不加,要不然每个源文件都有一个这个函数
// 这里也不是成员函数,所以没必要在函数前面加上static
static Loop* getLoop() {
    static Loop loop;
    return &loop;
}


}
