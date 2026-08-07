/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <functional>
#include <string>
#include <string_view>

namespace boost::redis {

/** @brief Defines logging configuration.
 *
 *  See the member descriptions for more info.
 */
struct logger {
   /// Syslog-like log levels.
   enum class level
   {
      /// Disabled
      disabled,

      /// Emergency
      emerg,

      /// Alert
      alert,

      /// Critical
      crit,

      /// Error
      err,

      /// Warning
      warning,

      /// Notice
      notice,

      /// Info
      info,

      /// Debug
      debug,
   };

   /** @brief Constructor from a level.
    *
    * Constructs a logger with the specified level
    * and a logging function that prints messages to `stderr`.
    *
    * @param l The value to set @ref lvl to.
    *
    * @par Exceptions
    * No-throw guarantee.
    */
   logger(level l = level::info);

   /** @brief Constructor from a level and a function.
    *
    * Constructs a logger by setting its members to the specified values.
    *
    * @param l The value to set @ref lvl to.
    * @param fn The value to set @ref fn to.
    *
    * @par Exceptions
    * No-throw guarantee.
    */
   logger(level l, std::function<void(level, std::string_view)> fn)
   : lvl{l}
   , fn{std::move(fn)}
   { }

   /**
    * @brief Defines a severity filter for messages.
    *
    * Only messages with a level >= to the one specified by the logger
    * will be logged.
    */
   level lvl;

   /**
    * @brief Defines a severity filter for messages.
    *
    * Only messages with a level >= to the one specified by the logger
    * will be logged.
    */
   std::function<void(level, std::string_view)> fn;
};

namespace detail {

struct buffered_logger {
   logger lgr;
   std::string buffer{};
};

}  // namespace detail

}  // namespace boost::redis

#endif  // BOOST_REDIS_LOGGER_HPP
