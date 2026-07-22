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

#include <filesystem>
#include <string>

#include "mrs_component.h"
#include "mrs_component_log.h"
#include "mrs_component_server_config.h"
#include "mrs_component_system_vars.h"
#include "mysql/components/services/bits/system_variables_bits.h"
#include "mysql/components/services/mysql_runtime_error_service.h"
#include "mysqlrouter/component/http_server_component.h"

namespace mrs_component {

constexpr uint64_t kDefaultHttpPort = 33443;
constexpr uint64_t kMinHttpPort = 1;
constexpr uint64_t kMaxHttpPort = 65535;
constexpr unsigned long kDefaultIoWorkerThreads = 0;
constexpr unsigned long kMinIoWorkerThreads = 0;
constexpr unsigned long kMaxIoWorkerThreads = 1024;
constexpr unsigned long kDefaultHttpMaxConnections = 1024;
constexpr unsigned long kMinHttpMaxConnections = 1;
constexpr unsigned long kMaxHttpMaxConnections = 100000;
constexpr uint64_t kDefaultHttpMaxBodySize = 16 * 1024 * 1024;
constexpr uint64_t kMinHttpMaxBodySize = 0;
constexpr uint64_t kMaxHttpMaxBodySize = 2147483648ULL;

// These component sysvar defaults are captured from the Server during
// component registration. SET ... = DEFAULT restores the captured value; it
// does not reread the corresponding Server sysvar.
static const char *get_ssl_cert_default() {
  const auto &server_configs = mrs_component().server_config();

  static std::string ssl_cert =
      server_configs.m_ssl_cert.empty()
          ? std::string{}
          : (std::filesystem::path(server_configs.m_datadir) /
             server_configs.m_ssl_cert)
                .string();

  return ssl_cert.c_str();
}

static const char *get_ssl_key_default() {
  const auto &server_configs = mrs_component().server_config();

  static std::string ssl_key =
      server_configs.m_ssl_key.empty()
          ? std::string{}
          : (std::filesystem::path(server_configs.m_datadir) /
             server_configs.m_ssl_key)
                .string();

  return ssl_key.c_str();
}

static const char *get_ssl_cipher_default() {
  const auto &server_configs = mrs_component().server_config();

  static std::string ssl_cipher = server_configs.m_ssl_cipher;

  return ssl_cipher.c_str();
}

/**
  Initialize the MySQL Rest Service Component system variables.
*/
int SystemVars::initialize_all() {
  // mysql_rest_service.http_port;
  m_http_port_sysvar.name = "http_port";
  m_http_port_sysvar.desc = "MySQL Rest Service HTTP Port";
  m_http_port_sysvar.type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  m_http_port_sysvar.min = kMinHttpPort;
  m_http_port_sysvar.max = kMaxHttpPort;
  m_http_port_sysvar.def = kDefaultHttpPort;
  m_http_port_sysvar.check_func = check_http_port;
  m_http_port_sysvar.update_func = update_ullong_var;

  // mysql_rest_service.ssl_cert;
  m_ssl_cert_sysvar.name = "ssl_cert";
  m_ssl_cert_sysvar.desc = "SSL certificate filename";
  m_ssl_cert_sysvar.type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  m_ssl_cert_sysvar.def = get_ssl_cert_default();
  m_ssl_cert_sysvar.check_func = check_str_var;
  m_ssl_cert_sysvar.update_func = update_string_var;

  // mysql_rest_service.ssl_key;
  m_ssl_key_sysvar.name = "ssl_key";
  m_ssl_key_sysvar.desc = "SSL key filename";
  m_ssl_key_sysvar.type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  m_ssl_key_sysvar.def = get_ssl_key_default();
  m_ssl_key_sysvar.check_func = check_str_var;
  m_ssl_key_sysvar.update_func = update_string_var;

  // mysql_rest_service.ssl_cipher;
  m_ssl_cipher_sysvar.name = "ssl_cipher";
  m_ssl_cipher_sysvar.desc =
      "TLS 1.2 cipher list for the HTTP listener; TLS 1.3 cipher suites use "
      "OpenSSL defaults";
  m_ssl_cipher_sysvar.type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  m_ssl_cipher_sysvar.def = get_ssl_cipher_default();
  m_ssl_cipher_sysvar.update_func = update_string_var;

  // mysql_rest_service.io_worker_threads;
  m_io_worker_threads_sysvar.name = "io_worker_threads";
  m_io_worker_threads_sysvar.desc =
      "Number of I/O worker threads for the HTTP listener; 0 automatically "
      "uses 25% of the available logical CPUs, rounded up, with a minimum "
      "of one";
  m_io_worker_threads_sysvar.type = PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED;
  m_io_worker_threads_sysvar.min = kMinIoWorkerThreads;
  m_io_worker_threads_sysvar.max = kMaxIoWorkerThreads;
  m_io_worker_threads_sysvar.def = kDefaultIoWorkerThreads;
  m_io_worker_threads_sysvar.check_func = check_io_worker_threads;
  m_io_worker_threads_sysvar.update_func = update_ulong_var;

  // mysql_rest_service.http_max_connections;
  m_http_max_connections_sysvar.name = "http_max_connections";
  m_http_max_connections_sysvar.desc =
      "Maximum number of active HTTP client connections";
  m_http_max_connections_sysvar.type = PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED;
  m_http_max_connections_sysvar.min = kMinHttpMaxConnections;
  m_http_max_connections_sysvar.max = kMaxHttpMaxConnections;
  m_http_max_connections_sysvar.def = kDefaultHttpMaxConnections;
  m_http_max_connections_sysvar.check_func = check_http_max_connections;
  m_http_max_connections_sysvar.update_func = update_http_max_connections;

  // mysql_rest_service.http_max_request_body_size;
  m_http_max_request_body_size_sysvar.name = "http_max_request_body_size";
  m_http_max_request_body_size_sysvar.desc =
      "Maximum HTTP request body size in bytes";
  m_http_max_request_body_size_sysvar.type =
      PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  m_http_max_request_body_size_sysvar.min = kMinHttpMaxBodySize;
  m_http_max_request_body_size_sysvar.max = kMaxHttpMaxBodySize;
  m_http_max_request_body_size_sysvar.def = kDefaultHttpMaxBodySize;
  m_http_max_request_body_size_sysvar.check_func = check_http_body_size;
  m_http_max_request_body_size_sysvar.update_func =
      update_http_max_request_body_size;

  // mysql_rest_service.http_max_response_body_size;
  m_http_max_response_body_size_sysvar.name = "http_max_response_body_size";
  m_http_max_response_body_size_sysvar.desc =
      "Maximum HTTP response body size in bytes";
  m_http_max_response_body_size_sysvar.type =
      PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  m_http_max_response_body_size_sysvar.min = kMinHttpMaxBodySize;
  m_http_max_response_body_size_sysvar.max = kMaxHttpMaxBodySize;
  m_http_max_response_body_size_sysvar.def = kDefaultHttpMaxBodySize;
  m_http_max_response_body_size_sysvar.check_func = check_http_body_size;
  m_http_max_response_body_size_sysvar.update_func =
      update_http_max_response_body_size;

  return 0;
}

/**
  Register the MySQL Rest Service Component system variables with the server
  and set the current values in sysvars.
*/
int SystemVars::register_all() {
  // INTEGRAL_CHECK_ARG(int) int_arg{0, 0, 0, 0};
  using ulonglong = unsigned long long;
  INTEGRAL_CHECK_ARG(ulonglong) ulonglong_arg{0, 0, 0, 0};
  STR_CHECK_ARG(str) str_arg{nullptr};

  // mysql_rest_service.http_port
  ulonglong_arg.def_val = m_http_port_sysvar.def;
  ulonglong_arg.min_val = m_http_port_sysvar.min;
  ulonglong_arg.max_val = m_http_port_sysvar.max;
  if (register_system_var(m_http_port_sysvar,
                          reinterpret_cast<void *>(&ulonglong_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.ssl_cert
  str_arg.def_val = const_cast<char *>(m_ssl_cert_sysvar.def);
  if (register_system_var(m_ssl_cert_sysvar,
                          reinterpret_cast<void *>(&str_arg)) != 0) {
    return 1;
  }

#if !defined(NDEBUG)
  if (debug_keyword_service->lookup_debug_keyword(
          "mrs_component_sysvar_registration_failure") != 0) {
    return 1;
  }
#endif

  // mysql_rest_service.ssl_key
  str_arg.def_val = const_cast<char *>(m_ssl_key_sysvar.def);
  if (register_system_var(m_ssl_key_sysvar,
                          reinterpret_cast<void *>(&str_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.ssl_cipher
  str_arg.def_val = const_cast<char *>(m_ssl_cipher_sysvar.def);
  if (register_system_var(m_ssl_cipher_sysvar,
                          reinterpret_cast<void *>(&str_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.io_worker_threads
  using unsigned_long = unsigned long;
  INTEGRAL_CHECK_ARG(unsigned_long) ulong_arg{0, 0, 0, 0};
  ulong_arg.def_val = m_io_worker_threads_sysvar.def;
  ulong_arg.min_val = m_io_worker_threads_sysvar.min;
  ulong_arg.max_val = m_io_worker_threads_sysvar.max;
  if (register_system_var(m_io_worker_threads_sysvar,
                          reinterpret_cast<void *>(&ulong_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.http_max_connections
  ulong_arg.def_val = m_http_max_connections_sysvar.def;
  ulong_arg.min_val = m_http_max_connections_sysvar.min;
  ulong_arg.max_val = m_http_max_connections_sysvar.max;
  if (register_system_var(m_http_max_connections_sysvar,
                          reinterpret_cast<void *>(&ulong_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.http_max_request_body_size
  ulonglong_arg.def_val = m_http_max_request_body_size_sysvar.def;
  ulonglong_arg.min_val = m_http_max_request_body_size_sysvar.min;
  ulonglong_arg.max_val = m_http_max_request_body_size_sysvar.max;
  if (register_system_var(m_http_max_request_body_size_sysvar,
                          reinterpret_cast<void *>(&ulonglong_arg)) != 0) {
    return 1;
  }

  // mysql_rest_service.http_max_response_body_size
  ulonglong_arg.def_val = m_http_max_response_body_size_sysvar.def;
  ulonglong_arg.min_val = m_http_max_response_body_size_sysvar.min;
  ulonglong_arg.max_val = m_http_max_response_body_size_sysvar.max;
  if (register_system_var(m_http_max_response_body_size_sysvar,
                          reinterpret_cast<void *>(&ulonglong_arg)) != 0) {
    return 1;
  }

  return 0;
}

/**
  Unregister the MySQL Rest Service Component system variables.
*/
int SystemVars::unregister_all() {
  int error = 0;
  error |= unregister_system_var(m_http_port_sysvar);
  error |= unregister_system_var(m_ssl_cert_sysvar);
  error |= unregister_system_var(m_ssl_key_sysvar);
  error |= unregister_system_var(m_ssl_cipher_sysvar);
  error |= unregister_system_var(m_io_worker_threads_sysvar);
  error |= unregister_system_var(m_http_max_connections_sysvar);
  error |= unregister_system_var(m_http_max_request_body_size_sysvar);
  error |= unregister_system_var(m_http_max_response_body_size_sysvar);

  return error;
}

SystemVars::ConfiguredValues SystemVars::get_configured_values() const {
  // The Server frees the old PLUGIN_VAR_MEMALLOC string immediately after the
  // update callback returns. Copy values under the callback mutex to prevent
  // concurrent updates from invalidating them.
  std::lock_guard lock(m_values_mutex);

  return {
      .http_port = static_cast<uint64_t>(m_http_port_sysvar.val),
      .ssl_cert =
          (m_ssl_cert_sysvar.val == nullptr) ? "" : m_ssl_cert_sysvar.val,
      .ssl_key = (m_ssl_key_sysvar.val == nullptr) ? "" : m_ssl_key_sysvar.val,
      .ssl_cipher =
          (m_ssl_cipher_sysvar.val == nullptr) ? "" : m_ssl_cipher_sysvar.val,
      .io_worker_threads = m_io_worker_threads_sysvar.val,
  };
}

void SystemVars::apply_http_limits() {
  // Serialize startup application with update callbacks so a concurrent SET
  // either precedes this snapshot or updates the active server afterward.
  std::lock_guard lock(m_values_mutex);

  auto &http_server = HttpServerComponent::get_instance();
  http_server.set_max_http_connections(m_http_max_connections_sysvar.val);
  http_server.set_max_request_body_size(
      static_cast<uint64_t>(m_http_max_request_body_size_sysvar.val));
  http_server.set_max_response_body_size(
      static_cast<uint64_t>(m_http_max_response_body_size_sysvar.val));
}

/**
  Register one system variable.

  The PERSIST_AS_READ_ONLY flag ensures that persisted or otherwise
  pre-configured system variables are loaded during component initialization
*/
int SystemVars::register_system_var(SysvarBase &sysvar, void *args) const {
  if (sysvar.registered) return 0;

  if (sysvar_register->register_variable(
          m_sysvar_prefix, sysvar.name,
          (sysvar.type | PLUGIN_VAR_PERSIST_AS_READ_ONLY), sysvar.desc,
          sysvar.check_func, sysvar.update_func, args, sysvar.val_ptr) != 0) {
    MRS_LOG_ERROR("Failed to register system variable '%s'", sysvar.name);

    return 1;
  }

  sysvar.registered = true;
  return 0;
}

/**
  Unregister one system variable.
*/
int SystemVars::unregister_system_var(SysvarBase &sysvar) const {
  if (!sysvar.registered) return 0;

  if (sysvar_unregister->unregister_variable(m_sysvar_prefix, sysvar.name) !=
      0) {
    MRS_LOG_ERROR("Failed to unregister system variable '%s'", sysvar.name);

    return 1;
  }

  sysvar.registered = false;
  return 0;
}

static void report_invalid_integer_value(SYS_VAR *var, long long value,
                                         bool is_unsigned,
                                         const char *requirement) {
  const auto invalid_value = is_unsigned
                                 ? std::to_string(static_cast<uint64_t>(value))
                                 : std::to_string(value);
  my_error(ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, MYF(0), var->name,
           invalid_value.c_str(), requirement);
}

int SystemVars::check_str_var(MYSQL_THD, SYS_VAR *var, void *new_value,
                              struct st_mysql_value *value) {
  if (var == nullptr || new_value == nullptr || value == nullptr) return 1;

  int length;

  const char *new_def = value->val_str(value, nullptr, &length);

  if (new_def == nullptr || length == 0) {
    my_error(ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, MYF(0), var->name,
             new_def == nullptr ? "NULL" : new_def,
             "A nonempty value is required.");
    return 1;
  }

  assert(new_def[length] == '\0');

  // If valid, assign new value.
  *static_cast<const char **>(new_value) = new_def;
  return 0;
}

/**
  Check for a valid http port value.
*/
int SystemVars::check_http_port(MYSQL_THD, SYS_VAR *var, void *new_value,
                                struct st_mysql_value *value) {
  long long orig{0};
  if (var == nullptr || new_value == nullptr || value == nullptr) return 1;

  if (value->val_int(value, &orig) != 0) return 1;

  const bool is_unsigned = value->is_unsigned(value) != 0;
  const auto port = static_cast<uint64_t>(orig);
  if ((!is_unsigned && orig <= 0) || port < kMinHttpPort ||
      port > kMaxHttpPort) {
    report_invalid_integer_value(
        var, orig, is_unsigned,
        "The value must be an integer between 1 and 65535.");
    return 1;
  }

  *static_cast<unsigned long long *>(new_value) =
      static_cast<unsigned long long>(port);

  return 0;
}

int SystemVars::check_io_worker_threads(MYSQL_THD, SYS_VAR *var,
                                        void *new_value,
                                        struct st_mysql_value *value) {
  long long orig{0};
  if (var == nullptr || new_value == nullptr || value == nullptr) return 1;

  if (value->val_int(value, &orig) != 0) return 1;

  const bool is_unsigned = value->is_unsigned(value) != 0;
  const auto worker_threads = static_cast<uint64_t>(orig);
  if ((!is_unsigned && orig < 0) || worker_threads > kMaxIoWorkerThreads) {
    report_invalid_integer_value(
        var, orig, is_unsigned,
        "The value must be an integer between 0 and 1024.");
    return 1;
  }

  *static_cast<unsigned long *>(new_value) =
      static_cast<unsigned long>(worker_threads);

  return 0;
}

int SystemVars::check_http_max_connections(MYSQL_THD, SYS_VAR *var,
                                           void *new_value,
                                           struct st_mysql_value *value) {
  long long orig{0};
  if (var == nullptr || new_value == nullptr || value == nullptr) return 1;

  if (value->val_int(value, &orig) != 0) return 1;

  const bool is_unsigned = value->is_unsigned(value) != 0;
  const auto max_connections = static_cast<uint64_t>(orig);
  if ((!is_unsigned && orig <= 0) || max_connections < kMinHttpMaxConnections ||
      max_connections > kMaxHttpMaxConnections) {
    report_invalid_integer_value(
        var, orig, is_unsigned,
        "The value must be an integer between 1 and 100000.");
    return 1;
  }

  *static_cast<unsigned long *>(new_value) =
      static_cast<unsigned long>(max_connections);

  return 0;
}

int SystemVars::check_http_body_size(MYSQL_THD, SYS_VAR *var, void *new_value,
                                     struct st_mysql_value *value) {
  long long orig{0};
  if (var == nullptr || new_value == nullptr || value == nullptr) return 1;

  if (value->val_int(value, &orig) != 0) return 1;

  const bool is_unsigned = value->is_unsigned(value) != 0;
  const auto body_size = static_cast<uint64_t>(orig);
  if ((!is_unsigned && orig < 0) || body_size > kMaxHttpMaxBodySize) {
    report_invalid_integer_value(
        var, orig, is_unsigned,
        "The value must be an integer between 0 and 2147483648.");
    return 1;
  }

  *static_cast<unsigned long long *>(new_value) =
      static_cast<unsigned long long>(body_size);

  return 0;
}

void SystemVars::update_ulong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                  const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  *static_cast<unsigned long *>(val_ptr) =
      *static_cast<const unsigned long *>(checked);
}

/**
  Update an unsigned long long system variable.
*/
void SystemVars::update_ullong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                   const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  *static_cast<unsigned long long *>(val_ptr) =
      *static_cast<const unsigned long long *>(checked);
}

/**
  Update a string system variable.
*/
void SystemVars::update_string_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                   const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  *static_cast<const char **>(val_ptr) =
      *static_cast<const char *const *>(checked);
}

void SystemVars::update_http_max_connections(MYSQL_THD, SYS_VAR *,
                                             void *val_ptr,
                                             const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  const auto value = *static_cast<const unsigned long *>(checked);
  *static_cast<unsigned long *>(val_ptr) = value;
  HttpServerComponent::get_instance().set_max_http_connections(value);
}

void SystemVars::update_http_max_request_body_size(MYSQL_THD, SYS_VAR *,
                                                   void *val_ptr,
                                                   const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  const auto value = *static_cast<const unsigned long long *>(checked);
  *static_cast<unsigned long long *>(val_ptr) = value;
  HttpServerComponent::get_instance().set_max_request_body_size(
      static_cast<uint64_t>(value));
}

void SystemVars::update_http_max_response_body_size(MYSQL_THD, SYS_VAR *,
                                                    void *val_ptr,
                                                    const void *checked) {
  auto &system_vars = mrs_component().system_vars();
  std::lock_guard lock(system_vars.m_values_mutex);

  const auto value = *static_cast<const unsigned long long *>(checked);
  *static_cast<unsigned long long *>(val_ptr) = value;
  HttpServerComponent::get_instance().set_max_response_body_size(
      static_cast<uint64_t>(value));
}

}  // namespace mrs_component
