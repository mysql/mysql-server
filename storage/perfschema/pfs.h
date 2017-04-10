/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_H
#define PFS_H

/**
  @file storage/perfschema/pfs.h
  Performance schema instrumentation (declarations).
*/

#define HAVE_PSI_1

#include <my_thread.h>
#include <my_thread_local.h>
#include <mysql/psi/psi_base.h>
#include <mysql/psi/psi_data_lock.h>

/**
  Entry point to the performance schema implementation.
  This singleton is used to discover the performance schema services.
*/
extern struct PSI_thread_bootstrap pfs_thread_bootstrap;
extern struct PSI_mutex_bootstrap pfs_mutex_bootstrap;
extern struct PSI_rwlock_bootstrap pfs_rwlock_bootstrap;
extern struct PSI_cond_bootstrap pfs_cond_bootstrap;
extern struct PSI_file_bootstrap pfs_file_bootstrap;
extern struct PSI_socket_bootstrap pfs_socket_bootstrap;
extern struct PSI_table_bootstrap pfs_table_bootstrap;
extern struct PSI_mdl_bootstrap pfs_mdl_bootstrap;
extern struct PSI_idle_bootstrap pfs_idle_bootstrap;
extern struct PSI_stage_bootstrap pfs_stage_bootstrap;
extern struct PSI_statement_bootstrap pfs_statement_bootstrap;
extern struct PSI_transaction_bootstrap pfs_transaction_bootstrap;
extern struct PSI_memory_bootstrap pfs_memory_bootstrap;
extern struct PSI_error_bootstrap pfs_error_bootstrap;
extern struct PSI_data_lock_bootstrap pfs_data_lock_bootstrap;

/** Performance schema Thread Local Storage key.  */
extern thread_local_key_t THR_PFS;
extern thread_local_key_t THR_PFS_VG;   // global_variables
extern thread_local_key_t THR_PFS_SV;   // session_variables
extern thread_local_key_t THR_PFS_VBT;  // variables_by_thread
extern thread_local_key_t THR_PFS_SG;   // global_status
extern thread_local_key_t THR_PFS_SS;   // session_status
extern thread_local_key_t THR_PFS_SBT;  // status_by_thread
extern thread_local_key_t THR_PFS_SBU;  // status_by_user
extern thread_local_key_t THR_PFS_SBH;  // status_by_host
extern thread_local_key_t THR_PFS_SBA;  // status_by_account

/** True when @c THR_PFS and all other Performance Schema TLS keys are
 * initialized. */
extern bool THR_PFS_initialized;

/* Only Innodb so far */
#define COUNT_DATA_LOCK_ENGINES 1

extern PSI_engine_data_lock_inspector *g_data_lock_inspector[];
extern unsigned int g_data_lock_inspector_count;

#endif
