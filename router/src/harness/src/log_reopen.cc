/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/log_reopen.h"

#include "dim.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"

IMPORT_LOG_FUNCTIONS()

namespace mysql_harness {

/**
 * notify a "log_reopen" is requested with optional filename for old logfile.
 *
 * @param dst rename old logfile to filename before reopen
 * @throws std::system_error same as std::unique_lock::lock does
 */
void LogReopen::request_reopen(const std::string &dst) {
  thread_state_.serialize_with_cv([dst](auto &st, auto &cv) {
    if (st.is_active()) return;  // ignore request if already running

    st.state(LogReopen::ThreadState::State::REQUESTED);
    st.destination(dst);

    cv.notify_one();
  });
}

/**
 * check reopen completed
 */
bool LogReopen::completed() const {
  return thread_state_([](const auto &st) { return st.is_completed(); });
}

/**
 * get last log reopen error
 */
std::string LogReopen::get_last_error() const {
  return thread_state_(
      [](const auto &st) -> std::string { return st.errmsg(); });
}

void LogReopen::set_complete_callback(LogReopen::reopen_callback cb) {
  complete_callback_([cb](auto &cb_) { cb_ = cb; });
}

/**
 * stop the log_reopen_thread_function.
 */
void LogReopen::stop() {
  thread_state_.serialize_with_cv([](auto &ts, auto &cv) {
    ts.state(ThreadState::State::SHUTDOWN);
    cv.notify_one();
  });
}

/**
 * join the log_reopen thread.
 */
void LogReopen::join() { reopen_thr_.join(); }

/**
 * destruct the thread.
 */
LogReopen::~LogReopen() {
  // if it didn't throw in the constructor, it is joinable and we have to
  // signal its shutdown
  if (reopen_thr_.joinable()) {
    try {
      // if stop throws ... the join will block
      stop();

      // if join throws, log it and expect std::thread::~thread to call
      // std::terminate
      join();
    } catch (const std::exception &e) {
      try {
        log_error("~LogReopenThread failed to join its thread: %s", e.what());
      } catch (...) {
        // ignore it, we did our best to tell the user why std::terminate will
        // be called in a bit
      }
    }
  }
}

void LogReopen::main_loop(LogReopen *self) {
  auto &logging_registry = mysql_harness::DIM::instance().get_LoggingRegistry();

  // wait until either shutdown or reopen is signalled.
  while (true) {
    auto is_shutdown{false};
    std::string destination;

    self->thread_state_.wait([&is_shutdown, &destination](auto &st) {
      switch (st.state()) {
        case ThreadState::State::SHUTDOWN:
          is_shutdown = true;
          return true;
        case ThreadState::State::NONE:
        case ThreadState::State::ACTIVE:
          return false;  // continue waiting.
        case ThreadState::State::REQUESTED:
          st.state(ThreadState::State::ACTIVE);
          st.errmsg({});

          destination = st.destination();
          st.destination({});
          return true;
      }

      harness_assert_this_should_not_execute();
      return false;
    });

    if (is_shutdown) break;

    std::string errmsg;
    // we do not lock while doing the log rotation,
    // it can take long time and we can't block the requestor which can run
    // in the context of signal handler
    try {
      logging_registry.flush_all_loggers(destination);
    } catch (const std::exception &e) {
      // leave actions on error to the defined callback function
      errmsg = e.what();

      self->thread_state_([&errmsg](auto &st) { st.errmsg(errmsg); });
    }

    // trigger the completion callback once mutex is not locked
    self->complete_callback_([&errmsg](auto cb) { cb(errmsg); });

    self->thread_state_([](auto &st) {
      if (st.state() == ThreadState::State::ACTIVE) {
        st.state(ThreadState::State::NONE);
      }
    });
  }
}

}  // namespace mysql_harness
