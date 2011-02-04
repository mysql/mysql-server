/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "trp_buffer.hpp"

TFPool::TFPool()
{
  m_first_free = 0;
  m_alloc_ptr = 0;
}

bool
TFPool::init(size_t mem, size_t page_sz)
{
  unsigned char * ptr = (m_alloc_ptr = (unsigned char*)malloc(mem));
  for (size_t i = 0; i + page_sz < mem; i += page_sz)
  {
    TFPage * p = (TFPage*)(ptr + i);
    p->m_size = (Uint16)(page_sz - offsetof(TFPage, m_data));
    p->init();
    p->m_next = m_first_free;
    m_first_free = p;
  }
  return true;
}

TFPool::~TFPool()
{
  if (m_alloc_ptr)
    free (m_alloc_ptr);
}

void
TFBuffer::validate() const
{
  if (m_bytes_in_buffer == 0)
  {
    assert(m_head == m_tail);
    if (m_head)
    {
      assert(m_head->m_bytes < m_head->m_size);  // Full pages should be release
      assert(m_head->m_bytes == m_head->m_start);
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
    assert(p->m_start <= p->m_bytes);
    assert((p->m_start & 3) == 0);
    assert(p->m_bytes - p->m_start > 0);
    assert(p->m_bytes - p->m_start <= (int)m_bytes_in_buffer);
    assert(p->m_next != p);
    if (p == m_tail)
    {
      assert(p->m_next == 0);
    }
    else
    {
      assert(p->m_next != 0);
    }
    sum += p->m_bytes - p->m_start;
    p = p->m_next;
  }
  assert(sum == m_bytes_in_buffer);
}

