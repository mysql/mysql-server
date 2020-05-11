/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_MOCK_SRV_SESSION_SERVICES_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_MOCK_SRV_SESSION_SERVICES_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_dbug.h"          // NOLINT(build/include_subdir)
#include "my_thread_local.h"  // NOLINT(build/include_subdir)
#include "mysql/service_srv_session_bits.h"

#ifdef __cplusplus
class THD;
#define MYSQL_THD THD *
#else
#define MYSQL_THD void *
#endif

namespace xpl {
namespace test {

class Mock_srv_session {
 public:
  Mock_srv_session() {
    DBUG_ASSERT(nullptr == m_srv_session);
    m_srv_session = this;
  }
  ~Mock_srv_session() { m_srv_session = nullptr; }

  MOCK_METHOD1(init_session_thread, int(const void *plugin));
  MOCK_METHOD0(deinit_session_thread, void());
  MOCK_METHOD2(open_session,
               MYSQL_SESSION(srv_session_error_cb error_cb, void *plugix_ctx));
  MOCK_METHOD1(detach_session, int(MYSQL_SESSION session));
  MOCK_METHOD1(close_session, int(MYSQL_SESSION session));
  MOCK_METHOD0(server_is_available, int());
  MOCK_METHOD2(attach_session,
               int(MYSQL_SESSION session, MYSQL_THD *ret_previous_thd));

  static Mock_srv_session *m_srv_session;
};

class Mock_srv_session_info {
 public:
  Mock_srv_session_info() {
    DBUG_ASSERT(nullptr == m_srv_session_info);
    m_srv_session_info = this;
  }
  ~Mock_srv_session_info() { m_srv_session_info = nullptr; }

  MOCK_METHOD1(get_session_id, my_thread_id(MYSQL_SESSION session));

  static Mock_srv_session_info *m_srv_session_info;
};

}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_MOCK_SRV_SESSION_SERVICES_H_
