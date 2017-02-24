/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_SERVER_DELEGATE_H_
#define _NGS_SERVER_DELEGATE_H_

#include "ngs_common/smart_ptr.h"

namespace ngs
{

class Session_interface;
class Client_interface;
class Protocol_encoder;

class Server_delegate
{
public:
  enum Reject_reason
  {
    AcceptError,
    TooManyConnections
  };
  virtual ~Server_delegate() {}

  virtual bool will_accept_client(const Client_interface &client) = 0;
  virtual void did_accept_client(const Client_interface &client) = 0;
  virtual void did_reject_client(Reject_reason reason) = 0;

  virtual ngs::shared_ptr<Client_interface> create_client(Connection_ptr sock) = 0;
  virtual ngs::shared_ptr<Session_interface> create_session(Client_interface &client,
                                                    Protocol_encoder &proto,
                                                    int session_id) = 0;

  virtual void on_client_closed(const Client_interface &client) = 0;
  virtual bool is_terminating() const = 0;
};

} // namespace ngs

#endif // _NGS_SERVER_DELEGATE_H_
