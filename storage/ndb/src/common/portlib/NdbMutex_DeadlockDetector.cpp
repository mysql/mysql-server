/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbMutex.h>
#include <NdbThread.h>
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR

#include "NdbMutex_DeadlockDetector.h"

static thread_local ndb_mutex_thr_state* NDB_THREAD_TLS_SELF = nullptr;

static NdbMutex g_mutex_no_mutex; // We need a mutex to assign numbers to mutexes...
static nmdd_mask g_mutex_no_mask = { 0, 0 };

static unsigned alloc_mutex_no();
static void release_mutex_no(unsigned no);

static NdbMutex* get_element(struct nmdd_mutex_array *, unsigned i);
static void add_mutex_to_array(struct nmdd_mutex_array *, NdbMutex*);
static void remove_mutex_from_array(struct nmdd_mutex_array *, NdbMutex*);

static void set_bit(struct nmdd_mask *, unsigned no);
static void clear_bit(struct nmdd_mask* , unsigned no);
static bool check_bit(const struct nmdd_mask* , unsigned no);

static void release(struct nmdd_mutex_array *);
static void release(struct nmdd_mask*);

extern "C"
void
NdbMutex_DeadlockDetectorInit()
{
  NdbMutex_Init(&g_mutex_no_mutex);
}

extern "C"
void
NdbMutex_DeadlockDetectorEnd()
{
  release(&g_mutex_no_mask);
}

extern "C"
void
ndb_mutex_created(NdbMutex* p)
{
  p->m_mutex_state = (ndb_mutex_state*)malloc(sizeof(ndb_mutex_state));
  bzero(p->m_mutex_state, sizeof(ndb_mutex_state));

  /**
   * Assign mutex no
   */
  p->m_mutex_state->m_no = alloc_mutex_no();
}

extern "C"
void
ndb_mutex_destoyed(NdbMutex* p)
{
  unsigned no = p->m_mutex_state->m_no;

  /**
   * In order to be able to reuse mutex_no,
   *   we need to clear this no from all mutexes that has it in before map...
   *   this is all mutexes in after map
   */
  for (unsigned i = 0; i<p->m_mutex_state->m_locked_after_list.m_used; i++)
  {
    NdbMutex * m = get_element(&p->m_mutex_state->m_locked_after_list, i);
    assert(check_bit(&p->m_mutex_state->m_locked_after_mask, m->m_mutex_state->m_no));

    /**
     * And we need to lock it while doing this
     */
    NdbMutex_Lock(m);
    assert(check_bit(&m->m_mutex_state->m_locked_before_mask, no));
    clear_bit(&m->m_mutex_state->m_locked_before_mask, no);
    remove_mutex_from_array(&m->m_mutex_state->m_locked_before_list, p);
    NdbMutex_Unlock(m);
  }

  /**
   * And we need to remove ourselfs from after list of mutexes in out before list
   */
  for (unsigned i = 0; i<p->m_mutex_state->m_locked_before_list.m_used; i++)
  {
    NdbMutex * m = get_element(&p->m_mutex_state->m_locked_before_list, i);
    NdbMutex_Lock(m);
    assert(check_bit(&m->m_mutex_state->m_locked_after_mask, no));
    clear_bit(&m->m_mutex_state->m_locked_after_mask, no);
    remove_mutex_from_array(&m->m_mutex_state->m_locked_after_list, p);
    NdbMutex_Unlock(m);
  }

  release(&p->m_mutex_state->m_locked_before_mask);
  release(&p->m_mutex_state->m_locked_before_list);
  release(&p->m_mutex_state->m_locked_after_mask);
  release(&p->m_mutex_state->m_locked_after_list);
  release_mutex_no(no);
}

static
ndb_mutex_thr_state*
get_thr()
{
  return NDB_THREAD_TLS_SELF;
}

#define INC_SIZE 16

static
void
add_lock_to_thread(ndb_mutex_thr_state * t, NdbMutex * m)
{
  add_mutex_to_array(&t->m_mutexes_locked, m);
}

static
void
add_lock_to_mutex_before_list(ndb_mutex_state * m1, NdbMutex * m2)
{
  assert(m1 != m2->m_mutex_state);
  unsigned no = m2->m_mutex_state->m_no;
  if (!check_bit(&m1->m_locked_before_mask, no))
  {
    set_bit(&m1->m_locked_before_mask, no);
    add_mutex_to_array(&m1->m_locked_before_list, m2);
  }
}

static
void
add_lock_to_mutex_after_list(ndb_mutex_state * m1, NdbMutex* m2)
{
  assert(m1 != m2->m_mutex_state);
  unsigned no = m2->m_mutex_state->m_no;
  if (!check_bit(&m1->m_locked_after_mask, no))
  {
    set_bit(&m1->m_locked_after_mask, no);
    add_mutex_to_array(&m1->m_locked_after_list, m2);
  }
}

extern "C"
void
ndb_mutex_locked(NdbMutex* p)
{
  ndb_mutex_state * m = p->m_mutex_state;
  ndb_mutex_thr_state * thr = get_thr();
  if (thr == 0)
  {
    /**
     * These are threads not started with NdbThread_Create(...)
     *   e.g mysql-server threads...ignore these for now
     */
    return;
  }

  for (unsigned i = 0; i < thr->m_mutexes_locked.m_used; i++)
  {
    /**
     * We want to lock m
     * Check that none of the mutex we curreny have locked
     *   have m in their *before* list
     */
    NdbMutex * h = get_element(&thr->m_mutexes_locked, i);
    if (check_bit(&h->m_mutex_state->m_locked_before_mask, m->m_no))
    {
      abort();
    }

    /**
     * Add h to m's list of before-locks
     */
    add_lock_to_mutex_before_list(m, h);

    /**
     * Add m to h's list of after locks
     */
    add_lock_to_mutex_after_list(h->m_mutex_state, p);
  }

  add_lock_to_thread(thr, p);
}

extern "C"
void
ndb_mutex_unlocked(NdbMutex* m)
{
  ndb_mutex_thr_state * thr = get_thr();
  if (thr == 0)
  {
    /**
     * These are threads not started with NdbThread_Create(...)
     *   e.g mysql-server threads...ignore these for now
     */
    return;
  }
  unsigned pos = thr->m_mutexes_locked.m_used;
  assert(pos > 0);
  assert(get_element(&thr->m_mutexes_locked, pos-1) == m);
  thr->m_mutexes_locked.m_used --;
}

extern "C"
void
ndb_mutex_try_locked(NdbMutex* p)
{

}

extern "C"
void
ndb_mutex_thread_init(struct ndb_mutex_thr_state* p)
{
  memset(p, 0, sizeof(* p));
  NDB_THREAD_TLS_SELF = p;
}

extern "C"
void
ndb_mutex_thread_exit()
{
  ndb_mutex_thr_state * thr = get_thr();
  if (thr == 0)
  {
    /**
     * These are threads not started with NdbThread_Create(...)
     *   e.g mysql-server threads...ignore these for now
     */
    return;
  }
  release(&thr->m_mutexes_locked);
}

/**
 * util
 */
static
void
set_bit(nmdd_mask * mask, unsigned no)
{
  unsigned byte_no = no / 8;
  unsigned bit_no = no & 7;
  if (byte_no >= mask->m_len)
  {
    unsigned new_len = mask->m_len + INC_SIZE;
    if (byte_no >= new_len)
    {
      new_len = byte_no + 1;
    }
    unsigned char * new_arr = (unsigned char*)malloc(new_len);
    bzero(new_arr, new_len);
    if (mask->m_len != 0)
    {
      memcpy(new_arr, mask->m_mask, mask->m_len);
      free(mask->m_mask);
    }
    mask->m_len = new_len;
    mask->m_mask = new_arr;
  }

  mask->m_mask[byte_no] |= (1 << bit_no);
}

static
void
clear_bit(nmdd_mask * mask, unsigned no)
{
  unsigned byte_no = no / 8;
  unsigned bit_no = no & 7;
  if (byte_no >= mask->m_len)
  {
    return;
  }

  mask->m_mask[byte_no] &= ~(unsigned char)(1 << bit_no);
}

static
bool
check_bit(const nmdd_mask * mask, unsigned no)
{
  unsigned byte_no = no / 8;
  unsigned bit_no = no & 7;
  if (byte_no >= mask->m_len)
  {
    return false;
  }

  return (mask->m_mask[byte_no] & (1 << bit_no)) != 0;
}

static
void
release(nmdd_mask * mask)
{
  if (mask->m_len != 0)
  {
    free(mask->m_mask);
  }
}

static
NdbMutex*
get_element(nmdd_mutex_array* arr, unsigned i)
{
  assert(i < arr->m_used);
  return arr->m_array[i];
}

static
void
add_mutex_to_array(nmdd_mutex_array* arr, NdbMutex* m)
{
  unsigned pos = arr->m_used;
  if (arr->m_used == arr->m_array_len)
  {
    unsigned new_len = arr->m_array_len + INC_SIZE;
    NdbMutex** new_arr = (NdbMutex**)malloc(new_len * sizeof(NdbMutex*));
    if (arr->m_array_len != 0)
    {
      memcpy(new_arr, arr->m_array, arr->m_array_len * sizeof(NdbMutex*));
      free(arr->m_array);
    }
    arr->m_array = new_arr;
    arr->m_array_len = new_len;
  }
  for (unsigned i = 0; i<arr->m_used; i++)
    assert(arr->m_array[i] != m);

  arr->m_array[pos] = m;
  arr->m_used++;
}

static
void
remove_mutex_from_array(nmdd_mutex_array* arr, NdbMutex* m)
{
  for (unsigned i = 0; i < arr->m_used; i++)
  {
    unsigned idx = arr->m_used - i - 1;
    if (arr->m_array[idx] == m)
    {
      memmove(arr->m_array+idx,
              arr->m_array + idx + 1,
              i * sizeof(NdbMutex*));
      arr->m_used--;
      return;
    }
  }
  assert(false);
}

static
void
release(nmdd_mutex_array* arr)
{
  if (arr->m_array_len)
  {
    free(arr->m_array);
  }
}

static
unsigned
ff(unsigned char b)
{
  for (unsigned i = 0; i<8; i++)
    if ((b & (1 << i)) == 0)
      return i;
  assert(false);
}

static
unsigned
alloc_mutex_no()
{
  Guard g(&g_mutex_no_mutex);
  unsigned no = 0;

  for (unsigned i = 0; i < g_mutex_no_mask.m_len; i++)
  {
    if (g_mutex_no_mask.m_mask[i] != 255)
    {
      no = (8 * i) + ff(g_mutex_no_mask.m_mask[i]);
      goto found;
    }
  }

  no = 8 * g_mutex_no_mask.m_len;
found:
  set_bit(&g_mutex_no_mask, no);
  assert(check_bit(&g_mutex_no_mask, no));
  return no;
}

static
void
release_mutex_no(unsigned no)
{
  Guard g(&g_mutex_no_mutex);
  assert(check_bit(&g_mutex_no_mask, no));
  clear_bit(&g_mutex_no_mask, no);
}

#endif
