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

#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_cache.h"

#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"
#include "sql/psi_memory_resource.h"

using namespace mysql::binlog::event::compression;
using namespace binlog;

namespace mysql::csa {

Event_set_fetchable_cache::Event_set_fetchable_cache(
    Event_set_fetchable_cache::Event_set_type &&events, bool is_trx,
    Event_set_fetchable::Log_event_ptr fde,
    Relay_log_deleter_handle delete_file_handle, bool streaming_open)
    : m_events(std::move(events)),
      m_is_trx(is_trx),
      m_fde_base(fde),
      m_delete_file_handle(delete_file_handle),
      m_stream_open(streaming_open),
      m_stream_sealed(!streaming_open),
      m_stream_truncated(false) {
  m_fde = dynamic_cast<Event_set_fetchable::Fde_ptr>(m_fde_base.get());
  assert(m_fde_base);
  assert(m_fde != nullptr);
  reset(false);
  m_delete_file_handle->add_subscriber();
}

void Event_set_fetchable_cache::set_success() {
  m_delete_file_handle->set_subscriber_success();
}

Event_set_fetchable::Fde_ptr Event_set_fetchable_cache::get_fde() {
  return m_fde;
}

bool Event_set_fetchable_cache::is_trx() const { return m_is_trx; }

void Event_set_fetchable_cache::append_event(IReader_event_ptr event,
                                             bool seal_after) {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    if (!m_stream_open || m_stream_sealed || m_stream_truncated) {
      return;
    }
    m_events.push_back(std::move(event));
    if (seal_after) {
      m_stream_sealed = true;
    }
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_cache::seal_stream() {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_cache::set_stream_truncated() {
  {
    std::lock_guard<std::mutex> lock(m_stream_mutex);
    m_stream_truncated = true;
    m_stream_sealed = true;
  }
  m_stream_cv.notify_one();
}

void Event_set_fetchable_cache::start_decompression() {
  assert(m_compressed_event);
  m_decompressing = true;
  m_compressed_event_ptr =
      dynamic_cast<Transaction_payload_log_event *>(m_compressed_event.get());
  m_decompressing_stream.reset(
      new Stream_type(*m_compressed_event_ptr, *m_fde,
                      psi_memory_resource(key_decompressing_stream)));
  if (!m_decompressing_stream) {
    m_failure_msg.assign("Fetchable event set: Out of memory");
    m_status = Return_status::error;
  }
}

void Event_set_fetchable_cache::end_decompression() {
  m_decompressing = false;
  m_decompressing_stream.reset();
  m_compressed_event_ptr = nullptr;
  m_compressed_event.reset();
}

std::optional<Event_set_fetchable_cache::Log_event_ptr>
Event_set_fetchable_cache::decompress() {
  Log_event_ptr current_event;
  *m_decompressing_stream >> current_event;
  if (m_decompressing_stream->has_error()) {
    using Status_t = Decompressing_event_object_istream::Status_t;
    switch (m_decompressing_stream->get_status()) {
      case Status_t::out_of_memory:
        m_failure_msg.assign(
            "Fetchable event set: out of memory while decompressing events");
        m_status = Return_status::error;
        break;
      case Status_t::exceeds_max_size:
      case Status_t::corrupted:
      case Status_t::truncated:
        m_failure_msg.assign(m_decompressing_stream->get_error_str().c_str());
        m_status = Return_status::error;
        break;
      case Status_t::success:
      case Status_t::end:
        end_decompression();
        break;
    }
  }

  if (current_event &&
      current_event->get_type_code() == mysql::binlog::event::XID_EVENT) {
    end_decompression();
  }

  if (current_event && !is_error()) {
    return current_event;
  }
  return {};
}

bool Event_set_fetchable_cache::wait_next() {
  while (true) {
    if (is_error() || is_done()) {
      return false;
    }

    if (m_decompressing) {
      return true;
    }

    {
      std::unique_lock<std::mutex> lock(m_stream_mutex);
      while (m_event_id >= m_events.size()) {
        if (m_stream_truncated || m_stream_sealed) {
          m_is_done = true;
          return false;
        }
        m_stream_cv.wait(lock);
      }
    }
    return true;
  }
}

std::optional<Managed_event> Event_set_fetchable_cache::fetch_next() {
  while (true) {
    if (is_error() || is_done()) {
      return {};
    }

    if (m_decompressing) {
      auto decompressed_result = decompress();
      if (!decompressed_result.has_value()) {
        if (is_error()) return {};
        continue;
      }
      {
        std::lock_guard<std::mutex> lock(m_stream_mutex);
        if (!m_decompressing && m_stream_sealed &&
            m_event_id == m_events.size()) {
          m_is_done = true;
        }
      }
      return Managed_event(decompressed_result.value(), false);
    }

    IReader_event_ptr reader_event;
    bool is_last_in_batch{false};
    {
      std::lock_guard<std::mutex> lock(m_stream_mutex);
      if (m_event_id >= m_events.size()) {
        return {};
      }
      reader_event = m_events[m_event_id++];
      is_last_in_batch = m_stream_sealed && m_event_id == m_events.size();
    }

    auto current_event = reader_event->decode();
    if (current_event->get_type_code() ==
        mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT) {
      m_compressed_event = current_event;
      start_decompression();
      if (is_error()) return {};
      continue;
    }
    if (is_last_in_batch) {
      m_is_done = true;
    }
    return Managed_event(current_event, false);
  }
}

const std::string &Event_set_fetchable_cache::get_error_str() const {
  return m_failure_msg;
}

bool Event_set_fetchable_cache::is_done() const {
  return m_is_done && !is_error();
}

bool Event_set_fetchable_cache::is_error() const {
  return m_status == Return_status::error;
}

Event_set_fetchable_cache::~Event_set_fetchable_cache() {}

void Event_set_fetchable_cache::reset(bool reset_events) {
  std::lock_guard<std::mutex> lock(m_stream_mutex);
  end_decompression();
  m_event_id = 0;
  m_is_done = false;
  m_status = Return_status::ok;
  if (reset_events) {
    for (auto &event : m_events) {
      assert(m_fde != nullptr);
      event->reset(m_fde);
    }
  }
  m_failure_msg.assign("");
  m_stream_truncated = false;
  if (!m_stream_open) {
    m_stream_sealed = true;
  }
}

}  // namespace mysql::csa
