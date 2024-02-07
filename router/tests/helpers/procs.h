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

#ifndef ROUTER_INTEGRATION_TESTS_PROCS_H
#define ROUTER_INTEGRATION_TESTS_PROCS_H

#include "process_manager.h"

namespace integration_tests {

/**
 * a process manager for a test-suite.
 *
 * it is similar to the normal ProcessManager, but changes when
 * logs are dumped:
 *
 * - ProcessManager dumps logs at destruct
 * - this class dumps logs at .clear() which may be called multiple times
 *   before destruct.
 *
 */
class Procs : public ProcessManager {
 public:
  [[nodiscard]] mysql_harness::Path get_origin() const {
    return ProcessManager::get_origin();
  }

  /**
   * shutdown and stop monitoring of processes.
   */
  void clear() {
    shutdown_all();
    ensure_clean_exit();

    if (dump_logs_) dump_all();

    // it will call shutdown again ...
    ProcessManager::clear();
  }

  void shutdown_all() { ProcessManager::shutdown_all(); }

  ~Procs() override { clear(); }

  void dump_logs() { dump_logs_ = true; }

  auto wait_for_exit(
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout) {
    return ProcessManager::wait_for_exit(timeout);
  }

 private:
  bool dump_logs_{false};
};
}  // namespace integration_tests

#endif
