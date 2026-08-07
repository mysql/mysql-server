//
// Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/receive_fsm.hpp>
#include <boost/redis/error.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/assert.hpp>

namespace boost::redis::detail {

constexpr bool is_any_cancel(asio::cancellation_type_t type)
{
   return !!(
      type & (asio::cancellation_type_t::terminal | asio::cancellation_type_t::partial |
              asio::cancellation_type_t::total));
}

// We use the receive2_cancelled flag rather than will_reconnect() to
// avoid entanglement between async_run and async_receive2 cancellations.
// If we had used will_reconnect(), async_receive2 would be cancelled
// when disabling reconnection and async_run exits, and in an unpredictable fashion.
receive_action receive_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Parallel async_receive2 operations not supported
      if (st.receive2_running) {
         BOOST_REDIS_YIELD(resume_point_, 1, receive_action::action_type::immediate)
         return system::error_code(error::already_running);
      }

      // We're now running. Discard any previous cancellation state
      st.receive2_running = true;
      st.receive2_cancelled = false;

      // This operation supports total cancellation. Set it up
      BOOST_REDIS_YIELD(resume_point_, 2, receive_action::action_type::setup_cancellation)

      while (true) {
         // Wait at least once for a notification to arrive
         BOOST_REDIS_YIELD(resume_point_, 3, receive_action::action_type::wait)

         // If the wait completed successfully, we have pushes. Drain the channel and exit
         if (!ec) {
            BOOST_REDIS_YIELD(resume_point_, 4, receive_action::action_type::drain_channel)
            st.receive2_running = false;
            return system::error_code();
         }

         // Check for cancellations
         if (is_any_cancel(cancel_state) || st.receive2_cancelled) {
            st.receive2_running = false;
            return system::error_code(asio::error::operation_aborted);
         }

         // If we get any unknown errors, propagate them (shouldn't happen, but just in case)
         if (ec != asio::experimental::channel_errc::channel_cancelled) {
            st.receive2_running = false;
            return ec;
         }

         // The channel was cancelled and no cancellation state is set.
         // This is due to a reconnection. Ignore the notification
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return receive_action{system::error_code()};
}

}  // namespace boost::redis::detail
