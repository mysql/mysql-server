/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_LOG_REOPEN_INCLUDED
#define MYSQL_HARNESS_LOG_REOPEN_INCLUDED

#include <functional>
#include <string>
#include <thread>

#include "harness_export.h"
#include "mysql/harness/stdx/monitor.h"

namespace mysql_harness {

class HARNESS_EXPORT LogReopen {
 public:
  using reopen_callback = std::function<void(const std::string &)>;

  class ThreadState {
   public:
    /**
     * request reopen
     *
     * @note Empty dst will cause reopen only, and the old content will not be
     * moved to dst.
     * @note This method uses mutex::try_lock() to avoid blocking the interrupt
     * handler if a signal is received during an already ongoing concurrent
     * reopen. The consequence is that reopen requests are ignored if rotation
     * is already in progress.
     *
     * @param dst filename to use for old log file during reopen
     * @throws std::system_error same as std::unique_lock::lock does
     */
    void request_reopen(const std::string &dst = "");

    /* Log reopen state triplet */
    enum class State { NONE, REQUESTED, ACTIVE, SHUTDOWN };

    /* Check log reopen completed */
    bool is_completed() const { return state_ == State::NONE; }

    /* Check log reopen requested */
    bool is_requested() const { return state_ == State::REQUESTED; }

    /* Check log reopen active */
    bool is_active() const { return state_ == State::ACTIVE; }

    bool is_shutdown() const { return state_ == State::SHUTDOWN; }

    /* Retrieve error from the last reopen */
    std::string errmsg() const { return errmsg_; }
    void errmsg(const std::string &errmsg) { errmsg_ = errmsg; }

    void destination(const std::string &dst) { dst_ = dst; }
    std::string destination() const { return dst_; }

    void state(State st) { state_ = st; }
    State state() const { return state_; }

   private:
    /* The log reopen thread state */
    State state_{State::NONE};

    /* The last error message from the log reopen thread */
    std::string errmsg_;

    /* The destination filename to use for the old logfile during reopen */
    std::string dst_;
  };  // class LogReopenThread

  LogReopen() { reopen_thr_ = std::thread{&LogReopen::main_loop, this}; }

  /**
   * destruct the thread.
   *
   * Same as std::thread it may call std::terminate in case the thread isn't
   * joined yet, but joinable.
   *
   * In case join() fails as best-effort, a log-message is attempted to be
   * written.
   */
  ~LogReopen();

  /**
   * notify a "log_reopen" is requested with optional filename for old logfile.
   *
   * @param dst rename old logfile to filename before reopen
   * @throws std::system_error same as std::unique_lock::lock does
   */
  void request_reopen(const std::string &dst = "");

  /**
   * check reopen completed
   */
  bool completed() const;

  /**
   * get last log reopen error
   */
  std::string get_last_error() const;

  /**
   * Setter for the log reopen thread completion callback function.
   *
   * @param cb Function to call at completion.
   */
  void set_complete_callback(reopen_callback cb);

 private:
  static void main_loop(LogReopen *self);  // thread.

  friend void main_loop(LogReopen *self);

  /**
   * stop the log_reopen_thread_function.
   */
  void stop();

  /**
   * join the log_reopen thread.
   *
   * @throws std::system_error same as std::thread::join
   */
  void join();

  /* The thread handle */
  std::thread reopen_thr_;

  Monitor<reopen_callback> complete_callback_{{}};

  WaitableMonitor<ThreadState> thread_state_{{}};
};

}  // namespace mysql_harness

#endif
