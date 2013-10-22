/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef GCS_COMMIT_VALIDATION_INCLUDED
#define GCS_COMMIT_VALIDATION_INCLUDED

#include <map>

/* Maps a condition variable to it's thread_id. */
typedef std::map<my_thread_id, std::pair<mysql_cond_t*, mysql_mutex_t*> > cond_map;

extern mysql_mutex_t LOCK_cond_map;
int init_cond_mutex(mysql_cond_t* condition, mysql_mutex_t* mutex);
void destroy_cond_mutex(mysql_cond_t* condition, mysql_mutex_t* mutex);
void init_validation_structures(void);
/*
  This method adds a pair of condition variable and mysql mutex to the
  map when the thread executing a transaction on the server has to wait
  for the certification result.

  @param[in] thread_id   The key of the map.
  @param[in]  cond       The condition variable corresponding to the thread.
  @param[in]  mutex      The mutex variable corresponding to the thread.

  @retval  0  if the addition of the condition variable was successful
          !=0 if there was a failure during the addition to the map.

*/
int add_transaction_wait_cond(my_thread_id thread_id, mysql_cond_t* cond,
                              mysql_mutex_t* mutex);

/*
  This method fetches a pair of condition variable and mutex from the
  map when the certification is done and the sleeping thread on the
  server is to be awakened.

  @param[in] thread_id   The key of the map

  @retval returns std::pair of mysql_cond_t and mysql_mutex_t pointer to
          corresponding to the given thread id.

*/
std::pair <mysql_cond_t*, mysql_mutex_t*>
get_transaction_wait_cond(my_thread_id thread_id);

/*
  This method deletes a condition variable and mutex pair and thread id pair from the
  condition variable map once the sleeping thread is awakened.

  @param[in] thread_id   The key of the map
*/
void delete_transaction_wait_cond(my_thread_id thread_id);

#endif
