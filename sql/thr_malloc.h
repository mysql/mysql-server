/* Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef THR_MALLOC_INCLUDED
#define THR_MALLOC_INCLUDED

#include <stddef.h>

typedef struct charset_info_st CHARSET_INFO;
typedef struct st_mem_root MEM_ROOT;
typedef unsigned int PSI_memory_key;

void init_sql_alloc(PSI_memory_key key,
                    MEM_ROOT *root, size_t block_size, size_t pre_alloc_size);

void *sql_alloc(size_t);
void *sql_calloc(size_t);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str, size_t len);
void *sql_memdup(const void * ptr, size_t size);
char *sql_strmake_with_convert(const char *str, size_t arg_length,
			       const CHARSET_INFO *from_cs,
			       size_t max_res_length,
			       const CHARSET_INFO *to_cs, size_t *result_length);

#endif /* THR_MALLOC_INCLUDED */
