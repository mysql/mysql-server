//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_LOGGER_TRAITS_HPP
#define BOOST_MQTT5_LOGGER_TRAITS_HPP

#include <boost/mqtt5/property_types.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_traits/is_detected.hpp>

#include <iostream>
#include <string_view>
#include <type_traits>

namespace boost::mqtt5 {

namespace asio = boost::asio;
using boost::system::error_code;

// NOOP Logger
class noop_logger {};

// at_resolve

template <typename T>
using at_resolve_sig = decltype(
    std::declval<T&>().at_resolve(
        std::declval<error_code>(),
        std::declval<std::string_view>(), std::declval<std::string_view>(),
        std::declval<const asio::ip::tcp::resolver::results_type&>()
    )
);
template <typename T>
constexpr bool has_at_resolve = boost::is_detected<at_resolve_sig, T>::value;

// at_tcp_connect

template <typename T>
using at_tcp_connect_sig = decltype(
    std::declval<T&>().at_tcp_connect(
        std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
    )
);
template <typename T>
constexpr bool has_at_tcp_connect = boost::is_detected<at_tcp_connect_sig, T>::value;

// at_tls_handshake

template <typename T>
using at_tls_handshake_sig = decltype(
    std::declval<T&>().at_tls_handshake(
        std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
    )
);
template <typename T>
constexpr bool has_at_tls_handshake = boost::is_detected<at_tls_handshake_sig, T>::value;

// at_ws_handshake

template <typename T>
using at_ws_handshake_sig = decltype(
    std::declval<T&>().at_ws_handshake(
        std::declval<error_code>(), std::declval<asio::ip::tcp::endpoint>()
    )
);
template <typename T>
constexpr bool has_at_ws_handshake = boost::is_detected<at_ws_handshake_sig, T>::value;

// at_connack

template <typename T>
using at_connack_sig = decltype(
    std::declval<T&>().at_connack(
        std::declval<reason_code>(),
        std::declval<bool>(), std::declval<const connack_props&>()
    )
);
template <typename T>
constexpr bool has_at_connack = boost::is_detected<at_connack_sig, T>::value;

// at_disconnect

template <typename T>
using at_disconnect_sig = decltype(
    std::declval<T&>().at_disconnect(
        std::declval<reason_code>(), std::declval<const disconnect_props&>()
    )
);
template <typename T>
constexpr bool has_at_disconnect = boost::is_detected<at_disconnect_sig, T>::value;

// at_transport_error

template <typename T>
using at_transport_error = decltype(
    std::declval<T&>().at_transport_error(std::declval<error_code>())
);
template <typename T>
constexpr bool has_at_transport_error = boost::is_detected<at_transport_error, T>::value;

} // end namespace boost::mqtt5


#endif // !BOOST_MQTT5_LOGGER_TRAITS_HPP
