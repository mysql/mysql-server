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

#ifndef PLUGIN_X_SRC_INTERFACE_SERVER_H_
#define PLUGIN_X_SRC_INTERFACE_SERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/authentication_container.h"
#include "plugin/x/src/interface/document_id_generator.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/sql_session.h"

namespace ngs {
class Scheduler_dynamic;
class Protocol_global_config;
class Client_list;
}  // namespace ngs

namespace xpl {
namespace iface {

class Client;
class Protocol_encoder;
class Ssl_context;

class Server {
 public:
  virtual ~Server() = default;

  virtual void start_failed() = 0;
  virtual bool reset() = 0;
  virtual bool prepare() = 0;
  virtual void delayed_start_tasks() = 0;
  virtual void start_tasks(xpl::iface::Sql_session *) = 0;
  virtual void stop() = 0;
  virtual void gracefull_shutdown() = 0;

  virtual iface::Authentication_container &get_authentications() = 0;

  virtual std::shared_ptr<ngs::Protocol_global_config> get_config() const = 0;

  virtual Document_id_generator &get_document_id_generator() const = 0;

  virtual xpl::Mutex &get_client_exit_mutex() = 0;

  virtual Ssl_context *ssl_context() const = 0;

  virtual std::shared_ptr<Session> create_session(Client *client,
                                                  Protocol_encoder *proto,
                                                  const int session_id) = 0;

  virtual bool is_running() = 0;

  virtual ngs::Error_code kill_client(const uint64_t client_id,
                                      xpl::iface::Session *requester) = 0;

  virtual std::shared_ptr<iface::Client> get_client(const THD *thd) = 0;
  virtual ngs::Client_list &get_client_list() = 0;
  virtual void on_client_closed(const Client &client) = 0;
  virtual void restart_client_supervision_timer() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SERVER_H_
