/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_XPL_CLIENT_H_
#define PLUGIN_X_SRC_XPL_CLIENT_H_

#include <memory>
#include <string>

#include "plugin/x/ngs/include/ngs/client.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {
class Session;

class Client;

class Protocol_monitor : public iface::Protocol_monitor {
 public:
  Protocol_monitor() = default;
  void init(Client *client);

  void on_notice_warning_send() override;
  void on_notice_other_send() override;
  void on_notice_global_send() override;
  void on_error_send() override;
  void on_fatal_error_send() override;
  void on_init_error_send() override;
  void on_row_send() override;
  void on_send(const uint32_t bytes_transferred) override;
  void on_send_compressed(const uint32_t bytes_transferred) override;
  void on_send_before_compression(const uint32_t bytes_transferred) override;
  void on_receive(const uint32_t bytes_transferred) override;
  void on_error_unknown_msg_type() override;
  void on_receive_compressed(const uint32_t bytes_transferred) override;
  void on_receive_after_decompression(
      const uint32_t bytes_transferred) override;
  void on_messages_sent(const uint32_t messages) override;

 private:
  Client *m_client{nullptr};
};

class Client : public ngs::Client {
 public:
  Client(std::shared_ptr<iface::Vio> connection, iface::Server *server,
         Client_id client_id, Protocol_monitor *pmon);
  ~Client() override;

 public:  // impl ngs::Client
  std::string resolve_hostname() override;
  Capabilities_configurator *capabilities_configurator() override;

  void set_is_interactive(const bool flag) override;

 public:
  bool is_handler_thd(const THD *thd) const override;

  std::string get_status_ssl_cipher_list() const;
  std::string get_status_compression_algorithm() const;
  std::string get_status_compression_level() const;

  void kill() override;

 private:
  bool is_localhost(const char *hostname);
};

typedef std::shared_ptr<Client> Client_ptr;

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_CLIENT_H_
