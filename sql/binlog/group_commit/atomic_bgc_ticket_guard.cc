/* Copyright (c) 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/binlog/group_commit/atomic_bgc_ticket_guard.h"

namespace binlog {

AtomicBgcTicketGuard::AtomicBgcTicketGuard(AtomicBgcTicket &bgcTicket,
                                           const BgcTicket &next_value)
    : m_ref(bgcTicket), m_next_value(next_value) {
  std::tie(m_prev_value, std::ignore) = m_ref.get().set_in_use();
}

AtomicBgcTicketGuard::AtomicBgcTicketGuard(AtomicBgcTicket &bgcTicket,
                                           bool inc_next_before_release)
    : m_ref(bgcTicket) {
  std::tie(m_prev_value, m_next_value) =
      m_ref.get().set_in_use(false, inc_next_before_release);
}

AtomicBgcTicketGuard::~AtomicBgcTicketGuard() {
  assert(m_next_value.get() == (m_next_value.get() & BgcTicket::clear_bit));
  m_ref.get().set_used(m_next_value);
}

BgcTicket AtomicBgcTicketGuard::get_next() const { return m_next_value; }
BgcTicket AtomicBgcTicketGuard::get_prev() const { return m_prev_value; }
void AtomicBgcTicketGuard::set_next(const BgcTicket &next) {
  m_next_value = next;
}

}  // namespace binlog
