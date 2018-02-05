/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_THREAD_APPLIER_INCLUDE
#define SQL_THREAD_APPLIER_INCLUDE

#include <mysql/group_replication_priv.h>

#include "my_inttypes.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/replication_threads_api.h"

class Applier_handler : public Event_handler {
 public:
  Applier_handler();
  int handle_event(Pipeline_event *ev, Continuation *cont);
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
  int initialize_repositories(bool reset_logs, ulong plugin_shutdown_timeout);

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
