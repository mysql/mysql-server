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

#ifndef MRS_COMPONENT_STATUS_VARS_H
#define MRS_COMPONENT_STATUS_VARS_H

#include <mutex>
#include <string>

#include "mysql/components/services/bits/status_variables_bits.h"

namespace mrs_component {

struct Statusvar {
  Statusvar() = default;

  SHOW_VAR var{.name = nullptr,
               .value = nullptr,
               .type = SHOW_UNDEF,
               .scope = SHOW_SCOPE_UNDEF};
  bool registered{false};
};

/**
  Owns and manages status variables exposed by the MRS component.
*/
class StatusVars {
 public:
  StatusVars() = default;
  ~StatusVars() = default;
  StatusVars(const StatusVars &) = delete;
  StatusVars &operator=(const StatusVars &) = delete;
  StatusVars(StatusVars &&) = delete;
  StatusVars &operator=(StatusVars &&) = delete;

  int initialize_all();
  int register_all();
  int unregister_all();

  void publish_current_runtime_values(unsigned long http_port,
                                      std::string tls_cert, std::string tls_key,
                                      std::string tls_cipher,
                                      unsigned long io_worker_threads);
  void clear_current_runtime_values();

 private:
  struct CurrentRuntimeValues {
    unsigned long http_port{0};
    std::string tls_cert;
    std::string tls_key;
    std::string tls_cipher;
    unsigned long io_worker_threads{0};
  };

  int register_status_var(Statusvar &statusvar) const;
  int unregister_status_var(Statusvar &statusvar) const;

  static int show_current_http_port(MYSQL_THD, SHOW_VAR *var, char *buf);
  static int show_current_tls_cert(MYSQL_THD, SHOW_VAR *var, char *buf);
  static int show_current_tls_key(MYSQL_THD, SHOW_VAR *var, char *buf);
  static int show_current_tls_cipher(MYSQL_THD, SHOW_VAR *var, char *buf);
  static int show_current_io_worker_threads(MYSQL_THD, SHOW_VAR *var,
                                            char *buf);
  static int show_state(MYSQL_THD, SHOW_VAR *var, char *buf);

  std::mutex m_current_runtime_values_mutex;
  CurrentRuntimeValues m_current_runtime_values;

  Statusvar m_current_http_port;
  Statusvar m_current_tls_cert;
  Statusvar m_current_tls_key;
  Statusvar m_current_tls_cipher;
  Statusvar m_current_io_worker_threads;
  Statusvar m_state;

  // Null terminator for the SHOW_VAR array
  Statusvar m_null_entry;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_STATUS_VARS_H
