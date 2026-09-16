// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "mysql/scheduler/task_id.h"

#include <cassert>
#include <mutex>

namespace mysql::scheduler {

Task_id::Task_id(std::size_t id) : m_id(id), m_is_valid(true) {}

bool Task_id::operator==(const Task_id &src) const {
  return m_is_valid && this->m_id == src.m_id;
}

std::ostream &operator<<(std::ostream &os, const Task_id &obj) {
  if (!obj.is_valid()) {
    os << "INVALID";
  } else {
    os << obj.m_id;
  }
  return os;
}

bool Task_id::operator<(const Task_id &other) const {
  return m_id < other.m_id;
}

}  // namespace mysql::scheduler
