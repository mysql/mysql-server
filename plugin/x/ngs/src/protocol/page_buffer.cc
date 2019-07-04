/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "my_config.h"

#include <sys/types.h>
#include <new>
#include <utility>

#include "my_dbug.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/protocol/page_buffer.h"

namespace ngs {

Page_buffer::Page_buffer(Page_pool &page_pool) : m_page_pool(page_pool) {}

Page *Page_buffer::get_current_page() {
  DBUG_ASSERT(m_current_page < m_pages.size());
  auto p = m_pages[m_current_page].get();
  return p;
}

bool Page_buffer::move_to_next_page_if_not_empty() {
  if (m_pages.size() && 0 == m_pages[m_current_page]->data_length) {
    return true;
  }

  if (m_pages.size() > m_current_page + 1) {
    ++m_current_page;

    return true;
  }

  try {
    m_pages.push_back(m_page_pool.allocate());
    if (1 != m_pages.size()) ++m_current_page;

    return true;
  } catch (const std::exception &e) {
    log_error(ER_XPLUGIN_BUFFER_PAGE_ALLOC_FAILED, e.what());
  }

  return false;
}

void Page_buffer::visit(Page_visitor *visitor) {
  for (const auto &page : m_pages) {
    // Stop the loop, no more pages with data
    if (0 == page->data_length) break;

    // Visitor didn't accept the page, thus the whole process
    // must be aborted
    log_debug("page->data:%p, page->data_length:%i", page->data,
              (int)page->data_length);
    if (!visitor->visit(page->data, page->data_length)) break;
  }
}

void Page_buffer::backup() {
  m_backup_page = m_current_page;
  if (m_pages.empty()) {
    m_backup_page_data_length = 0;
    return;
  }
  m_backup_page_data_length = m_pages[m_current_page]->data_length;
}

void Page_buffer::reset() {
  if (m_pages.empty()) return;

  while (true) {
    m_pages[m_current_page]->data_length = 0;

    if (0 == m_current_page) break;

    --m_current_page;
  }
}

void Page_buffer::restore() {
  DBUG_ASSERT(m_backup_page <= m_current_page);

  while (m_backup_page < m_current_page) {
    m_pages[m_current_page--]->data_length = 0;
  }

  m_pages[m_current_page]->data_length = m_backup_page_data_length;
}

}  // namespace ngs
