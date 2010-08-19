/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef SAFEMALLOC			/* We don't need SAFEMALLOC here */
#undef SAFEMALLOC
#endif

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>

	/* My memory allocator */

void *my_malloc(size_t size, myf my_flags)
{
  void* point;
  DBUG_ENTER("my_malloc");
  DBUG_PRINT("my",("size: %lu  my_flags: %d", (ulong) size, my_flags));

  if (!size)
    size=1;					/* Safety */

  point= (char *) malloc(size);
  DBUG_EXECUTE_IF("simulate_out_of_memory",
                  {
                    free(point);
                    point= NULL;
                  });

  if (point == NULL)
  {
    my_errno=errno;
    if (my_flags & MY_FAE)
      error_handler_hook=fatal_error_handler_hook;
    if (my_flags & (MY_FAE+MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG+ME_NOREFRESH),size);
    DBUG_EXECUTE_IF("simulate_out_of_memory",
                    DBUG_SET("-d,simulate_out_of_memory"););
    if (my_flags & MY_FAE)
      exit(1);
  }
  else if (my_flags & MY_ZEROFILL)
    bzero(point,size);
  DBUG_PRINT("exit",("ptr: 0x%lx", (long) point));
  DBUG_RETURN((void*) point);
} /* my_malloc */


	/* Free memory allocated with my_malloc */
	/*ARGSUSED*/

void my_no_flags_free(void* ptr)
{
  DBUG_ENTER("my_free");
  DBUG_PRINT("my",("ptr: 0x%lx", (long) ptr));
  if (ptr)
    free(ptr);
  DBUG_VOID_RETURN;
} /* my_free */


	/* malloc and copy */

void* my_memdup(const void *from, size_t length, myf my_flags)
{
  void *ptr;
  if ((ptr= my_malloc(length,my_flags)) != 0)
    memcpy(ptr, from, length);
  return(ptr);
}


char *my_strdup(const char *from, myf my_flags)
{
  char *ptr;
  size_t length= strlen(from)+1;
  if ((ptr= (char*) my_malloc(length, my_flags)))
    memcpy((uchar*) ptr, (uchar*) from,(size_t) length);
  return(ptr);
}


char *my_strndup(const char *from, size_t length, myf my_flags)
{
  char *ptr;
  if ((ptr= (char*) my_malloc(length+1,my_flags)) != 0)
  {
    memcpy((uchar*) ptr, (uchar*) from, length);
    ptr[length]=0;
  }
  return((char*) ptr);
}
