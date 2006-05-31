/* Copyright (C) 2006 MySQL AB

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

#ifndef atomic_rwlock_init

#ifdef MY_ATOMIC_EXTRA_DEBUG
#ifndef MY_ATOMIC_MODE_RWLOCKS
#error MY_ATOMIC_EXTRA_DEBUG can be only used with MY_ATOMIC_MODE_RWLOCKS
#endif
#define LOCK_PTR void *rw;
#else
#define LOCK_PTR
#endif

typedef volatile struct {uint8   val; LOCK_PTR} my_atomic_8_t;
typedef volatile struct {uint16  val; LOCK_PTR} my_atomic_16_t;
typedef volatile struct {uint32  val; LOCK_PTR} my_atomic_32_t;
typedef volatile struct {uint64  val; LOCK_PTR} my_atomic_64_t;

#ifndef MY_ATOMIC_MODE_RWLOCKS
#include "atomic/nolock.h"
#endif

#ifndef my_atomic_rwlock_init
#include "atomic/rwlock.h"
#endif

#define MY_ATOMIC_OK       0
#define MY_ATOMIC_NOT_1CPU 1
extern int my_atomic_initialize();

#endif

