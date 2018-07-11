/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SERVER_TASK_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SERVER_TASK_INTERFACE_H_

#include <functional>

#include "plugin/x/ngs/include/ngs/client_list.h"
#include "plugin/x/ngs/include/ngs/server_properties.h"
#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"

namespace ngs {

class Connection_acceptor_interface;

class Server_task_interface {
 public:
  class Task_context {
   public:
    using On_connection = std::function<void(Connection_acceptor_interface &)>;

   public:
    Task_context() {
      m_on_connection = [](Connection_acceptor_interface &) {};
    }
    Task_context(const On_connection &on_connection, const bool skip_networking,
                 Server_properties *properties, Client_list *client_list)
        : m_on_connection(on_connection),
          m_skip_networking(skip_networking),
          m_properties(properties),
          m_client_list(client_list) {}

   public:
    On_connection m_on_connection;  // = [](Connection_acceptor_interface &) {};
    bool m_skip_networking = false;
    Server_properties *m_properties = nullptr;
    Client_list *m_client_list = nullptr;
  };

  enum class Stop_cause {
    k_normal_shutdown,
    k_abort,
    k_server_task_triggered_event
  };

 public:  // Task control function
  virtual ~Server_task_interface() {}

  virtual bool prepare(Task_context *context) = 0;
  virtual void stop(const Stop_cause) = 0;

 public:  // Worker thread 'enabled' methods
  virtual void pre_loop() = 0;
  virtual void post_loop() = 0;
  virtual void loop() = 0;
};

using Server_tasks_interface_ptr = ngs::shared_ptr<Server_task_interface>;

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SERVER_TASK_INTERFACE_H_
