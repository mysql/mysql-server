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

#include "sql/changestreams/apply/core/managed_event.h"

namespace mysql::csa {

Managed_event::Managed_event(const Log_event_ptr &ev, bool ev_owns_memory)
    : m_event(ev) {
  if (ev_owns_memory) {
    ev->claim_memory_ownership(false);
    ev->claim_at_destruction(true);
  }
}

Managed_event::Log_event_ptr &Managed_event::get_event() { return m_event; }

const Managed_event::Log_event_ptr &Managed_event::get_event() const {
  return m_event;
}

void Managed_event::set_last_in_transaction(bool is_last) {
  m_is_last_in_transaction = is_last;
}

bool Managed_event::is_last_in_transaction() const {
  return m_is_last_in_transaction;
}

Managed_event::operator bool() const noexcept {
  return m_event.operator bool();
}

}  // namespace mysql::csa
