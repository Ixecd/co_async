/**
 * @file concepts.hpp
 * @author qc
 * @brief 概念(Python中称为鸭子类)
 * @details 有一群类,它们里面含有一些相同的函数,可以将这些函数提取出来构建成概念,之后可以将这些类转交给这个概念来操作.使用概念可以
 *          进行模板实参的编译时校验,以及基于类型属性的函数派发.编译时求值.
 * @version 0.1
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <coroutine>
#include <type_traits>
#include <concepts>
#include <utility> // for declval
#include <utilities/uninitialized.hpp>
#include <utilities/non_void_helper.hpp>

namespace co_async {

// Awaiter是一个concept,里面必然含有下面的三个成员函数
// requires表约束
// 下面这是一个概念的声明,要求 Awaiter中必须有一个class a 和一个 协程句柄, class a中必须有下面三个函数,并且中间那个参数是协程句柄
// Awaiter等待者
template <class A>
concept Awaiter = requires(A a, std::coroutine_handle<> h) {
    { a.await_ready() };
    { a.await_suspend(h) };
    { a.await_resume() };
};

// Awaitable可等待的,只要Awaiter或者A中有重载co_await并且返回类型为Awaiter即表示时可等待的这个概念
template <class A>
concept Awaitable = Awaiter<A> || requires(A a) {
    {a.operator co_await() } -> Awaiter;
};

// 对可等待的类型萃取
template <class A>
struct AwaitableTraits {
    using Type = A;
};

// Awaiter类型萃取
template <Awaiter A>
struct AwaitableTraits<A> {
    //将任意类型 A 转换成引用类型，在 decltype 表达式中不必经过构造函数就能使用成员函数(如果默认构造函数删除了的话就必须用这个)。
    using RetType = decltype(std::declval<A>().await_resume());
    // 如果await_resume返回类型为void,下面就要将 NonVoidRetType = NonVoidHelper;
    using NonVoidRetType = NonVoidHelper<RetType>::Type;
    using Type = RetType;
    using AwaiterType = A;
};

// ---------------------------------------------------------------------------
template <class A>
    requires(!Awaiter<A> && Awaitable<A>)
struct AwaitableTraits<A>
    : AwaitableTraits<decltype(std::declval<A>().operator co_await())> {};

template <class... Ts>
struct TypeList {};

template <class Last>
struct TypeList<Last> {
    using FirstType = Last;
    using LastType = Last;
};

template <class First, class... Ts>
struct TypeList<First, Ts...> {
    using FirstType = First;
    using LastType = typename TypeList<Ts...>::LastType;
};

}
