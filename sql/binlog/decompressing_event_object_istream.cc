/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "sql/binlog/decompressing_event_object_istream.h"
#include "scope_guard.h"    // Variable_scope_guard
#include "sql/mysqld.h"     // PSI_stage_info
#include "sql/sql_class.h"  // current_thd, THD

#include "libbinlogevents/include/compression/payload_event_buffer_istream.h"

namespace binlog {

Decompressing_event_object_istream::Decompressing_event_object_istream(
    IBasic_binlog_file_reader &reader)
    : m_binlog_reader(&reader),
      m_get_format_description_event([&]() -> Fde_ref_t {
        return m_binlog_reader->format_description_event();
      }) {}

Decompressing_event_object_istream::Decompressing_event_object_istream(
    const Tple_ptr_t &transaction_payload_log_event,
    Fde_ref_t format_description_event)
    : m_binlog_reader(nullptr),
      m_get_format_description_event(
          [&]() -> Fde_ref_t { return format_description_event; }) {
  begin_payload_event(transaction_payload_log_event);
}

Decompressing_event_object_istream::Decompressing_event_object_istream(
    const Transaction_payload_log_event &transaction_payload_log_event,
    Fde_ref_t format_description_event)
    : m_binlog_reader(nullptr),
      m_get_format_description_event(
          [&]() -> Fde_ref_t { return format_description_event; }) {
  begin_payload_event(transaction_payload_log_event);
}

#ifndef NDEBUG
Decompressing_event_object_istream::~Decompressing_event_object_istream() {
  // operator bool or operator! has reported error or EOF, but the
  // calling code has not checked which case it is. This is probably a
  // programming mistake. Remember to always check for error after a
  // read loop has ended.
  assert(!m_outstanding_error);
}
#endif

void Decompressing_event_object_istream::set_verify_checksum(
    bool verify_checksum) {
  m_verify_checksum = verify_checksum;
}

Decompressing_event_object_istream::operator bool() const { return !m_end; }

bool Decompressing_event_object_istream::operator!() const { return m_end; }

std::string Decompressing_event_object_istream::get_error_str() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return m_error_str;
}

bool Decompressing_event_object_istream::has_error() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return !m_error_str.empty();
}

Decompressing_event_object_istream::Status_t
Decompressing_event_object_istream::get_status() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return m_status;
}

static Decompressing_event_object_istream::Status_t binlog_read_error_to_status(
    Binlog_read_error::Error_type e) {
  switch (e) {
    case Binlog_read_error::SUCCESS:
      return Decompressing_event_object_istream::Status_t::success;
    case Binlog_read_error::READ_EOF:
      return Decompressing_event_object_istream::Status_t::end;
    case Binlog_read_error::MEM_ALLOCATE:
      return Decompressing_event_object_istream::Status_t::out_of_memory;
    case Binlog_read_error::BOGUS:
    case Binlog_read_error::SYSTEM_IO:
    case Binlog_read_error::EVENT_TOO_LARGE:
    case Binlog_read_error::CHECKSUM_FAILURE:
    case Binlog_read_error::INVALID_EVENT:
    case Binlog_read_error::CANNOT_OPEN:
    case Binlog_read_error::HEADER_IO_FAILURE:
    case Binlog_read_error::BAD_BINLOG_MAGIC:
    case Binlog_read_error::INVALID_ENCRYPTION_HEADER:
    case Binlog_read_error::CANNOT_GET_FILE_PASSWORD:
    case Binlog_read_error::READ_ENCRYPTED_LOG_FILE_IS_NOT_SUPPORTED:
    case Binlog_read_error::ERROR_DECRYPTING_FILE:
      return Decompressing_event_object_istream::Status_t::corrupted;
    case Binlog_read_error::TRUNC_EVENT:
    case Binlog_read_error::TRUNC_FD_EVENT:
      return Decompressing_event_object_istream::Status_t::truncated;
  }
  assert(false);  // not reached
  return Decompressing_event_object_istream::Status_t::success;
}

raii::Targeted_stringstream Decompressing_event_object_istream::error_stream(
    Status_t status) {
  DBUG_TRACE;
  m_status = status;
  raii::Targeted_stringstream stream(
      m_error_str, "",
      []([[maybe_unused]] const std::string &s) { DBUG_LOG("info", s); });
  if (m_embedded_event_number != 0)
    stream << "Error reading embedded Log_event #" << m_embedded_event_number
           << " from Payload event";
  else
    stream << "Error reading Log_event";
  stream << " at position " << m_event_position << ": ";
  m_end = true;
  return stream;
}

const Decompressing_event_object_istream::Grow_calculator_t &
Decompressing_event_object_istream::get_grow_calculator() const {
  return m_grow_calculator;
}

void Decompressing_event_object_istream::set_grow_calculator(
    const Grow_calculator_t &grow_calculator) {
  m_grow_calculator = grow_calculator;
}

bool Decompressing_event_object_istream::decode_from_buffer(
    Buffer_view_t &buffer_view, Event_ptr_t &out) {
  DBUG_TRACE;
  auto fde = m_get_format_description_event();
  Variable_scope_guard disable_checksum_guard{fde.footer()->checksum_alg};
  // Events contained in a Transaction_payload_log_event never have a
  // checksum (regardless of configuration). So we have to temporarily
  // disable checksums while decoding such an inner event.  The API to
  // control if we verify checksums is to set this member variable in
  // the footer of the format_description_log_event.
  fde.footer()->checksum_alg = binary_log::BINLOG_CHECKSUM_ALG_OFF;
  Log_event *ev{nullptr};
  auto error = binlog_event_deserialize(buffer_view.data(), buffer_view.size(),
                                        &fde, m_verify_checksum, &ev);
  if (error != Binlog_read_error::SUCCESS) {
    uint event_type = buffer_view.data()[EVENT_TYPE_OFFSET];
    error_stream(binlog_read_error_to_status(error))
        << "Failed decoding event of type "
        << Log_event::get_type_str(event_type) << " (" << event_type
        << "): " << Binlog_read_error(error).get_str();
    return true;
  }
  ev->common_header->log_pos = m_transaction_payload_event_offset;
  DBUG_LOG("info", "SUCCESS. returning decompressed event of type "
                       << ev->get_type_str());
  out = Event_ptr_t(ev);
  return false;
}

Decompressing_event_object_istream::Read_status
Decompressing_event_object_istream::read_from_payload_stream(Event_ptr_t &out) {
  DBUG_TRACE;
  if (!m_buffer_istream) {
    // may happen if begin_payload_event failed with OOM
    error_stream(Status_t::out_of_memory)
        << "Out of memory allocating buffer stream";
    return Read_status::error;
  }
  // Update m_grow_calculator. We do it per event, not only when
  // instantiating a payload_event_buffer_istream, so that user can
  // set a Grow_calculator per event if needed.
  m_buffer_istream->set_grow_calculator(m_grow_calculator);
  // Fetch a buffer from the stream
  Buffer_ptr_t buffer_ptr;
  if (*m_buffer_istream >> buffer_ptr) {
    if (decode_from_buffer(*buffer_ptr, out)) return Read_status::error;
    ++m_embedded_event_number;
    return Read_status::success;
  }
  // At this point, we either reached EOF, or there was an error.

  // Error? Then copy the message from the stream and return failure.
  if (m_buffer_istream->has_error()) {
    error_stream(m_buffer_istream->get_status())
        << m_buffer_istream->get_error_str();
    return Read_status::error;
  }

  // Reached EOF in the payload. Then delete the stream and return eof.
  DBUG_LOG("info", "EOF in compressed stream from payload event.");
  m_embedded_event_number = 0;
  m_transaction_payload_event_offset = 0;
  m_buffer_istream.reset();
  return Read_status::eof;
}

bool Decompressing_event_object_istream::read_from_binlog_stream(
    Event_ptr_t &out) {
  DBUG_TRACE;
  assert(m_embedded_event_number == 0);
  m_event_position = m_binlog_reader->position();
  Log_event *ev = m_binlog_reader->read_event_object();
  if (ev == nullptr) {
    auto error = m_binlog_reader->get_error_type();
    assert(error != Binlog_read_error::SUCCESS);
    if (error == Binlog_read_error::READ_EOF) {
      m_status = Status_t::end;
      DBUG_LOG("info", "read_event_object returned nullptr, for EOF.");
    } else {
      DBUG_LOG("info", "read_event_object returned nullptr, for error.");
      error_stream(binlog_read_error_to_status(error))
          << "Failed decoding event: " << m_binlog_reader->get_error_str();
    }
    return true;
  }

  DBUG_LOG("info", "SUCCESS. returning non-compressed event of type "
                       << ev->get_type_str());
  out = Event_ptr_t(ev);

  // If we got a TPLE, prepare to unfold it on next invocation. Return
  // the TPLE itself this time. Share pointer ownership between the
  // Payload_event_buffer_istream and the API client.
  if (ev->get_type_code() == binary_log::TRANSACTION_PAYLOAD_EVENT)
    begin_payload_event(
        std::const_pointer_cast<const Transaction_payload_log_event>(
            std::dynamic_pointer_cast<Transaction_payload_log_event>(out)));

  return false;
}

Decompressing_event_object_istream &
Decompressing_event_object_istream::operator>>(Event_ptr_t &out) {
  DBUG_TRACE;
  // The following call to `out.reset()` is a memory usage
  // optimization in the special case that `out` is the last owner of
  // its object, which is the case in the usual pattern:
  // while (stream >> event_ptr) {
  //   event_ptr->do_something();
  // }
  // This API guarantees that `out` will be replaced by either a valid
  // object, or nullptr (on error or end-of-stream).  Therefore, if
  // `out` is the last owner, its object will be released before
  // returning from this function. By calling `out.reset()` now, we
  // release that memory before we allocate new memory for the new
  // object we will return. Therefore, it ensures that we don't hold
  // two events in memory at the same time.
  out.reset();

  // Read and decode the next event, either from a payload event or
  // from a file stream, depending on the current state.
  //
  // Return false on success; true if error or EOF was reached.
  auto work = [&]() -> bool {
    // If we are processing a TPLE, decompress next event from there.
    if (m_embedded_event_number != 0) {
      switch (read_from_payload_stream(out)) {
        case Read_status::success:
          return false;
        case Read_status::error:
          return true;
        case Read_status::eof:
          break;  // fallthrough to read next event from file
      }
    }
    // If this stream was instantiated as reading from just one TPLE
    // event, not a Binlog_reader that yields multiple events, then we
    // have reached EOF.
    if (m_binlog_reader == nullptr) {
      m_status = Status_t::end;
      return true;
    }
    DBUG_LOG("info", "Reading non-compressed event.");
    return read_from_binlog_stream(out);
  };
  if (work()) {
    m_buffer_istream.reset();
    m_end = true;
#ifndef NDEBUG
    m_outstanding_error = true;
#endif
  }
  return *this;
}

}  // namespace binlog
