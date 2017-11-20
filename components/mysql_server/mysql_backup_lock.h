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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_BACKUP_LOCK_INCLUDED
#define MYSQL_BACKUP_LOCK_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/backup_lock_service.h>

/**
  Service API to acquire shared Backup Lock.

  @param opaque_thd    Current thread context.
  @param lock_kind     Kind of lock to acquire - BACKUP_LOCK_SERVICE_DEFAULT
                       or weaker.
  @param lock_timeout  Number of seconds to wait before giving up.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

DEFINE_BOOL_METHOD(mysql_acquire_backup_lock,
  (MYSQL_THD opaque_thd,
   enum enum_backup_lock_service_lock_kind lock_kind,
   unsigned long lock_timeout));


/**
  Service API to release Backup Lock.

  @param opaque_thd    Current thread context.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

DEFINE_BOOL_METHOD(mysql_release_backup_lock,
  (MYSQL_THD opaque_thd));

#endif /* COMPONENTS_MYSQL_SERVER_MYSQL_BACKUP_LOCK_H_ */
