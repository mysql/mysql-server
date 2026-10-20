//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECT_PARAMS_HPP
#define BOOST_REDIS_CONNECT_PARAMS_HPP

// Parameters used by redis_stream::async_connect

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_fsm.hpp>

#include <chrono>
#include <string_view>

namespace boost::redis::detail {

// Fully identifies where a server is listening. Reference type.
class any_address_view {
   transport_type type_;
   union {
      const address* tcp_;
      std::string_view unix_;
   };

public:
   any_address_view(const address& addr, bool use_ssl) noexcept
   : type_(use_ssl ? transport_type::tcp_tls : transport_type::tcp)
   , tcp_(&addr)
   { }

   explicit any_address_view(std::string_view unix_socket) noexcept
   : type_(transport_type::unix_socket)
   , unix_(unix_socket)
   { }

   transport_type type() const { return type_; }

   const address& tcp_address() const
   {
      BOOST_ASSERT(type_ == transport_type::tcp || type_ == transport_type::tcp_tls);
      return *tcp_;
   }

   std::string_view unix_socket() const
   {
      BOOST_ASSERT(type_ == transport_type::unix_socket);
      return unix_;
   }
};

struct connect_params {
   any_address_view addr;
   std::chrono::steady_clock::duration resolve_timeout;
   std::chrono::steady_clock::duration connect_timeout;
   std::chrono::steady_clock::duration ssl_handshake_timeout;
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP
