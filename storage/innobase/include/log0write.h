/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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

/**************************************************/ /**
 @file include/log0write.h

 *******************************************************/

#ifndef log0write_h
#define log0write_h

#ifndef UNIV_HOTBACKUP

/* std::memory_order_X */
#include <atomic>

/* log.write_to_file_requests_interval */
#include "log0sys.h"

/* log_t&, lsn_t */
#include "log0types.h"

/* srv_threads */
#include "srv0srv.h"

/**************************************************/ /**

 @name Log - waiting for redo written to disk.

 *******************************************************/

/** @{ */

/** Waits until the redo log is written up to a provided lsn.
@param[in]  log             redo log
@param[in]  lsn             lsn to wait for
@param[in]  flush_to_disk   true: wait until it is flushed
@return statistics about waiting inside */
Wait_stats log_write_up_to(log_t &log, lsn_t lsn, bool flush_to_disk);

/** Total number of redo log flushes (fsyncs) that have been started since
the redo log system (log_sys) became initialized (@see log_sys_init).
@return total number of fsyncs or 0 if the redo log system is uninitialized */
uint64_t log_total_flushes();

/** Number of currently pending redo log flushes (fsyncs in-progress).
@return number of pending fsyncs or 0 if the redo log system is uninitialized */
uint64_t log_pending_flushes();

/** Checks if the redo log writer exited extra margin. To minimize flipping
of log.m_writer_inside_extra_margin, the check assumes the very pessimistic
scenario in which a next write of the log_writer thread, would be executed
up to the current lsn.

Requirement: log.writer_mutex acquired and log.m_writer_inside_extra_margin
is true, before calling this function.

@remarks
This method is supposed to be used by the log_checkpointer thread to detect
situation in which the redo log writer has actually exited the extra_margin,
because of advanced log.last_checkpoint_lsn, but the log_writer thread didn't
notice it because it has not been active since then (e.g. because there is
nothing more to write, ie. log.write_lsn == current lsn).

@param[in,out]  log  redo log */
void log_writer_check_if_exited_extra_margin(log_t &log);

/** @} */

/**************************************************/ /**

 @name Log - the log write threads.

 *******************************************************/

/** @{ */

/** Pause / resume the log writer, the log flusher, the log write notifier
and the log flush notifier threads based on innodb_log_writer_threads value.
@note Calls to this function should be protected externally by some mutex.
The caller innodb_log_writer_threads_update() is protected
by LOCK_global_system_variables in mysqld. */
void log_control_writer_threads(log_t &log);

/** The log writer thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_writer(log_t *log_ptr);

/** The log flusher thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_flusher(log_t *log_ptr);

/** The log flush notifier thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_flush_notifier(log_t *log_ptr);

/** The log write notifier thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_write_notifier(log_t *log_ptr);

/** Validates that the log writer thread is active.
Used only to assert, that the state is correct. */
void log_writer_thread_active_validate();

/** Validates that the log writer, flusher threads are active.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_background_write_threads_active_validate(const log_t &log);

#define log_flusher_mutex_enter(log) mutex_enter(&((log).flusher_mutex))

#define log_flusher_mutex_enter_nowait(log) \
  mutex_enter_nowait(&((log).flusher_mutex))

#define log_flusher_mutex_exit(log) mutex_exit(&((log).flusher_mutex))

#define log_flusher_mutex_own(log) \
  (mutex_own(&((log).flusher_mutex)) || !log_flusher_is_active())

#define log_flush_notifier_mutex_enter(log) \
  mutex_enter(&((log).flush_notifier_mutex))

#define log_flush_notifier_mutex_exit(log) \
  mutex_exit(&((log).flush_notifier_mutex))

#define log_flush_notifier_mutex_own(log) \
  (mutex_own(&((log).flush_notifier_mutex)) || !log_flush_notifier_is_active())

#define log_writer_mutex_enter(log) mutex_enter(&((log).writer_mutex))

#define log_writer_mutex_enter_nowait(log) \
  mutex_enter_nowait(&((log).writer_mutex))

#define log_writer_mutex_exit(log) mutex_exit(&((log).writer_mutex))

#define log_writer_mutex_own(log) \
  (mutex_own(&((log).writer_mutex)) || !log_writer_is_active())

#define log_write_notifier_mutex_enter(log) \
  mutex_enter(&((log).write_notifier_mutex))

#define log_write_notifier_mutex_exit(log) \
  mutex_exit(&((log).write_notifier_mutex))

#define log_write_notifier_mutex_own(log) \
  (mutex_own(&((log).write_notifier_mutex)) || !log_write_notifier_is_active())

/** Checks if log writer thread is active.
@return true if and only if the log writer thread is active */
inline bool log_writer_is_active() {
  return srv_thread_is_active(srv_threads.m_log_writer);
}

/** Checks if log write notifier thread is active.
@return true if and only if the log write notifier thread is active */
inline bool log_write_notifier_is_active() {
  return srv_thread_is_active(srv_threads.m_log_write_notifier);
}

/** Checks if log flusher thread is active.
@return true if and only if the log flusher thread is active */
inline bool log_flusher_is_active() {
  return srv_thread_is_active(srv_threads.m_log_flusher);
}

/** Checks if log flush notifier thread is active.
@return true if and only if the log flush notifier thread is active */
inline bool log_flush_notifier_is_active() {
  return srv_thread_is_active(srv_threads.m_log_flush_notifier);
}

/** Checks if requests to write redo log buffer to disk are frequent
(which means that there is at least one request per 1ms in average).
@param[in]  interval  how often in average requests happen
@return true iff requests are considered frequent */
inline bool log_write_to_file_requests_are_frequent(
    std::chrono::microseconds interval) {
  return interval < std::chrono::milliseconds{1};
}

/** Checks if requests to write redo log buffer to disk are frequent
(which means that there is at least one request per 1ms in average).
@param[in]  log  redo log
@return true iff requests are considered frequent */
inline bool log_write_to_file_requests_are_frequent(const log_t &log) {
  return log_write_to_file_requests_are_frequent(
      log.write_to_file_requests_interval.load(std::memory_order_relaxed));
}

/** @} */

#else

#define log_writer_mutex_own(log) true

#endif /* UNIV_HOTBACKUP */

#endif /* !log0write_h */
