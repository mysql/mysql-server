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

#include <string>

#include "sql/changestreams/apply/psi/psi.h"

#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/mysql_thread.h"
#include "template_utils.h"

namespace mysql::csa {

static const std::string psi_category = "csa";

// CSA rw locks vars PSI
PSI_rwlock_key key_rwlock_module;

PSI_rwlock_info all_csa_rwlocks[] = {
    {&key_rwlock_module, "module", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PERMANENT,
     PSI_DOCUMENT_ME}};

static void init_psi_csa_rwlocks() {
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_rwlocks));
  PSI_RWLOCK_CALL(register_rwlock)
  (psi_category.c_str(), all_csa_rwlocks, count);
#endif
}

// CSA mutex vars PSI
PSI_mutex_key key_mutex_scheduler;
PSI_mutex_key key_mutex_module;

PSI_mutex_key key_mt_csa_sched_main;
PSI_mutex_key key_mt_csa_sched_end;
PSI_mutex_key key_mt_csa_sched_tasks;
PSI_mutex_key key_mt_csa_sched_phases;
PSI_mutex_key key_mt_csa_session_service_entry;
PSI_mutex_key key_mt_csa_prefetcher_wait;
PSI_mutex_key key_mt_csa_prefetcher_file_move;

PSI_mutex_info all_csa_locks[] = {
    {&key_mutex_scheduler, "scheduler", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_mutex_module, "module_lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_mt_csa_sched_main, "csa_sched_main_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_mt_csa_sched_end, "csa_sched_end_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_mt_csa_sched_tasks, "csa_sched_tasks_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_mt_csa_sched_phases, "csa_sched_phase_lock", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_mt_csa_session_service_entry, "csa_session_service_entry_lock",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_mt_csa_prefetcher_wait, "csa_prefetcher_wait_lock",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_mt_csa_prefetcher_file_move, "csa_prefetcher_file_move_lock",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
};

static void init_psi_csa_mutexes() {
#ifdef HAVE_PSI_MUTEX_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_locks));
  PSI_MUTEX_CALL(register_mutex)
  (psi_category.c_str(), all_csa_locks, count);
#endif
}

// CSA cond vars PSI.
PSI_cond_key key_cv_csa_sched_main;
PSI_cond_key key_cv_csa_sched_end;
PSI_cond_key key_cv_csa_prefetcher_wait;
PSI_cond_key key_cv_csa_prefetcher_file_move;

PSI_cond_info all_csa_conds[] = {
    {&key_cv_csa_sched_main, "csa_scheduler_main_cv", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_cv_csa_sched_end, "csa_scheduler_end_cv", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_cv_csa_prefetcher_wait, "csa_prefetcher_wait_cv", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME},
    {&key_cv_csa_prefetcher_file_move, "csa_prefetcher_file_move_cv",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

static void init_psi_csa_conds() {
#ifdef HAVE_PSI_COND_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_conds));
  PSI_COND_CALL(register_cond)
  (psi_category.c_str(), all_csa_conds, count);
#endif
}

// CSA threads PSI.
PSI_thread_key key_thread_worker;
PSI_thread_key key_thread_scheduler;
PSI_thread_key key_thread_prefetcher;
PSI_thread_key key_thread_session;

PSI_thread_info all_csa_threads[] = {
    {&key_thread_worker, "replica_worker", "rpl_rca_wkr",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME},
    {&key_thread_scheduler, "replica_scheduler", "rpl_rca_sch",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME},
    {&key_thread_prefetcher, "replica_prefetcher", "rpl_rca_pre",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME},
    {&key_thread_session, "replica_session", "rpl_rca_ses",
     PSI_FLAG_THREAD_SYSTEM, 0, PSI_DOCUMENT_ME}};

static void init_psi_csa_thread_keys(void) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_threads));
  PSI_THREAD_CALL(register_thread)
  ("sql", all_csa_threads, count);
#endif
}

PSI_memory_key key_mem_csa_prefetcher;
PSI_memory_key key_mem_csa_thp;
PSI_memory_key key_mem_csa_provider;
PSI_memory_key key_mem_csa_session_service;
PSI_memory_key key_mem_csa_scheduler;
PSI_memory_key key_decompressing_stream;

static PSI_memory_info all_csa_mem_resources[] = {
    {&key_mem_csa_prefetcher, "CSA Prefetcher memory", 0,
     PSI_VOLATILITY_UNKNOWN, "Memory allocated in CSA prefetcher"},
    {&key_mem_csa_thp, "CSA Thread pool memory", 0, PSI_VOLATILITY_UNKNOWN,
     "Memory allocated in CSA thread pool"},
    {&key_mem_csa_provider, "CSA Transaction provider memory", 0,
     PSI_VOLATILITY_UNKNOWN, "Memory allocated in CSA transaction provider"},
    {&key_mem_csa_provider, "CSA Session service memory", 0,
     PSI_VOLATILITY_UNKNOWN, "Memory allocated in CSA session service"},
    {&key_mem_csa_scheduler, "CSA scheduler memory", 0, PSI_VOLATILITY_UNKNOWN,
     "Memory allocated in CSA scheduler"},
    {&key_decompressing_stream, "CSA scheduler memory", 0,
     PSI_VOLATILITY_UNKNOWN,
     "Memory allocated in CSA reader while decompression"}};

static void init_psi_csa_mem() {
#ifdef HAVE_PSI_MEMORY_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_mem_resources));
  PSI_MEMORY_CALL(register_memory)
  (psi_category.c_str(), all_csa_mem_resources, count);
#endif
}

// Stages
PSI_stage_info stage_csa_session_attached = {
    0, "Session attached to worker thread.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_session_detached = {
    0, "Session detached from worker thread.", 0, PSI_DOCUMENT_ME};

PSI_stage_info stage_csa_starting = {0, "CSA service: initialization.", 0,
                                     PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_working = {0, "CSA service: working.", 0,
                                    PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_wait_for_trx = {
    0, "CSA service: Wait for transaction.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_update_statistics = {
    0, "CSA service: Update statistics.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_enqueue_transaction = {
    0, "CSA service: Enqueue transaction.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_stopping = {0, "CSA service: stopping.", 0,
                                     PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_stopped = {0, "CSA service: stopped.", 0,
                                    PSI_DOCUMENT_ME};

PSI_stage_info stage_csa_job_apply = {0, "Job: Applying.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_session_acquire = {0, "Job: Acquire session.", 0,
                                            PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_job_attach = {0, "Job: Attach to session.", 0,
                                       PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_job_detach = {0, "Job: Detach from session.", 0,
                                       PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_session_release = {0, "Job: Release session", 0,
                                            PSI_DOCUMENT_ME};

PSI_stage_info stage_csa_clock_add = {
    0, "Clock: Subscribe - wait for time ticket access.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_clock_tick = {
    0, "Clock: Tick - wait for time ticket access.", 0, PSI_DOCUMENT_ME};

PSI_stage_info stage_csa_dep_register = {0, "Dependencies: Register task.", 0,
                                         PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_dep_add = {0, "Dependencies: Add dependency.", 0,
                                    PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_dep_task_done = {
    0, "Dependencies: Release dependent tasks.", 0, PSI_DOCUMENT_ME};

PSI_stage_info stage_csa_sched_stopping = {0, "Scheduler: Stopping.", 0,
                                           PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_stopped = {0, "Scheduler: Stopped.", 0,
                                          PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_wait_for_work = {
    0, "Scheduler: Waiting for work.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_check_dependencies = {
    0, "Scheduler: Check dependencies.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_check_stage_queues = {
    0, "Scheduler: Check stage queues.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_enqueue_ready_tasks = {
    0, "Scheduler: Enqueue ready tasks.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_sync_clock = {
    0, "Scheduler: Synchronize due to clock queue full.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_sched_sync_limit = {
    0, "Scheduler: Synchronize due to reaching task limit.", 0,
    PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_thp_worker_wait = {
    0, "Thread pool worker: wait for task.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_thp_worker_exec = {
    0, "Thread pool worker: executing task.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_csa_thp_worker_stop = {0, "Thread pool worker: stopped.",
                                            0, PSI_DOCUMENT_ME};

PSI_stage_info *all_csa_plugin_stages[] = {&stage_csa_session_attached,
                                           &stage_csa_session_detached,
                                           &stage_csa_starting,
                                           &stage_csa_working,
                                           &stage_csa_wait_for_trx,
                                           &stage_csa_update_statistics,
                                           &stage_csa_enqueue_transaction,
                                           &stage_csa_stopping,
                                           &stage_csa_stopped,
                                           &stage_csa_job_apply,
                                           &stage_csa_session_acquire,
                                           &stage_csa_job_attach,
                                           &stage_csa_job_detach,
                                           &stage_csa_session_release,
                                           &stage_csa_clock_add,
                                           &stage_csa_clock_tick,
                                           &stage_csa_dep_register,
                                           &stage_csa_dep_add,
                                           &stage_csa_dep_task_done,
                                           &stage_csa_sched_stopping,
                                           &stage_csa_sched_stopped,
                                           &stage_csa_sched_wait_for_work,
                                           &stage_csa_sched_check_dependencies,
                                           &stage_csa_sched_check_stage_queues,
                                           &stage_csa_sched_enqueue_ready_tasks,
                                           &stage_csa_sched_sync_clock,
                                           &stage_csa_sched_sync_limit,
                                           &stage_csa_thp_worker_wait,
                                           &stage_csa_thp_worker_exec,
                                           &stage_csa_thp_worker_stop};

static void init_psi_stages_info(void) {
#ifdef HAVE_PSI_STAGE_INTERFACE
  int count;
  count = static_cast<int>(array_elements(all_csa_plugin_stages));
  PSI_STAGE_CALL(register_stage)
  (psi_category.c_str(), all_csa_plugin_stages, count);
#endif
}

void init_csa_psi(void) {
  init_psi_csa_thread_keys();
  init_psi_csa_conds();
  init_psi_csa_mutexes();
  init_psi_csa_rwlocks();
  init_psi_csa_mem();
  init_psi_stages_info();
}

}  // namespace mysql::csa
