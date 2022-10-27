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

#include "sql/binlog/group_commit/bgc_ticket.h"
#include <iostream>

namespace binlog {

BgcTicket::BgcTicket(const ValueType &ticket) : m_ticket(ticket) {}

bool BgcTicket::is_set() const {
  return (m_ticket & clear_bit) != kTicketUnset;
}

BgcTicket::ValueType BgcTicket::get_value() const {
  return m_ticket & clear_bit;
}
void BgcTicket::set_in_use() { m_ticket |= set_bit; }
void BgcTicket::set_used() { m_ticket &= clear_bit; }
bool BgcTicket::is_in_use() const { return m_ticket & set_bit; }

BgcTicket BgcTicket::next() const {
  auto current = get_value();
  if (current == max_ticket_value) {
    current = first_ticket_value - 1;
  }
  return BgcTicket(current + 1);
}

std::ostream &operator<<(std::ostream &os, const BgcTicket &arg) {
  os << arg.get_value() << "/" << arg.is_in_use();
  return os;
}

void BgcTicket::set_next() { m_ticket = next().get(); }

bool operator<(const BgcTicket &lhs, const BgcTicket &rhs) {
  return (lhs.get() & BgcTicket::clear_bit) <
         (rhs.get() & BgcTicket::clear_bit);
}

bool operator>(const BgcTicket &lhs, const BgcTicket &rhs) { return rhs < lhs; }

bool operator>=(const BgcTicket &lhs, const BgcTicket &rhs) {
  return !(lhs < rhs);
}

bool operator==(const BgcTicket &lhs, const BgcTicket &rhs) {
  return (lhs.get() & BgcTicket::clear_bit) ==
         (rhs.get() & BgcTicket::clear_bit);
}

bool operator!=(const BgcTicket &lhs, const BgcTicket &rhs) {
  return (lhs.get() & BgcTicket::clear_bit) !=
         (rhs.get() & BgcTicket::clear_bit);
}

}  // namespace binlog
