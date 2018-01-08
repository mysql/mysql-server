/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/protocol/page_pool.h"

using namespace ngs;

/*
NOTE: Commented for coverage. Uncomment when needed.

Page_pool::Page_pool(const int32_t page_size)
: m_pages_max(0),
  m_pages_cache_max(0),
  m_pages_allocated(0),
  m_pages_cached(0),
  m_page_size(page_size)
{
}

*/

Page_pool::Page_pool(const Pool_config &pool_config)
: m_pages_max(pool_config.pages_max),
  m_pages_cache_max(pool_config.pages_cache_max),
  m_pages_cached(0),
  m_page_size(pool_config.page_size),
  m_pages_allocated(0)
{
}

Page_pool::~Page_pool()
{
  MUTEX_LOCK(lock, m_mutex);
  std::for_each(m_pages_list.begin(), m_pages_list.end(), ngs::free_array<char>);
  m_pages_list.clear();
}


Resource<Page> Page_pool::allocate()
{
  // The code is valid only in case when the method is called only by one thread at a time
  if (m_pages_max != 0 && (++m_pages_allocated > m_pages_max - 1))
  {
    --m_pages_allocated;
    throw No_more_pages_exception();
  }

  char *object_data = pop_page();

  if (NULL == object_data)
  {
    size_t memory_to_allocate = m_page_size + sizeof(Page_memory_managed);

    ngs::allocate_array(object_data, memory_to_allocate, KEY_memory_x_send_buffer);
  }

  return Resource<Page>(new (object_data) Page_memory_managed(*this, m_page_size, object_data + sizeof(Page_memory_managed)));
}


void Page_pool::deallocate(Page *page)
{
  // multiple threads
  if (m_pages_max != 0)
    --m_pages_allocated;

  page->~Page();

  if (!push_page((char*)page))
  {
    ngs::free_array((char*)page);
  }
}


bool Page_pool::push_page(char *page_data)
{
  if (m_pages_cache_max != 0)
  {
    MUTEX_LOCK(lock, m_mutex);

    if (m_pages_cached >= m_pages_cache_max)
      return false;

    ++m_pages_cached;
    m_pages_list.push_back(page_data);

    return true;
  }

  return false;
}


char *Page_pool::pop_page()
{
  if (m_pages_cache_max != 0)
  {
    MUTEX_LOCK(lock, m_mutex);

    if (!m_pages_list.empty())
    {
      --m_pages_cached;
      char *result = m_pages_list.front();

      m_pages_list.pop_front();

      return result;
    }
  }

  return NULL;
}
