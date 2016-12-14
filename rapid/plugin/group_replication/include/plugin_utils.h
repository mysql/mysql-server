/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_UTILS_INCLUDED
#define PLUGIN_UTILS_INCLUDED

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "plugin_psi.h"
#include <mysql/group_replication_priv.h>

/**
  This method instructs all local transactions to rollback when certification is
  no longer possible.
*/
void unblock_waiting_transactions();

/**
  This method creates a server session and connects to the server
  to enable the read mode

  @param threaded   Shall the session create a new dedicated thread

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int set_server_read_mode(bool threaded);

/**
  This method creates a server session and connects to the server
  to reset the read mode

  @param threaded   Shall the session create a new dedicated thread

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int reset_server_read_mode(bool threaded);

template <typename T>
class Synchronized_queue
{
public:
  Synchronized_queue()
  {
    mysql_mutex_init(key_GR_LOCK_synchronized_queue, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_synchronized_queue, &cond);
  }

  /**
    Checks if the queue is empty
    @return if is empty
      @retval true  empty
      @retval false not empty
  */
  bool empty()
  {
    bool res= true;
    mysql_mutex_lock(&lock);
    res= queue.empty();
    mysql_mutex_unlock(&lock);

    return res;
  }

  /**
    Inserts an element in the queue.
    Alerts any other thread lock on pop() or front()
    @param value The value to insert
   */
  void push(const T& value)
  {
    mysql_mutex_lock(&lock);
    queue.push(value);
    mysql_mutex_unlock(&lock);
    mysql_cond_broadcast(&cond);
  }

  /**
    Fetches the front of the queue and removes it.
    @note The method will block if the queue is empty until a element is pushed

    @param out  The fetched reference.
  */
  void pop(T* out)
  {
    *out= NULL;
    mysql_mutex_lock(&lock);
    while (queue.empty())
      mysql_cond_wait(&cond, &lock); /* purecov: inspected */
    *out= queue.front();
    queue.pop();
    mysql_mutex_unlock(&lock);
  }

  /**
    Pops the front of the queue removing it.
    @note The method will block if the queue is empty until a element is pushed
  */
  void pop()
  {
    mysql_mutex_lock(&lock);
    while (queue.empty())
      mysql_cond_wait(&cond, &lock); /* purecov: inspected */
    queue.pop();
    mysql_mutex_unlock(&lock);
  }

  /**
    Fetches the front of the queue but does not remove it.
    @note The method will block if the queue is empty until a element is pushed

    @param out  The fetched reference.
  */
  void front(T* out)
  {
    *out= NULL;
    mysql_mutex_lock(&lock);
    while (queue.empty())
      mysql_cond_wait(&cond, &lock);
    *out= queue.front();
    mysql_mutex_unlock(&lock);
  }

  /**
    Checks the queue size
    @return the size of the queue
  */
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
    mysql_mutex_init(key_GR_LOCK_count_down_latch, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_count_down_latch, &cond);
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
  int count;
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
    :blocked(false), waiting(false)
  {
    mysql_mutex_init(key_GR_LOCK_wait_ticket, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_wait_ticket, &cond);
  }

  virtual ~Wait_ticket()
  {
    for (typename std::map<K,CountDownLatch*>::iterator it= map.begin();
         it != map.end();
         ++it)
      delete it->second; /* purecov: inspected */
    map.clear();

    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&lock);
  }

  /**
    Register ticker with status ongoing.

    @param       key     The key that identifies the ticket
    @return
         @retval 0       success
         @retval !=0     key already exists, error on insert or it is blocked
  */
  int registerTicket(const K& key)
  {
    int error= 0;

    mysql_mutex_lock(&lock);

    if (blocked)
    {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

    typename std::map<K,CountDownLatch*>::iterator it= map.find(key);
    if (it != map.end())
    {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

    CountDownLatch *cdl = new CountDownLatch(1);
    std::pair<typename std::map<K,CountDownLatch*>::iterator,bool> ret;
    ret= map.insert(std::pair<K,CountDownLatch*>(key,cdl));
    if (ret.second == false)
    {
      error= 1;   /* purecov: inspected */
      delete cdl; /* purecov: inspected */
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
         @retval !=0       key doesn't exist, or the Ticket is blocked
  */
  int waitTicket(const K& key)
  {
    int error= 0;
    CountDownLatch *cdl= NULL;

    mysql_mutex_lock(&lock);

    if (blocked)
    {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

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

      if (waiting)
      {
        if (map.empty())
        {
          mysql_cond_broadcast(&cond);
        }
      }
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

  /**
    Gets all the waiting keys.

    @param[out] key_list  all the keys to return
  */
  void get_all_waiting_keys(std::vector<K>& key_list)
  {
    mysql_mutex_lock(&lock);
    for(typename std::map<K,CountDownLatch*>::iterator iter = map.begin();
       iter != map.end();
       ++iter)
    {
       K key= iter->first;
       key_list.push_back(key);
    }
    mysql_mutex_unlock(&lock);
  }

  /**
    Blocks or unblocks the class from receiving waiting requests.

    @param[in] blocked_flag  if the class should block or not
  */
  void set_blocked_status(bool blocked_flag)
  {
    mysql_mutex_lock(&lock);
    blocked= blocked_flag;
    mysql_mutex_unlock(&lock);
  }

  int block_until_empty(int timeout)
  {
    mysql_mutex_lock(&lock);
    waiting= true;
    while (!map.empty())
    {
      struct timespec abstime;
      set_timespec(&abstime, 1);
#ifndef DBUG_OFF
      int error=
#endif
      mysql_cond_timedwait(&cond, &lock, &abstime);
      DBUG_ASSERT(error == ETIMEDOUT || error == 0);
      if (timeout >= 1)
      {
        timeout= timeout - 1;
      }
      else if (!map.empty())
      {
        //time out
        waiting= false;
        mysql_mutex_unlock(&lock);
        return 1;
      }
    }
    waiting= false;
    mysql_mutex_unlock(&lock);
    return 0;
  }

private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::map<K,CountDownLatch*> map;
  bool blocked;
  bool waiting;
};


class Mutex_autolock
{

public:
  Mutex_autolock(mysql_mutex_t *arg) : ptr_mutex(arg)
  {
    DBUG_ENTER("Mutex_autolock::Mutex_autolock");

    DBUG_ASSERT(arg != NULL);

    mysql_mutex_lock(ptr_mutex);
    DBUG_VOID_RETURN;
  }
  ~Mutex_autolock()
  {
      mysql_mutex_unlock(ptr_mutex);
  }

private:
  mysql_mutex_t *ptr_mutex;
  Mutex_autolock(Mutex_autolock const&); // no copies permitted
  void operator=(Mutex_autolock const&);
};

class Shared_writelock
{
public:
  Shared_writelock(Checkable_rwlock *arg)
    : shared_write_lock(arg), write_lock_in_use(false)
  {
    DBUG_ENTER("Shared_writelock::Shared_writelock");

    DBUG_ASSERT(arg != NULL);

    mysql_mutex_init(key_GR_LOCK_write_lock_protection, &write_lock, MY_MUTEX_INIT_FAST);

    DBUG_VOID_RETURN;
  }

  virtual ~Shared_writelock()
  {
    mysql_mutex_destroy(&write_lock);
  }

  int try_grab_write_lock()
  {
    int res= 0;
    mysql_mutex_lock(&write_lock);

    if (write_lock_in_use)
      res= 1; /* purecov: inspected */
    else
    {
      shared_write_lock->wrlock();
      write_lock_in_use= true;
    }

    mysql_mutex_unlock(&write_lock);
    return res;
  }

  void grab_write_lock()
  {
    mysql_mutex_lock(&write_lock);
    shared_write_lock->wrlock();
    write_lock_in_use= true;
    mysql_mutex_unlock(&write_lock);
  }

  void release_write_lock()
  {
    mysql_mutex_lock(&write_lock);
    shared_write_lock->unlock();
    write_lock_in_use= false;
    mysql_mutex_unlock(&write_lock);
  }

  /**
    Grab a read lock only if there is no write lock acquired.

    @return
         @retval 0         read lock acquired
         @retval !=0       there is a write lock acquired
  */
  int try_grab_read_lock()
  {
    int res= 0;
    mysql_mutex_lock(&write_lock);

    if (write_lock_in_use)
      res= 1;
    else
      shared_write_lock->rdlock();

    mysql_mutex_unlock(&write_lock);
    return res;
  }

  void grab_read_lock()
  {
    shared_write_lock->rdlock();
  }

  void release_read_lock()
  {
    shared_write_lock->unlock();
  }

private:
  Checkable_rwlock *shared_write_lock;
  mysql_mutex_t write_lock;
  bool write_lock_in_use;
};

#endif /* PLUGIN_UTILS_INCLUDED */
