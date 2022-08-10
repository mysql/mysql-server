/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/thread/mysql_thread.h"
#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "sql/sql_class.h"

static void *launch_thread(void *arg) {
  Mysql_thread *handler = (Mysql_thread *)arg;
  handler->dispatcher();
  return nullptr;
}

void Mysql_thread_task::execute() { m_body->run(m_parameters); }

Mysql_thread::Mysql_thread(PSI_thread_key thread_key,
                           PSI_mutex_key run_mutex_key,
                           PSI_cond_key run_cond_key,
                           PSI_mutex_key dispatcher_mutex_key,
                           PSI_cond_key dispatcher_cond_key)
    : m_thread_key(thread_key),
      m_mutex_key(run_mutex_key),
      m_cond_key(run_cond_key),
      m_dispatcher_mutex_key(dispatcher_mutex_key),
      m_dispatcher_cond_key(dispatcher_cond_key),
      m_state(),
      m_aborted(false),
      m_trigger_run_complete(false) {
  mysql_mutex_init(m_mutex_key, &m_run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(m_cond_key, &m_run_cond);
  mysql_mutex_init(m_dispatcher_mutex_key, &m_dispatcher_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(m_dispatcher_cond_key, &m_dispatcher_cond);
  m_trigger_queue = new Abortable_synchronized_queue<Mysql_thread_task *>(
      key_mysql_thread_queued_task);
}

Mysql_thread::~Mysql_thread() {
  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
  mysql_mutex_destroy(&m_dispatcher_lock);
  mysql_cond_destroy(&m_dispatcher_cond);

  if (nullptr != m_trigger_queue) {
    while (m_trigger_queue->size() > 0) {
      /* purecov: begin inspected */
      Mysql_thread_task *task = nullptr;
      m_trigger_queue->pop(&task);
      delete task;
      /* purecov: end */
    }
  }
  delete m_trigger_queue;
}

bool Mysql_thread::initialize() {
  DBUG_TRACE;

  mysql_mutex_lock(&m_run_lock);
  if (m_state.is_thread_alive()) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&m_run_lock);
    return false;
    /* purecov: end */
  }

  m_aborted = false;

  if ((mysql_thread_create(m_thread_key, &m_pthd, get_connection_attrib(),
                           launch_thread, (void *)this))) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&m_run_lock);
    return true;
    /* purecov: end */
  }
  m_state.set_created();

  while (m_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for Mysql_thread to start"));
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }
  mysql_mutex_unlock(&m_run_lock);

  return false;
}

bool Mysql_thread::terminate() {
  DBUG_TRACE;

  mysql_mutex_lock(&m_run_lock);
  if (m_state.is_thread_dead()) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&m_run_lock);
    return false;
    /* purecov: end */
  }

  m_aborted = true;
  m_trigger_queue->abort();

  while (m_state.is_thread_alive()) {
    DBUG_PRINT("sleep", ("Waiting for Mysql_thread to stop"));
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }
  mysql_mutex_unlock(&m_run_lock);

  mysql_mutex_lock(&m_dispatcher_lock);
  m_trigger_run_complete = true;
  mysql_cond_broadcast(&m_dispatcher_cond);
  mysql_mutex_unlock(&m_dispatcher_lock);

  return false;
}

void Mysql_thread::dispatcher() {
  DBUG_TRACE;

  // Thread context operations
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  // Needed to start replication threads
  thd->security_context()->skip_grants();
  global_thd_manager_add_thd(thd);
  m_thd = thd;

  mysql_mutex_lock(&m_run_lock);
  m_state.set_running();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  while (!m_aborted) {
    if (thd->killed) {
      break;
    }

    DBUG_EXECUTE_IF("group_replication_mysql_thread_dispatcher_before_pop", {
      Mysql_thread_task *t = nullptr;
      m_trigger_queue->front(&t);
      const char act[] =
          "now signal "
          "signal.group_replication_mysql_thread_dispatcher_before_pop_"
          "reached "
          "wait_for "
          "signal.group_replication_mysql_thread_dispatcher_before_pop_"
          "continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    Mysql_thread_task *task = nullptr;
    if (m_trigger_queue->pop(&task)) {
      break;
    }

    task->execute();

    mysql_mutex_lock(&m_dispatcher_lock);
    m_trigger_run_complete = true;
    mysql_cond_broadcast(&m_dispatcher_cond);
    mysql_mutex_unlock(&m_dispatcher_lock);
  }

  mysql_mutex_lock(&m_run_lock);
  m_aborted = true;
  m_trigger_queue->abort();
  mysql_mutex_unlock(&m_run_lock);

  mysql_mutex_lock(&m_dispatcher_lock);
  m_trigger_run_complete = true;
  mysql_cond_broadcast(&m_dispatcher_cond);
  mysql_mutex_unlock(&m_dispatcher_lock);

  thd->release_resources();
  global_thd_manager_remove_thd(thd);
  delete thd;
  m_thd = nullptr;
  my_thread_end();

  mysql_mutex_lock(&m_run_lock);
  m_state.set_terminated();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  my_thread_exit(nullptr);
}

bool Mysql_thread::trigger(Mysql_thread_task *task) {
  DBUG_TRACE;

  mysql_mutex_lock(&m_dispatcher_lock);
  if (m_trigger_queue->push(task)) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&m_dispatcher_lock);
    return true;
    /* purecov: end */
  }

  m_trigger_run_complete = false;
  while (!m_trigger_run_complete) {
    DBUG_PRINT("sleep", ("Waiting for Mysql_thread to complete a trigger run"));
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_dispatcher_cond, &m_dispatcher_lock, &abstime);
  }
  mysql_mutex_unlock(&m_dispatcher_lock);

  return false;
}
