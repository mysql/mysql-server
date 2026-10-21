/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>

namespace boost::redis::detail {

reader_fsm::action reader_fsm::resume(
   connection_state& st,
   std::size_t bytes_read,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      for (;;) {
         // Prepare the buffer for the read operation
         ec = st.mpx.prepare_read();
         if (ec) {
            log_err(st.logger, "Error preparing the read buffer: ", ec);
            return {ec};
         }

         // Read. The connection might spend health_check_interval without writing data.
         // Give it another health_check_interval for the response to arrive.
         // If we don't get anything in this time, consider the connection as dead
         log_debug(st.logger, "Reader task: issuing read");
         BOOST_REDIS_YIELD(resume_point_, 1, action::read_some(2 * st.cfg.health_check_interval))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Reader task: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Translate timeout errors caused by operation_aborted to more legible ones.
         // A timeout here means that we didn't receive data in time.
         // Note that cancellation is already handled by the above statement.
         if (ec == asio::error::operation_aborted) {
            ec = error::pong_timeout;
         }

         // Log what we read
         log_debug(st.logger, "Reader task: ", bytes_read, " bytes read");
         if (ec) {
            log_err(st.logger, "Error reading data from the server: ", ec);
         }

         // Process the bytes read, even if there was an error
         st.mpx.commit_read(bytes_read);

         // Check for read errors
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            return ec;
         }

         // Process the data that we've read
         while (st.mpx.get_read_buffer_size() != 0) {
            res_ = st.mpx.consume(ec);

            if (ec) {
               // TODO: Perhaps log what has not been consumed to aid
               // debugging.
               if (ec == error::resp3_hello) {
                  // This is already logged in the setup adapter
                  log_debug(st.logger, "Error processing message: setup request error");
               } else {
                  log_err(st.logger, "Error processing message: ", ec);
               }
               return ec;
            }

            if (res_.first == consume_result::needs_more) {
               log_debug(st.logger, "Reader task: incomplete message received");
               break;
            }

            if (res_.first == consume_result::got_push) {
               BOOST_REDIS_YIELD(resume_point_, 2, action::notify_push_receiver(res_.second))
               // Check for cancellations
               if (is_terminal_cancel(cancel_state)) {
                  log_debug(st.logger, "Reader task: cancelled (2)");
                  return system::error_code(asio::error::operation_aborted);
               }

               // Check for other errors.
               // We should't get any in the real world, but just in case.
               if (ec) {
                  log_err(st.logger, "Error notifying push receiver: ", ec);
                  return ec;
               }
            } else {
               // TODO: Here we should notify the exec operation that
               // it can be completed. This will improve log clarity
               // and will make this code symmetrical in how it
               // handles pushes and other messages. The new action
               // type can be named notify_exec. To do that we need to
               // refactor the multiplexer.
            }
         }
      }
   }

   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail
