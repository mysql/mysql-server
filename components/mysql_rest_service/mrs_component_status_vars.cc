/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "mrs_component_status_vars.h"

#include <cstdio>
#include <utility>

#include "mrs_component.h"
#include "scope_guard.h"

namespace mrs_component {

int StatusVars::initialize_all() {
  clear_current_runtime_values();

  m_current_http_port.var.name = "mysql_rest_service.current_http_port";
  m_current_http_port.var.value =
      reinterpret_cast<char *>(show_current_http_port);
  m_current_http_port.var.type = SHOW_FUNC;
  m_current_http_port.var.scope = SHOW_SCOPE_GLOBAL;

  m_current_tls_cert.var.name = "mysql_rest_service.current_tls_cert";
  m_current_tls_cert.var.value =
      reinterpret_cast<char *>(show_current_tls_cert);
  m_current_tls_cert.var.type = SHOW_FUNC;
  m_current_tls_cert.var.scope = SHOW_SCOPE_GLOBAL;

  m_current_tls_key.var.name = "mysql_rest_service.current_tls_key";
  m_current_tls_key.var.value = reinterpret_cast<char *>(show_current_tls_key);
  m_current_tls_key.var.type = SHOW_FUNC;
  m_current_tls_key.var.scope = SHOW_SCOPE_GLOBAL;

  m_current_tls_cipher.var.name = "mysql_rest_service.current_tls_cipher";
  m_current_tls_cipher.var.value =
      reinterpret_cast<char *>(show_current_tls_cipher);
  m_current_tls_cipher.var.type = SHOW_FUNC;
  m_current_tls_cipher.var.scope = SHOW_SCOPE_GLOBAL;

  m_current_io_worker_threads.var.name =
      "mysql_rest_service.current_io_worker_threads";
  m_current_io_worker_threads.var.value =
      reinterpret_cast<char *>(show_current_io_worker_threads);
  m_current_io_worker_threads.var.type = SHOW_FUNC;
  m_current_io_worker_threads.var.scope = SHOW_SCOPE_GLOBAL;

  m_state.var.name = "mysql_rest_service.state";
  m_state.var.value = reinterpret_cast<char *>(show_state);
  m_state.var.type = SHOW_FUNC;
  m_state.var.scope = SHOW_SCOPE_GLOBAL;

  return 0;
}

int StatusVars::register_all() {
  auto rollback = create_scope_guard([this] { (void)unregister_all(); });

  if (register_status_var(m_current_http_port) != 0) return 1;
  if (register_status_var(m_current_tls_cert) != 0) return 1;

#if !defined(NDEBUG)
  if (debug_keyword_service->lookup_debug_keyword(
          "mrs_component_statusvar_registration_failure") != 0) {
    return 1;
  }
#endif

  if (register_status_var(m_current_tls_key) != 0) return 1;
  if (register_status_var(m_current_tls_cipher) != 0) return 1;
  if (register_status_var(m_current_io_worker_threads) != 0) return 1;
  if (register_status_var(m_state) != 0) return 1;

  rollback.release();
  return 0;
}

int StatusVars::unregister_all() {
  int error = 0;
  error |= unregister_status_var(m_current_http_port);
  error |= unregister_status_var(m_current_tls_cert);
  error |= unregister_status_var(m_current_tls_key);
  error |= unregister_status_var(m_current_tls_cipher);
  error |= unregister_status_var(m_current_io_worker_threads);
  error |= unregister_status_var(m_state);

  return error;
}

void StatusVars::publish_current_runtime_values(
    unsigned long http_port, std::string tls_cert, std::string tls_key,
    std::string tls_cipher, unsigned long io_worker_threads) {
  CurrentRuntimeValues values{.http_port = http_port,
                              .tls_cert = std::move(tls_cert),
                              .tls_key = std::move(tls_key),
                              .tls_cipher = std::move(tls_cipher),
                              .io_worker_threads = io_worker_threads};

  std::lock_guard lock(m_current_runtime_values_mutex);
  m_current_runtime_values = std::move(values);
}

void StatusVars::clear_current_runtime_values() {
  std::lock_guard lock(m_current_runtime_values_mutex);
  m_current_runtime_values = {};
}

int StatusVars::show_current_http_port(MYSQL_THD, SHOW_VAR *var, char *buf) {
  auto &status_vars = mrs_component().status_vars();
  std::lock_guard lock(status_vars.m_current_runtime_values_mutex);

  var->type = SHOW_LONG;
  var->value = buf;
  *reinterpret_cast<unsigned long *>(buf) =
      status_vars.m_current_runtime_values.http_port;
  return 0;
}

int StatusVars::show_current_tls_cert(MYSQL_THD, SHOW_VAR *var, char *buf) {
  auto &status_vars = mrs_component().status_vars();
  std::lock_guard lock(status_vars.m_current_runtime_values_mutex);

  var->type = SHOW_CHAR;
  var->value = buf;
  (void)std::snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%s",
                      status_vars.m_current_runtime_values.tls_cert.c_str());
  return 0;
}

int StatusVars::show_current_tls_key(MYSQL_THD, SHOW_VAR *var, char *buf) {
  auto &status_vars = mrs_component().status_vars();
  std::lock_guard lock(status_vars.m_current_runtime_values_mutex);

  var->type = SHOW_CHAR;
  var->value = buf;
  (void)std::snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%s",
                      status_vars.m_current_runtime_values.tls_key.c_str());
  return 0;
}

int StatusVars::show_current_tls_cipher(MYSQL_THD, SHOW_VAR *var, char *buf) {
  auto &status_vars = mrs_component().status_vars();
  std::lock_guard lock(status_vars.m_current_runtime_values_mutex);

  var->type = SHOW_CHAR;
  var->value = buf;
  (void)std::snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%s",
                      status_vars.m_current_runtime_values.tls_cipher.c_str());
  return 0;
}

int StatusVars::show_current_io_worker_threads(MYSQL_THD, SHOW_VAR *var,
                                               char *buf) {
  auto &status_vars = mrs_component().status_vars();
  std::lock_guard lock(status_vars.m_current_runtime_values_mutex);

  var->type = SHOW_LONG;
  var->value = buf;
  *reinterpret_cast<unsigned long *>(buf) =
      status_vars.m_current_runtime_values.io_worker_threads;
  return 0;
}

int StatusVars::show_state(MYSQL_THD, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  // Copy the locked lifecycle snapshot into the Server-provided SHOW buffer;
  // never expose component-owned mutable storage to status-variable readers.
  (void)std::snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%s",
                      mrs_component().backend().lifecycle_state_name());
  return 0;
}

int StatusVars::register_status_var(Statusvar &statusvar) const {
  if (statusvar.registered) return 0;

  SHOW_VAR statvars[]{{statusvar.var}, {m_null_entry.var}};

  if (statusvar_register->register_variable(statvars) != 0) {
    MRS_LOG_ERROR("Failed to register status variable '%s'",
                  statusvar.var.name);
    return 1;
  }

  statusvar.registered = true;
  return 0;
}

int StatusVars::unregister_status_var(Statusvar &statusvar) const {
  if (!statusvar.registered) return 0;

  SHOW_VAR statvars[]{{statusvar.var}, {m_null_entry.var}};

  if (statusvar_register->unregister_variable(statvars) != 0) {
    MRS_LOG_ERROR("Failed to unregister status variable '%s'",
                  statusvar.var.name);
    return 1;
  }

  statusvar.registered = false;
  return 0;
}

}  // namespace mrs_component
