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

#include <compression/none_comp.h>

#include <cstring>  // std::memcpy

namespace binary_log::transaction::compression {

type None_comp::do_get_type_code() const { return type_code; }

void None_comp::do_reset() {
  m_input_data = nullptr;
  m_input_size = 0;
}

void None_comp::do_feed(const Char_t *input_data, Size_t input_size) {
  assert(!m_input_data);
  m_input_data = input_data;
  m_input_size = input_size;
}

Compress_status None_comp::do_compress(Managed_buffer_sequence_t &out) {
  auto grow_status = out.write(m_input_data, m_input_size);
  if (grow_status != Compress_status::success) return grow_status;
  reset();
  return Compress_status::success;
}

Compress_status None_comp::do_finish([
    [maybe_unused]] Managed_buffer_sequence_t &out) {
  // This will only be called after a successful call to @c compress,
  // so there is nothing to do.
  return Compress_status::success;
}

}  // namespace binary_log::transaction::compression
