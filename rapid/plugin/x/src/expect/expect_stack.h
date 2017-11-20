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

#ifndef X_SRC_EXPECT_EXPECT_STACK_H
#define X_SRC_EXPECT_EXPECT_STACK_H

#include <vector>

#include "expect/expect.h"
#include "ngs_common/protocol_protobuf.h"
#include "ngs/error_code.h"


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

#endif  // X_SRC_EXPECT_EXPECT_STACK_H
