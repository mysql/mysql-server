/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#ifndef trp_buffer_hpp
#define trp_buffer_hpp

#include <ndb_global.h>
#include "ndb_socket.h" // struct iovec
#include <portlib/NdbMutex.h>

struct TFPage
{
  /* In previous versions the number of data words was calculated such that
   * sizeof TFPage became 32'768 bytes.  That value is made explicit to more
   * easily track possible future changes.
   */
  static constexpr Uint32 SIZE = 8188 * sizeof(Uint32);

  static constexpr inline Uint32 max_data_bytes()
  {
    return SIZE;
  }

  inline Uint32 get_free_bytes() const {
    return SIZE - m_bytes;
  }

  inline bool is_full() const {
    return m_bytes == SIZE;
  }

  inline void init () {
    m_bytes = 0;
    m_start = 0;
    m_ref_count = 0;
    m_next = nullptr;
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

  Uint16 m_unused;

  /**
   * ref-count
   */
  Uint16 m_ref_count;

  /**
   * Pointer to next page.
   * Note, may be 32bit on some platforms.
   * alignas(8) for m_data makes sure that struct size still will be equal on
   * 32bit and 64bit platforms.
   */
  struct TFPage * m_next;

  /**
   * The data...
   * NOTE: This structure is tightly coupled with its allocation.
   * So changing this data structure requires careful consideration.
   * m_data actually houses a full page that is allocated when the
   * data structure is malloc'ed.
   */
  alignas(8) char m_data[SIZE];
};

struct TFBuffer
{
  TFBuffer() : m_head(nullptr), m_tail(nullptr), m_bytes_in_buffer(0) {}
  struct TFPage * m_head;
  struct TFPage * m_tail;
  Uint32 m_bytes_in_buffer;

  void validate() const;
  void clear() { m_bytes_in_buffer = 0; m_head = m_tail = nullptr;}
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
  friend class TFMTPool;
  unsigned char * m_alloc_ptr;
  Uint32 m_tot_send_buffer_pages;
  Uint32 m_pagesize;
  Uint32 m_free_send_buffer_pages;
  Uint32 m_reserved_send_buffer_pages;
  TFPage * m_first_free;
public:
  TFPool():
    m_alloc_ptr(nullptr),
    m_tot_send_buffer_pages(0),
    m_pagesize(SENDBUFFER_DEFAULT_PAGE_SIZE),
    m_free_send_buffer_pages(0),
    m_reserved_send_buffer_pages(0),
    m_first_free(nullptr)
    {}

  ~TFPool();

  bool init(size_t total_memory, 
            size_t reserved_memory = 0,
            size_t page_sz = SENDBUFFER_DEFAULT_PAGE_SIZE);
  bool inited() const { return m_alloc_ptr != nullptr;}

  TFPage* try_alloc(Uint32 N, bool reserved = false); // Return linked list of most N pages
  Uint32 try_alloc(struct iovec tmp[], Uint32 cnt);

  void release(TFPage* first, TFPage* last, Uint32 page_count);
  void release_list(TFPage* first);

  Uint64 get_total_send_buffer_size() const
  {
    /* We ignore the reserved space which is for 'emergency' use only */
    return Uint64(m_tot_send_buffer_pages - m_reserved_send_buffer_pages) * m_pagesize;
  }
  Uint64 get_total_used_send_buffer_size() const
  {
    return Uint64(m_tot_send_buffer_pages - m_free_send_buffer_pages) * m_pagesize;
  }
  Uint32 get_page_size() const
  {
    return m_pagesize;
  }

protected:
  static constexpr Uint32 SENDBUFFER_DEFAULT_PAGE_SIZE = 32 * 1024;
  static_assert(SENDBUFFER_DEFAULT_PAGE_SIZE == sizeof(TFPage));
};

class TFMTPool : private TFPool
{
  NdbMutex m_mutex;
public:
  explicit TFMTPool(const char * name = nullptr);

  bool init(size_t total_memory, 
            size_t reserved_memory = 0, 
            size_t page_sz = SENDBUFFER_DEFAULT_PAGE_SIZE) {
    return TFPool::init(total_memory, 
                        reserved_memory,
                        page_sz);
  }
  bool inited() const {
    return TFPool::inited();
  }

  TFPage* try_alloc(Uint32 N, bool reserved = false) {
    Guard g(&m_mutex);
    return TFPool::try_alloc(N, reserved);
  }

  void release(TFPage* first, TFPage* last, Uint32 page_count) {
    Guard g(&m_mutex);
    TFPool::release(first, last, page_count);
  }

  void release_list(TFPage* head) {
    TFPage * tail = head;
    Uint32 page_count = 1;
    while (tail->m_next != nullptr)
    {
      tail = tail->m_next;
      page_count++;
    }
    release(head, tail, page_count);
  }
  Uint64 get_total_send_buffer_size() const
  {
    return TFPool::get_total_send_buffer_size(); 
  }
  Uint64 get_total_used_send_buffer_size() const
  {
    return TFPool::get_total_used_send_buffer_size();
  }
  Uint32 get_page_size() const
  {
    return TFPool::get_page_size();
  }
};

inline
TFPage *
TFPool::try_alloc(Uint32 n, bool reserved)
{
  /* Try to alloc up to n, but maybe less, including 0 */

  /**
   * Don't worry about reserved et al unless we are low
   * on pages (unusual case)
   */
  if (unlikely(m_free_send_buffer_pages < m_reserved_send_buffer_pages + n))
  {
    Uint64 avail_pages = m_free_send_buffer_pages; 
    if (!reserved)
    {
      /* Some pages are unavailable for us */
      if (m_free_send_buffer_pages > m_reserved_send_buffer_pages)
      {
        /* Some lesser number of pages available */
        avail_pages = m_free_send_buffer_pages - 
          m_reserved_send_buffer_pages;
      }
      else
      {
        /* No pages available */
        avail_pages = 0;
      }
    }
    
    n = MIN(n, avail_pages);
  }
  
  if (n)
  {
    TFPage * h = m_first_free;
    TFPage * p = h;
    TFPage * prev = nullptr;
    m_free_send_buffer_pages -= n;
    while (n != 0)
    {
      assert(p);
      prev = p;
      p = p->m_next;
      n--;
    }
    prev->m_next = nullptr;
    m_first_free = p;
    return h;
  }

  return nullptr;
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
    tmp[i].iov_len = p->max_data_bytes();

    i++;
    p = p->m_next;
  }
  return i;
}

inline
void
TFPool::release(TFPage* first, TFPage* last, Uint32 page_count)
{
  last->m_next = m_first_free;
  m_first_free = first;
  m_free_send_buffer_pages += page_count;
  assert(m_free_send_buffer_pages <= m_tot_send_buffer_pages);
}

inline
void
TFPool::release_list(TFPage* head)
{
  TFPage * tail = head;
  Uint32 page_count = 1;
  while (tail->m_next != nullptr)
  {
    tail = tail->m_next;
    page_count++;
  }
  release(head, tail, page_count);
}

#endif
