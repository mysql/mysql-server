/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef SQL_THREAD_APPLIER_INCLUDE
#define SQL_THREAD_APPLIER_INCLUDE

#include "pipeline_handlers.h"
#include "replication_threads_api.h"
#include <mysql/group_replication_priv.h>


class Applier_handler : public Event_handler
{
public:
  Applier_handler();
  int handle_event(Pipeline_event *ev,Continuation *cont);
  int handle_action(Pipeline_action *action);
  int initialize();
  int terminate();
  bool is_unique();
  int get_role();

  /**
    Initializes the SQL thread when receiving a configuration package

    @param reset_logs                if a reset was executed in the server
    @param plugin_shutdown_timeout   the plugin's timeout for component shutdown

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_repositories(bool reset_logs,
                              ulong plugin_shutdown_timeout);

  /**
    Starts the SQL thread when receiving a action package

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int start_applier_thread();

  /**
    Stops the SQL thread when receiving a action package

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int stop_applier_thread();

  /**
    Checks if the applier, and its workers when parallel applier is
    enabled, has already consumed all relay log, that is, applier is
    waiting for transactions to be queued.

    @return the applier status
      @retval true      the applier is waiting
      @retval false     otherwise
  */
  bool is_applier_thread_waiting();

  /**
    Checks if all the queued transactions were executed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied

    @return the operation status
      @retval 0      All transactions were executed
      @retval -1     A timeout occurred
      @retval -2     An error occurred
  */
  int wait_for_gtid_execution(double timeout);

  /**
    Checks if the channel's relay log contains partial transaction.
    @return
      @retval true  If relaylog contains partial transaction.
      @retval false If relaylog does not contain partial transaction.
  */
  int is_partial_transaction_on_relay_log();

private:

  Replication_thread_api channel_interface;

};

#endif /* SQL_THREAD_APPLIER_INCLUDE */
