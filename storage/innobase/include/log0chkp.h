/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file include/log0chkp.h

 Redo log functions related to checkpointing and log free check.

 *******************************************************/

#ifndef log0chkp_h
#define log0chkp_h

#ifndef UNIV_HOTBACKUP

/* log_get_sn */
#include "log0log.h"

/* log.last_checkpoint_lsn */
#include "log0sys.h"

/* log_t&, lsn_t */
#include "log0types.h"

/* srv_threads.*, srv_read_only_mode, ... */
#include "srv0srv.h"

/* sync_check_iterate */
#include "sync0debug.h"

/* sync_allowed_latches, latch_level_t */
#include "sync0types.h"

/**************************************************/ /**

 @name Log - checkpointer thread and checkpointer mutex.

 *******************************************************/

/** @{ */

/** The log checkpointer thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_checkpointer(log_t *log_ptr);

/** Checks if log checkpointer thread is active.
@return true if and only if the log checkpointer thread is active */
inline bool log_checkpointer_is_active() {
  return srv_thread_is_active(srv_threads.m_log_checkpointer);
}

#define log_checkpointer_mutex_enter(log) \
  mutex_enter(&((log).checkpointer_mutex))

#define log_checkpointer_mutex_exit(log) mutex_exit(&((log).checkpointer_mutex))

#define log_checkpointer_mutex_own(log) \
  (mutex_own(&((log).checkpointer_mutex)) || !log_checkpointer_is_active())

/** @} */

/**************************************************/ /**

 @name Log - basic information about checkpoints.

 *******************************************************/

/** @{ */

/** Gets the last checkpoint lsn stored and flushed to disk.
@return last checkpoint lsn */
inline lsn_t log_get_checkpoint_lsn(const log_t &log) {
  return log.last_checkpoint_lsn.load();
}

/** Calculates age of current checkpoint as number of bytes since
last checkpoint. This includes bytes for headers and footers of
all log blocks. The calculation is based on the latest written
checkpoint lsn, and the current lsn, which points to the first
non reserved data byte. Note that the current lsn could not fit
the free space in the log files. This means that the checkpoint
age could potentially be larger than capacity of the log files.
However we do the best effort to avoid such situations, and if
they happen, user threads wait until the space is reclaimed.
@param[in]	log	redo log
@return checkpoint age as number of bytes */
inline lsn_t log_get_checkpoint_age(const log_t &log) {
  const lsn_t last_checkpoint_lsn = log.last_checkpoint_lsn.load();

  const lsn_t current_lsn = log_get_lsn(log);

  if (current_lsn <= last_checkpoint_lsn) {
    /* Writes or reads have been somehow reordered.
    Note that this function does not provide any lock,
    and does not assume any lock existing. Therefore
    the calculated result is already outdated when the
    function is finished. Hence, we might assume that
    this time we calculated age = 0, because checkpoint
    lsn is close to current lsn if such race happened. */
    return 0;
  }

  return current_lsn - last_checkpoint_lsn;
}

/** Provides opposite checkpoint header number to the given checkpoint
header number.
@param[in]  checkpoint_header_no  the given checkpoint header number
@return the opposite checkpoint header number */
Log_checkpoint_header_no log_next_checkpoint_header(
    Log_checkpoint_header_no checkpoint_header_no);

/** Computes lsn up to which sync flush should be done or returns 0
if there is no need to execute sync flush now.
@param[in,out]  log  redo log
@return lsn for which we want to have oldest_lsn >= lsn in each BP,
        or 0 if there is no need for sync flush */
lsn_t log_sync_flush_lsn(log_t &log);

/** @} */

/**************************************************/ /**

 @name Log - requests to make checkpoint.

 *******************************************************/

/** @{ */

/** Requests a fuzzy checkpoint write (for currently available lsn).
@param[in, out]  log   redo log
@param[in]       sync  whether request is sync (function should wait) */
void log_request_checkpoint(log_t &log, bool sync);

/** Requests a checkpoint written in the next log file (not in the one,
to which current log.last_checkpoint_lsn belongs to).
@param[in,out]   log   redo log */
void log_request_checkpoint_in_next_file(log_t &log);

/** Requests a checkpoint at the current lsn.
@param[in,out]   log            redo log
@param[out]      requested_lsn  lsn for which checkpoint was requested, or
                                stays unmodified if it wasn't requested
@return true iff requested (false if checkpoint_lsn was already at that lsn) */
bool log_request_latest_checkpoint(log_t &log, lsn_t &requested_lsn);

/** Make a checkpoint at the current lsn. Reads current lsn and waits
until all dirty pages have been flushed up to that lsn. Afterwards
requests a checkpoint write and waits until it is finished.
@param[in,out]   log   redo log
@return true iff current lsn was greater than last checkpoint lsn */
bool log_make_latest_checkpoint(log_t &log);

/** Make a checkpoint at the current lsn. Reads current lsn and waits
until all dirty pages have been flushed up to that lsn. Afterwards
requests a checkpoint write and waits until it is finished.
@return true iff current lsn was greater than last checkpoint lsn */
bool log_make_latest_checkpoint();

/** Updates the field log.dict_max_allowed_checkpoint_lsn. This is limitation
for lsn at which checkpoint might be written, imposed by cached changes to the
DD table buffer. It is called from DD code.
@param[in,out]  log      redo log
@param[in]      max_lsn  new value for the limitation */
void log_set_dict_max_allowed_checkpoint_lsn(log_t &log, lsn_t max_lsn);

/** @} */

/**************************************************/ /**

 @name Log - concurrency margins.

 *******************************************************/

/** @{ */

/** Computes concurrency margin to be used within log_free_check calls,
for a given redo log capacity (soft_logical_capacity).
@param[in]    log_capacity    redo log capacity (soft)
@param[out]   is_safe         true iff the computed margin wasn't truncated
                              because of too small log_capacity
@return the computed margin */
sn_t log_concurrency_margin(lsn_t log_capacity, bool &is_safe);

/** Updates log.concurrency_margin and log.concurrency_margin_is_safe for the
current capacity of the redo log and current innodb_thread_concurrency value.
@param[in,out]  log   redo log */
void log_update_concurrency_margin(log_t &log);

/** @} */

/**************************************************/ /**

 @name Log - free check waiting.

 *******************************************************/

/** @{ */

/** Waits until there is free space in log files which includes
concurrency margin required for all threads. You should rather
use log_free_check().
@param[in]     log   redo log */
void log_free_check_wait(log_t &log);

/** Provides current margin used in the log_free_check calls.
It is a sum of dict_persist_margin and concurrency_margin.
@param[in]  log   redo log
@return margin that would be used in log_free_check() */
lsn_t log_free_check_margin(const log_t &log);

/** Computes capacity of redo log available until log_free_check()
needs to wait. It uses a provided size of the log_free_check_margin.
@param[in]  log                 redo log
@param[in]  free_check_margin   size of the log_free_check_margin;
                                @see log_free_check_margin(log)
@return lsn capacity up to free_check_wait happens */
lsn_t log_free_check_capacity(const log_t &log, lsn_t free_check_margin);

/** Computes capacity of redo log available until log_free_check()
needs to wait. It calls log_free_check_margin(log) to obtain the
current log_free_check_margin.
@param[in]  log       redo log
@return lsn capacity up to free_check_wait happens */
lsn_t log_free_check_capacity(const log_t &log);

/** Checks if log_free_check() call should better be executed.
@param[in]  log   redo log
@return true iff log_free_check should be executed */
inline bool log_free_check_is_required(const log_t &log) {
  if (srv_read_only_mode) {
    return false;
  }
  const lsn_t lsn = log_get_lsn(log);
  return lsn > log.free_check_limit_lsn.load();
}

/** Checks if log_free_check() call should better be executed.
@return true iff log_free_check should be executed */
inline bool log_free_check_is_required() {
  ut_ad(log_sys != nullptr);
  return log_free_check_is_required(*log_sys);
}

#ifdef UNIV_DEBUG
/** Performs debug checks to validate some of the assumptions. */
void log_free_check_validate();
#endif /* UNIV_DEBUG */

/** Reserves free_check_margin in the redo space for the current thread.
For further details please look at description of @see log_free_check_margin().
@param[in]  log   redo log */
inline void log_free_check(log_t &log) {
  ut_d(log_free_check_validate());

  /** We prefer to wait now for the space in log file, because now
  are not holding any latches of dirty pages. */

  if (log_free_check_is_required(log)) {
    /* We need to wait, because the concurrency margin could be violated
    if we let all threads to go forward after making this check now.

    The waiting procedure is rather unlikely to happen for proper my.cnf.
    Therefore we extracted the code to separate function, to make the
    inlined log_free_check() small. */

    log_free_check_wait(log);
  }
}

/** Checks for free space in the redo log. Must be called when no latches
are held (except those listed as exceptions). Any database operation must
call this before it has produced LOG_CHECKPOINT_FREE_PER_THREAD * UNIV_PAGE_SIZE
bytes of redo log records. That's because that is the margin in redo log we
reserve by calling this function.

@remarks
Checks if lsn corresponding to current log.sn exceeds log.free_check_limit_lsn,
in which case waits (until it does not exceed). This function is called before
starting a mini-transaction, because thread must not hold block latches when
calling this function. It is also important that the caller does NOT hold any
latch, that might be tried to be acquired:
  - by the page cleaner (e.g. page/block latches),
  - or by the log flush process (e.g. file space latches),
  - or by any other thread, which might at that time already hold another
    latch, that could further lead to a similar problem in chain of threads.
For example, suppose a thread holding some latch X, which is neither used by
the page cleaners nor by the log flush process, called log_free_check() and
started to wait for the free space. Another thread, holding block's latch
(which obviously might be needed for the page cleaners) tries to acquire the
latch X. It needs to wait, because X has already been taken. Therefore, the
latched block cannot be flushed. If this block had old modifications
(low oldest_modification), it could effectively prevent any further attempts
to reclaim space in the redo log. The chain of waiting for each other threads
could obviously be even longer than the one in example. Therefore it is very
important not to call log_free_check() if we are holding any latchs which
might exist in any of such chains. As you can see, it is not that easy to see
if log_free_check() might be called. It is not only about direct holding
of block latches, but also such X (or Y acquired by thread holding such X),
could lead to a deadlock.

For sake of simplicity, you should better not keep any latch when calling to
the log_free_check() unless you are really sure about what you are doing. */
inline void log_free_check() {
  ut_ad(log_sys != nullptr);
  log_free_check(*log_sys);
}

/** @} */

/**************************************************/ /**

 @name Log - free check updates.

 *******************************************************/

/** @{ */

/** Updates log.free_check_limit_lsn in the log. The log_limits_mutex
must be acquired before a call (unless srv_is_being_started is true).
@param[in,out]  log   redo log */
void log_update_limits_low(log_t &log);

/** Updates log.dict_persist_margin and recompute free check limit.
@param[in,out]  log     redo log
@param[in]      margin  new value for log.dict_persist_margin */
void log_set_dict_persist_margin(log_t &log, sn_t margin);

/** @} */

/**************************************************/ /**

 @name Log - other functions related to checkpoints.

 *******************************************************/

/** @{ */

/** Writes checkpoint to the file containing the written checkpoint_lsn.
The checkpoint is written to the given checkpoint header. Unless InnoDB
is starting: checkpointer, writer and files mutexes must be acquired
before calling this function.
@param[in,out]  log                     redo log
@param[in]      checkpoint_file_handle  handle to opened file
@param[in]      checkpoint_header_no    checkpoint header to be written
@param[in]      next_checkpoint_lsn     the checkpoint lsn to write
@return DB_SUCCESS or error */
dberr_t log_files_write_checkpoint_low(
    log_t &log, Log_file_handle &checkpoint_file_handle,
    Log_checkpoint_header_no checkpoint_header_no, lsn_t next_checkpoint_lsn);

/** Writes the first data block to the log file using the provided handle
to the opened log file. The block is addressed by the given checkpoint_lsn,
filled with 0x00 and its data length points to checkpoint_lsn inside, making
the block logically empty.
@remarks This is used only during creation of new log files.
@param[in,out] log             redo log
@param[in]     file_handle     handle to the opened log file
@param[in]     checkpoint_lsn  the checkpoint lsn
@param[in]     file_start_lsn  start_lsn of the file
@return DB_SUCCESS or error */
dberr_t log_files_write_first_data_block_low(log_t &log,
                                             Log_file_handle &file_handle,
                                             lsn_t checkpoint_lsn,
                                             lsn_t file_start_lsn);

/** Writes the next checkpoint to the log file, by writing a single
checkpoint header with the checkpoint lsn. Flushes the file after the
write and updates the log.last_checkpoint_lsn.

@remarks Note that two checkpoint headers are used alternately for
consecutive checkpoints. If InnoDB crashed during the write, it would
still have the previous checkpoint info and recovery would work.
@param[in,out]  log    redo log
@param[in]      lsn    writes checkpoint at this lsn
@return DB_SUCCESS or error */
dberr_t log_files_next_checkpoint(log_t &log, lsn_t lsn);

/** @} */

#endif /* !UNIV_HOTBACKUP */

#endif /* !log0chkp_h */
