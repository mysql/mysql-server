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
  concurrent allocator based on pinning addresses

  strictly speaking it's not lock-free, as it can be blocked
  if a thread's purgatory is full and all addresses from there
  are pinned.

  But until the above happens, it's wait-free.

  It can be made strictly wait-free by increasing purgatory size.
  If it's larger than pins_in_stack*LF_PINBOX_PINS, then apocalyptical
  condition above will never happen. But than the memory requirements
  will be O(pins_in_stack^2).

  Note, that for large purgatory sizes it makes sense to remove
  purgatory array, and link objects in a list using embedded pointer.

  TODO test with more than 256 threads
  TODO test w/o alloca
*/

#include <my_global.h>
#include <my_sys.h>
#include <lf.h>

#define LF_PINBOX_MAX_PINS 65536

static void _lf_pinbox_real_free(LF_PINS *pins);

void lf_pinbox_init(LF_PINBOX *pinbox, lf_pinbox_free_func *free_func,
                   void *free_func_arg)
{
  DBUG_ASSERT(sizeof(LF_PINS) == 128);
  lf_dynarray_init(&pinbox->pinstack, sizeof(LF_PINS));
  pinbox->pinstack_top_ver=0;
  pinbox->pins_in_stack=0;
  pinbox->free_func=free_func;
  pinbox->free_func_arg=free_func_arg;
}

void lf_pinbox_destroy(LF_PINBOX *pinbox)
{
  lf_dynarray_destroy(&pinbox->pinstack);
}

LF_PINS *_lf_pinbox_get_pins(LF_PINBOX *pinbox)
{
  uint32 pins, next, top_ver;
  LF_PINS *el;

  top_ver=pinbox->pinstack_top_ver;
  do
  {
    if (!(pins=top_ver % LF_PINBOX_MAX_PINS))
    {
      pins=my_atomic_add32(&pinbox->pins_in_stack, 1)+1;
      el=(LF_PINS *)_lf_dynarray_lvalue(&pinbox->pinstack, pins);
      break;
    }
    el=(LF_PINS *)_lf_dynarray_value(&pinbox->pinstack, pins);
    next=el->link;
  } while (!my_atomic_cas32(&pinbox->pinstack_top_ver, &top_ver,
                            top_ver-pins+next+LF_PINBOX_MAX_PINS));
  el->link=pins;
  el->purgatory_count=0;
  el->pinbox=pinbox;
  return el;
}

void _lf_pinbox_put_pins(LF_PINS *pins)
{
  LF_PINBOX *pinbox=pins->pinbox;
  uint32 top_ver, nr;
  nr=pins->link;
#ifdef MY_LF_EXTRA_DEBUG
  {
    int i;
    for (i=0; i < LF_PINBOX_PINS; i++)
      assert(pins->pin[i] == 0);
  }
#endif
  while (pins->purgatory_count)
  {
    _lf_pinbox_real_free(pins);
    if (pins->purgatory_count && my_getncpus() == 1)
    {
      my_atomic_rwlock_wrunlock(&pins->pinbox->pinstack.lock);
      pthread_yield();
      my_atomic_rwlock_wrlock(&pins->pinbox->pinstack.lock);
    }
  }
  top_ver=pinbox->pinstack_top_ver;
  if (nr == pinbox->pins_in_stack)
  {
    int32 tmp=nr;
    if (my_atomic_cas32(&pinbox->pins_in_stack, &tmp, tmp-1))
      goto ret;
  }

  do
  {
    pins->link=top_ver % LF_PINBOX_MAX_PINS;
  } while (!my_atomic_cas32(&pinbox->pinstack_top_ver, &top_ver,
                            top_ver-pins->link+nr+LF_PINBOX_MAX_PINS));
ret:
  return;
}

static int ptr_cmp(void **a, void **b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

void _lf_pinbox_free(LF_PINS *pins, void *addr)
{
  while (pins->purgatory_count == LF_PURGATORY_SIZE)
  {
    _lf_pinbox_real_free(pins);
    if (pins->purgatory_count == LF_PURGATORY_SIZE && my_getncpus() == 1)
    {
      my_atomic_rwlock_wrunlock(&pins->pinbox->pinstack.lock);
      pthread_yield();
      my_atomic_rwlock_wrlock(&pins->pinbox->pinstack.lock);
    }
  }
  pins->purgatory[pins->purgatory_count++]=addr;
}

struct st_harvester {
  void **granary;
  int npins;
};

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

static void _lf_pinbox_real_free(LF_PINS *pins)
{
  int npins;
  void **addr=0;
  void **start, **cur, **end=pins->purgatory+pins->purgatory_count;
  LF_PINBOX *pinbox=pins->pinbox;

  npins=pinbox->pins_in_stack+1;

#ifdef HAVE_ALLOCA
  /* create a sorted list of pinned addresses, to speed up searches */
  if (sizeof(void *)*LF_PINBOX_PINS*npins < my_thread_stack_size)
  {
    struct st_harvester hv;
    addr= (void **) alloca(sizeof(void *)*LF_PINBOX_PINS*npins);
    hv.granary=addr;
    hv.npins=npins;
    _lf_dynarray_iterate(&pinbox->pinstack,
                         (lf_dynarray_func)harvest_pins, &hv);

    npins=hv.granary-addr;
    if (npins)
      qsort(addr, npins, sizeof(void *), (qsort_cmp)ptr_cmp);
  }
#endif

  start= cur= pins->purgatory;
  end= start+pins->purgatory_count;
  for (; cur < end; cur++)
  {
    if (npins)
    {
      if (addr)
      {
        void **a,**b,**c;
        for (a=addr, b=addr+npins-1, c=a+(b-a)/2; b-a>1; c=a+(b-a)/2)
          if (*cur == *c)
            a=b=c;
          else if (*cur > *c)
            a=c;
          else
            b=c;
        if (*cur == *a || *cur == *b)
          goto found;
      }
      else
      {
        if (_lf_dynarray_iterate(&pinbox->pinstack,
                                 (lf_dynarray_func)match_pins, *cur))
          goto found;
      }
    }
    /* not pinned - freeing */
    pinbox->free_func(*cur, pinbox->free_func_arg);
    continue;
found:
    /* pinned - keeping */
    *start++=*cur;
  }
  pins->purgatory_count=start-pins->purgatory;
#ifdef MY_LF_EXTRA_DEBUG
  while (start < pins->purgatory + LF_PURGATORY_SIZE)
    *start++=0;
#endif
}

static void alloc_free(void *node, LF_ALLOCATOR *allocator)
{
  void *tmp;
  tmp=allocator->top;
  do
  {
    (*(void **)node)=tmp;
  } while (!my_atomic_casptr((void **)&allocator->top, (void **)&tmp, node) &&
           LF_BACKOFF);
}

LF_REQUIRE_PINS(1);
void *_lf_alloc_new(LF_PINS *pins)
{
  LF_ALLOCATOR *allocator=(LF_ALLOCATOR *)(pins->pinbox->free_func_arg);
  void *node;
  for (;;)
  {
    do
    {
      node=allocator->top;
      _lf_pin(pins, 0, node);
    } while (node !=allocator->top && LF_BACKOFF);
    if (!node)
    {
      if (!(node=my_malloc(allocator->element_size, MYF(MY_WME|MY_ZEROFILL))))
        goto ret;
#ifdef MY_LF_EXTRA_DEBUG
      my_atomic_add32(&allocator->mallocs, 1);
#endif
      goto ret;
    }
    if (my_atomic_casptr((void **)&allocator->top,
                         (void *)&node, *(void **)node))
      goto ret;
  }
ret:
  _lf_unpin(pins, 0);
  return node;
}

void lf_alloc_init(LF_ALLOCATOR *allocator, uint size)
{
  lf_pinbox_init(&allocator->pinbox,
                 (lf_pinbox_free_func *)alloc_free, allocator);
  allocator->top=0;
  allocator->mallocs=0;
  allocator->element_size=size;
  DBUG_ASSERT(size >= (int)sizeof(void *));
}

void lf_alloc_destroy(LF_ALLOCATOR *allocator)
{
  void *el=allocator->top;
  while (el)
  {
    void *tmp=*(void **)el;
    my_free(el, MYF(0));
    el=tmp;
  }
  lf_pinbox_destroy(&allocator->pinbox);
  allocator->top=0;
}

/*
  NOTE
    this is NOT thread-safe !!!
*/
uint lf_alloc_in_pool(LF_ALLOCATOR *allocator)
{
  uint i;
  void *node;
  for (node=allocator->top, i=0; node; node=*(void **)node, i++) /* no op */;
  return i;
}

