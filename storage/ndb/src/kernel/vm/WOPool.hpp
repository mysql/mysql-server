/*
   Copyright (c) 2006, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef WOPOOL_HPP
#define WOPOOL_HPP

#include "Pool.hpp"

#define JAM_FILE_ID 303

struct WOPage {
  static constexpr Uint32 WOPAGE_WORDS = GLOBAL_PAGE_SIZE_WORDS - 2;

  Uint32 m_type_id;
  Uint32 m_ref_count;
  Uint32 m_data[WOPAGE_WORDS];
};

/**
 * Write Once Pool
 */
template <typename T>
struct WOPool {
  Record_info m_record_info;
  WOPage *m_memroot;
  WOPage *m_current_page;
  Pool_context m_ctx;
  Uint32 m_current_page_no;
  Uint16 m_current_pos;
  Uint16 m_current_ref_count;

 public:
  typedef T Type;
  WOPool();

  void init(const Record_info &ri, const Pool_context &pc);
  bool seize(Ptr<T> &);
  void release(Ptr<T>);
  void *getPtr(Uint32 i) const;

 private:
  void seize_in_page(Ptr<T> &);
  bool seize_new_page(Ptr<T> &);
  void release_not_current(Ptr<T>);

  [[noreturn]] void handle_invalid_release(Ptr<T>);
  [[noreturn]] void handle_invalid_get_ptr(Uint32 i) const;
  [[noreturn]] void handle_inconsistent_release(Ptr<T>);
};

template <typename T>
inline void WOPool<T>::seize_in_page(Ptr<T> &ptr) {
  Uint32 pos = m_current_pos;
  WOPage *pageP = m_current_page;
  Uint32 magic_pos = pos + m_record_info.m_offset_magic;
  Uint32 type_id = ~(Uint32)m_record_info.m_type_id;
  Uint32 size = m_record_info.m_size;
  Uint16 ref_count = m_current_ref_count;

  assert(pos + size < WOPage::WOPAGE_WORDS);
  ptr.i = (m_current_page_no << POOL_RECORD_BITS) + pos;
  Uint32 *const p = (pageP->m_data + pos);
  ptr.p = reinterpret_cast<T *>(p);  // TODO dynamic_cast?
  pageP->m_data[magic_pos] = type_id;
  m_current_pos = pos + size;
  m_current_ref_count = ref_count + 1;
}

template <typename T>
inline bool WOPool<T>::seize(Ptr<T> &ptr) {
  if (likely(m_current_pos + m_record_info.m_size < WOPage::WOPAGE_WORDS)) {
    seize_in_page(ptr);
    return true;
  }
  return seize_new_page(ptr);
}

template <typename T>
inline void WOPool<T>::release(Ptr<T> ptr) {
  Uint32 cur_page = m_current_page_no;
  Uint32 ptr_page = ptr.i >> POOL_RECORD_BITS;
  Uint32 *magic_ptr = (((Uint32 *)ptr.p) + m_record_info.m_offset_magic);
  Uint32 magic_val = *magic_ptr;

  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id)) {
    *magic_ptr = 0;
    if (cur_page == ptr_page) {
      if (m_current_ref_count == 1) {
        m_current_pos = 0;
      }
      m_current_ref_count--;
      return;
    }
    return release_not_current(ptr);
  }
  handle_invalid_release(ptr);
}

template <typename T>
inline void *WOPool<T>::getPtr(Uint32 i) const {
  Uint32 page_no = i >> POOL_RECORD_BITS;
  Uint32 page_idx = i & POOL_RECORD_MASK;
  WOPage *page = m_memroot + page_no;
  Uint32 *record = page->m_data + page_idx;
  Uint32 magic_val = *(record + m_record_info.m_offset_magic);
  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id)) {
    return record;
  }
  handle_invalid_get_ptr(i);
  return 0; /* purify: deadcode */
}

template <typename T>
WOPool<T>::WOPool() {
  memset(this, 0, sizeof(*this));
  m_current_pos = WOPage::WOPAGE_WORDS;
}

template <typename T>
void WOPool<T>::init(const Record_info &ri, const Pool_context &pc) {
  m_ctx = pc;
  m_record_info = ri;
  m_record_info.m_size = ((ri.m_size + 3) >> 2);  // Align to word boundary
  m_record_info.m_offset_magic = ((ri.m_offset_magic + 3) >> 2);
  m_memroot = (WOPage *)m_ctx.get_memroot();
#ifdef VM_TRACE
  g_eventLogger->info("WOPool<T>::init(%x, %d)", ri.m_type_id,
                      m_record_info.m_size);
#endif
}

template <typename T>
bool WOPool<T>::seize_new_page(Ptr<T> &ptr) {
  WOPage *page;
  Uint32 page_no = RNIL;
  if ((page =
           (WOPage *)m_ctx.alloc_page19(m_record_info.m_type_id, &page_no))) {
    if (m_current_page) {
      m_current_page->m_ref_count = m_current_ref_count;
    }

    m_current_pos = 0;
    m_current_ref_count = 0;
    m_current_page_no = page_no;
    m_current_page = page;
    page->m_type_id = m_record_info.m_type_id;
    seize_in_page(ptr);
    return true;
  }
  return false;
}

template <typename T>
void WOPool<T>::release_not_current(Ptr<T> ptr) {
  const Uint32 pageI = ptr.i >> POOL_RECORD_BITS;
  WOPage *page = m_memroot + pageI;
  Uint32 cnt = page->m_ref_count;
  Uint32 type = page->m_type_id;
  Uint32 ri_type = m_record_info.m_type_id;
  if (likely(cnt && type == ri_type)) {
    if (cnt == 1) {
      m_ctx.release_page(ri_type, pageI);
      return;
    }
    page->m_ref_count = cnt - 1;
    return;
  }

  handle_inconsistent_release(ptr);
}

template <typename T>
void WOPool<T>::handle_invalid_release(Ptr<T> ptr) {
  char buf[255];

  Uint32 pos = ptr.i & POOL_RECORD_MASK;
  Uint32 pageI = ptr.i >> POOL_RECORD_BITS;
  Uint32 *record_ptr_p = (Uint32 *)ptr.p;
  Uint32 *record_ptr_i = (m_memroot + pageI)->m_data + pos;

  Uint32 magic = *(record_ptr_p + m_record_info.m_offset_magic);
  BaseString::snprintf(buf, sizeof(buf),
                       "Invalid memory release: ptr (%x %p %p) magic: (%.8x "
                       "%.8x) memroot: %p page: %x",
                       ptr.i, ptr.p, record_ptr_i, magic,
                       m_record_info.m_type_id, m_memroot,
                       (m_memroot + pageI)->m_type_id);

  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

template <typename T>
void WOPool<T>::handle_invalid_get_ptr(Uint32 ptrI) const {
  char buf[255];

  Uint32 pos = ptrI & POOL_RECORD_MASK;
  Uint32 pageI = ptrI >> POOL_RECORD_BITS;
  Uint32 *record_ptr_i = (m_memroot + pageI)->m_data + pos;

  Uint32 magic = *(record_ptr_i + m_record_info.m_offset_magic);
  BaseString::snprintf(buf, sizeof(buf),
                       "Invalid memory access: ptr (%x %p) magic: (%.8x %.8x) "
                       "memroot: %p page: %x",
                       ptrI, record_ptr_i, magic, m_record_info.m_type_id,
                       m_memroot, (m_memroot + pageI)->m_type_id);

  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

template <typename T>
void WOPool<T>::handle_inconsistent_release(Ptr<T> ptr) {
  const Uint32 pageI = ptr.i >> POOL_RECORD_BITS;
  WOPage *page = m_memroot + pageI;
  Uint32 cnt = page->m_ref_count;
  Uint32 type = page->m_type_id;
  Uint32 ri_type = m_record_info.m_type_id;

  char buf[255];

  BaseString::snprintf(buf, sizeof(buf),
                       "Memory corruption: ptr (%x %p) page (%d %x %x)", ptr.i,
                       ptr.p, cnt, type, ri_type);

  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

#undef JAM_FILE_ID

#endif
