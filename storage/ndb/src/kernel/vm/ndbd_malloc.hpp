/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBD_MALLOC_H
#define NDBD_MALLOC_H

#include <stddef.h>

#include "ndb_types.h"

#define JAM_FILE_ID 234

/**
 * common memory allocation function for ndbd kernel
 */
void *ndbd_malloc(size_t size);
bool ndbd_malloc_need_watchdog(size_t size);
void *ndbd_malloc_watched(size_t size, volatile Uint32* watch_dog);
void ndbd_free(void *p, size_t size);
void ndbd_alloc_touch_mem(void * p, size_t sz, volatile Uint32 * watchCounter, bool make_readwritable);


#undef JAM_FILE_ID

#endif 
