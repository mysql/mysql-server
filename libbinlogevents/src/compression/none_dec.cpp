/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include <compression/none_dec.h>

#include <cstring>  // std::memcpy

namespace binary_log::transaction::compression {

type None_dec::do_get_type_code() const { return type_code; }

void None_dec::do_reset() {
  m_input_data = nullptr;
  m_input_size = 0;
  m_input_position = 0;
}

void None_dec::do_feed(const Char_t *input_data, Size_t input_size) {
  m_input_data = input_data;
  m_input_size = input_size;
  m_input_position = 0;
}

std::pair<Decompress_status, Decompressor::Size_t> None_dec::do_decompress(
    Char_t *out, Size_t output_size) {
  if (m_input_position == m_input_size)
    return std::make_pair(Decompress_status::end, 0);
  auto copy_size = std::min(output_size, m_input_size - m_input_position);
  std::memcpy(out, m_input_data + m_input_position, copy_size);
  m_input_position += copy_size;
  if (copy_size < output_size)
    return std::make_pair(Decompress_status::truncated, copy_size);
  return std::make_pair(Decompress_status::success, copy_size);
}

}  // namespace binary_log::transaction::compression
