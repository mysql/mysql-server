/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_PSI_INCLUDED
#define PLUGIN_PSI_INCLUDED

#include "plugin/group_replication/include/plugin_server_include.h"

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

/* clang-format off */

extern PSI_mutex_key key_GR_LOCK_applier_module_run,
    key_GR_LOCK_applier_module_suspend,
    key_GR_LOCK_autorejoin_module,
    key_GR_LOCK_cert_broadcast_run,
    key_GR_LOCK_cert_broadcast_dispatcher_run,
    key_GR_LOCK_certification_info,
    key_GR_LOCK_cert_members,
    key_GR_LOCK_channel_observation_list,
    key_GR_LOCK_channel_observation_removal,
    key_GR_LOCK_clone_donor_list,
    key_GR_LOCK_clone_handler_run,
    key_GR_LOCK_clone_query,
    key_GR_LOCK_clone_read_mode,
    key_GR_LOCK_count_down_latch,
    key_GR_LOCK_delayed_init_run,
    key_GR_LOCK_delayed_init_server_ready,
    key_GR_LOCK_group_action_coordinator_process,
    key_GR_LOCK_group_action_coordinator_thread,
    key_GR_LOCK_group_action_coordinator_thread_end,
    key_GR_LOCK_group_info_manager,
    key_GR_LOCK_group_part_handler_abort,
    key_GR_LOCK_group_part_handler_run,
    key_GR_LOCK_multi_primary_action_notification,
    key_GR_LOCK_pipeline_continuation,
    key_GR_LOCK_pipeline_stats_flow_control,
    key_GR_LOCK_pipeline_stats_transactions_waiting_apply,
    key_GR_LOCK_plugin_modules_termination,
    key_GR_LOCK_plugin_applier_module_initialize_terminate,
    key_GR_LOCK_plugin_online,
    key_GR_LOCK_primary_election_action_phase,
    key_GR_LOCK_primary_election_action_notification,
    key_GR_LOCK_primary_election_primary_process_run,
    key_GR_LOCK_primary_election_running_flag,
    key_GR_LOCK_primary_election_secondary_process_run,
    key_GR_LOCK_primary_election_validation_notification,
    key_GR_LOCK_recovery,
    key_GR_LOCK_recovery_donor_selection,
    key_GR_LOCK_recovery_metadata_receive,
    key_GR_LOCK_recovery_metadata_module_receive,
    key_GR_LOCK_recovery_module_run,
    key_GR_LOCK_server_ongoing_transaction_handler,
    key_GR_LOCK_message_service_run,
    key_GR_LOCK_session_thread_method_exec,
    key_GR_LOCK_session_thread_run,
    key_GR_LOCK_stage_monitor_handler,
    key_GR_LOCK_synchronized_queue,
    key_GR_LOCK_transaction_monitor_module,
    key_GR_LOCK_trx_unlocking,
    key_GR_LOCK_group_member_info_manager_update_lock,
    key_GR_LOCK_group_member_info_update_lock,
    key_GR_LOCK_view_modification_wait,
    key_GR_LOCK_wait_ticket,
    key_GR_LOCK_write_lock_protection,
    key_GR_LOCK_primary_promotion_policy,
    key_GR_LOCK_mysql_thread_run,
    key_GR_LOCK_mysql_thread_dispatcher_run,
    key_GR_LOCK_connection_map,
    key_GR_LOCK_mysql_thread_handler_run,
    key_GR_LOCK_mysql_thread_handler_dispatcher_run,
    key_GR_LOCK_mysql_thread_handler_read_only_mode_run,
    key_GR_LOCK_mysql_thread_handler_read_only_mode_dispatcher_run;

extern PSI_cond_key key_GR_COND_applier_module_run,
    key_GR_COND_applier_module_suspend,
    key_GR_COND_applier_module_wait,
    key_GR_COND_autorejoin_module,
    key_GR_COND_cert_broadcast_dispatcher_run,
    key_GR_COND_cert_broadcast_run,
    key_GR_COND_clone_handler_run,
    key_GR_COND_count_down_latch,
    key_GR_COND_delayed_init_run,
    key_GR_COND_delayed_init_server_ready,
    key_GR_COND_group_action_coordinator_process,
    key_GR_COND_group_action_coordinator_thread,
    key_GR_COND_group_action_coordinator_thread_end,
    key_GR_COND_group_part_handler_abort,
    key_GR_COND_group_part_handler_run,
    key_GR_COND_multi_primary_action_notification,
    key_GR_COND_pipeline_continuation,
    key_GR_COND_pipeline_stats_flow_control,
    key_GR_COND_plugin_online,
    key_GR_COND_primary_election_action_notification,
    key_GR_COND_primary_election_primary_process_run,
    key_GR_COND_primary_election_secondary_process_run,
    key_GR_COND_primary_election_validation_notification,
    key_GR_COND_recovery,
    key_GR_COND_recovery_metadata_receive,
    key_GR_COND_recovery_module_run,
    key_GR_COND_message_service_run,
    key_GR_COND_session_thread_method_exec,
    key_GR_COND_session_thread_run,
    key_GR_COND_synchronized_queue,
    key_GR_COND_transaction_monitor_module,
    key_GR_COND_view_modification_wait,
    key_GR_COND_wait_ticket,
    key_GR_COND_write_lock_protection,
    key_GR_COND_primary_promotion_policy,
    key_GR_COND_mysql_thread_run,
    key_GR_COND_mysql_thread_dispatcher_run,
    key_GR_COND_mysql_thread_handler_run,
    key_GR_COND_mysql_thread_handler_dispatcher_run,
    key_GR_COND_mysql_thread_handler_read_only_mode_run,
    key_GR_COND_mysql_thread_handler_read_only_mode_dispatcher_run;

extern PSI_thread_key key_GR_THD_applier_module_receiver,
    key_GR_THD_autorejoin,
    key_GR_THD_transaction_monitor,
    key_GR_THD_cert_broadcast,
    key_GR_THD_clone_thd,
    key_GR_THD_delayed_init,
    key_GR_THD_group_action_coordinator,
    key_GR_THD_plugin_session,
    key_GR_THD_primary_election_primary_process,
    key_GR_THD_primary_election_secondary_process,
    key_GR_THD_group_partition_handler,
    key_GR_THD_recovery,
    key_GR_THD_message_service_handler,
    key_GR_THD_mysql_thread,
    key_GR_THD_mysql_thread_handler,
    key_GR_THD_mysql_thread_handler_read_only_mode;

extern PSI_rwlock_key key_GR_RWLOCK_cert_stable_gtid_set,
    key_GR_RWLOCK_channel_observation_list,
    key_GR_RWLOCK_gcs_operations,
    key_GR_RWLOCK_gcs_operations_view_change_observers,
    key_GR_RWLOCK_group_event_observation_list,
    key_GR_RWLOCK_io_cache_unused_list,
    key_GR_RWLOCK_plugin_running,
    key_GR_RWLOCK_plugin_stop,
    key_GR_RWLOCK_transaction_observation_list,
    key_GR_RWLOCK_transaction_consistency_manager_map,
    key_GR_RWLOCK_transaction_consistency_manager_prepared_transactions_on_my_applier,
    key_GR_RWLOCK_flow_control_module_info,
    key_GR_RWLOCK_transaction_consistency_info_members_that_must_prepare_the_transaction;

extern PSI_stage_info info_GR_STAGE_autorejoin,
    info_GR_STAGE_multi_primary_mode_switch_pending_transactions,
    info_GR_STAGE_multi_primary_mode_switch_step_completion,
    info_GR_STAGE_multi_primary_mode_switch_buffered_transactions,
    info_GR_STAGE_multi_primary_mode_switch_completion,
    info_GR_STAGE_primary_election_buffered_transactions,
    info_GR_STAGE_primary_election_pending_transactions,
    info_GR_STAGE_primary_election_group_read_only,
    info_GR_STAGE_primary_election_old_primary_transactions,
    info_GR_STAGE_primary_switch_checks,
    info_GR_STAGE_primary_switch_pending_transactions,
    info_GR_STAGE_primary_switch_election,
    info_GR_STAGE_primary_switch_step_completion,
    info_GR_STAGE_primary_switch_completion,
    info_GR_STAGE_single_primary_mode_switch_checks,
    info_GR_STAGE_single_primary_mode_switch_election,
    info_GR_STAGE_single_primary_mode_switch_completion,
    info_GR_STAGE_module_executing,
    info_GR_STAGE_module_suspending,
    info_GR_STAGE_recovery_connecting_to_donor,
    info_GR_STAGE_recovery_transferring_state,
    info_GR_STAGE_clone_prepare,
    info_GR_STAGE_clone_execute;

extern PSI_memory_key key_write_set_encoded,
    key_certification_data,
    key_certification_data_gc,
    key_certification_info,
    key_transaction_data,
    key_sql_service_command_data,
    key_mysql_thread_queued_task,
    key_message_service_queue,
    key_message_service_received_message,
    key_group_member_info,
    key_consistent_members_that_must_prepare_transaction,
    key_consistent_transactions,
    key_consistent_transactions_prepared,
    key_consistent_transactions_waiting,
    key_consistent_transactions_delayed_view_change,
    key_compression_data,
    key_recovery_metadata_message_buffer;
/* clang-format on */

#endif /* PLUGIN_PSI_INCLUDED */
