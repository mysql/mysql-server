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

#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_relay_log.h"

#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"
#include "mysql/scheduler/logger_stream.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"
#include "sql/log_event.h"
#include "sql/psi_memory_resource.h"

using namespace mysql::binlog::event::compression;
using namespace binlog;

namespace mysql::csa {

Event_set_fetchable_relay_log::Event_set_fetchable_relay_log(
    std::string filename, std::size_t start_file_pos, std::size_t end_file_pos,
    Relay_log_deleter_handle deleter, bool checksum_validation, bool is_trx,
    Event_set_fetchable::Log_event_ptr fde, bool streaming_open)
    : m_file_name(filename),
      m_start_file_pos(start_file_pos),
      m_delete_file_handle(deleter),
      m_reader(checksum_validation),
      m_is_trx(is_trx),
      m_fde_base(fde),
      m_published_end_file_pos(end_file_pos),
      m_stream_open(streaming_open),
      m_stream_sealed(!streaming_open),
      m_stream_truncated(false) {
  m_fde = dynamic_cast<Event_set_fetchable::Fde_ptr>(m_fde_base.get());
  reset(false);
  m_delete_file_handle->add_subscriber();
}

void Event_set_fetchable_relay_log::set_success() {
  m_delete_file_handle->set_subscriber_success();
}

bool Event_set_fetchable_relay_log::decompressing() const {
  return m_decompressing;
}

Event_set_fetchable::Fde_ptr Event_set_fetchable_relay_log::get_fde() {
  return m_fde;
}

void Event_set_fetchable_relay_log::append_event_end(std::size_t end_file_pos,
                                                     bool seal_after) {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    if (!m_stream_open || m_stream_sealed || m_stream_truncated) {
      return;
    }
    if (end_file_pos > m_published_end_file_pos) {
      m_published_end_file_pos = end_file_pos;
    }
    if (seal_after) {
      m_stream_sealed = true;
    }
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_relay_log::seal_stream() {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_relay_log::set_stream_truncated() {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_truncated = true;
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

bool Event_set_fetchable_relay_log::wait_for_event_availability() {
  if (decompressing()) {
    return true;
  }
  while (true) {
    std::unique_lock<std::mutex> lock(m_stream_mutex);
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

bool Event_set_fetchable_relay_log::wait_next() {
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

std::optional<Managed_event>
Event_set_fetchable_relay_log::fetch_from_stream() {
  Log_event_ptr current_event;
  *m_input_stream >> current_event;
  if (m_input_stream->has_error()) {
    using Status_t = Decompressing_event_object_istream::Status_t;
    switch (m_input_stream->get_status()) {
      case Status_t::out_of_memory:
        m_failure_msg.assign(
            "Fetchable event set: out of memory while decompressing events");
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
    // decompressing stream won't always detect end of stream, we need to
    // check returned event
    m_decompressing = false;
    m_is_done = true;
    safe_close_reader();
  }

  if (current_event && current_event->get_type_code() ==
                           mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT) {
    assert(!decompressing());
    m_decompressing = true;
    // skip this and return next
    return fetch_from_stream();
  }

  if (is_error()) {
    safe_close_reader();
    return {};
  }

  if (!current_event) {
    // If stream is open and not truncated, wait for more data.
    if (!m_stream_sealed && !m_stream_truncated) {
      return {};
    }
    m_failure_msg.assign("Unexpected end of the stream");
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

std::optional<Managed_event> Event_set_fetchable_relay_log::fetch_next() {
  if (is_done() || is_error()) {
    return {};
  }
  return fetch_from_stream();
}

std::string Event_set_fetchable_relay_log::to_string() const {
  std::stringstream ss;
  ss << "file: " << m_file_name << " start_pos: " << m_start_file_pos << " "
     << "end_file_pos: " << m_published_end_file_pos;
  if (is_error()) {
    ss << " got error: " << m_failure_msg;
  }
  return ss.str();
}

const std::string &Event_set_fetchable_relay_log::get_error_str() const {
  return m_failure_msg;
}

bool Event_set_fetchable_relay_log::is_done() const {
  return m_is_done && !is_error();
}

bool Event_set_fetchable_relay_log::is_error() const {
  return m_status == Return_status::error;
}

Event_set_fetchable_relay_log::~Event_set_fetchable_relay_log() {
  safe_close_reader();
}

void Event_set_fetchable_relay_log::start_reading() {
  safe_open_reader();
  if (is_error()) {
    return;
  }
  m_input_stream.reset(
      new Stream_type(m_reader, psi_memory_resource(key_decompressing_stream)));
  if (!m_input_stream) {
    m_failure_msg.assign("Fetchable event set: Out of memory");
    m_status = Return_status::error;
    safe_close_reader();
    return;
  }
}

void Event_set_fetchable_relay_log::reset(bool) {
  m_decompressing = false;
  m_is_done = false;
  m_status = Return_status::ok;
  m_stream_truncated = false;
  safe_close_reader();
}

void Event_set_fetchable_relay_log::safe_open_reader() {
  safe_close_reader();
  if (m_reader.open(m_file_name.c_str(), m_start_file_pos)) {
    std::stringstream ss;
    ss << "Fetchable event set: Could not open relay log file: " << m_file_name
       << " " << m_start_file_pos;
    m_failure_msg.assign(ss.str());
    m_status = Return_status::error;
    return;
  }
  m_is_initialized = true;
}

void Event_set_fetchable_relay_log::safe_close_reader() {
  if (m_is_initialized) {
    m_reader.close();
  }
  m_is_initialized = false;
}

bool Event_set_fetchable_relay_log::is_trx() const { return m_is_trx; }

}  // namespace mysql::csa
