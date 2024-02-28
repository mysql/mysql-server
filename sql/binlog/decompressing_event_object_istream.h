/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef BINLOG_DECOMPRESSING_EVENT_OBJECT_ISTREAM_H_
#define BINLOG_DECOMPRESSING_EVENT_OBJECT_ISTREAM_H_

#include <queue>

#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"
#include "mysql/utils/nodiscard.h"
#include "sql/binlog_reader.h"
#include "sql/raii/targeted_stringstream.h"

/// @addtogroup Replication
/// @{
///
/// @file decompressing_event_object_istream.h
///
/// Stream class that yields Log_event objects, including events
/// contained in Transaction_payload_log_events.

namespace binlog {

/// Stream class that yields Log_event objects from a source.  The
/// source can be a Transaction_payload_log_event, in which case it will
/// produce the contained events.  Or it can be a file, in which case it
/// will yield all events in the file, and if there is a
/// Transaction_payload_log_event, it will yield first that and then all
/// the contained events.
///
/// The recommended use pattern is:
///
/// @code
///   while (stream >> event) {
///     // handle event
///   }
///   if (stream.has_error()) {
///     // handle error
///   }
/// @endcode
///
/// This class actually enforces that you call `has_error` after the
/// loop; failure to do so will result in an assertion in debug mode.
/// In the unlikely case that your code doesn't need to check for
/// errors, you can get rid of the assert by calling has_error() and
/// discarding the return value.
class Decompressing_event_object_istream {
 public:
  using Buffer_stream_t =
      mysql::binlog::event::compression::Payload_event_buffer_istream;
  using Buffer_view_t = Buffer_stream_t::Buffer_view_t;
  using Buffer_ptr_t = Buffer_stream_t::Buffer_ptr_t;
  using Event_ptr_t = std::shared_ptr<Log_event>;
  using Tple_ptr_t = std::shared_ptr<const Transaction_payload_log_event>;
  using Fde_ref_t = const mysql::binlog::event::Format_description_event &;
  using Status_t = mysql::binlog::event::compression::Decompress_status;
  using Grow_calculator_t = Buffer_stream_t::Grow_calculator_t;
  using Memory_resource_t = mysql::allocators::Memory_resource;

  /// Construct stream over a file, decompressing payload events.
  ///
  /// This will produce all events in the file, and in addition, each
  /// Transaction_payload_log_event is followed by the contained
  /// events.
  ///
  /// @param reader The source file to read from.
  /// @param memory_resource Instrumented memory allocator object
  explicit Decompressing_event_object_istream(
      IBasic_binlog_file_reader &reader,
      const Memory_resource_t &memory_resource = Memory_resource_t());

  /// Construct stream over a Transaction_payload_log_event.
  ///
  /// This will produce all events contained in the event, but not the
  /// event itself.
  ///
  /// This Decompressing_event_object_istream will, during its entire
  /// life time, hold shared ownership of the Transaction_payload_log_event.
  ///
  /// @param transaction_payload_log_event The source file to read from.
  ///
  /// @param format_description_event The FD event used to parse events.
  ///
  /// @param memory_resource Instrumented memory allocator object
  Decompressing_event_object_istream(
      const Tple_ptr_t &transaction_payload_log_event,
      Fde_ref_t format_description_event,
      const Memory_resource_t &memory_resource = Memory_resource_t());

  /// Construct stream over a Transaction_payload_log_event.
  ///
  /// This will produce all events contained in the event, but not the
  /// event itself.
  ///
  /// This Decompressing_event_object_istream will, during its entire
  /// life time, hold a pointer to the Transaction_payload_log_event,
  /// and the caller must ensure that the event outlives the stream.
  ///
  /// @param transaction_payload_log_event The source file to read from.
  ///
  /// @param format_description_event The FD event used to parse events.
  ///
  /// @param memory_resource Instrumented memory allocator object
  Decompressing_event_object_istream(
      const Transaction_payload_log_event &transaction_payload_log_event,
      Fde_ref_t format_description_event,
      const Memory_resource_t &memory_resource = Memory_resource_t());

#ifdef NDEBUG
  ~Decompressing_event_object_istream() = default;
#else
  ~Decompressing_event_object_istream();
#endif

  Decompressing_event_object_istream(Decompressing_event_object_istream &) =
      delete;
  Decompressing_event_object_istream(Decompressing_event_object_istream &&) =
      delete;
  Decompressing_event_object_istream &operator=(
      Decompressing_event_object_istream &) = delete;
  Decompressing_event_object_istream &operator=(
      Decompressing_event_object_istream &&) = delete;

  /// Specify whether checksums shall be verified or not.
  ///
  /// @param verify_checksum If true, verify checksums; otherwise
  /// don't.
  void set_verify_checksum(bool verify_checksum = true);

  /// Read an event from the stream.
  ///
  /// @param out Shared pointer to the produced event.  If this is a
  /// Transaction_payload_log_event, the stream will keep a shared
  /// pointer to it, until it has produced all the contained events.
  ///
  /// @return This object.
  Decompressing_event_object_istream &operator>>(Event_ptr_t &out);

  /// Indicate if EOF or error has not happened.
  ///
  /// @retval true last read was successful (or there was no last
  /// read).
  ///
  /// @retval false last read resulted in end-of-stream or error.
  explicit operator bool() const;

  /// Indicate if EOF or error has happened. This is the negation of
  /// `operator bool`.
  ///
  /// @retval false last read was successful, or there was no last
  /// read.
  ///
  /// @retval true last read resulted in end-of-stream or error.
  bool operator!() const;

  /// Return true if an error has happened.
  bool has_error() const;

  /// Return a message describing the last error.
  ///
  /// @retval "" No error
  ///
  /// @retval string Error
  std::string get_error_str() const;

  /// Return the status
  Status_t get_status() const;

  /// Return const reference to Grow_calculator to the internal event buffer.
  const Grow_calculator_t &get_grow_calculator() const;

  /// Set the Grow_calculator for the internal event buffer.
  void set_grow_calculator(const Grow_calculator_t &grow_calculator);

 private:
  /// Stream of events to read from
  IBasic_binlog_file_reader *m_binlog_reader{nullptr};
  /// Whether we should verify checksum. Unused!
  bool m_verify_checksum{false};
  /// Error from last operation
  std::string m_error_str;
  /// True if we have reached EOF, false otherwise.
  bool m_end{false};
  /// Status
  Status_t m_status{Status_t::success};
  /// Position of last event
  my_off_t m_event_position{0};
#ifndef NDEBUG
  /// True if a read has failed but neither `get_error_str` nor
  /// `has_error` has been called.
  mutable bool m_outstanding_error{false};
#endif

  /// Policy for growing buffers in the decompressing stream
  Grow_calculator_t m_grow_calculator;
  /// The decompression stream; non-null while we are positioned in a TPLE.
  std::unique_ptr<Buffer_stream_t> m_buffer_istream{nullptr};
  /// end_log_pos for the currently processed TPLE, if any
  my_off_t m_transaction_payload_event_offset{0};
  /// 0 when not processing a TPLE; N>0 when positioned before the Nth
  /// embedded event of a TPLE.
  int m_embedded_event_number{0};

  Memory_resource_t m_memory_resource;

  /// Return the current FDE.
  std::function<Fde_ref_t()> m_get_format_description_event;

  /// Report an error
  ///
  /// This sets the status as specified, and returns a stringstream to
  /// which the caller shall write a message.
  ///
  /// @param status The status
  ///
  /// @returns Targeted_stringstream object; the error for this stream
  /// will be set to the given status.
  [[NODISCARD]] raii::Targeted_stringstream error_stream(Status_t status);

  /// Prepare to unfold a given Transaction_payload_log_event by
  /// setting state variables and creating the
  /// Payload_event_buffer_istream object.
  ///
  /// This object will not hold ownership of the event. The caller
  /// must ensure that the event outlives the stream.
  ///
  /// @param tple Event to unfold
  void begin_payload_event(const Transaction_payload_log_event &tple) {
    do_begin_payload_event(tple, tple);
  }

  /// Prepare to unfold a given Transaction_payload_log_event by
  /// setting state variables and creating the
  /// Payload_event_buffer_istream object.
  ///
  /// This object will hold shared ownership of the event.
  ///
  /// @param tple Event to unfold
  void begin_payload_event(const Tple_ptr_t &tple) {
    do_begin_payload_event(*tple, tple);
  }

  /// Worker function implementing both forms of begin_payload_event.
  ///
  /// @tparam Event_ref_or_ptr Either reference-to-event or
  /// shared-pointer-to-event.
  ///
  /// @param tple Event to unfold.
  ///
  /// @param ownership_tple Ownership handle for the event. This can
  /// be a shared_ptr if this object should hold shared ownership, or
  /// a reference otherwise.
  template <class Event_ref_or_ptr>
  void do_begin_payload_event(const Transaction_payload_log_event &tple,
                              const Event_ref_or_ptr &ownership_tple) {
    DBUG_TRACE;
    m_transaction_payload_event_offset = tple.header()->log_pos;
    m_embedded_event_number = 1;
    assert(!m_buffer_istream);
    try {
      m_buffer_istream = std::make_unique<Buffer_stream_t>(ownership_tple, 0,
                                                           m_memory_resource);
    } catch (...) {
      // Leave m_buffer_istream empty.
      // Report error on next invocation of `operator>>`.
    }
  }

  /// Worker that deserializes an event from the buffer.
  ///
  /// @param[in] buffer Input byte buffer.
  ///
  /// @param[out] out Pointer to output event.
  ///
  /// @retval false success
  /// @retval true error
  [[NODISCARD]] bool decode_from_buffer(Buffer_view_t &buffer,
                                        Event_ptr_t &out);

  /// Status from read_from_payload_stream
  enum class Read_status { success, eof, error };

  /// Read and decode next event from the Payload_log_event stream
  ///
  /// @param[out] out Pointer to output event.
  ///
  /// @retval success The event was successfully read and decoded
  ///
  /// @retval error An error occurred. `get_error_str` will contain
  /// the reason.
  ///
  /// @retval eof Nothing was read because the read position was at
  /// the end of the event.
  [[NODISCARD]] Read_status read_from_payload_stream(Event_ptr_t &out);

  /// Read and decode the next event from the binlog stream.
  ///
  /// @param[out] out Pointer to the output event.
  ///
  /// @retval false The event was successfully read and decoded.
  ///
  /// @retval true Error or EOF occurred.  In case of error,
  /// `get_error_str` will contain the reason.
  [[NODISCARD]] bool read_from_binlog_stream(Event_ptr_t &out);
};

}  // namespace binlog

/// @} (end of group Replication)

#endif  // ifdef BINLOG_DECOMPRESSING_EVENT_OBJECT_ISTREAM_H_
