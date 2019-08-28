/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key hp_key_mutex_HP_SHARE_intern_lock;

static PSI_mutex_info all_heap_mutexes[]=
{
  { & hp_key_mutex_HP_SHARE_intern_lock, "HP_SHARE::intern_lock", 0}
  /*
    Note:
    THR_LOCK_heap is part of mysys, not storage/heap.
  */
};

void init_heap_psi_keys()
{
  const char* category= "memory";
  int count;

  count= array_elements(all_heap_mutexes);
  mysql_mutex_register(category, all_heap_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */


