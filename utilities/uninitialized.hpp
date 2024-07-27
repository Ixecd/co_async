/**
 * @file uninitialized.hpp
 * @author qc
 * @brief 未初始化类定义
 * @version 0.1
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include <utility> // for as_const
#include <memory>
#include <functional> // for std::reference_wrapper
#include "non_void_helper.hpp"

namespace co_async {

template <class T>
struct Uninitialized {
    union {
        T mValue;
    };

    Uninitialized() noexcept {}

    Uninitialized(Uninitialized &&) = delete;

    ~Uninitialized() {}

    T moveValue() {
        T ret(std::move(mValue)); // 移动拷贝一次,返回的是value,因为ret是局部变量要保证生命周期,所以外部还有一次拷贝赋值或者拷贝构造
        std::destroy_at(&mValue);
        return ret;
    }

    // 使用emplace直接给mValue初始化, placement new
    template <class... Ts>
    void putValue(Ts &&... value_args) {
        std::construct_at(&mValue, std::forward<Ts>(value_args)...);
    }

};

// void特化版本,如果value的类型为void 调用moveValue 就会返回一个NonVoidHelper对象
// putValue只会接受一个NonVoidHelper对象
template <>
struct Uninitialized<void> {
    auto moveValue() {
        return NonVoidHelper<>{};
    }

    void putValue(NonVoidHelper<>) {}
};

template <class T>
struct Uninitialized<T const> : Uninitialized<T> {};

// std::reference_wrapper<T> 外部传进来的value作为引用
// 不能在 list 上用 shuffle （要求随机访问），但能在 vector 上使用它
// std::suffle(Con.begin(), Con.end(), std::mt19937{std::random_device{}()}) 重排序给定范围 [first, last) 中的元素，使得这些元素的每个排列拥有相等的出现概率。
// Q:给你一个链表,里面有很多互不相同的数字,如何将这些数字打乱顺序?
// A:使用初始化列表将list中的元素全部给vector以引用的方式std::vector<std::reference_wrapper<int>>, 之后对这个vector进行suffle即可输出,之后操作list,也会修改vector中的值.
template <class T>
struct Uninitialized<T &> : Uninitialized<std::reference_wrapper<T>> {};

template <class T>
struct Uninitialized<T &&> : Uninitialized<T> {};

}