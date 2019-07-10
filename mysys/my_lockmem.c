/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Alloc a block of locked memory */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <my_list.h>

#ifdef HAVE_MLOCK
#include <sys/mman.h>

struct st_mem_list
{
  LIST list;
  uchar *page;
  uint size;
};

LIST *mem_list;

uchar *my_malloc_lock(uint size,myf MyFlags)
{
  int success;
  uint pagesize=sysconf(_SC_PAGESIZE);
  uchar *ptr;
  struct st_mem_list *element;
  DBUG_ENTER("my_malloc_lock");

  size=((size-1) & ~(pagesize-1))+pagesize;
  if (!(ptr=memalign(pagesize,size)))
  {
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG+ME_FATALERROR), size);
    DBUG_RETURN(0);
  }
  success = mlock((uchar*) ptr,size);
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
      (void) munlock((uchar*) ptr,size);
      free(ptr);
      DBUG_RETURN(0);
    }
    element->list.data=(uchar*) element;
    element->page=ptr;
    element->size=size;
    mysql_mutex_lock(&THR_LOCK_malloc);
    mem_list=list_add(mem_list,&element->list);
    mysql_mutex_unlock(&THR_LOCK_malloc);
  }
  DBUG_RETURN(ptr);
}


void my_free_lock(uchar *ptr)
{
  LIST *list;
  struct st_mem_list *element=0;

  mysql_mutex_lock(&THR_LOCK_malloc);
  for (list=mem_list ; list ; list=list->next)
  {
    element=(struct st_mem_list*) list->data;
    if (ptr == element->page)
    {						/* Found locked mem */
      (void) munlock((uchar*) ptr,element->size);
      mem_list=list_delete(mem_list,list);
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_malloc);
  my_free(element);
  free(ptr);					/* Free even if not locked */
}

#endif /* HAVE_MLOCK */
