/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/helper/multithread/initializer.h"

#include "mysql/service_srv_session.h"
#include "mysql/service_ssl_wrapper.h"

#include "plugin/x/src/xpl_log.h"

namespace xpl {

Server_thread_initializer::Server_thread_initializer() {
  initialize_server_thread();
}

Server_thread_initializer::~Server_thread_initializer() {
  deinitialize_server_thread();
}

void Server_thread_initializer::initialize_server_thread() {
  srv_session_init_thread(xpl::plugin_handle);
}

void Server_thread_initializer::deinitialize_server_thread() {
  srv_session_deinit_thread();
  ssl_wrapper_thread_cleanup();
}

}  // namespace xpl
