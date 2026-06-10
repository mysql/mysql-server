/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

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

#include "mysql/ranges/buffer_interface.h"  // Buffer_interface
#include "mysql/strconv/encode/out_str.h"   // Out_str_base
#include "mysql/utils/return_status.h"      // Return_status
#include "sql_string.h"                     // String

/// Output string wrapper for String
///
/// @see mysql/strconv/outstr.h
class Out_str_growable_mysql_string
    : public mysql::ranges::Buffer_interface<Out_str_growable_mysql_string>,
      public mysql::strconv::Out_str_base {
 public:
  static constexpr auto resize_policy = mysql::strconv::Resize_policy::growable;
  explicit Out_str_growable_mysql_string(String &str) noexcept : m_str(str) {}
  [[nodiscard]] std::size_t initial_capacity() const {
    return m_str.alloced_length();
  }
  [[nodiscard]] std::size_t size() const { return m_str.length(); }
  [[nodiscard]] char *data() const { return m_str.ptr(); }
  [[nodiscard]] mysql::utils::Return_status resize(std::size_t new_size) const {
    if (m_str.alloc(new_size)) return mysql::utils::Return_status::error;
    m_str.length(new_size);
    m_str.ptr()[new_size] = '\0';
    return mysql::utils::Return_status::ok;
  }

 private:
  String &m_str;
};

/// Return a new Output String Wrapper that wraps the given String.
///
/// This enables passing (wrapped) String objects to mysql::strconv::encode.
inline auto out_str_growable(String &str) {
  return Out_str_growable_mysql_string(str);
}
