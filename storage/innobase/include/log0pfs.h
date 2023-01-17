/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef log0pfs_h
#define log0pfs_h

/**************************************************/ /**
 @file include/log0pfs.h

 Redo log functions related to PFS tables for redo log.

 *******************************************************/

#include <mysql/components/service.h>

/** Acquire services required for redo log PFS tables.
@param[in,out]  reg_srv   registry which allows to acquire services
@retval true  success
@retval false error */
bool log_pfs_acquire_services(SERVICE_TYPE(registry) * reg_srv);

/** Release services that have been acquired for redo log PFS tables.
@param[in,out]  reg_srv   registry which was used to acquire the services */
void log_pfs_release_services(SERVICE_TYPE(registry) * reg_srv);

/** Create redo log PFS tables. Note that log_pfs_acquire_services() had to
be called prior to calling this function. If the log_pfs_acquire_services()
failed, then a call to this function is still allowed, but it will report
an error then.
@remarks
When srv_read_only_mode is true, this function reports success, but does
not create any tables.
@retval true  success
@retval false error */
bool log_pfs_create_tables();

/* Delete redo log PFS tables from in-memory structures. */
void log_pfs_delete_tables();

#endif /* !log0pfs_h */
