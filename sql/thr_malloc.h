/* Copyright (C) 2009 Sun Microsystems, Inc.

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

#ifndef SQL_THR_MALLOC_INCLUDED
#define SQL_THR_MALLOC_INCLUDED

#include <my_global.h>
#include <my_alloc.h>
#include <m_ctype.h>

void init_sql_alloc(MEM_ROOT *root, uint block_size, uint pre_alloc_size);
void *sql_alloc(size_t);
void *sql_calloc(size_t);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str, size_t len);
void *sql_memdup(const void * ptr, size_t size);
char *sql_strmake_with_convert(const char *str, size_t arg_length,
			       CHARSET_INFO *from_cs,
			       size_t max_res_length,
			       CHARSET_INFO *to_cs, size_t *result_length);

#endif  // SQL_THR_MALLOC_INCLUDED
