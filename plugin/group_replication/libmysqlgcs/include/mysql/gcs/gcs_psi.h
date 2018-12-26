/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_PSI_H
#define GCS_PSI_H

#include <my_sys.h>
#include "mysql/psi/psi_cond.h"
#include "mysql/psi/psi_mutex.h"
#include "mysql/psi/psi_thread.h"

extern PSI_mutex_key key_GCS_MUTEX_Gcs_async_buffer_m_free_buffer_mutex,
    key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_mutex,
    key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_parameters_mutex,
    key_GCS_MUTEX_Gcs_xcom_group_management_m_nodes_mutex,
    key_GCS_MUTEX_Gcs_xcom_interface_m_wait_for_ssl_init_mutex,
    key_GCS_MUTEX_Gcs_xcom_engine_m_wait_for_notification_mutex,
    key_GCS_MUTEX_Gcs_xcom_view_change_control_m_wait_for_view_mutex,
    key_GCS_MUTEX_Gcs_xcom_view_change_control_m_current_view_mutex,
    key_GCS_MUTEX_Gcs_xcom_view_change_control_m_joining_leaving_mutex,
    key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_cursor,
    key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
    key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status,
    key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
    key_GCS_MUTEX_Xcom_handler_m_lock;

extern PSI_cond_key key_GCS_COND_Gcs_async_buffer_m_wait_for_events_cond,
    key_GCS_COND_Gcs_async_buffer_m_free_buffer_cond,
    key_GCS_COND_Gcs_xcom_interface_m_wait_for_ssl_init_cond,
    key_GCS_COND_Gcs_xcom_engine_m_wait_for_notification_cond,
    key_GCS_COND_Gcs_xcom_view_change_control_m_wait_for_view_cond,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_exit,
    key_GCS_COND_Gcs_suspicions_manager_m_suspicions_cond;

extern PSI_thread_key key_GCS_THD_Gcs_ext_logger_impl_m_consumer,
    key_GCS_THD_Gcs_xcom_engine_m_engine_thread,
    key_GCS_THD_Gcs_xcom_control_m_xcom_thread,
    key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread;

extern PSI_memory_key key_MEM_XCOM_xcom_cache;

/**
  Registers the psi keys for the threads that will be instrumented.
*/

void register_gcs_thread_psi_keys();

/**
  Registers the psi keys for the mutexes and conds that will be instrumented.
*/

void register_gcs_mutex_cond_psi_keys();

/**
  Registers the psi keys for the memory operations that will be instrumented.
 */
void register_xcom_memory_psi_keys();

#endif /* GCS_PSI_H */
