/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/expect/expect_stack.h"

#include <string>

#include "plugin/x/src/xpl_error.h"


namespace xpl {

Expectation_stack::Expectation_stack() {
  /*
   Reserve four elements inside the vector holding open
   expectation blocks. In most cases there is going to be
   single expectation block used. But to allow fast nesting
   of expectations the value should be greater than one.
   */
  m_expect_stack.reserve(4);
}

ngs::Error_code Expectation_stack::open(const Mysqlx::Expect::Open &open) {
  ngs::Error_code error;
  Expectation expect;

  // if we're in a failed expect block, push an empty failed expectation to
  // the stack so that it can be popped when the matching close is seen.
  // No other evaluations are done in a failed state.
  if (!m_expect_stack.empty()) {
    if (m_expect_stack.back().failed()) {
      expect.set_failed(m_expect_stack.back().error());
      m_expect_stack.push_back(expect);

      return expect.error();
    }

    if (open.op() == Mysqlx::Expect::Open::EXPECT_CTX_COPY_PREV)
      expect = m_expect_stack.back();
  }

  for (int i = 0; i < open.cond_size(); i++) {
    const Mysqlx::Expect::Open::Condition &cond(open.cond(i));
    const std::string &condition_value = cond.has_condition_value() ?
        cond.condition_value():
        "";

    switch (cond.op()) {
      case Mysqlx::Expect::Open::Condition::EXPECT_OP_SET:
        error = expect.set(cond.condition_key(), condition_value);
        break;
      case Mysqlx::Expect::Open::Condition::EXPECT_OP_UNSET:
        expect.unset(cond.condition_key(), condition_value);
        break;
    }

    if (error)
      return error;
  }
  // we need to add the expectation block even if an error occurred,
  // otherwise we'll get mismatched open/close blocks
  // on_error should get called afterwards with this error, which should
  // fail the rest of the block
  m_expect_stack.push_back(expect);

  // now check for the expected conditions
  // this may block if a blocking condition is found
  if (!error) {
    error = m_expect_stack.back().check_conditions();
  }

  return error;
}

ngs::Error_code Expectation_stack::close() {
  if (m_expect_stack.empty())
    return ngs::Error_code(
        ER_X_EXPECT_NOT_OPEN,
        "Expect block currently not open");

  if (m_expect_stack.back().failed()) {
    const auto error = m_expect_stack.back().error();
    m_expect_stack.pop_back();

    return error;
  }

  m_expect_stack.pop_back();

  return ngs::Error_code();
}

ngs::Error_code Expectation_stack::pre_client_stmt(const int8_t msgid) {
  if (!m_expect_stack.empty() &&
      m_expect_stack.back().failed()) {
    // special handling for nested expect blocks
    // if a block open or close arrives in a failed state, we let it through
    // so that they can be pushed/popped on the stack and properly accounted
    switch (msgid) {
      case Mysqlx::ClientMessages::EXPECT_OPEN:   // fall through
      case Mysqlx::ClientMessages::EXPECT_CLOSE:
        break;

      default:
        return m_expect_stack.back().error();
    }
  }

  return ngs::Error_code();
}

// called after executing client statements
void Expectation_stack::post_client_stmt(
    const int8_t msgid,
    const ngs::Error_code &stmt_error) {
  if (stmt_error)
    post_client_stmt_failed(msgid);
}

void Expectation_stack::post_client_stmt_failed(
    const int8_t) {
  if (m_expect_stack.empty())
    return;

  auto &last_expect = m_expect_stack.back();

  if (last_expect.fail_on_error() &&
      !last_expect.error()) {
    const ngs::Error_code error(
        ER_X_EXPECT_NO_ERROR_FAILED,
        "Expectation failed: no_error");
    last_expect.set_failed(error);
  }
}

}  // namespace xpl
