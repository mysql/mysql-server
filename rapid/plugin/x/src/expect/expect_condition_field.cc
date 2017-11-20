/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "expect/expect_condition_field.h"
#include "xpl_error.h"
#include "xprotocol_tags.h"
#include "ngs_common/protocol_protobuf.h"

namespace xpl {

Expect_condition_field::Expect_condition_field(
    const std::string &value)
: Expect_condition(Mysqlx::Expect::Open_Condition_Key_EXPECT_FIELD_EXIST,
                   value) {
}

Expect_condition_field::Expect_condition_field(
    const Expect_condition_field &other)
: Expect_condition(other) {
}

Expect_condition_field::Expect_condition_ptr Expect_condition_field::clone() {
  return Expect_condition_ptr{ new Expect_condition_field(*this) };
}

ngs::Error_code Expect_condition_field::check_if_error() {
  static XProtocol_tags tags;

  if (!tags.is_chain_acceptable(value())) {
    return ngs::Error(
        ER_X_EXPECT_FIELD_EXISTS_FAILED,
        "Expectation failed: field_exists = '%s'", value().c_str());
  }

  return {};
}

}  // namespace xpl
