/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SESSION_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SESSION_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/session.h"

namespace xpl {
namespace test {
namespace mock {

class Session : public iface::Session {
 public:
  Session();
  virtual ~Session() override;

  MOCK_METHOD(Session_id, session_id, (), (const, override));
  MOCK_METHOD(ngs::Error_code, init, (), (override));
  MOCK_METHOD(void, on_close, (const Close_flags), (override));
  MOCK_METHOD(void, on_kill, (), (override));
  MOCK_METHOD(void, on_auth_success, (const iface::Authentication::Response &),
              (override));
  MOCK_METHOD(void, on_auth_failure, (const iface::Authentication::Response &),
              (override));
  MOCK_METHOD(void, on_reset, (), (override));
  MOCK_METHOD(bool, handle_message, (const ngs::Message_request &), (override));
  MOCK_METHOD(State, state, (), (const, override));
  MOCK_METHOD(State, state_before_close, (), (const, override));
  MOCK_METHOD(ngs::Session_status_variables &, get_status_variables, (),
              (override));
  MOCK_METHOD(iface::Client &, client, (), (override));
  MOCK_METHOD(const iface::Client &, client, (), (const, override));
  MOCK_METHOD(bool, can_see_user, (const std::string &), (const, override));

  MOCK_METHOD(void, mark_as_tls_session, (), (override));
  MOCK_METHOD(THD *, get_thd, (), (const, override));
  MOCK_METHOD(iface::Sql_session &, data_context, (), (override));
  MOCK_METHOD(iface::Protocol_encoder &, proto, (), (override));
  MOCK_METHOD(void, set_proto, (iface::Protocol_encoder *), (override));
  MOCK_METHOD(iface::Notice_configuration &, get_notice_configuration, (),
              (override));
  MOCK_METHOD(iface::Notice_output_queue &, get_notice_output_queue, (),
              (override));
  MOCK_METHOD(bool, get_prepared_statement_id, (const uint32_t, uint32_t *),
              (const, override));
  MOCK_METHOD(
      void, update_status,
      (ngs::Common_status_variables::Variable ngs::Common_status_variables::*),
      (override));
  MOCK_METHOD(iface::Document_id_aggregator &, get_document_id_aggregator, (),
              (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SESSION_H_
