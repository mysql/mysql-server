/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_PSI_INCLUDED
#define PLUGIN_PSI_INCLUDED

#include "plugin_server_include.h"

#ifdef HAVE_PSI_INTERFACE

/*
  Register the psi keys for mutexes

  @param[in]  mutexes        PSI mutex info
  @param[in]  mutex_count    The number of elements in mutexes
*/
void register_group_replication_mutex_psi_keys(PSI_mutex_info mutexes[],
                                               size_t mutex_count);

/*
  Register the psi keys for conditions

  @param[in]  conditions     PSI condition info
  @param[in]  cond_count     The number of elements in conditions

*/
void register_group_replication_cond_psi_keys(PSI_cond_info conditions[],
                                               size_t cond_count);

/*
  Register the psi keys for threads

  @param[in]  threads        PSI thread info
  @param[in]  thread_count   The number of elements in threads
*/
void register_group_replication_thread_psi_keys(PSI_thread_info threads[],
                                                size_t thread_count);

/*
  Register the psi keys for rwlocks

  @param[in]  keys           PSI rwlock info
  @param[in]  count          The number of elements in keys
*/
void register_group_replication_rwlock_psi_keys(PSI_rwlock_info *keys,
                                                size_t count);


/*
  Register the psi keys for mutexes, conditions, threads and rwlocks

*/
void register_all_group_replication_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

extern PSI_mutex_key
              key_GR_LOCK_applier_module_run,
              key_GR_LOCK_applier_module_suspend,
              key_GR_LOCK_cert_broadcast_run,
              key_GR_LOCK_cert_broadcast_dispatcher_run,
              key_GR_LOCK_certification_info,
              key_GR_LOCK_cert_members,
              key_GR_LOCK_channel_observation_list,
              key_GR_LOCK_delayed_init_run,
              key_GR_LOCK_delayed_init_server_ready,
              key_GR_LOCK_group_part_handler_run,
              key_GR_LOCK_group_part_handler_abort,
              key_GR_LOCK_view_modification_wait,
              key_GR_LOCK_group_info_manager,
              key_GR_LOCK_pipeline_continuation,
              key_GR_LOCK_synchronized_queue,
              key_GR_LOCK_count_down_latch,
              key_GR_LOCK_wait_ticket,
              key_GR_LOCK_read_mode,
              key_GR_LOCK_recovery_module_run,
              key_GR_LOCK_recovery,
              key_GR_LOCK_recovery_donor_selection,
              key_GR_LOCK_session_thread_method_exec,
              key_GR_LOCK_session_thread_run,
              key_GR_LOCK_plugin_running,
              key_GR_LOCK_force_members_running,
              key_GR_LOCK_write_lock_protection,
              key_GR_LOCK_pipeline_stats_flow_control,
              key_GR_LOCK_pipeline_stats_transactions_waiting_apply,
              key_GR_LOCK_trx_unlocking;

extern PSI_cond_key
              key_GR_COND_applier_module_run,
              key_GR_COND_applier_module_suspend,
              key_GR_COND_applier_module_wait,
              key_GR_COND_cert_broadcast_run,
              key_GR_COND_cert_broadcast_dispatcher_run,
              key_GR_COND_delayed_init_run,
              key_GR_COND_delayed_init_server_ready,
              key_GR_COND_group_part_handler_run,
              key_GR_COND_group_part_handler_abort,
              key_GR_COND_view_modification_wait,
              key_GR_COND_pipeline_continuation,
              key_GR_COND_synchronized_queue,
              key_GR_COND_count_down_latch,
              key_GR_COND_wait_ticket,
              key_GR_COND_recovery_module_run,
              key_GR_COND_recovery,
              key_GR_COND_session_thread_method_exec,
              key_GR_COND_session_thread_run,
              key_GR_COND_pipeline_stats_flow_control;

extern PSI_thread_key
               key_GR_THD_applier_module_receiver,
               key_GR_THD_cert_broadcast,
               key_GR_THD_delayed_init,
               key_GR_THD_plugin_session,
               key_GR_THD_group_partition_handler,
               key_GR_THD_recovery;

extern PSI_rwlock_key
               key_GR_RWLOCK_cert_stable_gtid_set,
               key_GR_RWLOCK_io_cache_unused_list,
               key_GR_RWLOCK_plugin_stop,
               key_GR_RWLOCK_gcs_operations,
               key_GR_RWLOCK_gcs_operations_finalize_ongoing;

#endif /* PLUGIN_PSI_INCLUDED */
