/* QQ: TODO multi-pinbox */
/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  wait-free concurrent allocator based on pinning addresses

  It works as follows: every thread (strictly speaking - every CPU, but
  it's too difficult to do) has a small array of pointers. They're called
  "pins".  Before using an object its address must be stored in this array
  (pinned).  When an object is no longer necessary its address must be
  removed from this array (unpinned). When a thread wants to free() an
  object it scans all pins of all threads to see if somebody has this
  object pinned.  If yes - the object is not freed (but stored in a
  "purgatory").  To reduce the cost of a single free() pins are not scanned
  on every free() but only added to (thread-local) purgatory. On every
  LF_PURGATORY_SIZE free() purgatory is scanned and all unpinned objects
  are freed.

  Pins are used to solve ABA problem. To use pins one must obey
  a pinning protocol:
   1. Let's assume that PTR is a shared pointer to an object. Shared means
      that any thread may modify it anytime to point to a different object
      and free the old object. Later the freed object may be potentially
      allocated by another thread. If we're unlucky that another thread may
      set PTR to point to this object again. This is ABA problem.
   2. Create a local pointer LOCAL_PTR.
   3. Pin the PTR in a loop:
      do
      {
        LOCAL_PTR= PTR;
        pin(PTR, PIN_NUMBER);
      } while (LOCAL_PTR != PTR)
   4. It is guaranteed that after the loop has ended, LOCAL_PTR
      points to an object (or NULL, if PTR may be NULL), that
      will never be freed. It is not guaranteed though
      that LOCAL_PTR == PTR (as PTR can change any time)
   5. When done working with the object, remove the pin:
      unpin(PIN_NUMBER)
   6. When copying pins (as in the list traversing loop:
        pin(CUR, 1);
        while ()
        {
          do                            // standard
          {                             //  pinning
            NEXT=CUR->next;             //   loop
            pin(NEXT, 0);               //    see #3
          } while (NEXT != CUR->next);  //     above
          ...
          ...
          CUR=NEXT;
          pin(CUR, 1);                  // copy pin[0] to pin[1]
        }
      which keeps CUR address constantly pinned), note than pins may be
      copied only upwards (!!!), that is pin[N] to pin[M], M > N.
   7. Don't keep the object pinned longer than necessary - the number of
      pins you have is limited (and small), keeping an object pinned
      prevents its reuse and cause unnecessary mallocs.

  Implementation details:
  Pins are given away from a "pinbox". Pinbox is stack-based allocator.
  It used dynarray for storing pins, new elements are allocated by dynarray
  as necessary, old are pushed in the stack for reuse. ABA is solved by
  versioning a pointer - because we use an array, a pointer to pins is 32 bit,
  upper 32 bits are used for a version.
*/

#include <my_global.h>
#include <my_sys.h>
#include <lf.h>

#define LF_PINBOX_MAX_PINS 65536

static void _lf_pinbox_real_free(LF_PINS *pins);

/*
  Initialize a pinbox. Normally called from lf_alloc_init.
  See the latter for details.
*/
void lf_pinbox_init(LF_PINBOX *pinbox, uint free_ptr_offset,
                    lf_pinbox_free_func *free_func, void *free_func_arg)
{
  DBUG_ASSERT(sizeof(LF_PINS) == 128);
  DBUG_ASSERT(free_ptr_offset % sizeof(void *) == 0);
  lf_dynarray_init(&pinbox->pinstack, sizeof(LF_PINS));
  pinbox->pinstack_top_ver= 0;
  pinbox->pins_in_stack= 0;
  pinbox->free_ptr_offset= free_ptr_offset;
  pinbox->free_func= free_func;
  pinbox->free_func_arg= free_func_arg;
}

void lf_pinbox_destroy(LF_PINBOX *pinbox)
{
  lf_dynarray_destroy(&pinbox->pinstack);
}

/*
  Get pins from a pinbox. Usually called via lf_alloc_get_pins() or
  lf_hash_get_pins().

  DESCRIPTION
    get a new LF_PINS structure from a stack of unused pins,
    or allocate a new one out of dynarray.
*/
LF_PINS *_lf_pinbox_get_pins(LF_PINBOX *pinbox)
{
  uint32 pins, next, top_ver;
  LF_PINS *el;

  top_ver= pinbox->pinstack_top_ver;
  do
  {
    if (!(pins= top_ver % LF_PINBOX_MAX_PINS))
    {
      pins= my_atomic_add32(&pinbox->pins_in_stack, 1)+1;
      el= (LF_PINS *)_lf_dynarray_lvalue(&pinbox->pinstack, pins);
      break;
    }
    el= (LF_PINS *)_lf_dynarray_value(&pinbox->pinstack, pins);
    next= el->link;
  } while (!my_atomic_cas32(&pinbox->pinstack_top_ver, &top_ver,
                            top_ver-pins+next+LF_PINBOX_MAX_PINS));
  el->link= pins;
  el->purgatory_count= 0;
  el->pinbox= pinbox;
  return el;
}

/*
  Put pins back to a pinbox. Usually called via lf_alloc_put_pins() or
  lf_hash_put_pins().

  DESCRIPTION
    empty the purgatory (XXX deadlock warning below!),
    push LF_PINS structure to a stack
*/
void _lf_pinbox_put_pins(LF_PINS *pins)
{
  LF_PINBOX *pinbox= pins->pinbox;
  uint32 top_ver, nr;
  nr= pins->link;
#ifdef MY_LF_EXTRA_DEBUG
  {
    int i;
    for (i= 0; i < LF_PINBOX_PINS; i++)
      DBUG_ASSERT(pins->pin[i] == 0);
  }
#endif
  /*
    XXX this will deadlock if other threads will wait for
    the caller to do something after _lf_pinbox_put_pins(),
    and they would have pinned addresses that the caller wants to free.
    Thus: only free pins when all work is done and nobody can wait for you!!!
  */
  while (pins->purgatory_count)
  {
    _lf_pinbox_real_free(pins);
    if (pins->purgatory_count)
    {
      my_atomic_rwlock_wrunlock(&pins->pinbox->pinstack.lock);
      pthread_yield();
      my_atomic_rwlock_wrlock(&pins->pinbox->pinstack.lock);
    }
  }
  top_ver= pinbox->pinstack_top_ver;
  if (nr == pinbox->pins_in_stack)
  {
    int32 tmp= nr;
    if (my_atomic_cas32(&pinbox->pins_in_stack, &tmp, tmp-1))
      goto ret;
  }

  do
  {
    pins->link= top_ver % LF_PINBOX_MAX_PINS;
  } while (!my_atomic_cas32(&pinbox->pinstack_top_ver, &top_ver,
                            top_ver-pins->link+nr+LF_PINBOX_MAX_PINS));
ret:
  return;
}

static int ptr_cmp(void **a, void **b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

#define add_to_purgatory(PINS, ADDR)                                    \
  do                                                                    \
  {                                                                     \
    *(void **)((char *)(ADDR)+(PINS)->pinbox->free_ptr_offset)=         \
      (PINS)->purgatory;                                                \
    (PINS)->purgatory= (ADDR);                                          \
    (PINS)->purgatory_count++;                                          \
  } while (0)

/*
  Free an object allocated via pinbox allocator

  DESCRIPTION
    add an object to purgatory. if necessary, call _lf_pinbox_real_free()
    to actually free something.
*/
void _lf_pinbox_free(LF_PINS *pins, void *addr)
{
  add_to_purgatory(pins, addr);
  if (pins->purgatory_count % LF_PURGATORY_SIZE)
    _lf_pinbox_real_free(pins);
}

struct st_harvester {
  void **granary;
  int npins;
};

/*
  callback for _lf_dynarray_iterate:
  scan all pins or all threads and accumulate all pins
*/
static int harvest_pins(LF_PINS *el, struct st_harvester *hv)
{
  int i;
  LF_PINS *el_end= el+min(hv->npins, LF_DYNARRAY_LEVEL_LENGTH);
  for (; el < el_end; el++)
  {
    for (i= 0; i < LF_PINBOX_PINS; i++)
    {
      void *p= el->pin[i];
      if (p)
        *hv->granary++= p;
    }
  }
  hv->npins-= LF_DYNARRAY_LEVEL_LENGTH;
  return 0;
}

/*
  callback for _lf_dynarray_iterate:
  scan all pins or all threads and see if addr is present there
*/
static int match_pins(LF_PINS *el, void *addr)
{
  int i;
  LF_PINS *el_end= el+LF_DYNARRAY_LEVEL_LENGTH;
  for (; el < el_end; el++)
    for (i= 0; i < LF_PINBOX_PINS; i++)
      if (el->pin[i] == addr)
        return 1;
  return 0;
}

/*
  Scan the purgatory as free everything that can be freed
*/
static void _lf_pinbox_real_free(LF_PINS *pins)
{
  int npins;
  void *list;
  void **addr;
  LF_PINBOX *pinbox= pins->pinbox;

  npins= pinbox->pins_in_stack+1;

#ifdef HAVE_ALLOCA
  /* create a sorted list of pinned addresses, to speed up searches */
  if (sizeof(void *)*LF_PINBOX_PINS*npins < my_thread_stack_size)
  {
    struct st_harvester hv;
    addr= (void **) alloca(sizeof(void *)*LF_PINBOX_PINS*npins);
    hv.granary= addr;
    hv.npins= npins;
    /* scan the dynarray and accumulate all pinned addresses */
    _lf_dynarray_iterate(&pinbox->pinstack,
                         (lf_dynarray_func)harvest_pins, &hv);

    npins= hv.granary-addr;
    /* and sort them */
    if (npins)
      qsort(addr, npins, sizeof(void *), (qsort_cmp)ptr_cmp);
  }
  else
#endif
    addr= 0;

  list= pins->purgatory;
  pins->purgatory= 0;
  pins->purgatory_count= 0;
  while (list)
  {
    void *cur= list;
    list= *(void **)((char *)cur+pinbox->free_ptr_offset);
    if (npins)
    {
      if (addr) /* use binary search */
      {
        void **a, **b, **c;
        for (a= addr, b= addr+npins-1, c= a+(b-a)/2; b-a>1; c= a+(b-a)/2)
          if (cur == *c)
            a= b= c;
          else if (cur > *c)
            a= c;
          else
            b= c;
        if (cur == *a || cur == *b)
          goto found;
      }
      else /* no alloca - no cookie. linear search here */
      {
        if (_lf_dynarray_iterate(&pinbox->pinstack,
                                 (lf_dynarray_func)match_pins, cur))
          goto found;
      }
    }
    /* not pinned - freeing */
    pinbox->free_func(cur, pinbox->free_func_arg);
    continue;
found:
    /* pinned - keeping */
    add_to_purgatory(pins, cur);
  }
}

/*
  callback for _lf_pinbox_real_free to free an unpinned object -
  add it back to the allocator stack
*/
static void alloc_free(struct st_lf_alloc_node *node, LF_ALLOCATOR *allocator)
{
  struct st_lf_alloc_node *tmp;
  tmp= allocator->top;
  do
  {
    node->next= tmp;
  } while (!my_atomic_casptr((void **)&allocator->top, (void **)&tmp, node) &&
           LF_BACKOFF);
}

/* lock-free memory allocator for fixed-size objects */

LF_REQUIRE_PINS(1);

/*
  initialize lock-free allocatod.

  SYNOPSYS
    allocator           -
    size                a size of an object to allocate
    free_ptr_offset     an offset inside the object to a sizeof(void *)
                        memory that is guaranteed to be unused after
                        the object is put in the purgatory. Unused by ANY
                        thread, not only the purgatory owner.
*/
void lf_alloc_init(LF_ALLOCATOR *allocator, uint size, uint free_ptr_offset)
{
  lf_pinbox_init(&allocator->pinbox, free_ptr_offset,
                 (lf_pinbox_free_func *)alloc_free, allocator);
  allocator->top= 0;
  allocator->mallocs= 0;
  allocator->element_size= size;
  DBUG_ASSERT(size >= (int)sizeof(void *));
  DBUG_ASSERT(free_ptr_offset < size);
}

/*
  destroy the allocator, free everything that's in it
*/
void lf_alloc_destroy(LF_ALLOCATOR *allocator)
{
  struct st_lf_alloc_node *node= allocator->top;
  while (node)
  {
    struct st_lf_alloc_node *tmp= node->next;
    my_free((void *)node, MYF(0));
    node= tmp;
  }
  lf_pinbox_destroy(&allocator->pinbox);
  allocator->top= 0;
}

/*
  Allocate and return an new object.

  DESCRIPTION
    Pop an unused object from the stack or malloc it is the stack is empty.
    pin[0] is used, it's removed on return.
*/
void *_lf_alloc_new(LF_PINS *pins)
{
  LF_ALLOCATOR *allocator= (LF_ALLOCATOR *)(pins->pinbox->free_func_arg);
  struct st_lf_alloc_node *node;
  for (;;)
  {
    do
    {
      node= allocator->top;
      _lf_pin(pins, 0, node);
    } while (node != allocator->top && LF_BACKOFF);
    if (!node)
    {
      if (!(node= (void *)my_malloc(allocator->element_size,
                                    MYF(MY_WME|MY_ZEROFILL))))
        break;
#ifdef MY_LF_EXTRA_DEBUG
      my_atomic_add32(&allocator->mallocs, 1);
#endif
      break;
    }
    if (my_atomic_casptr((void **)&allocator->top,
                         (void *)&node, *(void **)node))
      break;
  }
  _lf_unpin(pins, 0);
  return node;
}

/*
  count the number of objects in a pool.

  NOTE
    This is NOT thread-safe !!!
*/
uint lf_alloc_in_pool(LF_ALLOCATOR *allocator)
{
  uint i;
  struct st_lf_alloc_node *node;
  for (node= allocator->top, i= 0; node; node= node->next, i++)
    /* no op */;
  return i;
}

