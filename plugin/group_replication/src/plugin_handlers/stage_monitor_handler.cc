/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,git
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

#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include <include/mysql/components/services/psi_stage.h>
#include "plugin/group_replication/include/plugin.h"

Plugin_stage_monitor_handler::Plugin_stage_monitor_handler()
    : generic_service(nullptr),
      stage_progress_handler(nullptr),
      service_running(false) {
  mysql_mutex_init(key_GR_LOCK_stage_monitor_handler, &stage_monitor_lock,
                   MY_MUTEX_INIT_FAST);
}

Plugin_stage_monitor_handler::~Plugin_stage_monitor_handler() {
  mysql_mutex_destroy(&stage_monitor_lock);
}

int Plugin_stage_monitor_handler::terminate_stage_monitor() {
  end_stage();

  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  if (!service_running) {
    return 0; /* purecov: inspected */
  }

  service_running = false;

  SERVICE_TYPE(registry) *registry = NULL;
  if (!registry_module ||
      !(registry = registry_module->get_registry_handle())) {
    DBUG_ASSERT(0); /* purecov: inspected */
    return 1;       /* purecov: inspected */
  }
  registry->release(generic_service);

  return 0;
}

int Plugin_stage_monitor_handler::initialize_stage_monitor() {
  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  DBUG_ASSERT(!service_running);

  SERVICE_TYPE(registry) *registry = NULL;
  if (!registry_module ||
      !(registry = registry_module->get_registry_handle())) {
    return 1; /* purecov: inspected */
  }
  if (registry->acquire("psi_stage_v1.performance_schema", &generic_service))
    return 1; /* purecov: inspected */

  service_running = true;

  return 0;
}

int Plugin_stage_monitor_handler::set_stage(PSI_stage_key key, const char *file,
                                            int line, ulonglong estimated_work,
                                            ulonglong work_completed) {
  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  if (!service_running || key <= 0) {
    return 0; /* purecov: inspected */
  }

  SERVICE_TYPE(psi_stage_v1) * stage_service;
  stage_service =
      reinterpret_cast<SERVICE_TYPE(psi_stage_v1) *>(generic_service);
  stage_progress_handler = stage_service->start_stage(key, file, line);

  if (!stage_progress_handler) {
    return 1; /* purecov: inspected */
  }

  stage_progress_handler->m_work_estimated = estimated_work;
  stage_progress_handler->m_work_completed = work_completed;

  return 0;
}

void Plugin_stage_monitor_handler::set_estimated_work(
    ulonglong estimated_work) {
  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  if (!service_running) {
    return; /* purecov: inspected */
  }

  if (stage_progress_handler)
    stage_progress_handler->m_work_estimated = estimated_work;
}

void Plugin_stage_monitor_handler::set_completed_work(
    ulonglong work_completed) {
  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  if (!service_running) {
    return; /* purecov: inspected */
  }

  if (stage_progress_handler)
    stage_progress_handler->m_work_completed = work_completed;
}

void Plugin_stage_monitor_handler::end_stage() {
  Mutex_autolock auto_lock_mutex(&stage_monitor_lock);

  if (!service_running) {
    return; /* purecov: inspected */
  }

  SERVICE_TYPE(psi_stage_v1) * stage_service;
  stage_service =
      reinterpret_cast<SERVICE_TYPE(psi_stage_v1) *>(generic_service);
  stage_service->end_stage();
}