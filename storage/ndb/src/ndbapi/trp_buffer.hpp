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

#ifndef trp_buffer_hpp
#define trp_buffer_hpp

#include <ndb_global.h>
#include <ndb_socket.h> // struct iovec

struct TFPage
{
  inline Uint32 max_data_bytes() const {
    return m_size;
  }

  inline Uint32 get_free_bytes() const {
    return m_size - m_bytes;
  }

  inline bool is_full() const {
    return m_bytes == m_size;
  }

  inline void init () {
    m_bytes = 0;
    m_start = 0;
    m_ref_count = 0;
  }

  static TFPage* ptr(struct iovec p) {
    UintPtr v = UintPtr(p.iov_base);
    v -= offsetof(TFPage, m_data);
    return (TFPage*)v;
  }


  /**
   * Bytes on page
   */
  Uint16 m_bytes;

  /**
   * Start of unused data
   */
  Uint16 m_start;

  /**
   * size of page
   */
  Uint16 m_size;

  /**
   * ref-count
   */
  Uint16 m_ref_count;

  /**
   * Pointer to next page
   */
  struct TFPage * m_next;

  /**
   * The data...
   */
  char m_data[8];
};

/**
 * TFSentinel is used to link pages wo/ having to care about
 *   first page being null
 */
struct TFSentinel
{
  Uint64 data[sizeof(TFPage) / 8];

  TFSentinel() {
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(data); i++)
      data[i] = 0;
  }

  TFPage* getPtr() { return new (&data[0]) TFPage;}
};

struct TFBuffer
{
  TFBuffer() { m_bytes_in_buffer = 0; m_head = m_tail = 0;}
  Uint32 m_bytes_in_buffer;
  struct TFPage * m_head;
  struct TFPage * m_tail;

  void validate() const;
};

struct TFBufferGuard
{
#ifdef VM_TRACE
  const TFBuffer& buf;
  TFBuffer m_save;
  TFBufferGuard(const TFBuffer& _buf) : buf(_buf), m_save(_buf) {
    buf.validate();
  }
  ~TFBufferGuard() {
    buf.validate();
  }
#else
  TFBufferGuard(const TFBuffer&) {}
#endif
};

class TFPool
{
  unsigned char * m_alloc_ptr;
  TFPage * m_first_free;
public:
  TFPool();
  ~TFPool();

  bool init(size_t total_memory, size_t page_sz = 32768);
  bool inited() const { return m_alloc_ptr != 0;}

  TFPage* try_alloc(Uint32 N); // Return linked list of most N pages
  Uint32 try_alloc(struct iovec tmp[], Uint32 cnt);

  void release(TFPage* first, TFPage* last);
  void release_list(TFPage*);
};

inline
TFPage *
TFPool::try_alloc(Uint32 n)
{
  TFPage * h = m_first_free;
  if (h)
  {
    TFPage * p = h;
    TFPage * prev = 0;
    while (p != 0 && n != 0)
    {
      prev = p;
      p = p->m_next;
      n--;
    }
    prev->m_next = 0;
    m_first_free = p;
  }
  return h;
}

inline
Uint32
TFPool::try_alloc(struct iovec tmp[], Uint32 cnt)
{
  TFPage * p = try_alloc(cnt);
  Uint32 i = 0;
  while (p)
  {
    p->init();
    tmp[i].iov_base = p->m_data;
    tmp[i].iov_len = p->m_size;

    i++;
    p = p->m_next;
  }
  return i;
}

inline
void
TFPool::release(TFPage* first, TFPage* last)
{
  last->m_next = m_first_free;
  m_first_free = first;
}

inline
void
TFPool::release_list(TFPage* head)
{
  TFPage * tail = head;
  while (tail->m_next != 0)
    tail = tail->m_next;
  release(head, tail);
}

#endif
