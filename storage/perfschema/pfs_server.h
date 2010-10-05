/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_SERVER_H
#define PFS_SERVER_H

/**
  @file storage/perfschema/pfs_server.h
  Private interface for the server (declarations).
*/

#ifndef PFS_MAX_MUTEX_CLASS
  #define PFS_MAX_MUTEX_CLASS 200
#endif
#ifndef PFS_MAX_MUTEX
  #define PFS_MAX_MUTEX 1000000
#endif
#ifndef PFS_MAX_RWLOCK_CLASS
  #define PFS_MAX_RWLOCK_CLASS 30
#endif
#ifndef PFS_MAX_RWLOCK
  #define PFS_MAX_RWLOCK 1000000
#endif
#ifndef PFS_MAX_COND_CLASS
  #define PFS_MAX_COND_CLASS 80
#endif
#ifndef PFS_MAX_COND
  #define PFS_MAX_COND 1000
#endif
#ifndef PFS_MAX_THREAD_CLASS
  #define PFS_MAX_THREAD_CLASS 50
#endif
#ifndef PFS_MAX_THREAD
  #define PFS_MAX_THREAD 1000
#endif
#ifndef PFS_MAX_FILE_CLASS
  #define PFS_MAX_FILE_CLASS 50
#endif
#ifndef PFS_MAX_FILE
  #define PFS_MAX_FILE 10000
#endif
#ifndef PFS_MAX_FILE_HANDLE
  #define PFS_MAX_FILE_HANDLE 32768
#endif
#ifndef PFS_MAX_TABLE_SHARE
  #define PFS_MAX_TABLE_SHARE 50000
#endif
#ifndef PFS_MAX_TABLE
  #define PFS_MAX_TABLE 100000
#endif
#ifndef PFS_WAITS_HISTORY_SIZE
  #define PFS_WAITS_HISTORY_SIZE 10
#endif
#ifndef PFS_WAITS_HISTORY_LONG_SIZE
  #define PFS_WAITS_HISTORY_LONG_SIZE 10000
#endif

struct PFS_global_param
{
  bool m_enabled;
  ulong m_mutex_class_sizing;
  ulong m_rwlock_class_sizing;
  ulong m_cond_class_sizing;
  ulong m_thread_class_sizing;
  ulong m_table_share_sizing;
  ulong m_file_class_sizing;
  ulong m_mutex_sizing;
  ulong m_rwlock_sizing;
  ulong m_cond_sizing;
  ulong m_thread_sizing;
  ulong m_table_sizing;
  ulong m_file_sizing;
  ulong m_file_handle_sizing;
  ulong m_events_waits_history_sizing;
  ulong m_events_waits_history_long_sizing;
};

extern PFS_global_param pfs_param;

struct PSI_bootstrap*
initialize_performance_schema(const PFS_global_param *param);

void initialize_performance_schema_acl(bool bootstrap);

void check_performance_schema();

void shutdown_performance_schema();

#endif
