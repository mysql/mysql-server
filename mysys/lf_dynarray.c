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
  Analog of DYNAMIC_ARRAY that never reallocs
  (so no pointer into the array may ever become invalid).

  Memory is allocated in non-contiguous chunks.
  This data structure is not space efficient for sparce arrays.

  The number of elements is limited to 2^16

  Every element is aligned to sizeof(element) boundary
  (to avoid false sharing if element is big enough).

  Actually, it's wait-free, not lock-free ;-)
*/

#undef DBUG_OFF
#include <my_global.h>
#include <strings.h>
#include <my_sys.h>
#include <lf.h>

void lf_dynarray_init(LF_DYNARRAY *array, uint element_size)
{
  bzero(array, sizeof(*array));
  array->size_of_element=element_size;
  my_atomic_rwlock_init(&array->lock);
}

static void recursive_free(void **alloc, int level)
{
  if (!alloc) return;

  if (level)
  {
    int i;
    for (i=0; i < LF_DYNARRAY_LEVEL_LENGTH; i++)
      recursive_free(alloc[i], level-1);
    my_free((void *)alloc, MYF(0));
  }
  else
    my_free(alloc[-1], MYF(0));
}

void lf_dynarray_end(LF_DYNARRAY *array)
{
  int i;
  for (i=0; i < LF_DYNARRAY_LEVELS; i++)
    recursive_free(array->level[i], i);
  my_atomic_rwlock_destroy(&array->lock);
  bzero(array, sizeof(*array));
}

static const int dynarray_idxes_in_level[LF_DYNARRAY_LEVELS]=
{
  0, /* +1 here to to avoid -1's below */
  LF_DYNARRAY_LEVEL_LENGTH,
  LF_DYNARRAY_LEVEL_LENGTH * LF_DYNARRAY_LEVEL_LENGTH,
  LF_DYNARRAY_LEVEL_LENGTH * LF_DYNARRAY_LEVEL_LENGTH *
    LF_DYNARRAY_LEVEL_LENGTH
};

void *_lf_dynarray_lvalue(LF_DYNARRAY *array, uint idx)
{
  void * ptr, * volatile * ptr_ptr=0;
  int i;

  for (i=3; i > 0; i--)
  {
    if (ptr_ptr || idx >= dynarray_idxes_in_level[i])
    {
      if (!ptr_ptr)
      {
        ptr_ptr=&array->level[i];
        idx-= dynarray_idxes_in_level[i];
      }
      ptr=*ptr_ptr;
      if (!ptr)
      {
        void *alloc=my_malloc(LF_DYNARRAY_LEVEL_LENGTH * sizeof(void *),
                              MYF(MY_WME|MY_ZEROFILL));
        if (!alloc)
          return(NULL);
        if (my_atomic_casptr(ptr_ptr, &ptr, alloc))
          ptr= alloc;
        else
          my_free(alloc, MYF(0));
      }
      ptr_ptr=((void **)ptr) + idx / dynarray_idxes_in_level[i];
      idx%= dynarray_idxes_in_level[i];
    }
  }
  if (!ptr_ptr)
    ptr_ptr=&array->level[0];
  ptr=*ptr_ptr;
  if (!ptr)
  {
    void *alloc, *data;
    alloc=my_malloc(LF_DYNARRAY_LEVEL_LENGTH * array->size_of_element +
                    max(array->size_of_element, sizeof(void *)),
                    MYF(MY_WME|MY_ZEROFILL));
    if (!alloc)
      return(NULL);
    /* reserve the space for free() address */
    data= alloc + sizeof(void *);
    { /* alignment */
      intptr mod= ((intptr)data) % array->size_of_element;
      if (mod)
        data+= array->size_of_element - mod;
    }
    ((void **)data)[-1]=alloc; /* free() will need the original pointer */
    if (my_atomic_casptr(ptr_ptr, &ptr, data))
      ptr= data;
    else
      my_free(alloc, MYF(0));
  }
  return ptr + array->size_of_element * idx;
}

void *_lf_dynarray_value(LF_DYNARRAY *array, uint idx)
{
  void * ptr, * volatile * ptr_ptr=0;
  int i;

  for (i=3; i > 0; i--)
  {
    if (ptr_ptr || idx >= dynarray_idxes_in_level[i])
    {
      if (!ptr_ptr)
      {
        ptr_ptr=&array->level[i];
        idx-= dynarray_idxes_in_level[i];
      }
      ptr=*ptr_ptr;
      if (!ptr)
        return(NULL);
      ptr_ptr=((void **)ptr) + idx / dynarray_idxes_in_level[i];
      idx %= dynarray_idxes_in_level[i];
    }
  }
  if (!ptr_ptr)
    ptr_ptr=&array->level[0];
  ptr=*ptr_ptr;
  if (!ptr)
    return(NULL);
  return ptr + array->size_of_element * idx;
}

static int recursive_iterate(LF_DYNARRAY *array, void *ptr, int level,
                             lf_dynarray_func func, void *arg)
{
  int res, i;
  if (!ptr)
    return 0;
  if (!level)
    return func(ptr, arg);
  for (i=0; i < LF_DYNARRAY_LEVEL_LENGTH; i++)
    if ((res=recursive_iterate(array, ((void **)ptr)[i], level-1, func, arg)))
      return res;
  return 0;
}

int _lf_dynarray_iterate(LF_DYNARRAY *array, lf_dynarray_func func, void *arg)
{
  int i, res;
  for (i=0; i < LF_DYNARRAY_LEVELS; i++)
    if ((res=recursive_iterate(array, array->level[i], i, func, arg)))
      return res;
  return 0;
}

