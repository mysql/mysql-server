/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_SERVER_DELEGATE_H_
#define _NGS_SERVER_DELEGATE_H_

#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"

namespace ngs {

class Session_interface;
class Client_interface;
class Protocol_encoder;

class Server_delegate {
 public:
  enum Reject_reason { AcceptError, TooManyConnections };
  virtual ~Server_delegate() {}

  virtual bool will_accept_client(const Client_interface &client) = 0;
  virtual void did_accept_client(const Client_interface &client) = 0;
  virtual void did_reject_client(Reject_reason reason) = 0;

  virtual ngs::shared_ptr<Client_interface> create_client(
      std::shared_ptr<Vio_interface> sock) = 0;
  virtual ngs::shared_ptr<Session_interface> create_session(
      Client_interface &client, Protocol_encoder_interface &proto,
      const int session_id) = 0;

  virtual void on_client_closed(const Client_interface &client) = 0;
  virtual bool is_terminating() const = 0;
};

}  // namespace ngs

#endif  // _NGS_SERVER_DELEGATE_H_
