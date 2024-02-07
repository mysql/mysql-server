/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_PROCESS_STATE_COMPONENT_INCLUDED
#define MYSQL_HARNESS_PROCESS_STATE_COMPONENT_INCLUDED

#include <functional>
#include <string>
#include <vector>

#include "harness_export.h"
#include "mysql/harness/stdx/monitor.h"

namespace mysql_harness {
// set when the Router receives a signal to shut down or some fatal error
// condition occurred
class HARNESS_EXPORT ShutdownPending {
 public:
  /**
   * Reason for shutdown.
   */
  enum class Reason { NONE, REQUESTED, FATAL_ERROR };

  [[nodiscard]] std::string message() const { return message_; }
  void message(const std::string &msg) { message_ = msg; }

  [[nodiscard]] Reason reason() const { return reason_; }
  void reason(Reason r) { reason_ = r; }

 private:
  Reason reason_{Reason::NONE};
  std::string message_;
};

std::string HARNESS_EXPORT to_string(const ShutdownPending::Reason &reason);

/**
 * manages the state of the process.
 *
 * allows to shutdown the current process in a
 *
 * - thread safe
 * - waitable
 *
 * way.
 *
 * To be used
 *
 * - by the signal-handler to mark the process for shutdown,
 * - and by the Loader to wait for a shutdown to happen.
 */
class HARNESS_EXPORT ProcessStateComponent {
 public:
  static ProcessStateComponent &get_instance();

  using key_type = std::string;

  // disable copy, as we are a single-instance
  ProcessStateComponent(ProcessStateComponent const &) = delete;
  void operator=(ProcessStateComponent const &) = delete;

  // no move either
  ProcessStateComponent(ProcessStateComponent &&) = delete;
  void operator=(ProcessStateComponent &&) = delete;

  void clear();

  /**
   * request application shutdown.
   *
   * @param reason reason for the shutdown
   * @param msg human readable reason of the shutdown
   * @throws std::system_error same as std::unique_lock::lock does
   */
  void request_application_shutdown(
      const ShutdownPending::Reason reason = ShutdownPending::Reason::REQUESTED,
      const std::string &msg = {});

  using on_shutdown_request_callback = std::function<void(
      ShutdownPending::Reason reason, const std::string &msg)>;
  /**
   * register a callback that will get exectued whenever the component gets the
   * shutdown request
   *
   * @param callback callback to execute when the shutdown is requested
   */
  void register_on_shutdown_request_callback(
      on_shutdown_request_callback callback) {
    on_shutdown_request_callbacks_.push_back(callback);
  }

  /**
   * pending shutdown state.
   *
   * - synchronized
   * - waitable
   */
  WaitableMonitor<ShutdownPending> &shutdown_pending() {
    return shutdown_pending_;
  }

 private:
  ProcessStateComponent() = default;

  WaitableMonitor<ShutdownPending> shutdown_pending_{{}};

  std::vector<on_shutdown_request_callback> on_shutdown_request_callbacks_;
};

}  // namespace mysql_harness

#endif
