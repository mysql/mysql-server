/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <list>
#include <new>

#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ngs/memory.h"
#include "plugin/x/src/ngs/protocol/page_pool.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace ngs {

Memory_block_pool::Memory_block_pool(const Pool_config &config)
    : m_mutex(KEY_mutex_x_page_pool), m_config(config) {}

Memory_block_pool::~Memory_block_pool() {
  MUTEX_LOCK(lock, m_mutex);

  while (m_page_cache) {
    auto to_delete = reinterpret_cast<char *>(m_page_cache);
    m_page_cache = m_page_cache->m_next;

    ngs::free_array<char>(to_delete);
  }
}

char *Memory_block_pool::allocate() {
  ++m_pages_allocated;
  char *object_data = get_page_from_cache();

  if (nullptr == object_data) {
    size_t memory_to_allocate = m_config.m_page_size;

    ngs::allocate_array(object_data, memory_to_allocate,
                        KEY_memory_x_send_buffer);
  }

  return object_data;
}

void Memory_block_pool::deallocate(char *page) {
  --m_pages_allocated;
  if (try_to_cache_page(page)) return;

  ngs::free_array(page);
}

bool Memory_block_pool::try_to_cache_page(char *page_data) {
  if (m_config.m_pages_cache_max != 0) {
    MUTEX_LOCK(lock, m_mutex);

    if (m_number_of_cached_pages >= m_config.m_pages_cache_max) return false;

    ++m_number_of_cached_pages;
    m_page_cache = new (page_data) Node_linked_list(m_page_cache);

    return true;
  }

  return false;
}

char *Memory_block_pool::get_page_from_cache() {
  if (m_config.m_pages_cache_max != 0) {
    MUTEX_LOCK(lock, m_mutex);

    if (m_page_cache) {
      --m_number_of_cached_pages;
      auto result = m_page_cache;

      m_page_cache = m_page_cache->m_next;
      result->~Node_linked_list();

      return reinterpret_cast<char *>(result);
    }
  }

  return nullptr;
}

const Pool_config *Memory_block_pool::get_config() const { return &m_config; }

}  // namespace ngs
