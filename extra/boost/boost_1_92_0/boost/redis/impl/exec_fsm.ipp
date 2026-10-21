//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_FSM_IPP
#define BOOST_REDIS_EXEC_FSM_IPP

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

namespace boost::redis::detail {

inline bool is_partial_or_terminal_cancel(asio::cancellation_type_t type)
{
   return !!(type & (asio::cancellation_type_t::partial | asio::cancellation_type_t::terminal));
}

inline bool is_total_cancel(asio::cancellation_type_t type)
{
   return !!(type & asio::cancellation_type_t::total);
}

exec_action exec_fsm::resume(
   bool connection_is_open,
   connection_state& st,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Check whether the user wants to wait for the connection to
      // be established.
      if (elem_->get_request().get_config().cancel_if_not_connected && !connection_is_open) {
         BOOST_REDIS_YIELD(resume_point_, 1, exec_action_type::immediate)
         elem_.reset();  // Deallocate memory before finalizing
         return system::error_code(error::not_connected);
      }

      // No more immediate errors. Set up the supported cancellation types.
      // This is required to get partial and total cancellations.
      // This is a potentially allocating operation, so do it as late as we can.
      BOOST_REDIS_YIELD(resume_point_, 2, exec_action_type::setup_cancellation)

      // Add the request to the multiplexer
      st.mpx.add(elem_);

      // Notify the writer task that there is work to do. If the task is not
      // listening (e.g. it's already writing or the connection is not healthy),
      // this is a no-op. Since this is sync, no cancellation can happen here.
      BOOST_REDIS_YIELD(resume_point_, 3, exec_action_type::notify_writer)

      while (true) {
         // Wait until we get notified. This will return once the request completes,
         // or upon any kind of cancellation
         BOOST_REDIS_YIELD(resume_point_, 4, exec_action_type::wait_for_response)

         // If the request has completed (with error or not), we're done
         if (elem_->is_done()) {
            // If the request completed successfully and we were configured to do so,
            // record the changes applied to the pubsub state
            if (!elem_->get_error())
               st.tracker.commit_changes(elem_->get_request());

            // Deallocate memory before finalizing
            exec_action act{elem_->get_error(), elem_->get_read_size()};
            elem_.reset();
            return act;
         }

         // Total cancellation can only be handled if the request hasn't been sent yet.
         // Partial and terminal cancellation can always be served
         if (
            (is_total_cancel(cancel_state) && elem_->is_waiting()) ||
            is_partial_or_terminal_cancel(cancel_state)) {
            st.mpx.cancel(elem_);
            elem_.reset();  // Deallocate memory before finalizing
            return exec_action{asio::error::operation_aborted};
         }
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return exec_action{system::error_code()};
}

}  // namespace boost::redis::detail

#endif
