/*
 *
 Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gtest/gtest.h>
#include <memory>

#include "session_t.h"


namespace xcl {
namespace test {

TEST_F(Xcl_session_impl_tests, xsession_set_capability_successful) {
  const bool value_bool = true;

  ASSERT_FALSE(m_sut->set_capability(
      XSession::Capability_can_handle_expired_password,
      value_bool));
}

TEST_F(Xcl_session_impl_tests, xsession_set_capability_unsuccessful) {
  const char *value_pchar = "pchar";
  const std::string value_string = "std::string";
  const int64_t value_int = 10;

  ASSERT_TRUE(m_sut->set_capability(
      XSession::Capability_can_handle_expired_password,
      value_pchar));

  ASSERT_TRUE(m_sut->set_capability(
      XSession::Capability_can_handle_expired_password,
      value_string));

  ASSERT_TRUE(m_sut->set_capability(
      XSession::Capability_can_handle_expired_password,
      value_int));
}


}  // namespace test
}  // namespace xcl
