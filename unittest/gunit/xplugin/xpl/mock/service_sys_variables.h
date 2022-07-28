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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVICE_SYS_VARIABLES_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVICE_SYS_VARIABLES_H_

#include <gmock/gmock.h>

#include "plugin/x/src/interface/service_sys_variables.h"

namespace xpl {
namespace test {
namespace mock {

class Service_sys_variables : public iface::Service_sys_variables {
 public:
  Service_sys_variables();
  virtual ~Service_sys_variables() override;

  MOCK_METHOD(bool, get_variable,
              (const char *component_name, const char *name, void **val,
               size_t *out_length_of_val),
              (override));
  MOCK_METHOD(bool, is_valid, (), (const, override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SERVICE_SYS_VARIABLES_H_
