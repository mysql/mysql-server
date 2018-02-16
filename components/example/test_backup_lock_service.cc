/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <components/mysql_server/mysql_backup_lock.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/backup_lock_service.h>

#include <stdio.h>

extern REQUIRES_SERVICE_PLACEHOLDER(mysql_backup_lock);

/**
  Initialization method for Component.

  @return Operation status
    @retval 0  Success
    @retval !=0  Failure
*/

mysql_service_status_t test_backup_lock_service_init() {
  return mysql_service_mysql_backup_lock->acquire(
      nullptr, BACKUP_LOCK_SERVICE_DEFAULT, 100);
}

/**
  De-initialization method for Component.

  @return Operation status
    @retval 0  Success
    @retval !=0  Failure
*/

mysql_service_status_t test_backup_lock_service_deinit() {
  return mysql_service_mysql_backup_lock->release(nullptr);
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_backup_lock_service)
END_COMPONENT_PROVIDES();

REQUIRES_SERVICE_PLACEHOLDER(mysql_backup_lock);

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_backup_lock_service)
REQUIRES_SERVICE(mysql_backup_lock), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_backup_lock_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_backup_lock_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_backup_lock_service, "mysql:test_backup_lock_service")
test_backup_lock_service_init,
    test_backup_lock_service_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_backup_lock_service)
    END_DECLARE_LIBRARY_COMPONENTS
