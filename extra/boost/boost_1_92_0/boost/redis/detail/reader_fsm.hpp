/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READER_FSM_HPP
#define BOOST_REDIS_READER_FSM_HPP

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/multiplexer.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstddef>

namespace boost::redis::detail {

class read_buffer;

class reader_fsm {
public:
   class action {
   public:
      enum class type
      {
         read_some,
         notify_push_receiver,
         done,
      };

      action(system::error_code ec) noexcept
      : type_(type::done)
      , ec_(ec)
      { }

      static action read_some(std::chrono::steady_clock::duration timeout) { return {timeout}; }

      static action notify_push_receiver(std::size_t bytes) { return {bytes}; }

      type get_type() const { return type_; }

      system::error_code error() const
      {
         BOOST_ASSERT(type_ == type::done);
         return ec_;
      }

      std::chrono::steady_clock::duration timeout() const
      {
         BOOST_ASSERT(type_ == type::read_some);
         return timeout_;
      }

      std::size_t push_size() const
      {
         BOOST_ASSERT(type_ == type::notify_push_receiver);
         return push_size_;
      }

   private:
      action(std::size_t push_size) noexcept
      : type_(type::notify_push_receiver)
      , push_size_(push_size)
      { }

      action(std::chrono::steady_clock::duration t) noexcept
      : type_(type::read_some)
      , timeout_(t)
      { }

      type type_;
      union {
         system::error_code ec_;
         std::chrono::steady_clock::duration timeout_;
         std::size_t push_size_{};
      };
   };

   action resume(
      connection_state& st,
      std::size_t bytes_read,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);

   reader_fsm() = default;

private:
   int resume_point_{0};
   std::pair<consume_result, std::size_t> res_{consume_result::needs_more, 0u};
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READER_FSM_HPP
