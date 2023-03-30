/*
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SRV_SESSION_SERVICES_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SRV_SESSION_SERVICES_H_

#include <assert.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// NOLINT(build/include_subdir)
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
namespace mock {

class Srv_session {
 public:
  Srv_session();
  ~Srv_session();

  MOCK_METHOD(int, init_session_thread, (const void *plugin));
  MOCK_METHOD(void, deinit_session_thread, ());
  MOCK_METHOD(MYSQL_SESSION, open_session,
              (srv_session_error_cb error_cb, void *plugix_ctx));
  MOCK_METHOD(int, detach_session, (MYSQL_SESSION session));
  MOCK_METHOD(int, close_session, (MYSQL_SESSION session));
  MOCK_METHOD(int, server_is_available, ());
  MOCK_METHOD(int, attach_session,
              (MYSQL_SESSION session, MYSQL_THD *ret_previous_thd));

  static Srv_session *m_srv_session;
};

class Srv_session_info {
 public:
  Srv_session_info();
  ~Srv_session_info();

  MOCK_METHOD(my_thread_id, get_session_id, (MYSQL_SESSION session));

  static Srv_session_info *m_srv_session_info;
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SRV_SESSION_SERVICES_H_
