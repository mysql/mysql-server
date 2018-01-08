/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "trp_buffer.hpp"

bool
TFPool::init(size_t mem, 
             size_t reserved_mem,
             size_t page_sz)
{
  m_pagesize = page_sz;
  m_tot_send_buffer_pages = mem/page_sz;
  size_t tot_alloc = m_tot_send_buffer_pages * page_sz;
  assert(reserved_mem < mem);
  m_reserved_send_buffer_pages = reserved_mem / page_sz;

  unsigned char * ptr = (m_alloc_ptr = (unsigned char*)malloc(tot_alloc));
  for (size_t i = 0; i + page_sz <= tot_alloc; i += page_sz)
  {
    TFPage * p = (TFPage*)(ptr + i);
    p->m_size = (Uint16)(page_sz - offsetof(TFPage, m_data));
    assert(((UintPtr)(&p->m_data[0]) & 3) == 0);
    p->init();
    p->m_next = m_first_free;
    m_first_free = p;
    m_free_send_buffer_pages++;
  }
  
  assert(m_free_send_buffer_pages == m_tot_send_buffer_pages);
  assert(m_free_send_buffer_pages > m_reserved_send_buffer_pages);
  
  return true;
}

TFPool::~TFPool()
{
  if (m_alloc_ptr)
    free (m_alloc_ptr);
}

TFMTPool::TFMTPool(const char * name)
{
  NdbMutex_InitWithName(&m_mutex, name);
}

void
TFBuffer::validate() const
{
  if (m_bytes_in_buffer == 0)
  {
    assert(m_head == m_tail);
    if (m_head)
    {
      assert(m_head->m_start < m_head->m_size);  // Full pages should be release
      assert(m_head->m_bytes == 0);
    }
    return;
  }
  else
  {
    assert(m_head != 0);
    assert(m_tail != 0);
  }
  Uint32 sum = 0;
  TFPage * p = m_head;
  while (p)
  {
    assert(p->m_bytes <= p->m_size);
    assert(p->m_start <= p->m_size);
    assert(p->m_start + p->m_bytes <= p->m_size);
    assert(p->m_bytes <= (int)m_bytes_in_buffer);
    assert(p->m_next != p);
    if (p == m_tail)
    {
      assert(p->m_next == 0);
    }
    else
    {
      assert(p->m_next != 0);
    }
    sum += p->m_bytes;
    p = p->m_next;
  }
  assert(sum == m_bytes_in_buffer);
}

