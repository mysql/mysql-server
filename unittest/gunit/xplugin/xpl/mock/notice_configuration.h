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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_NOTICE_CONFIGURATION_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_NOTICE_CONFIGURATION_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/notice_configuration.h"

namespace xpl {
namespace test {
namespace mock {

class Notice_configuration : public iface::Notice_configuration {
 public:
  Notice_configuration();
  virtual ~Notice_configuration() override;

  MOCK_METHOD(bool, get_notice_type_by_name,
              (const std::string &name, ngs::Notice_type *out_notice_type),
              (const, override));
  MOCK_METHOD(bool, get_name_by_notice_type,
              (const ngs::Notice_type notice_type, std::string *out_name),
              (const, override));
  MOCK_METHOD(bool, is_notice_enabled, (const ngs::Notice_type notice_type),
              (const, override));
  MOCK_METHOD(void, set_notice,
              (const ngs::Notice_type notice_type,
               const bool should_be_enabled),
              (override));
  MOCK_METHOD(bool, is_any_dispatchable_notice_enabled, (), (const, override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_NOTICE_CONFIGURATION_H_
