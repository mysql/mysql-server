/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Alloc a block of locked memory */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <my_list.h>

#ifdef HAVE_MLOCK
#include <sys/mman.h>

struct st_mem_list
{
  LIST list;
  byte *page;
  uint size;
};

LIST *mem_list;

byte *my_malloc_lock(uint size,myf MyFlags)
{
  int success;
  uint pagesize=sysconf(_SC_PAGESIZE);
  byte *ptr;
  struct st_mem_list *element;
  DBUG_ENTER("my_malloc_lock");

  size=((size-1) & ~(pagesize-1))+pagesize;
  if (!(ptr=memalign(pagesize,size)))
  {
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG),size);
    DBUG_RETURN(0);
  }
  success = mlock((byte*) ptr,size);
  if (success != 0 && geteuid() == 0)
  {
    DBUG_PRINT("warning",("Failed to lock memory. errno %d\n",
			  errno));
    fprintf(stderr, "Warning: Failed to lock memory. errno %d\n",
	    errno);
  }
  else
  {
    /* Add block in a list for munlock */
    if (!(element=(struct st_mem_list*) my_malloc(sizeof(*element),MyFlags)))
    {
      VOID(munlock((byte*) ptr,size));
      free(ptr);
      DBUG_RETURN(0);
    }
    element->list.data=(byte*) element;
    element->page=ptr;
    element->size=size;
    pthread_mutex_lock(&THR_LOCK_malloc);
    mem_list=list_add(mem_list,&element->list);
    pthread_mutex_unlock(&THR_LOCK_malloc);
  }
  DBUG_RETURN(ptr);
}


void my_free_lock(byte *ptr,myf Myflags __attribute__((unused)))
{
  LIST *list;
  struct st_mem_list *element=0;

  pthread_mutex_lock(&THR_LOCK_malloc);
  for (list=mem_list ; list ; list=list->next)
  {
    element=(struct st_mem_list*) list->data;
    if (ptr == element->page)
    {						/* Found locked mem */
      VOID(munlock((byte*) ptr,element->size));
      mem_list=list_delete(mem_list,list);
      break;
    }
  }
  pthread_mutex_unlock(&THR_LOCK_malloc);
  if (element)
    my_free((gptr) element,MYF(0));
  free(ptr);					/* Free even if not locked */
}

#endif /* HAVE_MLOCK */
