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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_BUFFER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_BUFFER_H_

#include <stdint.h>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/protocol/page_pool.h"

namespace ngs {

enum Alloc_result { Memory_allocated, Memory_error, Memory_no_free_pages };

class Page_visitor {
 public:
  virtual ~Page_visitor() = default;

  virtual bool visit(const char *, ssize_t) = 0;
};

class Page_buffer {
 public:
  using Buffer_page = Resource<Page>;
  using Pages = std::vector<Buffer_page>;

  Page_buffer(Page_pool &page_pool);

  Page *get_current_page();
  bool move_to_next_page_if_not_empty();

  void visit(Page_visitor *visitor);

  void backup();
  void restore();

  void reset();

 private:
  Page_pool &m_page_pool;
  Pages m_pages;
  uint32_t m_current_page = 0;
  uint32_t m_backup_page = 0;
  uint32_t m_backup_page_data_length = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_BUFFER_H_
