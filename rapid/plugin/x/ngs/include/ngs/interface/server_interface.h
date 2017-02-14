/*
 * Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
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

#ifndef NGS_SERVER_INTERFACE_H_
#define NGS_SERVER_INTERFACE_H_

#include "ngs/protocol_authentication.h"
#include "ngs_common/smart_ptr.h"


namespace ngs {

class Client_interface;
class Server;
class Session_interface;
class Scheduler_dynamic;
class Protocol_encoder;
class Protocol_config;
class Mutex;

class Server_interface {
public:
  virtual ~Server_interface() {}


  virtual void get_authentication_mechanisms(std::vector<std::string> &auth_mech, Client_interface &client) = 0;

  virtual ngs::shared_ptr<Scheduler_dynamic> get_worker_scheduler() const = 0;
  virtual Authentication_handler_ptr         get_auth_handler(const std::string &name, Session_interface *session) = 0;
  virtual ngs::shared_ptr<Protocol_config>   get_config() const = 0;
  virtual Mutex &get_client_exit_mutex() = 0;

  virtual Ssl_context *ssl_context() const = 0;

  virtual ngs::shared_ptr<Session_interface> create_session(
      Client_interface &client,
      Protocol_encoder &proto,
      int session_id) = 0;

  virtual bool is_running() = 0;

  virtual void on_client_closed(const Client_interface &client) = 0;
  virtual void restart_client_supervision_timer() = 0;
};

} // namespace ngs

#endif // NGS_SERVER_INTERFACE_H_
