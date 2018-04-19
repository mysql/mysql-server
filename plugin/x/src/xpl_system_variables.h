/*
   Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef XPL_SYSTEM_VARIABLES_H
#define XPL_SYSTEM_VARIABLES_H

#include <algorithm>
#include <vector>

#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/src/xpl_log.h"

#ifdef max_allowed_packet
#undef max_allowed_packet
#endif  // max_allowed_packet

struct SYS_VAR;
class THD;

namespace xpl {

struct Ssl_config {
  Ssl_config();

  bool is_configured() const;

  char *ssl_key;
  char *ssl_ca;
  char *ssl_capath;
  char *ssl_cert;
  char *ssl_cipher;
  char *ssl_crl;
  char *ssl_crlpath;

 private:
  bool has_value(const char *ptr) const;

  char m_null_char;
};

class Plugin_system_variables {
 public:
  static int max_connections;
  static unsigned int port;
  static unsigned int min_worker_threads;
  static unsigned int idle_worker_thread_timeout;
  static unsigned int max_allowed_packet;
  static unsigned int connect_timeout;
  static char *socket;
  static unsigned int port_open_timeout;
  static char *bind_address;
  static uint32_t m_interactive_timeout;
  static uint32_t m_document_id_unique_prefix;

  static Ssl_config ssl_config;

 public:
  typedef ngs::function<void(THD *)> Value_changed_callback;

  static void clean_callbacks();
  static void registry_callback(Value_changed_callback callcback);

  template <typename Copy_type>
  static void update_func(THD *thd, SYS_VAR *var, void *tgt, const void *save);

  static void setup_system_variable_from_env_or_compile_opt(
      char *&cnf_option, const char *env_variable, const char *compile_option);

 private:
  static const char *get_system_variable_impl(const char *cnf_option,
                                              const char *env_variable,
                                              const char *compile_option);

  static std::vector<Value_changed_callback> m_callbacks;
};

template <typename Copy_type>
void Plugin_system_variables::update_func(THD *thd, SYS_VAR *, void *tgt,
                                          const void *save) {
  *(Copy_type *)tgt = *(Copy_type *)save;

  std::for_each(
      m_callbacks.begin(), m_callbacks.end(),
      [&thd](const Value_changed_callback &callback) { callback(thd); });
}

}  // namespace xpl

#endif /* XPL_SYSTEM_VARIABLES_H */
