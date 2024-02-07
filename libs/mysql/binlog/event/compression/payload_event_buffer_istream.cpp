/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"

#include "mysql/binlog/event/byteorder.h"
#include "mysql/binlog/event/string/concat.h"
#include "mysql/binlog/event/wrapper_functions.h"

using mysql::binlog::event::string::concat;

namespace mysql::binlog::event::compression {

Payload_event_buffer_istream::Payload_event_buffer_istream(
    const Transaction_payload_event &transaction_payload_log_event,
    Size_t default_buffer_size, const Memory_resource_t &memory_resource)
    : Payload_event_buffer_istream(
          reinterpret_cast<const Char_t *>(
              transaction_payload_log_event.get_payload()),
          transaction_payload_log_event.get_payload_size(),
          transaction_payload_log_event.get_compression_type(),
          default_buffer_size, memory_resource) {
  m_grow_calculator.set_max_size(max_log_event_size);
}

Payload_event_buffer_istream::Payload_event_buffer_istream(
    const std::shared_ptr<const Transaction_payload_event> &tple,
    Size_t default_buffer_size, const Memory_resource_t &memory_resource)
    : Payload_event_buffer_istream(*tple, default_buffer_size,
                                   memory_resource) {
  // Keep a reference just to protect deletion.
  // Nolint: this cannot be an initializer since we delegate
  // initialization to another constructor.
  // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
  m_tple = tple;

  m_grow_calculator.set_max_size(max_log_event_size);
}

Payload_event_buffer_istream::~Payload_event_buffer_istream() {
#ifndef NDEBUG
  // Payload_event_buffer_istream has reported error or EOF, but the
  // calling code has not checked which case it is. This is probably a
  // programming mistake. Remember to always check for error after a
  // read loop has ended.
  assert(!m_outstanding_error);
#endif
}

bool Payload_event_buffer_istream::operator!() const {
  return m_status != Status_t::success;
}

Payload_event_buffer_istream::operator bool() const {
  return m_status == Status_t::success;
}

Payload_event_buffer_istream::Status_t
Payload_event_buffer_istream::get_status() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return m_status;
}

void Payload_event_buffer_istream::set_status(const Status_t status) {
  BAPI_TRACE;
  BAPI_LOG("info", "status=" << status);
#ifndef NDEBUG
  if (status != Status_t::success) m_outstanding_error = true;
#endif
  m_status = status;
}

bool Payload_event_buffer_istream::has_error() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return m_status != Status_t::end && m_status != Status_t::success;
}

std::string Payload_event_buffer_istream::get_error_str() const {
#ifndef NDEBUG
  m_outstanding_error = false;
#endif
  return m_error_str;
}

void Payload_event_buffer_istream::set_error_str(const std::string &s) {
  BAPI_TRACE;
  BAPI_LOG("info", "Error: " << s);
  m_error_str = s;
}

void Payload_event_buffer_istream::initialize() {
  try {
    m_decompressor = Factory_t::build_decompressor(m_compression_algorithm,
                                                   m_memory_resource);
    if (m_decompressor == nullptr) {
      set_error_str(
          concat("Unknown compression algorithm in Payload_log_event: ",
                 m_compression_algorithm, "."));
      set_status(Status_t::corrupted);
      return;
    }
    m_decompressor->feed(m_compressed_buffer, m_compressed_buffer_size);
  } catch (const std::bad_alloc &) {
    set_error_str(
        "Out of memory while allocating decompressor in Payload_log_event.");
    set_status(Status_t::out_of_memory);
  }
}

void Payload_event_buffer_istream::update_buffer() {
  if (m_status != Status_t::success) {
    BAPI_LOG("info", "returning pre-existing status "
                         << (int)m_status << " with error " << get_error_str());
    return;
  }

  // Allocate a new buffer if it has not yet been allocated, or if
  // caller still holds a reference to the existing one.
  if (m_managed_buffer_ptr.use_count() != 1) {
#ifndef NDEBUG
    // "nolint": clang-tidy reports an unnecessary warning since 'if'
    // and 'else' branches may compile to the same. Suppressing that.
    // NOLINTBEGIN(bugprone-branch-clone)
    if (m_managed_buffer_ptr.use_count() == 0)
      BAPI_LOG("info", "Allocating managed buffer for the first time.");
    else
      BAPI_LOG("info",
               "Allocating new managed buffer. "
               "The previous one cannot be reused since there are "
                   << m_managed_buffer_ptr.use_count()
                   << ">1 shared pointer references to "
                      "it.");
      // NOLINTEND(bugprone-branch-clone)
#endif
    try {
      auto allocator = Allocator_t<Managed_buffer_t>(m_memory_resource);
      auto *managed_buffer = allocator.allocate(1);
      new (managed_buffer)
          Managed_buffer_t(m_default_buffer_size, m_memory_resource);
      m_managed_buffer_ptr.reset(managed_buffer, allocator.get_deleter());
    } catch (const std::bad_alloc &) {
      set_error_str(
          "Out of memory while allocating output buffer in "
          "Payload_log_event.");
      set_status(Status_t::out_of_memory);
      return;
    }
  } else {
    BAPI_LOG("info", "Reusing and overwriting existing managed buffer.");
  }
  m_managed_buffer_ptr->set_position(0);
  // Combine the compressor's Grow_constraint with the user-specified
  // Grow_policy
  const auto &decompressor_constraint =
      m_decompressor->get_grow_constraint_hint();
  const auto grow_calculator =
      decompressor_constraint.combine_with(m_grow_calculator);
  m_managed_buffer_ptr->set_grow_calculator(grow_calculator);
}

void Payload_event_buffer_istream::read_event() {
  // Read up to and including the event length.
  constexpr uint32_t header_length = EVENT_LEN_OFFSET + 4;
  uint32_t read_length = header_length;
  auto status = m_decompressor->decompress(*m_managed_buffer_ptr, read_length);
  if (status == Status_t::success) {
    // Read up to and including the type code.
    Event_reader reader{reinterpret_cast<const char *>(
                            m_managed_buffer_ptr->read_part().begin()),
                        read_length};
    reader.go_to(EVENT_TYPE_OFFSET);
    auto event_type = reader.read<uint8_t>();
    // Error is impossible; we decompressed EVENT_LEN_OFFSET+4 bytes and
    // EVENT_LEN_OFFSET > EVENT_TYPE_OFFSET.
    assert(!reader.get_error());

    // Check validity of type code.
    if (event_type == mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT) {
      // A valid TPLE cannot contain a TPLE, so detect it and error
      // out.  Without such a check, a maliciously crafted TPLE may
      // contain itself compressed (a "quine"), which might lead to
      // infinite loops or infinite recursion in API client code that
      // uses the iterator, like:
      //   void handle_event(Log_event *e):
      //     if e is a TRANSACTION_PAYLOAD_EVENT:
      //       for each event e' contained in the payload of e:
      //         handle_event(e');
      set_error_str(
          "Payload_log_event corrupted: contains an embedded "
          "Payload_log_event");
      set_status(Status_t::corrupted);
      return;
    }

    // Decode event length.
    reader.go_to(EVENT_LEN_OFFSET);
    read_length = reader.read<uint32_t>();
    if (read_length < LOG_EVENT_HEADER_LEN) {
      set_error_str(concat(
          "Length field of embedded event in Payload_log_event is only ",
          read_length, " bytes, but ", LOG_EVENT_HEADER_LEN, " are required."));
      set_status(Status_t::corrupted);
      return;
    }
    // Error is impossible; we have EVENT_LEN_OFFSET+4 bytes.
    assert(!reader.get_error());

    // Decompress the rest of the event.
    status = m_decompressor->decompress(*m_managed_buffer_ptr,
                                        read_length - header_length);
    if (status == Status_t::end) {
      // The read operation encountered 'END' after reading just a
      // partial header, so the event as a whole was 'TRUNCATED'.
      status = Status_t::truncated;
    }
  }

  switch (status) {
    case Status_t::success:
      return;
    case Status_t::end:
      set_status(status);
      return;
    case Status_t::truncated:
      set_error_str(
          "Payload_log_event corrupted: the compressed payload has been "
          "truncated.");
      // This is a form of corruption; don't distinguish the error status.
      set_status(Status_t::corrupted);
      return;
    case Status_t::corrupted:
      set_error_str(
          "Payload_log_event corrupted: compression stream is "
          "corrupted.");
      set_status(status);
      return;
    case Status_t::exceeds_max_size: {
      set_error_str(
          concat("Length field of embedded event in Payload_log_event is ",
                 read_length, " bytes, exceeding the maximum of ",
                 m_managed_buffer_ptr->get_grow_calculator().get_max_size(),
                 " bytes."));
      set_status(status);
      return;
    }
    case Status_t::out_of_memory:
      set_error_str(
          "Out of memory while decompressing event embedded in "
          "Payload_log_event.");
      set_status(status);
      return;
    default:
      // Can't happen
      break;
  }
  assert(false);  // Can't happen
  set_status(Status_t::corrupted);
}

Payload_event_buffer_istream &Payload_event_buffer_istream::operator>>(
    Buffer_ptr_t &out) {
  // Remove one from the use_count. In the common case where it is the
  // only other pointer to m_managed_buffer_ptr, this allows us to
  // reuse m_managed_buffer_ptr.  Also, in the less common case where
  // user passes the last pointer to some other buffer, we free that
  // memory before we allocate more.
  out.reset();
  // Produce the object in m_managed_buffer_ptr.
  next();
  // On success, share the read part with the caller.
  if (m_status == Status_t::success)
    out =
        Buffer_ptr_t(m_managed_buffer_ptr, &m_managed_buffer_ptr->read_part());
  return *this;
}

Payload_event_buffer_istream &Payload_event_buffer_istream::operator>>(
    Managed_buffer_ptr_t &out) {
  // Remove one from the use_count. In the common case where it is the
  // only other pointer to m_managed_buffer_ptr, this allows us to
  // reuse m_managed_buffer_ptr.  Also, in the less common case where
  // user passes the last pointer to some other buffer, we free that
  // memory before we allocate more.
  out.reset();
  // Produce the object in m_managed_buffer_ptr.
  next();
  // On success, share the managed buffer with the caller.
  if (m_status == Status_t::success) out = m_managed_buffer_ptr;
  return *this;
}

void Payload_event_buffer_istream::next() {
  BAPI_TRACE;

  // If we failed once, we failed and can't be repaired.  (It would
  // have been "nice" if, after `out_of_memory` or `exceeds_max_size`,
  // users had the chance to free memory or increase the max size and
  // then retry.  But `read_event` is not idempotent; it may have
  // decompressed a half event header before the failure, in which
  // case it has already advanced the stream, making it impossible to
  // resume.)
  if (m_status != Status_t::success) return;
  update_buffer();
  if (m_status != Status_t::success) return;
  read_event();
  BAPI_LOG("info", "Status: " << debug_string(m_status));
}

const Payload_event_buffer_istream::Grow_calculator_t &
Payload_event_buffer_istream::get_grow_calculator() const {
  return m_grow_calculator;
}

void Payload_event_buffer_istream::set_grow_calculator(
    const Payload_event_buffer_istream::Grow_calculator_t &grow_calculator) {
  m_grow_calculator = grow_calculator;
}

}  // namespace mysql::binlog::event::compression
