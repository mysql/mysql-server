/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/** @file include/srv0shutdown.h
 Shutdowns the Innobase database server

 *******************************************************/

#ifndef srv0shutdown_h
#define srv0shutdown_h

#include "my_compiler.h"
#include "univ.i"

/** Shut down all InnoDB background tasks that may look up objects in
the data dictionary. */
void srv_pre_dd_shutdown();

/** Shut down the InnoDB database. */
void srv_shutdown();

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
[[noreturn]] void srv_fatal_error();

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
