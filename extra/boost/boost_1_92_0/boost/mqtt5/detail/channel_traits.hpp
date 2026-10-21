//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_CHANNEL_TRAITS_HPP
#define BOOST_MQTT5_CHANNEL_TRAITS_HPP

#include <boost/asio/error.hpp>

#include <deque>
#include <type_traits>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

template <typename Element>
class bounded_deque {
    std::deque<Element> _buffer;
    static constexpr size_t MAX_SIZE = 65535;

public:
    bounded_deque() = default;
    bounded_deque(size_t n) : _buffer(n) {}

    size_t size() const {
        return _buffer.size();
    }

    template <typename E>
    void push_back(E&& e) {
        if (_buffer.size() == MAX_SIZE)
            _buffer.pop_front();
        _buffer.push_back(std::forward<E>(e));
    }

    void pop_front() {
        _buffer.pop_front();
    }

    void clear() {
        _buffer.clear();
    }

    const auto& front() const noexcept {
        return _buffer.front();
    }

    auto& front() noexcept {
        return _buffer.front();
    }
};

template <typename... Signatures>
struct channel_traits {
    template <typename... NewSignatures>
    struct rebind {
        using other = channel_traits<NewSignatures...>;
    };
};

template <typename R, typename... Args>
struct channel_traits<R(error_code, Args...)> {
    static_assert(sizeof...(Args) > 0);

    template <typename... NewSignatures>
    struct rebind {
        using other = channel_traits<NewSignatures...>;
    };

    template <typename Element>
    struct container {
        using type = bounded_deque<Element>;
    };

    using receive_cancelled_signature = R(error_code, Args...);

    template <typename F>
    static void invoke_receive_cancelled(F f) {
        std::forward<F>(f)(
            asio::error::operation_aborted,
            typename std::decay_t<Args>()...
        );
    }

    using receive_closed_signature = R(error_code, Args...);

    template <typename F>
    static void invoke_receive_closed(F f) {
        std::forward<F>(f)(
            asio::error::operation_aborted,
            typename std::decay_t<Args>()...
        );
    }
};

} // namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_CHANNEL_TRAITS_HPP
