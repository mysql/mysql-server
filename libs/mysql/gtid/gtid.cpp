/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mysql/gtid/gtid.h"
#include <string>
#include "mysql/gtid/uuid.h"
#include "mysql/serialization/primitive_type_codec.h"

namespace mysql::gtid {

template <class T>
using Primitive_type_codec = mysql::serialization::Primitive_type_codec<T>;

Gtid::Gtid(const Tsid &tsid, gno_t seqno) : m_tsid(tsid), m_gno(seqno) {}

Gtid::~Gtid() = default;

gno_t Gtid::get_gno() const { return m_gno; }
const Uuid &Gtid::get_uuid() const { return m_tsid.get_uuid(); }

const Tsid &Gtid::get_tsid() const { return m_tsid; }

const Tag &Gtid::get_tag() const { return m_tsid.get_tag(); }

std::string Gtid::to_string() const {
  std::stringstream ss;
  ss << m_tsid.to_string() << separator_gtid << m_gno;
  return ss.str();
}

Gtid &Gtid::operator=(const Gtid &other) {
  m_gno = other.get_gno();
  m_tsid = other.m_tsid;
  return *this;
}

Gtid::Gtid(const Gtid &other) { *this = other; }

bool Gtid::operator==(const Gtid &other) const {
  return m_tsid == other.get_tsid() && other.get_gno() == m_gno;
}

bool Gtid::operator!=(const Gtid &other) const { return !(*this == other); }

std::size_t Gtid::encode_gtid_tagged(unsigned char *buf) const {
  std::size_t bytes_written = 0;
  bytes_written += m_tsid.encode_tsid(buf, Gtid_format::tagged);
  bytes_written +=
      Primitive_type_codec<int64_t>::write_bytes<0>(buf + bytes_written, m_gno);
  return bytes_written;
}

std::size_t Gtid::decode_gtid_tagged(const unsigned char *buf,
                                     std::size_t len) {
  std::size_t bytes_read = 0;
  bytes_read += m_tsid.decode_tsid(buf, len, Gtid_format::tagged);
  bytes_read += Primitive_type_codec<int64_t>::read_bytes<0>(
      buf + bytes_read, len - bytes_read, m_gno);
  return bytes_read;
}

}  // namespace mysql::gtid
