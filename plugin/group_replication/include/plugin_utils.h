/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_UTILS_INCLUDED
#define GCS_UTILS_INCLUDED

#include <map>
#include <queue>
#include <string>

#include "plugin_psi.h"


template <typename T>
class Synchronized_queue
{
public:
  Synchronized_queue()
  {

    PSI_cond_info queue_conds[]=
    {
      { &key_cond, "COND_sync_queue_wait", 0}
    };

    PSI_mutex_info queue_mutexes[]=
    {
      { &key_mutex, "LOCK_sync_queue_wait", 0}
    };

    register_gcs_psi_keys(queue_mutexes, 1, queue_conds, 1);

    mysql_mutex_init(key_mutex, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond, &cond);
  }

  bool empty()
  {
    bool res= true;
    mysql_mutex_lock(&lock);
    res= queue.empty();
    mysql_mutex_unlock(&lock);

    return res;
  }

  int push(const T& value)
  {
    mysql_mutex_lock(&lock);
    queue.push(value);
    mysql_mutex_unlock(&lock);
    mysql_cond_broadcast(&cond);
    return 0;
  }

  int pop(T* out)
  {
    *out= NULL;
    mysql_mutex_lock(&lock);
    while (queue.empty())
      mysql_cond_wait(&cond, &lock);
    *out= queue.front();
    queue.pop();
    mysql_mutex_unlock(&lock);

    return 0;
  }

  ulong size()
  {
    ulong qsize= 0;
    mysql_mutex_lock(&lock);
    qsize= queue.size();
    mysql_mutex_unlock(&lock);

    return qsize;
  }

private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::queue<T> queue;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_mutex;
  PSI_cond_key key_cond;
#endif
};


/**
  Synchronization auxiliary class that allows one or more threads
  to wait on a given number of requirements.

  Usage:
    CountDownLatch(count):
      Create the latch with the number of requirements to wait.
    wait():
      Block until the number of requirements reaches zero.
    countDown():
      Decrease the number of requirements by one.
*/
class CountDownLatch
{
public:
  /**
    Create the latch with the number of requirements to wait.

    @param       count     The number of requirements to wait
  */
  CountDownLatch(uint count) : count(count)
  {
#ifdef HAVE_PSI_INTERFACE
    PSI_cond_info conds[]=
    {
      { &key_cond, "COND_count_down_latch", 0}
    };

    PSI_mutex_info mutexes[]=
    {
      { &key_mutex, "LOCK_count_down_latch", 0}
    };

    register_gcs_psi_keys(mutexes, 1, conds, 1);
#endif /* HAVE_PSI_INTERFACE */

    mysql_mutex_init(key_mutex, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond, &cond);
  }

  virtual ~CountDownLatch()
  {
    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&lock);
  }

  /**
    Block until the number of requirements reaches zero.
  */
  void wait()
  {
    mysql_mutex_lock(&lock);
    while (count > 0)
      mysql_cond_wait(&cond, &lock);
    mysql_mutex_unlock(&lock);
  }

  /**
    Decrease the number of requirements by one.
  */
  void countDown()
  {
    mysql_mutex_lock(&lock);
    --count;
    if (count == 0)
      mysql_cond_broadcast(&cond);
    mysql_mutex_unlock(&lock);
  }

  /**
    Get current number requirements.

    @return      the number of requirements
  */
  uint getCount()
  {
    uint res= 0;
    mysql_mutex_lock(&lock);
    res= count;
    mysql_mutex_unlock(&lock);
    return res;
  }

private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  uint count;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_mutex;
  PSI_cond_key key_cond;
#endif
};

/**
  Ticket register/wait auxiliary class.
  Usage:
    registerTicket(k):
      create a ticket with key k with status ongoing.
    releaseTicket(k):
      set ticket with key k status to done.
    waitTicket(k):
      wait until ticket with key k status is changed to done.
*/
template <typename K>
class Wait_ticket
{
public:
  Wait_ticket()
  {
#ifdef HAVE_PSI_INTERFACE
    PSI_cond_info map_conds[]=
    {
      { &key_cond, "COND_wait_ticket", 0}
    };

    PSI_mutex_info map_mutexes[]=
    {
      { &key_mutex, "LOCK_wait_ticket", 0}
    };

    register_gcs_psi_keys(map_mutexes, 1, map_conds, 1);
#endif /* HAVE_PSI_INTERFACE */

    mysql_mutex_init(key_mutex, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond, &cond);
  }

  virtual ~Wait_ticket()
  {
    for (typename std::map<K,CountDownLatch*>::iterator it= map.begin();
         it != map.end();
         ++it)
      delete it->second;
    map.clear();

    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&lock);
  }

  /**
    Register ticker with status ongoing.

    @param       key       The key that identifies the ticket
    @return
         @retval 0         success
         @retval !=0        (key already exists or error on insert)
  */
  int registerTicket(const K& key)
  {
    int error= 0;

    mysql_mutex_lock(&lock);
    typename std::map<K,CountDownLatch*>::iterator it= map.find(key);
    if (it != map.end())
    {
      mysql_mutex_unlock(&lock);
      return 1;
    }

    CountDownLatch *cdl = new CountDownLatch(1);
    std::pair<typename std::map<K,CountDownLatch*>::iterator,bool> ret;
    ret= map.insert(std::pair<K,CountDownLatch*>(key,cdl));
    if (ret.second == false)
    {
      error= 1;
      delete cdl;
    }

    mysql_mutex_unlock(&lock);
    return error;
  }

  /**
   Wait until ticket status is done.
   @note The ticket is removed after the wait.

    @param       key       The key that identifies the ticket
    @return
         @retval 0         success
         @retval !=0       (key doesn't exist)
  */
  int waitTicket(const K& key)
  {
    int error= 0;
    CountDownLatch *cdl= NULL;

    mysql_mutex_lock(&lock);
    typename std::map<K,CountDownLatch*>::iterator it= map.find(key);
    if (it == map.end())
      error= 1;
    else
      cdl= it->second;
    mysql_mutex_unlock(&lock);

    if (cdl != NULL)
    {
      cdl->wait();

      mysql_mutex_lock(&lock);
      delete cdl;
      map.erase(it);
      mysql_mutex_unlock(&lock);
    }

    return error;
  }

  /**
   Set ticket status to done.

    @param       key       The key that identifies the ticket
    @return
         @retval 0         success
         @retval !=0       (key doesn't exist)
  */
  int releaseTicket(const K& key)
  {
    int error= 0;

    mysql_mutex_lock(&lock);
    typename std::map<K,CountDownLatch*>::iterator it= map.find(key);
    if (it == map.end())
      error= 1;
    else
      it->second->countDown();
    mysql_mutex_unlock(&lock);

    return error;
  }

private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::map<K,CountDownLatch*> map;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_mutex;
  PSI_cond_key key_cond;
#endif
};


class Mutex_autolock
{

public:
  Mutex_autolock(pthread_mutex_t *arg) : ptr_mutex(arg)
  {
    DBUG_ENTER("Mutex_autolock::Mutex_autolock");

    DBUG_ASSERT(arg != NULL);

    pthread_mutex_lock(ptr_mutex);
    DBUG_VOID_RETURN;
  }
  ~Mutex_autolock()
  {
      pthread_mutex_unlock(ptr_mutex);
  }

private:
  pthread_mutex_t *ptr_mutex;
  Mutex_autolock(Mutex_autolock const&); // no copies permitted
  void operator=(Mutex_autolock const&);
};

#endif /* GCS_UTILS_INCLUDED */
