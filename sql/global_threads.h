/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef GLOBAL_THREADS_INCLUDED
#define GLOBAL_THREADS_INCLUDED

/*
  TODO: Make a proper interface for keeping track of global threads.
 */
#include "sql_list.h"
#include "sql_class.h"

extern I_List<THD> threads;
extern uint volatile thread_count;

#endif  // GLOBAL_THREADS_INCLUDED
