// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_UUIDS_UUID_H
#define MYSQL_UUIDS_UUID_H

/// @file
/// Experimental API header

#include <array>                    // array
#include <cstring>                  // memcpy, size_t
#include <string_view>              // string_view
#include <type_traits>              // is_trivially_default_constructible_v
#include <utility>                  // ignore
#include "mysql/strconv/strconv.h"  // Parser

/// @addtogroup GroupLibsMysqlUuids
/// @{

namespace mysql::uuids {

/// Holds data for a UUID
///
/// This satisfies std::is_trivially_default_constructible,
/// std::is_trivially_copyable, and std::is_standard_layout.
///
/// Use mysql::strconv::encode_text / mysql::strconv::decode_text to convert to
/// and from text format.
class Uuid : public mysql::ranges::Buffer_interface<Uuid> {
 public:
  /// The number of bytes in the data of a Uuid.
  static constexpr std::size_t byte_size = 16;
  static constexpr std::size_t text_size = 36;
  static constexpr std::size_t section_count = 5;
  static constexpr std::array<std::size_t, section_count> section_sizes = {
      4, 2, 2, 2, 6};

  /// Return the size
  [[nodiscard]] constexpr std::size_t size() const { return byte_size; }

  /// Return the data buffer as `char *`.
  [[nodiscard]] char *data() { return m_data.data(); }

  /// Return the data buffer as `const char *`.
  [[nodiscard]] const char *data() const { return m_data.data(); }

  /// Copy `other` to this.
  void assign(const Uuid &other) {
    if (&other == this) return;
    memcpy(data(), other.data(), size());
  }

  /// Copy `other`, which is represented as binary, to this.
  [[nodiscard]] mysql::utils::Return_status assign(
      const std::string_view &other) {
    if (other.size() != byte_size) return mysql::utils::Return_status::error;
    memcpy(data(), other.data(), size());
    return mysql::utils::Return_status::ok;
  }

 private:
  /// The data for this Uuid.
  std::array<char, byte_size> m_data;
};

static_assert(std::is_trivially_default_constructible_v<Uuid>);
static_assert(std::is_trivially_copyable_v<Uuid>);
static_assert(std::is_standard_layout_v<Uuid>);

}  // namespace mysql::uuids

namespace mysql::strconv {

/// Enable encode_text(Uuid).
template <Is_string_target Target_t>
void encode_impl(const Text_format &format [[maybe_unused]], Target_t &target,
                 const mysql::uuids::Uuid &uuid) {
  if constexpr (Target_t::target_type == Target_type::counter) {
    target.advance(mysql::uuids::Uuid::text_size);
  } else {
    bool first = true;
    const auto *data = uuid.data();
    const auto *data_pos = data;
    [[maybe_unused]] auto *target_start_pos = target.pos();
    for (auto size : mysql::uuids::Uuid::section_sizes) {
      if (!first) {
        target.write_char('-');
      }
      first = false;
      target.write(Hex_format{}, {data_pos, size});
      data_pos += size;
    }
    assert(data_pos == uuid.end());
    assert(target.pos() - target_start_pos == mysql::uuids::Uuid::text_size);
  }
}

/// Enable decode(Text_format, string, Uuid).
inline void decode_impl(const Text_format &format, Parser &parser,
                        mysql::uuids::Uuid &uuid) {
  using Uuid_t = mysql::uuids::Uuid;
  static constexpr auto return_ok = mysql::utils::Return_status::ok;

  // Cursor to output.
  auto *out = uuid.data();

  // Decode `2*size` hex digits from input into `size` bytes of output, and
  // advance the cursors into input and output.
  auto read_hex = [&](std::size_t size) {
    auto ret = parser.read_to_out_str(Repeat::exact(size) | Hex_format{},
                                      out_str_fixed_nz(out, size));
    out += size;
    return ret;
  };

  // Check for brace-enclosed format.
  parser.skip(format | Repeat::optional(), "{");
  bool brace_format = parser.is_found();

  // Whether we use hyphens or not. With braces, hyphens are mandatory;
  // without braces, hyphens are optional.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum { yes, no, unknown } hyphen_format = brace_format ? yes : unknown;

  bool first = true;
  for (const auto &section_size : Uuid_t::section_sizes) {
    if (!first) {
      if (hyphen_format == unknown) {
        parser.skip(format | Repeat::optional(), "-");
        hyphen_format = parser.is_found() ? yes : no;
      } else if (hyphen_format == yes) {
        if (parser.skip(format, "-") != return_ok) return;
      }
    }
    first = false;
    if (read_hex(section_size) != return_ok) return;
  }

  if (brace_format) {
    std::ignore = parser.skip(format, "}");  // return anyways
  }
}

/// Enable encode(Binary_format, Uuid).
void encode_impl(const Binary_format &, Is_string_target auto &target,
                 const mysql::uuids::Uuid &uuid) {
  target.write_raw(uuid.string_view());
}

/// Enable decode(Binary_format, Uuid).
inline void decode_impl(const Binary_format &, Parser &parser,
                        mysql::uuids::Uuid &uuid) {
  std::size_t size = mysql::uuids::Uuid::byte_size;
  if (parser.read_to_out_str(Fixstr_binary_format{size},
                             out_str_fixed_nz(uuid.data(), size)) !=
      mysql::utils::Return_status::ok)
    return;
  assert(size == mysql::uuids::Uuid::byte_size);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlUuids
/// @}

#endif  // ifndef MYSQL_UUIDS_UUID_H
