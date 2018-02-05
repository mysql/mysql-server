/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/plugin_utils.h"

#include "my_inttypes.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/plugin.h"

using std::vector;

Blocked_transaction_handler::Blocked_transaction_handler() {
  mysql_mutex_init(key_GR_LOCK_trx_unlocking, &unblocking_process_lock,
                   MY_MUTEX_INIT_FAST);
}

Blocked_transaction_handler::~Blocked_transaction_handler() {
  mysql_mutex_destroy(&unblocking_process_lock);
}

void Blocked_transaction_handler::unblock_waiting_transactions() {
  mysql_mutex_lock(&unblocking_process_lock);
  vector<my_thread_id> waiting_threads;
  certification_latch->get_all_waiting_keys(waiting_threads);

  if (!waiting_threads.empty()) {
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_UNABLE_TO_CERTIFY_PLUGIN_TRANS);
  }

  vector<my_thread_id>::const_iterator it;
  for (it = waiting_threads.begin(); it != waiting_threads.end(); it++) {
    my_thread_id thread_id = (*it);
    Transaction_termination_ctx transaction_termination_ctx;
    memset(&transaction_termination_ctx, 0,
           sizeof(transaction_termination_ctx));
    transaction_termination_ctx.m_thread_id = thread_id;
    transaction_termination_ctx.m_rollback_transaction = true;
    transaction_termination_ctx.m_generated_gtid = false;
    transaction_termination_ctx.m_sidno = -1;
    transaction_termination_ctx.m_gno = -1;
    if (set_transaction_ctx(transaction_termination_ctx) ||
        certification_latch->releaseTicket(thread_id)) {
      // Nothing much we can do
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_UNBLOCK_CERTIFIED_TRANS); /* purecov: inspected */
    }
  }
  mysql_mutex_unlock(&unblocking_process_lock);
}

void log_primary_member_details() {
  // Special case to display Primary member details in secondary member logs.
  if (local_member_info->in_primary_mode() &&
      (local_member_info->get_role() ==
       Group_member_info::MEMBER_ROLE_SECONDARY)) {
    std::string primary_member_uuid;
    group_member_mgr->get_primary_member_uuid(primary_member_uuid);
    Group_member_info *primary_member_info =
        group_member_mgr->get_group_member_info(primary_member_uuid);
    if (primary_member_info != NULL) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SERVER_WORKING_AS_SECONDARY,
                   primary_member_info->get_hostname().c_str(),
                   primary_member_info->get_port());
      delete primary_member_info;
    }
  }
}
