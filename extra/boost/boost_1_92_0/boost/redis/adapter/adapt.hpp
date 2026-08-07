/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_ADAPT_HPP
#define BOOST_REDIS_ADAPTER_ADAPT_HPP

#include <boost/redis/adapter/detail/response_traits.hpp>
#include <boost/redis/adapter/detail/result_traits.hpp>
#include <boost/redis/ignore.hpp>

namespace boost::redis::adapter {

/** @brief Adapts a type to be used as a response.
 *
 *  The type T must be either
 *
 *  @li a `response<T1, T2, T3, ...>`
 *  @li `std::vector<node<String>>`
 *
 *  The types T1, T2, etc can be any STL container, any integer type
 *  and `std::string`.
 *
 *  @tparam T The response type.
 */
template <class T>
auto boost_redis_adapt(T& t) noexcept
{
   return detail::response_traits<T>::adapt(t);
}

/** @brief Adapts a type to be used as the response to an individual command.
 *
 * It can be used with low-level APIs, like @ref boost::redis::resp3::parser.
 */
template <class T>
auto adapt2(T& t = redis::ignore) noexcept
{
   return detail::result_traits<T>::adapt(t);
}

}  // namespace boost::redis::adapter

#endif  // BOOST_REDIS_ADAPTER_ADAPT_HPP
