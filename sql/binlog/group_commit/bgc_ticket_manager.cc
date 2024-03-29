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

#include "sql/binlog/group_commit/bgc_ticket_manager.h"
#include "sql/debug_sync.h"
#include "sql/sql_class.h"

binlog::Bgc_ticket_manager &binlog::Bgc_ticket_manager::coalesce() {
  AtomicBgcTicketGuard back_ticket_guard = m_back_ticket.scoped_lock(true);
  auto back_ticket = back_ticket_guard.get_prev();
  auto next_back_ticket = back_ticket_guard.get_next();
  AtomicBgcTicketGuard front_ticket_guard = m_front_ticket.scoped_lock();
  front_ticket_guard.set_next(next_back_ticket);

  do {
    this->m_sessions_per_ticket.pop();
  } while (this->m_sessions_per_ticket.get_state() !=
           queue_type::enum_queue_state::NO_MORE_ELEMENTS);
  this->m_back_ticket_sessions_count = 0;
  this->m_front_ticket_processed_sessions_count = 0;
  this->m_coalesced_ticket.store(back_ticket);
  return (*this);
}

binlog::BgcTicket binlog::Bgc_ticket_manager::assign_session_to_ticket() {
  AtomicBgcTicketGuard back_ticket_guard = m_back_ticket.scoped_lock();
  ++this->m_back_ticket_sessions_count;
  return back_ticket_guard.get_next();
}

binlog::Bgc_ticket_manager::queue_value_type
binlog::Bgc_ticket_manager::add_processed_sessions_to_front_ticket(
    queue_value_type sessions_to_add,
    const binlog::BgcTicket &ticket [[maybe_unused]]) {
  AtomicBgcTicketGuard front_ticket_guard = m_front_ticket.scoped_lock();
  assert(front_ticket_guard.get_prev() == ticket);
  this->m_front_ticket_processed_sessions_count += sessions_to_add;
  return m_front_ticket_processed_sessions_count;
}

binlog::BgcTicket binlog::Bgc_ticket_manager::get_front_ticket() const {
  return this->m_front_ticket.load();
}

binlog::BgcTicket binlog::Bgc_ticket_manager::get_back_ticket() const {
  return this->m_back_ticket.load();
}

binlog::BgcTicket binlog::Bgc_ticket_manager::get_coalesced_ticket() const {
  return this->m_coalesced_ticket.load();
}

std::pair<binlog::BgcTicket, binlog::BgcTicket>
binlog::Bgc_ticket_manager::push_new_ticket(BgcTmOptions options) {
  AtomicBgcTicketGuard back_ticket_guard = m_back_ticket.scoped_lock();
  auto prev_ticket = back_ticket_guard.get_prev();
  if (options == BgcTmOptions::inc_session_count)
    ++this->m_back_ticket_sessions_count;

  if (this->m_back_ticket_sessions_count != 0) {
    this->m_sessions_per_ticket.push(this->m_back_ticket_sessions_count);
    this->m_back_ticket_sessions_count = 0;
    assert(this->m_sessions_per_ticket.get_state() !=
           queue_type::enum_queue_state::NO_SPACE_AVAILABLE);
    back_ticket_guard.set_next(prev_ticket.next());
  }
  return std::make_pair(prev_ticket, back_ticket_guard.get_next());
}

std::pair<binlog::BgcTicket, binlog::BgcTicket>
binlog::Bgc_ticket_manager::pop_front_ticket(BgcTmOptions options) {
  AtomicBgcTicketGuard front_ticket_guard = m_front_ticket.scoped_lock();
  BgcTicket prev_front = front_ticket_guard.get_prev();
  if (options == BgcTmOptions::inc_session_count)
    ++this->m_front_ticket_processed_sessions_count;

  BgcTicket::ValueType front_ticket_sessions =
      this->m_sessions_per_ticket.front();
  if (prev_front != this->m_back_ticket.load() &&
      this->m_front_ticket_processed_sessions_count == front_ticket_sessions) {
    this->m_front_ticket_processed_sessions_count = 0;
    this->m_sessions_per_ticket.pop();
    front_ticket_guard.set_next(prev_front.next());  // increment on release
  }
  return {prev_front, front_ticket_guard.get_next()};
}

std::string binlog::Bgc_ticket_manager::to_string() const {
  std::ostringstream oss;
  this->to_string(oss);
  return oss.str();
}

void binlog::Bgc_ticket_manager::to_string(std::ostream &out) const {
  out << "Bgc_ticket_manager (" << std::hex << this << std::dec
      << "):" << std::endl
      << " · m_back_ticket: " << this->m_back_ticket << std::endl
      << " · m_front_ticket: " << this->m_front_ticket << std::endl
      << " · m_coalesced_ticket: " << this->m_coalesced_ticket << std::endl
      << " · m_back_ticket_sessions_count: "
      << this->m_back_ticket_sessions_count << std::endl
      << " · m_front_ticket_processed_sessions_count: "
      << this->m_front_ticket_processed_sessions_count << std::endl
      << " · m_sessions_per_ticket: " << this->m_sessions_per_ticket
      << std::flush;
}

binlog::Bgc_ticket_manager &binlog::Bgc_ticket_manager::instance() {
  static binlog::Bgc_ticket_manager manager;
  return manager;
}
