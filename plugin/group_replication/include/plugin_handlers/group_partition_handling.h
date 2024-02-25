/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef GROUP_PARTITION_HANDLING_INCLUDE
#define GROUP_PARTITION_HANDLING_INCLUDE

#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"
#include "plugin/group_replication/include/plugin_utils.h"

class Group_partition_handling {
 public:
  Group_partition_handling(ulong unreachable_timeout);

  /**
    The class destructor
  */
  virtual ~Group_partition_handling();

  /**
    The thread handler.

    @retval 0      OK
    @retval !=0    Error
  */
  int partition_thread_handler();

  /**
    Launch the partition thread handler

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int launch_partition_handler_thread();

  /**
    Terminate the partition thread handler

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate_partition_handler_thread();

  /**
    Is the member currently/or was on a partition?

    @note this flag is set to true whenever the partition handler is launched
    and set to false whenever someone tries to abort it.

    @return is member on partition, or was on one
      @retval true     yes
      @retval false    no
  */
  bool is_member_on_partition();

  /**
    Is the partition handler thread running
    @return true if running, false otherwise
  */
  bool is_partition_handler_running();

  /**
    Updates the timeout when the member becomes unreachable.

    @param unreachable_timeout  The timeout before going into error
  */
  void update_timeout_on_unreachable(ulong unreachable_timeout);

  /**
    @return the configured timeout
      @retval 0  The partition thread won't run or timeout.
      @retval >0 After this seconds the plugin will move to ERROR in a minority
  */
  ulong get_timeout_on_unreachable();

  /**
    Signals the thread to abort the waiting process.

    @return the operation status
      @retval true    It already killed pending transactions and left the group
      @retval false   The thread was not running, or was aborted in time.
  */
  bool abort_partition_handler_if_running();

  /**
    @return is the process finished
      @retval true    It already killed pending transactions and left the group
      @retval false   The thread was not running, or was aborted.
  */
  bool is_partition_handling_terminated();

 private:
  // Group_partition_handling variables

  /** Is member in partition */
  bool member_in_partition;

  /** Group partition thread state */
  thread_state group_partition_thd_state;

  /** Should we abort the process that will kill pending transaction */
  bool partition_handling_aborted;

  /** Did the partition handler terminate and killed pending transactions */
  bool partition_handling_terminated;

  /** The number of seconds until the member goes into error state*/
  ulong timeout_on_unreachable;

  /* Thread related structures */

  my_thread_handle partition_trx_handler_pthd;
  // run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t run_cond;
  mysql_mutex_t trx_termination_aborted_lock;
  mysql_cond_t trx_termination_aborted_cond;
};

#endif /* GROUP_PARTITION_HANDLING_INCLUDE */
