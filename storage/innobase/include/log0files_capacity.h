/*****************************************************************************

Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
 @file include/log0files_capacity.h

 Redo log management of capacity.

 *******************************************************/

#ifndef log0files_capacity_h
#define log0files_capacity_h

/* Log_files_dict */
#include "log0files_dict.h"

/* log_t&, lsn_t */
#include "log0types.h"

/* os_offset_t */
#include "os0file.h"

/** Responsible for the redo log capacity computations. Computes size for the
next log file that will be created. Tracks the redo resize operation when the
innodb_redo_log_capacity gets changed. Computes maximum ages for dirty pages,
which are then used by page cleaner coordinator.

@remarks When downsize is started, the limits for ages are decreased, forcing
page cleaners to flush more dirty pages then. File sizes for new redo files
are adjusted accordingly, so they could always be effectively used to hold
the whole existing redo log data (for the given current logical size). */
class Log_files_capacity {
 public:
  /** Initialize on discovered set of redo log files (empty set if new redo
  is being created).
  @param[in]  files                   in-memory dictionary of existing files
  @param[in]  current_logical_size    current logical size of data in redo,
                                      or 0 if new redo is being created
  @param[in]  current_checkpoint_age  current checkpoint age */
  void initialize(const Log_files_dict &files, lsn_t current_logical_size,
                  lsn_t current_checkpoint_age);

  /** Updates all internal limits according to the provided parameters.
  If there are any values outside this class, on which computations of
  limits depend on, they should be explicitly provided here, except the
  server variables (srv_thread_concurrency, srv_redo_log_capacity_used).
  @param[in]  files                   in-memory dictionary of files
  @param[in]  current_logical_size    current logical size of data
                                      in the redo log; it depends
                                      directly on the oldest redo
                                      log consumer
  @param[in]  current_checkpoint_age  current checkpoint age  */
  void update(const Log_files_dict &files, lsn_t current_logical_size,
              lsn_t current_checkpoint_age);

  /** @return true iff resize-down is pending */
  bool is_resizing_down() const;

  /** Provides maximum limitation for space occupied on disk.
  @note This value changes only during calls to @see update or @see initialize.
  @return maximum allowed size on disk, in bytes */
  os_offset_t current_physical_capacity() const;

  /** If a redo downsize is in progress, it is the targeted value for the
  current_physical_capacity() (is equal if there is no resize in progress).
  It is set to srv_redo_log_capacity_used when @see update is called.
  @note This value changes only during calls to @see update or @see initialize.
  @return targeted physical capacity, in bytes */
  os_offset_t target_physical_capacity() const;

  /** Soft limit for logical capacity of the redo log. When the log writer
  exceeds this limitation, all user threads are paused during log_free_check()
  calls and message is emitted to the error log. The log writer can still
  continue to write until it reaches the hard limit for logical capacity
  (value returned by hard_logical_capacity()).
  @note This value changes only during calls to @see update or @see initialize.
  @return limitation for free space in the redo log for user threads */
  lsn_t soft_logical_capacity() const;

  /** Hard limit for logical capacity of the redo log. This limitation includes
  "extra_writer_margin" that belongs to the log writer thread. The log writer
  does not to exceed this limit. If space isn't reclaimed after 1 sec wait,
  it writes only as much as possible or crashes the InnoDB.
  @note This value changes only during calls to @see update or @see initialize.
  @return limitation for free space in the redo log for the log writer thread */
  lsn_t hard_logical_capacity() const;

  /** Once checkpoint age exceeds this value, the flushing of pages starts to
  be adaptive. The adaptive page flushing is becoming more and more aggressive
  in the following range: adaptive_flush_min_age()..adaptive_flush_max_age().
  @note This value changes only during calls to @see update or @see initialize.
  @note Note that it must hold:
  adaptive_flush_min_age() < adaptive_flush_max_age() <=
  soft_logical_capacity().
  @remarks
  The diagram below shows how flushing / checkpointing becomes more aggressive
  when the age of the oldest modified page gets increased:

  adaptive_flush_min_age  adaptive_flush_max_age  aggressive_checkpoint_min_age
         |                        |                     |
  -------!------------------------!---------------------!----------------->age
  regular     adaptive flushing     aggressive flushing   aggr. checkpoints
  @return limitation to start adaptive flushing */
  lsn_t adaptive_flush_min_age() const;

  /** Once checkpoint age exceeds that value, the flushing of pages is the most
  aggressive possible since then. For more details @see adaptive_flush_min_age.
  @note This value changes only during calls to @see update or @see initialize.
  @return limitation to start furious flushing */
  lsn_t adaptive_flush_max_age() const;

  /** Once checkpoint age exceeds that value, the log checkpointer thread keeps
  writing checkpoints aggressively (whatever the progress of last_checkpoint_lsn
  would it make). Before that happens, checkpoints could be written periodically
  (for more details @see adaptive_flush_min_age).
  @note This value changes only during calls to @see update or @see initialize.
  @note It holds: adaptive_flush_max_age() < aggressive_checkpoint_min_age().
  @return limitation to start aggressive checkpointing */
  lsn_t aggressive_checkpoint_min_age() const;

  /** Provides size of the next redo log file that will be created. The initial
  value becomes set during a call to @see initialize. Since then, it changes
  only when innodb_redo_log_capacity is changed, during a call to @see update.
  @note Does not depend on whether the file actually might be created or not.
  It is log_files_governor's responsibility not to exceed the physical capacity.
  @remarks
  The strategy used by the Log_files_capacity, guarantees that next redo log
  file should always be possible to be created. That's because:
  1. The next file size is always chosen as:
        innodb_redo_log_capacity / LOG_N_FILES.
  1. The logical capacity of the redo log is limited to:
        (LOG_N_FILES - 2) / LOG_N_FILES * m_current_physical_capacity.
  2. The m_current_physical_capacity is changed only after resize is finished,
     and the resize is considered finished only when:
      - all redo log files have size <= innodb_redo_log_capacity / LOG_N_FILES,
      - and the logical size of the redo log can fit physical size of
        LOG_N_FILES - 2 redo files, which guarantees that at most
        LOG_N_FILES - 1 redo files will ever need to exist (consider scenario
        in which oldest_lsn is at the very end of the oldest redo files and
        newest_lsn is at the very beginning of the newest redo file if you
        are curious why -2 is there instead of -1).
  @return file size suggested for next log file to create */
  os_offset_t next_file_size() const;

  /** Computes size of a next redo log file that would be chosen for a given
  physical capacity.
  @param[in]  physical_capacity   physical capacity assumed for computation
  @return file size suggested for next log file to create */
  static os_offset_t next_file_size(os_offset_t physical_capacity);

  /** Provides margin which might be used ahead of the newest lsn to create
  a next file earlier if needed (it will be created as unused redo file).
  @note This value changes only during calls to @see update or @see initialize.
  @return the maximum allowed margin ahead of the newest lsn to be reserved */
  lsn_t next_file_earlier_margin() const;

  /** Computes margin which might be used ahead of the newest lsn to create
  a next file earlier if needed (it will be created as unused redo file).
  The computation is done for a given physical capacity.
  @param[in]  physical_capacity   physical capacity assumed for computation
  @return the maximum allowed margin ahead of the newest lsn to be reserved */
  static lsn_t next_file_earlier_margin(os_offset_t physical_capacity);

  /** Computes hard logical capacity, that corresponds to the provided
  soft logical capacity of the redo log (@see soft_logical_capacity()).
  @param[in]  soft_logical_capacity  logical capacity for user threads,
                                     used in log_free_check() calls
  @return hard logical capacity */
  static lsn_t guess_hard_logical_capacity_for_soft(
      lsn_t soft_logical_capacity);

  /** Computes soft logical capacity, that corresponds to the provided
  hard logical capacity of the redo log (@see hard_logical_capacity()).
  @param[in]  hard_logical_capacity  logical capacity for the log writer
  @return soft logical capacity */
  static lsn_t soft_logical_capacity_for_hard(lsn_t hard_logical_capacity);

  /** Computes hard logical capacity, that corresponds to the provided
  physical capacity of the redo log (@see hard_logical_capacity()).
  @param[in]  physical_capacity         physical capacity for the redo log
  @return hard logical capacity */
  static lsn_t hard_logical_capacity_for_physical(
      os_offset_t physical_capacity);

  /** Computes maximum age of dirty pages up to which there is no sync flush
  enforced on page cleaners. This is a smaller value than soft logical capacity,
  because sync flush must be started earlier than log_free_check() calls begin
  to stop user threads.
  @param[in]  soft_logical_capacity   logical capacity for user threads,
                                      used in log_free_check() calls
  @return maximum age of dirty pages before the sync flush is started */
  static lsn_t sync_flush_logical_capacity_for_soft(
      lsn_t soft_logical_capacity);

  /** Computes soft logical capacity, that corresponds to the provided
  maximum age of dirty pages up to which there is no sync flush enforced
  on page cleaners. This is a larger value than the provided maximum age,
  because sync flush must be started earlier than log_free_check() calls
  begin to stop user threads.
  @param[in]  adaptive_flush_max_age   maximum age of dirty page without
                                      sync flush started
  @return soft logical capacity */
  static lsn_t guess_soft_logical_capacity_for_sync_flush(
      lsn_t adaptive_flush_max_age);

 private:
  /** @see m_exposed */
  struct Exposed {
    /** Value returned by @see soft_logical_capacity */
    atomic_lsn_t m_soft_logical_capacity{0};

    /** Value returned by @see hard_logical_capacity */
    atomic_lsn_t m_hard_logical_capacity{0};

    /** Value returned by @see adaptive_flush_min_age */
    atomic_lsn_t m_adaptive_flush_min_age{0};

    /** Value returned by @see adaptive_flush_max_age */
    atomic_lsn_t m_adaptive_flush_max_age{0};

    /** Value returned by @see aggressive_checkpoint_min_age */
    atomic_lsn_t m_agressive_checkpoint_min_age{0};
  };

  /** Cache for values returned by getters in this object, which otherwise
  would need to be computed on-demand. These values do not have impact on
  state updates of this object.
  @note Updated only during calls to @see initialize and @see update. */
  Exposed m_exposed{};

  /** This is limitation for space on disk we are never allowed to exceed.
  This is the guard of disk space - current size of all log files on disk
  is always not greater than this value.
  @note Updated only during calls to @see initialize and @see update. */
  os_offset_t m_current_physical_capacity{0};

  /** Goal we are trying to achieve for m_current_physical_capacity when
  resize operation is in progress, else: equal to m_current_physical_capacity.
  During startup (when srv_is_being_started is true) it stays equal to the
  m_current_physical_capacity (which is then computed for discovered log files).
  After startup, it's set to srv_redo_log_capacity_used by calls to
  @see update. */
  os_offset_t m_target_physical_capacity{0};

  /** Current resize direction. When user decides to resize down the redo log,
  it becomes Log_resize_mode::RESIZING_DOWN until the resize is finished or
  user decides to stop it (providing other capacity). Note, that resize is not
  started during startup (when srv_is_being_started is true).
  @note Updated only during calls to @see initialize and @see update. */
  Log_resize_mode m_resize_mode{Log_resize_mode::NONE};

  /** Cancels current resize operation immediately.
  @remarks If the innodb_redo_log_capacity is changed when there is a previous
  redo resize in progress, the previous resize is first cancelled. */
  void cancel_resize();

  /** Updates m_target_physical_capacity (reading srv_redo_log_capacity_used)
  and possibly starts a new downsize operation. Might also update:
  m_resize_mode, m_current_physical_capacity. */
  void update_target();

  /** Checks if target of the resize is reached, with regards to the criteria
  based on the current logical size of the redo.
  @param[in]  current_logical_size  current logical size of the redo log
  @return true iff the target is reached */
  bool is_target_reached_for_logical_size(lsn_t current_logical_size) const;

  /** Checks if target of the resize is reached, with regards to the criteria
  based on the current physical size of existing log files (excludes unused).
  @param[in]  current_physical_size  total size of existing redo log files,
                                     excluding unused (spare) files
  @return true iff the target is reached */
  bool is_target_reached_for_physical_size(
      os_offset_t current_physical_size) const;

  /** Checks if target of the resize is reached, with regards to the criteria
  based on the largest existing redo file.
  @param[in]  files   in-memory dictionary of existing files
  @return true iff the target is reached */
  bool is_target_reached_for_max_file_size(const Log_files_dict &files) const;

  /** Checks if target of the resize is reached.
  @param[in]  files                 in-memory dictionary of existing files
  @param[in]  current_logical_size  current logical size of the redo log
  @return true iff the target is reached */
  bool is_target_reached_for_resizing_down(const Log_files_dict &files,
                                           lsn_t current_logical_size) const;

  /** Checks if target of the resize is reached with regards to all criteria
  and updates the m_resize_mode, m_current_physical_capacity when that happens
  (marking the resize operation as finished).
  @param[in]  files                 in-memory dictionary of existing files
  @param[in]  current_logical_size  current logical size of the redo log */
  void update_if_target_reached(const Log_files_dict &files,
                                lsn_t current_logical_size);

  /** Updates value of server status variable: innodb_redo_log_resize_status. */
  void update_resize_status_variable();

  /** Updates values of server status variables:
  innodb_redo_log_capacity_resized, innodb_redo_log_logical_size,
  innodb_redo_log_physical_size, innodb_redo_log_resize_status.
  @param[in]  files                 in-memory dictionary of existing files
  @param[in]  current_logical_size  current logical size of the redo log */
  void update_status_variables(const Log_files_dict &files,
                               lsn_t current_logical_size);

  /** Updates cached and exposed values related to the logical redo capacity:
    - @see m_soft_logical_capacity
    - @see m_hard_logical_capacity
    - @see m_agressive_checkpoint_min_age
    - @see m_adaptive_flush_min_age
    - @see m_adaptive_flush_max_age
  @param[in]  current_logical_size  current logical size of the redo log */
  void update_exposed(lsn_t current_logical_size);

  /** Computes suggested value for the current hard logical capacity.
  @remarks This becomes non-trivial when the redo log is being resized down,
  because this method is supposed to follow the checkpoint age then.
  On the other hand, when the redo log is not being resized down, this method
  computes the hard logical capacity by using simple math based on the current
  physical capacity only (ie. ignoring the current checkpoint age).
  @param[in]  current_checkpoint_age   current checkpoint age,
                                       used only when resizing down
  @return suggested value for current hard logical capacity */
  lsn_t get_suggested_hard_logical_capacity(lsn_t current_checkpoint_age) const;
};

/** Retrieves limitations determined by the current state of log.m_capacity.
These values are retrieved atomically (are consistent with each other).
@param[in]   log                       redo log
@param[out]  limit_for_free_check      soft capacity of the redo decreased by
                                       the current free check margin; this is
                                       limit for size of redo until which the
                                       log_free_check calls do not force waits
@param[out]  limit_for_dirty_page_age  limit for the oldest dirty page until
                                       which the async (adaptive) flushing is
                                       not forced to be started (it might be
                                       started if turned on explicitly by the
                                       innodb_adaptive_flushing); note that
                                       computation of this value include doing
                                       the subtraction of the current log free
                                       check margin */
void log_files_capacity_get_limits(const log_t &log,
                                   lsn_t &limit_for_free_check,
                                   lsn_t &limit_for_dirty_page_age);

#endif /* !log0files_capacity_h */
