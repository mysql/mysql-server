/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"

#include <assert.h>
#include <stddef.h>

#include "my_compiler.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"

Finalize_notification::Finalize_notification(Gcs_xcom_engine *gcs_engine,
                                             xcom_finalize_functor *functor)
    : m_gcs_engine(gcs_engine), m_functor(functor) {}

Finalize_notification::~Finalize_notification() {}

void Finalize_notification::do_execute() {
  /*
    It will stop queueing notifications because we want to
    stop the engine and flush any possible notification in
    the queue. Note that after executing the callback, it
    may not be possible to do so because some objects may
    be destroyed.
  */
  m_gcs_engine->cleanup();

  /*
    For example, now it is safe to kill XCOM's thread if it
    has not been stopped already.
  */
  if (m_functor) (*m_functor)();
}

Initialize_notification::Initialize_notification(
    xcom_initialize_functor *functor)
    : m_functor(functor) {}

Initialize_notification::~Initialize_notification() {}

void Initialize_notification::do_execute() {
  if (m_functor) (*m_functor)();
}

Data_notification::Data_notification(xcom_receive_data_functor *functor,
                                     synode_no message_id,
                                     Gcs_xcom_nodes *xcom_nodes, u_int size,
                                     char *data)
    : m_functor(functor),
      m_message_id(message_id),
      m_xcom_nodes(xcom_nodes),
      m_size(size),
      m_data(data) {}

Data_notification::~Data_notification() {}

void Data_notification::do_execute() {
  (*m_functor)(m_message_id, m_xcom_nodes, m_size, m_data);
}

Status_notification::Status_notification(xcom_status_functor *functor,
                                         int status)
    : m_functor(functor), m_status(status) {}

Status_notification::~Status_notification() {}

void Status_notification::do_execute() { (*m_functor)(m_status); }

Global_view_notification::Global_view_notification(
    xcom_global_view_functor *functor, synode_no config_id,
    synode_no message_id, Gcs_xcom_nodes *xcom_nodes)

    : m_functor(functor),
      m_config_id(config_id),
      m_message_id(message_id),
      m_xcom_nodes(xcom_nodes) {}

Global_view_notification::~Global_view_notification() {}

void Global_view_notification::do_execute() {
  (*m_functor)(m_config_id, m_message_id, m_xcom_nodes);
}

Local_view_notification::Local_view_notification(
    xcom_local_view_functor *functor, synode_no message_id,
    Gcs_xcom_nodes *xcom_nodes)

    : m_functor(functor), m_message_id(message_id), m_xcom_nodes(xcom_nodes) {}

Local_view_notification::~Local_view_notification() {}

void Local_view_notification::do_execute() {
  (*m_functor)(m_message_id, m_xcom_nodes);
}

Expel_notification::Expel_notification(xcom_expel_functor *functor)
    : m_functor(functor) {}

Expel_notification::~Expel_notification() {}

void Expel_notification::do_execute() { (*m_functor)(); }

Control_notification::Control_notification(xcom_control_functor *functor,
                                           Gcs_control_interface *control_if)
    : m_functor(functor), m_control_if(control_if) {}

Control_notification::~Control_notification() {}

void Control_notification::do_execute() {
  static_cast<void>((*m_functor)(m_control_if));
}

void *process_notification_thread(void *ptr_object) {
  Gcs_xcom_engine *engine = static_cast<Gcs_xcom_engine *>(ptr_object);
  engine->process();

  My_xp_thread_util::exit(0);

  return NULL;
}

Gcs_xcom_engine::Gcs_xcom_engine()
    : m_wait_for_notification_cond(),
      m_wait_for_notification_mutex(),
      m_notification_queue(),
      m_engine_thread(),
      m_schedule(true) {
  m_wait_for_notification_cond.init(
      key_GCS_COND_Gcs_xcom_engine_m_wait_for_notification_cond);
  m_wait_for_notification_mutex.init(
      key_GCS_MUTEX_Gcs_xcom_engine_m_wait_for_notification_mutex, NULL);
}

Gcs_xcom_engine::~Gcs_xcom_engine() {
  m_wait_for_notification_cond.destroy();
  m_wait_for_notification_mutex.destroy();
}

void Gcs_xcom_engine::initialize(
    xcom_initialize_functor *functor MY_ATTRIBUTE((unused))) {
  assert(m_notification_queue.empty());
  assert(m_schedule);
  m_engine_thread.create(key_GCS_THD_Gcs_xcom_engine_m_engine_thread, NULL,
                         process_notification_thread, (void *)this);
}

void Gcs_xcom_engine::finalize(xcom_finalize_functor *functor) {
  push(new Finalize_notification(this, functor));
  m_engine_thread.join(NULL);
  assert(m_notification_queue.empty());
  assert(!m_schedule);
}

void Gcs_xcom_engine::process() {
  Gcs_xcom_notification *notification = NULL;
  bool stop = false;

  while (!stop) {
    m_wait_for_notification_mutex.lock();
    while (m_notification_queue.empty()) {
      m_wait_for_notification_cond.wait(
          m_wait_for_notification_mutex.get_native_mutex());
    }
    notification = m_notification_queue.front();
    m_notification_queue.pop();
    m_wait_for_notification_mutex.unlock();

    MYSQL_GCS_LOG_TRACE("Started executing during regular phase: %p",
                        notification)
    stop = (*notification)();
    MYSQL_GCS_LOG_TRACE("Finish executing during regular phase: %p",
                        notification)
    delete notification;
  }
}

void Gcs_xcom_engine::cleanup() {
  Gcs_xcom_notification *notification = NULL;

  m_wait_for_notification_mutex.lock();
  m_schedule = false;
  m_wait_for_notification_mutex.unlock();

  while (!m_notification_queue.empty()) {
    notification = m_notification_queue.front();
    m_notification_queue.pop();

    MYSQL_GCS_LOG_TRACE("Started executing during clean up phase: %p",
                        notification)
    (*notification)();
    MYSQL_GCS_LOG_TRACE("Finished executing during clean up phase: %p",
                        notification)

    delete notification;
  }
}

bool Gcs_xcom_engine::push(Gcs_xcom_notification *request) {
  bool scheduled = false;

  m_wait_for_notification_mutex.lock();
  if (m_schedule) {
    m_notification_queue.push(request);
    m_wait_for_notification_cond.broadcast();
    scheduled = true;
  }
  m_wait_for_notification_mutex.unlock();

  return scheduled;
}
