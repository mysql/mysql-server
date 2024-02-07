/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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

#include "unittest/gunit/xplugin/xpl/mock/account_verification.h"
#include "unittest/gunit/xplugin/xpl/mock/account_verification_handler.h"
#include "unittest/gunit/xplugin/xpl/mock/authentication.h"
#include "unittest/gunit/xplugin/xpl/mock/authentication_container.h"
#include "unittest/gunit/xplugin/xpl/mock/capabilities_configurator.h"
#include "unittest/gunit/xplugin/xpl/mock/capability_handler.h"
#include "unittest/gunit/xplugin/xpl/mock/client.h"
#include "unittest/gunit/xplugin/xpl/mock/document_id_aggregator.h"
#include "unittest/gunit/xplugin/xpl/mock/document_id_generator.h"
#include "unittest/gunit/xplugin/xpl/mock/file.h"
#include "unittest/gunit/xplugin/xpl/mock/listener_factory.h"
#include "unittest/gunit/xplugin/xpl/mock/message_dispatcher.h"
#include "unittest/gunit/xplugin/xpl/mock/notice_configuration.h"
#include "unittest/gunit/xplugin/xpl/mock/notice_output_queue.h"
#include "unittest/gunit/xplugin/xpl/mock/operations_factory.h"
#include "unittest/gunit/xplugin/xpl/mock/protocol_encoder.h"
#include "unittest/gunit/xplugin/xpl/mock/protocol_monitor.h"
#include "unittest/gunit/xplugin/xpl/mock/scheduler_dynamic.h"
#include "unittest/gunit/xplugin/xpl/mock/server.h"
#include "unittest/gunit/xplugin/xpl/mock/service_sys_variables.h"
#include "unittest/gunit/xplugin/xpl/mock/session.h"
#include "unittest/gunit/xplugin/xpl/mock/sha256_password_cache.h"
#include "unittest/gunit/xplugin/xpl/mock/socket.h"
#include "unittest/gunit/xplugin/xpl/mock/socket_events.h"
#include "unittest/gunit/xplugin/xpl/mock/sql_session.h"
#include "unittest/gunit/xplugin/xpl/mock/srv_session_services.h"
#include "unittest/gunit/xplugin/xpl/mock/ssl_context.h"
#include "unittest/gunit/xplugin/xpl/mock/ssl_context_options.h"
#include "unittest/gunit/xplugin/xpl/mock/ssl_session_options.h"
#include "unittest/gunit/xplugin/xpl/mock/system.h"
#include "unittest/gunit/xplugin/xpl/mock/vio.h"
#include "unittest/gunit/xplugin/xpl/mock/waiting_for_io.h"

namespace xpl {
namespace test {
namespace mock {

Account_verification_handler::Account_verification_handler() {}
Account_verification_handler::~Account_verification_handler() {}

Account_verification::Account_verification() {}
Account_verification::~Account_verification() {}

Authentication_container::Authentication_container() {}
Authentication_container::~Authentication_container() {}

Authentication::Authentication() {}
Authentication::~Authentication() {}

Capability_handler::Capability_handler() {}
Capability_handler::~Capability_handler() {}

Client::Client() {}
Client::~Client() {}

Document_id_aggregator::Document_id_aggregator() {}
Document_id_aggregator::~Document_id_aggregator() {}

Document_id_generator::Document_id_generator() {}
Document_id_generator::~Document_id_generator() {}

File::File() {}
File::~File() {}

Listener_factory::Listener_factory() {}
Listener_factory::~Listener_factory() {}

Message_dispatcher::Message_dispatcher() {}
Message_dispatcher::~Message_dispatcher() {}

Notice_configuration::Notice_configuration() {}
Notice_configuration::~Notice_configuration() {}

Notice_output_queue::Notice_output_queue() {}
Notice_output_queue::~Notice_output_queue() {}

Operations_factory::Operations_factory() {}
Operations_factory::~Operations_factory() {}

Protocol_encoder::Protocol_encoder() {}
Protocol_encoder::~Protocol_encoder() {}

Protocol_monitor::Protocol_monitor() {}
Protocol_monitor::~Protocol_monitor() {}

Scheduler_dynamic::Scheduler_dynamic() {}
Scheduler_dynamic::~Scheduler_dynamic() {}

Server::Server() {}
Server::~Server() {}

Service_sys_variables::Service_sys_variables() {}
Service_sys_variables::~Service_sys_variables() {}

Session::Session() {}
Session::~Session() {}

Sha256_password_cache::Sha256_password_cache() {}
Sha256_password_cache::~Sha256_password_cache() {}

Cache_based_verification::Cache_based_verification(
    xpl::iface::SHA256_password_cache *cache)
    : xpl::Cache_based_verification(cache) {}
Cache_based_verification::~Cache_based_verification() {}

Socket_events::Socket_events() {}
Socket_events::~Socket_events() {}

Socket::Socket() {}
Socket::~Socket() {}

Sql_session::Sql_session() {}
Sql_session::~Sql_session() {}

Srv_session::Srv_session() {
  assert(nullptr == m_srv_session);
  m_srv_session = this;
}
Srv_session::~Srv_session() { m_srv_session = nullptr; }

Srv_session_info::Srv_session_info() {
  assert(nullptr == m_srv_session_info);
  m_srv_session_info = this;
}
Srv_session_info::~Srv_session_info() { m_srv_session_info = nullptr; }

Ssl_context_options::Ssl_context_options() {}
Ssl_context_options::~Ssl_context_options() {}

Ssl_context::Ssl_context() {}
Ssl_context::~Ssl_context() {}

Ssl_session_options::Ssl_session_options() {}
Ssl_session_options::~Ssl_session_options() {}

System::System() {}
System::~System() {}

Vio::Vio() {}
Vio::~Vio() {}

Waiting_for_io::Waiting_for_io() {}
Waiting_for_io::~Waiting_for_io() {}

}  // namespace mock
}  // namespace test
}  // namespace xpl
