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

#include "mysql/gtid/tag.h"
#include <assert.h>
#include <cctype>
#include <cstring>
#include <iostream>
#include "mysql/gtid/internal/parsing_helpers.h"
#include "mysql/gtid/tag_plain.h"
#include "mysql/serialization/primitive_type_codec.h"

using namespace std;

using Field_size = mysql::serialization::Field_size;

template <class T>
using Primitive_type_codec = mysql::serialization::Primitive_type_codec<T>;

template <Field_size field_size>
using Byte_count_helper = mysql::serialization::Byte_count_helper<field_size>;

namespace mysql::gtid {

void Tag::replace(const char *text, std::size_t len) {
  m_data.clear();
  m_data.reserve(len);
  for (size_t result_pos = 0; result_pos < len; ++result_pos) {
    m_data.push_back(internal::tolower(text[result_pos]));
  }
}

Tag::Tag(const std::string &text) { std::ignore = from_string(text); }

std::string Tag::to_string() const { return m_data; }

std::size_t Tag::to_string(char *out) const {
  if (is_empty()) {
    return 0;
  }
  memcpy(out, m_data.data(), m_data.length());
  out[m_data.length()] = '\0';
  return m_data.length();
}

std::size_t Tag::encode_tag(unsigned char *buf,
                            const Gtid_format &gtid_format) const {
  if (gtid_format == Gtid_format::untagged) {
    assert(is_empty());
    return 0;
  }
  return Primitive_type_codec<std::string>::write_bytes<tag_max_length>(buf,
                                                                        m_data);
}

std::size_t Tag::decode_tag(const unsigned char *buf, std::size_t buf_len,
                            const Gtid_format &gtid_format) {
  m_data.clear();
  std::size_t bytes_read = 0;
  if (gtid_format == Gtid_format::untagged) {
    return bytes_read;
  }
  Tag unverified_tag;
  bytes_read = Primitive_type_codec<std::string>::read_bytes<tag_max_length>(
      buf, buf_len, unverified_tag.m_data);
  auto characters_valid = from_cstring(unverified_tag.m_data.data());
  if (characters_valid == unverified_tag.get_length()) {
    return bytes_read;
  }
  return 0;
}

size_t Tag::get_encoded_length(const Gtid_format &gtid_format) const {
  if (gtid_format == Gtid_format::tagged) {
    return Byte_count_helper<tag_max_length>::count_write_bytes(m_data);
  }
  // untagged GTIDs
  assert(m_data.length() == 0);
  return 0;
}

std::size_t Tag::get_length() const { return m_data.length(); }

bool Tag::is_valid_end_char(const char &character) {
  return character == gtid_separator || character == '\0' ||
         character == gtid_set_separator;
}

size_t Tag::from_string(const std::string &text) {
  return from_cstring(text.c_str());
}

bool Tag::is_character_valid(const char &character, std::size_t pos) {
  return (internal::isalpha(character) || character == '_' ||
          (internal::isdigit(character) && pos > 0));
}

size_t Tag::from_cstring(const char *text) {
  m_data.clear();
  size_t result_len = 0, pos = 0;
  while (internal::isspace(text[pos])) {
    ++pos;
  }
  size_t start_pos = pos;
  while (is_character_valid(text[pos], result_len) &&
         result_len < tag_max_length) {
    ++result_len;
    ++pos;
  }
  while (internal::isspace(text[pos])) {
    ++pos;
  }
  if (is_valid_end_char(text[pos]) == false) {
    // invalid tag specification, return 0
    return 0;
  }
  replace(text + start_pos, result_len);
  return pos;
}

bool Tag::operator==(const Tag &other) const { return m_data == other.m_data; }

bool Tag::operator!=(const Tag &other) const { return !(*this == other); }

Tag::Tag(const Tag_plain &tag) {
  m_data = "";
  if (tag.is_defined()) {
    m_data.assign(reinterpret_cast<const char *>(tag.data()));
  }
}

size_t Tag::Hash::operator()(const Tag &arg) const {
  return std::hash<std::string>{}(arg.m_data);
}

}  // namespace mysql::gtid
