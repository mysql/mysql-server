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

#include "health_monitor_periodic_reporter.h"
#include "health_monitor_system_vars.h"

namespace hm {

/**
  The Triggered_Reporter overrides run_process to react only to signals.
  When signaled, the Triggered_Reporter will issue the report according
  to the definition.
 */
class Triggered_Reporter : public Periodic_Reporter {
 public:
  Triggered_Reporter(const char *reporter_name, const char *status_name,
                     const char *instr_name, const char *os_name,
                     const char *description, sv_bool *sysvar_running,
                     sv_char *sysvar_def);
  void run_process() override;
  void issue_report() override;
  // Default definition of the report triggered when approaching OOM.
  static const char *default_memory_report;
};

}  // namespace hm
