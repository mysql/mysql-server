/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tm_status_variables.h"
#include "option_usage.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_required_services.h"

namespace telemetry {

/* Indexed by RUN_LEVEL_BOOT, ... */
static const char *run_level_names[] = {
    "BOOT",      "INSTALL", "DETECT_RESOURCE", "DECODE_SECRET",
    "CONFIGURE", "READY",   "FAILED",          "UNINSTALL"};

static int tm_show_run_level(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR_PTR;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<const char **>(buf);

  auto runlevel = g_run_level.load();
  if (runlevel < RUN_LEVEL_BOOT) {
    runlevel = RUN_LEVEL_BOOT;
  }
  if (runlevel > RUN_LEVEL_UNINSTALL) {
    runlevel = RUN_LEVEL_UNINSTALL;
  }

  *typed_buf = run_level_names[runlevel];
  return 0;
}

static int tm_show_session_refcount(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_LONG;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<long *>(buf);
  *typed_buf = g_session_count.load();
  return 0;
}

static SHOW_VAR status_func_var[] = {
    {"telemetry.run_level", reinterpret_cast<char *>(tm_show_run_level),
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"telemetry.live_sessions",
     reinterpret_cast<char *>(tm_show_session_refcount), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"option_tracker_usage:MySQL Telemetry",
     reinterpret_cast<char *>(&opt_option_tracker_usage_otel_component),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

void register_status_variables() {
  if (statvar_register_srv->register_variable(status_func_var) != 0) {
    log_error("%s: Failed to register status variables.", component_name);
  }
}

void unregister_status_variables() {
  if (statvar_register_srv->unregister_variable(status_func_var) != 0) {
    log_error("%s: Failed to unregister status variables.", component_name);
  }
}

}  // namespace telemetry
