/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_IGNORE_HPP
#define BOOST_REDIS_ADAPTER_IGNORE_HPP

#include <boost/redis/error.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

namespace boost::redis::adapter {

/** @brief An adapter that ignores responses.
 *
 *  RESP3 errors won't be ignored.
 */
struct ignore {
   void on_init() { }
   void on_done() { }

   void on_node(resp3::basic_node<std::string_view> const& nd, system::error_code& ec)
   {
      switch (nd.data_type) {
         case resp3::type::simple_error: ec = redis::error::resp3_simple_error; break;
         case resp3::type::blob_error:   ec = redis::error::resp3_blob_error; break;
         case resp3::type::null:         ec = redis::error::resp3_null; break;
         default:                        ;
      }
   }
};

}  // namespace boost::redis::adapter

#endif  // BOOST_REDIS_ADAPTER_IGNORE_HPP
