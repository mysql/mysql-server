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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_CLIENT_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_CLIENT_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/interface/session_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs_common/chrono.h"

namespace ngs {

class Server_interface;
class Protocol_encoder_interface;

class Client_interface {
 public:
  typedef uint64_t Client_id;

  enum Client_state {
    Client_invalid,
    Client_accepted,
    Client_accepted_with_session,
    Client_authenticating_first,
    Client_running,
    Client_closing,
    Client_closed
  };

 public:
  virtual ~Client_interface() {}

  virtual Protocol_encoder_interface &protocol() const = 0;
  virtual Server_interface &server() const = 0;
  virtual Vio_interface &connection() = 0;

  virtual void activate_tls() = 0;

 public:  // Notifications from Server object
  virtual void on_auth_timeout() = 0;
  virtual void on_server_shutdown() = 0;

  virtual void run(const bool skip_resolve_name) = 0;
  virtual Mutex &get_session_exit_mutex() = 0;

 public:
  virtual const char *client_address() const = 0;
  virtual const char *client_hostname() const = 0;
  virtual const char *client_id() const = 0;
  virtual Client_id client_id_num() const = 0;
  virtual int client_port() const = 0;

  virtual void reset_accept_time() = 0;
  virtual chrono::time_point get_accept_time() const = 0;
  virtual Client_state get_state() const = 0;
  virtual bool supports_expired_passwords() const = 0;

  virtual bool is_interactive() const = 0;
  virtual void set_is_interactive(const bool is_interactive) = 0;

  virtual void set_write_timeout(const uint32_t) = 0;
  virtual void set_read_timeout(const uint32_t) = 0;
  virtual void set_wait_timeout(const uint32_t) = 0;

  virtual Session_interface *session() = 0;
  virtual ngs::shared_ptr<ngs::Session_interface> session_smart_ptr() = 0;

 public:
  virtual void on_session_reset(Session_interface &s) = 0;
  virtual void on_session_close(Session_interface &s) = 0;
  virtual void on_session_auth_success(Session_interface &s) = 0;

  virtual void disconnect_and_trigger_close() = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_CLIENT_INTERFACE_H_
