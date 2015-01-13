/*
   Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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

Ndb_component::Ndb_component(const char *name)
  : m_thread_state(TS_UNINIT),
    m_name(name)
{
}

Ndb_component::~Ndb_component()
{

}

int
Ndb_component::init()
{
  assert(m_thread_state == TS_UNINIT);

  native_mutex_init(&m_start_stop_mutex, MY_MUTEX_INIT_FAST);
  native_cond_init(&m_start_stop_cond);

  int res= do_init();
  if (res == 0)
  {
    m_thread_state= TS_INIT;
  }
  return res;
}

extern "C" void *
Ndb_component_run_C(void * arg)
{
  my_thread_init();
  Ndb_component * self = reinterpret_cast<Ndb_component*>(arg);
  self->run_impl();
  my_thread_end();
  my_thread_exit(0);
  return NULL;                              // Avoid compiler warnings
}

extern my_thread_attr_t connection_attrib; // mysql global pthread attr

int
Ndb_component::start()
{
  assert(m_thread_state == TS_INIT);
  native_mutex_lock(&m_start_stop_mutex);
  m_thread_state= TS_STARTING;
  int res= my_thread_create(&m_thread, &connection_attrib, Ndb_component_run_C,
                            this);

  if (res == 0)
  {
    while (m_thread_state == TS_STARTING)
    {
      native_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
    native_mutex_unlock(&m_start_stop_mutex);
    return m_thread_state == TS_RUNNING ? 0 : 1;
  }

  native_mutex_unlock(&m_start_stop_mutex);
  return res;
}

void
Ndb_component::run_impl()
{
  native_mutex_lock(&m_start_stop_mutex);
  if (m_thread_state == TS_STARTING)
  {
    m_thread_state= TS_RUNNING;
    native_cond_signal(&m_start_stop_cond);
    native_mutex_unlock(&m_start_stop_mutex);
    do_run();
    native_mutex_lock(&m_start_stop_mutex);
  }
  m_thread_state = TS_STOPPED;
  native_cond_signal(&m_start_stop_cond);
  native_mutex_unlock(&m_start_stop_mutex);
}

bool
Ndb_component::is_stop_requested()
{
  bool res = false;
  native_mutex_lock(&m_start_stop_mutex);
  res = m_thread_state != TS_RUNNING;
  native_mutex_unlock(&m_start_stop_mutex);
  return res;
}

int
Ndb_component::stop()
{
  log_info("Stop");
  native_mutex_lock(&m_start_stop_mutex);
  assert(m_thread_state == TS_RUNNING ||
         m_thread_state == TS_STOPPING ||
         m_thread_state == TS_STOPPED);

  if (m_thread_state == TS_RUNNING)
  {
    m_thread_state= TS_STOPPING;
  }

  // Give subclass a call, should wake itself up to quickly detect the stop
  do_wakeup();

  if (m_thread_state == TS_STOPPING)
  {
    while (m_thread_state != TS_STOPPED)
    {
      native_cond_signal(&m_start_stop_cond);
      native_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
  }
  native_mutex_unlock(&m_start_stop_mutex);
  log_info("Stop completed");

  return 0;
}

int
Ndb_component::deinit()
{
  assert(m_thread_state == TS_STOPPED);
  native_mutex_destroy(&m_start_stop_mutex);
  native_cond_destroy(&m_start_stop_cond);
  return do_deinit();
}

#include "ndb_log.h"


void Ndb_component::log_verbose(unsigned verbose_level, const char *fmt, ...)
{
  // Print message only if verbose level is set high enough
  if (ndb_log_get_verbose_level() < verbose_level)
    return;

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, m_name, fmt, args);
  va_end(args);
}


void Ndb_component::log_error(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_ERROR_LEVEL, m_name, fmt, args);
  va_end(args);
}


void Ndb_component::log_warning(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_WARNING_LEVEL, m_name, fmt, args);
  va_end(args);
}


void Ndb_component::log_info(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, m_name, fmt, args);
  va_end(args);
}
