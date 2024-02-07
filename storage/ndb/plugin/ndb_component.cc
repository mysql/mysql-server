/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_component.h"

#include <stdarg.h>

#include "my_systime.h"  // set_timespec

Ndb_component::Ndb_component(const char *name, const char *psi_name)
    : m_thread_state(TS_UNINIT),
      m_server_started(false),
      m_name(name),
      m_psi_name(psi_name) {}

Ndb_component::~Ndb_component() {}

int Ndb_component::init() {
  assert(m_thread_state == TS_UNINIT);

  // Register a PSI key for this thread
  PSI_thread_info thread_info = {&m_psi_thread_key,
                                 m_psi_name,          // Name
                                 m_psi_name,          // OS name
                                 PSI_FLAG_SINGLETON,  // flags
                                 0,
                                 PSI_DOCUMENT_ME};
  mysql_thread_register("ndbcluster", &thread_info, 1);

  mysql_mutex_init(PSI_INSTRUMENT_ME, &m_start_stop_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &m_start_stop_cond);

  int res = do_init();
  if (res == 0) {
    m_thread_state = TS_INIT;
  }
  return res;
}

extern "C" void *Ndb_component_run_C(void *arg) {
  my_thread_init();
  Ndb_component *self = reinterpret_cast<Ndb_component *>(arg);
  self->run_impl();
  my_thread_end();
  my_thread_exit(nullptr);
  return nullptr;  // Avoid compiler warnings
}

extern my_thread_attr_t connection_attrib;  // mysql global pthread attr

int Ndb_component::start() {
  assert(m_thread_state == TS_INIT);
  mysql_mutex_lock(&m_start_stop_mutex);
  m_thread_state = TS_STARTING;

  const int res =
      mysql_thread_create(m_psi_thread_key, &m_thread, &connection_attrib,
                          Ndb_component_run_C, this);
  if (res == 0) {
    while (m_thread_state == TS_STARTING) {
      mysql_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
    mysql_mutex_unlock(&m_start_stop_mutex);
    return m_thread_state == TS_RUNNING ? 0 : 1;
  }

  mysql_mutex_unlock(&m_start_stop_mutex);
  return res;
}

void Ndb_component::run_impl() {
  mysql_mutex_lock(&m_start_stop_mutex);
  if (m_thread_state == TS_STARTING) {
    m_thread_state = TS_RUNNING;
    mysql_cond_signal(&m_start_stop_cond);
    mysql_mutex_unlock(&m_start_stop_mutex);
    do_run();
    mysql_mutex_lock(&m_start_stop_mutex);
  }
  m_thread_state = TS_STOPPED;
  mysql_cond_signal(&m_start_stop_cond);
  mysql_mutex_unlock(&m_start_stop_mutex);
}

bool Ndb_component::is_stop_requested() {
  bool res = false;
  mysql_mutex_lock(&m_start_stop_mutex);
  res = m_thread_state != TS_RUNNING;
  mysql_mutex_unlock(&m_start_stop_mutex);
  return res;
}

int Ndb_component::stop() {
  log_info("Stop");
  mysql_mutex_lock(&m_start_stop_mutex);
  assert(m_thread_state == TS_RUNNING || m_thread_state == TS_STOPPING ||
         m_thread_state == TS_STOPPED);

  if (m_thread_state == TS_RUNNING) {
    m_thread_state = TS_STOPPING;
  }

  // Give subclass a call, should wake itself up to quickly
  // detect the stop.
  // Unlock the mutex first to avoid deadlock betewen
  // is_stop_requested() and do_wakeup() due to different
  // mutex lock order
  mysql_mutex_unlock(&m_start_stop_mutex);
  do_wakeup();
  mysql_mutex_lock(&m_start_stop_mutex);

  if (m_thread_state == TS_STOPPING) {
    while (m_thread_state != TS_STOPPED) {
      mysql_cond_signal(&m_start_stop_cond);
      mysql_cond_wait(&m_start_stop_cond, &m_start_stop_mutex);
    }
  }
  mysql_mutex_unlock(&m_start_stop_mutex);
  log_info("Stop completed");

  return 0;
}

int Ndb_component::deinit() {
  assert(m_thread_state == TS_STOPPED);
  mysql_mutex_destroy(&m_start_stop_mutex);
  mysql_cond_destroy(&m_start_stop_cond);
  return do_deinit();
}

void Ndb_component::set_server_started() {
  mysql_mutex_lock(&m_start_stop_mutex);

  // Can only transition to "server started" once
  assert(m_server_started == false);
  m_server_started = true;

  mysql_cond_signal(&m_start_stop_cond);
  mysql_mutex_unlock(&m_start_stop_mutex);
}

bool Ndb_component::is_server_started() {
  bool server_started;
  mysql_mutex_lock(&m_start_stop_mutex);
  server_started = m_server_started;
  mysql_mutex_unlock(&m_start_stop_mutex);
  return server_started;
}

bool Ndb_component::wait_for_server_started(void) {
  log_verbose(1, "Wait for server start");

  mysql_mutex_lock(&m_start_stop_mutex);
  while (!m_server_started) {
    // Wait max one second before checking again if server has been
    // started or shutdown has been requested
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_start_stop_cond, &m_start_stop_mutex, &abstime);

    // Has shutdown been requested
    if (m_thread_state != TS_RUNNING) {
      mysql_mutex_unlock(&m_start_stop_mutex);
      return false;
    }
  }
  mysql_mutex_unlock(&m_start_stop_mutex);

  log_verbose(1, "Detected server start");

  return true;
}

#include "storage/ndb/plugin/ndb_log.h"

void Ndb_component::log_verbose(unsigned verbose_level, const char *fmt,
                                ...) const {
  // Print message only if verbose level is set high enough
  if (ndb_log_get_verbose_level() < verbose_level) return;

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, m_name, fmt, args);
  va_end(args);
}

void Ndb_component::log_error(const char *fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_ERROR_LEVEL, m_name, fmt, args);
  va_end(args);
}

void Ndb_component::log_warning(const char *fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_WARNING_LEVEL, m_name, fmt, args);
  va_end(args);
}

void Ndb_component::log_info(const char *fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, m_name, fmt, args);
  va_end(args);
}
