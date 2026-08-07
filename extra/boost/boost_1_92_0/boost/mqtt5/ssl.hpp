//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_SSL_HPP
#define BOOST_MQTT5_SSL_HPP

#include <boost/mqtt5/detail/async_traits.hpp>
#include <boost/mqtt5/detail/shutdown.hpp>

#include <boost/mqtt5/types.hpp>

#include <boost/asio/ssl.hpp>

namespace boost::mqtt5 {

namespace detail {

// in namespace boost::mqtt5::detail to enable ADL
template <typename Stream, typename ShutdownHandler>
void async_shutdown(
    boost::asio::ssl::stream<Stream>& stream, ShutdownHandler&& handler
) {
    stream.async_shutdown(std::move(handler));
}

} // end namespace detail

} // end namespace boost::mqtt5

#endif // !BOOST_MQTT5_SSL_HPP
