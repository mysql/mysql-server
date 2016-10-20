/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef XPL_SYSTEM_VARIABLES_H
#define XPL_SYSTEM_VARIABLES_H

#include "my_global.h"
#include "xpl_log.h"
#include "ngs_common/bind.h"
#include <vector>
#include <algorithm>

#ifdef max_allowed_packet
#undef max_allowed_packet
#endif // max_allowed_packet


struct st_mysql_sys_var;
class THD;

namespace xpl
{

struct Ssl_config
{
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
  bool  has_value(const char *ptr) const;

  char m_null_char;
};

class Plugin_system_variables
{
public:
  static int          max_connections;
  static unsigned int port;
  static unsigned int min_worker_threads;
  static unsigned int idle_worker_thread_timeout;
  static unsigned int max_allowed_packet;
  static unsigned int connect_timeout;
  static char        *socket;
  static unsigned int port_open_timeout;
  static char        *bind_address;

  static Ssl_config ssl_config;

public:
  typedef ngs::function<void()> Value_changed_callback;

  static void clean_callbacks();
  static void registry_callback(Value_changed_callback callcback);

  template<typename Copy_type>
  static void update_func(THD *thd, st_mysql_sys_var *var,
                          void *tgt, const void *save);

  static void setup_system_variable_from_env_or_compile_opt(char *&cnf_option, const char *env_variable, const char *compile_option);

private:
  struct Executor
  {
    void operator() (const Value_changed_callback & callback) { callback(); };
  };

  static const char *get_system_variable_impl(const char *cnf_option, const char *env_variable, const char *compile_option);

  static std::vector<Value_changed_callback> m_callbacks;
};

template<typename Copy_type>
void Plugin_system_variables::update_func(THD *thd, st_mysql_sys_var *var, void *tgt, const void *save)
{
  *(Copy_type*)tgt = *(Copy_type*) save;

  std::for_each(m_callbacks.begin(), m_callbacks.end(), Executor());
}

} // namespace xpl

#endif /* XPL_SYSTEM_VARIABLES_H */
