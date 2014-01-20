/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief Contains wrapper functions for memory allocation and deallocation.
  This includes generic functions to be called from the binlogapi library,
  which call the appropriate corresponding function, depending on whether
  the library is compiled independently, or with the MySQL server.
*/

#ifndef WRAPPER_FUNCTIONS_H
#define WRAPPER_FUNCTIONS_H

#include "config.h"

#if HAVE_MYSYS
#include "my_sys.h"
extern PSI_memory_key key_memory_Incident_log_event_message;
extern PSI_memory_key key_memory_Rows_query_log_event_rows_query;
extern PSI_memory_key key_memory_log_event;

/**
  This enum will be sued by the wrapper method bapi_malloc,
  to pass it to the method my_malloc, as we can not have a parameter of
  type PSI_memory_key in the method bapi_malloc.
*/
enum PSI_memory_key_to_int
{
  MEMORY_LOG_EVENT,
  ROWS_QUERY_LOG_EVENT_ROWS_QUERY,
  INCIDENT_LOG_EVENT_MESSAGE
};

#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <cstring>
#endif

/**
  This is a wrapper function, and returns a pointer to a new string which is
  a duplicate of the input string. The terminating Null character is added.

  If compiled with MySQL server,the strndup function from the mysys library is
  called, which allow instrumenting memory allocated. Else, the standard
  string function is called.

  @param destination The string to be duplicated
  @param n The number of bytes to be copied

  @return The duplicated string, or NULL if insufficient memory was available.
*/
inline const char* bapi_strndup(const char *destination, size_t n)
{
#if HAVE_MYSYS
/* Call the function in mysys library, required for memory instrumentation */
  return my_strndup(key_memory_log_event, destination, n, MYF(MY_WME));
#else
  return strndup(destination, n);
#endif
}

/**
  This is a wrapper function, and returns a pointer to a new memory with the
  contents copied from the input memory pointer, upto a given length

  @param source Pointer to the buffer from which data is to be copied
  @param len Length upto which the source should be copied

  @return dest pointer to a new memory if allocation was successful
          NULL otherwise
*/
inline const void* bapi_memdup(const void* source, size_t len, int flags= 16)
{
  const void* dest;
#if HAVE_MYSYS
  /* Call the function in mysys library, required for memory instrumentation */
  if (flags == 16)
    dest= my_memdup(key_memory_log_event, source, len, MYF(MY_WME));
  else
    dest= my_memdup(key_memory_log_event, source, len, MYF(0));
#else
  dest= malloc(len);
  if (dest)
    memcpy(dest, source, len);
#endif
  return dest;
}
/**
  This is a wrapper function inorder to  allocate memory from the heap
  in the binlogapi library.

  If compiled with the MySQL server, and memory is allocated using memory
  allocating methods from the mysys library, my_malloc is called. Otherwise,
  the standard malloc() is called from the function.

  @param size         Size of the memory to be allocated.
  @param key_to_int   A mapping from the PSI_memory_key to an enum
  @flags              flags to be passed to my_malloc
  @return Void pointer to the allocated chunk of memory
*/
inline void * bapi_malloc(size_t size, enum PSI_memory_key_to_int key_to_int=
                          MEMORY_LOG_EVENT, int flags= 0)
{
  void * dest= NULL;
  #if HAVE_MYSYS
  if (key_to_int == ROWS_QUERY_LOG_EVENT_ROWS_QUERY)
    dest= my_malloc(key_memory_Rows_query_log_event_rows_query,size, MYF(flags));
  else if (key_to_int == INCIDENT_LOG_EVENT_MESSAGE)
    dest= my_malloc(key_memory_Incident_log_event_message,size, MYF(flags));
  else
    dest= my_malloc(key_memory_log_event,size, MYF(flags));
  #else
    dest=  malloc(size);
  #endif
  return dest;
}
/**
  This is a wrapper function inorder to free the memory allocated from the heap
  in the binlogapi library.

  If compiled with the MySQL server, and memory is allocated using memory
  allocating methods from the mysys library, my_free is called. Otherwise,
  the standard free() is called from the function.

  @param Pointer to the memory which is to be freed.
*/
inline void bapi_free(void* ptr)
{
#if HAVE_MYSYS
  return my_free(ptr);
#else
  return free(ptr);
#endif
  ptr= 0;
}

#endif


