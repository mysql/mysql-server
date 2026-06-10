/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_TASK_CONTROL_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_TASK_CONTROL_H_

#include "mysql/harness/utility/wait_variable.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace helper {

enum State {
  k_stateInitialize,
  k_stateRunning,
  k_stateSuspended,
  k_stateStopped
};

template <typename T>
class TaskControl : public T {
 public:
  template <typename... Args>
  TaskControl(Args &&...args) : T(std::forward<Args>(args)...) {}

  void task_suspend() {
    log_debug("TaskControl::susspend");
    if (state_.exchange({k_stateRunning}, k_stateSuspended)) {
      T::stop();
    }
  }

  void task_resume() {
    log_debug("TaskControl::resume");
    if (state_.exchange({k_stateSuspended}, k_stateRunning)) {
    }
  }

 public:
  void start() override {
    log_debug("TaskControl::start");
    if (state_.exchange(k_stateInitialize, k_stateSuspended)) {
      State fetched;
      bool was_running{false};
      do {
        fetched = state_.wait({k_stateRunning, k_stateStopped});

        if (was_running) {
          log_debug("TaskControl::start - reset()");
          T::reset();
          was_running = false;
        }
        if (fetched == k_stateRunning) {
          was_running = true;
          log_debug("TaskControl::start - start()");
          T::start();
        }

      } while (fetched != k_stateStopped);
    }

    log_debug("TaskControl::start stopping");

    state_.set(k_stateStopped);
  }

  void stop() override {
    if (state_.exchange({k_stateInitialize, k_stateRunning, k_stateSuspended},
                        k_stateStopped)) {
      log_debug("TaskControl::stop");
      T::stop();
    }
  }

 private:
  template <typename StateType>
  using WaitableVariable = mysql_harness::utility::WaitableVariable<StateType>;

  WaitableVariable<State> state_{k_stateInitialize};
};

}  // namespace helper

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_TASK_CONTROL_H_ */
