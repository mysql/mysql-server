/* Copyright (c) 2012, 2013 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_MYSQL_ALLOC_INCLUDED
#define MYSQL_SERVICE_MYSQL_ALLOC_INCLUDED

#ifndef MYSQL_ABI_CHECK
#include <stdlib.h>
#endif

/* PSI_memory_key */
#include "mysql/psi/psi_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* myf */
typedef int myf_t;

typedef void * (*mysql_malloc_t)(PSI_memory_key key, size_t size, myf_t flags);
typedef void * (*mysql_realloc_t)(PSI_memory_key key, void *ptr, size_t size, myf_t flags);
typedef void (*mysql_free_t)(void *ptr);
typedef void * (*my_memdup_t)(PSI_memory_key key, const void *from, size_t length, myf_t flags);
typedef char * (*my_strdup_t)(PSI_memory_key key, const char *from, myf_t flags);
typedef char * (*my_strndup_t)(PSI_memory_key key, const char *from, size_t length, myf_t flags);

struct mysql_malloc_service_st
{
  mysql_malloc_t mysql_malloc;
  mysql_realloc_t mysql_realloc;
  mysql_free_t mysql_free;
  my_memdup_t my_memdup;
  my_strdup_t my_strdup;
  my_strndup_t my_strndup;
};

extern struct mysql_malloc_service_st *mysql_malloc_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_malloc mysql_malloc_service->mysql_malloc
#define my_realloc mysql_malloc_service->mysql_realloc
#define my_free mysql_malloc_service->mysql_free
#define my_memdup mysql_malloc_service->my_memdup
#define my_strdup mysql_malloc_service->my_strdup
#define my_strndup mysql_malloc_service->my_strndup

#else

extern void * my_malloc(PSI_memory_key key, size_t size, myf_t flags);
extern void * my_realloc(PSI_memory_key key, void *ptr, size_t size, myf_t flags);
extern void my_free(void *ptr);
extern void * my_memdup(PSI_memory_key key, const void *from, size_t length, myf_t flags);
extern char * my_strdup(PSI_memory_key key, const char *from, myf_t flags);
extern char * my_strndup(PSI_memory_key key, const char *from, size_t length, myf_t flags);

#endif

#ifdef __cplusplus
}
#endif

#endif

