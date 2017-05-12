/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "table_cache.h"

#include <stdio.h>
#include <string.h>

#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql_test.h" // lock_descriptions[]
#include "template_utils.h"
#include "thr_lock.h"
#include "thr_mutex.h"


/**
  Container for all table cache instances in the system.
*/
Table_cache_manager table_cache_manager;


#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key Table_cache::m_lock_key;
PSI_mutex_info Table_cache::m_mutex_keys[]= {
  { &m_lock_key, "LOCK_table_cache", 0, 0}
};
#endif


static const uchar *table_cache_key(const uchar *record,
                                    size_t *length)
{
  TABLE_SHARE *share= ((Table_cache_element*)record)->get_share();
  *length= share->table_cache_key.length;
  return (uchar*) share->table_cache_key.str;
}


static void table_cache_free_entry(void *arg)
{
  Table_cache_element *element= pointer_cast<Table_cache_element*>(arg);
  delete element;
}


/**
  Initialize instance of table cache.

  @retval false - success.
  @retval true  - failure.
*/

bool Table_cache::init()
{
  mysql_mutex_init(m_lock_key, &m_lock, MY_MUTEX_INIT_FAST);
  m_unused_tables= NULL;
  m_table_count= 0;

  if (my_hash_init(&m_cache, &my_charset_bin,
                   table_cache_size_per_instance, 0,
                   table_cache_key, table_cache_free_entry,
                   0,
                   PSI_INSTRUMENT_ME))
  {
    mysql_mutex_destroy(&m_lock);
    return true;
  }
  return false;
}


/** Destroy instance of table cache. */

void Table_cache::destroy()
{
  my_hash_free(&m_cache);
  mysql_mutex_destroy(&m_lock);
}


/** Init P_S instrumentation key for mutex protecting Table_cache instance. */

void Table_cache::init_psi_keys()
{
#ifdef HAVE_PSI_INTERFACE
  mysql_mutex_register("sql", m_mutex_keys,
                       static_cast<int>(array_elements(m_mutex_keys)));
#endif
}


#ifdef EXTRA_DEBUG
void Table_cache::check_unused()
{
  uint count= 0;

  if (m_unused_tables != NULL)
  {
    TABLE *cur_link= m_unused_tables;
    TABLE *start_link= m_unused_tables;
    do
    {
      if (cur_link != cur_link->next->prev || cur_link != cur_link->prev->next)
      {
	DBUG_PRINT("error",("Unused_links aren't linked properly"));
	return;
      }
    } while (count++ < m_table_count &&
	     (cur_link= cur_link->next) != start_link);
    if (cur_link != start_link)
      DBUG_PRINT("error",("Unused_links aren't connected"));
  }

  for (uint idx= 0; idx < m_cache.records; idx++)
  {
    Table_cache_element *el=
      (Table_cache_element*) my_hash_element(&m_cache, idx);

    Table_cache_element::TABLE_list::Iterator it(el->free_tables);
    TABLE *entry;
    while ((entry= it++))
    {
      /* We must not have TABLEs in the free list that have their file closed. */
      DBUG_ASSERT(entry->db_stat && entry->file);
      /* Merge children should be detached from a merge parent */
      DBUG_ASSERT(! entry->file->extra(HA_EXTRA_IS_ATTACHED_CHILDREN));

      if (entry->in_use)
        DBUG_PRINT("error",("Used table is in share's list of unused tables"));
      count--;
    }
    it.init(el->used_tables);
    while ((entry= it++))
    {
      if (!entry->in_use)
        DBUG_PRINT("error",("Unused table is in share's list of used tables"));
    }
  }

  if (count != 0)
    DBUG_PRINT("error",("Unused_links doesn't match open_cache: diff: %d",
                        count));
}
#endif


/** Free all unused TABLE objects in the table cache. */

void Table_cache::free_all_unused_tables()
{
  assert_owner();

  while (m_unused_tables)
  {
    TABLE *table_to_free= m_unused_tables;
    remove_table(table_to_free);
    intern_close_table(table_to_free);
  }
}


#ifndef DBUG_OFF
/**
  Print debug information for the contents of the table cache.
*/

void Table_cache::print_tables()
{
  uint unused= 0;
  uint count=0;

  static_assert(TL_WRITE_ONLY+1 == array_elements(lock_descriptions), "");

  for (uint idx= 0; idx < m_cache.records; idx++)
  {
    Table_cache_element *el=
      (Table_cache_element*) my_hash_element(&m_cache, idx);

    Table_cache_element::TABLE_list::Iterator it(el->used_tables);
    TABLE *entry;
    while ((entry= it++))
    {
      printf("%-14.14s %-32s%6ld%8u%6d  %s\n",
             entry->s->db.str, entry->s->table_name.str, entry->s->version,
             entry->in_use->thread_id(), entry->db_stat ? 1 : 0,
             lock_descriptions[(int)entry->reginfo.lock_type]);
    }
    it.init(el->free_tables);
    while ((entry= it++))
    {
      unused++;
      printf("%-14.14s %-32s%6ld%8ld%6d  %s\n",
             entry->s->db.str, entry->s->table_name.str, entry->s->version,
             0L, entry->db_stat ? 1 : 0, "Not in use");
    }
  }

  if (m_unused_tables != NULL)
  {
    TABLE *start_link= m_unused_tables;
    TABLE *lnk= m_unused_tables;
    do
    {
      if (lnk != lnk->next->prev || lnk != lnk->prev->next)
      {
	printf("unused_links isn't linked properly\n");
	return;
      }
    } while (count++ < m_table_count && (lnk= lnk->next) != start_link);
    if (lnk != start_link)
      printf("Unused_links aren't connected\n");
  }

  if (count != unused)
    printf("Unused_links (%d) doesn't match table_def_cache: %d\n", count,
           unused);
}
#endif


/**
  Initialize all instances of table cache to be used by server.

  @retval false - success.
  @retval true  - failure.
*/

bool Table_cache_manager::init()
{
  Table_cache::init_psi_keys();
  for (uint i= 0; i < table_cache_instances; i++)
  {
    if (m_table_cache[i].init())
    {
      for (uint j= 0; j < i; j++)
        m_table_cache[i].destroy();
      return true;
    }
  }

  return false;
}


/** Destroy all instances of table cache which were used by server. */

void Table_cache_manager::destroy()
{
  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].destroy();
}


/**
  Get total number of used and unused TABLE objects in all table caches.

  @note Doesn't require acquisition of table cache locks if inexact number
        of tables is acceptable.
*/

uint Table_cache_manager::cached_tables()
{
  uint result= 0;

  for (uint i= 0; i < table_cache_instances; i++)
    result+= m_table_cache[i].cached_tables();

  return result;
}


/**
  Acquire locks on all instances of table cache and table definition
  cache (i.e. LOCK_open).
*/

void Table_cache_manager::lock_all_and_tdc()
{
  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].lock();

  mysql_mutex_lock(&LOCK_open);
}


/**
  Release locks on all instances of table cache and table definition
  cache.
*/

void Table_cache_manager::unlock_all_and_tdc()
{
  mysql_mutex_unlock(&LOCK_open);

  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].unlock();
}


/**
  Assert that caller owns locks on all instances of table cache.
*/

void Table_cache_manager::assert_owner_all()
{
  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].assert_owner();
}


/**
  Assert that caller owns locks on all instances of table cache
  and table definition cache.
*/

void Table_cache_manager::assert_owner_all_and_tdc()
{
  assert_owner_all();

  mysql_mutex_assert_owner(&LOCK_open);
}


/**
   Remove and free all or some (depending on parameter) TABLE objects
   for the table from all table cache instances.

   @param  thd          Thread context
   @param  remove_type  Type of removal. @sa tdc_remove_table().
   @param  share        TABLE_SHARE for the table to be removed.

   @note Caller should own LOCK_open and locks on all table cache
         instances.
*/
void Table_cache_manager::free_table(THD *thd,
                                     enum_tdc_remove_table_type remove_type,
                                     TABLE_SHARE *share)
{
  Table_cache_element *cache_el[MAX_TABLE_CACHES];

  assert_owner_all_and_tdc();

  /*
    Freeing last TABLE instance for the share will destroy the share
    and corresponding TABLE_SHARE::cache_element[] array. To make
    iteration over this array safe, even when share is destroyed in
    the middle of iteration, we create copy of this array on the stack
    and iterate over it.
  */
  memcpy(&cache_el, share->cache_element,
         table_cache_instances * sizeof(Table_cache_element *));

  for (uint i= 0; i < table_cache_instances; i++)
  {
    if (cache_el[i])
    {
      Table_cache_element::TABLE_list::Iterator it(cache_el[i]->free_tables);
      TABLE *table;

#ifndef DBUG_OFF
      if (remove_type == TDC_RT_REMOVE_ALL)
        DBUG_ASSERT(cache_el[i]->used_tables.is_empty());
      else if (remove_type == TDC_RT_REMOVE_NOT_OWN ||
               remove_type == TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE)
      {
        Table_cache_element::TABLE_list::Iterator it2(cache_el[i]->used_tables);
        while ((table= it2++))
        {
          if (table->in_use != thd)
            DBUG_ASSERT(0);
        }
      }
#endif

      while ((table= it++))
      {
        m_table_cache[i].remove_table(table);
        intern_close_table(table);
      }
    }
  }
}


/** Free all unused TABLE objects in all table cache instances. */

void Table_cache_manager::free_all_unused_tables()
{
  assert_owner_all_and_tdc();

  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].free_all_unused_tables();
}


#ifndef DBUG_OFF
/**
  Print debug information for the contents of all table cache instances.
*/

void Table_cache_manager::print_tables()
{
  puts("DB             Table                            Version  Thread  Open  Lock");

  for (uint i= 0; i < table_cache_instances; i++)
    m_table_cache[i].print_tables();
}
#endif

