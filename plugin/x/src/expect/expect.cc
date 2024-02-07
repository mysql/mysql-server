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

#include "plugin/x/src/expect/expect.h"

#include <algorithm>
#include <string>
#include <utility>

#include "plugin/x/src/expect/expect_condition.h"
#include "plugin/x/src/expect/expect_condition_docid.h"
#include "plugin/x/src/expect/expect_condition_field.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

// :docnote:
// NO_ERROR means "enable exceptions", meaning any error that happens inside
// a block will cause all subsequent statements to fail until the matching close
// is found.
//
// Nesting behaviour:
//
// Case 1: No_error
//
// open(NO_ERROR) - ok
//   stmt1 - ok
//   stmt2 - error
//   stmt3 - fail
// close() - fail
//
// Case 2: Plain
//
// open() - ok
//   stmt1 - ok
//   stmt2 - error
//   stmt3 - ok
// close() - ok
//
// Case 3: No_error nested within no_error
//
// 3.1: error in outer block fails the whole thing
// open(NO_ERROR) - ok
//   stmt1 - error
//   open(NO_ERROR) - fail
//     stmt2 - fail
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// 3.2: error propagates up and fails the outer block
// open(NO_ERROR) - ok
//   stmt1 - ok
//   open(NO_ERROR) - ok
//     stmt2 - error
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// Case 4: Plain nested within no_error
//
// 4.1: same as 3.1
// open(NO_ERROR) - ok
//   stmt1 - error
//   open() - fail
//     stmt2 - fail
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// 4.2: plain block effectively "catches" the error and prevents it from failing
// the outer block
// open(NO_ERROR) - ok
//   stmt1 - ok
//   open() - ok
//     stmt2 - error
//   close() - ok
//   stmt3 - ok
// close() - ok
//
// Case 5: No_error nested within Plain
//
// 5.1: trivial
// open() - ok
//   stmt1 - error
//   open(NO_ERROR) - ok
//     stmt2 - ok
//   close() - ok
//   stmt3 - ok
// close() - ok
//
// 5.2: error propagates up, but is ignored by the outer block
// open() - ok
//   stmt1 - ok
//   open(NO_ERROR) - ok
//     stmt2 - error
//   close() - fail
//   stmt3 - ok
// close() - ok
//
// Case 6: Plain nested within plain: trivial, behaves like a flat plain block
//

Expectation::Expectation(const Expectation &other)
    // this is instance specific data, don't copy it
    : m_error(other.m_error), m_fail_on_error(other.m_fail_on_error) {
  for (const auto &cond : other.m_conditions) {
    m_conditions.push_back(cond->clone());
  }
}

void Expectation::swap(Expectation &one, Expectation &other) {
  using std::swap;

  swap(one.m_error, other.m_error);
  swap(one.m_fail_on_error, other.m_fail_on_error);
  swap(one.m_conditions, other.m_conditions);
}

Expectation &Expectation::operator=(Expectation other) {
  swap(*this, other);

  return *this;
}

ngs::Error_code Expectation::error() const { return m_error; }

ngs::Error_code Expectation::check_conditions() {
  for (auto &cond : m_conditions) {
    const auto error_code = cond->check_if_error();

    if (error_code) {
      set_failed(error_code);
      return error_code;
    }
  }

  return {};
}

void Expectation::unset(const uint32_t key, const std::string &value) {
  if (Mysqlx::Expect::Open_Condition_Key_EXPECT_NO_ERROR == key) {
    m_fail_on_error = false;
    return;
  }

  const bool ignore_value = value.empty();

  auto elements_to_remove = std::remove_if(
      m_conditions.begin(), m_conditions.end(),
      [ignore_value, &key, &value](const Expect_condition_ptr &cond) {
        return cond->key() == key && (ignore_value || cond->value() == value);
      });

  m_conditions.erase(elements_to_remove, m_conditions.end());
}

void Expectation::add_condition(Expect_condition_ptr cond) {
  m_conditions.emplace_back(std::move(cond));
}

ngs::Error_code Expectation::set(const uint32_t key, const std::string &value) {
  switch (key) {
    case Mysqlx::Expect::Open_Condition_Key_EXPECT_NO_ERROR:
      if (value == "1" || value.empty())
        m_fail_on_error = true;
      else if (value == "0")
        m_fail_on_error = false;
      else
        return ngs::Error_code(
            ER_X_EXPECT_BAD_CONDITION_VALUE,
            "Invalid value '" + value + "' for expectation no_error");
      break;

    case Mysqlx::Expect::Open_Condition_Key_EXPECT_FIELD_EXIST:
      add_condition(Expect_condition_ptr{new Expect_condition_field(value)});
      break;

    case Mysqlx::Expect::Open_Condition_Key_EXPECT_DOCID_GENERATED:
      add_condition(Expect_condition_ptr{new Expect_condition_docid()});
      break;

    default:
      return ngs::Error(ER_X_EXPECT_BAD_CONDITION, "Unknown condition key: %u",
                        static_cast<unsigned>(key));
  }

  return ngs::Error_code();
}

}  // namespace xpl
