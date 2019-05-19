/*****************************************************************************

Copyright (c) 2000, 2019, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file srv/srv0file.cc
 Service of data file operation.

 Created 1/11/2019 Jianwei Zhao
 *******************************************************/
#include "fil0purge.h"
#include "my_inttypes.h"
#include "sql/mysqld.h"
#include "sql/mysqld_thd_manager.h"
#include "srv0start.h"

#include "srv0file.h"

#ifdef UNIV_PFS_THREAD
/* File purge thread PFS key */
mysql_pfs_key_t srv_file_purge_thread_key;
#endif

#ifdef UNIV_PFS_MUTEX
/* File purge list mutex PFS key */
mysql_pfs_key_t file_purge_list_mutex_key;
#endif

/** Whether enable the data file purge background little by little */
bool srv_data_file_purge = false;

/** Whether unlink the file immediately by purge thread */
bool srv_data_file_purge_immediate = false;

/** Whether purge all when normal shutdown */
bool srv_data_file_purge_all_at_shutdown = false;

/** Time interval (milliseconds) every data file purge operation */
ulong srv_data_file_purge_interval = 100;

/** Max size (MB) every data file purge operation */
ulong srv_data_file_purge_max_size = 512;

/** The directory that purged data file will be removed into */
char *srv_data_file_purge_dir = nullptr;

/** Whether to print data file purge process */
bool srv_print_data_file_purge_process = false;

/** Indicate whether file purge system initted */
static bool file_purge_system_inited = false;

/** Purge thread event condition */
os_event_t file_purge_event;

/** Data file purge system initialize when InnoDB server boots */
void srv_file_purge_init() {
  file_purge_sys = UT_NEW_NOKEY(
      File_purge(Global_THD_manager::reserved_thread_id, server_start_time));

  file_purge_event = os_event_create(0);

  /** If not setting special directory, inherit MySQL datadir directly. */
  if (srv_data_file_purge_dir) {
    os_file_type_t type;
    bool exists;
    bool success = os_file_status(srv_data_file_purge_dir, &exists, &type);

    if (success && exists) {
      /* If directory has existed, set dir directly */
      file_purge_sys->set_dir(srv_data_file_purge_dir);
    } else if ((os_file_create_subdirs_if_needed(srv_data_file_purge_dir) ==
                DB_SUCCESS) &&
               (os_file_create_directory(srv_data_file_purge_dir, false))) {
      /* If not exist, try to create dir */
      file_purge_sys->set_dir(srv_data_file_purge_dir);
    } else {
      /* Defaultly use innodb data dir */
      file_purge_sys->set_dir(MySQL_datadir_path);
    }
  } else { /* srv_data_file_purge_dir is null */
    file_purge_sys->set_dir(MySQL_datadir_path);
  }

  file_purge_system_inited = true;
}

/** Data file purge system destroy when InnoDB server shutdown */
void srv_file_purge_destroy() {
  UT_DELETE(file_purge_sys);
  os_event_destroy(file_purge_event);
  file_purge_system_inited = false;
}

/* Data file purge thread runtime */
void srv_file_purge_thread(void) {
  int64_t sig_count;
  ut_a(file_purge_sys);

loop:
  int truncated =
      file_purge_sys->purge_file(srv_data_file_purge_max_size * 1024 * 1024,
                                 srv_data_file_purge_immediate);

  if (truncated <= 0) {
    sig_count = os_event_reset(file_purge_event);
    os_event_wait_time_low(file_purge_event, 5000000, sig_count);
  } else if (truncated > 0) {
    os_thread_sleep(srv_data_file_purge_interval * 1000);
  }

  if (srv_shutdown_state >= SRV_SHUTDOWN_CLEANUP) goto exit_func;

  goto loop;

exit_func:
  /**
    Purge all renamed tmp data file requirement when shutdown:
      - innodb_fast_shutdown = 0 or 1;
      - innodb_data_file_purge_all_at_shutdown is true;

    It will unlink files regardless of file size.
  */
  if (srv_fast_shutdown < 2 && srv_data_file_purge_all_at_shutdown) {
    ib::info(ER_IB_MSG_FP_CLEANUP, file_purge_sys->length());

    file_purge_sys->purge_all(srv_data_file_purge_max_size * 1024 * 1024, true);

    ib::info(ER_IB_MSG_FP_COMPLETE);
  }
  srv_threads.m_file_purge_thread_active = false;
  return;
}

/** Wakeup the background thread when shutdown */
void srv_wakeup_file_purge_thread() { os_event_set(file_purge_event); }

