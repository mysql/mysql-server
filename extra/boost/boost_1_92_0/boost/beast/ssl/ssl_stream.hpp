//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_SSL_STREAM_HPP
#define BOOST_BEAST_CORE_SSL_STREAM_HPP

#include <boost/beast/core/detail/config.hpp>

// This include is necessary to work with `ssl::stream` and `boost::beast::websocket::stream`
#include <boost/beast/websocket/ssl.hpp>

// VFALCO We include this because anyone who uses ssl will
//        very likely need to check for ssl::error::stream_truncated
#include <boost/asio/ssl/error.hpp>

#include <boost/asio/ssl/stream.hpp>

namespace boost {
namespace beast {

/** (Deprecated: Use asio::ssl::stream instead.) Provides stream-oriented functionality using OpenSSL
*/
template<class NextLayer>
struct ssl_stream : net::ssl::stream<NextLayer>
{
    using net::ssl::stream<NextLayer>::stream;
};

#if ! BOOST_BEAST_DOXYGEN
template<class SyncStream>
void
teardown(
    boost::beast::role_type role,
    ssl_stream<SyncStream>& stream,
    boost::system::error_code& ec)
{
    // Just forward it to the underlying ssl::stream
    using boost::beast::websocket::teardown;
    teardown(role, static_cast<net::ssl::stream<SyncStream>&>(stream), ec);
}

template<class AsyncStream,
        typename TeardownHandler = net::default_completion_token_t<beast::executor_type<AsyncStream>>>
void
async_teardown(
    boost::beast::role_type role,
    ssl_stream<AsyncStream>& stream,
    TeardownHandler&& handler = net::default_completion_token_t<beast::executor_type<AsyncStream>>{})
{
    // Just forward it to the underlying ssl::stream
    using boost::beast::websocket::async_teardown;
    async_teardown(role, static_cast<net::ssl::stream<AsyncStream>&>(stream),
        std::forward<TeardownHandler>(handler));
}
#endif

} // beast
} // boost

#endif
