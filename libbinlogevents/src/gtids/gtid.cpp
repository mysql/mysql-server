/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "libbinlogevents/include/gtids/gtid.h"
#include <string>
#include "libbinlogevents/include/uuid.h"

namespace binary_log::gtids {

Gtid::Gtid(const Uuid &uuid, gno_t seqno) : m_uuid(uuid), m_gno(seqno) {}

Gtid::~Gtid() = default;

gno_t Gtid::get_gno() const { return m_gno; }
const Uuid &Gtid::get_uuid() const { return m_uuid; }

std::string Gtid::to_string() const {
  std::stringstream ss;
  ss << m_uuid.to_string() << SEPARATOR_UUID_SEQNO << m_gno;
  return ss.str();
}

Gtid &Gtid::operator=(const Gtid &other) {
  m_gno = other.get_gno();
  m_uuid.copy_from(other.get_uuid());

  return *this;
}

Gtid::Gtid(const Gtid &other) { *this = other; }

bool Gtid::operator==(const Gtid &other) const {
  return other.get_uuid().equals(m_uuid) && other.get_gno() == m_gno;
}

bool Gtid::operator!=(const Gtid &other) const {
  return other.get_gno() != m_gno || !other.get_uuid().equals(m_uuid);
}

}  // namespace binary_log::gtids
