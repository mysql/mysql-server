//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_WRITER_FSM_IPP
#define BOOST_REDIS_WRITER_FSM_IPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

inline void process_ping_node(
   buffered_logger& lgr,
   resp3::basic_node<std::string_view> const& nd,
   system::error_code& ec)
{
   switch (nd.data_type) {
      case resp3::type::simple_error: ec = redis::error::resp3_simple_error; break;
      case resp3::type::blob_error:   ec = redis::error::resp3_blob_error; break;
      default:                        ;
   }

   if (ec) {
      log_err(lgr, "Health checker: server answered ping with an error: ", nd.value);
   }
}

inline any_adapter make_ping_adapter(buffered_logger& lgr)
{
   return any_adapter{
      [&lgr](any_adapter::parse_event evt, resp3::node_view const& nd, system::error_code& ec) {
         if (evt == any_adapter::parse_event::node)
            process_ping_node(lgr, nd, ec);
      }};
}

writer_action writer_fsm::resume(
   connection_state& st,
   system::error_code ec,
   std::size_t bytes_written,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      for (;;) {
         // Attempt to write while we have requests ready to send
         while (st.mpx.prepare_write() != 0u) {
            // Write an entire message. We can't use asio::async_write because we want
            // to apply timeouts to individual write operations
            for (;;) {
               // Write what we can. If nothing has been written for the health check
               // interval, we consider the connection as failed
               BOOST_REDIS_YIELD(
                  resume_point_,
                  1,
                  writer_action::write_some(st.cfg.health_check_interval))

               // Commit the received bytes. This accounts for partial success
               bool finished = st.mpx.commit_write(bytes_written);
               log_debug(st.logger, "Writer task: ", bytes_written, " bytes written.");

               // Check for cancellations and translate error codes
               if (is_terminal_cancel(cancel_state))
                  ec = asio::error::operation_aborted;
               else if (ec == asio::error::operation_aborted)
                  ec = error::write_timeout;

               // Check for errors
               if (ec) {
                  if (ec == asio::error::operation_aborted) {
                     log_debug(st.logger, "Writer task: cancelled (1).");
                  } else {
                     log_err(st.logger, "Error writing data to the server: ", ec);
                  }
                  return ec;
               }

               // Are we done yet?
               if (finished)
                  break;
            }
         }

         // No more requests ready to be written. Wait for more, or until we need to send a PING
         BOOST_REDIS_YIELD(resume_point_, 2, writer_action::wait(st.cfg.health_check_interval))

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Writer task: cancelled (2).");
            return system::error_code(asio::error::operation_aborted);
         }

         // If we weren't notified, it's because there is no data and we should send a health check
         if (!ec) {
            auto elem = make_elem(st.ping_req, make_ping_adapter(st.logger));
            elem->set_done_callback([] { });
            st.mpx.add(elem);
         }
      }
   }

   // We should never reach here
   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail

#endif
