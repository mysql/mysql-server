/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "targeted_stringstream.h"

namespace raii {

Targeted_stringstream::Targeted_stringstream(
    std::string &target, const std::string &suffix,
    const std::function<void(const std::string &)> &callback)
    : m_active(true),
      m_target(target),
      m_suffix(suffix),
      m_callback(callback) {}

Targeted_stringstream::Targeted_stringstream(
    Targeted_stringstream &&other) noexcept
    : m_active(true),
      m_target(other.m_target),
      m_suffix(std::move(other.m_suffix)),
      m_stream(std::move(other.m_stream)),
      m_callback(std::move(other.m_callback)) {
  other.m_active = false;
}

Targeted_stringstream &Targeted_stringstream::operator=(
    Targeted_stringstream &&other) noexcept {
  m_active = true;
  m_target = other.m_target;
  m_suffix = std::move(other.m_suffix);
  m_stream = std::move(other.m_stream);
  m_callback = std::move(other.m_callback);
  other.m_active = false;
  return *this;
}

Targeted_stringstream::~Targeted_stringstream() {
  if (m_active) {
    m_stream << m_suffix;
    m_target.assign(m_stream.str());
    m_callback(m_target);
  }
}

}  // namespace raii
