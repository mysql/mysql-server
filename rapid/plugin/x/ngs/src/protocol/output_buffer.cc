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

#include "plugin/x/ngs/include/ngs/protocol/output_buffer.h"

#include "my_byteorder.h"
#include "my_inttypes.h"


namespace ngs {

Output_buffer::Output_buffer(Page_pool& page_pool)
  : Buffer(page_pool) {
}


bool Output_buffer::add_int32(int32_t i) {
  const uint32 raw_data_size = sizeof(int32_t);
  uchar raw_data[raw_data_size];
  int4store(raw_data, i);

  add_bytes(reinterpret_cast<char*>(raw_data),
            raw_data_size);

  return true;
}

bool Output_buffer::add_int8(int8_t i) {
  void *ptr;
  int size;

  do {
    if (!Next(&ptr, &size))
      return false;
  }
  while (size < 1);

  *(int8_t*)ptr = i;

  if (size > 1) // return leftover
    BackUp(size-1);

  return true;
}


bool Output_buffer::add_bytes(const char *data, size_t length) {
  void *ptr;
  int size;

  do {
    if (!Next(&ptr, &size) || size < 0)
      return false;

    if ((size_t)size >= length) {
      memcpy(ptr, data, length);
      BackUp(static_cast<int>(size - length));
      length = 0;
    }
    else {
      memcpy(ptr, data, size);
      data += size;
      length -= size;
    }
  }
  while (length > 0);

  return true;
}


bool Output_buffer::Next(void** data, int* size) {
  // point *data to the beginning of the next page
  // point size to the data left in that page
  // only up to m_artificial_length must be passed

  // first, check if there are pages left with free space
  for (Page_list::const_iterator p = m_pages.begin();
       p != m_pages.end();
       ++p) {
    if ((*p)->length < (*p)->capacity) {
      // ensure that the next page is empty
      Page_list::const_iterator next = p;
      ++next;
      if (next == m_pages.end() || (*next)->length == 0) {
        *data = (*p)->data + (*p)->length;
        *size = (*p)->capacity - (*p)->length;
        (*p)->length = (*p)->capacity;
        m_length += *size;
        return true;
      }
    }
  }

  // no more space left, just add new pages
  if (Memory_allocated == add_pages(1)) {
    Buffer_page &p = m_pages.back();

    *data = p->data;
    *size = p->capacity;
    p->length = p->capacity;
    m_length += *size;
    return true;
  }
  return false;
}


void Output_buffer::BackUp(int count) {
  // return unused bytes from the last Next() call
  for (Page_list::const_reverse_iterator p = m_pages.rbegin();
       p != m_pages.rend() && count > 0; ++p)
  {
    if ((*p)->length > 0)
    {
      if (count > 0 && (size_t)count < (*p)->length)
      {
        (*p)->length -= count;
        m_length -= count;
        count = 0;
      }
      else
      {
        count -= (*p)->length;
        m_length -= (*p)->length;
        (*p)->length = 0;
      }
    }
  }
}

int64_t Output_buffer::ByteCount() const {
  size_t count = 0;
  for (Page_list::const_iterator p = m_pages.begin();
       p != m_pages.end(); ++p)
    count += (*p)->length;
  return count;
}


Const_buffer_sequence Output_buffer::get_buffers() {
  Const_buffer_sequence buffers;
  buffers.reserve(m_pages.size());

  for (Page_list::const_iterator p = m_pages.begin();
       p != m_pages.end() && (*p)->length > 0;++p) {
    buffers.push_back(std::make_pair((*p)->data, (*p)->length));
  }
  return buffers;
}

void Output_buffer::save_state() {
  m_saved_length = m_length;
  Page_list::iterator it = m_pages.begin();

  for (; it != m_pages.end(); ++it) {
    (*it)->save_state();
  }
}

void Output_buffer::rollback() {
  m_length = m_saved_length;
  Page_list::iterator it = m_pages.begin();

  for (; it != m_pages.end(); ++it) {
    (*it)->rollback();
  }
}

} // namespace ngs
