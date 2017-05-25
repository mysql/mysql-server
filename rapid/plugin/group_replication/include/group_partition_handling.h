/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GROUP_PARTITION_HANDLING_INCLUDE
#define GROUP_PARTITION_HANDLING_INCLUDE

#include "read_mode_handler.h"
#include "plugin_utils.h"

class Group_partition_handling
{

public:

  Group_partition_handling(Shared_writelock *shared_stop_write_lock,
                           ulong unreachable_timeout);

  /**
    The class destructor
  */
  virtual ~Group_partition_handling();

  /**
    The thread handler.

    @return
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
      @retval 0  The partition thread wont run or timeout.
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

  /**
    Internal method that contains the logic for leaving and killing transactions
  */
  void kill_transactions_and_leave();

  //Group_partition_handling variables

  /** Is member in partition */
  bool member_in_partition;

  /** Is the thread running */
  bool thread_running;

  /** Should we abort the process that will kill pending transaction */
  bool partition_handling_aborted;

  /** Did the partition handler terminate and killed pending transactions */
  bool partition_handling_terminated;

  /** The number of seconds until the member goes into error state*/
  ulong timeout_on_unreachable;

  /** The stop lock used when killing transaction/stopping server*/
  Shared_writelock *shared_stop_write_lock;

  /* Thread related structures */

  my_thread_handle partition_trx_handler_pthd;
  //run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t  run_cond;
  mysql_mutex_t trx_termination_aborted_lock;
  mysql_cond_t  trx_termination_aborted_cond;
};

#endif /* GROUP_PARTITION_HANDLING_INCLUDE */
