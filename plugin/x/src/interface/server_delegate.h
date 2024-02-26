/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SERVER_DELEGATE_H_
#define PLUGIN_X_SRC_INTERFACE_SERVER_DELEGATE_H_

#include <memory>

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {
namespace iface {

class Protocol_encoder;

class Server_delegate {
 public:
  enum class Reject_reason { k_accept_error, k_too_many_connections };
  virtual ~Server_delegate() = default;

  virtual bool will_accept_client(const Client &client) = 0;
  virtual void did_accept_client(const Client &client) = 0;
  virtual void did_reject_client(Reject_reason reason) = 0;

  virtual std::shared_ptr<Client> create_client(std::shared_ptr<Vio> sock) = 0;
  virtual std::shared_ptr<Session> create_session(Client *client,
                                                  Protocol_encoder *proto,
                                                  const int session_id) = 0;

  virtual void on_client_closed(const Client &client) = 0;
  virtual bool is_terminating() const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SERVER_DELEGATE_H_
