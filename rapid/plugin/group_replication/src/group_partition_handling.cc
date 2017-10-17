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

#include "group_partition_handling.h"
#include "plugin_psi.h"
#include "plugin.h"
#include <mysql/group_replication_priv.h>

using std::string;

static void *launch_handler_thread(void* arg)
{
  Group_partition_handling *handler= (Group_partition_handling*) arg;
  handler->partition_thread_handler();
  return 0;
}

Group_partition_handling::
Group_partition_handling(Shared_writelock *shared_stop_lock,
                         ulong unreachable_timeout)
  : member_in_partition(false),
    thread_running(false), partition_handling_aborted(false),
    partition_handling_terminated(false),
    timeout_on_unreachable(unreachable_timeout),
    shared_stop_write_lock(shared_stop_lock)
{
  mysql_mutex_init(key_GR_LOCK_group_part_handler_run, &run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_group_part_handler_abort,
                   &trx_termination_aborted_lock,
                   MY_MUTEX_INIT_FAST);

  mysql_cond_init(key_GR_COND_group_part_handler_run, &run_cond);
  mysql_cond_init(key_GR_COND_group_part_handler_abort,
                  &trx_termination_aborted_cond);

}

Group_partition_handling::~Group_partition_handling()
{
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
  mysql_mutex_destroy(&trx_termination_aborted_lock);
  mysql_cond_destroy(&trx_termination_aborted_cond);
}

void
Group_partition_handling::update_timeout_on_unreachable(ulong unreachable_timeout)
{
  timeout_on_unreachable= unreachable_timeout;
}

ulong Group_partition_handling::get_timeout_on_unreachable()
{
  return timeout_on_unreachable;
}

bool Group_partition_handling::is_member_on_partition()
{
  return member_in_partition;
}

bool Group_partition_handling::is_partition_handler_running()
{
  return thread_running;
}

bool Group_partition_handling::is_partition_handling_terminated()
{
  return partition_handling_terminated;
}

void Group_partition_handling::kill_transactions_and_leave()
{
  DBUG_ENTER("Group_partition_handling::kill_transactions_and_leave");

  log_message(MY_ERROR_LEVEL,
              "This member could not reach a majority of the members for more "
              "than %ld seconds. The member will now leave the group as instructed "
              "by the group_replication_unreachable_majority_timeout option.",
               timeout_on_unreachable);

  /*
    Suspend the applier for the uncommon case of a network restore happening
    when this termination process is ongoing.
    Don't care if an error is returned because the applier failed.
  */
  applier_module->add_suspension_packet();

  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR);

  bool set_read_mode= false;
  Gcs_operations::enum_leave_state state= gcs_module->leave();

  std::stringstream ss;
  plugin_log_level log_severity= MY_WARNING_LEVEL;
  switch (state)
  {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      ss << "Unable to confirm whether the server has left the group or not. "
            "Check performance_schema.replication_group_members to check group membership information.";
      log_severity= MY_ERROR_LEVEL;
      set_read_mode= true;
      break;
    case Gcs_operations::ALREADY_LEAVING:
      ss << "Skipping leave operation: concurrent attempt to leave the group is on-going."; /* purecov: inspected */
      break; /* purecov: inspected */
    case Gcs_operations::ALREADY_LEFT:
      ss << "Skipping leave operation: member already left the group."; /* purecov: inspected */
      break; /* purecov: inspected */
    case Gcs_operations::NOW_LEAVING:
      set_read_mode= true;
      ss << "The server was automatically set into read only mode after an error was detected.";
      log_severity= MY_ERROR_LEVEL;
      break;
  }
  log_message(log_severity, ss.str().c_str());

  /*
    If true it means:
    1) The plugin is stopping and waiting on some transactions to finish.
       No harm in unblocking them first cutting the stop command time
    2) There was an error in the applier and the plugin will leave the group.
       No problem, both processes will try to kill the transactions and set the
       read mode to true.
  */
  bool already_locked= shared_stop_write_lock->try_grab_write_lock();

  //kill pending transactions
  blocked_transaction_handler->unblock_waiting_transactions();

  if (!already_locked)
    shared_stop_write_lock->release_write_lock();

  if (set_read_mode)
    enable_server_read_mode(PSESSION_INIT_THREAD);

  DBUG_VOID_RETURN;
}

bool Group_partition_handling::abort_partition_handler_if_running()
{
  DBUG_ENTER("Group_partition_handling::abort_partition_handler_if_running");

  // if someone tried to cancel it, we are no longer in a partition.
  member_in_partition= false;

  /*
    This check is safe to invoke as the start method and abort method are only
    invoked in GCS serialized operations.
  */
  if (thread_running)
    terminate_partition_handler_thread();

  DBUG_RETURN(partition_handling_terminated);
}

int Group_partition_handling::launch_partition_handler_thread()
{
  DBUG_ENTER("Group_partition_handling::launch_partition_handler_thread");

  member_in_partition= true;

  //If the timeout is set to 0 do nothing
  if (!timeout_on_unreachable)
     return 0;

  mysql_mutex_lock(&run_lock);

  partition_handling_aborted= false;

  if(thread_running)
  {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    DBUG_RETURN(0);                /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_group_partition_handler,
                          &partition_trx_handler_pthd,
                          get_connection_attrib(),
                          launch_handler_thread,
                          (void*)this))
  {
    DBUG_RETURN(1); /* purecov: inspected */
  }

  while (!thread_running)
  {
    DBUG_PRINT("sleep",("Waiting for the partition handler thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int Group_partition_handling::terminate_partition_handler_thread()
{
  DBUG_ENTER("Group_partition_handling::terminate_partition_handler_thread");

  mysql_mutex_lock(&run_lock);

  if (!thread_running)
  {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(0);
  }

  mysql_mutex_lock(&trx_termination_aborted_lock);
  partition_handling_aborted= true;
  mysql_cond_broadcast(&trx_termination_aborted_cond);
  mysql_mutex_unlock(&trx_termination_aborted_lock);

  ulong stop_wait_timeout= TRANSACTION_KILL_TIMEOUT;

  while (thread_running)
  {
    DBUG_PRINT("loop", ("killing group replication partition handler thread"));

    struct timespec abstime;
    set_timespec(&abstime, 2);
#ifndef DBUG_OFF
    int error=
#endif
      mysql_cond_timedwait(&run_cond, &run_lock, &abstime);
    if (stop_wait_timeout >= 2)
    {
      stop_wait_timeout= stop_wait_timeout - 2;
    }
      /* purecov: begin inspected */
    else if (thread_running) // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(1);
    }
    /* purecov: inspected */
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!thread_running);

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int Group_partition_handling::partition_thread_handler()
{
  DBUG_ENTER("Group_partition_handling::partition_thread_handler");

  mysql_mutex_lock(&run_lock);
  thread_running= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  struct timespec abstime;
  bool timeout= false;

  longlong timeout_remaining_time= timeout_on_unreachable;

  mysql_mutex_lock(&trx_termination_aborted_lock);
  while (!timeout && !partition_handling_aborted)
  {
    set_timespec(&abstime, 2);
    mysql_cond_timedwait(&trx_termination_aborted_cond,
                         &trx_termination_aborted_lock, &abstime);

    timeout_remaining_time -= 2;
    timeout= (timeout_remaining_time <= 0);
  }

  mysql_mutex_unlock(&trx_termination_aborted_lock);

  if (!partition_handling_aborted)
  {
    partition_handling_terminated= true;
    kill_transactions_and_leave();
  }

  mysql_mutex_lock(&run_lock);
  thread_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}
