/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "connection_control_coordinator.h"
#include "connection_control.h"
#include <my_global.h>
#include <my_atomic.h>

namespace connection_control
{
  /**
    Reset Connection_event_coordinator information
  */

  void
    Connection_event_coordinator::reset()
  {
    m_subscribers.clear();
    for (uint i= (uint)STAT_CONNECTION_DELAY_TRIGGERED;
         i < (uint)STAT_LAST; ++i)
      m_status_vars_subscription[i]= 0;
  }


  /**
    Register an event subscriber.

    A subscriber can provide following preferences:
      1. Set of events for which subscriber is interested
      2. Set of variables for which subscriber would like to receive update
      3. Set of stats for which subscriber would like to send update

    @param subscriber [in]    Handle to Connection_event_observers
    @param events [in]        Event mask supplied by subscriber
    @param sys_vars [in]      opt_connection_control vector
    @param status_vars [in]   stats_connection_control vector

    @returns subscription status
      @retval false Subscription successful
      @retval true Failure in subscription for given combination of prefernece.
                   Most probably, other subscriber has already subscribed for
                   status var update.
  */

  bool
  Connection_event_coordinator::register_event_subscriber(
    Connection_event_observer **subscriber,
    std::vector<opt_connection_control> *sys_vars,
    std::vector<stats_connection_control> *status_vars)
  {
    DBUG_ENTER("Connection_event_coordinator::register_event_subscriber");
    bool error= false;
    std::vector<opt_connection_control>::iterator sys_vars_it;
    std::vector<stats_connection_control>::iterator status_vars_it;

    DBUG_ASSERT(subscriber != 0);

    if (status_vars)
    {
      for (status_vars_it= status_vars->begin();
           status_vars_it != status_vars->end();
           ++status_vars_it)
      {
        if (*status_vars_it >= STAT_LAST ||
            m_status_vars_subscription[*status_vars_it] != 0)
        {
          /*
            Either an invalid status variable is specified or
            someone has already subscribed for status variable
          */
          error= true;
          break;
        }
      }
    }

    if (!error && sys_vars)
    {
      for (sys_vars_it= sys_vars->begin();
           sys_vars_it != sys_vars->end();
           ++sys_vars_it)
      {
        if (*sys_vars_it >= OPT_LAST)
          error= true;
        break;
      }
    }

    if (!error)
    {
      /*
        Create Connection_event_subscriber object and
        initialize it with required details.
      */
      Connection_event_subscriber subscriber_info;
      subscriber_info.m_subscriber= *subscriber;

      /* Reset the list first */
      for (uint i= (uint)OPT_FAILED_CONNECTIONS_THRESHOLD;
           i < (uint)OPT_LAST;
           ++i)
        subscriber_info.m_sys_vars[i]= false;

      /* Now set the bits which are requested by subscriber */
      for (sys_vars_it= sys_vars->begin();
           sys_vars_it != sys_vars->end();
           ++sys_vars_it)
        subscriber_info.m_sys_vars[*sys_vars_it]= true;

      /* Insert new entry in m_subscribers */
      try
      {
        m_subscribers.push_back(subscriber_info);
      }
      catch (...)
      {
        /* Something went wrong. Mostly likely OOM. */
        error= true;
      }

      /*
        Update m_status_vars_subscription only if subscriber information
        has been inserted in m_subscribers successfully.
      */
      if (!error)
      {
        for (status_vars_it= status_vars->begin();
             status_vars_it != status_vars->end();
             ++status_vars_it)
          m_status_vars_subscription[*status_vars_it]= *subscriber;
      }
    }
    DBUG_RETURN(error);
  }


  /**
      Handle connection event.
      When a notification from server is received, perform following:
      1. Iterate through list of subscribers
      - If a subscriber has shown interest in received event,
        call notify() for the subscriber
      2. Interate through list of status variables
      - If subscriber has show interest in any status variable,
        call notify_status_var() for the subscriber
      - If subscriber suggests an action on status variable,
        perform the action

      Note : If we receive error from a subscriber, we log it and move on.

      @param thd [in]               THD handle
      @param error_handler [in]     Error handler class
      @param connection_event [in]  Event information
  */

  void
  Connection_event_coordinator::notify_event(
    MYSQL_THD thd,
    Error_handler *error_handler,
    const mysql_event_connection *connection_event)
  {
    DBUG_ENTER("Connection_event_coordinator::notify_event");
    std::vector<Connection_event_subscriber>::iterator it= m_subscribers.begin();

    while (it != m_subscribers.end())
    {
      Connection_event_subscriber event_subscriber= *it;
      (void)event_subscriber.m_subscriber->notify_event(thd, this,
                                                        connection_event,
                                                        error_handler);

      ++it;
    }

    DBUG_VOID_RETURN;
  }


  /**
    Process change in sys_var value

    Iterate through all subscribers
    - If a subscriber has shown interest in getting notification for given
      system variable, call notify_sys_var.

    Note : If we receive error from a subscriber, we log it and move on.

    @param error_hanlder [in]     Error handler class
    @param opt_connection_control Variable information
    @param new_value [in]         New value for variable
  */

  void
  Connection_event_coordinator::notify_sys_var(Error_handler *error_handler,
                                               opt_connection_control variable,
                                               void *new_value)
  {
    DBUG_ENTER("Connection_event_coordinator::notify_sys_var");
    std::vector<Connection_event_subscriber>::iterator it= m_subscribers.begin();

    while (it != m_subscribers.end())
    {
      Connection_event_subscriber event_subscriber= *it;
      if (event_subscriber.m_sys_vars[variable])
      {
        (void) event_subscriber.m_subscriber->notify_sys_var(this,
                                                             variable,
                                                             new_value,
                                                             error_handler);
      }
      ++it;
    }

    DBUG_VOID_RETURN;
  }


  /**
    Update a status variable

    @param observer [in]   Requestor
    @param status_var [in] Status variable to be updated
    @param action [in]     Operation to be performed on status variable

    @returns status of the operation
      @retval false Success
      @retval true Error in processing
  */

  bool
  Connection_event_coordinator::notify_status_var(Connection_event_observer **observer,
                                                  stats_connection_control status_var,
                                                  status_var_action action)
  {
    DBUG_ENTER("Connection_event_coordinator::notify_status_var");
    bool error= false;

    if (m_status_vars_subscription[status_var] == *observer)
    {
      if (status_var < STAT_LAST)
      {
        switch (action)
        {
          case ACTION_INC:
          {
            my_atomic_add64(&g_statistics.stats_array[status_var], 1);
            break;
          }
          case ACTION_RESET:
          {
            my_atomic_store64(&g_statistics.stats_array[status_var], 0);
            break;
          }
          default:
          {
            error= true;
            DBUG_ASSERT(FALSE);
            break;
          }
        }
      }
    }

    DBUG_RETURN(error);
  }
}
