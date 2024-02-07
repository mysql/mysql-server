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

#include "mysql/harness/process_state_component.h"

#include <memory>
#include <mutex>
#include <string>

namespace mysql_harness {

/**
 * request application shutdown.
 *
 * @throws std::system_error same as std::unique_lock::lock does
 */
void ProcessStateComponent::request_application_shutdown(
    const ShutdownPending::Reason reason, const std::string &msg) {
  for (auto &clb : on_shutdown_request_callbacks_) {
    clb(reason, msg);
  }
  on_shutdown_request_callbacks_.clear();

  shutdown_pending_.serialize_with_cv([reason, msg](auto &pending, auto &cv) {
    pending.reason(reason);
    pending.message(msg);

    cv.notify_one();
  });
}

ProcessStateComponent &ProcessStateComponent::get_instance() {
  static ProcessStateComponent instance;

  return instance;
}

std::string to_string(const ShutdownPending::Reason &reason) {
  switch (reason) {
    case ShutdownPending::Reason::REQUESTED:
      return "REQUESTED";
    case ShutdownPending::Reason::FATAL_ERROR:
      return "FATAL_ERROR";
    case ShutdownPending::Reason::NONE:;  // fallthrough
  }

  return "UNKNOWN";
}

}  // namespace mysql_harness
