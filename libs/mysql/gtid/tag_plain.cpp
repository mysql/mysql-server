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

#include "mysql/gtid/tag_plain.h"
#include <cstring>
#include "mysql/gtid/tag.h"  // Tag member functions

using namespace std;

namespace mysql::gtid {

std::size_t Tag_plain::to_string(char *buf) const {
  auto len = length();
  memcpy(buf, m_data, len);
  return len;
}

void Tag_plain::clear() { m_data[0] = 0; }

bool Tag_plain::is_defined() const { return m_data[0] != 0; }

std::size_t Tag_plain::length() const {
  return strlen(reinterpret_cast<const char *>(m_data));
}

Tag_plain::Tag_plain(const Tag &tag) { set(tag); }

void Tag_plain::set(const Tag &tag) {
  clear();
  if (tag.is_defined()) {
    auto len = tag.get_data().length();
    memcpy(m_data, tag.get_data().data(), len);
    m_data[len] = '\0';
  }
}

const unsigned char *Tag_plain::data() const { return m_data; }

}  // namespace mysql::gtid
