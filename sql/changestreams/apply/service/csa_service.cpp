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

#include "sql/changestreams/apply/service/csa_service.h"
#include "mysql/scheduler/base_dependency_tracker.h"
#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_sequencer.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/resource/applier_channel_monitor.h"
#include "sql/changestreams/apply/resource/resource_map.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/scheduler/dependency_adapter_lwm.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_adaptive_reader.h"
#include "sql/changestreams/apply/storage/relay_log/sync_transaction_provider.h"
#include "sql/mysqld.h"
#include "sql/psi_memory_resource.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"
#include "sql/rpl_replica.h"  // sql_slave_killed

#include <sstream>

namespace mysql::csa {

Csa_service::Csa_service()
    : Module(key_mutex_module, key_rwlock_module), m_kernel_ref_count(0) {}

Csa_service::~Csa_service() {}

bool Csa_service::do_init() {
  DBUG_ENTER("Csa_service::do_init");
  DBUG_RETURN(0);
}

bool Csa_service::do_deinit() {
  DBUG_ENTER("Csa_service::deinit");
  DBUG_RETURN(0);
}

namespace {

auto create_scheduler_psi_params() {
  return mysql::scheduler::Scheduler_psi{
      key_mt_csa_sched_main,
      key_mt_csa_sched_end,
      key_cv_csa_sched_main,
      key_cv_csa_sched_end,
      key_mt_csa_sched_tasks,
      key_mt_csa_sched_phases,
      key_thread_scheduler,
      stage_csa_sched_stopping.m_key,
      stage_csa_sched_wait_for_work.m_key,
      stage_csa_sched_stopped.m_key,
      stage_csa_sched_check_dependencies.m_key,
      stage_csa_sched_check_stage_queues.m_key,
      stage_csa_sched_enqueue_ready_tasks.m_key,
      stage_csa_sched_sync_clock.m_key,
      stage_csa_sched_sync_limit.m_key,
      psi_memory_resource(key_mem_csa_scheduler)};
}

auto create_thread_pool_psi_params() {
  return mysql::scheduler::Thread_pool_psi{
      key_thread_worker, stage_csa_thp_worker_wait.m_key,
      stage_csa_thp_worker_exec.m_key, stage_csa_thp_worker_stop.m_key,
      psi_memory_resource(key_mem_csa_thp)};
}

auto create_session_service_psi_params() {
  return mysql::csa::Session_service_psi{
      key_mt_csa_session_service_entry, stage_csa_session_acquire.m_key,
      stage_csa_session_release.m_key,
      psi_memory_resource(key_mem_csa_session_service)};
}

auto create_scheduler_clock_psi_params() {
  return mysql::scheduler::Scheduler_clock_psi{stage_csa_clock_add.m_key,
                                               stage_csa_clock_tick.m_key};
}

Session_service_ptr create_session_service() {
  return std::make_shared<Session_service_bounded_queue>(
      create_session_service_psi_params());
}

}  // namespace

bool Csa_service::run(Relay_log_info *rli) {
  bool res = false;
  assert(rli);

  // ---------------------------------------------------------------
  // initialization
  // ---------------------------------------------------------------
  concurrency::set_stage(stage_csa_starting.m_key);

  auto channel = rli->mi->get_channel();

  auto scope_lock = wrlock();

  // variables setup
  using Dependency_adapter_ptr = mysql::csa::Dependency_adapter_ptr;
  Csa_service::Scheduler_ptr new_scheduler;
  Csa_service::Schedule_factory_ptr schedule_factory;
  mysql::scheduler::Task_sequencer gen;
  Dependency_adapter_ptr dependency_resolver;
  Csa_service::Clock_ptr clock;
  Csa_channel::Session_service_ptr session_service;
  std::size_t trx_num{0};

  dependency_resolver.reset(new mysql::csa::Dependency_adapter_lwm());

  std::chrono::time_point<std::chrono::system_clock> time_start;

  ++m_kernel_ref_count;

  int n_workers = rli->get_applier_worker_count();

  std::size_t channel_instance_id = rli->get_channel_instance_id();

  mysql::csa::Transaction_conflict_monitor::get(channel_instance_id)
      .reset(new Transaction_conflict_manager());
  mysql::csa::Transaction_conflict_monitor::get(channel_instance_id)->start();

  if (mysql::scheduler::Statistics_map::init_statistics(
          channel_instance_id, n_workers, tune::extended_statistics_enabled) ||
      mysql::csa::Statistics_map::init_statistics(
          channel_instance_id, n_workers, tune::extended_statistics_enabled)) {
    rli->report(ERROR_LEVEL, ER_SERVER_OUT_OF_RESOURCES, "%s",
                ER_THD(rli->info_thd, ER_SERVER_OUT_OF_RESOURCES));
    return true;
  }

  if (Resource_map::init_resources(
          channel_instance_id, rli->get_applier_event_memory_limit(),
          &stage_replica_waiting_worker_to_free_events)) {
    rli->report(ERROR_LEVEL, ER_SERVER_OUT_OF_RESOURCES, "%s",
                ER_THD(rli->info_thd, ER_SERVER_OUT_OF_RESOURCES));
    return true;
  }

  auto &stat_monitor = scheduler::Statistics_monitor::get(channel_instance_id);
  stat_monitor.reset();

  auto csa_channel_it = csa_channels.emplace(
      std::piecewise_construct, std::forward_as_tuple(rli->mi->get_channel()),
      std::forward_as_tuple());
  assert(csa_channel_it.second);
  auto &csa_channel = csa_channel_it.first->second;
  csa_channel.channel_instance_id = channel_instance_id;
  csa_channel.channel_thd = current_thd;

  assert(!csa_channel.thread_pool);

  try {
    auto thread_pool = std::make_shared<Csa_service::Thread_pool>(
        n_workers, channel_instance_id, create_thread_pool_psi_params());
    if (thread_pool->init()) {
      std::ostringstream error_message;
      error_message
          << "Could not initialize Change Streams Applier worker thread pool. "
          << "The user requested " << n_workers
          << " worker thread(s), but thread creation "
          << "failed due to insufficient resources.";
      const auto error_message_str = error_message.str();
      rli->report(ERROR_LEVEL, ER_REPLICA_FATAL_ERROR,
                  ER_THD(rli->info_thd, ER_REPLICA_FATAL_ERROR),
                  error_message_str.c_str());
      return true;
    }
    clock = std::make_shared<Clock_type>(tune::scheduler_clock_capacity,
                                         create_scheduler_clock_psi_params());
    Clock_ptr commit_order_clock = std::make_shared<Commit_order_clock_type>();
    Dependency_tracker_ptr dep(new Dependency_tracker());
    new_scheduler = std::make_shared<Scheduler_type>(
        thread_pool, clock, std::move(dep), channel_instance_id,
        std::min(tune::scheduler_max_task_number_factor * n_workers,
                 tune::csa_session_default_cache_size),
        create_scheduler_psi_params());
    if (opt_replica_preserve_commit_order) {
      new_scheduler->register_phase(commit_order_clock);
    }
    schedule_factory =
        std::make_shared<Schedule_factory>(clock, commit_order_clock);
    session_service = create_session_service();
    csa_channel.scheduler = new_scheduler;
    csa_channel.thread_pool = thread_pool;
    csa_channel.scheduler_clock = clock;
    csa_channel.session_service = session_service;
    csa_channel.commit_order_clock = commit_order_clock;
  } catch (std::exception &) {
    rli->report(ERROR_LEVEL, ER_SERVER_OUT_OF_RESOURCES, "%s",
                ER_THD(rli->info_thd, ER_SERVER_OUT_OF_RESOURCES));
    return true;
  }
  session_service->init(tune::csa_session_default_cache_size, rli);
  MYSQL_LIB_LOG_DEBUG() << "Successfully initialized CSA with " << n_workers
                        << " workers!";

  dependency_resolver->set_worker_num(n_workers);

  assert(new_scheduler);

  Transaction_provider_sptr provider;
  // create a relay log reader
  provider.reset(new Sync_transaction_provider(
      channel_instance_id, rli, tune::provider_max_read_event_bytes,
      tune::provider_max_read_payload_bytes));
  if (!provider) {
    rli->report(ERROR_LEVEL, ER_SERVER_OUT_OF_RESOURCES, "%s",
                ER_THD(rli->info_thd, ER_SERVER_OUT_OF_RESOURCES));
    return true;
  }
  csa_channel.provider = provider;

  scope_lock.reset();

  // ---------------------------------------------------------------
  // main loop processing
  // ---------------------------------------------------------------

  time_start = std::chrono::system_clock::now();

  tune::csa_show_config_info(channel);

  Applier_channel_monitor channel_monitor(csa_channel);

  provider->start();

  MYSQL_LIB_LOG_INFO() << "CSA initialized for channel: '" << channel
                       << "', starting applying transactions.";
  concurrency::set_stage(stage_csa_working.m_key);
  while (true) {
    // check applier progress
    channel_monitor.check_applier_progress();

    // get the next job
    auto next = provider->next();
    if (!next) {
      if (provider->is_error() || provider->is_stopped() ||
          new_scheduler->is_error()) {
        break;
      }
      continue;
    }

    // register dependencies

    Job_applier *job_applier = reinterpret_cast<Job_applier *>(next);
    ++trx_num;

    concurrency::set_stage(stage_csa_update_statistics.m_key);

    auto seq_num = job_applier->get_sequence_number();
    auto commit_parent = job_applier->get_last_committed();

    // ensures that there is space for another transaction
    bool is_available = new_scheduler->ensure_space();
    if (!is_available) {
      delete next;
      break;  // error or stop was requested
    }

    job_applier->prepare_for_apply(session_service);

    // run the job

    auto task_internal_id = gen.next_id();
    auto deps =
        dependency_resolver->solve(task_internal_id, seq_num, commit_parent);

    bool is_trx = next->is_trx();

    Task_exec_job task_job(next, seq_num, session_service, stat_monitor,
                           csa_channel.m_channel_stopped);

    concurrency::set_stage(stage_csa_enqueue_transaction.m_key);

    bool enqueued{false};

    assert(deps.first.has_value());
    auto sched =
        schedule_factory->create(task_internal_id, deps.first.value(),
                                 (is_trx && opt_replica_preserve_commit_order));

    if (deps.second.has_value()) {
      enqueued = new_scheduler->enqueue_after(deps.second.value(), sched,
                                              std::move(task_job));
    } else {
      enqueued = new_scheduler->enqueue(sched, std::move(task_job));
    }

    if (!enqueued) {
      break;
    }
    stat_monitor.get_stat(Statistics_map::trx_scheduled_cnt).add(1);
  }
  if (new_scheduler->is_error() && !csa_channel.m_channel_stopped) {
    res = true;
    MYSQL_LIB_LOG_DEBUG() << "CSA was requested to stop upon applier error";
    csa_channel.session_service
        ->enable_stop_error_suppression_for_clean_sessions();
    csa_channel.m_channel_stopped = true;  // prevent job retries
  } else if (provider->is_error()) {
    res = true;
    MYSQL_LIB_LOG_DEBUG()
        << "CSA was requested to stop upon transaction provider error";
    csa_channel.session_service
        ->enable_stop_error_suppression_for_clean_sessions();
    csa_channel.m_channel_stopped = true;  // prevent job retries
  } else {                                 // provider stop or applier stop
    MYSQL_LIB_LOG_DEBUG() << "CSA was requested to stop";
  }
  concurrency::set_thd_stage(current_thd, stage_csa_stopping);
  provider->stop();
  provider->finish();
  if (!new_scheduler->synchronize(false)) {
    session_service->awake_sessions(true);
    new_scheduler->synchronize(true);
  }
  mysql::csa::Transaction_conflict_monitor::get(channel_instance_id)->stop();
  auto time_apply = (std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now() - time_start)
                         .count());
  MYSQL_LIB_LOG_DEBUG() << "Overall apply time without replay [ms]: "
                        << time_apply;
  if (time_apply > 0) {
    MYSQL_LIB_LOG_DEBUG() << "Mean TPS (no replay): "
                          << trx_num * 1000.0 / time_apply;
  }
  MYSQL_LIB_LOG_DEBUG() << "Applied transactions count: " << trx_num;

  // exiting, not using the kernel anymore
  if (res)
    MYSQL_LIB_LOG_DEBUG() << "Applier main loop exited with an error!";
  else
    MYSQL_LIB_LOG_DEBUG() << "Applier main loop exited successfully!";

  concurrency::set_thd_stage(current_thd, stage_csa_stopped);
  return res;
}

void Csa_service::remove(Relay_log_info *rli) {
  auto channel = rli->mi->get_channel();
  clean_up_context(channel);
  global_tsid_lock->wrlock();
  char *gtid_executed_string;
  std::ignore =
      gtid_state->get_executed_gtids()->to_string(&gtid_executed_string);
  global_tsid_lock->unlock();
  MYSQL_LIB_LOG_INFO() << "Change Stream Applier Service Thread for channel: '"
                       << rli->mi->get_channel()
                       << "' is stopped. Gtid executed set: "
                       << gtid_executed_string;
  my_free(gtid_executed_string);
}

void Csa_service::clean_up_context(const std::string &channel) {
  {
    global_tsid_lock->wrlock();
    char *gtid_executed_string;
    std::ignore =
        gtid_state->get_executed_gtids()->to_string(&gtid_executed_string);
    global_tsid_lock->unlock();
    MYSQL_LIB_LOG_DEBUG() << "Verify GTID executed: " << gtid_executed_string;
    my_free(gtid_executed_string);
  }

  auto scope_lock = wrlock();

  // we need to stop the channel

  --m_kernel_ref_count;

  auto csa_channel_it = csa_channels.find(channel);
  if (csa_channel_it == csa_channels.end()) {
    return;
  }
  auto &csa_channel = csa_channel_it->second;
  auto channel_instance_id = csa_channel.channel_instance_id;

  auto workers_cnt = csa_channel.thread_pool->size();
  auto &inactive_stats_entry = m_inactive_channel_stats[channel];
  inactive_stats_entry.clear();
  for (std::size_t worker_id = 0; worker_id < workers_cnt; ++worker_id) {
    auto del_stat =
        get_session_legacy_stats_internal(channel.c_str(), worker_id);
    if (del_stat.has_value()) {
      auto &vv = del_stat.value();
      vv.m_is_on = false;
      vv.m_thread_id = 0;  // reset worker id
      inactive_stats_entry.push_back(vv);
    }
  }
  // Below cleanup (resets) can be done automatically, however,
  // additional asserts allow to catch bugs in debug mode.
  assert(csa_channel.scheduler.use_count() == 1);
  csa_channel.scheduler.reset();
  assert(csa_channel.thread_pool.use_count() == 1);
  csa_channel.thread_pool.reset();
  assert(csa_channel.session_service.use_count() == 1);
  csa_channel.session_service->deinit();
  csa_channel.session_service.reset();
  mysql::scheduler::Statistics_monitor::clear(channel_instance_id);
  assert(current_thd == csa_channel.channel_thd);
  // last operation
  csa_channels.erase(csa_channel_it);
}

void Csa_service::stop(const char *channel_id, bool force_kill) {
  if (!is_inited()) {
    return;
  }
  auto scope_lock = rdlock();

  auto csa_channel_it = csa_channels.find(channel_id);
  if (csa_channel_it == csa_channels.end()) {
    return;
  }
  auto &csa_channel = csa_channel_it->second;

  // do a soft stop - waits for the scheduled transactions to finish
  if (csa_channel.m_soft_stop_enabled || m_soft_stop) {
    // || Multisource_info::is_group_replication_channel_name(channel_id)) {
    csa_channel.provider->stop();
    return;
  }

  /// No more retries for transactions, applier is stopping
  csa_channel.session_service
      ->enable_stop_error_suppression_for_clean_sessions();
  csa_channel.m_channel_stopped = true;
  csa_channel.provider->stop();
  // abort rli should be set to true
  // instruct scheduler to stop scheduling and clean queues
  csa_channel.scheduler->stop_now();
  // instruct thread pool workers not to take more tasks
  // csa_channel.thread_pool->end_execution();
  // awake all sessions and kill if needed
  csa_channel.session_service->awake_sessions(force_kill);
}

std::size_t Csa_service::get_workers_number(const char *channel) {
  if (!is_inited()) {
    return 0;
  }
  auto scope_lock = wrlock();
  auto it = csa_channels.find(channel);
  if (it == csa_channels.end()) {
    auto inactive_stats_it = m_inactive_channel_stats.find(channel);
    if (inactive_stats_it == m_inactive_channel_stats.end()) {
      return 0;
    }
    return inactive_stats_it->second.size();
  }
  return it->second.thread_pool->size();
}

bool Csa_service::initialize_channel_data(const char *channel, std::size_t,
                                          std::size_t worker_num) {
  if (!is_inited()) {
    return false;
  }
  auto scope_lock = wrlock();
  auto &inactive_stats_entry = m_inactive_channel_stats[channel];
  inactive_stats_entry.clear();
  for (std::size_t worker_id = 0; worker_id < worker_num; ++worker_id) {
    Session_legacy_stats new_entry;
    new_entry.m_channel_id = channel;
    new_entry.m_psf_worker_id = worker_id + 1;
    new_entry.m_thread_id = 0;
    inactive_stats_entry.emplace_back(std::move(new_entry));
  }
  return false;
}

std::optional<Session_legacy_stats>
Csa_service::get_session_legacy_stats_internal(const char *channel,
                                               std::size_t worker_id) {
  auto it = csa_channels.find(channel);
  if (it == csa_channels.end()) {
    auto inactive_stats_it = m_inactive_channel_stats.find(channel);
    if (inactive_stats_it == m_inactive_channel_stats.end()) {
      return {};
    }
    if (inactive_stats_it->second.size() <= worker_id) {
      return {};
    }
    auto result = inactive_stats_it->second[worker_id];
    return result;
  }
  auto channel_instance_id = it->second.channel_instance_id;
  auto &stat_monitor =
      mysql::scheduler::Statistics_monitor::get(channel_instance_id);
  auto session_id_res = stat_monitor.find_stat(Statistics_map::worker_session);
  auto thread_id_res = stat_monitor.find_stat(
      mysql::scheduler::Statistics_map::thp_thread_internal_id);
  auto session_worker_res =
      stat_monitor.find_stat(Statistics_map::session_worker);

  if (!session_id_res.has_value() || !thread_id_res.has_value() ||
      !session_worker_res.has_value()) {
    return {};
  }
  auto session_id = session_id_res.value().get().get(worker_id);
  Session_legacy_stats data;
  auto thread_id = thread_id_res.value().get().get(worker_id);
  if (session_worker_res.value().get().get(session_id) ==
      static_cast<long long>(worker_id)) {
    auto result = it->second.session_service->get_session_stats(session_id);
    if (result.has_value()) {
      data = result.value();
    }
  }
  data.m_psf_worker_id = worker_id + 1;
  data.m_channel_id = channel;
  data.m_thread_id = thread_id;
  data.m_is_on = true;
  return data;
}

std::optional<Session_legacy_stats> Csa_service::get_session_legacy_stats(
    const char *channel, std::size_t worker_id) {
  if (!is_inited()) {
    return {};
  }
  auto scope_lock = wrlock();
  return get_session_legacy_stats_internal(channel, worker_id);
}

void Csa_service::clear_channel_data(const char *channel, bool remove) {
  if (!is_inited()) {
    return;
  }
  auto scope_lock = wrlock();
  if (remove) {
    m_inactive_channel_stats.erase(channel);
    return;
  }
  auto inactive_stats_entry_it = m_inactive_channel_stats.find(channel);
  if (inactive_stats_entry_it != m_inactive_channel_stats.end()) {
    for (auto &entry : inactive_stats_entry_it->second) {
      entry.clear();
    }
  }
}

std::optional<bool> Csa_service::has_applied_all_work(const char *channel) {
  if (!is_inited()) {
    return {};
  }
  auto scope_lock = wrlock();
  auto it = csa_channels.find(channel);
  if (it == csa_channels.end()) {
    return {};
  }
  return it->second.scheduler->get_scheduled_tasks_count() == 0;
}

bool Csa_service::is_csa_event_applier(const char *channel,
                                       unsigned int thread_id) {
  if (!is_inited()) {
    return false;
  }
  auto scope_lock = wrlock();
  auto it = csa_channels.find(channel);
  if (it == csa_channels.end()) {
    return false;
  }
  return it->second.session_service->has_thd_id(thread_id);
}

}  // end of namespace mysql::csa
