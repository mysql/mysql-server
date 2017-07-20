/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "expected_error.h"

#include <algorithm>
#include <iterator>


bool Expected_error::check_error(const xcl::XError &err) {
  if (!m_expect_errno.empty()) return check(err);
  m_console.print_error(*m_stack, err, '\n');
  return !m_fatal_errors;
}

bool Expected_error::check_ok() {
  if (m_expect_errno.empty()) return true;
  return check(xcl::XError());
}

bool Expected_error::check(const xcl::XError &err) {
  if (m_expect_errno.find(err.error()) == m_expect_errno.end()) {
    print_unexpected_error(err);
    m_expect_errno.clear();
    return !m_fatal_errors;
  }

  print_expected_error(err);
  m_expect_errno.clear();
  return true;
}

void Expected_error::print_unexpected_error(const xcl::XError &err) {
  m_console.print_error_red(
      *m_stack, "Got unexpected error: ", err, "; expected was",
      (m_expect_errno.size() > 1 ? " one of: " : " "), m_expect_errno);
}

void Expected_error::print_expected_error(const xcl::XError &err) {
  m_console.print("Got expected error");
  if (m_expect_errno.size() == 1) {
    if (err.error()) m_console.print(": ");
    m_console.print(err, "\n");
  } else {
    m_console.print(" (one of: ", m_expect_errno, ")\n");
  }
}
