//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_REBIND_EXECUTOR_HPP
#define BOOST_MQTT5_REBIND_EXECUTOR_HPP

namespace boost::asio::ssl {

// forward declare to preserve optional OpenSSL dependency
template <typename Stream>
class stream;

} // end namespace boost::asio::ssl

// forward declare to avoid Beast dependency

namespace boost::beast::websocket {

template <typename Stream, bool deflate_supported>
class stream;

}// end namespace boost::beast::websocket

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <typename Stream, typename Executor>
struct rebind_executor {
    using other = typename Stream::template rebind_executor<Executor>::other;
};

// asio::ssl::stream does not define a rebind_executor member type
template <typename Stream, typename Executor>
struct rebind_executor<asio::ssl::stream<Stream>, Executor> {
    using other = typename asio::ssl::stream<
        typename rebind_executor<Stream, Executor>::other
    >;
};

template <typename Stream, bool deflate_supported, typename Executor>
struct rebind_executor<
    boost::beast::websocket::stream<asio::ssl::stream<Stream>, deflate_supported>,
    Executor
> {
    using other = typename boost::beast::websocket::stream<
        asio::ssl::stream<typename rebind_executor<Stream, Executor>::other>,
        deflate_supported
    >;
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_REBIND_EXECUTOR_HPP
