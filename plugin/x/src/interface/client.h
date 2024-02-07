/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_INTERFACE_CLIENT_H_
#define PLUGIN_X_SRC_INTERFACE_CLIENT_H_

#include <memory>
#include <vector>

#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/helper/optional_value.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/vio.h"
#include "plugin/x/src/ngs/compression_types.h"
#include "plugin/x/src/ngs/protocol/message.h"
#include "plugin/x/src/ngs/protocol_fwd.h"

class THD;

namespace xpl {
namespace iface {

class Protocol_encoder;
class Server;
class Waiting_for_io;

class Client {
 public:
  using Client_id = uint64_t;

  enum class State {
    k_invalid,
    k_accepted,
    k_accepted_with_session,
    k_authenticating_first,
    k_running,
    k_closing,
    k_closed
  };

 public:
  virtual ~Client() = default;

  virtual iface::Protocol_encoder &protocol() const = 0;
  virtual iface::Server &server() const = 0;
  virtual iface::Vio &connection() const = 0;
  virtual void configure_compression_opts(
      const ngs::Compression_algorithm algo, const int64_t max_msg,
      const bool combine, const Optional_value<int64_t> &level) = 0;

  virtual void activate_tls() = 0;

 public:  // Notifications from Server object
  virtual void on_auth_timeout() = 0;
  virtual void on_server_shutdown() = 0;
  virtual void kill() = 0;

  virtual void run() = 0;
  virtual Mutex &get_session_exit_mutex() = 0;

 public:
  virtual const char *client_address() const = 0;
  virtual const char *client_hostname() const = 0;
  virtual const char *client_hostname_or_address() const = 0;
  virtual const char *client_id() const = 0;
  virtual Client_id client_id_num() const = 0;
  virtual int client_port() const = 0;

  virtual void reset_accept_time() = 0;
  virtual chrono::Time_point get_accept_time() const = 0;
  virtual State get_state() const = 0;
  virtual bool supports_expired_passwords() const = 0;
  virtual void set_supports_expired_passwords(bool flag) = 0;

  virtual bool is_interactive() const = 0;
  virtual void set_is_interactive(const bool is_interactive) = 0;

  virtual void set_write_timeout(const uint32_t) = 0;
  virtual void set_read_timeout(const uint32_t) = 0;
  virtual void set_wait_timeout(const uint32_t) = 0;

  virtual iface::Session *session() = 0;
  virtual std::shared_ptr<iface::Session> session_shared_ptr() const = 0;

  virtual void on_session_reset(iface::Session *s) = 0;
  virtual void on_session_close(iface::Session *s) = 0;
  virtual void on_session_auth_success(iface::Session *s) = 0;

  virtual void disconnect_and_trigger_close() = 0;

  virtual bool is_handler_thd(const THD *thd) const = 0;
  virtual void handle_message(ngs::Message_request *message) = 0;
  virtual void get_capabilities(
      const Mysqlx::Connection::CapabilitiesGet &msg) = 0;
  virtual void set_capabilities(
      const Mysqlx::Connection::CapabilitiesSet &msg) = 0;

  virtual iface::Waiting_for_io *get_idle_processing() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_CLIENT_H_
