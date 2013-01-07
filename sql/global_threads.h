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

#include <my_global.h>
#include <my_pthread.h>
#include <set>

class THD;

extern mysql_mutex_t LOCK_thread_count;
extern mysql_mutex_t LOCK_thread_remove;
extern mysql_cond_t COND_thread_count;

/**
  We maintail a set of all registered threads.
  We provide acccessors to iterate over all threads.
  There is no guarantee on the order of THDs when iterating.

  We also provide mutators for inserting, and removing an element:
  add_global_thread() inserts a THD into the set, and increments the counter.
  remove_global_thread() removes a THD from the set, and decrements the counter.
  remove_global_thread() also broadcasts COND_thread_count.

  All functions must be called with LOCK_thread_count.
 */
typedef std::set<THD*>::iterator Thread_iterator;
Thread_iterator global_thread_list_begin();
Thread_iterator global_thread_list_end();
void copy_global_thread_list(std::set<THD*> *new_copy);
void add_global_thread(THD *);
void remove_global_thread(THD *);

/*
  We maintain a separate counter for the number of threads,
  which can be accessed without LOCK_thread_count.
  An un-locked read, means that the result is fuzzy of course.
  This accessor is used by DBUG printing, by signal handlers,
  and by the 'mysqladmin status' command.
*/
uint get_thread_count();

#endif  // GLOBAL_THREADS_INCLUDED
