/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOG_UTILS_HPP
#define BOOST_REDIS_LOG_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/logger.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace boost::redis::detail {

// Internal trait that defines how to log different types.
// The base template applies to types convertible to string_view
template <class T>
struct log_traits {
   // log should convert the input value to string and append it to the supplied buffer
   static inline void log(std::string& to, std::string_view value) { to += value; }
};

// Formatting size_t and error codes is shared between almost all FSMs, so it's defined here.
// Support for types used only in one FSM should be added in the relevant FSM file.
template <>
struct log_traits<std::size_t> {
   static inline void log(std::string& to, std::size_t value) { to += std::to_string(value); }
};

template <>
struct log_traits<system::error_code> {
   static inline void log(std::string& to, system::error_code value)
   {
      // Using error_code::what() includes any source code info
      // that the error may contain, making the messages too long.
      // This implementation was taken from error_code::what()
      to += value.message();
      to += " [";
      to += value.to_string();
      to += ']';
   }
};

template <>
struct log_traits<address> {
   static inline void log(std::string& to, const address& value)
   {
      to += value.host;
      to += ':';
      to += value.port;
   }
};

template <class... Args>
void format_log_args(std::string& to, const Args&... args)
{
   auto dummy = {(log_traits<Args>::log(to, args), 0)...};
   ignore_unused(dummy);
}

// Logs a message with the specified severity to the logger.
// Formatting won't be performed if the logger's level is inferior to lvl.
// args are stringized using log_traits, and concatenated.
template <class Arg0, class... Rest>
void log(buffered_logger& to, logger::level lvl, const Arg0& arg0, const Rest&... arg_rest)
{
   // Severity check
   if (to.lgr.lvl < lvl)
      return;

   // Optimization: if we get passed a single string, don't copy it to the buffer
   if constexpr (sizeof...(Rest) == 0u && std::is_convertible_v<Arg0, std::string_view>) {
      to.lgr.fn(lvl, arg0);
   } else {
      to.buffer.clear();
      format_log_args(to.buffer, arg0, arg_rest...);
      to.lgr.fn(lvl, to.buffer);
   }
}

// Shorthand for each log level we use
template <class... Args>
void log_debug(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::debug, args...);
}

template <class... Args>
void log_info(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::info, args...);
}

template <class... Args>
void log_err(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::err, args...);
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_LOGGER_HPP
