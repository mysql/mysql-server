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

#include "mrs_component_server_config.h"
#include <stdexcept>
#include <string_view>

#include "mrs_component.h"
#include "scope_guard.h"

namespace mrs_component {

int ServerConfig::get_sysvar_value(const std::string &name,
                                   std::string &value) const {
  std::string result(256, ' ');  // start small
  size_t buffer_size = result.length();

  const auto read_value = [&]() {
    char *buffer_ptr = result.data();
    return sysvar_reader->get(nullptr, "GLOBAL", "mysql_server", name.c_str(),
                              reinterpret_cast<void **>(&buffer_ptr),
                              &buffer_size);
  };

  if (read_value() != 0) {
    if (buffer_size <= result.size()) return 1;

    result.resize(buffer_size);
    if (read_value() != 0) return 1;
  }

  result.resize(buffer_size);  // resize to the actual size
  value = std::move(result);

  return 0;
}

int ServerConfig::get_have_ssl(bool &value) const {
  value = false;

  MYSQL_THD thd = nullptr;
  if (mysql_service_mysql_current_thread_reader->get(&thd) != 0) return 1;

  // Passing a null THD to the status service can end the MRS worker's thread
  // state. The table-access factory preserves that state while supplying a
  // temporary THD; no tables or transactions are opened here.
  auto context = table_access_factory->create(thd, /*max_tables=*/1);
  if (context == nullptr) return 1;
  const auto context_cleanup =
      create_scope_guard([context] { table_access_factory->destroy(context); });
  if (mysql_service_mysql_current_thread_reader->get(&thd) != 0 ||
      thd == nullptr) {
    return 1;
  }

  my_h_string cache_mode = nullptr;
  // Read only this value, without aggregating status across active sessions.
  const auto read_error = statusvar_reader->get(
      thd, "Ssl_session_cache_mode", /*get_global=*/true, &cache_mode);
  const auto string_cleanup = create_scope_guard([cache_mode] {
    if (cache_mode != nullptr) string_factory->destroy(cache_mode);
  });
  if (read_error || cache_mode == nullptr) return 1;

  const char *data = nullptr;
  size_t length = 0;
  CHARSET_INFO_h charset = nullptr;
  if (string_get_data_in_charset->get_data(cache_mode, &data, &length,
                                           &charset) != 0 ||
      data == nullptr || length == 0) {
    return 1;
  }

  // NONE means no active mysql_main TLS context; OFF only disables its cache.
  value = std::string_view(data, length) != "NONE";

  MRS_LOG_DEBUG("MySQL TLS %s", value ? "enabled" : "disabled");

  return 0;
}

bool ServerConfig::requires_local_authentication() const {
  // TLS availability can change while MRS runs.
  bool have_ssl = false;
  if (get_have_ssl(have_ssl) != 0) {
    throw std::runtime_error("Failed to read MySQL TLS availability");
  }
  return !have_ssl;
}

int ServerConfig::fetch_required_sysvars() {
  if (get_sysvar_value("basedir", m_basedir) != 0) {
    MRS_LOG_ERROR("Unable to read @@basedir");
    return 1;
  }

  if (get_sysvar_value("datadir", m_datadir) != 0) {
    MRS_LOG_ERROR("Unable to read @@datadir");
    return 1;
  }

  if (get_sysvar_value("log_error_verbosity", m_log_level) != 0) {
    MRS_LOG_ERROR("Unable to read @@log_error_verbosity");
    return 1;
  }

  if (get_sysvar_value("hostname", m_hostname) != 0) {
    MRS_LOG_ERROR("Unable to read @@hostname");
    return 1;
  }

  if (get_sysvar_value("port", m_port) != 0) {
    MRS_LOG_ERROR("Unable to read @@port");
    return 1;
  }

  if (get_sysvar_value("server_id", m_server_id) != 0) {
    MRS_LOG_ERROR("Unable to read @@server_id");
    return 1;
  }

  if (get_sysvar_value("server_uuid", m_server_uuid) != 0) {
    MRS_LOG_ERROR("Unable to read @@server_uuid");
    return 1;
  }

  if (get_sysvar_value("ssl_cert", m_ssl_cert) != 0) {
    MRS_LOG_ERROR("Unable to read @@ssl_cert");
    return 1;
  }

  if (get_sysvar_value("ssl_key", m_ssl_key) != 0) {
    MRS_LOG_ERROR("Unable to read @@ssl_key");
    return 1;
  }

  if (get_sysvar_value("ssl_cipher", m_ssl_cipher) != 0) {
    MRS_LOG_ERROR("Unable to read @@ssl_cipher");
    return 1;
  }

  if (get_sysvar_value("bind_address", m_bind_address) != 0) {
    MRS_LOG_ERROR("Unable to read @@bind_address");
    return 1;
  }

  if (get_sysvar_value("socket", m_socket) != 0) {
    MRS_LOG_ERROR("Unable to read @@socket");
    return 1;
  }

  return 0;
}

}  // namespace mrs_component
