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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SSL_CONTEXT_OPTIONS_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SSL_CONTEXT_OPTIONS_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/ssl_context_options.h"

namespace xpl {
namespace test {
namespace mock {

class Ssl_context_options : public iface::Ssl_context_options {
 public:
  Ssl_context_options();
  virtual ~Ssl_context_options() override;

  MOCK_METHOD(int64_t, ssl_ctx_verify_depth, (), (override));
  MOCK_METHOD(int64_t, ssl_ctx_verify_mode, (), (override));

  MOCK_METHOD(std::string, ssl_server_not_after, (), (override));
  MOCK_METHOD(std::string, ssl_server_not_before, (), (override));

  MOCK_METHOD(int64_t, ssl_sess_accept_good, (), (override));
  MOCK_METHOD(int64_t, ssl_sess_accept, (), (override));
  MOCK_METHOD(int64_t, ssl_accept_renegotiates, (), (override));

  MOCK_METHOD(std::string, ssl_session_cache_mode, (), (override));

  MOCK_METHOD(int64_t, ssl_session_cache_hits, (), (override));
  MOCK_METHOD(int64_t, ssl_session_cache_misses, (), (override));
  MOCK_METHOD(int64_t, ssl_session_cache_overflows, (), (override));
  MOCK_METHOD(int64_t, ssl_session_cache_size, (), (override));
  MOCK_METHOD(int64_t, ssl_session_cache_timeouts, (), (override));
  MOCK_METHOD(int64_t, ssl_used_session_cache_entries, (), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  // UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SSL_CONTEXT_OPTIONS_H_
