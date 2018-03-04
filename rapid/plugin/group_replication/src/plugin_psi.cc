/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include "plugin/group_replication/include/plugin_psi.h"

#include <stddef.h>

PSI_mutex_key  key_GR_LOCK_applier_module_run,
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

PSI_cond_key   key_GR_COND_applier_module_run,
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
               key_GR_COND_pipeline_stats_flow_control,
               key_GR_COND_write_lock_protection;

PSI_thread_key key_GR_THD_applier_module_receiver,
               key_GR_THD_cert_broadcast,
               key_GR_THD_delayed_init,
               key_GR_THD_plugin_session,
               key_GR_THD_group_partition_handler,
               key_GR_THD_recovery;

PSI_rwlock_key key_GR_RWLOCK_cert_stable_gtid_set,
               key_GR_RWLOCK_io_cache_unused_list,
               key_GR_RWLOCK_plugin_stop,
               key_GR_RWLOCK_gcs_operations,
               key_GR_RWLOCK_gcs_operations_finalize_ongoing;

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_info all_group_replication_psi_mutex_keys[]=
{
  {&key_GR_LOCK_applier_module_run, "LOCK_applier_module_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_applier_module_suspend, "LOCK_applier_module_suspend", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_cert_broadcast_run, "LOCK_certifier_broadcast_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_cert_broadcast_dispatcher_run, "LOCK_certifier_broadcast_dispatcher_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_certification_info, "LOCK_certification_info", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_cert_members, "LOCK_certification_members", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_channel_observation_list, "LOCK_channel_observation_list", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_delayed_init_run, "LOCK_delayed_init_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_delayed_init_server_ready, "LOCK_delayed_init_server_ready", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_group_part_handler_run, "key_GR_LOCK_group_part_handler_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_group_part_handler_abort, "key_GR_LOCK_group_part_handler_abort", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_view_modification_wait, "LOCK_view_modification_wait", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_group_info_manager, "LOCK_group_info_manager", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_pipeline_continuation, "LOCK_pipeline_continuation", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_synchronized_queue, "LOCK_synchronized_queue", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_count_down_latch, "LOCK_count_down_latch", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_wait_ticket, "LOCK_wait_ticket", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_recovery_module_run, "LOCK_recovery_module_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_recovery, "LOCK_recovery", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_recovery_donor_selection, "LOCK_recovery_donor_selection", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_session_thread_method_exec, "LOCK_session_thread_method_exec", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_session_thread_run, "LOCK_session_thread_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_plugin_running, "LOCK_plugin_running", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_force_members_running, "LOCK_force_members_running", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_write_lock_protection, "LOCK_write_lock_protection", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_pipeline_stats_flow_control, "LOCK_pipeline_stats_flow_control", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_pipeline_stats_transactions_waiting_apply, "LOCK_pipeline_stats_transactions_waiting_apply", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_trx_unlocking, "LOCK_transaction_unblocking", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};

static PSI_cond_info all_group_replication_psi_condition_keys[]=
{
  {&key_GR_COND_applier_module_run, "COND_applier_module_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_applier_module_suspend,  "COND_applier_module_suspend", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_applier_module_wait, "COND_applier_module_wait", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_cert_broadcast_run, "COND_certifier_broadcast_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_cert_broadcast_dispatcher_run, "COND_certifier_broadcast_dispatcher_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_delayed_init_run,  "COND_delayed_init_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_delayed_init_server_ready, "COND_delayed_init_server_ready", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_group_part_handler_run, "COND_group_part_handler_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_LOCK_group_part_handler_abort, "COND_group_part_handler_abort", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_view_modification_wait, "COND_view_modification_wait", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_pipeline_continuation, "COND_pipeline_continuation", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_synchronized_queue, "COND_synchronized_queue", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_count_down_latch, "COND_count_down_latch", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_wait_ticket, "COND_wait_ticket", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_recovery_module_run, "COND_recovery_module_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_recovery, "COND_recovery", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_session_thread_method_exec, "COND_session_thread_method_exec", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_session_thread_run, "COND_session_thread_run", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_pipeline_stats_flow_control, "COND_pipeline_stats_flow_control", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_COND_write_lock_protection, "COND_write_lock_protection", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
};

static PSI_thread_info all_group_replication_psi_thread_keys[]=
{
  {&key_GR_THD_applier_module_receiver, "THD_applier_module_receiver", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_THD_cert_broadcast, "THD_certifier_broadcast", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_THD_delayed_init, "THD_delayed_initialization", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_THD_plugin_session, "THD_plugin_server_session", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_THD_group_partition_handler, "THD_group_partition_handler", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_THD_recovery, "THD_recovery", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};

static PSI_rwlock_info all_group_replication_psi_rwlock_keys[]=
{
  {&key_GR_RWLOCK_cert_stable_gtid_set, "RWLOCK_certifier_stable_gtid_set", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_RWLOCK_io_cache_unused_list , "RWLOCK_io_cache_unused_list", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_RWLOCK_plugin_stop, "RWLOCK_plugin_stop", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_RWLOCK_gcs_operations, "RWLOCK_gcs_operations", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GR_RWLOCK_gcs_operations_finalize_ongoing, "RWLOCK_gcs_operations_finalize_ongoing", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};

void register_group_replication_mutex_psi_keys(PSI_mutex_info mutexes[],
                                               size_t mutex_count)
{
  const char* category= "group_rpl";
  if (mutexes != NULL)
  {
    mysql_mutex_register(category, mutexes, static_cast<int>(mutex_count));
  }
}

void register_group_replication_cond_psi_keys(PSI_cond_info conds[],
                                               size_t cond_count)
{
  const char* category= "group_rpl";
  if (conds != NULL)
  {
    mysql_cond_register(category, conds, static_cast<int>(cond_count));
  }
}

void register_group_replication_thread_psi_keys(PSI_thread_info threads[],
                                                size_t thread_count)
{
  const char* category= "group_rpl";
  if (threads != NULL)
  {
    mysql_thread_register(category, threads, static_cast<int>(thread_count));
  }
}

void register_group_replication_rwlock_psi_keys(PSI_rwlock_info *keys,
                                                size_t count)
{
  const char *category= "group_rpl";
  mysql_rwlock_register(category, keys, static_cast<int>(count));
}

void register_all_group_replication_psi_keys()
{
  register_group_replication_mutex_psi_keys(all_group_replication_psi_mutex_keys,
                                            array_elements(all_group_replication_psi_mutex_keys));
  register_group_replication_cond_psi_keys(all_group_replication_psi_condition_keys,
                                           array_elements(all_group_replication_psi_condition_keys));
  register_group_replication_thread_psi_keys(all_group_replication_psi_thread_keys,
                                             array_elements(all_group_replication_psi_thread_keys));
  register_group_replication_rwlock_psi_keys(all_group_replication_psi_rwlock_keys,
                                             array_elements(all_group_replication_psi_rwlock_keys));
}

#endif
