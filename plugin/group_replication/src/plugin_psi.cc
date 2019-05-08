/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/mysql_thread.h"

/* clang-format off */
PSI_mutex_key key_GR_LOCK_applier_module_run,
    key_GR_LOCK_applier_module_suspend,
    key_GR_LOCK_autorejoin_module,
    key_GR_LOCK_cert_broadcast_run,
    key_GR_LOCK_cert_broadcast_dispatcher_run,
    key_GR_LOCK_certification_info,
    key_GR_LOCK_cert_members,
    key_GR_LOCK_channel_observation_list,
    key_GR_LOCK_channel_observation_removal,
    key_GR_LOCK_count_down_latch,
    key_GR_LOCK_delayed_init_run,
    key_GR_LOCK_delayed_init_server_ready,
    key_GR_LOCK_force_members_running,
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
    key_GR_LOCK_plugin_online, key_GR_LOCK_plugin_running,
    key_GR_LOCK_primary_election_action_phase,
    key_GR_LOCK_primary_election_action_notification,
    key_GR_LOCK_primary_election_primary_process_run,
    key_GR_LOCK_primary_election_running_flag,
    key_GR_LOCK_primary_election_secondary_process_run,
    key_GR_LOCK_primary_election_validation_notification,
    key_GR_LOCK_primary_promotion_policy,
    key_GR_LOCK_recovery,
    key_GR_LOCK_recovery_donor_selection,
    key_GR_LOCK_recovery_module_run,
    key_GR_LOCK_server_ongoing_transaction_handler,
    key_GR_LOCK_session_thread_method_exec,
    key_GR_LOCK_session_thread_run,
    key_GR_LOCK_stage_monitor_handler,
    key_GR_LOCK_synchronized_queue,
    key_GR_LOCK_trx_unlocking,
    key_GR_LOCK_group_member_info_manager_update_lock,
    key_GR_LOCK_group_member_info_update_lock,
    key_GR_LOCK_view_modification_wait,
    key_GR_LOCK_wait_ticket,
    key_GR_LOCK_write_lock_protection;

PSI_cond_key key_GR_COND_applier_module_run,
    key_GR_COND_applier_module_suspend,
    key_GR_COND_applier_module_wait,
    key_GR_COND_autorejoin_module,
    key_GR_COND_cert_broadcast_dispatcher_run,
    key_GR_COND_cert_broadcast_run,
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
    key_GR_COND_primary_promotion_policy,
    key_GR_COND_recovery,
    key_GR_COND_recovery_module_run,
    key_GR_COND_session_thread_method_exec,
    key_GR_COND_session_thread_run,
    key_GR_COND_synchronized_queue,
    key_GR_COND_view_modification_wait,
    key_GR_COND_wait_ticket,
    key_GR_COND_write_lock_protection;

PSI_thread_key key_GR_THD_applier_module_receiver,
    key_GR_THD_autorejoin,
    key_GR_THD_cert_broadcast,
    key_GR_THD_delayed_init,
    key_GR_THD_group_action_coordinator,
    key_GR_THD_plugin_session,
    key_GR_THD_primary_election_primary_process,
    key_GR_THD_primary_election_secondary_process,
    key_GR_THD_group_partition_handler,
    key_GR_THD_recovery;

PSI_rwlock_key key_GR_RWLOCK_cert_stable_gtid_set,
    key_GR_RWLOCK_channel_observation_list,
    key_GR_RWLOCK_gcs_operations,
    key_GR_RWLOCK_gcs_operations_finalize_ongoing,
    key_GR_RWLOCK_gcs_operations_view_change_observers,
    key_GR_RWLOCK_group_event_observation_list,
    key_GR_RWLOCK_io_cache_unused_list,
    key_GR_RWLOCK_plugin_stop,
    key_GR_RWLOCK_transaction_observation_list,
    key_GR_RWLOCK_transaction_consistency_manager_map,
    key_GR_RWLOCK_transaction_consistency_manager_prepared_transactions_on_my_applier;
/* clang-format on */

#ifdef HAVE_PSI_INTERFACE

PSI_stage_info info_GR_STAGE_autorejoin = {
    0, "Undergoing auto-rejoin procedure", PSI_FLAG_STAGE_PROGRESS,
    PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_multi_primary_mode_switch_pending_transactions = {
    0, "Multi-primary Switch: waiting for pending transactions to finish",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_multi_primary_mode_switch_step_completion = {
    0, "Multi-primary Switch: waiting on another member step completion",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_multi_primary_mode_switch_buffered_transactions = {
    0, "Multi-primary Switch: applying buffered transactions",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_multi_primary_mode_switch_completion = {
    0, "Multi-primary Switch: waiting for operation to complete on all members",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_election_buffered_transactions = {
    0, "Primary Election: applying buffered transactions",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_election_pending_transactions = {
    0, "Primary Election: waiting on current primary transaction execution",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_election_group_read_only = {
    0, "Primary Election: waiting for members to enable super_read_only",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_election_old_primary_transactions = {
    0, "Primary Election: stabilizing transactions from former primaries",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_switch_checks = {
    0, "Primary switch: checking current primary pre-conditions",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_switch_pending_transactions = {
    0, "Primary Switch: waiting for pending transactions to finish",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_switch_election = {
    0, "Primary Switch: executing Primary election", PSI_FLAG_STAGE_PROGRESS,
    PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_switch_step_completion = {
    0, "Primary Switch: waiting on another member step completion",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_primary_switch_completion = {
    0, "Primary Switch: waiting for operation to complete on all members",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_single_primary_mode_switch_checks = {
    0, "Single-primary Switch: checking group pre-conditions",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_single_primary_mode_switch_election = {
    0, "Single-primary Switch: executing Primary election",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_single_primary_mode_switch_completion = {
    0,
    "Single-primary Switch: waiting for operation to complete on all members",
    PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};

PSI_stage_info info_GR_STAGE_module_executing = {
    0, "Group Replication Module: Executing", 0, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_module_suspending = {
    0, "Group Replication Module: Suspending", 0, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_recovery_connecting_to_donor = {
    0, "Group Replication Recovery: Connecting to donor", 0, PSI_DOCUMENT_ME};
PSI_stage_info info_GR_STAGE_recovery_transferring_state = {
    0, "Group Replication Recovery: Transferring state from donor", 0,
    PSI_DOCUMENT_ME};

static PSI_mutex_info all_group_replication_psi_mutex_keys[] = {
    {&key_GR_LOCK_applier_module_run, "LOCK_applier_module_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_applier_module_suspend, "LOCK_applier_module_suspend",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_autorejoin_module, "LOCK_autorejoin_module",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_cert_broadcast_run, "LOCK_certifier_broadcast_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_cert_broadcast_dispatcher_run,
     "LOCK_certifier_broadcast_dispatcher_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_certification_info, "LOCK_certification_info",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_cert_members, "LOCK_certification_members",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_channel_observation_list, "LOCK_channel_observation_list",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_channel_observation_removal,
     "LOCK_channel_observation_removal", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_count_down_latch, "LOCK_count_down_latch", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_delayed_init_run, "LOCK_delayed_init_run", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_delayed_init_server_ready, "LOCK_delayed_init_server_ready",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_force_members_running, "LOCK_force_members_running",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_action_coordinator_process,
     "key_GR_LOCK_group_action_coordinator_process", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_action_coordinator_thread,
     "key_GR_LOCK_group_action_coordinator_thread", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_action_coordinator_thread_end,
     "key_GR_LOCK_group_action_coordinator_thread_end", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_info_manager, "LOCK_group_info_manager",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_part_handler_abort,
     "key_GR_LOCK_group_part_handler_abort", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_part_handler_run, "key_GR_LOCK_group_part_handler_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_multi_primary_action_notification,
     "LOCK_multi_primary_action_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_pipeline_continuation, "LOCK_pipeline_continuation",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_pipeline_stats_flow_control,
     "LOCK_pipeline_stats_flow_control", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_pipeline_stats_transactions_waiting_apply,
     "LOCK_pipeline_stats_transactions_waiting_apply", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_plugin_modules_termination, "LOCK_plugin_modules_termination",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_plugin_online, "LOCK_plugin_online", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_plugin_running, "LOCK_plugin_running", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_action_phase,
     "LOCK_primary_election_action_phase", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_action_notification,
     "LOCK_primary_election_action_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_primary_process_run,
     "LOCK_primary_election_primary_process_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_running_flag,
     "LOCK_primary_election_running_flag", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_secondary_process_run,
     "LOCK_primary_election_secondary_process_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_election_validation_notification,
     "LOCK_primary_election_validation_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_primary_promotion_policy, "LOCK_primary_promotion_policy",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_recovery, "LOCK_recovery", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_recovery_donor_selection, "LOCK_recovery_donor_selection",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_recovery_module_run, "LOCK_recovery_module_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_server_ongoing_transaction_handler,
     "LOCK_server_ongoing_transaction_handler", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_session_thread_method_exec, "LOCK_session_thread_method_exec",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_session_thread_run, "LOCK_session_thread_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_stage_monitor_handler, "LOCK_stage_monitor_handler",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_synchronized_queue, "LOCK_synchronized_queue",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_trx_unlocking, "LOCK_transaction_unblocking",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_member_info_manager_update_lock,
     "LOCK_group_member_info_manager_update_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_group_member_info_update_lock,
     "LOCK_group_member_info_update_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_view_modification_wait, "LOCK_view_modification_wait",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_LOCK_wait_ticket, "LOCK_wait_ticket", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_LOCK_write_lock_protection, "LOCK_write_lock_protection",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

static PSI_cond_info all_group_replication_psi_condition_keys[] = {
    {&key_GR_COND_applier_module_run, "COND_applier_module_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_applier_module_suspend, "COND_applier_module_suspend",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_applier_module_wait, "COND_applier_module_wait",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_cert_broadcast_run, "COND_certifier_broadcast_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_cert_broadcast_dispatcher_run,
     "COND_certifier_broadcast_dispatcher_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_delayed_init_run, "COND_delayed_init_run", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_COND_delayed_init_server_ready, "COND_delayed_init_server_ready",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_group_action_coordinator_process,
     "COND_group_action_coordinator_process", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_group_action_coordinator_thread,
     "COND_group_action_coordinator_thread", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_group_action_coordinator_thread_end,
     "COND_group_action_coordinator_thread_end", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_group_part_handler_run, "COND_group_part_handler_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_group_part_handler_abort, "COND_group_part_handler_abort",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_multi_primary_action_notification,
     "COND_multi_primary_action_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_view_modification_wait, "COND_view_modification_wait",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_pipeline_continuation, "COND_pipeline_continuation",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_synchronized_queue, "COND_synchronized_queue",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_count_down_latch, "COND_count_down_latch", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_COND_wait_ticket, "COND_wait_ticket", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_recovery_module_run, "COND_recovery_module_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_recovery, "COND_recovery", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_session_thread_method_exec, "COND_session_thread_method_exec",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_session_thread_run, "COND_session_thread_run",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_pipeline_stats_flow_control,
     "COND_pipeline_stats_flow_control", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_primary_election_action_notification,
     "COND_primary_election_action_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_primary_election_primary_process_run,
     "COND_primary_election_primary_process_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_primary_election_secondary_process_run,
     "COND_primary_election_secondary_process_run", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_primary_election_validation_notification,
     "COND_primary_election_validation_notification", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_write_lock_protection, "COND_write_lock_protection",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_plugin_online, "COND_plugin_online", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_COND_primary_promotion_policy, "COND_primary_promotion_policy",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_COND_autorejoin_module, "COND_autorejoin_module",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

static PSI_thread_info all_group_replication_psi_thread_keys[] = {
    {&key_GR_THD_applier_module_receiver, "THD_applier_module_receiver",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_THD_cert_broadcast, "THD_certifier_broadcast", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_THD_delayed_init, "THD_delayed_initialization", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_THD_group_action_coordinator, "THD_group_action_coordinator",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_THD_plugin_session, "THD_plugin_server_session",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_THD_primary_election_primary_process,
     "THD_primary_election_primary_process", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_THD_primary_election_secondary_process,
     "THD_primary_election_secondary_process", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_THD_group_partition_handler, "THD_group_partition_handler",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_THD_recovery, "THD_recovery", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_THD_autorejoin, "THD_autorejoin", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static PSI_rwlock_info all_group_replication_psi_rwlock_keys[] = {
    {&key_GR_RWLOCK_cert_stable_gtid_set, "RWLOCK_certifier_stable_gtid_set",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_channel_observation_list, "RWLOCK_channel_observation_list",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_gcs_operations, "RWLOCK_gcs_operations", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_gcs_operations_finalize_ongoing,
     "RWLOCK_gcs_operations_finalize_ongoing", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_gcs_operations_view_change_observers,
     "RWLOCK_gcs_operations_view_change_observers", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_group_event_observation_list,
     "RWLOCK_group_event_observation_list", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_io_cache_unused_list, "RWLOCK_io_cache_unused_list",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_plugin_stop, "RWLOCK_plugin_stop", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_transaction_observation_list,
     "RWLOCK_transaction_observation_list", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_transaction_consistency_manager_map,
     "RWLOCK_transaction_consistency_manager_map", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_GR_RWLOCK_transaction_consistency_manager_prepared_transactions_on_my_applier,
     "RWLOCK_transaction_consistency_manager_prepared_transactions_on_my_"
     "applier",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

static PSI_stage_info *all_group_replication_stages_keys[] = {
    &info_GR_STAGE_autorejoin,
    &info_GR_STAGE_multi_primary_mode_switch_pending_transactions,
    &info_GR_STAGE_multi_primary_mode_switch_step_completion,
    &info_GR_STAGE_multi_primary_mode_switch_buffered_transactions,
    &info_GR_STAGE_multi_primary_mode_switch_completion,
    &info_GR_STAGE_primary_election_buffered_transactions,
    &info_GR_STAGE_primary_election_pending_transactions,
    &info_GR_STAGE_primary_election_group_read_only,
    &info_GR_STAGE_primary_election_old_primary_transactions,
    &info_GR_STAGE_primary_switch_checks,
    &info_GR_STAGE_primary_switch_pending_transactions,
    &info_GR_STAGE_primary_switch_election,
    &info_GR_STAGE_primary_switch_step_completion,
    &info_GR_STAGE_primary_switch_completion,
    &info_GR_STAGE_single_primary_mode_switch_checks,
    &info_GR_STAGE_single_primary_mode_switch_election,
    &info_GR_STAGE_single_primary_mode_switch_completion,
    &info_GR_STAGE_module_executing,
    &info_GR_STAGE_module_suspending,
    &info_GR_STAGE_recovery_connecting_to_donor,
    &info_GR_STAGE_recovery_transferring_state};

void register_group_replication_mutex_psi_keys(PSI_mutex_info mutexes[],
                                               size_t mutex_count) {
  const char *category = "group_rpl";
  if (mutexes != NULL) {
    mysql_mutex_register(category, mutexes, static_cast<int>(mutex_count));
  }
}

void register_group_replication_cond_psi_keys(PSI_cond_info conds[],
                                              size_t cond_count) {
  const char *category = "group_rpl";
  if (conds != NULL) {
    mysql_cond_register(category, conds, static_cast<int>(cond_count));
  }
}

void register_group_replication_thread_psi_keys(PSI_thread_info threads[],
                                                size_t thread_count) {
  const char *category = "group_rpl";
  if (threads != NULL) {
    mysql_thread_register(category, threads, static_cast<int>(thread_count));
  }
}

void register_group_replication_rwlock_psi_keys(PSI_rwlock_info *keys,
                                                size_t count) {
  const char *category = "group_rpl";
  mysql_rwlock_register(category, keys, static_cast<int>(count));
}

void register_group_replication_stage_psi_keys(
    PSI_stage_info **keys MY_ATTRIBUTE((unused)),
    size_t count MY_ATTRIBUTE((unused))) {
#ifdef HAVE_PSI_STAGE_INTERFACE
  const char *category = "group_rpl";
  mysql_stage_register(category, keys, static_cast<int>(count));
#endif
}

void register_all_group_replication_psi_keys() {
  register_group_replication_mutex_psi_keys(
      all_group_replication_psi_mutex_keys,
      array_elements(all_group_replication_psi_mutex_keys));
  register_group_replication_cond_psi_keys(
      all_group_replication_psi_condition_keys,
      array_elements(all_group_replication_psi_condition_keys));
  register_group_replication_thread_psi_keys(
      all_group_replication_psi_thread_keys,
      array_elements(all_group_replication_psi_thread_keys));
  register_group_replication_rwlock_psi_keys(
      all_group_replication_psi_rwlock_keys,
      array_elements(all_group_replication_psi_rwlock_keys));
  register_group_replication_stage_psi_keys(
      all_group_replication_stages_keys,
      array_elements(all_group_replication_stages_keys));
}

#endif
