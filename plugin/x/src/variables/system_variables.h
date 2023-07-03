/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_H_
#define PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "mysql/plugin.h"
#include "plugin/x/src/compression_level_variable.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/service_sys_variables.h"
#include "plugin/x/src/variables/set_variable.h"
#include "plugin/x/src/variables/ssl_config.h"
#include "plugin/x/src/variables/timeout_config.h"

namespace xpl {

class Plugin_system_variables {
 public:
  static int m_max_connections;
  static unsigned int m_port;
  static unsigned int m_min_worker_threads;
  static unsigned int m_idle_worker_thread_timeout;
  static unsigned int m_max_allowed_packet;
  static unsigned int m_connect_timeout;
  static char *m_socket;
  static unsigned int m_port_open_timeout;
  static char *m_bind_address;
  static uint32_t m_interactive_timeout;
  static uint32_t m_document_id_unique_prefix;
  static bool m_enable_hello_notice;

  static Set_variable m_compression_algorithms;

  static Compression_deflate_level_variable m_deflate_default_compression_level;
  static Compression_lz4_level_variable m_lz4_default_compression_level;
  static Compression_zstd_level_variable m_zstd_default_compression_level;

  static Compression_deflate_level_variable
      m_deflate_max_client_compression_level;
  static Compression_lz4_level_variable m_lz4_max_client_compression_level;
  static Compression_zstd_level_variable m_zstd_max_client_compression_level;

  static Ssl_config m_ssl_config;
  static struct SYS_VAR *m_plugin_system_variables[];

 public:
  using Client_interface_ptr = std::shared_ptr<iface::Client>;
  using Value_changed_callback = std::function<void(THD *)>;
  using Get_client_callback = std::function<Client_interface_ptr(THD *)>;
  static const Timeouts_config get_global_timeouts();
  static void fetch_plugin_variables();
  static void set_thd_wait_timeout(THD *thd, const uint32_t timeout_value);

  static std::string get_system_variable(const std::string &name,
                                         bool *out_error = nullptr);

  static Get_client_callback get_client_callback();
  static void initialize(iface::Service_sys_variables *sys_var,
                         Value_changed_callback update_callback,
                         Get_client_callback client_callback,
                         const bool fetch = true);
  static void cleanup();

 private:
  static iface::Service_sys_variables *m_sys_var;
  static Value_changed_callback m_update_callback;
  static Get_client_callback m_client_callback;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_H_
