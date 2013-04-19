/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* Not MT-SAFE */

#include "mysys_priv.h"
#include "my_static.h"
#include "mysys_err.h"
#include <m_string.h>

/*
  Alloc for things we don't nend to free run-time (that only
  should be free'd on exit)

  SYNOPSIS
    my_once_alloc()
      Size
      MyFlags

  NOTES
    No DBUG_ENTER... here to get smaller dbug-startup 
*/

void* my_once_alloc(size_t Size, myf MyFlags)
{
  size_t get_size, max_left;
  uchar* point;
  reg1 USED_MEM *next;
  reg2 USED_MEM **prev;

  Size= ALIGN_SIZE(Size);
  prev= &my_once_root_block;
  max_left=0;
  for (next=my_once_root_block ; next && next->left < Size ; next= next->next)
  {
    if (next->left > max_left)
      max_left=next->left;
    prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    get_size= Size+ALIGN_SIZE(sizeof(USED_MEM));
    if (max_left*4 < my_once_extra && get_size < my_once_extra)
      get_size=my_once_extra;			/* Normal alloc */

    if ((next = (USED_MEM*) malloc(get_size)) == 0)
    {
      my_errno=errno;
      if (MyFlags & (MY_FAE+MY_WME))
	my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG+ME_FATALERROR), get_size);
      return((uchar*) 0);
    }
    DBUG_PRINT("test",("my_once_malloc %lu byte malloced", (ulong) get_size));
    next->next= 0;
    next->size= get_size;
    next->left= get_size-ALIGN_SIZE(sizeof(USED_MEM));
    *prev=next;
  }
  point= (uchar*) ((char*) next+ (next->size-next->left));
  next->left-= Size;

  if (MyFlags & MY_ZEROFILL)
    memset(point, 0, Size);
  return((void*) point);
} /* my_once_alloc */


char *my_once_strdup(const char *src,myf myflags)
{
  size_t len= strlen(src)+1;
  uchar *dst= my_once_alloc(len, myflags);
  if (dst)
    memcpy(dst, src, len);
  return (char*) dst;
}


void *my_once_memdup(const void *src, size_t len, myf myflags)
{
  uchar *dst= my_once_alloc(len, myflags);
  if (dst)
    memcpy(dst, src, len);
  return dst;
}


/*
  Deallocate everything that was allocated with my_once_alloc

  SYNOPSIS
    my_once_free()
*/

void my_once_free(void)
{
  reg1 USED_MEM *next,*old;
  DBUG_ENTER("my_once_free");

  for (next=my_once_root_block ; next ; )
  {
    old=next; next= next->next ;
    free((uchar*) old);
  }
  my_once_root_block=0;

  DBUG_VOID_RETURN;
} /* my_once_free */
