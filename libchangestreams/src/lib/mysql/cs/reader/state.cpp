/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "libchangestreams/include/mysql/cs/reader/state.h"
#include "my_byteorder.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/statement_events.h"
#include "mysql/gtids/gtids.h"  // Gtid_set

namespace cs::reader {

State &State::operator=(const State &other) {
  if (m_gtid_set.assign(other.get_gtids()) != mysql::utils::Return_status::ok)
    throw std::bad_alloc{};
  return *this;
}

State::State(const State &other) {
  if (m_gtid_set.assign(other.get_gtids()) != mysql::utils::Return_status::ok)
    throw std::bad_alloc{};
}

std::string State::to_string() const {
  return mysql::strconv::throwing::encode_text(m_gtid_set);
}

void State::reset() { m_gtid_set.clear(); }

const mysql::gtids::Gtid_set &State::get_gtids() const { return m_gtid_set; }

void State::add_gtid_set(const mysql::gtids::Gtid_set &gtids) {
  if (m_gtid_set.inplace_union(gtids) != mysql::utils::Return_status::ok)
    throw std::bad_alloc{};
}

void State::add_gtid(const mysql::gtids::Gtid &gtid) {
  if (m_gtid_set.insert(gtid) != mysql::utils::Return_status::ok)
    throw std::bad_alloc{};
}

std::ostringstream &operator<<(std::ostringstream &out, const State &in) {
  out << mysql::strconv::throwing::encode_text(in.get_gtids()) << std::flush;
  return out;
}

std::stringstream &operator<<(std::stringstream &out, const State &in) {
  out << mysql::strconv::throwing::encode_text(in.get_gtids()) << std::flush;
  return out;
}

bool State::operator==(const State &other) const {
  return other.get_gtids() == m_gtid_set;
}

}  // namespace cs::reader
