/*****************************************************************************

Copyright (c) 1995, 2020, Oracle and/or its affiliates. All Rights Reserved.

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

/** Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
 and srv_parse_log_group_home_dirs(). */
void srv_free_paths_and_sizes(void);

/** Adds a slash or a backslash to the end of a string if it is missing
 and the string is not empty.
 @return string which has the separator if the string is not empty */
char *srv_add_path_separator_if_needed(
    char *str); /*!< in: null-terminated character string */
#ifndef UNIV_HOTBACKUP

/** Open an undo tablespace.
@param[in]  undo_space  Undo tablespace
@return DB_SUCCESS or error code */
dberr_t srv_undo_tablespace_open(undo::Tablespace &undo_space);

/** Upgrade undo tablespaces by deleting the old undo tablespaces
referenced by the TRX_SYS page.
@return error code */
dberr_t srv_undo_tablespaces_upgrade();

/** Start InnoDB.
@param[in]	create_new_db		Whether to create a new database
@return DB_SUCCESS or error code */
dberr_t srv_start(bool create_new_db) MY_ATTRIBUTE((warn_unused_result));

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
@param[in]	max_len		filename max length */
void srv_get_encryption_data_filename(dict_table_t *table, char *filename,
                                      ulint max_len);
#endif /* !UNIV_HOTBACKUP */

/** true if the server is being started */
extern bool srv_is_being_started;
/** true if SYS_TABLESPACES is available for lookups */
extern bool srv_sys_tablespaces_open;
/** true if the server is being started, before rolling back any
incomplete transactions */
extern bool srv_startup_is_before_trx_rollback_phase;

/** TRUE if a raw partition is in use */
extern ibool srv_start_raw_disk_in_use;

/** Shutdown state */
enum srv_shutdown_t {
  /** Database running normally. */
  SRV_SHUTDOWN_NONE = 0,

  /** Shutdown has started. Stopping the thread responsible for rollback of
  recovered transactions. In case of slow shutdown, this implies waiting
  for completed rollback of all recovered transactions.
  @remarks Note that user transactions are stopped earlier, when the
  shutdown state is still equal to SRV_SHUTDOWN_NONE (user transactions
  are closed when related connections are closed in close_connections()). */
  SRV_SHUTDOWN_RECOVERY_ROLLBACK,

  /** Stopping threads that might use system transactions or DD objects.
  This is important because we need to ensure that in the next phase no
  undo records could be produced (we will be stopping purge threads).
  After next phase DD is shut down, so also no accesses to DD objects
  are allowed then. List of threads being stopped within this phase:
    - dict_stats thread,
    - fts_optimize thread,
    - ts_alter_encrypt thread.
  The master thread exits its main loop and finishes its first phase
  of shutdown (in which it was allowed to touch DD objects). */
  SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS,

  /** Stopping the purge threads. Before we enter this phase, we have
  the guarantee that no new undo records could be produced. */
  SRV_SHUTDOWN_PURGE,

  /** Shutting down the DD. */
  SRV_SHUTDOWN_DD,

  /** Stopping remaining InnoDB background threads except:
    - the master thread,
    - redo log threads,
    - page cleaner threads,
    - archiver threads.
  List of threads being stopped within this phase:
    - lock_wait_timeout thread,
    - error_monitor thread,
    - monitor thread,
    - buf_dump thread,
    - buf_resize thread.
  @remarks If your thread might touch DD objects or use system transactions
  it must be stopped within SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS phase.
*/
  SRV_SHUTDOWN_CLEANUP,

  /** Stopping the master thread. */
  SRV_SHUTDOWN_MASTER_STOP,

  /** Once we enter this phase, the page cleaners can clean up the buffer pool
  and exit. The redo log threads write and flush the log buffer and exit after
  the page cleaners (and within this phase). */
  SRV_SHUTDOWN_FLUSH_PHASE,

  /** Last phase after ensuring that all data have been flushed to disk and
  the flushed_lsn has been updated in the header of system tablespace.
  During this phase we close all files and ensure archiver has archived all. */
  SRV_SHUTDOWN_LAST_PHASE,

  /** Exit all threads and free resources. We might reach this phase in one
  of two different ways:
    - after visiting all previous states (usual shutdown),
    - or during startup when we failed and we abort the startup. */
  SRV_SHUTDOWN_EXIT_THREADS
};

/** At a shutdown this value climbs from SRV_SHUTDOWN_NONE
to SRV_SHUTDOWN_EXIT_THREADS. */
extern std::atomic<enum srv_shutdown_t> srv_shutdown_state;

/** Call std::quick_exit(3) */
void srv_fatal_error() MY_ATTRIBUTE((noreturn));

/** Attempt to shutdown all background threads created by InnoDB.
NOTE: Does not guarantee they are actually shut down, only does
the best effort. Changes state of shutdown to SHUTDOWN_EXIT_THREADS,
wakes up the background threads and waits a little bit. It might be
used within startup phase or when fatal error is discovered during
some IO operation. Therefore you must not assume anything related
to the state in which it might be used. */
void srv_shutdown_exit_threads();

/** Checks if all recovered transactions are supposed to be rolled back
before shutdown is ended.
@return value of the check */
bool srv_shutdown_waits_for_rollback_of_recovered_transactions();

/** Allows to safely check value of the current shutdown state.
Note that the current shutdown state might be changed while the
check is being executed, but the check is based on a single load
of the srv_shutdown_state (atomic global variable). */
template <typename F>
bool srv_shutdown_state_matches(F &&f) {
  const auto state = srv_shutdown_state.load();
  return std::forward<F>(f)(state);
}

#endif
