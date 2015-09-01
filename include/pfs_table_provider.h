/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_TABLE_PROVIDER_H
#define PFS_TABLE_PROVIDER_H

/**
  @file include/pfs_table_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_TABLE_INTERFACE
#ifdef MYSQL_SERVER
#ifndef EMBEDDED_LIBRARY
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "mysql/psi/psi.h"

#define PSI_TABLE_CALL(M) pfs_ ## M ## _v1

C_MODE_START

PSI_table_share*
pfs_get_table_share_v1(my_bool temporary, struct TABLE_SHARE *share);

void pfs_release_table_share_v1(PSI_table_share* share);

void
pfs_drop_table_share_v1(my_bool temporary,
                        const char *schema_name, int schema_name_length,
                        const char *table_name, int table_name_length);

PSI_table*
pfs_open_table_v1(PSI_table_share *share, const void *identity);

void pfs_unbind_table_v1(PSI_table *table);

PSI_table *
pfs_rebind_table_v1(PSI_table_share *share, const void *identity, PSI_table *table);

void pfs_close_table_v1(struct TABLE_SHARE *server_share, PSI_table *table);

PSI_table_locker*
pfs_start_table_io_wait_v1(PSI_table_locker_state *state,
                           PSI_table *table,
                           PSI_table_io_operation op,
                           uint index,
                           const char *src_file, uint src_line);

PSI_table_locker*
pfs_start_table_lock_wait_v1(PSI_table_locker_state *state,
                             PSI_table *table,
                             PSI_table_lock_operation op,
                             ulong op_flags,
                             const char *src_file, uint src_line);

void pfs_end_table_io_wait_v1(PSI_table_locker* locker, ulonglong numrows);

void pfs_end_table_lock_wait_v1(PSI_table_locker* locker);

void pfs_unlock_table_v1(PSI_table *table);

C_MODE_END

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* EMBEDDED_LIBRARY */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_TABLE_INTERFACE */

#endif

