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

#ifndef MRS_COMPONENT_SYSTEM_VARS_H
#define MRS_COMPONENT_SYSTEM_VARS_H

#include <cstdint>
#include <mutex>
#include <string>

#include "mysql/components/services/component_sys_var_service.h"

namespace mrs_component {

// Common sysvar layout
struct SysvarBase {
  SysvarBase() = default;

  const char *name = nullptr;
  const char *desc = nullptr;
  int type = 0;
  void *val_ptr = nullptr;
  bool registered = false;

  mysql_sys_var_check_func check_func = nullptr;
  mysql_sys_var_update_func update_func = nullptr;
};

template <typename T>
struct Sysvar : public SysvarBase {
  Sysvar() { val_ptr = reinterpret_cast<void *>(&val); }
  T val{};
  T min{};
  T max{};
  T def{};
};

// Different types of sysvars
using sv_ulong = Sysvar<unsigned long>;
using sv_ulonglong = Sysvar<unsigned long long>;
using sv_char = Sysvar<const char *>;

/**
  Owns and manages system variables exposed by the MRS component.
*/
class SystemVars {
 public:
  struct ConfiguredValues {
    uint64_t http_port;
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_cipher;
    unsigned long io_worker_threads;
  };

  SystemVars() = default;
  ~SystemVars() = default;
  SystemVars(const SystemVars &) = delete;
  SystemVars &operator=(const SystemVars &) = delete;
  SystemVars(SystemVars &&) = delete;
  SystemVars &operator=(SystemVars &&) = delete;

  int initialize_all();
  int register_all();
  int unregister_all();
  int register_system_var(SysvarBase &sysvar, void *args) const;
  int unregister_system_var(SysvarBase &sysvar) const;

  [[nodiscard]] ConfiguredValues get_configured_values() const;
  void apply_http_limits();

 private:
  static int check_http_port(MYSQL_THD, SYS_VAR *, void *new_value,
                             struct st_mysql_value *value);
  static int check_io_worker_threads(MYSQL_THD, SYS_VAR *, void *new_value,
                                     struct st_mysql_value *value);
  static int check_http_max_connections(MYSQL_THD, SYS_VAR *, void *new_value,
                                        struct st_mysql_value *value);
  static int check_http_body_size(MYSQL_THD, SYS_VAR *, void *new_value,
                                  struct st_mysql_value *value);
  static int check_str_var(MYSQL_THD, SYS_VAR *var, void *new_value,
                           struct st_mysql_value *value);
  static void update_ulong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                               const void *checked);
  static void update_ullong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                const void *checked);
  static void update_string_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                const void *checked);
  static void update_http_max_connections(MYSQL_THD, SYS_VAR *, void *val_ptr,
                                          const void *checked);
  static void update_http_max_request_body_size(MYSQL_THD, SYS_VAR *,
                                                void *val_ptr,
                                                const void *checked);
  static void update_http_max_response_body_size(MYSQL_THD, SYS_VAR *,
                                                 void *val_ptr,
                                                 const void *checked);

  const char *m_sysvar_prefix{"mysql_rest_service"};

  mutable std::mutex m_values_mutex;
  sv_ulonglong m_http_port_sysvar;      // mysql_rest_service.http_port;
  sv_char m_ssl_cert_sysvar;            // mysql_rest_service.ssl_cert;
  sv_char m_ssl_key_sysvar;             // mysql_rest_service.ssl_key;
  sv_char m_ssl_cipher_sysvar;          // mysql_rest_service.ssl_cipher;
  sv_ulong m_io_worker_threads_sysvar;  // mysql_rest_service.io_worker_threads;
  sv_ulong m_http_max_connections_sysvar;
  sv_ulonglong m_http_max_request_body_size_sysvar;
  sv_ulonglong m_http_max_response_body_size_sysvar;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_SYSTEM_VARS_H
