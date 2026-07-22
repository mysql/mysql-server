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
#include <cstdint>

#include "m_string.h"
#include "mrs_component.h"
#include "mrs_component_command_executor.h"
#include "mrs_component_table_access.h"

namespace mrs_component {

int ServerConfig::get_sysvar_value(const std::string &name,
                                   std::string &value) {
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

int ServerConfig::get_have_ssl(bool &value) {
  value = false;

  // One table only needed: performance_schema.tls_channel_status
  auto ta = Table_accessor::make_with_current_ctx(1);
  if (!ta.valid()) {
    MRS_LOG_DEBUG("get_have_ssl: no TA session (no THD?)");
    return 1;
  }

  auto tk = ta.add_table_read("performance_schema", "tls_channel_status");
  if (tk.id == SIZE_MAX || !ta.begin() || !ta.materialize_table(tk)) {
    MRS_LOG_DEBUG(
        "get_have_ssl: failed to scan performance_schema.tls_channel_status");
    return 1;
  }

  TA_table tbl_tls = tk.tbl;

  bool ok = ta.scan(tbl_tls, [&](TA_table t) {
    // tls_channel_status: | CHANNEL | PROPERTY | VALUE |
    enum : std::uint8_t { CHANNEL = 0, PROPERTY = 1, VALUE = 2 };

    std::string ch, prop;
    if (!ta.get_varchar(t, CHANNEL, ch)) {
      return true;  // No value for CHANNEL, skip row
    }
    if (!ta.get_varchar(t, PROPERTY, prop)) {
      return true;  // No value for PROPERTY, skip row
    }

    // Found both columns, if "mysql_main" and "Enabled" check the VALUE
    if ((native_strcasecmp(ch.c_str(), "mysql_main") == 0) &&
        native_strcasecmp(prop.c_str(), "Enabled") == 0) {
      std::string val;
      if (ta.get_varchar(t, VALUE, val)) {
        value = native_strcasecmp(val.c_str(), "Yes") == 0;
      }
      return false;  // Found it, got the value, scan done
    }

    return true;  // keep scanning
  });

  if (!ok) {
    MRS_LOG_DEBUG("get_have_ssl: scan(tls_channel_status) failed");
    return 1;
  }

  MRS_LOG_DEBUG("MySQL TLS %s", value ? "enabled" : "disabled");

  return 0;
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

  if (get_have_ssl(m_have_ssl) != 0) {
    MRS_LOG_ERROR(
        "Unable to get TLS properties to determine whether SSL is available");
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
