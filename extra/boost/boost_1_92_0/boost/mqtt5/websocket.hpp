//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_WEBSOCKET_HPP
#define BOOST_MQTT5_WEBSOCKET_HPP

#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/shutdown.hpp>

#include <boost/mqtt5/types.hpp>

#include <boost/beast/http/field.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace boost::mqtt5 {

// Trait definition for Beast
template <typename Stream>
struct ws_handshake_traits<boost::beast::websocket::stream<Stream>> {

    template <typename CompletionToken>
    static decltype(auto) async_handshake(
        boost::beast::websocket::stream<Stream>& stream,
        authority_path ap, CompletionToken&& token
    ) {
        using namespace boost::beast;

        // Set suggested timeout settings for the websocket
        stream.set_option(
            websocket::stream_base::timeout::suggested(role_type::client)
        );

        stream.binary(true);

        // Set a decorator to change the User-Agent of the handshake
        stream.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::sec_websocket_protocol, "mqtt");
                req.set(http::field::user_agent, "boost.mqtt");
            })
        );

        stream.async_handshake(
            ap.host + ':' + ap.port, ap.path,
            std::forward<CompletionToken>(token)
        );
    }
};

namespace detail {

// in namespace boost::mqtt5::detail to enable ADL
template <typename Stream, typename ShutdownHandler>
void async_shutdown(
    boost::beast::websocket::stream<Stream>& stream, ShutdownHandler&& handler
) {
    stream.async_close(
        beast::websocket::close_code::normal,
        std::move(handler)
    );
}

} // end namespace detail

} // end namespace boost::mqtt5

#endif // !BOOST_MQTT5_WEBSOCKET_HPP
