/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

#include "health_monitor_collector.h"
#include "health_monitor_report_manager.h"
#include "health_monitor_system_vars.h"

#include <string>

namespace hm {

/**
  The Periodic_Reporter has a framework for printing reports to some
  destination. A Report_Definition_Manager will observe changes to a config
  variable that defines the report contents. On some event, the report is
  issued. Such an event can be the expiration of a timer, a signal, etc.

  The Periodic_Reporter will issue reports at specified intervals, as
  indicated by the report definition. Other behavior can be defined in
  subclasses.
 */
class Periodic_Reporter : public Collector {
 public:
  Periodic_Reporter(const char *reporter_name, const char *status_name,
                    const char *instr_name, const char *os_name,
                    const char *description, sv_bool *sysvar_running,
                    sv_char *sysvar_def);
  ~Periodic_Reporter() override = default;
  Periodic_Reporter(const Periodic_Reporter &) = delete;
  Periodic_Reporter &operator=(const Periodic_Reporter &) = delete;
  Periodic_Reporter(Periodic_Reporter &&) = delete;
  Periodic_Reporter &operator=(Periodic_Reporter &&) = delete;

  void pre_process() override;
  void process() override;
  virtual void issue_report();

  void log_resource_status() override;
  std::string format_settings() override;
  bool settings_changed() override;
  void set_params() override;
  bool running() const override;
  int report_deprecated_usage();

  // Default definition of all periodic reports.
  static const char *default_periodic_report;

 private:
  const static uint DEPRECATION_REPORT_SAMPLE_INTERVAL = 3600;
  int deprecation_msg_next = 0;

 protected:
  bool m_settings_changed = false;
  Report_Definition_Manager m_rep_mgr;
  sv_bool *m_sysvar_running = nullptr;
  sv_char *m_sysvar_def = nullptr;
};

}  // namespace hm
