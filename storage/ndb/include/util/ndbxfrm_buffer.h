/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_UTIL_NDBXFRM_BUFFER_H
#define NDB_UTIL_NDBXFRM_BUFFER_H

#include "util/require.h"
#include "portlib/NdbMem.h"
#include "util/ndbxfrm_iterator.h"

class ndbxfrm_buffer
{
  using byte = unsigned char;
  static constexpr size_t SIZE = 32768;

  byte* m_data;
  const byte* m_data_end = m_data + SIZE;
  const byte* m_read_head;
  byte* m_write_head;
  bool m_wrote_last;

public:
  ndbxfrm_buffer()
  : m_data(nullptr),
    m_data_end(nullptr),
    m_read_head(nullptr),
    m_write_head(nullptr),
    m_wrote_last(false)
  {
    void *p = NdbMem_AlignedAlloc(NDB_O_DIRECT_WRITE_ALIGNMENT, SIZE);
    // new (p) byte[SIZE] is not portable, use plain cast instead.
    m_data = static_cast<byte*>(p);
    m_data_end = m_data + SIZE;
  }
  ~ndbxfrm_buffer()
  {
    NdbMem_AlignedFree(m_data);
  }
  static constexpr size_t size() { return SIZE; }
  void init()
  {
    m_read_head = m_write_head = m_data;
    m_wrote_last = false;
  }
  void init_reverse()
  {
    m_read_head = m_write_head = m_data + SIZE;
    m_wrote_last = false;
  }
  ndbxfrm_input_iterator get_input_iterator()
  {
    require(m_write_head >= m_read_head);
    return ndbxfrm_input_iterator(m_read_head, m_write_head, m_wrote_last);
  }
  ndbxfrm_input_reverse_iterator get_input_reverse_iterator()
  {
    require(m_write_head <= m_read_head);
    return ndbxfrm_input_reverse_iterator(
        m_read_head, m_write_head, m_wrote_last);
  }
  ndbxfrm_output_iterator get_output_iterator()
  {
    require(m_write_head >= m_read_head);
    return ndbxfrm_output_iterator(
       m_write_head, m_data + SIZE, m_wrote_last);
  }
  ndbxfrm_output_reverse_iterator get_output_reverse_iterator()
  {
    require(m_write_head <= m_read_head);
    return ndbxfrm_output_reverse_iterator(
        m_write_head, m_data, m_wrote_last);
  }
  void update_write(ndbxfrm_output_iterator& it)
  {
    m_write_head = it.begin();
    require(m_write_head >= m_read_head);
    if (it.last())
    {
      m_wrote_last = true;
    }
    require(it.end() == m_data_end);
  }
  void update_reverse_write(ndbxfrm_output_reverse_iterator& it)
  {
    m_write_head = it.begin();
    require(m_write_head <= m_read_head);
    if (it.last())
    {
      m_wrote_last = true;
    }
    require(it.end() == m_data);
  }
  void update_read(ndbxfrm_input_iterator& it)
  {
    m_read_head = it.cbegin();
    require(m_write_head >= m_read_head);
    if (unlikely(it.cend() != m_write_head))
    {
      /*
       * When one reach end of file there may be trailer data that is read but
       * should not be read further the readable data may be reduced by
       * reducing the write head.
       */
      require(it.cend() >= m_read_head);
      require(it.cend() < m_write_head);
      m_write_head -= m_write_head - it.cend();
    }
    require(m_write_head >= m_read_head);
  }
  void update_reverse_read(ndbxfrm_input_reverse_iterator& it)
  {
    m_read_head = it.cbegin();
    require(m_write_head <= m_read_head);
    if (unlikely(it.cend() != m_write_head))
    {
      /*
       * When reading backwards one reach start of file there may be header
       * data that is read but should not be read further the readable data may
       * be reduced by reducing the write head.
       */
      require(it.cend() <= m_read_head);
      require(it.cend() > m_write_head);
      m_write_head += it.cend() - m_write_head;
    }
    require(m_write_head <= m_read_head);
  }
  size_t read_size() const
  {
    require(m_write_head >= m_read_head);
    return m_write_head - m_read_head;
  }
  size_t reverse_read_size() const
  {
    require(m_write_head <= m_read_head);
    return m_read_head - m_write_head;
  }
  size_t write_space() const
  {
    return m_data_end - m_write_head;
  }
  void rebase(size_t block_size)
  {
    require(m_write_head >= m_read_head);
    if (block_size == 0) block_size = 1;
    size_t old_read_offset = m_read_head - m_data;
    size_t new_read_offset = old_read_offset % block_size;
    memmove(
        m_data + new_read_offset, m_read_head, m_write_head - m_read_head);
    m_write_head -= (old_read_offset - new_read_offset);
    m_read_head -= (old_read_offset - new_read_offset);
  }
  void rebase_reverse(size_t block_size)
  {
    require(m_write_head <= m_read_head);
    if (block_size == 0) block_size = 1;
    size_t old_read_offset = m_data + SIZE - m_read_head;
    size_t new_read_offset = old_read_offset % block_size;
    memmove(m_data + SIZE - new_read_offset - (m_read_head - m_write_head),
            m_write_head,
            m_read_head - m_write_head);
    m_write_head += (old_read_offset - new_read_offset);
    m_read_head += (old_read_offset - new_read_offset);
  }
  void clear_last() { m_wrote_last = false; }
  void set_last() { m_wrote_last = true; }
  bool last() const { return m_wrote_last; }
};

#endif
