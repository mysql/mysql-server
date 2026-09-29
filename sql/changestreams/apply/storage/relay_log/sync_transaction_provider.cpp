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

#include "sql/changestreams/apply/storage/relay_log/sync_transaction_provider.h"

#include <cassert>

#include "mysql/psi/mysql_file.h"  // mysql_file_close
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_reader.h"  // Queued_transaction_reader
#include "sql/mysqld.h"  // slave_trans_retries
#include "sql/rpl_mi.h"  // Master_info

using namespace mysql::binlog::event;

namespace mysql::csa {

Sync_transaction_provider::Sync_transaction_provider(
    int instance_id, Relay_log_info *rli, std::size_t max_read_event_bytes,
    std::size_t max_read_payload_bytes)
    : m_rli(rli),
      m_reader(new Transaction_provider::Common_reader_type(
          instance_id, rli, max_read_event_bytes, max_read_payload_bytes)),
      m_stat_monitor(scheduler::Statistics_monitor::get(instance_id)) {}

// In-memory relay-log overload: the provider drains transactions from the
// channel's Trx_envelope_queue via a Queued_transaction_reader instead of
// reading the on-disk relay log. Selected by Csa_service::run for in-memory
// channels; the classic (bytes-bounded) overload above is used otherwise.
Sync_transaction_provider::Sync_transaction_provider(int instance_id,
                                                     Relay_log_info *rli,
                                                     Trx_envelope_queue *queue)
    : m_rli(rli),
      m_reader(
          std::make_shared<Queued_transaction_reader>(instance_id, rli, queue)),
      m_stat_monitor(scheduler::Statistics_monitor::get(instance_id)) {}

void Sync_transaction_provider::start() {}

bool Sync_transaction_provider::is_error() const {
  return m_reader->is_error();
}

void Sync_transaction_provider::stop() {
  m_is_stopped = true;
  m_reader->stop();
}

void Sync_transaction_provider::finish() {
  assert(is_stopped());
  if (!is_stopped()) {
    return;
  }
  auto pending_job = m_reader->read();
  assert(pending_job == nullptr);
  delete pending_job;
}

bool Sync_transaction_provider::is_stopped() const {
  return m_is_stopped || m_reader->is_stopped();
}

Job_ptr Sync_transaction_provider::next() {
  if (is_stopped()) {
    return Job_ptr();
  }
  auto job = m_reader->read();
  m_stat_monitor.get().get_stat(Statistics_map::trx_provided_cnt).add(1);
  return job;
}

}  // namespace mysql::csa
