/**
 * @file non_void_helper.hpp
 * @author qc
 * @brief void_helper
 * @version 0.1
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <utility>

namespace co_async {

// 注意,这里的class T = void 并非特化 而是在 NonVoidHelper<> nvh; 中直接NonVoidHelper<void> nvh;
template <class T = void>
struct NonVoidHelper {
    using Type = T;
};

// 这个才是特化
template<>
struct NonVoidHelper<void> {
    using Type = NonVoidHelper;

    explicit NonVoidHelper() = default;

    template <class T>
    constexpr friend T&&operator , (T &&t, NonVoidHelper) {
        return std::forward<T>(t);
    }

    char const *repr() const noexcept {
        return "NonVoidHelper";
    }
};



}
