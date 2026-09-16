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

#include "sql/changestreams/apply/storage/relay_log/relay_log_adaptive_reader.h"

#include <iomanip>

#include "mysql/psi/mysql_file.h"  // mysql_file_close
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_cache.h"
#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_relay_log.h"
#include "sql/changestreams/apply/storage/relay_log/reader_controller_read_type.h"
#include "sql/mysqld.h"  // slave_trans_retries
#include "sql/rpl_mi.h"  // Master_info

using namespace mysql::binlog::event;

using Log_event_ptr = std::shared_ptr<Log_event>;
using Read_type = mysql::csa::Reader_controller_read_type;
using Statistics_map_sched = mysql::scheduler::Statistics_map;
using Statistics_monitor = mysql::scheduler::Statistics_monitor;

namespace mysql::csa {

Relay_log_adaptive_reader::Relay_log_adaptive_reader(
    int instance_id, Relay_log_info *rli, std::size_t max_read_event_bytes,
    std::size_t max_read_payload_bytes)
    : m_rli(rli),
      m_instance_id(instance_id),
      m_stat_monitor(scheduler::Statistics_monitor::get(instance_id)),
      m_resource_monitor(Resource_monitor::get(instance_id)),
      m_max_read_event_bytes(max_read_event_bytes),
      m_max_read_payload_bytes(max_read_payload_bytes) {
  m_prefetcher.reset(new Log_prefetcher(
      &rli->relay_log, key_mt_csa_prefetcher_wait, key_cv_csa_prefetcher_wait,
      key_mt_csa_prefetcher_file_move, key_cv_csa_prefetcher_file_move,
      key_thread_prefetcher));
  m_channel.reset(new Channel(rli->mi->get_channel(), instance_id,
                              rli->get_commit_order_manager()));
  m_reader = new Event_reader_controller(rli, m_prefetcher);
  if (!m_reader || !m_channel) {
    // OOM todo
    return;
  }
  m_shared_controller.reset(m_reader);
  m_reader->open();
  m_transaction_boundary_parser.reset();
  m_delete_handler = std::make_shared<Relay_log_deleter>(
      rli->get_group_relay_log_name(), m_shared_controller);
  // add reader as subscriber
  m_delete_handler->add_subscriber();
}

Relay_log_adaptive_reader::~Relay_log_adaptive_reader() { m_reader->close(); }

void Relay_log_adaptive_reader::tune() {
  if (!tune::csa_provider_enable_tune) {
    return;
  }
  auto now_time = std::chrono::system_clock::now();
  auto current_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now_time - m_last_tune_time)
                              .count();
  if (current_duration >= m_tune_period_ms) {
    auto &stat_monitor = Statistics_monitor::get(m_instance_id);
    // check if parameters need tuning
    m_thp_task_exec_time.update(
        stat_monitor.get_stat(Statistics_map_sched::thp_task_exec_time)
            .get_timer());
    m_thp_worker_exec_time.update(
        stat_monitor.get_stat(Statistics_map_sched::thp_worker_exec_time)
            .get_timer());
    if (m_thp_worker_exec_time.get() > 0) {
      double worker_load = static_cast<double>(m_thp_task_exec_time.get()) /
                           static_cast<double>(m_thp_worker_exec_time.get());
      auto current = m_max_read_event_bytes;
      if (worker_load < m_worker_min_load_threshold) {
        m_max_read_event_bytes = 0;
        m_max_read_payload_bytes = 0;
      } else if (worker_load > m_worker_max_load_threshold) {
        m_max_read_event_bytes = tune::provider_max_read_event_bytes;
        m_max_read_payload_bytes = tune::provider_max_read_payload_bytes;
      }
      if (current != m_max_read_event_bytes) {
        MYSQL_LIB_LOG_INFO()
            << "Reconfiguring CSA reader threshold for channel '"
            << m_rli->get_channel() << "' to: " << m_max_read_event_bytes
            << ", due to detected worker utilization: " << std::fixed
            << std::setprecision(2) << (worker_load * 100.0) << "%";
      }
    }
    m_last_tune_time = now_time;
  }
}

bool Relay_log_adaptive_reader::is_stopped() const {
  return m_is_error || m_reader->is_stopped();
}

namespace {
std::size_t get_gtid_transaction_length(Log_event *ev, Relay_log_info *rli,
                                        int64_t &prev) {
  Gtid_log_event *gtid_ev = dynamic_cast<Gtid_log_event *>(ev);
  assert(gtid_ev);
  auto lc = gtid_ev->last_committed;
  auto seq = gtid_ev->sequence_number;
  if (seq <= 1 || lc == 0) {
    prev = -1;
  }
  // check timestamps
  // dependency adapter adds barriers on inconsistend timestams,
  // issue warnings if needed
  if (unlikely(lc != 0 && lc >= seq)) {
    char buff_gtid[Gtid::MAX_TEXT_LENGTH + 1];
    gtid_ev->get_gtid_spec().to_string(gtid_ev->get_tsid(), buff_gtid);
    LogErr(WARNING_LEVEL, ER_RPL_INCONSISTENT_TIMESTAMPS_IN_TRX, buff_gtid,
           rli->get_event_relay_log_name(), seq, lc);
  } else if (unlikely(seq <= prev)) {
    char buff_gtid[Gtid::MAX_TEXT_LENGTH + 1];
    gtid_ev->get_gtid_spec().to_string(gtid_ev->get_tsid(), buff_gtid);
    LogErr(WARNING_LEVEL, ER_RPL_INCONSISTENT_SEQUENCE_NO_IN_TRX, buff_gtid,
           rli->get_event_relay_log_name(), seq, prev);
  }
  prev = seq;
  return gtid_ev->get_trx_length();
}
}  // namespace

Job_ptr Relay_log_adaptive_reader::read() {
  Job_ptr job{nullptr};
  tune();

  auto clear_active_transaction_state = [this]() {
    m_active_fetchable_transaction.reset();
    m_start_batch_metadata = Event_file_metadata();
    m_open_cache_batch = nullptr;
    m_open_stream_batch = nullptr;
    m_current_transaction_max_event_length = 0;
    m_is_in_trx = false;
    m_read_type = Read_type::event;
    m_next_read_type = Read_type::event;
  };

  auto truncate_active_transaction =
      [this, &clear_active_transaction_state](Job_ptr job_ret) -> Job_ptr {
    if (m_open_cache_batch) {
      m_open_cache_batch->set_stream_truncated();
    }
    if (m_open_stream_batch) {
      m_open_stream_batch->set_stream_truncated();
    }
    if (m_active_fetchable_transaction) {
      m_active_fetchable_transaction->set_fetching_truncated();
    }
    if (m_active_fetchable_transaction) {
      --m_prev_seq;
    }
    m_transaction_boundary_parser.reset();
    clear_active_transaction_state();
    return job_ret;
  };

  auto end_current_batch = [this]() {
    if (m_open_cache_batch) {
      m_open_cache_batch->seal_stream();
      m_open_cache_batch = nullptr;
    }
    if (m_open_stream_batch) {
      m_open_stream_batch->seal_stream();
      m_open_stream_batch = nullptr;
    }
  };

  auto return_from_read =
      [this, &truncate_active_transaction](Job_ptr job_ret) -> Job_ptr {
    if (job_ret != nullptr || !m_active_fetchable_transaction) {
      return job_ret;
    }
    return truncate_active_transaction(job_ret);
  };

  while (!is_stopped()) {
    std::optional<Event_file_metadata> metadata_result;
    IReader_event_ptr next_event;
    std::shared_ptr<Log_event> optional_event;

    {
      MUTEX_LOCK(lock, &m_rli->data_lock);
      unsigned int timeout_in_ms = m_is_in_trx ? 0 : 100;
      metadata_result = m_reader->read_next(timeout_in_ms, m_read_type);
      m_is_error = m_reader->is_error();
    }

    if (!metadata_result.has_value() || m_is_error) {
      return return_from_read(job);
    }

    auto metadata = metadata_result.value();
    if (metadata.get_length() > m_current_transaction_max_event_length) {
      m_current_transaction_max_event_length = metadata.get_length();
    }

    if (metadata.has_event()) {
      optional_event = metadata.get_event();
    }

    m_next_read_type = m_read_type;
    bool is_gtid_event =
        mysql::binlog::event::Log_event_type_helper::is_any_gtid_event(
            metadata.get_type());
    if (is_gtid_event) {
      if (m_active_fetchable_transaction) {
        m_active_fetchable_transaction->set_success();
        truncate_active_transaction(job);
      }

      assert(optional_event);
      if (m_rli->is_until_satisfied_before_dispatching_event(
              optional_event.get())) {
        MUTEX_LOCK(lock, &m_rli->data_lock);
        m_rli->abort_slave = 1;
        m_stopped = true;
        return return_from_read(nullptr);
      }

      auto trx_size =
          get_gtid_transaction_length(optional_event.get(), m_rli, m_prev_seq);
      if (trx_size > m_max_read_event_bytes &&
          trx_size < m_max_read_payload_bytes) {
        m_next_read_type = Read_type::cache_metadata;
      } else if (trx_size > m_max_read_payload_bytes) {
        m_next_read_type = Read_type::metadata;
      }
      next_event = std::make_shared<Cached_event>(optional_event);
    }

    mysql::binlog::event::Log_event_basic_info log_event_info;
    log_event_info.event_type = metadata.get_type();
    log_event_info.query = metadata.get_query().c_str();
    log_event_info.query_length = metadata.get_query().size();
    log_event_info.ignorable_event = metadata.is_ignorable();
    auto was_in_trx = m_is_in_trx;
    m_transaction_boundary_parser.feed_event(log_event_info, true);
    m_is_in_trx = m_transaction_boundary_parser.is_inside_transaction();
    if (m_transaction_boundary_parser.is_error()) {
      m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                    ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                    "Transaction boundary parser returned an error");
      m_is_error = true;
      return return_from_read(job);
    }

    bool is_special_control = metadata.get_type() == FORMAT_DESCRIPTION_EVENT ||
                              metadata.get_type() == ROTATE_EVENT ||
                              metadata.get_type() == STOP_EVENT ||
                              metadata.get_type() == PREVIOUS_GTIDS_LOG_EVENT;

    bool is_ignored =
        m_transaction_boundary_parser.was_event_ignored() || is_special_control;

    if (m_start_batch_metadata.get_file_name().empty()) {
      m_start_batch_metadata = metadata;
    }

    bool same_file = metadata.is_in_same_file(m_start_batch_metadata);
    if (!same_file || is_ignored) {
      end_current_batch();
      m_start_batch_metadata = is_ignored ? Event_file_metadata() : metadata;
    }

    if (metadata.get_file_name() != m_delete_handler->get_file_name()) {
      m_delete_handler->set_subscriber_success();
      m_delete_handler = std::make_shared<Relay_log_deleter>(
          metadata.get_file_name(), m_shared_controller);
      m_delete_handler->add_subscriber();
    }

    bool transaction_event =
        (m_is_in_trx || was_in_trx) && !is_ignored && !is_special_control;
    bool transaction_finished = was_in_trx && !m_is_in_trx;

    if (is_gtid_event && !m_active_fetchable_transaction) {
      m_active_fetchable_transaction =
          std::make_shared<Fetchable_transaction>();
    }
    if (m_active_fetchable_transaction &&
        (is_gtid_event || transaction_event)) {
      m_active_fetchable_transaction->update_max_event_length(
          metadata.get_length());
    }

    if (transaction_event) {
      if ((m_read_type != Read_type::metadata && metadata.has_event()) ||
          metadata.get_type() == INCIDENT_EVENT) {
        next_event = std::make_shared<Cached_event>(optional_event);
      } else if (m_read_type == Read_type::cache_metadata) {
        assert(metadata.has_payload());
        next_event = std::make_shared<Cached_event_payload>(
            metadata.get_payload(), m_current_fde);
      }

      if (next_event) {
        if (!m_open_cache_batch && m_active_fetchable_transaction) {
          auto cache_batch = std::make_unique<Event_set_fetchable_cache>(
              Event_set_fetchable_cache::Event_set_type{}, true, m_current_fde,
              m_delete_handler, true);
          m_open_cache_batch = cache_batch.get();
          m_active_fetchable_transaction->append_batch(std::move(cache_batch));
        }
        if (m_open_cache_batch) {
          if (transaction_finished && m_active_fetchable_transaction) {
            m_active_fetchable_transaction->set_fetching_complete();
            m_open_cache_batch->append_event(next_event, true);
            m_open_cache_batch = nullptr;
          } else {
            m_open_cache_batch->append_event(next_event);
          }
        }
      } else {
        if (!m_open_stream_batch && m_active_fetchable_transaction) {
          auto stream_batch = std::make_unique<Event_set_fetchable_relay_log>(
              m_start_batch_metadata.get_file_name(),
              m_start_batch_metadata.get_file_pos(),
              m_start_batch_metadata.get_file_pos(), m_delete_handler,
              opt_replica_sql_verify_checksum, true, m_current_fde, true);
          m_open_stream_batch = stream_batch.get();
          m_active_fetchable_transaction->append_batch(std::move(stream_batch));
        }
        if (m_open_stream_batch) {
          auto end_file_pos = metadata.get_file_pos() + metadata.get_length();
          if (transaction_finished && m_active_fetchable_transaction) {
            m_active_fetchable_transaction->set_fetching_complete();
            m_open_stream_batch->append_event_end(end_file_pos, true);
            m_open_stream_batch = nullptr;
          } else {
            m_open_stream_batch->append_event_end(end_file_pos);
          }
        }
      }
    }

    if ((was_in_trx && !m_is_in_trx) || m_next_read_type != m_read_type) {
      end_current_batch();
      m_start_batch_metadata = Event_file_metadata();
    }

    m_read_type = m_next_read_type;

    if (is_gtid_event) {
      Job_applier *trx = new Job_applier(m_channel.get(), slave_trans_retries,
                                         m_active_fetchable_transaction,
                                         m_stat_monitor, m_resource_monitor);
      assert(trx);
      if (m_rli->is_until_satisfied_after_dispatching_event()) {
        MUTEX_LOCK(lock, &m_rli->data_lock);
        m_rli->abort_slave = 1;
        m_stopped = true;
      }
      return return_from_read(trx);
    }

    switch (metadata.get_type()) {
      case mysql::binlog::event::OBSOLETE_WRITE_ROWS_EVENT_V1:
      case mysql::binlog::event::OBSOLETE_UPDATE_ROWS_EVENT_V1:
      case mysql::binlog::event::OBSOLETE_DELETE_ROWS_EVENT_V1:
      case mysql::binlog::event::APPEND_BLOCK_EVENT:
      case mysql::binlog::event::DELETE_FILE_EVENT:
      case mysql::binlog::event::INTVAR_EVENT:
      case mysql::binlog::event::UNKNOWN_EVENT:
      case mysql::binlog::event::START_EVENT_V3: {
        m_is_error = true;
        std::stringstream ss;
        ss << "CSA does not support "
           << get_event_type_as_string(metadata.get_type()) << " events";
        m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                      ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                      ss.str().c_str());
        return return_from_read(job);
      }
      case mysql::binlog::event::HEARTBEAT_LOG_EVENT:
      case mysql::binlog::event::HEARTBEAT_LOG_EVENT_V2:
      case mysql::binlog::event::STOP_EVENT: {
        assert(m_transaction_boundary_parser.was_event_ignored());
        break;
      }
      case mysql::binlog::event::BEGIN_LOAD_QUERY_EVENT:
      case mysql::binlog::event::EXECUTE_LOAD_QUERY_EVENT:
      case mysql::binlog::event::VIEW_CHANGE_EVENT:
      case mysql::binlog::event::QUERY_EVENT:
      case mysql::binlog::event::ROWS_QUERY_LOG_EVENT:
      case mysql::binlog::event::XID_EVENT:
      case mysql::binlog::event::TABLE_MAP_EVENT:
      case mysql::binlog::event::WRITE_ROWS_EVENT:
      case mysql::binlog::event::UPDATE_ROWS_EVENT:
      case mysql::binlog::event::DELETE_ROWS_EVENT:
      case mysql::binlog::event::XA_PREPARE_LOG_EVENT:
      case mysql::binlog::event::PARTIAL_UPDATE_ROWS_EVENT:
      case mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT:
      case mysql::binlog::event::INCIDENT_EVENT:
      case mysql::binlog::event::RAND_EVENT:
      case mysql::binlog::event::USER_VAR_EVENT:
      case mysql::binlog::event::IGNORABLE_LOG_EVENT: {
        if (!m_is_in_trx) {
          if (metadata.get_type() != QUERY_EVENT &&
              metadata.get_type() != XID_EVENT &&
              metadata.get_type() != XA_PREPARE_LOG_EVENT &&
              metadata.get_type() != TRANSACTION_PAYLOAD_EVENT &&
              metadata.get_type() != INCIDENT_EVENT) {
            if (m_active_fetchable_transaction) {
              m_active_fetchable_transaction->set_success();
            }
            return return_from_read(job);
          }

          end_current_batch();
          if (m_active_fetchable_transaction && !transaction_finished) {
            m_active_fetchable_transaction->set_fetching_complete();
          }

          assert(m_active_fetchable_transaction);
          clear_active_transaction_state();
          break;
        }
        break;
      }
      case mysql::binlog::event::FORMAT_DESCRIPTION_EVENT:
        if (optional_event) {
          m_current_fde = optional_event;
        }
        if (optional_event && optional_event->common_header->log_pos != 0 &&
            !m_is_in_trx) {
          if (m_active_fetchable_transaction) {
            m_active_fetchable_transaction->set_success();
          }
          return return_from_read(job);
        }
        break;
      case mysql::binlog::event::ROTATE_EVENT: {
        if (optional_event && !optional_event->is_artificial_event() &&
            !m_is_in_trx) {
          m_prev_seq = -1;
        }
        break;
      }
      case mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT:
      case mysql::binlog::event::SLAVE_EVENT:
      case mysql::binlog::event::TRANSACTION_CONTEXT_EVENT:
      case mysql::binlog::event::ANONYMOUS_GTID_LOG_EVENT:
      case mysql::binlog::event::GTID_LOG_EVENT:
      case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
        break;
      }
      default:
        if (!is_ignored) {
          return return_from_read(job);
        }
        break;
    }
  }

  return return_from_read(job);
}
bool Relay_log_adaptive_reader::is_error() const { return m_is_error; }

void Relay_log_adaptive_reader::stop() { m_reader->stop(); }

}  // namespace mysql::csa
