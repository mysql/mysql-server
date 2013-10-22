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

#include "my_global.h"
#include <gcs_replication.h>
#include <mysqld.h>
#include <log.h>
#include "gcs_commit_validation.h"
#include "gcs_plugin_utils.h"

/* A Map that maps, a thread_id to a condition_variable. */
cond_map cond_to_thd_id_map;
/* An iterator to traverse the map. */
cond_map::iterator cond_iterator;

mysql_mutex_t LOCK_cond_map;
PSI_mutex_key key_LOCK_cond_map;

PSI_cond_key key_COND_certify_wait;
static PSI_cond_info validation_conditions[]=
{
  { &key_COND_certify_wait, "COND_certify_wait", 0}
};

PSI_mutex_key key_LOCK_certify_wait;
static PSI_mutex_info validation_mutexes[]=
{
  { &key_LOCK_certify_wait, "LOCK_certify_wait", 0},
};

void init_validation_structures(void)
{
  register_gcs_psi_keys(validation_mutexes, validation_conditions);
  mysql_mutex_init(key_LOCK_cond_map, &LOCK_cond_map, MY_MUTEX_INIT_FAST);
}

int init_cond_mutex(mysql_cond_t* condition, mysql_mutex_t* mutex)
{
  int error= 0;
  if ((error= mysql_mutex_init(key_LOCK_certify_wait, mutex, MY_MUTEX_INIT_FAST)))
    return 1;
  if ((error= mysql_cond_init(key_COND_certify_wait, condition, NULL)))
    return 1;

  return error;
}

void destroy_cond_mutex(mysql_cond_t* condition, mysql_mutex_t* mutex)
{
  if (mutex)
    mysql_mutex_destroy(mutex);
  if (condition)
    mysql_cond_destroy(condition);
}

int add_transaction_wait_cond(my_thread_id thread_id, mysql_cond_t* cond,
                              mysql_mutex_t* mutex)
{
  DBUG_ENTER("add_transaction_condition");

  cond_map::const_iterator iterator;

  mysql_mutex_lock(&LOCK_cond_map);
  std::pair <mysql_cond_t*, mysql_mutex_t*> temp= std::make_pair (cond,
                                                                  mutex);
  std::pair<std::map<my_thread_id, std::pair <mysql_cond_t*, mysql_mutex_t*> >::iterator,bool> ret;
  ret= cond_to_thd_id_map.insert(std::pair<my_thread_id, std::pair
                                 <mysql_cond_t*, mysql_mutex_t*> >(thread_id, temp));
  mysql_mutex_unlock(&LOCK_cond_map);

  // If a map insert fails, ret.second is false
  if(ret.second)
    DBUG_RETURN(0);
  DBUG_RETURN(1);
}

std::pair <mysql_cond_t*, mysql_mutex_t*>
get_transaction_wait_cond(my_thread_id thread_id)
{
  DBUG_ENTER("get_transaction_condition");

  DBUG_ASSERT(thread_id > 0);
  cond_map::iterator iterator;

  mysql_mutex_lock(&LOCK_cond_map);
  iterator= cond_to_thd_id_map.find(thread_id);

  if (iterator == cond_to_thd_id_map.end())
  {
    sql_print_error("The map is empty");
    mysql_mutex_unlock(&LOCK_cond_map);

    DBUG_RETURN(std::make_pair((mysql_cond_t*) 0, (mysql_mutex_t*)0));
  }

  mysql_mutex_unlock(&LOCK_cond_map);
  DBUG_RETURN(iterator->second);
}

void delete_transaction_wait_cond(my_thread_id thread_id)
{
  DBUG_ENTER("delete_transaction_condition");

  mysql_mutex_lock(&LOCK_cond_map);
  cond_map::iterator iterator;
  iterator= cond_to_thd_id_map.find(thread_id);
  if (iterator == cond_to_thd_id_map.end())
  {
    sql_print_error("The map does not contain the given manish thread id");
    mysql_mutex_unlock(&LOCK_cond_map);
    DBUG_VOID_RETURN;
  }
  cond_to_thd_id_map.erase(iterator);
  mysql_mutex_unlock(&LOCK_cond_map);

  DBUG_VOID_RETURN;
}
