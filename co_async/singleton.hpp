/**
 * @file singleton.hpp
 * @author qc
 * @brief 
 * @version 0.1
 * @date 2024-07-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once;
#include <memory>

template <class T, class X = void, int N = 1>
class Singleton {
public:
    static T* getInstance() {
        static T t;
        return &t;
    }

private:
    Singleton() {}
    ~Singleton() {}

    Singleton& = (Singleton &&) = delete;
};

template <class T, class X = void, int N = 1>
class SingletonPtr {
public:
    static std::shared_ptr<T> getInstance() {
        static std::shared_ptr<T> t(new T);
        return t;
    }
private:
    SingletonPtr() {}
    ~SingletonPtr() {}

    SingletonPtr& = (SingletonPtr &&) = delete;
};