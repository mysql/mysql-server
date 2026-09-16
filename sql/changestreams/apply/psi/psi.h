// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_SERVICE_PSI_H
#define MYSQL_CSA_SERVICE_PSI_H

#include <mysql/components/services/psi_cond_service.h>
#include <mysql/components/services/psi_memory_service.h>
#include <mysql/components/services/psi_mutex_service.h>
#include <mysql/components/services/psi_rwlock_service.h>
#include <mysql/components/services/psi_stage_service.h>
#include <mysql/components/services/psi_thread_service.h>

namespace mysql::csa {

/// @brief PSI rwlock key for module locking.
extern PSI_rwlock_key key_rwlock_module;

/// @name Mutex Keys
/// @{
/// @brief PSI mutex key for scheduler.
extern PSI_mutex_key key_mutex_scheduler;
/// @brief PSI mutex key for module.
extern PSI_mutex_key key_mutex_module;

/// @brief PSI mutex key for CSA scheduler main.
extern PSI_mutex_key key_mt_csa_sched_main;
/// @brief PSI mutex key for CSA scheduler end.
extern PSI_mutex_key key_mt_csa_sched_end;
/// @brief PSI mutex key for CSA scheduler tasks.
extern PSI_mutex_key key_mt_csa_sched_tasks;
/// @brief PSI mutex key for CSA scheduler phases.
extern PSI_mutex_key key_mt_csa_sched_phases;

/// @brief PSI mutex key for CSA session service entry.
extern PSI_mutex_key key_mt_csa_session_service_entry;

/// @brief PSI mutex key for CSA prefetcher wait.
extern PSI_mutex_key key_mt_csa_prefetcher_wait;
/// @brief PSI mutex key for CSA prefetcher file move.
extern PSI_mutex_key key_mt_csa_prefetcher_file_move;
/// @}

/// @name Condition Variable Keys
/// @{
/// @brief PSI cond key for CSA scheduler main.
extern PSI_cond_key key_cv_csa_sched_main;
/// @brief PSI cond key for CSA scheduler end.
extern PSI_cond_key key_cv_csa_sched_end;
/// @brief PSI cond key for CSA prefetcher wait.
extern PSI_cond_key key_cv_csa_prefetcher_wait;
/// @brief PSI cond key for CSA prefetcher file move.
extern PSI_cond_key key_cv_csa_prefetcher_file_move;
/// @}

/// Initializes PSI condition variable keys for MTA.
void init_mta_psi_cond_keys();

/// @name Thread Keys
/// @{
/// @brief PSI thread key for worker threads.
extern PSI_thread_key key_thread_worker;
/// @brief PSI thread key for scheduler threads.
extern PSI_thread_key key_thread_scheduler;
/// @brief PSI thread key for prefetcher threads.
extern PSI_thread_key key_thread_prefetcher;
/// @brief PSI thread key for prefetcher threads.
extern PSI_thread_key key_thread_session;
/// @}

/// @name Memory Keys
/// @{
/// @brief PSI memory key for CSA prefetcher.
extern PSI_memory_key key_mem_csa_prefetcher;
/// @brief PSI memory key for CSA THP.
extern PSI_memory_key key_mem_csa_thp;
/// @brief PSI memory key for CSA provider.
extern PSI_memory_key key_mem_csa_provider;
/// @brief PSI memory key for CSA session service.
extern PSI_memory_key key_mem_csa_session_service;
/// @brief PSI memory key for CSA scheduler.
extern PSI_memory_key key_mem_csa_scheduler;
/// @brief PSI memory key for decompressing stream.
extern PSI_memory_key key_decompressing_stream;
/// @}

/// @name Stage Information
/// @{
/// @brief Stage for CSA session attached.
extern PSI_stage_info stage_csa_session_attached;
/// @brief Stage for CSA session detached.
extern PSI_stage_info stage_csa_session_detached;

/// @brief Stage for CSA starting.
extern PSI_stage_info stage_csa_starting;
/// @brief Stage for CSA working.
extern PSI_stage_info stage_csa_working;
/// @brief Stage for CSA wait for transaction.
extern PSI_stage_info stage_csa_wait_for_trx;
/// @brief Stage for CSA update statistics.
extern PSI_stage_info stage_csa_update_statistics;
/// @brief Stage for CSA enqueue transaction.
extern PSI_stage_info stage_csa_enqueue_transaction;
/// @brief Stage for CSA stopping.
extern PSI_stage_info stage_csa_stopping;
/// @brief Stage for CSA stopped.
extern PSI_stage_info stage_csa_stopped;

/// @brief Stage for CSA job apply.
extern PSI_stage_info stage_csa_job_apply;
/// @brief Stage for CSA session acquire.
extern PSI_stage_info stage_csa_session_acquire;
/// @brief Stage for CSA job attach.
extern PSI_stage_info stage_csa_job_attach;
/// @brief Stage for CSA job detach.
extern PSI_stage_info stage_csa_job_detach;
/// @brief Stage for CSA session release.
extern PSI_stage_info stage_csa_session_release;

/// @brief Stage for CSA clock add.
extern PSI_stage_info stage_csa_clock_add;
/// @brief Stage for CSA clock tick.
extern PSI_stage_info stage_csa_clock_tick;

/// @brief Stage for CSA dependency register.
extern PSI_stage_info stage_csa_dep_register;
/// @brief Stage for CSA dependency add.
extern PSI_stage_info stage_csa_dep_add;
/// @brief Stage for CSA dependency task done.
extern PSI_stage_info stage_csa_dep_task_done;

/// @brief Stage for CSA scheduler stopping.
extern PSI_stage_info stage_csa_sched_stopping;
/// @brief Stage for CSA scheduler stopped.
extern PSI_stage_info stage_csa_sched_stopped;
/// @brief Stage for CSA scheduler wait for work.
extern PSI_stage_info stage_csa_sched_wait_for_work;
/// @brief Stage for CSA scheduler check dependencies.
extern PSI_stage_info stage_csa_sched_check_dependencies;
/// @brief Stage for CSA scheduler check stage queues.
extern PSI_stage_info stage_csa_sched_check_stage_queues;
/// @brief Stage for CSA scheduler enqueue ready tasks.
extern PSI_stage_info stage_csa_sched_enqueue_ready_tasks;
/// @brief Stage for CSA scheduler sync clock.
extern PSI_stage_info stage_csa_sched_sync_clock;
/// @brief Stage for CSA scheduler sync limit.
extern PSI_stage_info stage_csa_sched_sync_limit;

/// @brief Stage for CSA THP worker wait.
extern PSI_stage_info stage_csa_thp_worker_wait;
/// @brief Stage for CSA THP worker exec.
extern PSI_stage_info stage_csa_thp_worker_exec;
/// @brief Stage for CSA THP worker stop.
extern PSI_stage_info stage_csa_thp_worker_stop;
/// @}

/// @brief Initializes PSI keys for CSA.
void init_csa_psi();

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SERVICE_PSI_H
