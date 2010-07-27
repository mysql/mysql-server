/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>

/**
  Allocate a sized block of memory.

  @param size   The size of the memory block in bytes.
  @param flags  Failure action modifiers (bitmasks).

  @return A pointer to the allocated memory block, or NULL on failure.
*/
void *my_malloc(size_t size, myf my_flags)
{
  void* point;
  DBUG_ENTER("my_malloc");
  DBUG_PRINT("my",("size: %lu  my_flags: %d", (ulong) size, my_flags));

  /* Safety */
  if (!size)
    size=1;

  point= malloc(size);
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
    bzero(point, size);
  DBUG_PRINT("exit",("ptr: %p", point));
  DBUG_RETURN(point);
}


/**
   @brief wrapper around realloc()

   @param  oldpoint        pointer to currently allocated area
   @param  size            new size requested, must be >0
   @param  my_flags        flags

   @note if size==0 realloc() may return NULL; my_realloc() treats this as an
   error which is not the intention of realloc()
*/
void *my_realloc(void *oldpoint, size_t size, myf my_flags)
{
  void *point;
  DBUG_ENTER("my_realloc");
  DBUG_PRINT("my",("ptr: %p  size: %lu  my_flags: %d", oldpoint,
                   (ulong) size, my_flags));

  DBUG_ASSERT(size > 0);
  if (!oldpoint && (my_flags & MY_ALLOW_ZERO_PTR))
    DBUG_RETURN(my_malloc(size, my_flags));
#ifdef USE_HALLOC
  if (!(point = malloc(size)))
  {
    if (my_flags & MY_FREE_ON_ERROR)
      my_free(oldpoint);
    if (my_flags & MY_HOLD_ON_ERROR)
      DBUG_RETURN(oldpoint);
    my_errno=errno;
    if (my_flags & MY_FAE+MY_WME)
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG),size);
  }
  else
  {
    memcpy(point,oldpoint,size);
    free(oldpoint);
  }
#else
  if ((point= realloc(oldpoint, size)) == NULL)
  {
    if (my_flags & MY_FREE_ON_ERROR)
      my_free(oldpoint);
    if (my_flags & MY_HOLD_ON_ERROR)
      DBUG_RETURN(oldpoint);
    my_errno=errno;
    if (my_flags & (MY_FAE+MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG), size);
  }
#endif
  DBUG_PRINT("exit",("ptr: %p", point));
  DBUG_RETURN(point);
}


/**
  Free memory allocated with my_malloc.

  @remark Relies on free being able to handle a NULL argument.

  @param ptr Pointer to the memory allocated by my_malloc.
*/
void my_free(void *ptr)
{
  DBUG_ENTER("my_free");
  DBUG_PRINT("my",("ptr: %p", ptr));
  free(ptr);
  DBUG_VOID_RETURN;
}


void *my_memdup(const void *from, size_t length, myf my_flags)
{
  void *ptr;
  if ((ptr= my_malloc(length,my_flags)) != 0)
    memcpy(ptr, from, length);
  return ptr;
}


char *my_strdup(const char *from, myf my_flags)
{
  char *ptr;
  size_t length= strlen(from)+1;
  if ((ptr= (char*) my_malloc(length, my_flags)))
    memcpy(ptr, from, length);
  return ptr;
}


char *my_strndup(const char *from, size_t length, myf my_flags)
{
  char *ptr;
  if ((ptr= (char*) my_malloc(length+1, my_flags)))
  {
    memcpy(ptr, from, length);
    ptr[length]= 0;
  }
  return ptr;
}

