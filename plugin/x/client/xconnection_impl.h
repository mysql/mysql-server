/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_CONNECTION_IMPL_H_
#define X_CLIENT_CONNECTION_IMPL_H_

#include <memory>

#include "my_io.h"
#include "plugin/x/client/mysqlxclient/xconnection.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/xcontext.h"
#include "violite.h"

struct sockaddr_un;

namespace xcl {

class Ssl_config;
class Connection_config;

class Connection_impl : public XConnection {
 public:
  explicit Connection_impl(std::shared_ptr<Context> context);
  ~Connection_impl() override;

  XError connect_to_localhost(const std::string &unix_socket) override;
  XError connect(const std::string &host, const uint16_t port,
                 const Internet_protocol ip_mode) override;

  my_socket get_socket_fd() override;

  XError activate_tls() override;
  XError shutdown(const Shutdown_type how_to_shutdown) override;

  XError write(const uint8_t *data, const std::size_t data_length) override;
  XError read(uint8_t *data, const std::size_t data_length) override;

  void close() override;

  const State &state() override;

  XError set_read_timeout(const int deadline_seconds) override;
  XError set_write_timeout(const int deadline_seconds) override;

 private:
  XError connect(sockaddr *sockaddr, const std::size_t addr_size);

  static XError get_ssl_init_error(const int init_error_id);
  static XError get_ssl_error(const int error_id);
  static XError get_socket_error(const int error_id);
  static std::string get_socket_error_description(const int error_id);

  st_VioSSLFd *m_vioSslFd;
  Vio *m_vio;
  bool m_ssl_active;
  bool m_connected;
  Connection_type m_connection_type;
  enum_ssl_init_error m_ssl_init_error;
  std::unique_ptr<State> m_state;
  std::shared_ptr<Context> m_context;
  std::string m_hostname;
};

}  // namespace xcl

#endif  // X_CLIENT_CONNECTION_IMPL_H_
