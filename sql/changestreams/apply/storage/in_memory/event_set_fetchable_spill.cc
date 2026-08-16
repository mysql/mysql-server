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

#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"

#include <cassert>
#include <sstream>
#include <utility>

#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/log_event.h"    // Format_description_log_event
#include "sql/mysqld.h"       // opt_replica_sql_verify_checksum
#include "sql/psi_memory_resource.h"
#include "sql/sql_class.h"  // THD

using namespace mysql::binlog::event::compression;
using namespace binlog;

namespace mysql::csa {

namespace {
const std::string kEmptyString{};
}  // namespace

Event_set_fetchable_spill::Event_set_fetchable_spill(
    bool is_trx, Event_set_fetchable::Log_event_ptr fde,
    std::string relay_log_dir, Transaction_envelope *owner_envelope,
    bool streaming_open)
    : m_is_trx(is_trx),
      m_fde_base(std::move(fde)),
      m_envelope(owner_envelope),
      m_reader(opt_replica_sql_verify_checksum),
      m_stream_open(streaming_open),
      m_stream_sealed(!streaming_open),
      m_stream_truncated(false) {
  m_status = Return_status::ok;
  m_fde = dynamic_cast<Event_set_fetchable::Fde_ptr>(m_fde_base.get());
  assert(m_fde_base);
  assert(m_fde != nullptr);

  // Provision the backing spill file and write the relay-log prefix.
  std::shared_ptr<Format_description_log_event> typed_fde(m_fde_base, m_fde);
  m_writer = std::make_unique<Spill_file_writer>(std::move(typed_fde),
                                                 std::move(relay_log_dir));
  if (m_writer->open() || m_writer->flush()) {
    // Provisioning failed: record the error and leave the stream un-open so
    // append_event() short-circuits.
    // TODO: Surface the error.
    m_failure_msg.assign(m_writer->get_error_str());
    m_status = Return_status::error;
    m_stream_open = false;
    m_stream_sealed = true;
    return;
  }
  // Where the header (magic + FDE) ends and where next read starts from.
  m_start_file_pos = m_writer->end_position();
  m_published_end_file_pos = m_writer->end_position();
}

Event_set_fetchable_spill::~Event_set_fetchable_spill() { safe_close_reader(); }

Event_set_fetchable::Fde_ptr Event_set_fetchable_spill::get_fde() {
  return m_fde;
}

bool Event_set_fetchable_spill::is_trx() const { return m_is_trx; }

void Event_set_fetchable_spill::append_event(const char *buf, std::size_t len,
                                             bool seal_after) {
  // Reject once the stream is closed to appends (sealed/truncated/error).
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    if (is_error() || !m_stream_open || m_stream_sealed || m_stream_truncated) {
      return;
    }
  }

  // Write the raw bytes and flush so a concurrent reader can see them.
  if (m_writer->append_raw(buf, len) || m_writer->flush()) {
    m_failure_msg.assign(m_writer->get_error_str());
    m_status = Return_status::error;
    // On a write error, truncate the stream (which also wakes any parked
    // reader) and report.
    // TODO: Surface the error.
    set_stream_truncated();
    return;
  }

  // Publish the new readable end position and seal if this was the terminal
  // event.
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_published_end_file_pos = m_writer->end_position();
    if (seal_after) {
      m_stream_sealed = true;
    }
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_spill::seal_stream() {
  // The single authoritative seal for the transaction's reception.
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_spill::set_stream_truncated() {
  // Mark the owning transaction truncated FIRST, then this batch. Truncation
  // must reach both levels:
  //   - set_fetching_truncated() makes the consumer's wait/fetch observe it so
  //     the worker rolls back instead of committing a partial transaction;
  //   - the batch flags + m_stream_cv notify below wake a worker parked in this
  //     batch's wait_next().
  if (m_fetchable_trx != nullptr) m_fetchable_trx->set_fetching_truncated();
  // Mark the owning envelope truncated so the sweep reclaims this slot.
  if (m_envelope != nullptr) m_envelope->set_truncated();
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_truncated = true;
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_spill::safe_open_reader() {
  safe_close_reader();
  // Open at the first-event offset so read_fdle() consumes the FDE prefix and
  // installs the file's FDE, leaving the reader positioned at the first event.
  if (m_reader.open(m_writer->file_name().c_str(), m_start_file_pos)) {
    std::stringstream ss;
    ss << "Spill event set: could not open spill file: "
       << m_writer->file_name() << " @ " << m_start_file_pos;
    m_failure_msg.assign(ss.str());
    m_status = Return_status::error;
    return;
  }
  m_is_initialized = true;
}

void Event_set_fetchable_spill::safe_close_reader() {
  if (m_is_initialized) {
    m_reader.close();
  }
  m_is_initialized = false;
}

void Event_set_fetchable_spill::start_reading() {
  safe_open_reader();
  if (is_error()) {
    return;
  }
  m_input_stream.reset(
      new Stream_type(m_reader, psi_memory_resource(key_decompressing_stream)));
  if (!m_input_stream) {
    m_failure_msg.assign("Spill event set: out of memory");
    m_status = Return_status::error;
    safe_close_reader();
    return;
  }
}

bool Event_set_fetchable_spill::wait_for_event_availability() {
  if (decompressing()) {
    return true;
  }
  while (true) {
    std::unique_lock<std::mutex> lock(m_stream_mutex);
    // More bytes have been published than the reader has consumed: readable.
    if (m_reader.position() < m_published_end_file_pos) {
      return true;
    }
    if (m_stream_truncated || m_stream_sealed) {
      m_is_done = true;
      safe_close_reader();
      return false;
    }
    m_stream_cv.wait(lock);
  }
}

bool Event_set_fetchable_spill::wait_next() {
  if (is_done() || is_error()) {
    safe_close_reader();
    return false;
  }
  if (!m_is_initialized) {
    start_reading();
    if (is_error()) {
      return false;
    }
  }
  return wait_for_event_availability();
}

std::optional<Managed_event> Event_set_fetchable_spill::fetch_from_stream() {
  Log_event_ptr current_event;
  *m_input_stream >> current_event;
  if (m_input_stream->has_error()) {
    using Status_t = Decompressing_event_object_istream::Status_t;
    switch (m_input_stream->get_status()) {
      case Status_t::out_of_memory:
        m_failure_msg.assign(
            "Spill event set: out of memory while decompressing events");
        m_status = Return_status::error;
        break;
      case Status_t::exceeds_max_size:
      case Status_t::corrupted:
      case Status_t::truncated:
        m_failure_msg.assign(m_input_stream->get_error_str().c_str());
        m_status = Return_status::error;
        break;
      case Status_t::success:
      case Status_t::end:
        if (!decompressing()) {
          m_is_done = true;
        } else {
          m_decompressing = false;
        }
        break;
    }
  }

  if (decompressing() && current_event &&
      current_event->get_type_code() == mysql::binlog::event::XID_EVENT) {
    // The decompressing stream does not always detect end-of-stream; the XID
    // event terminates the decompressed transaction.
    m_decompressing = false;
    m_is_done = true;
    safe_close_reader();
  }

  if (current_event && current_event->get_type_code() ==
                           mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT) {
    assert(!decompressing());
    m_decompressing = true;
    // Skip the wrapper and return the first decompressed event.
    return fetch_from_stream();
  }

  if (is_error()) {
    safe_close_reader();
    return {};
  }

  if (!current_event) {
    // Ran out of readable bytes. If the stream is sealed or
    // truncated, hitting the end unexpectedly is an error.
    if (!m_stream_sealed && !m_stream_truncated) {
      return {};
    }
    m_failure_msg.assign("Spill event set: unexpected end of the stream");
    m_status = Return_status::error;
    safe_close_reader();
    return {};
  }

  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    if (m_stream_sealed && !m_decompressing &&
        m_reader.position() >= m_published_end_file_pos) {
      m_is_done = true;
      safe_close_reader();
    }
  }

  return Managed_event(current_event, true);
}

std::optional<Managed_event> Event_set_fetchable_spill::fetch_next() {
  if (is_done() || is_error()) {
    return {};
  }
  return fetch_from_stream();
}

void Event_set_fetchable_spill::set_success() {
  // Commit hook. commit() marks the envelope committed and resets its payload
  // under only the per-envelope mutex. Skipped when the worker's THD was killed.
  const THD *thd = current_thd;
  // Prevent commit hook invoked during commit_order_manager fail through.
  if (thd != nullptr && thd->is_killed()) {
    return;
  }
  if (m_envelope != nullptr) m_envelope->commit();
}

void Event_set_fetchable_spill::reset(bool) {
  // Close the reader and clear consumer/error state so the transaction can be
  // re-read from the start (worker retry). The published position and sealed
  // flag reflect what was received and are left intact.
  safe_close_reader();
  std::lock_guard<std::mutex> lock(m_stream_mutex);
  m_decompressing = false;
  m_is_done = false;
  m_status = Return_status::ok;
  m_failure_msg.assign("");
  m_stream_truncated = false;
}

std::size_t Event_set_fetchable_spill::published_end_position() const {
  std::lock_guard<std::mutex> lock(m_stream_mutex);
  return m_published_end_file_pos;
}

bool Event_set_fetchable_spill::is_sealed() const {
  std::lock_guard<std::mutex> lock(m_stream_mutex);
  return m_stream_sealed;
}

bool Event_set_fetchable_spill::is_stream_truncated() const {
  std::lock_guard<std::mutex> lock(m_stream_mutex);
  return m_stream_truncated;
}

const std::string &Event_set_fetchable_spill::spill_file_name() const {
  return m_writer ? m_writer->file_name() : kEmptyString;
}

const std::string &Event_set_fetchable_spill::get_error_str() const {
  return m_failure_msg;
}

bool Event_set_fetchable_spill::is_done() const {
  return m_is_done && !is_error();
}

bool Event_set_fetchable_spill::is_error() const {
  return m_status == Return_status::error;
}

}  // namespace mysql::csa
