//
// Copyright (c) 2025-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_SHUTDOWN_HPP
#define BOOST_MQTT5_SHUTDOWN_HPP

#include <boost/asio/basic_stream_socket.hpp>

namespace boost::mqtt5::detail {

template <typename Stream, typename ShutdownHandler>
void async_shutdown(Stream& /* stream */, ShutdownHandler&& /* handler */) {
/*
    If you are trying to use beast::websocket::stream and/or OpenSSL
    and this goes off, you need to add an include for one of these
        * <boost/mqtt5/websocket.hpp>
        * <boost/mqtt5/ssl.hpp>
        * <boost/mqtt5/websocket_ssl.hpp>

    If you are trying to use mqtt_client with user-defined stream type, you must
    provide an overload of async_shutdown that is discoverable via
    argument-dependent lookup (ADL).
*/
    static_assert(sizeof(Stream) == -1,
        "Unknown Stream type in async_shutdown.");
}

template <typename P, typename E, typename ShutdownHandler>
void async_shutdown(
    asio::basic_stream_socket<P, E>& socket, ShutdownHandler&& handler
) {
    boost::system::error_code ec;
    socket.shutdown(asio::socket_base::shutdown_both, ec);
    return std::move(handler)(ec);
}

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_SHUTDOWN_HPP
