
/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_RESULT_HPP
#define BOOST_REDIS_ADAPTER_RESULT_HPP

#include <boost/redis/detail/resp3_type_to_error.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/system/result.hpp>

#include <string>

namespace boost::redis::adapter {

/// Stores any resp3 error.
struct error {
   /// RESP3 error data type.
   resp3::type data_type = resp3::type::invalid;

   /// Diagnostic error message sent by Redis.
   std::string diagnostic;
};

/** @brief Compares two error objects for equality
 *  @relates error
 *
 *  @param a Left hand side error object.
 *  @param b Right hand side error object.
 */
inline bool operator==(error const& a, error const& b)
{
   return a.data_type == b.data_type && a.diagnostic == b.diagnostic;
}

/** @brief Compares two error objects for difference
 *  @relates error
 *
 *  @param a Left hand side error object.
 *  @param b Right hand side error object.
 */
inline bool operator!=(error const& a, error const& b) { return !(a == b); }

/// Stores response to individual Redis commands.
template <class Value>
using result = system::result<Value, error>;

/**
 * @brief Allows using @ref error with `boost::system::result`.
 * @param e The error to throw.
 * @relates error
 */
BOOST_NORETURN inline void throw_exception_from_error(error const& e, boost::source_location const&)
{
   throw system::system_error(
      system::error_code(detail::resp3_type_to_error(e.data_type)),
      e.diagnostic);
}

}  // namespace boost::redis::adapter

#endif  // BOOST_REDIS_ADAPTER_RESULT_HPP
