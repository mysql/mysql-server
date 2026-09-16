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

#ifndef MRS_COMPONENT_SERVER_CONFIG_H
#define MRS_COMPONENT_SERVER_CONFIG_H

#include <string>

namespace mrs_component {

/**
  Holds the Server configuration required by the MRS component.
*/
class ServerConfig {
 public:
  ServerConfig() = default;
  ~ServerConfig() = default;
  ServerConfig(const ServerConfig &) = delete;
  ServerConfig &operator=(const ServerConfig &) = delete;
  ServerConfig(ServerConfig &&) = delete;
  ServerConfig &operator=(ServerConfig &&) = delete;

  std::string m_basedir;
  std::string m_datadir;
  std::string m_log_level;
  std::string m_hostname;
  std::string m_port;
  std::string m_server_id;
  std::string m_server_uuid;
  std::string m_ssl_cert;
  std::string m_ssl_key;
  std::string m_ssl_cipher;
  std::string m_bind_address;
  std::string m_socket;

  int fetch_required_sysvars();
  bool requires_local_authentication() const;

 private:
  int get_sysvar_value(const std::string &name, std::string &value) const;
  int get_have_ssl(bool &value) const;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_SERVER_CONFIG_H
