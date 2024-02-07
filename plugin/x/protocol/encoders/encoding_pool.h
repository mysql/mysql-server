/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_POOL_H_
#define PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_POOL_H_

#include <cassert>
#include <cstdint>
#include <vector>

#include "plugin/x/src/ngs/protocol/page_pool.h"

namespace protocol {

class Page {
 public:
  template <int size>
  explicit Page(uint8_t (&data_arr)[size]) {
    m_begin_data = m_current_data = reinterpret_cast<uint8_t *>(&data_arr);
    m_end_data = m_begin_data + size;
  }

  Page(const uint32_t size, char *data_ptr) {
    m_begin_data = m_current_data = reinterpret_cast<uint8_t *>(data_ptr);
    m_end_data = m_begin_data + size;
  }

  virtual ~Page() = default;

  void reset() {
    m_current_data = m_begin_data;
    m_next_page = nullptr;
  }

  bool is_at_least(const uint32_t needed_size) const {
    return m_end_data > m_current_data + needed_size;
  }
  bool is_full() const { return m_end_data <= m_current_data; }
  bool is_empty() const { return m_begin_data == m_current_data; }

  uint32_t get_used_bytes() const { return m_current_data - m_begin_data; }
  uint32_t get_free_bytes() const { return m_end_data - m_current_data; }
  uint8_t *get_free_ptr() const { return m_current_data; }

  uint8_t *m_begin_data;
  uint8_t *m_current_data;
  uint8_t *m_end_data;
  uint32_t m_references = 0;

  Page *m_next_page = nullptr;
};

class Encoding_pool {
 public:
  using Pool = ngs::Page_pool<Page>;

 public:
  explicit Encoding_pool(const uint32_t local_cache,
                         ngs::Memory_block_pool *memory_pool)
      : m_local_cache(local_cache), m_pool(memory_pool) {}

  ~Encoding_pool() {
    while (m_empty_pages) {
      auto page = m_empty_pages;
      m_empty_pages = m_empty_pages->m_next_page;
      --m_pages;
      m_pool.deallocate(page);
    }
    assert(0 == m_pages);
  }

  Page *alloc_page() {
    if (m_empty_pages) {
      auto result = m_empty_pages;
      m_empty_pages = m_empty_pages->m_next_page;

      result->reset();
      return result;
    }

    ++m_pages;
    return m_pool.allocate();
  }

  void release_page(Page *page) {
    if (m_pages < m_local_cache) {
      page->m_next_page = m_empty_pages;
      m_empty_pages = page;

      return;
    }

    --m_pages;
    m_pool.deallocate(page);
  }

  const Pool *get_pool() const { return &m_pool; }

 private:
  Page *m_empty_pages = nullptr;
  uint32_t m_local_cache = 0;
  uint32_t m_pages = 0;

  Pool m_pool;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_POOL_H_
