// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_LOG_PREFETCHER_H
#define MYSQL_CSA_LOG_PREFETCHER_H

#include <fstream>
#include <functional>
#include <memory>
#include <optional>

#include "mysql/allocators/allocator.h"
#include "mysql/allocators/memory_resource.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/locking_queue.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/concurrency/thread.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/utils/return_status.h"
#include "sql/binlog.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/storage/relay_log/data_source.h"

namespace mysql::csa {

class Log_prefetcher;
using Log_prefetcher_sptr = std::shared_ptr<Log_prefetcher>;

/// @brief This class starts an asynchronous thread that prefetches consecutive
/// logs recorded in the index file. It contains the following main functions:
/// - dequeue - function to get the next prefetched batch (see Data_source)
/// - open - open a given file, check if file is hot and notify prefetcher
///   to work on it
/// - stop - Gracefully stops the prefetcher and blocks until
///   thread is joined
/// - is_waiting_for_next_file - checks whether prefetcher finished processing
///   a given file and if prefetcher works on the next file or blocked
/// - is_error - Checks whether prefetcher reported an error
class Log_prefetcher {
 public:
  using Mt_key = mysql::concurrency::Mutex_key;
  using Cv_key = mysql::concurrency::Cv_key;
  using Th_key = mysql::concurrency::Thread_key;
  using Mem_key = PSI_memory_key;
  using Memory_allocator = mysql::allocators::Allocator<uint8_t>;
  /// @param log_ptr The MYSQL_BIN_LOG object we use to know from where we need
  /// to read data
  /// @param mt_key_wait Instrumentation key for internal mutex (wait for work)
  /// @param cv_key_wait Instrumentation key for internal cv (wait for work)
  /// @param mt_key_file_move Instrumentation key for internal mutex (wait for
  /// file rotation)
  /// @param cv_key_file_move Instrumentation key for internal cv (wait for file
  /// rotation)
  /// @param key_th_prefetcher Instrumentation key for prefetcher thread
  /// @param key_memory Instrumentation key for allocated memory
  Log_prefetcher(MYSQL_BIN_LOG *log_ptr, Mt_key mt_key_wait = 0,
                 Cv_key cv_key_wait = 0, Mt_key mt_key_file_move = 0,
                 Cv_key cv_key_file_move = 0, Th_key key_th_prefetcher = 0,
                 Mem_key key_memory = 0);
  virtual ~Log_prefetcher();

  Log_prefetcher(const Log_prefetcher &) = delete;
  Log_prefetcher &operator=(const Log_prefetcher &) = delete;
  Log_prefetcher(Log_prefetcher &&) = delete;
  Log_prefetcher &operator=(Log_prefetcher &&) = delete;

  using Data_type = Data_source;
  using Elem_type = Data_source_sptr;

  /// @brief Starts asynchronous thread prefetching data
  void start_prefetcher();

  /// @brief Tries to open a new, gifen file. Prefetcher must stop on cv
  /// before calling this function (see is_waiting_for_next_file).
  /// @return True in case file is inactive and prefetcher will work on it,
  /// false otherwise
  /// @param file_name Starts reading from this file path
  bool open(const char *file_name);

  /// @brief Consumes the next batch of data
  /// @return The next batch of data or empty object in case stopped in the
  /// process
  template <typename P>
  std::optional<Elem_type> dequeue(P &&wait_predicate);

  /// Checks whether prefetcher is stopped
  /// @brief True if stop was requested, false otherwise
  bool is_stopped() const;

  /// @brief Gracefully stops the prefetcher. Blocks waiting for notification
  /// that prefetcher thread is done
  void stop();

  /// @brief Returns true if error occurred
  /// @return True in case error occurred, false otherwise
  bool is_error() const;

  /// @brief Checks whether we reached inactive file after the prev_file
  /// This function takes previous file to check if prefetcher finished rotation
  /// on this file and consecutive file to check if prefetcher opened it and
  /// stopped working until file becomes inactive.
  /// @param prev_file File we know we read all data from (got eof in the
  /// last consumed batch) to check if prefetcher finished rotation on this file
  /// @param next_file Consecutive file we want to open the stream on.
  /// @retval true Prefetcher waits for file_name to become inactive
  /// @retval false Prefetcher does not wait for 'file_name'
  bool is_waiting_for_next_file(const std::string &prev_file,
                                const std::string &next_file);

 private:
  /// Thread type
  using Thread_type = mysql::concurrency::Thread;

  /// @brief Opens new or next file for reading
  /// @param next_file True if reading file after the current one
  /// @return True in case file is inactive and prefetcher will work on it,
  /// false otherwise
  bool open_file(bool next_file);

  /// @brief If currently rotated file is file_name, waits until rotate is done.
  /// If prefetcher moves a different file, function returns immediately.
  /// @param file_name File expected to be moved from.
  void ensure_file_done(const std::string &file_name);

  /// @brief Internal stop flag set to true when stop of the thread is
  /// requested
  std::atomic<bool> m_stopped{false};

  /// Sets internal error (under lock) and internal error message. Later on,
  /// stops the prefetcher.
  /// This function will report any error to error log
  /// @param msg Error message
  void set_error(const char *msg);

  /// Obtains the current file under the lock
  /// @return File currently being prefetched
  std::string get_current_file() const;

  /// Updates the current file and its activity under the lock
  /// @param file_name Update the current file name to this file
  /// @param active True if file_name is an active log
  void update_current_file(const char *file_name, bool active);

  using Locking_queue_type = mysql::concurrency::Locking_queue<Elem_type>;
  using Sync_bounded_queue =
      mysql::concurrency::Sync_bounded_queue<Elem_type,
                                             tune::prefetcher_queue_max_size>;
  using Queue_type =
      std::conditional<tune::csa_prefetcher_simple_queue, Locking_queue_type,
                       Sync_bounded_queue>::type;

  /// The MYSQL_BIN_LOG object we use to know from where we need to read data
  MYSQL_BIN_LOG *m_log;

  /// Queue into which prefetcher puts data batches read from a raw binary
  /// file
  Queue_type m_cache;

  /// Thread that runs prefetching
  Thread_type m_prefetcher;

  /// True in case reading from the active relay log file (used currently by
  /// the receiver thread), protected with m_mt_prefetcher.
  /// Prefetcher can read only from inactive files
  /// Protected by m_mt_prefetcher
  bool m_log_active{true};

  /// @brief Runs a thread that prefetch data from the relay log
  void run_prefetch_thread();

  /// Mutex protecting access to m_tasks
  mutable mysql::concurrency::Mutex m_mt_prefetcher;
  /// Cv used by the scheduler main thread to wait on, when no task is
  /// available or tasks in m_task queue are not read to execute
  mysql::concurrency::Condition_variable m_cv_prefetcher;

  /// Notification atomic for end of execution
  std::atomic<bool> m_end{false};

  /// Mutex protecting access to m_move_file
  mutable mysql::concurrency::Mutex m_mt_move_file;
  /// CV to notify the watcher that prefetcher finished moving
  /// from m_move_file to the next file (see ensure_file_done)
  mysql::concurrency::Condition_variable m_cv_move_file;

  /// Is set to a value if prefetcher initiated file move procedure
  /// Watcher may wait until prefetcher moves to the next file by
  /// using the m_cv_move_file notification cv and associated
  /// m_mt_move_file
  std::string m_move_file{""};

  /// This way we track the actual number of bytes that are cached in the
  /// m_cache
  std::atomic<std::size_t> m_bytes_fetched{0};
  /// Default batch size ~16MB
  std::size_t m_batch_size{tune::prefetcher_batch_size};
  /// The maximum number of bytes we can prefetch
  static constexpr std::size_t max_bytes_fetched{1073741824};

  /// Stream to read from
  std::ifstream m_istream;

  /// Error message if any. Protected by m_mt_prefetcher
  std::string m_error_message{""};

  /// Becomes true in case error has been encountered
  std::atomic<bool> m_is_error{false};

  /// Currently processed log file
  /// Protected by m_mt_prefetcher
  std::string m_current_log_name{""};

  /// Length of the currently opened file
  std::size_t m_current_file_length{0};

  /// Current file offset
  std::size_t m_current_offset{0};

  /// Key for prefetcher thread
  Th_key m_key_th_prefetcher{0};

  /// Memory_resource to handle all allocations.
  Memory_allocator m_allocator;
};

}  // namespace mysql::csa

#include "sql/changestreams/apply/storage/relay_log/log_prefetcher_impl.hpp"

#endif  // MYSQL_CSA_LOG_PREFETCHER_H
