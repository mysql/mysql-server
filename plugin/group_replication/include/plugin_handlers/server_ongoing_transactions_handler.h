/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef SERVER_ONGOING_TRANSACTIONS_HANDLER_INCLUDED
#define SERVER_ONGOING_TRANSACTIONS_HANDLER_INCLUDED

#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"

#include <queue>

class Server_ongoing_transactions_handler : public Group_transaction_listener {
 public:
  /** Initialize the class and get the server service */
  Server_ongoing_transactions_handler();

  /** Class destructor */
  ~Server_ongoing_transactions_handler() override;

  /**
    Fetch the registry and the service for this class
    @param stage_handler the stage handler to report progress

    @returns false in case of success, or true otherwise
  */
  bool initialize_server_service(Plugin_stage_monitor_handler *stage_handler);

  /**
    Get the list of running transactions from the server
    @param[out] ids an array of thread ids
    @param[out] size the size of the array returned on ids
    @returns 0 in case of success, 1 in case of error
  */
  bool get_server_running_transactions(ulong **ids, ulong *size);

  /**
    Gets running transactions and waits for its end

    @param abort_flag cancel flag
    @param id_to_ignore if different from 0, the method does not wait for it

    @returns 0 in case of success, 1 in case of error
  */
  int wait_for_current_transaction_load_execution(
      bool *abort_flag, my_thread_id id_to_ignore = 0);

  /** Abort any running waiting process */
  void abort_waiting_process();

  int before_transaction_begin(my_thread_id thread_id,
                               ulong gr_consistency_level, ulong hold_timeout,
                               enum_rpl_channel_type rpl_channel_type) override;

  int before_commit(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int before_rollback(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int after_rollback(my_thread_id thread_id) override;

  int after_commit(my_thread_id thread_id, rpl_sidno sidno,
                   rpl_gno gno) override;

 private:
  /** The transactions that finished while the service is running */
  std::queue<my_thread_id> thread_ids_finished;

  /** The lock for the query wait */
  mysql_mutex_t query_wait_lock;

  /** The server service handle */
  my_h_service generic_service;

  /** A stage handler for reporting progress*/
  Plugin_stage_monitor_handler *stage_handler;
};

#endif /* SERVER_ONGOING_TRANSACTIONS_HANDLER_INCLUDED */
