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

#include "mysql/gtid/tsid.h"
#include <sstream>
#include "mysql/gtid/gtid.h"
#include "mysql/gtid/internal/parsing_helpers.h"
#include "mysql/gtid/tsid_plain.h"

using namespace std;

namespace mysql::gtid {

string Tsid::to_string() const {
  stringstream ss;
  ss << m_uuid.to_string();
  if (is_tagged()) {
    ss << tsid_separator << m_tag.to_string();
  }
  return ss.str();
}

std::size_t Tsid::to_string(char *out) const {
  return to_string(out, tsid_separator);
}

std::size_t Tsid::to_string(char *out, const char *tag_sid_separator) const {
  std::size_t length = 0;
  length += m_uuid.to_string(out);
  if (is_tagged()) {
    memcpy(out + length, tag_sid_separator, strlen(tag_sid_separator));
    length += strlen(tag_sid_separator);
    length += m_tag.to_string(out + length);
  }
  return length;
}

bool Tsid::operator==(const Tsid &other) const {
  return m_uuid == other.m_uuid && m_tag == other.m_tag;
}

bool Tsid::operator!=(const Tsid &other) const { return !(*this == other); }

bool Tsid::operator<(const Tsid &other) const {
  return m_uuid < other.m_uuid ||
         (m_uuid == other.m_uuid && m_tag < other.m_tag);
}

std::size_t Tsid::from_cstring(const char *text) {
  std::size_t characters_read = 0;
  if (m_uuid.parse(text + characters_read, Uuid::TEXT_LENGTH) != 0) {
    return 0;
  }
  characters_read += Uuid::TEXT_LENGTH;
  while (internal::isspace(text[characters_read])) {
    ++characters_read;
  }
  if (text[characters_read] == '\0' ||
      text[characters_read] == gtid_set_separator) {
    return characters_read;
  }
  if (text[characters_read] != gtid_separator) {
    return 0;
  }
  std::size_t characters_after_sep = 1;
  while (internal::isspace(text[characters_read + characters_after_sep])) {
    ++characters_after_sep;
  }
  characters_read +=
      m_tag.from_cstring(text + characters_read + characters_after_sep);
  if (m_tag.is_empty()) {
    return characters_read;
  }
  return characters_read + characters_after_sep;
}

std::size_t Tsid::encode_tsid(unsigned char *buf,
                              const Gtid_format &gtid_format) const {
  std::size_t bytes_written = Uuid::BYTE_LENGTH;
  m_uuid.copy_to(buf);
  bytes_written += m_tag.encode_tag(buf + bytes_written, gtid_format);
  return bytes_written;
}

std::size_t Tsid::decode_tsid(const unsigned char *stream,
                              std::size_t stream_len,
                              const Gtid_format &gtid_format) {
  if (stream_len < Uuid::BYTE_LENGTH) {
    return 0;
  }
  std::size_t bytes_read = Uuid::BYTE_LENGTH;
  m_uuid.copy_from(stream);
  bytes_read += m_tag.decode_tag(stream + bytes_read, stream_len - bytes_read,
                                 gtid_format);
  if (bytes_read == Uuid::BYTE_LENGTH && gtid_format == Gtid_format::tagged) {
    bytes_read = 0;  // error case, invalid tag encoding
  }
  return bytes_read;
}

Tsid::Tsid(const Uuid &uuid, const Tag &tag) : m_uuid(uuid), m_tag(tag) {}

Tsid::Tsid(const Uuid &uuid) : m_uuid(uuid) {}

Tsid::Tsid(const Tsid_plain &arg) : m_uuid(arg.m_uuid), m_tag(arg.m_tag) {}

void Tsid::clear() {
  m_uuid.clear();
  m_tag = Tag();
}

}  // namespace mysql::gtid
