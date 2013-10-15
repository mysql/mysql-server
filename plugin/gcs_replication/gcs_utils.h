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

#ifndef GCS_UTILS_INCLUDE
#define GCS_UTILS_INCLUDE

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif
#ifndef HAVE_REPLICATION
#define HAVE_REPLICATION
#endif

#include <queue>
#include <vector>
#include <my_global.h>
#include <my_sys.h>


static void register_gcs_psi_keys(PSI_mutex_info gcs_mutexes[],
                                  PSI_cond_info gcs_conds[]);

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

    register_gcs_psi_keys(queue_mutexes, queue_conds);

    mysql_mutex_init(key_mutex, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond, &cond, NULL);
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

private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::queue<T> queue;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_mutex;
  PSI_cond_key key_cond;
#endif
};


/*
  Register the psi keys for mutexes and conditions

  @param[in]  gcs_mutexes    PSI mutex info
  @param[in]  gcs_conds      PSI condition info
*/
static void register_gcs_psi_keys(PSI_mutex_info gcs_mutexes[],
                                  PSI_cond_info gcs_conds[])
{
  const char* category= "gcs";
  if (gcs_mutexes != NULL)
  {
    int count= array_elements(gcs_mutexes);
    mysql_mutex_register(category, gcs_mutexes, count);
  }
  if (gcs_conds)
  {
    int count= array_elements(gcs_conds);
    mysql_cond_register(category, gcs_conds, count);
  }
}

inline bool is_local(){
  return false;
}


class MessageBuffer
{

private:
  std::vector<unsigned char> *buffer;

public:
  MessageBuffer()
  {
    this->buffer= new std::vector<unsigned char>();
  }

  MessageBuffer(int capacity)
  {
    this->buffer= new std::vector<unsigned char>();
    this->buffer->reserve(capacity);
  }

  ~MessageBuffer()
  {
    delete this->buffer;
  }

  size_t length()
  {
    return this->buffer->size();
  }

  void append(const unsigned char *s, size_t len)
  {
    this->buffer->insert(this->buffer->end(), s, s+len);
  }

  void reset()
  {
    this->buffer->clear();
  }

  const unsigned char* data()
  {
    return &this->buffer->front();
  }
};

#endif /* GCS_UTILS_INCLUDE */
