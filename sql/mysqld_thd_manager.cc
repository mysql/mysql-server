/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "debug_sync.h"              // DEBUG_SYNC_C
#include "sql_class.h"               // THD


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
static PSI_mutex_key key_LOCK_thd_count;
static PSI_mutex_key key_LOCK_thd_remove;
static PSI_mutex_key key_LOCK_thread_created;

static PSI_mutex_info all_thd_manager_mutexes[]=
{
  { &key_LOCK_thd_count, "LOCK_thd_count", PSI_FLAG_GLOBAL},
  { &key_LOCK_thd_remove, "LOCK_thd_remove", PSI_FLAG_GLOBAL},
  { &key_LOCK_thread_created, "LOCK_thread_created", PSI_FLAG_GLOBAL }
};

static PSI_cond_key key_COND_thd_count;

static PSI_cond_info all_thd_manager_conds[]=
{
  { &key_COND_thd_count, "COND_thread_count", PSI_FLAG_GLOBAL}
};
#endif // HAVE_PSI_INTERFACE


Global_THD_manager::Global_THD_manager()
  : num_thread_running(0),
    thread_created(0),
    thread_id(1L),
    unit_test(false)
{
  thd_list= new std::set<THD*>;

#ifdef HAVE_PSI_INTERFACE
  int count= array_elements(all_thd_manager_mutexes);
  mysql_mutex_register("sql", all_thd_manager_mutexes, count);

  count= array_elements(all_thd_manager_conds);
  mysql_cond_register("sql", all_thd_manager_conds, count);
#endif

  mysql_mutex_init(key_LOCK_thd_count, &LOCK_thd_count,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_remove,
                   &LOCK_thd_remove, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thread_created,
                   &LOCK_thread_created, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_thd_count, &COND_thd_count, NULL);
  my_atomic_rwlock_init(&thread_running_lock);
  my_atomic_rwlock_init(&thread_id_lock);
}


Global_THD_manager::~Global_THD_manager()
{
  delete thd_list;
  thd_list= NULL;
  mysql_mutex_destroy(&LOCK_thd_count);
  mysql_mutex_destroy(&LOCK_thd_remove);
  mysql_mutex_destroy(&LOCK_thread_created);
  mysql_cond_destroy(&COND_thd_count);
  my_atomic_rwlock_destroy(&thread_running_lock);
  my_atomic_rwlock_destroy(&thread_id_lock);
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
  DBUG_PRINT("info", ("Global_THD_manager::add_thread %p", thd));
  mysql_mutex_lock(&LOCK_thd_count);
  std::pair<std::set<THD*>::iterator, bool>
    insert_result= thd_list->insert(thd);
  if (insert_result.second)
  {
    ++global_thd_count;
  }
  // Adding the same THD twice is an error.
  DBUG_ASSERT(insert_result.second);
  mysql_mutex_unlock(&LOCK_thd_count);
}


void Global_THD_manager::remove_thd(THD *thd)
{
  DBUG_PRINT("info", ("Global_THD_manager::remove_thread %p", thd));
  mysql_mutex_lock(&LOCK_thd_remove);
  mysql_mutex_lock(&LOCK_thd_count);

  if (!unit_test)
    DBUG_ASSERT(thd->release_resources_done());

  /*
    Used by binlog_reset_master.  It would be cleaner to use
    DEBUG_SYNC here, but that's not possible because the THD's debug
    sync feature has been shut down at this point.
  */
  DBUG_EXECUTE_IF("sleep_after_lock_thread_count_before_delete_thd", sleep(5););

  const size_t num_erased= thd_list->erase(thd);
  if (num_erased == 1)
    --global_thd_count;
  // Removing a THD that was never added is an error.
  DBUG_ASSERT(1 == num_erased);
  mysql_mutex_unlock(&LOCK_thd_remove);
  mysql_cond_broadcast(&COND_thd_count);
  mysql_mutex_unlock(&LOCK_thd_count);
}


void Global_THD_manager::inc_thread_running()
{
  my_atomic_rwlock_wrlock(&thread_running_lock);
  my_atomic_add32(&num_thread_running, 1);
  my_atomic_rwlock_wrunlock(&thread_running_lock);
}


void Global_THD_manager::dec_thread_running()
{
  my_atomic_rwlock_wrlock(&thread_running_lock);
  my_atomic_add32(&num_thread_running, -1);
  my_atomic_rwlock_wrunlock(&thread_running_lock);
}


void Global_THD_manager::inc_thread_created()
{
  mysql_mutex_lock(&LOCK_thread_created);
  thread_created++;
  mysql_mutex_unlock(&LOCK_thread_created);
}


my_thread_id Global_THD_manager::get_inc_thread_id()
{
  my_atomic_rwlock_wrlock(&thread_id_lock);
  my_thread_id id= static_cast<my_thread_id>(my_atomic_add64(&thread_id, 1));
  my_atomic_rwlock_wrunlock(&thread_id_lock);
  return id;
}


void Global_THD_manager::wait_till_no_thd()
{
  mysql_mutex_lock(&LOCK_thd_count);
  while (get_thd_count() > 0)
  {
    mysql_cond_wait(&COND_thd_count, &LOCK_thd_count);
    DBUG_PRINT("quit", ("One thread died (count=%u)", get_thd_count()));
  }
  mysql_mutex_unlock(&LOCK_thd_count);
}


void Global_THD_manager::do_for_all_thd_copy(Do_THD_Impl *func)
{
  /* Take copy of global_thread_list. */
  std::set<THD*> thd_list_copy;
  Do_THD doit(func);

  mysql_mutex_lock(&LOCK_thd_remove);
  mysql_mutex_lock(&LOCK_thd_count);
  try
  {
    thd_list_copy= *thd_list;
  }
  catch (std::bad_alloc)
  {
  }

  /*
    Allow inserts to global_thread_list. Newly added thd
    will not be accounted for when executing func.
  */
  mysql_mutex_unlock(&LOCK_thd_count);

  /* Execute func for all existing threads. */
  std::for_each(thd_list_copy.begin(), thd_list_copy.end(), doit);

  DEBUG_SYNC_C("inside_do_for_all_thd_copy");
  mysql_mutex_unlock(&LOCK_thd_remove);
}


void Global_THD_manager::do_for_all_thd(Do_THD_Impl *func)
{
  Do_THD doit(func);
  mysql_mutex_lock(&LOCK_thd_count);
  std::for_each(thd_list->begin(), thd_list->end(), doit);
  mysql_mutex_unlock(&LOCK_thd_count);
}


THD* Global_THD_manager::find_thd(Find_THD_Impl *func)
{
  Find_THD find_thd(func);
  mysql_mutex_lock(&LOCK_thd_count);
  std::set<THD*>::const_iterator it= std::find_if(thd_list->begin(),
                                                  thd_list->end(), find_thd);
  THD* ret= NULL;
  if (it != thd_list->end())
    ret= *it;
  mysql_mutex_unlock(&LOCK_thd_count);
  return ret;
}


void inc_thread_created()
{
  Global_THD_manager::get_instance()->inc_thread_created();
}


void thd_lock_thread_count(THD *)
{
  mysql_mutex_lock(&Global_THD_manager::get_instance()->LOCK_thd_count);
}


void thd_unlock_thread_count(THD *)
{
  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
  mysql_cond_broadcast(&thd_manager->COND_thd_count);
  mysql_mutex_unlock(&thd_manager->LOCK_thd_count);
}


void remove_global_thread(THD *thd)
{
  Global_THD_manager::get_instance()->remove_thd(thd);
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
