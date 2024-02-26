/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_EXPECT_EXPECT_H_
#define PLUGIN_X_SRC_EXPECT_EXPECT_H_

#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "plugin/x/src/expect/expect_condition.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace xpl {

class Expectation {
 public:
  using Expect_condition_ptr = std::unique_ptr<Expect_condition>;

 public:
  Expectation() : m_fail_on_error(false) {}
  Expectation(const Expectation &other);

  /*
    Make a copy of "other" element while calling assignment operator.
    This way we do not need temporary variable while swapping.
  */
  Expectation &operator=(Expectation other);

  // whether an error occurred previously in a no_error block
  void set_failed(const ngs::Error_code &error) { m_error = error; }
  bool failed() const { return m_error; }
  bool fail_on_error() const { return m_fail_on_error; }
  ngs::Error_code error() const;

  ngs::Error_code check_conditions();

  void unset(const uint32_t key, const std::string &value);
  ngs::Error_code set(const uint32_t key, const std::string &value);

  void add_condition(Expect_condition_ptr condition);

 private:
  static void swap(Expectation &one, Expectation &other);

  std::deque<Expect_condition_ptr> m_conditions;
  ngs::Error_code m_error;
  bool m_fail_on_error;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_EXPECT_EXPECT_H_
