//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_ONE_FSM_IPP
#define BOOST_REDIS_EXEC_ONE_FSM_IPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/exec_one_fsm.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

exec_one_action exec_one_fsm::resume(
   multiplexer& mpx,
   system::error_code ec,
   std::size_t bytes_transferred,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Send the request to the server
      BOOST_REDIS_YIELD(resume_point_, 1, exec_one_action_type::write)

      // Errors and cancellations
      if (is_terminal_cancel(cancel_state))
         return system::error_code{asio::error::operation_aborted};
      if (ec)
         return ec;

      // If the request didn't expect any response, we're done
      if (remaining_responses_ == 0u)
         return system::error_code{};

      // Read responses until we're done
      mpx.get_read_buffer().clear();
      while (true) {
         // Prepare the buffer to read some data
         ec = mpx.prepare_read();
         if (ec)
            return ec;

         // Read data
         BOOST_REDIS_YIELD(resume_point_, 2, exec_one_action_type::read_some)

         // Errors and cancellations
         if (is_terminal_cancel(cancel_state))
            return system::error_code{asio::error::operation_aborted};
         if (ec)
            return ec;

         // Commit the data into the buffer
         mpx.commit_read(bytes_transferred);

         // Consume the data until we run out or all the responses have been read
         while (resp3::parse(parser_, mpx.get_read_buffer().get_commited(), adapter_, ec)) {
            // Check for errors
            if (ec)
               return ec;

            // We've finished parsing a response
            mpx.get_read_buffer().consume(parser_.get_consumed());
            parser_.reset();

            // When no more responses remain, we're done.
            // Don't read ahead, even if more data is available
            if (--remaining_responses_ == 0u)
               return system::error_code{};
         }
      }
   }

   BOOST_ASSERT(false);
   return system::error_code();
}

}  // namespace boost::redis::detail

#endif
