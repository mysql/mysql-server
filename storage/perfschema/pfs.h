/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_H
#define PFS_H

/**
  @file storage/perfschema/pfs.h
  Performance schema instrumentation (declarations).
*/

void record_main_thread_id();

#define HAVE_PSI_1

#include <mysql/components/services/bits/psi_bits.h>
#include <mysql/psi/psi_data_lock.h>

#include "my_thread.h"
#include "my_thread_local.h"

#include <mysql/components/service_implementation.h>

struct PFS_thread;

/**
  Entry point to the performance schema implementation.
  This singleton is used to discover the performance schema services.
*/
extern struct PSI_cond_bootstrap pfs_cond_bootstrap;
extern struct PSI_data_lock_bootstrap pfs_data_lock_bootstrap;
extern struct PSI_error_bootstrap pfs_error_bootstrap;
extern struct PSI_file_bootstrap pfs_file_bootstrap;
extern struct PSI_idle_bootstrap pfs_idle_bootstrap;
extern struct PSI_mdl_bootstrap pfs_mdl_bootstrap;
extern struct PSI_memory_bootstrap pfs_memory_bootstrap;
extern struct PSI_mutex_bootstrap pfs_mutex_bootstrap;
extern struct PSI_rwlock_bootstrap pfs_rwlock_bootstrap;
extern struct PSI_socket_bootstrap pfs_socket_bootstrap;
extern struct PSI_stage_bootstrap pfs_stage_bootstrap;
extern struct PSI_statement_bootstrap pfs_statement_bootstrap;
extern struct PSI_system_bootstrap pfs_system_bootstrap;
extern struct PSI_table_bootstrap pfs_table_bootstrap;
extern struct PSI_thread_bootstrap pfs_thread_bootstrap;
extern struct PSI_transaction_bootstrap pfs_transaction_bootstrap;
extern struct PSI_tls_channel_bootstrap pfs_tls_channel_bootstrap;

/** Performance schema Thread Local Storage.  */
extern thread_local PFS_thread *THR_PFS;

/* Only Innodb so far */
#define COUNT_DATA_LOCK_ENGINES 1

extern PSI_engine_data_lock_inspector *g_data_lock_inspector[];
extern unsigned int g_data_lock_inspector_count;

#endif
