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

#include "plugin/group_replication/include/hold_transactions.h"
#include <mysqld_error.h>
#include "plugin/group_replication/include/plugin.h"

Hold_transactions::Hold_transactions() {
  // primary failover safety mutex and condition variable
  mysql_mutex_init(key_GR_LOCK_primary_promotion_policy,
                   &primary_promotion_policy_mutex, MY_MUTEX_INIT_FAST);

  mysql_cond_init(key_GR_COND_primary_promotion_policy,
                  &primary_promotion_policy_condition);
}

Hold_transactions::~Hold_transactions() {
  mysql_mutex_destroy(&primary_promotion_policy_mutex);
  mysql_cond_destroy(&primary_promotion_policy_condition);
}

void Hold_transactions::enable() {
  {
    DBUG_TRACE;

    mysql_mutex_lock(&primary_promotion_policy_mutex);
    applying_backlog = true;
    mysql_mutex_unlock(&primary_promotion_policy_mutex);
  }
}

void Hold_transactions::disable() {
  {
    DBUG_TRACE;

    mysql_mutex_lock(&primary_promotion_policy_mutex);
    applying_backlog = false;
    mysql_cond_broadcast(&primary_promotion_policy_condition);
    mysql_mutex_unlock(&primary_promotion_policy_mutex);
  }
}

int Hold_transactions::wait_until_primary_failover_complete(
    ulong hold_timeout) {
  DBUG_TRACE;

  int ret = 0;
  ulong time_lapsed = 0;
  struct timespec abstime;

  mysql_mutex_lock(&primary_promotion_policy_mutex);

  while (applying_backlog && hold_timeout > time_lapsed &&
         !is_thread_killed() &&
         Group_member_info::MEMBER_ERROR !=
             local_member_info->get_recovery_status()) {
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&primary_promotion_policy_condition,
                         &primary_promotion_policy_mutex, &abstime);
    time_lapsed++;
  }

  if (hold_timeout == time_lapsed)
    ret = ER_GR_HOLD_WAIT_TIMEOUT;
  else if (get_plugin_is_stopping() || is_thread_killed())
    ret = ER_GR_HOLD_KILLED;
  else if (applying_backlog && Group_member_info::MEMBER_ERROR ==
                                   local_member_info->get_recovery_status())
    ret = ER_GR_HOLD_MEMBER_STATUS_ERROR;

  mysql_mutex_unlock(&primary_promotion_policy_mutex);

  return ret;
}
