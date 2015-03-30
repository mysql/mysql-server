/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_BUFFER_CONTAINER_H
#define PFS_BUFFER_CONTAINER_H

#include "my_global.h"
#include "pfs_lock.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"
#include "pfs_program.h"
#include "pfs_prepared_stmt.h"
#include "pfs_builtin_memory.h"

#define USE_SCALABLE

struct PFS_builtin_memory_class;

template <class T>
class PFS_buffer_const_iterator;

template <class T>
class PFS_buffer_processor;

template <class T, class U, class V>
class PFS_buffer_iterator;

template <class T, int PAGE_SIZE, int PAGE_COUNT, class U, class V>
class PFS_buffer_scalable_iterator;

template <class T>
class PFS_buffer_default_array;

template <class T>
class PFS_buffer_default_allocator;

template <class T, class U, class V>
class PFS_buffer_container;

template <class T, int PAGE_SIZE, int PAGE_COUNT, class U, class V>
class PFS_buffer_scalable_container;

template <class T>
class PFS_buffer_default_array
{
public:
  typedef T value_type;

  value_type *allocate(pfs_dirty_state *dirty_state, size_t max)
  {
    size_t index;
    size_t monotonic;
    size_t monotonic_max;
    value_type *pfs;

    if (m_full)
      return NULL;

    monotonic= PFS_atomic::add_u32(& m_monotonic.m_u32, 1);
    monotonic_max= monotonic + max;

    do
    {
      index= monotonic % max;
      pfs= m_ptr + index;

      if (pfs->m_lock.free_to_dirty(dirty_state))
      {
        return pfs;
      }
      monotonic= PFS_atomic::add_u32(& m_monotonic.m_u32, 1);
    }
    while (monotonic < monotonic_max);

    m_full= true;
    return NULL;
  }

  void deallocate(value_type *pfs)
  {
    pfs->m_lock.allocated_to_free();
    m_full= false;
  }

  bool m_full;
  PFS_cacheline_uint32 m_monotonic;
  T * m_ptr;
};

template <class T>
class PFS_buffer_default_allocator
{
public:
  typedef PFS_buffer_default_array<T> array_type;

  PFS_buffer_default_allocator(PFS_builtin_memory_class *klass)
    : m_builtin_class(klass)
  {}

  int alloc_array(array_type *array, size_t size)
  {
    array->m_ptr= NULL;
    array->m_full= true;
    array->m_monotonic.m_u32= 0;

    if (size > 0)
    {
      array->m_ptr= PFS_MALLOC_ARRAY(m_builtin_class,
                                     size, T, MYF(MY_ZEROFILL));
      if (array->m_ptr == NULL)
        return 1;
      array->m_full= false;
    }
    return 0;
  }

  void free_array(array_type *array, size_t size)
  {
    PFS_FREE_ARRAY(m_builtin_class,
                   size, T, array->m_ptr);
    array->m_ptr= NULL;
  }

private:
  PFS_builtin_memory_class *m_builtin_class;
};

template <class T,
          class U = PFS_buffer_default_array<T>,
          class V = PFS_buffer_default_allocator<T> >
class PFS_buffer_container
{
public:
  friend class PFS_buffer_iterator<T, U, V>;

  typedef T value_type;
  typedef U array_type;
  typedef V allocator_type;
  typedef PFS_buffer_const_iterator<T> const_iterator_type;
  typedef PFS_buffer_iterator<T, U, V> iterator_type;
  typedef PFS_buffer_processor<T> processor_type;
  typedef void (*function_type)(value_type *);

  PFS_buffer_container(allocator_type *allocator)
  {
    m_array.m_full= true;
    m_array.m_ptr= NULL;
    m_array.m_monotonic.m_u32= 0;
    m_lost= 0;
    m_max= 0;
    m_allocator= allocator;
  }

  int init(ulong max_size)
  {
    if (max_size > 0)
    {
      int rc= m_allocator->alloc_array(& m_array, max_size);
      if (rc != 0)
      {
        m_allocator->free_array(& m_array, max_size);
        return 1;
      }
      m_max= max_size;
      m_array.m_full= false;
    }
    return 0;
  }

  void cleanup()
  {
    m_allocator->free_array(& m_array, m_max);
  }

  ulong get_row_count() const
  {
    return m_max;
  }

  ulong get_row_size() const
  {
    return sizeof(value_type);
  }

  ulong get_memory() const
  {
    return get_row_count() * get_row_size();
  }

  value_type *allocate(pfs_dirty_state *dirty_state)
  {
    value_type *pfs;

    pfs= m_array.allocate(dirty_state, m_max);
    if (pfs == NULL)
    {
      m_lost++;
    }

    return pfs;
  }

  void deallocate(value_type *pfs)
  {
    m_array.deallocate(pfs);
  }

  iterator_type iterate()
  {
    return PFS_buffer_iterator<T, U, V>(this, 0);
  }

  iterator_type iterate(uint index)
  {
    DBUG_ASSERT(index <= m_max);
    return PFS_buffer_iterator<T, U, V>(this, index);
  }

  void apply(function_type fct)
  {
    value_type *pfs= m_array.m_ptr;
    value_type *pfs_last= pfs + m_max;

    while (pfs < pfs_last)
    {
      if (pfs->m_lock.is_populated())
      {
        fct(pfs);
      }
      pfs++;
    }
  }

  void apply_all(function_type fct)
  {
    value_type *pfs= m_array.m_ptr;
    value_type *pfs_last= pfs + m_max;

    while (pfs < pfs_last)
    {
      fct(pfs);
      pfs++;
    }
  }

  void apply(processor_type & proc)
  {
    value_type *pfs= m_array.m_ptr;
    value_type *pfs_last= pfs + m_max;

    while (pfs < pfs_last)
    {
      if (pfs->m_lock.is_populated())
      {
        proc(pfs);
      }
      pfs++;
    }
  }

  void apply_all(processor_type & proc)
  {
    value_type *pfs= m_array.m_ptr;
    value_type *pfs_last= pfs + m_max;

    while (pfs < pfs_last)
    {
      proc(pfs);
      pfs++;
    }
  }

  inline value_type* get(uint index)
  {
    DBUG_ASSERT(index < m_max);

    value_type *pfs= m_array.m_ptr + index;
    if (pfs->m_lock.is_populated())
    {
      return pfs;
    }

    return NULL;
  }

  value_type* get(uint index, bool *has_more)
  {
    if (index >= m_max)
    {
      *has_more= false;
      return NULL;
    }

    *has_more= true;
    return get(index);
  }

  value_type *sanitize(value_type *unsafe)
  {
    intptr offset;
    value_type *pfs= m_array.m_ptr;
    value_type *pfs_last= pfs + m_max;

    if ((pfs <= unsafe) &&
        (unsafe < pfs_last))
    {
      offset= ((intptr) unsafe - (intptr) pfs) % sizeof(value_type);
      if (offset == 0)
        return unsafe;
    }

    return NULL;
  }

  ulong m_lost;

private:
  value_type* scan_next(uint & index, uint * found_index)
  {
    DBUG_ASSERT(index <= m_max);

    value_type *pfs_first= m_array.m_ptr;
    value_type *pfs= pfs_first + index;
    value_type *pfs_last= pfs_first + m_max;

    while (pfs < pfs_last)
    {
      if (pfs->m_lock.is_populated())
      {
        uint found= pfs - pfs_first;
        *found_index= found;
        index= found + 1;
        return pfs;
      }
      pfs++;
    }

    index= m_max;
    return NULL;
  }

  ulong m_max;
  array_type m_array;
  allocator_type *m_allocator;
};

template <class T,
          int PAGE_SIZE,
          int PAGE_COUNT,
          class U = PFS_buffer_default_array<T>,
          class V = PFS_buffer_default_allocator<T> >
class PFS_buffer_scalable_container
{
public:
  friend class PFS_buffer_scalable_iterator<T, PAGE_SIZE, PAGE_COUNT, U, V>;

  typedef T value_type;
  typedef U array_type;
  typedef V allocator_type;
  typedef PFS_buffer_const_iterator<T> const_iterator_type;
  typedef PFS_buffer_scalable_iterator<T, PAGE_SIZE, PAGE_COUNT, U, V> iterator_type;
  typedef PFS_buffer_processor<T> processor_type;
  typedef void (*function_type)(value_type *);

  PFS_buffer_scalable_container(allocator_type *allocator)
  {
    m_allocator= allocator;
  }

  int init(long max_size)
  {
    int i;

    m_full= true;
    m_max= PAGE_COUNT * PAGE_SIZE;
    m_max_page_count= PAGE_COUNT;
    m_last_page_size= PAGE_SIZE;
    m_lost= 0;
    m_monotonic.m_u32= 0;
    m_max_page_index.m_u32= 0;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      m_pages[i]= NULL;
    }

    if (max_size == 0)
    {
      /* No allocation. */
      m_max_page_count= 0;
    }
    else if (max_size > 0)
    {
      if (max_size % PAGE_SIZE == 0)
      {
        m_max_page_count= max_size / PAGE_SIZE;
      }
      else
      {
        m_max_page_count= max_size / PAGE_SIZE + 1;
        m_last_page_size= max_size % PAGE_SIZE;
      }
      /* Bounded allocation. */
      m_full= false;
    }
    else
    {
      /* max_size = -1 means unbounded allocation */
      m_full= false;
    }
    return 0;
  }

  void cleanup()
  {
    int i;
    array_type *page;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        m_allocator->free_array(page, PAGE_SIZE);
        delete page;
        m_pages[i]= NULL;
      }
    }
  }

  ulong get_row_count()
  {
    ulong page_count= PFS_atomic::load_u32(& m_max_page_index.m_u32);

    return page_count * PAGE_SIZE;
  }

  ulong get_row_size() const
  {
    return sizeof(value_type);
  }

  ulong get_memory()
  {
    return get_row_count() * get_row_size();
  }

  value_type *allocate(pfs_dirty_state *dirty_state)
  {
    if (m_full)
    {
      m_lost++;
      return NULL;
    }

    uint index;
    uint monotonic;
    uint monotonic_max;
    uint current_page_count;
    uint page_logical_size;
    value_type *pfs;
    array_type *array;
    array_type *old_array;

    void *addr;
    void * volatile * typed_addr;
    void *ptr;
    void *old_ptr;

    /*
      1: Try to find an available record within the existing pages
    */
    current_page_count= PFS_atomic::load_u32(& m_max_page_index.m_u32);

    if (current_page_count != 0)
    {
      monotonic= PFS_atomic::load_u32(& m_monotonic.m_u32);
      monotonic_max= monotonic + current_page_count;

      while (monotonic < monotonic_max)
      {
        /*
          Scan in the [0 .. current_page_count - 1] range,
          in parallel with m_monotonic (see below)
        */
        index= monotonic % current_page_count;

        /* Atomic Load, array= m_pages[index] */
        addr= & m_pages[index];
        typed_addr= static_cast<void * volatile *>(addr);
        ptr= my_atomic_loadptr(typed_addr);
        array= static_cast<array_type *>(ptr);

        if (array != NULL)
        {
          page_logical_size= get_page_logical_size(index);
          pfs= array->allocate(dirty_state, page_logical_size);
          if (pfs != NULL)
          {
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
        monotonic= PFS_atomic::add_u32(& m_monotonic.m_u32, 1);
      };
    }

    /*
      2: Try to add a new page, beyond the m_max_page_index limit
    */
    while (current_page_count < m_max_page_count)
    {
      /* Peek for pages added by collaborating threads */

      /* (2-a) Atomic Load, array= m_pages[current_page_count] */
      addr= & m_pages[current_page_count];
      typed_addr= static_cast<void * volatile *>(addr);
      ptr= my_atomic_loadptr(typed_addr);
      array= static_cast<array_type *>(ptr);

      if (array == NULL)
      {
        /* (2-b) Found no page, allocate a new one */
        array= new array_type();
        builtin_memory_scalable_buffer.count_alloc(sizeof (array_type));

        int rc= m_allocator->alloc_array(array, PAGE_SIZE);
        if (rc != 0)
        {
          m_allocator->free_array(array, PAGE_SIZE);
          delete array;
          builtin_memory_scalable_buffer.count_free(sizeof (array_type));
          m_lost++;
          return NULL;
        }

        /* (2-c) Atomic CAS, array <==> (m_pages[current_page_count] if NULL)  */
        old_ptr= NULL;
        ptr= array;
        if (my_atomic_casptr(typed_addr, & old_ptr, ptr))
        {
          /* CAS: Ok */

          /* Advertise the new page */
          PFS_atomic::add_u32(& m_max_page_index.m_u32, 1);
        }
        else
        {
          /* CAS: Race condition with another thread */

          old_array= static_cast<array_type *>(old_ptr);

          /* Delete the page */
          m_allocator->free_array(array, PAGE_SIZE);
          delete array;
          builtin_memory_scalable_buffer.count_free(sizeof (array_type));

          /* Use the new page added concurrently instead */
          array= old_array;
        }
      }

      DBUG_ASSERT(array != NULL);
      page_logical_size= get_page_logical_size(current_page_count);
      pfs= array->allocate(dirty_state, page_logical_size);
      if (pfs != NULL)
      {
        return pfs;
      }

      current_page_count++;
    }

    m_lost++;
    m_full= true;
    return NULL;
  }

  void deallocate(value_type *safe_pfs)
  {
    /* Mark the object free */
    safe_pfs->m_lock.allocated_to_free();

    /* Flag the containing page as not full. */
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        if ((pfs <= safe_pfs) &&
            (safe_pfs < pfs_last))
        {
          page->m_full= false;
          break;
        }
      }
    }

    /* Flag the overall container as not full. */
    m_full= false;
  }

  iterator_type iterate()
  {
    return PFS_buffer_scalable_iterator<T, PAGE_SIZE, PAGE_COUNT, U, V>(this, 0);
  }

  iterator_type iterate(uint index)
  {
    DBUG_ASSERT(index <= m_max);
    return PFS_buffer_scalable_iterator<T, PAGE_SIZE, PAGE_COUNT, U, V>(this, index);
  }

  void apply(function_type fct)
  {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        while (pfs < pfs_last)
        {
          if (pfs->m_lock.is_populated())
          {
            fct(pfs);
          }
          pfs++;
        }
      }
    }
  }

  void apply_all(function_type fct)
  {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        while (pfs < pfs_last)
        {
          fct(pfs);
          pfs++;
        }
      }
    }
  }

  void apply(processor_type & proc)
  {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        while (pfs < pfs_last)
        {
          if (pfs->m_lock.is_populated())
          {
            proc(pfs);
          }
          pfs++;
        }
      }
    }
  }

  void apply_all(processor_type & proc)
  {
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        while (pfs < pfs_last)
        {
          proc(pfs);
          pfs++;
        }
      }
    }
  }

  value_type* get(uint index)
  {
    DBUG_ASSERT(index < m_max);

    uint index_1= index / PAGE_SIZE;
    array_type *page= m_pages[index_1];
    if (page != NULL)
    {
      uint index_2= index % PAGE_SIZE;
      value_type *pfs= page->m_ptr + index_2;

      if (pfs->m_lock.is_populated())
      {
        return pfs;
      }
    }

    return NULL;
  }

  value_type* get(uint index, bool *has_more)
  {
    if (index >= m_max)
    {
      *has_more= false;
      return NULL;
    }

    uint index_1= index / PAGE_SIZE;
    array_type *page= m_pages[index_1];

    if (page == NULL)
    {
      *has_more= false;
      return NULL;
    }

    *has_more= true;
    uint index_2= index % PAGE_SIZE;
    value_type *pfs= page->m_ptr + index_2;

    if (pfs->m_lock.is_populated())
    {
      return pfs;
    }

    return NULL;
  }

  value_type *sanitize(value_type *unsafe)
  {
    intptr offset;
    uint i;
    array_type *page;
    value_type *pfs;
    value_type *pfs_last;

    for (i=0 ; i < PAGE_COUNT; i++)
    {
      page= m_pages[i];
      if (page != NULL)
      {
        pfs= page->m_ptr;
        pfs_last= pfs + PAGE_SIZE;

        if ((pfs <= unsafe) &&
            (unsafe < pfs_last))
        {
          offset= ((intptr) unsafe - (intptr) pfs) % sizeof(value_type);
          if (offset == 0)
            return unsafe;
        }
      }
    }

    return NULL;
  }

  ulong m_lost;

private:

  uint get_page_logical_size(uint page_index)
  {
    if (page_index + 1 < m_max_page_count)
      return PAGE_SIZE;
    return m_last_page_size;
  }

  value_type* scan_next(uint & index, uint * found_index)
  {
    DBUG_ASSERT(index <= m_max);

    uint index_1= index / PAGE_SIZE;
    uint index_2= index % PAGE_SIZE;
    array_type *page;
    value_type *pfs_first;
    value_type *pfs;
    value_type *pfs_last;

    do
    {
      page= m_pages[index_1];

      if (page == NULL)
      {
        index= static_cast<uint>(m_max);
        return NULL;
      }

      pfs_first= page->m_ptr;
      pfs= pfs_first + index_2;
      pfs_last= pfs_first + PAGE_SIZE;

      while (pfs < pfs_last)
      {
        if (pfs->m_lock.is_populated())
        {
          uint found= index_1 * PAGE_SIZE + static_cast<uint>(pfs - pfs_first);
          *found_index= found;
          index= found + 1;
          return pfs;
        }
        pfs++;
      }

      index_1++;
      index_2= 0;
    }
    while (index_1 < PAGE_COUNT);

    index= static_cast<uint>(m_max);
    return NULL;
  }

  bool m_full;
  size_t m_max;
  PFS_cacheline_uint32 m_monotonic;
  PFS_cacheline_uint32 m_max_page_index;
  ulong m_max_page_count;
  ulong m_last_page_size;
  array_type * m_pages[PAGE_COUNT];
  allocator_type *m_allocator;
};

template <class T, class U, class V>
class PFS_buffer_iterator
{
  friend class PFS_buffer_container<T, U, V>;

  typedef T value_type;
  typedef PFS_buffer_container<T, U, V> container_type;

public:
  value_type* scan_next()
  {
    uint unused;
    return m_container->scan_next(m_index, & unused);
  }

  value_type* scan_next(uint * found_index)
  {
    return m_container->scan_next(m_index, found_index);
  }

private:
  PFS_buffer_iterator(container_type *container, uint index)
    : m_container(container),
      m_index(index)
  {}

  container_type *m_container;
  uint m_index;
};

template <class T, int page_size, int page_count, class U, class V>
class PFS_buffer_scalable_iterator
{
  friend class PFS_buffer_scalable_container<T, page_size, page_count, U, V>;

  typedef T value_type;
  typedef PFS_buffer_scalable_container<T, page_size, page_count, U, V> container_type;

public:
  value_type* scan_next()
  {
    uint unused;
    return m_container->scan_next(m_index, & unused);
  }

  value_type* scan_next(uint * found_index)
  {
    return m_container->scan_next(m_index, found_index);
  }

private:
  PFS_buffer_scalable_iterator(container_type *container, uint index)
    : m_container(container),
      m_index(index)
  {}

  container_type *m_container;
  uint m_index;
};

template <class T>
class PFS_buffer_processor
{
public:
  virtual ~PFS_buffer_processor<T> ()
  {}
  virtual void operator()(T *element) = 0;
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_mutex, 1024, 1024> PFS_mutex_container;
#else
typedef PFS_buffer_container<PFS_mutex> PFS_mutex_container;
#endif
typedef PFS_mutex_container::iterator_type PFS_mutex_iterator;
extern PFS_mutex_container global_mutex_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_rwlock, 1024, 1024> PFS_rwlock_container;
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
typedef PFS_buffer_scalable_container<PFS_file, 1024, 1024> PFS_file_container;
#else
typedef PFS_buffer_container<PFS_file> PFS_file_container;
#endif
typedef PFS_file_container::iterator_type PFS_file_iterator;
extern PFS_file_container global_file_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_socket, 256, 256> PFS_socket_container;
#else
typedef PFS_buffer_container<PFS_socket> PFS_socket_container;
#endif
typedef PFS_socket_container::iterator_type PFS_socket_iterator;
extern PFS_socket_container global_socket_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_metadata_lock, 1024, 1024> PFS_mdl_container;
#else
typedef PFS_buffer_container<PFS_metadata_lock> PFS_mdl_container;
#endif
typedef PFS_mdl_container::iterator_type PFS_mdl_iterator;
extern PFS_mdl_container global_mdl_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_setup_actor, 128, 1024> PFS_setup_actor_container;
#else
typedef PFS_buffer_container<PFS_setup_actor> PFS_setup_actor_container;
#endif
typedef PFS_setup_actor_container::iterator_type PFS_setup_actor_iterator;
extern PFS_setup_actor_container global_setup_actor_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_setup_object, 128, 1024> PFS_setup_object_container;
#else
typedef PFS_buffer_container<PFS_setup_object> PFS_setup_object_container;
#endif
typedef PFS_setup_object_container::iterator_type PFS_setup_object_iterator;
extern PFS_setup_object_container global_setup_object_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table, 1024, 1024> PFS_table_container;
#else
typedef PFS_buffer_container<PFS_table> PFS_table_container;
#endif
typedef PFS_table_container::iterator_type PFS_table_iterator;
extern PFS_table_container global_table_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share, 1024, 1024> PFS_table_share_container;
#else
typedef PFS_buffer_container<PFS_table_share> PFS_table_share_container;
#endif
typedef PFS_table_share_container::iterator_type PFS_table_share_iterator;
extern PFS_table_share_container global_table_share_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share_index, 1024, 1024> PFS_table_share_index_container;
#else
typedef PFS_buffer_container<PFS_table_share_index> PFS_table_share_index_container;
#endif
typedef PFS_table_share_index_container::iterator_type PFS_table_share_index_iterator;
extern PFS_table_share_index_container global_table_share_index_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_table_share_lock, 1024, 1024> PFS_table_share_lock_container;
#else
typedef PFS_buffer_container<PFS_table_share_lock> PFS_table_share_lock_container;
#endif
typedef PFS_table_share_lock_container::iterator_type PFS_table_share_lock_iterator;
extern PFS_table_share_lock_container global_table_share_lock_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_program, 1024, 1024> PFS_program_container;
#else
typedef PFS_buffer_container<PFS_program> PFS_program_container;
#endif
typedef PFS_program_container::iterator_type PFS_program_iterator;
extern PFS_program_container global_program_container;

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_prepared_stmt, 1024, 1024> PFS_prepared_stmt_container;
#else
typedef PFS_buffer_container<PFS_prepared_stmt> PFS_prepared_stmt_container;
#endif
typedef PFS_prepared_stmt_container::iterator_type PFS_prepared_stmt_iterator;
extern PFS_prepared_stmt_container global_prepared_stmt_container;

class PFS_account_array : public PFS_buffer_default_array<PFS_account>
{
public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_memory_stat *m_instr_class_memory_array;
};

class PFS_account_allocator
{
public:
  int alloc_array(PFS_account_array *array, size_t size);
  void free_array(PFS_account_array *array, size_t size);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_account,
                                      128,
                                      128,
                                      PFS_account_array,
                                      PFS_account_allocator> PFS_account_container;
#else
typedef PFS_buffer_container<PFS_account,
                             PFS_account_array,
                             PFS_account_allocator> PFS_account_container;
#endif
typedef PFS_account_container::iterator_type PFS_account_iterator;
extern PFS_account_container global_account_container;

class PFS_host_array : public PFS_buffer_default_array<PFS_host>
{
public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_memory_stat *m_instr_class_memory_array;
};

class PFS_host_allocator
{
public:
  int alloc_array(PFS_host_array *array, size_t size);
  void free_array(PFS_host_array *array, size_t size);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_host,
                                      128,
                                      128,
                                      PFS_host_array,
                                      PFS_host_allocator> PFS_host_container;
#else
typedef PFS_buffer_container<PFS_host,
                             PFS_host_array,
                             PFS_host_allocator> PFS_host_container;
#endif
typedef PFS_host_container::iterator_type PFS_host_iterator;
extern PFS_host_container global_host_container;

class PFS_thread_array : public PFS_buffer_default_array<PFS_thread>
{
public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_memory_stat *m_instr_class_memory_array;

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

class PFS_thread_allocator
{
public:
  int alloc_array(PFS_thread_array *array, size_t size);
  void free_array(PFS_thread_array *array, size_t size);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_thread,
                                      256,
                                      256,
                                      PFS_thread_array,
                                      PFS_thread_allocator> PFS_thread_container;
#else
typedef PFS_buffer_container<PFS_thread,
                             PFS_thread_array,
                             PFS_thread_allocator> PFS_thread_container;
#endif
typedef PFS_thread_container::iterator_type PFS_thread_iterator;
extern PFS_thread_container global_thread_container;

class PFS_user_array : public PFS_buffer_default_array<PFS_user>
{
public:
  PFS_single_stat *m_instr_class_waits_array;
  PFS_stage_stat *m_instr_class_stages_array;
  PFS_statement_stat *m_instr_class_statements_array;
  PFS_transaction_stat *m_instr_class_transactions_array;
  PFS_memory_stat *m_instr_class_memory_array;
};

class PFS_user_allocator
{
public:
  int alloc_array(PFS_user_array *array, size_t size);
  void free_array(PFS_user_array *array, size_t size);
};

#ifdef USE_SCALABLE
typedef PFS_buffer_scalable_container<PFS_user,
                                      128,
                                      128,
                                      PFS_user_array,
                                      PFS_user_allocator> PFS_user_container;
#else
typedef PFS_buffer_container<PFS_user,
                             PFS_user_array,
                             PFS_user_allocator> PFS_user_container;
#endif
typedef PFS_user_container::iterator_type PFS_user_iterator;
extern PFS_user_container global_user_container;

#endif

