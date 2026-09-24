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

#include "sql/changestreams/apply/storage/in_memory/queued_transaction_reader.h"

#include "mysqld_error.h"  // ER_REPLICA_FATAL_ERROR
#include "sql/changestreams/apply/context/channel.h"
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/psi/psi.h"    // stage_csa_working
#include "sql/changestreams/apply/psi/stage.h"  // concurrency::set_thd_stage
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/derror.h"       // ER_THD
#include "sql/mysqld.h"  // slave_trans_retries
#include "sql/rpl_mi.h"  // Master_info
#include "sql/rpl_rli.h"  // Relay_log_info

namespace mysql::csa {

Queued_transaction_reader::Queued_transaction_reader(int instance_id,
                                                     Relay_log_info *rli,
                                                     Trx_envelope_queue *queue)
    : m_instance_id(instance_id),
      m_rli(rli),
      m_queue(queue),
      m_stat_monitor(scheduler::Statistics_monitor::get(instance_id)),
      m_resource_monitor(Resource_monitor::get(instance_id)) {
  m_channel.reset(new Channel(rli->mi->get_channel(), instance_id,
                              rli->get_commit_order_manager()));
}

Queued_transaction_reader::Queued_transaction_reader(Trx_envelope_queue *queue)
    : m_instance_id(0),
      m_rli(nullptr),
      m_queue(queue),
      m_stat_monitor(scheduler::Statistics_monitor::get(0)),
      m_resource_monitor(Resource_monitor::get(0)) {
  // Test-only constructor.
}

Queued_transaction_reader::~Queued_transaction_reader() = default;

Job_ptr Queued_transaction_reader::read() {
  if (is_stopped()) {
    return nullptr;
  }

  // Report the coordinator as caught up while it blocks below waiting for the
  // next transaction, mirroring the on-disk reader.
  if (m_rli != nullptr)
    concurrency::set_thd_stage(m_rli->info_thd,
                               stage_replica_has_read_all_relay_log);

  // Sweeps the committed head envelope,and dispatches the next uncommitted transaction.
  Transaction_envelope *envelope = m_queue->sweep_and_dispatch();
  if (envelope == nullptr) {
    return nullptr;
  }

  // A transaction is available again: restore the working stage.
  if (m_rli != nullptr)
    concurrency::set_thd_stage(m_rli->info_thd, stage_csa_working);

  // Take a shared ownership copy of the transaction
  std::shared_ptr<Fetchable_transaction> fetchable;
  Trx_payload *payload = envelope->payload();
  if (payload != nullptr) {
    fetchable = payload->fetchable();
  }

  Job_applier *job =
      new Job_applier(m_channel.get(), slave_trans_retries, fetchable,
                      m_stat_monitor, m_resource_monitor);
  return job;
}

bool Queued_transaction_reader::is_stopped() const {
  return m_is_error || m_stopped || m_queue->is_stopped();
}

bool Queued_transaction_reader::is_error() const { return m_is_error; }

void Queued_transaction_reader::stop() {
  m_stopped = true;
  // Stop the APPLIER role.
  m_queue->stop(Trx_envelope_queue::Scope::APPLIER);
}

}  // namespace mysql::csa
