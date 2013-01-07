/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_component.h"

Ndb_component::Ndb_component()
  : m_thread_state(TS_UNINIT)
{
}

Ndb_component::~Ndb_component()
{

}

int
Ndb_component::init()
{
  assert(m_thread_state == TS_UNINIT);

  pthread_mutex_init(&m_start_stop_mutex, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&m_start_stop_cond, NULL);

  int res= do_init();
  if (res == 0)
  {
    m_thread_state= TS_INIT;
  }
  return res;
}

void *
Ndb_component_run_C(void * arg)
{
  my_thread_init();
  Ndb_component * self = reinterpret_cast<Ndb_component*>(arg);
  self->run_impl();
  my_thread_end();
  pthread_exit(0);
  return NULL;                              // Avoid compiler warnings
}

extern pthread_attr_t connection_attrib; // mysql global pthread attr

int
Ndb_component::start()
{
  assert(m_thread_state == TS_INIT);
  pthread_mutex_lock(&m_start_stop_mutex);
  m_thread_state= TS_STARTING;
  int res= pthread_create(&m_thread, &connection_attrib, Ndb_component_run_C,
                          this);

  if (res == 0)
  {
    while (m_thread_state == TS_STARTING)
    {
      pthread_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
    pthread_mutex_unlock(&m_start_stop_mutex);
    return m_thread_state == TS_RUNNING ? 0 : 1;
  }

  pthread_mutex_unlock(&m_start_stop_mutex);
  return res;
}

void
Ndb_component::run_impl()
{
  pthread_detach_this_thread();
  pthread_mutex_lock(&m_start_stop_mutex);
  if (m_thread_state == TS_STARTING)
  {
    m_thread_state= TS_RUNNING;
    pthread_cond_signal(&m_start_stop_cond);
    pthread_mutex_unlock(&m_start_stop_mutex);
    do_run();
    pthread_mutex_lock(&m_start_stop_mutex);
  }
  m_thread_state = TS_STOPPED;
  pthread_cond_signal(&m_start_stop_cond);
  pthread_mutex_unlock(&m_start_stop_mutex);
}

bool
Ndb_component::is_stop_requested()
{
  bool res = false;
  pthread_mutex_lock(&m_start_stop_mutex);
  res = m_thread_state != TS_RUNNING;
  pthread_mutex_unlock(&m_start_stop_mutex);
  return res;
}

int
Ndb_component::stop()
{
  pthread_mutex_lock(&m_start_stop_mutex);
  assert(m_thread_state == TS_RUNNING ||
         m_thread_state == TS_STOPPING ||
         m_thread_state == TS_STOPPED);

  if (m_thread_state == TS_RUNNING)
  {
    m_thread_state= TS_STOPPING;
  }

  if (m_thread_state == TS_STOPPING)
  {
    while (m_thread_state != TS_STOPPED)
    {
      pthread_cond_signal(&m_start_stop_cond);
      pthread_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
  }
  pthread_mutex_unlock(&m_start_stop_mutex);

  return 0;
}

int
Ndb_component::deinit()
{
  assert(m_thread_state == TS_STOPPED);
  pthread_mutex_destroy(&m_start_stop_mutex);
  pthread_cond_destroy(&m_start_stop_cond);
  return do_deinit();
}
