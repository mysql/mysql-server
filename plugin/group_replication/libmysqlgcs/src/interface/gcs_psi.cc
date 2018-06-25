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

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_psi.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"

#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_thread.h"

PSI_thread_key key_GCS_THD_Gcs_ext_logger_impl_m_consumer,
    key_GCS_THD_Gcs_xcom_engine_m_engine_thread,
    key_GCS_THD_Gcs_xcom_control_m_xcom_thread,
    key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread;

static PSI_thread_info all_gcs_psi_thread_keys_info[] = {
    {&key_GCS_THD_Gcs_ext_logger_impl_m_consumer,
     "THD_Gcs_ext_logger_impl::m_consumer", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_THD_Gcs_xcom_engine_m_engine_thread,
     "THD_Gcs_xcom_engine::m_engine_thread", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_THD_Gcs_xcom_control_m_xcom_thread,
     "THD_Gcs_xcom_control::m_xcom_thread", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread,
     "THD_Gcs_xcom_control::m_suspicions_processing_thread", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
};

PSI_mutex_key key_GCS_MUTEX_Gcs_async_buffer_m_free_buffer_mutex,
    key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_mutex,
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

PSI_cond_key key_GCS_COND_Gcs_async_buffer_m_wait_for_events_cond,
    key_GCS_COND_Gcs_async_buffer_m_free_buffer_cond,
    key_GCS_COND_Gcs_xcom_interface_m_wait_for_ssl_init_cond,
    key_GCS_COND_Gcs_xcom_engine_m_wait_for_notification_cond,
    key_GCS_COND_Gcs_xcom_view_change_control_m_wait_for_view_cond,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status,
    key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_exit;

static PSI_mutex_info all_gcs_psi_mutex_keys_info[] = {
    {&key_GCS_MUTEX_Gcs_async_buffer_m_free_buffer_mutex,
     "GCS_Gcs_async_buffer::m_free_buffer_mutex", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_mutex,
     "GCS_Gcs_suspicions_manager::m_suspicions_mutex", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_group_management_m_nodes_mutex,
     "GCS_Gcs_xcom_group_management::m_nodes_mutex", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_interface_m_wait_for_ssl_init_mutex,
     "GCS_Gcs_xcom_interface::m_wait_for_ssl_init_mutex", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_engine_m_wait_for_notification_mutex,
     "GCS_Gcs_xcom_engine::m_wait_for_notification_mutex", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_view_change_control_m_wait_for_view_mutex,
     "GCS_Gcs_xcom_view_change_control::m_wait_for_view_mutex",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_view_change_control_m_current_view_mutex,
     "GCS_Gcs_xcom_view_change_control::m_current_view_mutex",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_view_change_control_m_joining_leaving_mutex,
     "GCS_Gcs_xcom_view_change_control::m_joining_leaving_mutex",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_cursor,
     "GCS_Gcs_xcom_proxy_impl::m_lock_xcom_cursor", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
     "GCS_Gcs_xcom_proxy_impl::m_lock_xcom_ready", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status,
     "GCS_Gcs_xcom_proxy_impl::m_lock_xcom_comms_status", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
     "GCS_Gcs_xcom_proxy_impl::m_lock_xcom_exit", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_MUTEX_Xcom_handler_m_lock, "GCS_Xcom_handler::m_lock",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

static PSI_cond_info all_gcs_psi_cond_keys_info[] = {
    {&key_GCS_COND_Gcs_async_buffer_m_wait_for_events_cond,
     "GCS_Gcs_async_buffer::m_wait_for_events_cond", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_async_buffer_m_free_buffer_cond,
     "GCS_Gcs_async_buffer::m_free_buffer_cond", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_interface_m_wait_for_ssl_init_cond,
     "GCS_Gcs_xcom_interface::m_wait_for_ssl_init_cond", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_engine_m_wait_for_notification_cond,
     "GCS_Gcs_xcom_engine::m_wait_for_notification_cond", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_view_change_control_m_wait_for_view_cond,
     "GCS_Gcs_xcom_view_change_control::m_wait_for_view_cond",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready,
     "GCS_Gcs_xcom_proxy_impl::m_cond_xcom_ready", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status,
     "GCS_Gcs_xcom_proxy_impl::m_cond_xcom_comms_status", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_exit,
     "GCS_Gcs_xcom_proxy_impl::m_cond_xcom_exit", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

PSI_memory_key key_MEM_XCOM_xcom_cache;

static PSI_memory_info xcom_cache_memory_info[] = {
    {&key_MEM_XCOM_xcom_cache, "GCS_XCom::xcom_cache",
     PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_UNKNOWN,
     "Memory usage statistics for the XCom cache."}};

void register_gcs_thread_psi_keys() {
  const char *category = "group_rpl";
  int count = static_cast<int>(array_elements(all_gcs_psi_thread_keys_info));

  mysql_thread_register(category, all_gcs_psi_thread_keys_info, count);
}

void register_gcs_mutex_cond_psi_keys() {
  const char *category = "group_rpl";
  int count = static_cast<int>(array_elements(all_gcs_psi_cond_keys_info));

  mysql_cond_register(category, all_gcs_psi_cond_keys_info, count);

  count = static_cast<int>(array_elements(all_gcs_psi_mutex_keys_info));

  mysql_mutex_register(category, all_gcs_psi_mutex_keys_info, count);
}

void register_xcom_memory_psi_keys() {
  const char *category = "group_rpl";
  int count = (int)array_elements(xcom_cache_memory_info);

  mysql_memory_register(category, xcom_cache_memory_info, count);
}

/*
  Debug counter. Tracks the current allocated count (effectively mirroring
  the value shown in the "CURRENT_NUMBER_OF_BYTES_USED" row of the
  "performance_schema.memory_summary_global_by_event_name" table.
*/
static long long current_count = 0;

/**
  Reports to PSI the allocation of 'size' bytes of data.
*/
extern "C" int psi_report_mem_alloc(size_t size) {
#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_thread *owner = NULL;
  PSI_memory_key key = key_MEM_XCOM_xcom_cache;
  if (PSI_MEMORY_CALL(memory_alloc)(key, size, &owner) ==
      PSI_NOT_INSTRUMENTED) {
    return 0;
  }
  /* This instrument is flagged global, so there should be no thread owner. */
  DBUG_ASSERT(owner == NULL);
#endif /* HAVE_PSI_MEMORY_INTERFACE */
  current_count += size;
  return 1;
}

/**
  Reports to PSI the deallocation of 'size' bytes of data.
*/
extern "C" void psi_report_mem_free(size_t size, int is_instrumented) {
  if (!is_instrumented) {
    return;
  }
  current_count -= size;

#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_MEMORY_CALL(memory_free)(key_MEM_XCOM_xcom_cache, size, NULL);
#endif /* HAVE_PSI_MEMORY_INTERFACE */
}

/**
  After the cache is de-initialized 'current_count' must be zero; otherwise
  we have allocated data that has not been deallocated (or has not been
  reported as deallocated).
*/
extern "C" void psi_report_cache_shutdown() { DBUG_ASSERT(current_count == 0); }
