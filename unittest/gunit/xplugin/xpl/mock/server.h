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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVER_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVER_H_

#include <gmock/gmock.h>
#include <memory>

#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/ngs/client_list.h"

namespace xpl {
namespace test {
namespace mock {

class Server : public iface::Server {
 public:
  Server();
  virtual ~Server() override;

  MOCK_METHOD(iface::Authentication_container &, get_authentications, (),
              (override));

  MOCK_METHOD(bool, reset, (), (override));
  MOCK_METHOD(void, start_failed, (), (override));
  MOCK_METHOD(bool, prepare, (), (override));
  MOCK_METHOD(void, start_tasks, (), (override));
  MOCK_METHOD(void, stop, (), (override));
  MOCK_METHOD(void, gracefull_shutdown, (), (override));

  MOCK_METHOD(void, delayed_start_tasks, (), (override));

  MOCK_METHOD(std::shared_ptr<ngs::Protocol_global_config>, get_config, (),
              (const, override));
  MOCK_METHOD(bool, is_running, (), (override));
  MOCK_METHOD(iface::Ssl_context *, ssl_context, (), (const, override));
  MOCK_METHOD(void, on_client_closed, (const iface::Client &), (override));
  MOCK_METHOD(std::shared_ptr<iface::Session>, create_session,
              (iface::Client *, iface::Protocol_encoder *, const int),
              (override));

  MOCK_METHOD(ngs::Client_list &, get_client_list, (), (override));
  MOCK_METHOD(std::shared_ptr<iface::Client>, get_client, (const THD *),
              (override));
  MOCK_METHOD(ngs::Error_code, kill_client, (const uint64_t, iface::Session *),
              (override));

  MOCK_METHOD(Mutex &, get_client_exit_mutex, (), (override));
  MOCK_METHOD(void, restart_client_supervision_timer, (), (override));

  MOCK_METHOD(iface::Document_id_generator &, get_document_id_generator, (),
              (const, override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVER_H_
