/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/binlog/group_commit/atomic_bgc_ticket.h"
#include <thread>

namespace binlog {

AtomicBgcTicket::AtomicBgcTicket(const BgcTicket &src) : m_ticket(src.get()) {}

AtomicBgcTicket::AtomicBgcTicket(const BgcTicket::ValueType &src)
    : m_ticket(src) {}

std::pair<BgcTicket, BgcTicket> AtomicBgcTicket::set_in_use(
    bool inc_next_before_lock, bool inc_next_before_release) {
  BgcTicket prev_ticket, next_ticket;
  while (true) {
    auto current_value =
        m_ticket->load(std::memory_order_acquire) & BgcTicket::clear_bit;
    prev_ticket = next_ticket = BgcTicket(current_value);
    BgcTicket in_use_ticket = prev_ticket;
    if (inc_next_before_lock) {
      in_use_ticket.set_next();
      next_ticket.set_next();
    } else if (inc_next_before_release) {
      next_ticket.set_next();
    }
    in_use_ticket.set_in_use();
    if (m_ticket->compare_exchange_strong(current_value, in_use_ticket.get(),
                                          std::memory_order_release)) {
      return std::make_pair(prev_ticket, next_ticket);
    }
    std::this_thread::yield();
  }
  return std::make_pair(prev_ticket, next_ticket);
}

std::ostream &operator<<(std::ostream &os, const AtomicBgcTicket &arg) {
  os << arg.load();
  return os;
}

BgcTicket AtomicBgcTicket::load() const { return BgcTicket(m_ticket->load()); }

void AtomicBgcTicket::store(const BgcTicket &value) {
  m_ticket->store(value.get());
}

AtomicBgcTicketGuard AtomicBgcTicket::scoped_lock(
    bool inc_next_before_release) {
  return AtomicBgcTicketGuard(*this, inc_next_before_release);
}

AtomicBgcTicketGuard AtomicBgcTicket::scoped_lock(const BgcTicket &next_value) {
  return AtomicBgcTicketGuard(*this, next_value);
}

void AtomicBgcTicket::set_used(const BgcTicket &next_value) {
  m_ticket->store(next_value.get() & BgcTicket::clear_bit);
}

}  // namespace binlog
