/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_BUFFER_H_
#define _NGS_BUFFER_H_

#include <stdint.h>
#include <list>
#include <vector>
#include "ngs/protocol/page_pool.h"
#include "ngs_common/types.h"

namespace ngs
{

  enum Alloc_result{ Memory_allocated, Memory_error, Memory_no_free_pages };

  class Buffer
  {
  public:
    typedef Resource<Page>           Buffer_page;
    typedef std::list< Buffer_page > Page_list;

    Buffer(Page_pool& page_pool);

    virtual ~Buffer();

    Alloc_result reserve(size_t space);
    Alloc_result add_pages(unsigned int npages);

    bool uint32_at(size_t offset, uint32_t &ret);
    bool int32_at(size_t offset, int32_t &ret);
    bool int8_at(size_t offset, int8_t &ret);

    size_t capacity() const;
    size_t length() const;
    size_t available_space() const;

    Page_list &pages() { return m_pages; }
    void add_bytes_transferred(size_t nbytes);

    Resource<Page> pop_front();
    void  push_back(const Resource<Page> &);

    void reset();

  protected:
    size_t m_capacity;
    size_t m_length;
    Page_pool& m_page_pool;
    Page_list m_pages;

  private:
    Buffer(const Buffer &);
    Buffer &operator=(const Buffer &);
  };

} // namespace ngs

#endif // _NGS_BUFFER_H_
