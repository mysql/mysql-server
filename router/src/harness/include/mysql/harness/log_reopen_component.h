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

#ifndef MYSQL_HARNESS_LOG_REOPEN_COMPONENT_INCLUDED
#define MYSQL_HARNESS_LOG_REOPEN_COMPONENT_INCLUDED

#include <memory>  // unique_ptr

#include "harness_export.h"
#include "mysql/harness/log_reopen.h"

namespace mysql_harness {

/**
 * component that manages the reopening of logfiles.
 *
 * depends on the logging-registry to have initialized all loggers.
 *
 * As the loggers are plugins, init() must be called after the Loader
 * started all the plugins which can be done by:
 *
 * @code
 * loader->after_all_started([](){
 *   LogReopenComponent::get_instance().init();
 * });
 * @endcode
 *
 * The component should be shut down again after the plugins
 * start to shutdown again.
 *
 * @code
 * loader->after_first_exit([](){
 *   LogReopenComponent::get_instance().reset();
 * });
 * @endcode
 */
class HARNESS_EXPORT LogReopenComponent {
 public:
  static LogReopenComponent &get_instance();

  // disable copy, as we are a single-instance
  LogReopenComponent(LogReopenComponent const &) = delete;
  void operator=(LogReopenComponent const &) = delete;

  // no move either
  LogReopenComponent(LogReopenComponent &&) = delete;
  void operator=(LogReopenComponent &&) = delete;

  /**
   * initialize the log-component.
   *
   * starts LogReopen thread.
   */
  void init() { log_reopen_ = std::make_unique<LogReopen>(); }

  /**
   * forwards pointer deref's to the log_reopen instance.
   */
  LogReopen *operator->() { return log_reopen_.operator->(); }

  /**
   * forwards pointer deref's to the log_reopen instance.
   */
  const LogReopen *operator->() const { return log_reopen_.operator->(); }

  /**
   * checks if the component is initialized.
   */
  operator bool() const { return (bool)log_reopen_; }

  /**
   * shutdown the log-component.
   */
  void reset() { log_reopen_.reset(); }

 private:
  LogReopenComponent() = default;

  std::unique_ptr<LogReopen> log_reopen_;
};

}  // namespace mysql_harness

#endif
