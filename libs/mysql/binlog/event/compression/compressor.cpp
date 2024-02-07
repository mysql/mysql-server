/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mysql/binlog/event/compression/compressor.h"

namespace mysql::binlog::event::compression {

type Compressor::get_type_code() const { return do_get_type_code(); }

void Compressor::reset() {
  BAPI_TRACE;
  do_reset();
  m_pending_input = false;
  m_empty = true;
  m_pledged_input_size = 0;
}

void Compressor::feed_char_t(const Char_t *input_data, Size_t input_size) {
  BAPI_TRACE;
  BAPI_LOG("info", BAPI_VAR(input_size));
  assert(!m_pending_input);
  do_feed(input_data, input_size);
  m_pending_input = true;
  m_empty = false;
}

Compress_status Compressor::compress(Managed_buffer_sequence_t &out) {
  BAPI_TRACE;
  auto ret = do_compress(out);
  if (ret == Compress_status::out_of_memory) reset();
  if (ret != Compress_status::exceeds_max_size) m_pending_input = false;
  BAPI_LOG("info", BAPI_VAR(ret) << " " << BAPI_VAR(out.read_part().size()));
  return ret;
}

Compress_status Compressor::finish(Managed_buffer_sequence_t &out) {
  BAPI_TRACE;
  // Consume all input, if not consumed already.
  auto compress_status = compress(out);
  if (compress_status != Compress_status::success) return compress_status;

  // Produce all output.
  auto ret = do_finish(out);
  if (ret == Compress_status::out_of_memory) reset();
  if (ret != Compress_status::exceeds_max_size) m_pending_input = false;
  if (ret == Compress_status::success) m_empty = true;
  BAPI_LOG("info", BAPI_VAR(ret) << " " << BAPI_VAR(out.read_part().size()));
  return ret;
}

Compressor::Grow_constraint_t Compressor::get_grow_constraint_hint() const {
  return do_get_grow_constraint_hint();
}

Compressor::Grow_constraint_t Compressor::do_get_grow_constraint_hint() const {
  return Grow_constraint_t();
}

void Compressor::set_pledged_input_size(Size_t size) {
  assert(m_empty);
  m_pledged_input_size = size;
  do_set_pledged_input_size(size);
}

Compressor::Size_t Compressor::get_pledged_input_size() const {
  return m_pledged_input_size;
}

void Compressor::do_set_pledged_input_size([[maybe_unused]] Size_t size) {}

}  // namespace mysql::binlog::event::compression
