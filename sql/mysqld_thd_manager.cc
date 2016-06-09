/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqld_thd_manager.h"

#include "mysql/thread_pool_priv.h"  // inc_thread_created
#include "mutex_lock.h"              // Mutex_lock
#include "debug_sync.h"              // DEBUG_SYNC_C
#include "sql_class.h"               // THD

#include <functional>
#include <algorithm>

int Global_THD_manager::global_thd_count= 0;
Global_THD_manager *Global_THD_manager::thd_manager = NULL;

/**
  Internal class used in do_for_all_thd() and do_for_all_thd_copy()
  implementation.
*/

class Do_THD : public std::unary_function<THD*, void>
{
public:
  explicit Do_THD(Do_THD_Impl *impl) : m_impl(impl) {}

  /**
    Users of this class will override operator() in the _Impl class.

    @param thd THD of one element in global thread list
  */
  void operator()(THD* thd)
  {
    m_impl->operator()(thd);
  }
private:
  Do_THD_Impl *m_impl;
};


/**
  Internal class used in find_thd() implementation.
*/

class Find_THD : public std::unary_function<THD*, bool>
{
public:
  explicit Find_THD(Find_THD_Impl *impl) : m_impl(impl) {}

  bool operator()(THD* thd)
  {
    return m_impl->operator()(thd);
  }
private:
  Find_THD_Impl *m_impl;
};


#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_thd_list;
static PSI_mutex_key key_LOCK_thd_remove;
static PSI_mutex_key key_LOCK_thread_ids;

static PSI_mutex_info all_thd_manager_mutexes[]=
{
  { &key_LOCK_thd_list, "LOCK_thd_list", PSI_FLAG_GLOBAL},
  { &key_LOCK_thd_remove, "LOCK_thd_remove", PSI_FLAG_GLOBAL},
  { &key_LOCK_thread_ids, "LOCK_thread_ids", PSI_FLAG_GLOBAL }
};

static PSI_cond_key key_COND_thd_list;

static PSI_cond_info all_thd_manager_conds[]=
{
  { &key_COND_thd_list, "COND_thd_list", PSI_FLAG_GLOBAL}
};
#endif // HAVE_PSI_INTERFACE


const my_thread_id Global_THD_manager::reserved_thread_id= 0;

Global_THD_manager::Global_THD_manager()
  : thd_list(PSI_INSTRUMENT_ME),
    thread_ids(PSI_INSTRUMENT_ME),
    num_thread_running(0),
    thread_created(0),
    thread_id_counter(reserved_thread_id + 1),
    unit_test(false)
{
#ifdef HAVE_PSI_INTERFACE
  int count= array_elements(all_thd_manager_mutexes);
  mysql_mutex_register("sql", all_thd_manager_mutexes, count);

  count= array_elements(all_thd_manager_conds);
  mysql_cond_register("sql", all_thd_manager_conds, count);
#endif

  mysql_mutex_init(key_LOCK_thd_list, &LOCK_thd_list,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_remove,
                   &LOCK_thd_remove, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thread_ids,
                   &LOCK_thread_ids, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_thd_list, &COND_thd_list);

  // The reserved thread ID should never be used by normal threads,
  // so mark it as in-use. This ID is used by temporary THDs never
  // added to the list of THDs.
  thread_ids.push_back(reserved_thread_id);
}


Global_THD_manager::~Global_THD_manager()
{
  thread_ids.erase_unique(reserved_thread_id);
  DBUG_ASSERT(thd_list.empty());
  DBUG_ASSERT(thread_ids.empty());
  mysql_mutex_destroy(&LOCK_thd_list);
  mysql_mutex_destroy(&LOCK_thd_remove);
  mysql_mutex_destroy(&LOCK_thread_ids);
  mysql_cond_destroy(&COND_thd_list);
}


/*
  Singleton Instance creation
  This method do not require mutex guard as it is called only from main thread.
*/
bool Global_THD_manager::create_instance()
{
  if (thd_manager == NULL)
    thd_manager= new (std::nothrow) Global_THD_manager();
  return (thd_manager == NULL);
}


void Global_THD_manager::destroy_instance()
{
  delete thd_manager;
  thd_manager= NULL;
}


void Global_THD_manager::add_thd(THD *thd)
{
  DBUG_PRINT("info", ("Global_THD_manager::add_thd %p", thd));
  // Should have an assigned ID before adding to the list.
  DBUG_ASSERT(thd->thread_id() != reserved_thread_id);
  mysql_mutex_lock(&LOCK_thd_list);
  // Technically it is not supported to compare pointers, but it works.
  std::pair<THD_array::iterator, bool> insert_result=
    thd_list.insert_unique(thd);
  if (insert_result.second)
  {
    ++global_thd_count;
  }
  // Adding the same THD twice is an error.
  DBUG_ASSERT(insert_result.second);
  mysql_mutex_unlock(&LOCK_thd_list);
}


void Global_THD_manager::remove_thd(THD *thd)
{
  DBUG_PRINT("info", ("Global_THD_manager::remove_thd %p", thd));
  mysql_mutex_lock(&LOCK_thd_remove);
  mysql_mutex_lock(&LOCK_thd_list);

  if (!unit_test)
    DBUG_ASSERT(thd->release_resources_done());

  /*
    Used by binlog_reset_master.  It would be cleaner to use
    DEBUG_SYNC here, but that's not possible because the THD's debug
    sync feature has been shut down at this point.
  */
  DBUG_EXECUTE_IF("sleep_after_lock_thread_count_before_delete_thd", sleep(5););

  const size_t num_erased= thd_list.erase_unique(thd);
  if (num_erased == 1)
    --global_thd_count;
  // Removing a THD that was never added is an error.
  DBUG_ASSERT(1 == num_erased);
  mysql_mutex_unlock(&LOCK_thd_remove);
  mysql_cond_broadcast(&COND_thd_list);
  mysql_mutex_unlock(&LOCK_thd_list);
}


my_thread_id Global_THD_manager::get_new_thread_id()
{
  my_thread_id new_id;
  Mutex_lock lock(&LOCK_thread_ids);
  do {
    new_id= thread_id_counter++;
  } while (!thread_ids.insert_unique(new_id).second);
  return new_id;
}


void Global_THD_manager::release_thread_id(my_thread_id thread_id)
{
  if (thread_id == reserved_thread_id)
    return; // Some temporary THDs are never given a proper ID.
  Mutex_lock lock(&LOCK_thread_ids);
  const size_t num_erased MY_ATTRIBUTE((unused))=
    thread_ids.erase_unique(thread_id);
  // Assert if the ID was not found in the list.
  DBUG_ASSERT(1 == num_erased);
}


void Global_THD_manager::set_thread_id_counter(my_thread_id new_id)
{
  DBUG_ASSERT(unit_test == true);
  Mutex_lock lock(&LOCK_thread_ids);
  thread_id_counter= new_id;
}


void Global_THD_manager::wait_till_no_thd()
{
  mysql_mutex_lock(&LOCK_thd_list);
  while (get_thd_count() > 0)
  {
    mysql_cond_wait(&COND_thd_list, &LOCK_thd_list);
    DBUG_PRINT("quit", ("One thread died (count=%u)", get_thd_count()));
  }
  mysql_mutex_unlock(&LOCK_thd_list);
}


void Global_THD_manager::do_for_all_thd_copy(Do_THD_Impl *func)
{
  Do_THD doit(func);

  mysql_mutex_lock(&LOCK_thd_remove);
  mysql_mutex_lock(&LOCK_thd_list);

  /* Take copy of global_thread_list. */
  THD_array thd_list_copy(thd_list);

  /*
    Allow inserts to global_thread_list. Newly added thd
    will not be accounted for when executing func.
  */
  mysql_mutex_unlock(&LOCK_thd_list);

  /* Execute func for all existing threads. */
  std::for_each(thd_list_copy.begin(), thd_list_copy.end(), doit);

  DEBUG_SYNC_C("inside_do_for_all_thd_copy");
  mysql_mutex_unlock(&LOCK_thd_remove);
}


void Global_THD_manager::do_for_all_thd(Do_THD_Impl *func)
{
  Do_THD doit(func);
  mysql_mutex_lock(&LOCK_thd_list);
  std::for_each(thd_list.begin(), thd_list.end(), doit);
  mysql_mutex_unlock(&LOCK_thd_list);
}


THD* Global_THD_manager::find_thd(Find_THD_Impl *func)
{
  Find_THD find_thd(func);
  mysql_mutex_lock(&LOCK_thd_list);
  THD_array::const_iterator it=
    std::find_if(thd_list.begin(), thd_list.end(), find_thd);
  THD* ret= NULL;
  if (it != thd_list.end())
    ret= *it;
  mysql_mutex_unlock(&LOCK_thd_list);
  return ret;
}


void inc_thread_created()
{
  Global_THD_manager::get_instance()->inc_thread_created();
}


void thd_lock_thread_count(THD *)
{
  mysql_mutex_lock(&Global_THD_manager::get_instance()->LOCK_thd_list);
}


void thd_unlock_thread_count(THD *)
{
  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
  mysql_cond_broadcast(&thd_manager->COND_thd_list);
  mysql_mutex_unlock(&thd_manager->LOCK_thd_list);
}


template <typename T>
class Run_free_function : public Do_THD_Impl
{
public:
  typedef void (do_thd_impl)(THD*, T);

  Run_free_function(do_thd_impl *f, T arg) : m_func(f), m_arg(arg) {}

  virtual void operator()(THD *thd)
  {
    (*m_func)(thd, m_arg);
  }
private:
  do_thd_impl *m_func;
  T m_arg;
};


void do_for_all_thd(do_thd_impl_uint64 f, uint64 v)
{
  Run_free_function<uint64> runner(f, v);
  Global_THD_manager::get_instance()->do_for_all_thd(&runner);
}
