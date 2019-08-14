/*****************************************************************************

Copyright (c) 1995, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/srv0start.h
 Starts the Innobase database server

 Created 10/10/1995 Heikki Tuuri
 *******************************************************/

#ifndef srv0start_h
#define srv0start_h

#include "log0types.h"
#include "os0thread-create.h"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */
#include "trx0purge.h"
#include "univ.i"
#include "ut0byte.h"

// Forward declaration
struct dict_table_t;

#ifndef UNIV_DEBUG
#define RECOVERY_CRASH(x) \
  do {                    \
  } while (0)
#else
#define RECOVERY_CRASH(x)                                  \
  do {                                                     \
    if (srv_force_recovery_crash == x) {                   \
      flush_error_log_messages();                          \
      fprintf(stderr, "innodb_force_recovery_crash=%lu\n", \
              srv_force_recovery_crash);                   \
      fflush(stderr);                                      \
      _exit(3);                                            \
    }                                                      \
  } while (0)
#endif /* UNIV_DEBUG */

/** If buffer pool is less than the size,
only one buffer pool instance is used. */
#define BUF_POOL_SIZE_THRESHOLD (1024 * 1024 * 1024)

/** Parse temporary tablespace configuration.
 @return true if ok, false on parse error */
bool srv_parse_temp_data_file_paths_and_sizes(
    char *str); /*!< in/out: the data file path string */
/** Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
 and srv_parse_log_group_home_dirs(). */
void srv_free_paths_and_sizes(void);

/** Adds a slash or a backslash to the end of a string if it is missing
 and the string is not empty.
 @return string which has the separator if the string is not empty */
char *srv_add_path_separator_if_needed(
    char *str); /*!< in: null-terminated character string */
#ifndef UNIV_HOTBACKUP

/** Upgrade undo tablespaces by deleting the old undo tablespaces
referenced by the TRX_SYS page.
@return error code */
dberr_t srv_undo_tablespaces_upgrade();

/** Start InnoDB.
@param[in]	create_new_db		Whether to create a new database
@return DB_SUCCESS or error code */
dberr_t srv_start(bool create_new_db);

/** Fix up an undo tablespace if it was in the process of being truncated
when the server crashed. This is the second call and is done after the DD
is available so now we know the space_name, file_name and previous space_id.
@param[in]  space_name  undo tablespace name
@param[in]  file_name   undo tablespace file name
@param[in]  space_id    undo tablespace ID
@return error code */
dberr_t srv_undo_tablespace_fixup(const char *space_name, const char *file_name,
                                  space_id_t space_id);

/** On a restart, initialize the remaining InnoDB subsystems so that
any tables (including data dictionary tables) can be accessed. */
void srv_dict_recover_on_restart();

/** Start up the InnoDB service threads which are independent of DDL recovery
@param[in]	bootstrap	True if this is in bootstrap */
void srv_start_threads(bool bootstrap);

/** Start the remaining InnoDB service threads which must wait for
complete DD recovery(post the DDL recovery) */
void srv_start_threads_after_ddl_recovery();

/** Shut down all InnoDB background tasks that may look up objects in
the data dictionary. */
void srv_pre_dd_shutdown();

/** Shut down the InnoDB database. */
void srv_shutdown();

/** Shut down all InnoDB background threads. */
void srv_shutdown_all_bg_threads();

/** Start purge threads. During upgrade we start
purge threads early to apply purge. */
void srv_start_purge_threads();

/** If early redo/undo log encryption processing is done.
@return true if it's done. */
bool is_early_redo_undo_encryption_done();

/** Copy the file path component of the physical file to parameter. It will
 copy up to and including the terminating path separator.
 @return number of bytes copied or ULINT_UNDEFINED if destination buffer
         is smaller than the path to be copied. */
ulint srv_path_copy(char *dest,             /*!< out: destination buffer */
                    ulint dest_len,         /*!< in: max bytes to copy */
                    const char *basedir,    /*!< in: base directory */
                    const char *table_name) /*!< in: source table name */
    MY_ATTRIBUTE((warn_unused_result));

/** Get the encryption-data filename from the table name for a
single-table tablespace.
@param[in]	table		table object
@param[out]	filename	filename
@param[in]	max_len		filename max length
@param[in]	convert		convert to lower case */
void srv_get_encryption_data_filename(dict_table_t *table, char *filename,
                                      ulint max_len, bool convert = false);
#endif /* !UNIV_HOTBACKUP */

/** true if the server is being started */
extern bool srv_is_being_started;
/** true if SYS_TABLESPACES is available for lookups */
extern bool srv_sys_tablespaces_open;
/** true if the server is being started, before rolling back any
incomplete transactions */
extern bool srv_startup_is_before_trx_rollback_phase;
#ifdef UNIV_DEBUG
/** true if srv_pre_dd_shutdown() has been completed */
extern bool srv_is_being_shutdown;
#endif /* UNIV_DEBUG */

/** TRUE if a raw partition is in use */
extern ibool srv_start_raw_disk_in_use;

/** Shutdown state */
enum srv_shutdown_t {
  /** Database running normally. */
  SRV_SHUTDOWN_NONE = 0,

  /** Stopping all extra background tasks. This includes the purge threads and
  every other thread in Srv_threads except:
    - master thread,
    - redo log threads,
    - page cleaner threads,
    - archiver threads.
  At this phase the purge threads must be stopped. */
  SRV_SHUTDOWN_CLEANUP,

  /** Stopping the master thread. */
  SRV_SHUTDOWN_MASTER_STOP,

  /** Once we enter this phase the page_cleaners can clean up the buffer pool
  and exit. Redo log threads write and flush the log buffer and exit after
  page cleaners (and within this phase). Then we switch to the LAST_PHASE. */
  SRV_SHUTDOWN_FLUSH_PHASE,

  /** Last phase after ensuring that all data have been flushed to disk and
  the flushed_lsn has been updated in the header of system tablespace.
  During this phase we close all files and ensure archiver has archived all. */
  SRV_SHUTDOWN_LAST_PHASE,

  /** Exit all threads and free resources. */
  SRV_SHUTDOWN_EXIT_THREADS
};

/** At a shutdown this value climbs from SRV_SHUTDOWN_NONE to
SRV_SHUTDOWN_CLEANUP and then to SRV_SHUTDOWN_LAST_PHASE, and so on */
extern std::atomic<enum srv_shutdown_t> srv_shutdown_state;

/** Call exit(3) */
void srv_fatal_error() MY_ATTRIBUTE((noreturn));
/**
Shutdown all background threads created by InnoDB. */
void srv_shutdown_all_bg_threads();

#endif
