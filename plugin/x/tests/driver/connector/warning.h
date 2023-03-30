/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_WARNING_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_WARNING_H_

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

class Warning {
 public:
  Warning(const std::string &text, const uint32_t code, const bool is_note)
      : m_text(text), m_code(code), m_is_note(is_note) {}

  std::string m_text;
  uint32_t m_code;
  bool m_is_note;
};

std::ostream &operator<<(std::ostream &os, const Warning &warning);

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_WARNING_H_
