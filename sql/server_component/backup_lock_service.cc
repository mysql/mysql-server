/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "components/mysql_server/mysql_backup_lock.h"
#include "current_thd.h"      // current_thd
#include "sql_backup_lock.h"  // acquire_exclusive_backup_lock,
                              // release_backup_lock

void mysql_backup_lock_service_init()
{
  return;
}


DEFINE_BOOL_METHOD(mysql_acquire_backup_lock,
  (MYSQL_THD opaque_thd,
   enum enum_backup_lock_service_lock_kind lock_kind,
   unsigned long lock_timeout))
{
  THD *thd;
  if (opaque_thd)
    thd= static_cast<THD*>(opaque_thd);
  else
    thd= current_thd;

  if (lock_kind == BACKUP_LOCK_SERVICE_DEFAULT)
    return acquire_exclusive_backup_lock(thd, lock_timeout);

  /*
    Return error in case lock_kind has an unexpected value.
    As new kind of lock be added into the enumeration
    enum_backup_lock_service_lock_kind its handling should be added here.
  */
  return true;
}


DEFINE_BOOL_METHOD(mysql_release_backup_lock,
  (MYSQL_THD opaque_thd))
{
  THD *thd;
  if (opaque_thd)
    thd= static_cast<THD*>(opaque_thd);
  else
    thd= current_thd;

  release_backup_lock(thd);

  return false;
}


