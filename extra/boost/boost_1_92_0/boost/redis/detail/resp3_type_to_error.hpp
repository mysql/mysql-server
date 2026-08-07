/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_RESP3_TYPE_TO_ERROR_HPP
#define BOOST_RESP3_TYPE_TO_ERROR_HPP

#include <boost/redis/error.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert.hpp>

namespace boost::redis::detail {

inline error resp3_type_to_error(resp3::type t)
{
   switch (t) {
      case resp3::type::simple_error: return error::resp3_simple_error;
      case resp3::type::blob_error:   return error::resp3_blob_error;
      case resp3::type::null:         return error::resp3_null;
      default:                        BOOST_ASSERT_MSG(false, "Unexpected data type."); return error::resp3_simple_error;
   }
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_ADAPTER_RESULT_HPP
