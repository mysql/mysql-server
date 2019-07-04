/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/protocol/page_output_stream.h"

namespace ngs {

Page_output_stream::Page_output_stream(ngs::Page_pool &pool) : m_buffer(pool) {}

bool Page_output_stream::Next(void **data, int *size) {
  if (!m_page) {
    if (!move_to_next_page()) return false;
  }

  auto unused = m_page->capacity - m_page->data_length;
  if (!unused) {
    if (!move_to_next_page()) return false;

    unused = m_page->capacity;
  }

  *data = m_page->data + m_page->data_length;
  *size = unused;
  m_page->data_length = m_page->capacity;

  return true;
}

void Page_output_stream::BackUp(int count) {
  DBUG_ASSERT(count <= static_cast<int>(m_page->data_length));
  m_page->data_length -= count;
}

int64_t Page_output_stream::ByteCount() const {
  if (nullptr == m_page) return m_bytes_total;

  return m_bytes_total + m_page->data_length;
}

void Page_output_stream::visit_buffers(Page_visitor *visitor) {
  m_buffer.visit(visitor);
}

void Page_output_stream::backup_current_position() {
  m_backup_bytes_total = m_bytes_total;
  m_buffer.backup();
}

void Page_output_stream::restore_position() {
  m_bytes_total = m_backup_bytes_total;
  m_buffer.restore();
}

void *Page_output_stream::reserve_space(const uint32_t size,
                                        const bool update_on_fail) {
  if (m_page) {
    const auto unused = m_page->capacity - m_page->data_length;

    if (unused > size) {
      void *result = m_page->data + m_page->data_length;
      m_page->data_length += size;

      return result;
    }
  }

  // Try to move to the next page and place the data there.
  // This operation will leave the previous page unfilled.
  if (update_on_fail && move_to_next_page()) return reserve_space(size, false);

  m_fatal = true;

  return nullptr;
}

void Page_output_stream::reset() {
  m_buffer.reset();
  m_bytes_total = 0;
  m_page = m_buffer.get_current_page();
}

bool Page_output_stream::move_to_next_page() {
  if (m_fatal) return false;

  if (m_page) m_bytes_total += m_page->data_length;

  if (!m_buffer.move_to_next_page_if_not_empty()) {
    m_fatal = true;
    return false;
  }

  m_page = m_buffer.get_current_page();

  return true;
}

}  // namespace ngs
