/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "connection_control_coordinator.h"
#include "connection_control.h"
#include "failed_attempts_list_imp.h"

namespace connection_control {
/**
  Reset Connection_event_coordinator information
*/

void Connection_event_coordinator::reset() {
  for (uint i = static_cast<uint>(STAT_CONNECTION_DELAY_TRIGGERED);
       i < static_cast<uint>(STAT_LAST); ++i) {
    m_status_vars_subscription[i] = nullptr;
  }
}

/**
  Register an event subscriber.

  A subscriber can provide following preferences:
    1. Set of events for which subscriber is interested
    2. Set of variables for which subscriber would like to receive update
    3. Set of stats for which subscriber would like to send update

  @param [in] subscriber    Handle to Connection_event_observers
  @param [in] sys_vars      opt_connection_control vector
  @param [in] status_vars   stats_connection_control vector

  @returns subscription status
    @retval false Subscription successful
    @retval true Failure in subscription for given combination of prefernece.
                 Most probably, other subscriber has already subscribed for
                 status var update.
*/

bool Connection_event_coordinator::register_event_subscriber(
    Connection_event_observer **subscriber,
    std::vector<opt_connection_control, CustomAllocator<opt_connection_control>>
        *sys_vars,
    std::vector<stats_connection_control,
                CustomAllocator<stats_connection_control>> *status_vars) {
  bool error = false;

  assert(subscriber != nullptr);

  if (status_vars != nullptr) {
    for (const auto &var : *status_vars) {
      if (var >= STAT_LAST || m_status_vars_subscription[var] != nullptr) {
        /*
          Either an invalid status variable is specified or
          someone has already subscribed for status variable
        */
        error = true;
        break;
      }
    }
  }

  if (!error && sys_vars != nullptr) {
    for (const auto &var : *sys_vars) {
      if (var >= OPT_LAST) {
        error = true;
        break;
      }
    }
  }

  if (!error) {
    /*
      Create Connection_event_subscriber object and
      initialize it with required details.
    */
    try {
      Connection_event_subscriber const subscriber_info(subscriber, sys_vars);

      /* Insert new entry in m_subscribers */
      m_subscribers.push_back(subscriber_info);
    } catch (...) {
      /* Something went wrong. Mostly likely OOM. */
      error = true;
    }

    /*
      Update m_status_vars_subscription only if subscriber information
      has been inserted in m_subscribers successfully.
      Only one subscriber gets notification for status vars (the last added one)
    */
    if (!error) {
      if (status_vars != nullptr) {
        for (const auto &var : *status_vars) {
          m_status_vars_subscription[var] = *subscriber;
        }
      }
    }
  }
  return error;
}

/**
    Handle connection event.
    When a notification from server is received, perform following:
    Iterate through list of subscribers
    If a subscriber has shown interest in received event,
    call notify() for the subscriber

    Note : If we receive error from a subscriber, we log it and move on.

    @param [in] thd               THD handle
    @param [in] connection_event  Event information
*/

void Connection_event_coordinator::notify_event(
    MYSQL_THD thd,
    const mysql_event_tracking_connection_data *connection_event) {
  for (auto &subscriber : m_subscribers) {
    (void)subscriber.m_subscriber->notify_event(thd, this, connection_event);
  }
}

/**
  Process change in sys_var value

  Iterate through all subscribers
  - If a subscriber has shown interest in getting notification for given
    system variable, call notify_sys_var.

  Note : If we receive error from a subscriber, we log it and move on.

  @param [in] variable               Variable information
  @param [in] new_value              New value for variable
*/

void Connection_event_coordinator::notify_sys_var(
    opt_connection_control variable, void *new_value) {
  for (auto &subscriber : m_subscribers) {
    if (subscriber.m_sys_vars[variable]) {
      (void)subscriber.m_subscriber->notify_sys_var(this, variable, new_value);
    }
  }
}

/**
  Update a status variable

  @param [in] observer   Requestor
  @param [in] status_var Status variable to be updated
  @param [in] action     Operation to be performed on status variable

  @returns status of the operation
    @retval false Success
    @retval true Error in processing
*/

bool Connection_event_coordinator::notify_status_var(
    Connection_event_observer **observer, stats_connection_control status_var,
    status_var_action action) {
  bool error = false;

  if (status_var < STAT_LAST &&
      m_status_vars_subscription[status_var] == *observer) {
    switch (action) {
      case ACTION_INC:
        ++g_statistics.stats_array[status_var];
        break;
      case ACTION_RESET:
        g_statistics.stats_array[status_var].store(0);
        g_failed_attempts_list.reset();
        break;
      default:
        error = true;
        assert(false);
        break;
    }
  }
  return error;
}
}  // namespace connection_control
