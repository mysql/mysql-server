/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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
 * along with this program; if not, write to the Free Softwa re * Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_PAGE_POOL_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_PAGE_POOL_H_

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <list>
#include <new>

#include "plugin/x/src/helper/multithread/mutex.h"

#define k_minimum_page_size 4096

namespace ngs {

struct Pool_config {
  int32_t m_pages_cache_max;
  int32_t m_page_size;
};

class Memory_block_pool {
 public:
  explicit Memory_block_pool(const Pool_config &config);
  ~Memory_block_pool();

  char *allocate();
  void deallocate(char *page);
  const Pool_config *get_config() const;

 private:
  bool try_to_cache_page(char *page_data);
  char *get_page_from_cache();

  struct Node_linked_list {
    explicit Node_linked_list(Node_linked_list *next = nullptr)
        : m_next(next) {}

    Node_linked_list *m_next;
  };

  xpl::Mutex m_mutex;
  const Pool_config m_config;
  std::atomic<int32_t> m_pages_allocated{0};
  int32_t m_number_of_cached_pages{0};

  Node_linked_list *m_page_cache{nullptr};
};

/**
  Manager for memory pages

  In context of this class, page is a application allocated memory block of
  predefined size. This class caches some number of pages for later reuse.

  Memory block is represented by "Page" type (template argument). The manager,
  allocates a memory block and passes is to the object in its constructor.
  Such "Page" object is either cached or returned to the user.

  "Page" must have following constructor:

  ``` C++
    class Page {
     public:
      Page(uint32_t pcapacity, char *pdata) {
    };
  ```

  There is additional goal except caching, the class allocates "Page" object
  and the memory region in single memory call. Application should not depend
  on this behavior.
*/
template <typename Page>
class Page_pool {
 public:
  using Pool = Page_pool<Page>;
  explicit Page_pool(Memory_block_pool *memory_pool)
      : m_internal_pool(memory_pool) {}

  Page *allocate() {
    auto object_data = m_internal_pool->allocate();
    return new (object_data)
        Page(m_internal_pool->get_config()->m_page_size - sizeof(Page),
             object_data + sizeof(Page));
  }

  void deallocate(Page *page) {
    page->~Page();

    m_internal_pool->deallocate(reinterpret_cast<char *>(page));
  }

  const Pool_config *get_config() const {
    return m_internal_pool->get_config();
  }

 private:
  Page_pool(const Page_pool &) = delete;
  Page_pool &operator=(const Page_pool &) = delete;

  Memory_block_pool *m_internal_pool;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_PAGE_POOL_H_
