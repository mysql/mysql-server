/* QQ: TODO multi-pinbox */
/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
      allocated by another thread. If we're unlucky that other thread may
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

  Explanations:

   3. The loop is important. The following can occur:
        thread1> LOCAL_PTR= PTR
        thread2> free(PTR); PTR=0;
        thread1> pin(PTR, PIN_NUMBER);
      now thread1 cannot access LOCAL_PTR, even if it's pinned,
      because it points to a freed memory. That is, it *must*
      verify that it has indeed pinned PTR, the shared pointer.

   6. When a thread wants to free some LOCAL_PTR, and it scans
      all lists of pins to see whether it's pinned, it does it
      upwards, from low pin numbers to high. Thus another thread
      must copy an address from one pin to another in the same
      direction - upwards, otherwise the scanning thread may
      miss it.

  Implementation details:

  Pins are given away from a "pinbox". Pinbox is stack-based allocator.
  It used dynarray for storing pins, new elements are allocated by dynarray
  as necessary, old are pushed in the stack for reuse. ABA is solved by
  versioning a pointer - because we use an array, a pointer to pins is 16 bit,
  upper 16 bits are used for a version.

  It is assumed that pins belong to a THD and are not transferable
  between THD's (LF_PINS::stack_ends_here being a primary reason
  for this limitation).
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
  DBUG_ASSERT(free_ptr_offset % sizeof(void *) == 0);
  compile_time_assert(sizeof(LF_PINS) == 64);
  lf_dynarray_init(&pinbox->pinarray, sizeof(LF_PINS));
  pinbox->pinstack_top_ver= 0;
  pinbox->pins_in_array= 0;
  pinbox->free_ptr_offset= free_ptr_offset;
  pinbox->free_func= free_func;
  pinbox->free_func_arg= free_func_arg;
}

void lf_pinbox_destroy(LF_PINBOX *pinbox)
{
  lf_dynarray_destroy(&pinbox->pinarray);
}

/*
  Get pins from a pinbox. Usually called via lf_alloc_get_pins() or
  lf_hash_get_pins().

  SYNOPSYS
    pinbox      -

  DESCRIPTION
    get a new LF_PINS structure from a stack of unused pins,
    or allocate a new one out of dynarray.

  NOTE
    It is assumed that pins belong to a thread and are not transferable
    between threads.
*/
LF_PINS *_lf_pinbox_get_pins(LF_PINBOX *pinbox)
{
  struct st_my_thread_var *var;
  uint32 pins, next, top_ver;
  LF_PINS *el;
  /*
    We have an array of max. 64k elements.
    The highest index currently allocated is pinbox->pins_in_array.
    Freed elements are in a lifo stack, pinstack_top_ver.
    pinstack_top_ver is 32 bits; 16 low bits are the index in the
    array, to the first element of the list. 16 high bits are a version
    (every time the 16 low bits are updated, the 16 high bits are
    incremented). Versioniong prevents the ABA problem.
  */
  top_ver= pinbox->pinstack_top_ver;
  do
  {
    if (!(pins= top_ver % LF_PINBOX_MAX_PINS))
    {
      /* the stack of free elements is empty */
      pins= my_atomic_add32((int32 volatile*) &pinbox->pins_in_array, 1)+1;
      if (unlikely(pins >= LF_PINBOX_MAX_PINS))
        return 0;
      /*
        note that the first allocated element has index 1 (pins==1).
        index 0 is reserved to mean "NULL pointer"
      */
      el= (LF_PINS *)_lf_dynarray_lvalue(&pinbox->pinarray, pins);
      if (unlikely(!el))
        return 0;
      break;
    }
    el= (LF_PINS *)_lf_dynarray_value(&pinbox->pinarray, pins);
    next= el->link;
  } while (!my_atomic_cas32((int32 volatile*) &pinbox->pinstack_top_ver,
                            (int32*) &top_ver,
                            top_ver-pins+next+LF_PINBOX_MAX_PINS));
  /*
    set el->link to the index of el in the dynarray (el->link has two usages:
    - if element is allocated, it's its own index
    - if element is free, it's its next element in the free stack
  */
  el->link= pins;
  el->purgatory_count= 0;
  el->pinbox= pinbox;
  var= my_thread_var;
  /*
    Threads that do not call my_thread_init() should still be
    able to use the LF_HASH.
  */
  el->stack_ends_here= (var ? & var->stack_ends_here : NULL);
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

#ifndef DBUG_OFF
  {
    /* This thread should not hold any pin. */
    int i;
    for (i= 0; i < LF_PINBOX_PINS; i++)
      DBUG_ASSERT(pins->pin[i] == 0);
  }
#endif /* DBUG_OFF */

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
      my_atomic_rwlock_wrunlock(&pins->pinbox->pinarray.lock);
      pthread_yield();
      my_atomic_rwlock_wrlock(&pins->pinbox->pinarray.lock);
    }
  }
  top_ver= pinbox->pinstack_top_ver;
  do
  {
    pins->link= top_ver % LF_PINBOX_MAX_PINS;
  } while (!my_atomic_cas32((int32 volatile*) &pinbox->pinstack_top_ver,
                            (int32*) &top_ver,
                            top_ver-pins->link+nr+LF_PINBOX_MAX_PINS));
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
  scan all pins of all threads and accumulate all pins
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
  /*
    hv->npins may become negative below, but it means that
    we're on the last dynarray page and harvest_pins() won't be
    called again. We don't bother to make hv->npins() correct
    (that is 0) in this case.
  */
  hv->npins-= LF_DYNARRAY_LEVEL_LENGTH;
  return 0;
}

/*
  callback for _lf_dynarray_iterate:
  scan all pins of all threads and see if addr is present there
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

#if STACK_DIRECTION < 0
#define available_stack_size(CUR,END) (long) ((char*)(CUR) - (char*)(END))
#else
#define available_stack_size(CUR,END) (long) ((char*)(END) - (char*)(CUR))
#endif

#define next_node(P, X) (*((uchar * volatile *)(((uchar *)(X)) + (P)->free_ptr_offset)))
#define anext_node(X) next_node(&allocator->pinbox, (X))

/*
  Scan the purgatory and free everything that can be freed
*/
static void _lf_pinbox_real_free(LF_PINS *pins)
{
  int npins;
  void *list;
  void **addr= NULL;
  void *first= NULL, *last= NULL;
  LF_PINBOX *pinbox= pins->pinbox;

  npins= pinbox->pins_in_array+1;

#ifdef HAVE_ALLOCA
  if (pins->stack_ends_here != NULL)
  {
    int alloca_size= sizeof(void *)*LF_PINBOX_PINS*npins;
    /* create a sorted list of pinned addresses, to speed up searches */
    if (available_stack_size(&pinbox, *pins->stack_ends_here) > alloca_size)
    {
      struct st_harvester hv;
      addr= (void **) alloca(alloca_size);
      hv.granary= addr;
      hv.npins= npins;
      /* scan the dynarray and accumulate all pinned addresses */
      _lf_dynarray_iterate(&pinbox->pinarray,
                           (lf_dynarray_func)harvest_pins, &hv);

      npins= hv.granary-addr;
      /* and sort them */
      if (npins)
        qsort(addr, npins, sizeof(void *), (qsort_cmp)ptr_cmp);
    }
  }
#endif

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
        for (a= addr, b= addr+npins-1, c= a+(b-a)/2; (b-a) > 1; c= a+(b-a)/2)
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
        if (_lf_dynarray_iterate(&pinbox->pinarray,
                                 (lf_dynarray_func)match_pins, cur))
          goto found;
      }
    }
    /* not pinned - freeing */
    if (last)
      last= next_node(pinbox, last)= (uchar *)cur;
    else
      first= last= (uchar *)cur;
    continue;
found:
    /* pinned - keeping */
    add_to_purgatory(pins, cur);
  }
  if (last)
    pinbox->free_func(first, last, pinbox->free_func_arg);
}

/* lock-free memory allocator for fixed-size objects */

LF_REQUIRE_PINS(1)

/*
  callback for _lf_pinbox_real_free to free a list of unpinned objects -
  add it back to the allocator stack

  DESCRIPTION
    'first' and 'last' are the ends of the linked list of nodes:
    first->el->el->....->el->last. Use first==last to free only one element.
*/
static void alloc_free(uchar *first,
                       uchar volatile *last,
                       LF_ALLOCATOR *allocator)
{
  /*
    we need a union here to access type-punned pointer reliably.
    otherwise gcc -fstrict-aliasing will not see 'tmp' changed in the loop
  */
  union { uchar * node; void *ptr; } tmp;
  tmp.node= allocator->top;
  do
  {
    anext_node(last)= tmp.node;
  } while (!my_atomic_casptr((void **)(char *)&allocator->top,
                             (void **)&tmp.ptr, first) && LF_BACKOFF);
}

/*
  initialize lock-free allocator

  SYNOPSYS
    allocator           -
    size                a size of an object to allocate
    free_ptr_offset     an offset inside the object to a sizeof(void *)
                        memory that is guaranteed to be unused after
                        the object is put in the purgatory. Unused by ANY
                        thread, not only the purgatory owner.
                        This memory will be used to link waiting-to-be-freed
                        objects in a purgatory list.
*/
void lf_alloc_init(LF_ALLOCATOR *allocator, uint size, uint free_ptr_offset)
{
  lf_pinbox_init(&allocator->pinbox, free_ptr_offset,
                 (lf_pinbox_free_func *)alloc_free, allocator);
  allocator->top= 0;
  allocator->mallocs= 0;
  allocator->element_size= size;
  DBUG_ASSERT(size >= sizeof(void*) + free_ptr_offset);
}

/*
  destroy the allocator, free everything that's in it

  NOTE
    As every other init/destroy function here and elsewhere it
    is not thread safe. No, this function is no different, ensure
    that no thread needs the allocator before destroying it.
    We are not responsible for any damage that may be caused by
    accessing the allocator when it is being or has been destroyed.
    Oh yes, and don't put your cat in a microwave.
*/
void lf_alloc_destroy(LF_ALLOCATOR *allocator)
{
  uchar *node= allocator->top;
  while (node)
  {
    uchar *tmp= anext_node(node);
    my_free(node);
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
  uchar *node;
  for (;;)
  {
    do
    {
      node= allocator->top;
      _lf_pin(pins, 0, node);
    } while (node != allocator->top && LF_BACKOFF);
    if (!node)
    {
      node= (void *)my_malloc(allocator->element_size, MYF(MY_WME));
#ifdef MY_LF_EXTRA_DEBUG
      if (likely(node != 0))
        my_atomic_add32(&allocator->mallocs, 1);
#endif
      break;
    }
    if (my_atomic_casptr((void **)(char *)&allocator->top,
                         (void *)&node, anext_node(node)))
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
uint lf_alloc_pool_count(LF_ALLOCATOR *allocator)
{
  uint i;
  uchar *node;
  for (node= allocator->top, i= 0; node; node= anext_node(node), i++)
    /* no op */;
  return i;
}

