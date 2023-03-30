/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/gcs_view_modification_notifier.h"

#include <time.h>

#include "my_dbug.h"
#include "my_systime.h"
#include "mysql/psi/mysql_cond.h"
#include "plugin/group_replication/include/plugin_psi.h"

Plugin_gcs_view_modification_notifier::Plugin_gcs_view_modification_notifier()
    : view_changing(false), cancelled_view_change(false), error(0) {
  mysql_cond_init(key_GR_COND_view_modification_wait, &wait_for_view_cond);
  mysql_mutex_init(key_GR_LOCK_view_modification_wait, &wait_for_view_mutex,
                   MY_MUTEX_INIT_FAST);
}

Plugin_gcs_view_modification_notifier::
    ~Plugin_gcs_view_modification_notifier() {
  mysql_mutex_destroy(&wait_for_view_mutex);
  mysql_cond_destroy(&wait_for_view_cond);
}

void Plugin_gcs_view_modification_notifier::start_view_modification() {
  mysql_mutex_lock(&wait_for_view_mutex);
  view_changing = true;
  cancelled_view_change = false;
  error = 0;
  mysql_mutex_unlock(&wait_for_view_mutex);
}

bool Plugin_gcs_view_modification_notifier::is_view_modification_ongoing() {
  mysql_mutex_lock(&wait_for_view_mutex);
  bool result = view_changing;
  mysql_mutex_unlock(&wait_for_view_mutex);
  return result;
}

void Plugin_gcs_view_modification_notifier::end_view_modification() {
  mysql_mutex_lock(&wait_for_view_mutex);
  view_changing = false;
  mysql_cond_broadcast(&wait_for_view_cond);
  mysql_mutex_unlock(&wait_for_view_mutex);
}

void Plugin_gcs_view_modification_notifier::cancel_view_modification(
    int errnr) {
  mysql_mutex_lock(&wait_for_view_mutex);
  view_changing = false;
  cancelled_view_change = true;
  this->error = errnr;
  mysql_cond_broadcast(&wait_for_view_cond);
  mysql_mutex_unlock(&wait_for_view_mutex);
}

bool Plugin_gcs_view_modification_notifier::is_cancelled() {
  assert(view_changing == false);
  return cancelled_view_change;
}

bool Plugin_gcs_view_modification_notifier::wait_for_view_modification(
    long timeout) {
  int result = 0;

  mysql_mutex_lock(&wait_for_view_mutex);

  DBUG_EXECUTE_IF("group_replication_skip_wait_for_view_modification",
                  { view_changing = false; };);

  while (view_changing && !cancelled_view_change) {
    struct timespec ts;
    set_timespec(&ts, timeout);
    result =
        mysql_cond_timedwait(&wait_for_view_cond, &wait_for_view_mutex, &ts);

    if (result != 0)  // It means that it broke by timeout or an error.
    {
      view_changing = false;
      break;
    }
  }

  DBUG_EXECUTE_IF("group_replication_force_view_modification_timeout",
                  { result = 1; };);
  if (result != 0) error = GROUP_REPLICATION_CONFIGURATION_ERROR;

  mysql_mutex_unlock(&wait_for_view_mutex);

  return (result != 0 || cancelled_view_change);
}

int Plugin_gcs_view_modification_notifier::get_error() {
  assert(view_changing == false);
  return error;
}
