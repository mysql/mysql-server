/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CLIENT_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CLIENT_H_

#include <gmock/gmock.h>
#include <memory>

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/server.h"

namespace xpl {
namespace test {
namespace mock {

class Client : public iface::Client {
 public:
  Client();
  virtual ~Client() override;

  MOCK_METHOD(Mutex &, get_session_exit_mutex, (), (override));

  MOCK_METHOD(const char *, client_id, (), (const, override));

  MOCK_METHOD(void, kill, (), (override));
  MOCK_METHOD(const char *, client_address, (), (const, override));
  MOCK_METHOD(const char *, client_hostname, (), (const, override));
  MOCK_METHOD(const char *, client_hostname_or_address, (), (const, override));
  MOCK_METHOD(iface::Vio &, connection, (), (const, override));
  MOCK_METHOD(iface::Server &, server, (), (const, override));
  MOCK_METHOD(iface::Protocol_encoder &, protocol, (), (const, override));

  MOCK_METHOD(Client_id, client_id_num, (), (const, override));
  MOCK_METHOD(int, client_port, (), (const, override));

  MOCK_METHOD(chrono::Time_point, get_accept_time, (), (const, override));
  MOCK_METHOD(Client::State, get_state, (), (const, override));

  MOCK_METHOD(iface::Session *, session, (), (override));
  MOCK_METHOD(std::shared_ptr<iface::Session>, session_shared_ptr, (),
              (const, override));
  MOCK_METHOD(bool, supports_expired_passwords, (), (const, override));
  MOCK_METHOD(void, set_supports_expired_passwords, (bool), (override));

  MOCK_METHOD(bool, is_interactive, (), (const, override));
  MOCK_METHOD(void, set_is_interactive, (bool), (override));

  MOCK_METHOD(void, set_wait_timeout, (const unsigned int), (override));
  MOCK_METHOD(void, set_read_timeout, (const unsigned int), (override));
  MOCK_METHOD(void, set_write_timeout, (const unsigned int), (override));

  MOCK_METHOD(void, configure_compression_opts,
              (const ngs::Compression_algorithm algo, const int64_t max_msg,
               const bool combine, const Optional_value<int64_t> &level),
              (override));
  MOCK_METHOD(void, handle_message, (ngs::Message_request *), (override));

  MOCK_METHOD(void, get_capabilities,
              (const Mysqlx::Connection::CapabilitiesGet &), (override));
  MOCK_METHOD(void, set_capabilities,
              (const Mysqlx::Connection::CapabilitiesSet &), (override));

  MOCK_METHOD(iface::Waiting_for_io *, get_idle_processing, (), (override));

  MOCK_METHOD(bool, on_session_reset_void, (iface::Session &));
  MOCK_METHOD(bool, on_session_close_void, (iface::Session &));
  MOCK_METHOD(bool, on_session_auth_success_void, (iface::Session &));
  MOCK_METHOD(bool, on_connection_close_void, (iface::Session &));

  MOCK_METHOD(bool, disconnect_and_trigger_close_void, ());
  MOCK_METHOD(bool, is_handler_thd, (const THD *), (const, override));

  MOCK_METHOD(bool, activate_tls_void, ());
  MOCK_METHOD(bool, on_auth_timeout_void, ());
  MOCK_METHOD(bool, on_server_shutdown_void, ());
  MOCK_METHOD(bool, run_void, ());
  MOCK_METHOD(bool, reset_accept_time_void, ());

  void on_session_reset(iface::Session *arg) override {
    on_session_reset_void(*arg);
  }

  void on_session_close(iface::Session *arg) override {
    on_session_close_void(*arg);
  }

  void on_session_auth_success(iface::Session *arg) override {
    on_session_auth_success_void(*arg);
  }

  void disconnect_and_trigger_close() override {
    disconnect_and_trigger_close_void();
  }

  void activate_tls() override { activate_tls_void(); }

  void on_auth_timeout() override { on_auth_timeout_void(); }

  void on_server_shutdown() override { on_server_shutdown_void(); }

  void run() override { run_void(); }

  void reset_accept_time() override { reset_accept_time_void(); }
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_CLIENT_H_
