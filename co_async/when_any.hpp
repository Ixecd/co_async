#pragma once

#include <coroutine>
#include <span>
#include <tuple>
#include <variant> // 多选一 不能是void
#include <type_traits>
#include "task.hpp"
#include "concepts.hpp"
#include "uninitialized.hpp"
#include "return_previous.hpp"

using namespace std::chrono_literals;

namespace co_async {

struct WhenAnyCtlBlock {
    static constexpr std::size_t kNullIndex = std::size_t(-1);

    std::size_t mIndex{kNullIndex};
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};



template <std::size_t... Is, Awaiter... Ts>
Task<std::variant<typename AwaitableTraits<Ts>::NonVoidRetType...>> whenAnyImpl(std::index_sequence<Is...>, Ts &&... ts) {
    // CtlBlock
    WhenAnyCtlBlock control{};
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
}


// when_any 协程实现 本质上是Task + tuple
template <Awaiter... Ts>
    requires(sizeof...(Ts) != 0)
auto when_any(Ts &&... ts) {
    return whenAnyImpl(std::make_index_sequence<sizeof...(ts)>{}, std::forward<Ts>(ts)...);
}


}