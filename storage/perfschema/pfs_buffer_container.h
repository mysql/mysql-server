/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_BUFFER_CONTAINER_H
#define PFS_BUFFER_CONTAINER_H

/**
  @file storage/perfschema/pfs_buffer_container.h
  Generic buffer container.
*/

#include <stddef.h>
#include <sys/types.h>
#include <atomic>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/pfs_user.h"

#define USE_SCALABLE

class PFS_opaque_container_page;
class PFS_opaque_container;

struct PFS_builtin_memory_class;

template <class T>
class PFS_buffer_const_iterator;

template <class T>
class PFS_buffer_processor;

template <class T, class U, class V>
class PFS_buffer_iterator;

template <class T, int PFS_PAGE_SIZE, int PFS_PAGE_COUNT, class U, class V>
class PFS_buffer_scalable_iterator;

template <class T>
class PFS_buffer_default_array;

template <class T>
class PFS_buffer_default_allocator;

template <class T, class U, class V>
class PFS_buffer_container;

template <class T, int PFS_PAGE_SIZE, int PFS_PAGE_COUNT, class U, class V>
class PFS_buffer_scalable_container;

template <class B, int COUNT>
class PFS_partitioned_buffer_scalable_iterator;

template <class B, int COUNT>
class PFS_partitioned_buffer_scalable_container;

template <class T>
class PFS_buffer_default_array {
 public:
  typedef T value_type;

  value_type *allocate(pfs_dirty_state *dirty_state) {
    size_t index;
    size_t monotonic;
    size_t monotonic_max;
    value_type *pfs;

    if (m_full) {
      return NULL;
    }

    monotonic = m_monotonic.m_size_t++;
    monotonic_max = monotonic + m_max;

    if (unlikely(monotonic >= monotonic_max)) {
      /*
        This will happen once every 2^64 - m_max calls.
        Computation of monotonic_max just overflowed,
        so reset monotonic counters and start again from the beginning.
      */
      m_monotonic.m_size_t.store(0);
      monotonic = 0;
      monotonic_max = m_max;
    }

    while (monotonic < monotonic_max) {
      index = monotonic % m_max;
      pfs = m_ptr + index;

      if (pfs->m_lock.free_to_dirty(dirty_state)) {
        return pfs;
      }
      monotonic = m_monotonic.m_size_t++;
    }

    m_full = true;
    return NULL;
  }

  void deallocate(value_type *pfs) {
    pfs->m_lock.allocated_to_free();
    m_full = false;
  }

  T *get_first() { return m_ptr; }

  T *get_last() { return m_ptr + m_max; }

  bool m_full;
  PFS_cacheline_atomic_size_t m_monotonic;
  T *m_ptr;
  size_t m_max;
  /** Container. */
  PFS_opaque_container *m_container;
};

template <class T>
class PFS_buffer_default_allocator {
 public:
  typedef PFS_buffer_default_array<T> array_type;

  PFS_buffer_default_allocator(PFS_builtin_memory_class *klass)
      : m_builtin_class(klass) {}

  int alloc_array(array_type *array) {
    array->m_ptr = NULL;
    array->m_full = true;
    array->m_monotonic.m_size_t.store(0);

    if (array->m_max > 0) {
      array->m_ptr = PFS_MALLOC_ARRAY(m_builtin_class, array->m_max, sizeof(T),
                                      T, MYF(MY_ZEROFILL));
      if (array->m_ptr == NULL) {
        return 1;
      }
      array->m_full = false;
    }
    return 0;
  }

  void free_array(array_type *array) {
    DBUG_ASSERT(array->m_max > 0);

    PFS_FREE_ARRAY(m_builtin_class, array->m_max, sizeof(T), array->m_ptr);
    array->m_ptr = NULL;
  }

 private:
  PFS_builtin_memory_class *m_builtin_class;
};

template <class T, class U = PFS_buffer_default_array<T>,
          class V = PFS_buffer_default_allocator<T>>
class PFS_buffer_container {
 public:
  friend class PFS_buffer_iterator<T, U, V>;

  typedef T value_type;
  typedef U array_type;
  typedef V allocator_type;
  typedef PFS_buffer_const_iterator<T> const_iterator_type;
  typedef PFS_buffer_iterator<T, U, V> iterator_type;
  typedef PFS_buffer_processor<T> processor_type;
  typedef void (*function_type)(value_type *);

  PFS_buffer_container(allocator_type *allocator) {
    m_array.m_full = true;
    m_array.m_ptr = NULL;
    m_array.m_max = 0;
    m_array.m_monotonic.m_size_t = 0;
    m_lost = 0;
    m_max = 0;
    m_allocator = allocator;
  }

  int init(size_t max_size) {
    if (max_size > 0) {
      m_array.m_max = max_size;
      int rc = m_allocator->alloc_array(&m_array);
      if (rc != 0) {
        m_allocator->free_array(&m_array);
        return 1;
      }
      m_max = max_size;
      m_array.m_full = false;
    }
    return 0;
  }

  void cleanup() { m_allocator->free_array(&m_array); }

  size_t get_row_count() const { return m_max; }

  size_t get_row_size() const { return sizeof(value_type); }

  size_t get_memory() const { return get_row_count() * get_row_size(); }

  value_type *allocate(pfs_dirty_state *dirty_state) {
    value_type *pfs;

    pfs = m_array.allocate(dirty_state, m_max);
    if (pfs == NULL) {
      m_lost++;
    }

    return pfs;
  }

  void deallocate(value_type *pfs) { m_array.deallocate(pfs); }

  iterator_type iterate() { return PFS_buffer_iterator<T, U, V>(this, 0); }

  iterator_type iterate(uint index) {
    DBUG_ASSERT(index <= m_max);
    return PFS_buffer_iterator<T, U, V>(this, index);
  }

  void apply(function_type fct) {
    value_type *pfs = m_array.get_first();
    value_type *pfs_last = m_array.get_last();

    while (pfs < pfs_last) {
      if (pfs->m_lock.is_populated()) {
        fct(pfs);
      }
      pfs++;
    }
  }

  void apply_all(function_type fct) {
    value_type *pfs = m_array.get_first();
    value_type *pfs_last = m_array.get_last();

    while (pfs < pfs_last) {
      fct(pfs);
      pfs++;
    }
  }

  void apply(processor_type &proc) {
    value_type *pfs = m_array.get_first();
    value_type *pfs_last = m_array.get_last();

    while (pfs < pfs_last) {
      if (pfs->m_lock.is_populated()) {
        proc(pfs);
      }
      pfs++;
    }
  }

  void apply_all(processor_type &proc) {
    value_type *pfs = m_array.get_first();
    value_type *pfs_last = m_array.get_last();

    while (pfs < pfs_last) {
      proc(pfs);
      pfs++;
    }
  }

  inline value_type *get(uint index) {
    DBUG_ASSERT(index < m_max);

    value_type *pfs = m_array.m_ptr + index;
    if (pfs->m_lock.is_populated()) {
      return pfs;
    }

    return NULL;
  }

  value_type *get(uint index, bool *has_more) {
    if (index >= m_max) {
      *has_more = false;
      return NULL;
    }

    *has_more = true;
    return get(index);
  }

  value_type *sanitize(value_type *unsafe) {
    intptr offset;
    value_type *pfs = m_array.get_first();
    value_type *pfs_last = m_array.get_last();

    if ((pfs <= unsafe) && (unsafe < pfs_last)) {
      offset = ((intptr)unsafe - (intptr)pfs) % sizeof(value_type);
      if (offset == 0) {
        return unsafe;
      }
    }

    return NULL;
  }

  ulong m_lost;

 private:
  value_type *scan_next(uint &index, uint *found_index) {
    DBUG_ASSERT(index <= m_max);

    value_type *pfs_first = m_array.get_first();
    value_type *pfs = pfs_first + index;
    value_type *pfs_last = m_array.get_last();

    while (pfs < pfs_last) {
      if (pfs->m_lock.is_populated()) {
        uint found = pfs - pfs_first;
        *found_index = found;
        index = found + 1;
        return pfs;
      }
      pfs++;
    }

    index = m_max;
    return NULL;
  }

  size_t m_max;
  array_type m_array;
  allocator_type *m_allocator;
};

template <class T, int PFS_PAGE_SIZE, int PFS_PAGE_COUNT,
          class U = PFS_buffer_default_array<T>,
          class V = PFS_buffer_default_allocator<T>>
class PFS_buffer_scalable_container {
 public:
  friend class PFS_buffer_scalable_iterator<T, PFS_PAGE_SIZE, PFS_PAGE_COUNT, U,
                                            V>;

  /**
    Type of elements in the buffer.
    The following attributes are required:
    - @code pfs_lock m_lock @endcode
    - @code PFS_opaque_container_page *m_page @endcode
  */
  typedef T value_type;
  /**
    Type of pages in the buffer.
    The following attributes are required:
    - @code PFS_opaque_container *m_container @endcode
  */
  typedef U array_type;
  typedef V allocator_type;
  /** This container type */
  typedef PFS_buffer_scalable_container<T, PFS_PAGE_SIZE, PFS_PAGE_COUNT, U, V>
      container_type;
  typedef PFS_buffer_const_iterator<T> const_iterator_type;
  typedef PFS_buffer_scalable_iterator<T, PFS_PAGE_SIZE, PFS_PAGE_COUNT, U, V>
      iterator_type;
  typedef PFS_buffer_processor<T> processor_type;
  typedef void (*function_type)(value_type *);

  static const size_t MAX_SIZE = PFS_PAGE_SIZE * PFS_PAGE_COUNT;

  PFS_buffer_scalable_container(allocator_type *allocator) {
    m_allocator = allocator;
    m_initialized = false;
  }

  int init(long max_size) {
    int i;

    m_initialized = true;
    m_full = true;
    m_max = PFS_PAGE_COUNT * PFS_PAGE_SIZE;
    m_max_page_count = PFS_PAGE_COUNT;
    m_last_page_size = PFS_PAGE_SIZE;
    m_lost = 0;
    m_monotonic.m_size_t.store(0);
    m_max_page_index.m_size_t.store(0);

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      m_pages[i] = NULL;
    }

    if (max_size == 0) {
      /* No allocation. */
      m_max_page_count = 0;
    } else if (max_size > 0) {
      if (max_size % PFS_PAGE_SIZE == 0) {
        m_max_page_count = max_size / PFS_PAGE_SIZE;
      } else {
        m_max_page_count = max_size / PFS_PAGE_SIZE + 1;
        m_last_page_size = max_size % PFS_PAGE_SIZE;
      }
      /* Bounded allocation. */
      m_full = false;

      if (m_max_page_count > PFS_PAGE_COUNT) {
        m_max_page_count = PFS_PAGE_COUNT;
        m_last_page_size = PFS_PAGE_SIZE;
      }
    } else {
      /* max_size = -1 means unbounded allocation */
      m_full = false;
    }

    DBUG_ASSERT(m_max_page_count <= PFS_PAGE_COUNT);
    DBUG_ASSERT(0 < m_last_page_size);
    DBUG_ASSERT(m_last_page_size <= PFS_PAGE_SIZE);

    native_mutex_init(&m_critical_section, NULL);
    return 0;
  }

  void cleanup() {
    int i;
    array_type *page;

    if (!m_initialized) {
      return;
    }

    native_mutex_lock(&m_critical_section);

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        m_allocator->free_array(page);
        delete page;
        m_pages[i] = NULL;
      }
    }
    native_mutex_unlock(&m_critical_section);

    native_mutex_destroy(&m_critical_section);

    m_initialized = false;
  }

  size_t get_row_count() {
    size_t page_count = m_max_page_index.m_size_t.load();

    return page_count * PFS_PAGE_SIZE;
  }

  size_t get_row_size() const { return sizeof(value_type); }

  size_t get_memory() { return get_row_count() * get_row_size(); }

  value_type *allocate(pfs_dirty_state *dirty_state) {
    if (m_full) {
      m_lost++;
      return NULL;
    }

    size_t index;
    size_t monotonic;
    size_t monotonic_max;
    size_t current_page_count;
    value_type *pfs;
    array_type *array;

    /*
      1: Try to find an available record within the existing pages
    */
    current_page_count = m_max_page_index.m_size_t.load();

    if (current_page_count != 0) {
      monotonic = m_monotonic.m_size_t.load();
      monotonic_max = monotonic + current_page_count;

      if (unlikely(monotonic >= monotonic_max)) {
        /*
          This will happen once every 2^64 - current_page_count calls.
          Computation of monotonic_max just overflowed,
          so reset monotonic counters and start again from the beginning.
        */
        m_monotonic.m_size_t.store(0);
        monotonic = 0;
        monotonic_max = current_page_count;
      }

      while (monotonic < monotonic_max) {
        /*
          Scan in the [0 .. current_page_count - 1] range,
          in parallel with m_monotonic (see below)
        */
        index = monotonic % current_page_count;

        /* Atomic Load, array= m_pages[index] */
        array = m_pages[index].load();

        if (array != NULL) {
          pfs = array->allocate(dirty_state);
          if (pfs != NULL) {
            /* Keep a pointer to the parent page, for deallocate(). */
            pfs->m_page = reinterpret_cast<PFS_opaque_container_page *>(array);
            return pfs;
          }
        }

        /*
          Parallel scans collaborate to increase
          the common monotonic scan counter.

          Note that when all the existing page are full,
          one thread will eventually add a new page,
          and cause m_max_page_index to increase,
          which fools all the modulo logic for scans already in progress,
          because the monotonic counter is not folded to the same place
          (sometime modulo N, sometime modulo N+1).

          This is actually ok: since all the pages are full anyway,
          there is nothing to miss, so better increase the monotonic
          counter faster and then move on to the detection of new pages,
          in part 2: below.
        */
        monotonic = m_monotonic.m_size_t++;
      };
    }

    /*
      2: Try to add a new page, beyond the m_max_page_index limit
    */
    while (current_page_count < m_max_page_count) {
      /* Peek for pages added by collaborating threads */

      /* (2-a) Atomic Load, array= m_pages[current_page_count] */
      array = m_pages[current_page_count].load();

      if (array == NULL) {
        // ==================================================================
        // BEGIN CRITICAL SECTION -- buffer expand
        // ==================================================================

        /*
          On a fresh started server, buffers are typically empty.
          When a sudden load spike is seen by the server,
          multiple threads may want to expand the buffer at the same time.

          Using a compare and swap to allow multiple pages to be added,
          possibly freeing duplicate pages on collisions,
          does not work well because the amount of code involved
          when creating a new page can be significant (PFS_thread),
          causing MANY collisions between (2-b) and (2-d).

          A huge number of collisions (which can happen when thousands
          of new connections hits the server after a restart)
          leads to a huge memory consumption, and to OOM.

          To mitigate this, we use here a mutex,
          to enforce that only ONE page is added at a time,
          so that scaling the buffer happens in a predictable
          and controlled manner.
        */
        native_mutex_lock(&m_critical_section);

        /*
          Peek again for pages added by collaborating threads,
          this time as the only thread allowed to expand the buffer
        */

        /* (2-b) Atomic Load, array= m_pages[current_page_count] */

        array = m_pages[current_page_count].load();

        if (array == NULL) {
          /* (2-c) Found no page, allocate a new one */
          array = new array_type();
          builtin_memory_scalable_buffer.count_alloc(sizeof(array_type));

          array->m_max = get_page_logical_size(current_page_count);
          int rc = m_allocator->alloc_array(array);
          if (rc != 0) {
            m_allocator->free_array(array);
            delete array;
            builtin_memory_scalable_buffer.count_free(sizeof(array_type));
            m_lost++;
            native_mutex_unlock(&m_critical_section);
            return NULL;
          }

          /* Keep a pointer to this container, for static_deallocate(). */
          array->m_container = reinterpret_cast<PFS_opaque_container *>(this);

          /* (2-d) Atomic STORE, m_pages[current_page_count] = array  */
          m_pages[current_page_count].store(array);

          /* Advertise the new page */
          ++m_max_page_index.m_size_t;
        }

        native_mutex_unlock(&m_critical_section);

        // ==================================================================
        // END CRITICAL SECTION -- buffer expand
        // ==================================================================
      }

      DBUG_ASSERT(array != NULL);
      pfs = array->allocate(dirty_state);
      if (pfs != NULL) {
        /* Keep a pointer to the parent page, for deallocate(). */
        pfs->m_page = reinterpret_cast<PFS_opaque_container_page *>(array);
        return pfs;
      }

      current_page_count++;
    }

    m_lost++;
    m_full = true;
    return NULL;
  }

  void deallocate(value_type *safe_pfs) {
    /* Find the containing page */
    PFS_opaque_container_page *opaque_page = safe_pfs->m_page;
    array_type *page = reinterpret_cast<array_type *>(opaque_page);

    /* Mark the object free */
    safe_pfs->m_lock.allocated_to_free();

    /* Flag the containing page as not full. */
    page->m_full = false;

    /* Flag the overall container as not full. */
    m_full = false;
  }

  static void static_deallocate(value_type *safe_pfs) {
    /* Find the containing page */
    PFS_opaque_container_page *opaque_page = safe_pfs->m_page;
    array_type *page = reinterpret_cast<array_type *>(opaque_page);

    /* Mark the object free */
    safe_pfs->m_lock.allocated_to_free();

    /* Flag the containing page as not full. */
    page->m_full = false;

    /* Find the containing buffer */
    PFS_opaque_container *opaque_container = page->m_container;
    PFS_buffer_scalable_container *container;
    container = reinterpret_cast<container_type *>(opaque_container);

    /* Flag the overall container as not full. */
    container->m_full = false;
  }

  iterator_type iterate() {
    return PFS_buffer_scalable_iterator<T, PFS_PAGE_SIZE, PFS_PAGE_COUNT, U, V>(
        this, 0);
  }

  iterator_type iterate(uint index) {
    DBUG_ASSERT(index <= m_max);
    return PFS_buffer_scalable_iterator<T, PFS_PAGE_SIZE, PFS_PAGE_COUNT, U, V>(
        this, index);
  }

  void apply(function_type fct) {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        pfs = page->get_first();
        pfs_last = page->get_last();

        while (pfs < pfs_last) {
          if (pfs->m_lock.is_populated()) {
            fct(pfs);
          }
          pfs++;
        }
      }
    }
  }

  void apply_all(function_type fct) {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        pfs = page->get_first();
        pfs_last = page->get_last();

        while (pfs < pfs_last) {
          fct(pfs);
          pfs++;
        }
      }
    }
  }

  void apply(processor_type &proc) {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        pfs = page->get_first();
        pfs_last = page->get_last();

        while (pfs < pfs_last) {
          if (pfs->m_lock.is_populated()) {
            proc(pfs);
          }
          pfs++;
        }
      }
    }
  }

  void apply_all(processor_type &proc) {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        pfs = page->get_first();
        pfs_last = page->get_last();

        while (pfs < pfs_last) {
          proc(pfs);
          pfs++;
        }
      }
    }
  }

  value_type *get(uint index) {
    DBUG_ASSERT(index < m_max);

    uint index_1 = index / PFS_PAGE_SIZE;
    array_type *page = m_pages[index_1];
    if (page != NULL) {
      uint index_2 = index % PFS_PAGE_SIZE;

      if (index_2 >= page->m_max) {
        return NULL;
      }

      value_type *pfs = page->m_ptr + index_2;

      if (pfs->m_lock.is_populated()) {
        return pfs;
      }
    }

    return NULL;
  }

  value_type *get(uint index, bool *has_more) {
    if (index >= m_max) {
      *has_more = false;
      return NULL;
    }

    uint index_1 = index / PFS_PAGE_SIZE;
    array_type *page = m_pages[index_1];

    if (page == NULL) {
      *has_more = false;
      return NULL;
    }

    uint index_2 = index % PFS_PAGE_SIZE;

    if (index_2 >= page->m_max) {
      *has_more = false;
      return NULL;
    }

    *has_more = true;
    value_type *pfs = page->m_ptr + index_2;

    if (pfs->m_lock.is_populated()) {
      return pfs;
    }

    return NULL;
  }

  value_type *sanitize(value_type *unsafe) {
    intptr offset;
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i = 0; i < PFS_PAGE_COUNT; i++) {
      page = m_pages[i];
      if (page != NULL) {
        pfs = page->get_first();
        pfs_last = page->get_last();

        if ((pfs <= unsafe) && (unsafe < pfs_last)) {
          offset = ((intptr)unsafe - (intptr)pfs) % sizeof(value_type);
          if (offset == 0) {
            return unsafe;
          }
        }
      }
    }

    return NULL;
  }

  ulong m_lost;

 private:
  uint get_page_logical_size(uint page_index) {
    if (page_index + 1 < m_max_page_count) {
      return PFS_PAGE_SIZE;
    }
    DBUG_ASSERT(page_index + 1 == m_max_page_count);
    return m_last_page_size;
  }

  value_type *scan_next(uint &index, uint *found_index) {
    DBUG_ASSERT(index <= m_max);

    uint index_1 = index / PFS_PAGE_SIZE;
    uint index_2 = index % PFS_PAGE_SIZE;
    array_type *page;
    value_type *pfs_first;
    value_type *pfs;
    value_type *pfs_last;

    while (index_1 < PFS_PAGE_COUNT) {
      page = m_pages[index_1];

      if (page == NULL) {
        index = static_cast<uint>(m_max);
        return NULL;
      }

      pfs_first = page->get_first();
      pfs = pfs_first + index_2;
      pfs_last = page->get_last();

      while (pfs < pfs_last) {
        if (pfs->m_lock.is_populated()) {
          uint found =
              index_1 * PFS_PAGE_SIZE + static_cast<uint>(pfs - pfs_first);
          *found_index = found;
          index = found + 1;
          return pfs;
        }
        pfs++;
      }

      index_1++;
      index_2 = 0;
    }

    index = static_cast<uint>(m_max);
    return NULL;
  }

  bool m_initialized;
  bool m_full;
  size_t m_max;
  PFS_cacheline_atomic_size_t m_monotonic;
  PFS_cacheline_atomic_size_t m_max_page_index;
  size_t m_max_page_count;
  size_t m_last_page_size;
  std::atomic<array_type *> m_pages[PFS_PAGE_COUNT];
  allocator_type *m_allocator;
  native_mutex_t m_critical_section;
};

template <class T, class U, class V>
class PFS_buffer_iterator {
  friend class PFS_buffer_container<T, U, V>;

  typedef T value_type;
  typedef PFS_buffer_container<T, U, V> container_type;

 public:
  value_type *scan_next() {
    uint unused;
    return m_container->scan_next(m_index, &unused);
  }

  value_type *scan_next(uint *found_index) {
    return m_container->scan_next(m_index, found_index);
  }

 private:
  PFS_buffer_iterator(container_type *container, uint index)
      : m_container(container), m_index(index) {}

  container_type *m_container;
  uint m_index;
};

template <class T, int page_size, int page_count, class U, class V>
class PFS_buffer_scalable_iterator {
  friend class PFS_buffer_scalable_container<T, page_size, page_count, U, V>;

  typedef T value_type;
  typedef PFS_buffer_scalable_container<T, page_size, page_count, U, V>
      container_type;

 public:
  value_type *scan_next() {
    uint unused;
    return m_container->scan_next(m_index, &unused);
  }

  value_type *scan_next(uint *found_index) {
    return m_container->scan_next(m_index, found_index);
  }

 private:
  PFS_buffer_scalable_iterator(container_type *container, uint index)
      : m_container(container), m_index(index) {}

  container_type *m_container;
  uint m_index;
};

template <class T>
class PFS_buffer_processor {
 public:
  virtual ~PFS_buffer_processor<T>() {}
  virtual void operator()(T *element) = 0;
};

template <class B, int PFS_PARTITION_COUNT>
class PFS_partitioned_buffer_scalable_container {
 public:
  friend class PFS_partitioned_buffer_scalable_iterator<B, PFS_PARTITION_COUNT>;

  typedef typename B::value_type value_type;
  typedef typename B::allocator_type allocator_type;
  typedef PFS_partitioned_buffer_scalable_iterator<B, PFS_PARTITION_COUNT>
      iterator_type;
  typedef typename B::iterator_type sub_iterator_type;
  typedef typename B::processor_type processor_type;
  typedef typename B::function_type function_type;

  PFS_partitioned_buffer_scalable_container(allocator_type *allocator) {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i] = new B(allocator);
    }
  }

  ~PFS_partitioned_buffer_scalable_container() {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      delete m_partitions[i];
    }
  }

  int init(long max_size) {
    int rc = 0;
    // FIXME: we have max_size * PFS_PARTITION_COUNT here
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      rc |= m_partitions[i]->init(max_size);
    }
    return rc;
  }

  void cleanup() {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i]->cleanup();
    }
  }

  size_t get_row_count() const {
    size_t sum = 0;

    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      sum += m_partitions[i]->get_row_count();
    }

    return sum;
  }

  size_t get_row_size() const { return sizeof(value_type); }

  size_t get_memory() const {
    size_t sum = 0;

    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      sum += m_partitions[i]->get_memory();
    }

    return sum;
  }

  long get_lost_counter() {
    long sum = 0;

    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      sum += m_partitions[i]->m_lost;
    }

    return sum;
  }

  value_type *allocate(pfs_dirty_state *dirty_state, uint partition) {
    DBUG_ASSERT(partition < PFS_PARTITION_COUNT);

    return m_partitions[partition]->allocate(dirty_state);
  }

  void deallocate(value_type *safe_pfs) {
    /*
      One issue here is that we do not know which partition
      the record belongs to.
      Each record points to the parent page,
      and each page points to the parent buffer,
      so using static_deallocate here,
      which will find the correct partition by itself.
    */
    B::static_deallocate(safe_pfs);
  }

  iterator_type iterate() { return iterator_type(this, 0, 0); }

  iterator_type iterate(uint user_index) {
    uint partition_index;
    uint sub_index;
    unpack_index(user_index, &partition_index, &sub_index);
    return iterator_type(this, partition_index, sub_index);
  }

  void apply(function_type fct) {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i]->apply(fct);
    }
  }

  void apply_all(function_type fct) {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i]->apply_all(fct);
    }
  }

  void apply(processor_type &proc) {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i]->apply(proc);
    }
  }

  void apply_all(processor_type &proc) {
    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      m_partitions[i]->apply_all(proc);
    }
  }

  value_type *get(uint user_index) {
    uint partition_index;
    uint sub_index;
    unpack_index(user_index, &partition_index, &sub_index);

    if (partition_index >= PFS_PARTITION_COUNT) {
      return NULL;
    }

    return m_partitions[partition_index]->get(sub_index);
  }

  value_type *get(uint user_index, bool *has_more) {
    uint partition_index;
    uint sub_index;
    unpack_index(user_index, &partition_index, &sub_index);

    if (partition_index >= PFS_PARTITION_COUNT) {
      *has_more = false;
      return NULL;
    }

    *has_more = true;
    return m_partitions[partition_index]->get(sub_index);
  }

  value_type *sanitize(value_type *unsafe) {
    value_type *safe = NULL;

    for (int i = 0; i < PFS_PARTITION_COUNT; i++) {
      safe = m_partitions[i]->sanitize(unsafe);
      if (safe != NULL) {
        return safe;
      }
    }

    return safe;
  }

 private:
  static void pack_index(uint partition_index, uint sub_index,
                         uint *user_index) {
    static_assert(PFS_PARTITION_COUNT <= (1 << 8), "2^8 = 256 partitions max.");
    static_assert((B::MAX_SIZE) <= (1 << 24),
                  "2^24 = 16777216 max per partitioned buffer.");

    *user_index = (partition_index << 24) + sub_index;
  }

  static void unpack_index(uint user_index, uint *partition_index,
                           uint *sub_index) {
    *partition_index = user_index >> 24;
    *sub_index = user_index & 0x00FFFFFF;
  }

  value_type *scan_next(uint &partition_index, uint &sub_index,
                        uint *found_partition, uint *found_sub_index) {
    value_type *record = NULL;
    DBUG_ASSERT(partition_index < PFS_PARTITION_COUNT);

    while (partition_index < PFS_PARTITION_COUNT) {
      sub_iterator_type sub_iterator =
          m_partitions[partition_index]->iterate(sub_index);
      record = sub_iterator.scan_next(found_sub_index);
      if (record != NULL) {
        *found_partition = partition_index;
        sub_index = *found_sub_index + 1;
        return record;
      }

      partition_index++;
      sub_index = 0;
    }

    *found_partition = PFS_PARTITION_COUNT;
    *found_sub_index = 0;
    sub_index = 0;
    return NULL;
  }

  B *m_partitions[PFS_PARTITION_COUNT];
};

template <class B, int PFS_PARTITION_COUNT>
class PFS_partitioned_buffer_scalable_iterator {
 public:
  friend class PFS_partitioned_buffer_scalable_container<B,
                                                         PFS_PARTITION_COUNT>;

  typedef typename B::value_type value_type;
  typedef PFS_partitioned_buffer_scalable_container<B, PFS_PARTITION_COUNT>
      container_type;

  value_type *scan_next() {
    uint unused_partition;
    uint unused_sub_index;
    return m_container->scan_next(m_partition, m_sub_index, &unused_partition,
                                  &unused_sub_index);
  }

  value_type *scan_next(uint *found_user_index) {
    uint found_partition;
    uint found_sub_index;
    value_type *record;
    record = m_container->scan_next(m_partition, m_sub_index, &found_partition,
                                    &found_sub_index);
    container_type::pack_index(found_partition, found_sub_index,
                               found_user_index);
    return record;
  }

 private:
  PFS_partitioned_buffer_scalable_iterator(container_type *container,
                                           uint partition, uint sub_index)
      : m_container(container),
        m_partition(partition),
        m_sub_index(sub_index) {}

  container_type *m_container;
  uint m_partition;
  uint m_sub_index;
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_mutex, 1024, 1024>
    PFS_mutex_basic_container;
typedef PFS_partitioned_buffer_scalable_container<PFS_mutex_basic_container,
                                                  PFS_MUTEX_PARTITIONS>
    PFS_mutex_container;
#else
typedef PFS_buffer_container<PFS_mutex> PFS_mutex_container;
#endif
typedef PFS_mutex_container::iterator_type PFS_mutex_iterator;
extern PFS_mutex_container global_mutex_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_rwlock, 1024, 1024>
    PFS_rwlock_container;
#else
typedef PFS_buffer_container<PFS_rwlock> PFS_rwlock_container;
#endif
typedef PFS_rwlock_container::iterator_type PFS_rwlock_iterator;
extern PFS_rwlock_container global_rwlock_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_cond, 256, 256> PFS_cond_container;
#else
typedef PFS_buffer_container<PFS_cond> PFS_cond_container;
#endif
typedef PFS_cond_container::iterator_type PFS_cond_iterator;
extern PFS_cond_container global_cond_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_file, 4 * 1024, 4 * 1024>
    PFS_file_container;
#else
typedef PFS_buffer_container<PFS_file> PFS_file_container;
#endif
typedef PFS_file_container::iterator_type PFS_file_iterator;
extern PFS_file_container global_file_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_socket, 256, 256>
    PFS_socket_container;
#else
typedef PFS_buffer_container<PFS_socket> PFS_socket_container;
#endif
typedef PFS_socket_container::iterator_type PFS_socket_iterator;
extern PFS_socket_container global_socket_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_metadata_lock, 1024, 1024>
    PFS_mdl_container;
#else
typedef PFS_buffer_container<PFS_metadata_lock> PFS_mdl_container;
#endif
typedef PFS_mdl_container::iterator_type PFS_mdl_iterator;
extern PFS_mdl_container global_mdl_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_setup_actor, 128, 1024>
    PFS_setup_actor_container;
#else
typedef PFS_buffer_container<PFS_setup_actor> PFS_setup_actor_container;
#endif
typedef PFS_setup_actor_container::iterator_type PFS_setup_actor_iterator;
extern PFS_setup_actor_container global_setup_actor_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_setup_object, 128, 1024>
    PFS_setup_object_container;
#else
typedef PFS_buffer_container<PFS_setup_object> PFS_setup_object_container;
#endif
typedef PFS_setup_object_container::iterator_type PFS_setup_object_iterator;
extern PFS_setup_object_container global_setup_object_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table, 1024, 1024>
    PFS_table_container;
#else
typedef PFS_buffer_container<PFS_table> PFS_table_container;
#endif
typedef PFS_table_container::iterator_type PFS_table_iterator;
extern PFS_table_container global_table_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share, 4 * 1024, 4 * 1024>
    PFS_table_share_container;
#else
typedef PFS_buffer_container<PFS_table_share> PFS_table_share_container;
#endif
typedef PFS_table_share_container::iterator_type PFS_table_share_iterator;
extern PFS_table_share_container global_table_share_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share_index, 8 * 1024, 8 * 1024>
    PFS_table_share_index_container;
#else
typedef PFS_buffer_container<PFS_table_share_index>
    PFS_table_share_index_container;
#endif
typedef PFS_table_share_index_container::iterator_type
    PFS_table_share_index_iterator;
extern PFS_table_share_index_container global_table_share_index_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share_lock, 4 * 1024, 4 * 1024>
    PFS_table_share_lock_container;
#else
typedef PFS_buffer_container<PFS_table_share_lock>
    PFS_table_share_lock_container;
#endif
typedef PFS_table_share_lock_container::iterator_type
    PFS_table_share_lock_iterator;
extern PFS_table_share_lock_container global_table_share_lock_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_program, 1024, 1024>
    PFS_program_container;
#else
typedef PFS_buffer_container<PFS_program> PFS_program_container;
#endif
typedef PFS_program_container::iterator_type PFS_program_iterator;
extern PFS_program_container global_program_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_prepared_stmt, 1024, 1024>
    PFS_prepared_stmt_container;
#else
typedef PFS_buffer_container<PFS_prepared_stmt> PFS_prepared_stmt_container;
#endif
typedef PFS_prepared_stmt_container::iterator_type PFS_prepared_stmt_iterator;
extern PFS_prepared_stmt_container global_prepared_stmt_container;

class PFS_account_array : public PFS_buffer_default_array<PFS_account> {
 public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_error_stat *m_instr_class_errors_array;
  PFS_memory_shared_stat *m_instr_class_memory_array;
};

class PFS_account_allocator {
 public:
  int alloc_array(PFS_account_array *array);
  void free_array(PFS_account_array *array);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_account, 128, 128, PFS_account_array,
                                      PFS_account_allocator>
    PFS_account_container;
#else
typedef PFS_buffer_container<PFS_account, PFS_account_array,
                             PFS_account_allocator>
    PFS_account_container;
#endif
typedef PFS_account_container::iterator_type PFS_account_iterator;
extern PFS_account_container global_account_container;

class PFS_host_array : public PFS_buffer_default_array<PFS_host> {
 public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_error_stat *m_instr_class_errors_array;
  PFS_memory_shared_stat *m_instr_class_memory_array;
};

class PFS_host_allocator {
 public:
  int alloc_array(PFS_host_array *array);
  void free_array(PFS_host_array *array);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_host, 128, 128, PFS_host_array,
                                      PFS_host_allocator>
    PFS_host_container;
#else
typedef PFS_buffer_container<PFS_host, PFS_host_array, PFS_host_allocator>
    PFS_host_container;
#endif
typedef PFS_host_container::iterator_type PFS_host_iterator;
extern PFS_host_container global_host_container;

class PFS_thread_array : public PFS_buffer_default_array<PFS_thread> {
 public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_error_stat *m_instr_class_errors_array;
  PFS_memory_safe_stat *m_instr_class_memory_array;

  PFS_events_waits *m_waits_history_array;
  PFS_events_stages *m_stages_history_array;
  PFS_events_statements *m_statements_history_array;
  PFS_events_statements *m_statements_stack_array;
  PFS_events_transactions *m_transactions_history_array;
  char *m_session_connect_attrs_array;

  char *m_current_stmts_text_array;
  char *m_history_stmts_text_array;
  unsigned char *m_current_stmts_digest_token_array;
  unsigned char *m_history_stmts_digest_token_array;
};

class PFS_thread_allocator {
 public:
  int alloc_array(PFS_thread_array *array);
  void free_array(PFS_thread_array *array);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_thread, 256, 256, PFS_thread_array,
                                      PFS_thread_allocator>
    PFS_thread_container;
#else
typedef PFS_buffer_container<PFS_thread, PFS_thread_array, PFS_thread_allocator>
    PFS_thread_container;
#endif
typedef PFS_thread_container::iterator_type PFS_thread_iterator;
extern PFS_thread_container global_thread_container;

class PFS_user_array : public PFS_buffer_default_array<PFS_user> {
 public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_error_stat *m_instr_class_errors_array;
  PFS_memory_shared_stat *m_instr_class_memory_array;
};

class PFS_user_allocator {
 public:
  int alloc_array(PFS_user_array *array);
  void free_array(PFS_user_array *array);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_user, 128, 128, PFS_user_array,
                                      PFS_user_allocator>
    PFS_user_container;
#else
typedef PFS_buffer_container<PFS_user, PFS_user_array, PFS_user_allocator>
    PFS_user_container;
#endif
typedef PFS_user_container::iterator_type PFS_user_iterator;
extern PFS_user_container global_user_container;

#endif
