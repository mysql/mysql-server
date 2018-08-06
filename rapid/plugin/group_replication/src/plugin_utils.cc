/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin_utils.h"
#include "plugin.h"

using std::vector;

Blocked_transaction_handler::Blocked_transaction_handler()
{
  mysql_mutex_init(key_GR_LOCK_trx_unlocking, &unblocking_process_lock, MY_MUTEX_INIT_FAST);
}

Blocked_transaction_handler::~Blocked_transaction_handler()
{
  mysql_mutex_destroy(&unblocking_process_lock);
}

void Blocked_transaction_handler::unblock_waiting_transactions()
{
  mysql_mutex_lock(&unblocking_process_lock);
  vector<my_thread_id> waiting_threads;
  certification_latch->get_all_waiting_keys(waiting_threads);

  if (!waiting_threads.empty())
  {
    log_message(MY_WARNING_LEVEL,
                "Due to a plugin error, some transactions can't be certified"
                " and will now rollback.");
  }

  vector<my_thread_id>::const_iterator it;
  for (it= waiting_threads.begin(); it != waiting_threads.end(); it++)
  {
    my_thread_id thread_id= (*it);
    Transaction_termination_ctx transaction_termination_ctx;
    memset(&transaction_termination_ctx,
           0, sizeof(transaction_termination_ctx));
    transaction_termination_ctx.m_thread_id= thread_id;
    transaction_termination_ctx.m_rollback_transaction= TRUE;
    transaction_termination_ctx.m_generated_gtid= FALSE;
    transaction_termination_ctx.m_sidno= -1;
    transaction_termination_ctx.m_gno= -1;
    if (set_transaction_ctx(transaction_termination_ctx) ||
        certification_latch->releaseTicket(thread_id))
    {
      //Nothing much we can do
      log_message(MY_ERROR_LEVEL,
                 "Error when trying to unblock non certified transactions."
                 " Check for consistency errors when restarting the service"); /* purecov: inspected */
    }
  }
  mysql_mutex_unlock(&unblocking_process_lock);
}

void log_primary_member_details()
{
  // Special case to display Primary member details in secondary member logs.
  if (local_member_info->in_primary_mode() &&
      (local_member_info->get_role() == Group_member_info::MEMBER_ROLE_SECONDARY))
  {
    std::string primary_member_uuid;
    group_member_mgr->get_primary_member_uuid(primary_member_uuid);
    Group_member_info* primary_member_info=
                 group_member_mgr->get_group_member_info(primary_member_uuid);
    if (primary_member_info != NULL)
    {
      log_message(MY_INFORMATION_LEVEL,
                  "This server is working as secondary member with primary "
                  "member address %s:%u.",
                  primary_member_info->get_hostname().c_str(),
                  primary_member_info->get_port());
      delete primary_member_info;
    }
  }
}

void abort_plugin_process(const char *message)
{
  log_message(MY_ERROR_LEVEL, "The plugin encountered a critical error and will abort: %s", message);
#if !defined(DBUG_OFF)
  DBUG_SUICIDE();
#endif
  abort();
}
