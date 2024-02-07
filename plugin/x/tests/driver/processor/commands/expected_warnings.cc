/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/processor/commands/expected_warnings.h"

#include <algorithm>
#include <iterator>

bool Expected_warnings::check_warnings(const Warnings &warnings) {
  bool report_ok = true;
  if (m_fatal_warnings && m_expect_wno.empty()) {
    // Exepect no warnings !
    m_expect_wno.insert(0);
    report_ok = false;
  }

  if (m_expect_wno.empty()) return true;

  if (1 == m_expect_wno.size() && m_expect_wno.count(0) > 0 &&
      !warnings.empty()) {
    print_unexpected_warning(warnings[0]);
    clear_expectations();
    return !m_fatal_errors;
  }

  bool is_unexpected = false;

  for (const auto &w : warnings) {
    if (!check(w)) {
      is_unexpected = true;
      break;
    }
  }

  if (!is_unexpected) {
    for (const auto wno : m_expect_wno) {
      if (0 != wno && std::none_of(warnings.begin(), warnings.end(),
                                   [wno](const Warning &w) {
                                     return static_cast<int>(w.m_code) == wno;
                                   })) {
        m_console.print("Was expecting ", wno, ", still it was not reported.");
        clear_expectations();
        return !m_fatal_errors;
      }
    }
  }

  if (is_unexpected) {
    clear_expectations();
    return !m_fatal_errors;
  }

  if (report_ok) print_expected_warnings();
  clear_expectations();
  return true;
}

bool Expected_warnings::check(const Warning &warning) {
  if (m_expect_wno.find(warning.m_code) == m_expect_wno.end()) {
    print_unexpected_warning(warning);
    return false;
  }

  return true;
}

void Expected_warnings::clear_expectations() { m_expect_wno.clear(); }

void Expected_warnings::print_unexpected_warning(const Warning &warning) {
  m_console.print_error_red(
      *m_stack, "Got unexpected warning: (", warning, "); expected was",
      (m_expect_wno.size() > 1 ? " one of: " : " "), m_expect_wno, "\n");
}

void Expected_warnings::print_expected_warnings() {
  if (m_expect_wno.size() == 1)
    m_console.print("Got expected warning: ", m_expect_wno, "\n");
  else
    m_console.print("Got expected warnings: ", m_expect_wno, "\n");
}
