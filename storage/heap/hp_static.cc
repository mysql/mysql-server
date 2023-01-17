/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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
  Static variables for heap library. All defined here for easy making of
  a shared library
*/

#ifndef MY_GLOBAL_INCLUDED
#include "storage/heap/heapdef.h"
#endif
#include "my_macros.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_memory.h"

LIST *heap_open_list = nullptr, *heap_share_list = nullptr;

PSI_memory_key hp_key_memory_HP_SHARE;
PSI_memory_key hp_key_memory_HP_INFO;
PSI_memory_key hp_key_memory_HP_PTRS;
PSI_memory_key hp_key_memory_HP_KEYDEF;

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_info all_heap_memory[] = {
    {&hp_key_memory_HP_SHARE, "HP_SHARE", 0, 0, PSI_DOCUMENT_ME},
    {&hp_key_memory_HP_INFO, "HP_INFO", 0, 0, PSI_DOCUMENT_ME},
    {&hp_key_memory_HP_PTRS, "HP_PTRS", 0, 0, PSI_DOCUMENT_ME},
    {&hp_key_memory_HP_KEYDEF, "HP_KEYDEF", 0, 0, PSI_DOCUMENT_ME}};

void init_heap_psi_keys() {
  const char *category = "memory";
  int count;

  /* Note: THR_LOCK_heap is part of mysys, not storage/heap. */

  count = array_elements(all_heap_memory);
  mysql_memory_register(category, all_heap_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */
