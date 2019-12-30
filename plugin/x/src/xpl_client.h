/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/client.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/src/global_timeouts.h"

struct SHOW_VAR;

namespace xpl {
class Session;

class Client;

class Protocol_monitor : public ngs::Protocol_monitor_interface {
 public:
  Protocol_monitor() : m_client(0) {}
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
  Client *m_client;
};

class Client : public ngs::Client {
 public:
  Client(std::shared_ptr<ngs::Vio_interface> connection,
         ngs::Server_interface &server, Client_id client_id,
         Protocol_monitor *pmon, const Global_timeouts &timeouts);
  ~Client() override;

 public:  // impl ngs::Client
  std::string resolve_hostname() override;
  Capabilities_configurator *capabilities_configurator() override;

  void set_is_interactive(const bool flag) override;

 public:
  bool is_handler_thd(const THD *thd) const override;

  void get_status_ssl_cipher_list(SHOW_VAR *var);

  void kill();

 private:
  bool is_localhost(const char *hostname);
};

typedef std::shared_ptr<Client> Client_ptr;

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_CLIENT_H_
