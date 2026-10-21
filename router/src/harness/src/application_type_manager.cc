/*
Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/application_type_manager.h"

#include <cassert>
#include <stdexcept>

namespace mysql_harness {

ApplicationTypeManager &ApplicationTypeManager::instance() {
  static ApplicationTypeManager instance_;
  return instance_;
}

void ApplicationTypeManager::set(std::string_view type) {
  if (m_app_type.has_value()) {
    assert(false && "Application should set this value once.");
    throw std::logic_error("Application type was already set");
  }

  m_app_type = std::string(type);
}

const std::string &ApplicationTypeManager::get() const {
  if (!m_app_type.has_value()) {
    assert(false && "Application should set value at start.");
    throw std::logic_error("Application type was not set before calling get()");
  }

  return m_app_type.value();
}

void ApplicationTypeManager::reset() { m_app_type.reset(); }

}  // namespace mysql_harness
