/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/expect/expect_condition_field.h"

#include "plugin/x/generated/xprotocol_tags.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

Expect_condition_field::Expect_condition_field(const std::string &value)
    : Expect_condition(Mysqlx::Expect::Open_Condition_Key_EXPECT_FIELD_EXIST,
                       value) {}

Expect_condition_field::Expect_condition_field(
    const Expect_condition_field &other) = default;

Expect_condition_field::Expect_condition_ptr Expect_condition_field::clone() {
  return Expect_condition_ptr{new Expect_condition_field(*this)};
}

ngs::Error_code Expect_condition_field::check_if_error() {
  static XProtocol_tags tags;

  if (!tags.is_chain_acceptable(value())) {
    return ngs::Error(ER_X_EXPECT_FIELD_EXISTS_FAILED,
                      "Expectation failed: field_exists = '%s'",
                      value().c_str());
  }

  return {};
}

}  // namespace xpl
