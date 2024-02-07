// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "sql/changestreams/index/locked_sidno_set.h"

namespace cs::index {

Locked_sidno_set::Locked_sidno_set(Gtid_state &gtid_state)
    : m_gtid_state(gtid_state) {}

void Locked_sidno_set::add_lock_for_sidno(rpl_sidno sidno) {
  assert(!m_locked);
  m_sidno_set.insert(sidno);
}

void Locked_sidno_set::lock() {
  if (m_locked == false) {
    m_locked = true;
    for (const auto &sidno : m_sidno_set) {
      m_gtid_state.get().lock_sidno(sidno);
    }
  }
}

Locked_sidno_set::~Locked_sidno_set() {
  if (m_locked == true) {
    for (const auto &sidno : m_sidno_set) {
      m_gtid_state.get().unlock_sidno(sidno);
    }
  }
}

}  // namespace cs::index
