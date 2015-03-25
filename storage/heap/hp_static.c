/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/*
  Static variables for heap library. All definied here for easy making of
  a shared library
*/

#ifndef MY_GLOBAL_INCLUDED
#include "heapdef.h"
#endif

LIST *heap_open_list=0,*heap_share_list=0;

PSI_memory_key hp_key_memory_HP_SHARE;
PSI_memory_key hp_key_memory_HP_INFO;
PSI_memory_key hp_key_memory_HP_PTRS;
PSI_memory_key hp_key_memory_HP_KEYDEF;

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_info all_heap_memory[]=
{
  { & hp_key_memory_HP_SHARE, "HP_SHARE", 0},
  { & hp_key_memory_HP_INFO, "HP_INFO", 0},
  { & hp_key_memory_HP_PTRS, "HP_PTRS", 0},
  { & hp_key_memory_HP_KEYDEF, "HP_KEYDEF", 0}
};

void init_heap_psi_keys()
{
  const char* category= "memory";
  int count;

  /* Note: THR_LOCK_heap is part of mysys, not storage/heap. */

  count= array_elements(all_heap_memory);
  mysql_memory_register(category, all_heap_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */


