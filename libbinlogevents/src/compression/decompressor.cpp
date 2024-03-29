/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <compression/decompressor.h>

namespace binary_log::transaction::compression {

type Decompressor::get_type_code() const { return do_get_type_code(); }

void Decompressor::reset() {
  BAPI_TRACE;
  do_reset();
}

void Decompressor::feed_char_t(const Char_t *input_data, Size_t input_size) {
  BAPI_TRACE;
  do_feed(input_data, input_size);
}

Decompress_status Decompressor::decompress(Managed_buffer_t &out,
                                           Size_t output_size) {
  BAPI_TRACE;
  // Grow output buffer if needed.
  switch (out.reserve_write_size(output_size)) {
    case Grow_status_t::exceeds_max_size:
      return Decompress_status::exceeds_max_size;
    case Grow_status_t::out_of_memory:
      reset();
      return Decompress_status::out_of_memory;
    case Grow_status_t::success:
      break;
    default:
      assert(0);
  }
  auto [status, size] = decompress(out.write_part().begin(), output_size);
  out.increase_position(size);
  BAPI_LOG("info", BAPI_VAR(status) << " " << BAPI_VAR(size));
  return status;
}

std::pair<Decompress_status, Decompressor::Size_t> Decompressor::decompress(
    Char_t *out, Size_t output_size) {
  BAPI_TRACE;
  auto [status, size] = do_decompress(out, output_size);
  if (status == Decompress_status::out_of_memory ||
      status == Decompress_status::corrupted)
    reset();
  BAPI_LOG("info", BAPI_VAR(status) << " " << BAPI_VAR(size));
  return std::make_pair(status, size);
}

Decompressor::Grow_constraint_t Decompressor::get_grow_constraint_hint() const {
  return do_get_grow_constraint_hint();
}

Decompressor::Grow_constraint_t Decompressor::do_get_grow_constraint_hint()
    const {
  return Grow_constraint_t();
}

}  // namespace binary_log::transaction::compression
