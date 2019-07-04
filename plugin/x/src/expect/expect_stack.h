/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_EXPECT_EXPECT_STACK_H_
#define PLUGIN_X_SRC_EXPECT_EXPECT_STACK_H_

#include <vector>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/expect/expect.h"

namespace xpl {

class Expectation_stack {
 public:
  Expectation_stack();

  ngs::Error_code open(const Mysqlx::Expect::Open &open);
  ngs::Error_code close();

  // Called before executing client statements,
  // should signal error if one is returned
  ngs::Error_code pre_client_stmt(const int8_t msgid);

  // called when an error occurs executing client statements
  void post_client_stmt(const int8_t msgid, const ngs::Error_code &stmt_error);
  void post_client_stmt_failed(const int8_t msgid);

 private:
  std::vector<Expectation> m_expect_stack;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_EXPECT_EXPECT_STACK_H_
