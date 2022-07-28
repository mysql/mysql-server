/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_EXPECT_EXPECT_CONDITION_DOCID_H_
#define PLUGIN_X_SRC_EXPECT_EXPECT_CONDITION_DOCID_H_

#include "plugin/x/src/expect/expect_condition.h"

namespace xpl {

class Expect_condition_docid : public Expect_condition {
 public:
  Expect_condition_docid()
      : Expect_condition(
            Mysqlx::Expect::Open_Condition_Key_EXPECT_DOCID_GENERATED, "") {}
  Expect_condition_docid(const Expect_condition_docid &other) = default;

  Expect_condition_ptr clone() override {
    return Expect_condition_ptr{new Expect_condition_docid(*this)};
  }

  ngs::Error_code check_if_error() override { return ngs::Success(); }
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_EXPECT_EXPECT_CONDITION_DOCID_H_
