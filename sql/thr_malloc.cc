/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/* Mallocs for used in threads */

#include "mysql_priv.h"

extern "C" {
  void sql_alloc_error_handler(void)
  {
    current_thd->fatal_error=1; /* purecov: inspected */
    sql_print_error(ER(ER_OUT_OF_RESOURCES));
  }
}

void init_sql_alloc(MEM_ROOT *mem_root,uint block_size)
{
  init_alloc_root(mem_root,block_size);
  mem_root->error_handler=sql_alloc_error_handler;
}


gptr sql_alloc(uint Size)
{
  MEM_ROOT *root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
  char *ptr= (char*) alloc_root(root,Size);
  return ptr;
}


gptr sql_calloc(uint size)
{
  gptr ptr;
  if ((ptr=sql_alloc(size)))
    bzero((char*) ptr,size);
  return ptr;
}


char *sql_strdup(const char *str)
{
  uint len=(uint) strlen(str)+1;
  char *pos;
  if ((pos= (char*) sql_alloc(len)))
    memcpy(pos,str,len);
  return pos;
}


char *sql_strmake(const char *str,uint len)
{
  char *pos;
  if ((pos= (char*) sql_alloc(len+1)))
  {
    memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}


gptr sql_memdup(const void *ptr,uint len)
{
  char *pos;
  if ((pos= (char*) sql_alloc(len)))
    memcpy(pos,ptr,len);
  return pos;
}

void sql_element_free(void *ptr __attribute__((unused)))
{} /* purecov: deadcode */
