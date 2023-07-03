/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_CLIENT_XCYCLIC_BUFFER_H_
#define PLUGIN_X_CLIENT_XCYCLIC_BUFFER_H_

#include <algorithm>
#include <memory>

#include "my_inttypes.h"

namespace xcl {

class Cyclic_buffer {
 public:
  Cyclic_buffer(const uint64_t size) { change_size(size); }

  void change_size(const uint64_t size) {
    m_buffer_size = size;
    m_buffer.reset(new uint8_t[size]);
    m_buffer_offset = 0;
    m_buffer_data_in = 0;
  }

  void put(const uint8_t *data_in, uint64_t size) {
    assert(size <= space_left());

    uint64_t out_size;
    uint8_t *out_ptr;
    while (size) {
      begin_direct_update(&out_ptr, &out_size);
      const auto copy_block_size = std::min(size, out_size);
      memcpy(out_ptr, data_in, copy_block_size);
      end_direct_update(copy_block_size);
      size -= copy_block_size;
      out_ptr += copy_block_size;
    }
  }

  uint64_t get(uint8_t *buffer_data_out, uint64_t buffer_data_size) {
    const auto copy_without_roll =
        std::min(buffer_data_size, used_without_roll());
    memcpy(buffer_data_out, m_buffer.get() + m_buffer_offset,
           copy_without_roll);
    buffer_data_size -= copy_without_roll;
    buffer_data_out += copy_without_roll;
    m_buffer_data_in -= copy_without_roll;
    m_buffer_offset = (m_buffer_offset + copy_without_roll) % m_buffer_size;

    const auto copy_roll = std::min(buffer_data_size, m_buffer_data_in);
    memcpy(buffer_data_out, m_buffer.get() + m_buffer_offset, copy_roll);

    m_buffer_data_in -= copy_roll;
    m_buffer_offset = (m_buffer_offset + copy_roll) % m_buffer_size;

    return copy_roll + copy_without_roll;
  }

  uint64_t space_left() const { return m_buffer_size - m_buffer_data_in; }
  uint64_t space_used() const { return m_buffer_data_in; }

  bool begin_direct_update(uint8_t **out_ptr, uint64_t *out_size) {
    const uint64_t data_end_offset =
        (m_buffer_offset + m_buffer_data_in) % m_buffer_size;

    *out_size = (m_buffer_offset > data_end_offset)
                    ? m_buffer_offset - data_end_offset
                    : m_buffer_size - data_end_offset;
    *out_ptr = m_buffer.get() + data_end_offset;

    if (m_buffer_data_in == m_buffer_size) *out_size = 0;

    return *out_size > 0;
  }

  void end_direct_update(uint64_t commit_size) {
    assert(commit_size <= space_left());
    m_buffer_data_in = m_buffer_data_in + commit_size;
  }

 private:
  uint64_t used_without_roll() const {
    const auto to_end = m_buffer_size - m_buffer_offset;
    return std::min(to_end, m_buffer_data_in);
  }

  uint64_t m_buffer_size;
  std::unique_ptr<uint8_t[]> m_buffer;
  uint64_t m_buffer_offset{0};
  uint64_t m_buffer_data_in{0};
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XCYCLIC_BUFFER_H_
