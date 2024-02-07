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

#ifndef BINARY_LOG_TRANSACTION_COMPRESSION_PAYLOAD_EVENT_BUFFER_ISTREAM_H_
#define BINARY_LOG_TRANSACTION_COMPRESSION_PAYLOAD_EVENT_BUFFER_ISTREAM_H_

#include <memory>
#include <string>

#include "mysql/binlog/event/compression/buffer/managed_buffer.h"  // mysql::binlog::event::compression::buffer::Managed_buffer
#include "mysql/binlog/event/compression/decompressor.h"  // mysqlns::compression::Decompressor
#include "mysql/binlog/event/compression/factory.h"  // mysqlns::compression::Factory
#include "mysql/binlog/event/control_events.h"  // Transaction_payload_event
#include "mysql/binlog/event/event_reader.h"    // Event_reader
#include "mysql/binlog/event/nodiscard.h"       // NODISCARD

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{
///
/// @file payload_event_buffer_istream.h
///
/// Stream class that yields decompressed event byte buffers from a
/// Transaction_payload_log_event.

namespace mysql::binlog::event::compression {

/// Stream class that yields a stream of byte buffers, each holding the
/// raw decompressed data of one event contained in a
/// Transaction_payload_log_event.
///
/// The suggested use pattern is:
///
///   while (stream >> event_buffer) {
///     // handle event
///   }
///   if (stream.error()) {
///     // handle error
///   }
class Payload_event_buffer_istream {
 public:
  using Char_t = unsigned char;
  using Size_t =
      mysql::binlog::event::compression::buffer::Buffer_view<Char_t>::Size_t;
  using Grow_calculator_t =
      mysql::binlog::event::compression::buffer::Grow_calculator;
  using Managed_buffer_t =
      mysql::binlog::event::compression::buffer::Managed_buffer<Char_t>;
  using Decompressor_t = Decompressor;
  using Status_t = Decompress_status;
  using Factory_t = Factory;
  using Buffer_view_t = Managed_buffer_t::Buffer_view_t;
  using Buffer_ptr_t = std::shared_ptr<Buffer_view_t>;
  using Managed_buffer_ptr_t = std::shared_ptr<Managed_buffer_t>;
  using Memory_resource_t = mysql::binlog::event::resource::Memory_resource;
  template <class T>
  using Allocator_t = mysql::binlog::event::resource::Allocator<T>;

  /// Construct the stream from the raw compressed data.
  ///
  /// This stream will keep a pointer to the buffer, so the caller
  /// must ensure that the buffer outlives the stream.
  ///
  /// @param compressed_buffer Input buffer (compressed bytes).
  ///
  /// @param compressed_buffer_size Size of input buffer.
  ///
  /// @param compression_algorithm The algorithm the input was
  /// compressed with.
  ///
  /// @param default_buffer_size The default size of the internal
  /// event buffer.  To tune this, consider that bigger buffers reduce
  /// allocations since one buffer will be reused for all smaller
  /// events, whereas smaller buffers reduce memory footprint in case
  /// all events fit within the buffer size.
  ///
  /// @param memory_resource @c Memory_resource object used to
  /// allocate memory.
  template <class String_char_t>
  Payload_event_buffer_istream(
      const String_char_t *compressed_buffer, Size_t compressed_buffer_size,
      type compression_algorithm, Size_t default_buffer_size = 0,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : m_memory_resource(memory_resource),
        m_compressed_buffer(
            reinterpret_cast<const Char_t *>(compressed_buffer)),
        m_compressed_buffer_size(compressed_buffer_size),
        m_compression_algorithm(compression_algorithm),
        m_default_buffer_size(default_buffer_size) {
    initialize();
  }

  /// Construct the stream from the raw compressed data.
  ///
  /// This stream will keep a pointer to the buffer, so the caller
  /// must ensure that the buffer outlives the stream.
  ///
  /// @param compressed_data Input buffer (compressed bytes).
  ///
  /// @param compression_algorithm The algorithm the input was
  /// compressed with.
  ///
  /// @param default_buffer_size The default size of the internal
  /// event buffer.  To tune this, consider that bigger buffers reduce
  /// allocations since one buffer will be reused for all smaller
  /// events, whereas smaller buffers reduce memory footprint in case
  /// all events fit within the buffer size.
  ///
  /// @param memory_resource @c Memory_resource object used to
  /// allocate memory.
  // Nolint: clang-tidy does not recognize that m_default_buffer_size
  // is initialized, despite it is initialized in the targed
  // constructor.
  // NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
  template <class String_char_t>
  Payload_event_buffer_istream(
      const std::basic_string<String_char_t> &compressed_data,
      type compression_algorithm, Size_t default_buffer_size = 0,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Payload_event_buffer_istream(
            compressed_data.data(), compressed_data.size(),
            compression_algorithm, default_buffer_size, memory_resource) {}
  // NOLINTEND(cppcoreguidelines-pro-type-member-init)

  /// Construct the stream from a (non-owned) Payload Event.
  ///
  /// This stream will keep a pointer to the buffer owned by the
  /// event, so the caller must ensure that the event outlives the
  /// stream.
  ///
  /// @param transaction_payload_log_event Event containing the
  /// compressed data.
  ///
  /// @param default_buffer_size The default size of the internal
  /// event buffer.  To tune this, consider that bigger buffers reduce
  /// allocations since one buffer will be reused for all smaller
  /// events, whereas smaller buffers reduce memory footprint in case
  /// all events fit within the buffer size.
  ///
  /// @param memory_resource @c Memory_resource object used to
  /// allocate memory.
  explicit Payload_event_buffer_istream(
      const Transaction_payload_event &transaction_payload_log_event,
      Size_t default_buffer_size = 0,
      const Memory_resource_t &memory_resource = Memory_resource_t());

  /// Construct the stream from a shared pointer to an event.
  ///
  /// This stream will, as long as it lives, hold shared ownership of
  /// the event.  If, when this stream is deleted, it is the last
  /// owner of the event, it will delete the event.
  ///
  /// @param tple Event containing the compressed data.
  ///
  /// @param default_buffer_size The default size of the internal
  /// event buffer.  To tune this, consider that bigger buffers reduce
  /// allocations since one buffer will be reused for all smaller
  /// events, whereas smaller buffers reduce memory footprint in case
  /// all events fit within the buffer size.
  ///
  /// @param memory_resource @c Memory_resource object used to
  /// allocate memory.
  explicit Payload_event_buffer_istream(
      const std::shared_ptr<const Transaction_payload_event> &tple,
      Size_t default_buffer_size = 0,
      const Memory_resource_t &memory_resource = Memory_resource_t());

  Payload_event_buffer_istream(Payload_event_buffer_istream &) = delete;
  Payload_event_buffer_istream(Payload_event_buffer_istream &&) = delete;
  Payload_event_buffer_istream &operator=(Payload_event_buffer_istream &) =
      delete;
  Payload_event_buffer_istream &operator=(Payload_event_buffer_istream &&) =
      delete;

  ~Payload_event_buffer_istream();

  /// Read the next event from the stream and update the stream state.
  ///
  /// If the stream status is already something else than
  /// `Decompress_status::success`, the function does not change the
  /// status.
  ///
  /// If the function was able to read an event, it modifies `out` to
  /// point to a buffer holding event data.  This leaves the stream
  /// state as `Decompress_status::success`, and subsequent
  /// invocations of `operator bool` will return true.
  ///
  /// If an error occurred, or the end of the stream was reached, the
  /// function resets `out` to nullptr.  It also alters the stream
  /// state to the relevant `Decompress_status`, and subsequent
  /// invocations of `operator bool` will return false.  If the
  /// resulting status is not `Decompress_status::end`, an error
  /// message can subsequently be obtained by calling `get_error_str`.
  ///
  /// @note This class attempts to protect against a common coding
  /// mistake.  The mistake occurs when a caller forgets to check the
  /// reason for ending the stream; whether it actually reached the
  /// end, or whether there was an error. Normally, the caller should
  /// act differently in the two cases. The protection mechanism is
  /// enabled in debug mode, and enforces that the user calls
  /// `get_status` after the stream ends (whether it ends by reaching
  /// the end or by an error).  If the stream ends, and the user does
  /// not call `get_status`, and then the stream object is destroyed,
  /// the destructor raises an assertion.
  ///
  /// @note The output is a reference to a shared pointer, and the
  /// stream is another owner of the same shared pointer.  On the next
  /// invocation of `operator>>`, the buffer will be reused if there
  /// are no other owners than the stream and the output argument.  If
  /// there are other owners to it, a new buffer is allocated.  So a
  /// caller is allowed to keep a shared pointer to the output buffer
  /// as long as it needs.  If the caller does not keep any shared
  /// pointer to the output buffer, it allows the stream to reduce
  /// allocations and memory footprint.
  ///
  /// @note Compressed events never have a checksum, regardless of
  /// configuration.  If the event is to be decoded, you must disable
  /// checksum checks first.
  ///
  /// @param[out] out The target buffer where this function will store
  /// the event data.
  Payload_event_buffer_istream &operator>>(Buffer_ptr_t &out);

  /// Read the next event into a Managed_buffer.
  ///
  /// @see operator>>(Buffer_ptr_t &out)
  Payload_event_buffer_istream &operator>>(Managed_buffer_ptr_t &out);

  /// Indicate if EOF or error has not happened.
  ///
  /// @retval true last read was successful, or no read has yet been
  /// attempted).
  ///
  /// @retval false last read resulted in end-of-stream or error.
  explicit operator bool() const;

  /// Indicate if EOF or error has happened. This is the negation of
  /// `operator bool`.
  ///
  /// @retval false last read was successful, or no read has yet been
  /// attempted.
  ///
  /// @retval true last read resulted in end-of-stream or error.
  bool operator!() const;

  /// Return the stream status.
  ///
  /// @retval success Last read was successful
  ///
  /// @retval end Last read could not complete because the position
  /// was at the end.
  ///
  /// @retval out_of_memory Error: memory allocation failed.
  ///
  /// @retval exceeds_max_size Error: the event was larger than
  /// the configured maximum.
  ///
  /// @retval corrupted A corruption error was reported from ZSTD, or
  /// the stream was truncated.
  Status_t get_status() const;

  /// Return true if there was an error.
  bool has_error() const;

  /// Return a string that describes the last error, or empty string.
  ///
  /// This corresponds to the return value from `has_error`. When
  /// `has_error` returns success or eof, `get_error_str` returns an
  /// empty string.
  std::string get_error_str() const;

  /// Return Grow_calculator used for output buffers.
  const Grow_calculator_t &get_grow_calculator() const;

  /// Set a new Grow_calculator to use for output buffers.
  void set_grow_calculator(const Grow_calculator_t &grow_calculator);

 private:
  /// Construct and initialize the decompressor.
  ///
  /// This will attempt to initialize the decompressor and feed it the
  /// input buffer. If an error occurs, the stream error state is set
  /// accordingly.
  void initialize();

  /// Decompress the next event into the internal buffer.
  ///
  /// If any error occurs, the stream error state is set accordingly.
  void next();

  /// Allocate the output buffer if needed.
  ///
  /// It reuses the existing buffer if this object holds the only
  /// reference to it. Otherwise it allocates a new buffer.  If
  /// allocation fails, the stream error state is set accordingly.
  void update_buffer();

  // Worker function that actually reads the event, used by the
  // implementation of `operator>>`.
  void read_event();

  /// Specify the string that subsequent calls to error_str will
  /// return.
  void set_error_str(const std::string &s);

  /// Update the status.
  void set_status(Status_t status);

  /// Memory_resource to handle all allocations.
  Memory_resource_t m_memory_resource;

  // Input.

  /// The buffer we are reading from.
  const Char_t *m_compressed_buffer{nullptr};
  /// Size of the buffer we are reading from.
  Size_t m_compressed_buffer_size{0};
  /// Compression algorithm we are using.
  type m_compression_algorithm{NONE};
  /// The event we are reading from.  We don't use this, we only hold
  /// the shared pointer to prevent that the caller destroys the
  /// object that owns the buffer.
  std::shared_ptr<const Transaction_payload_event> m_tple{nullptr};

  // Output.

  /// Grow calculator for the Managed_buffer.
  Grow_calculator_t m_grow_calculator;
  /// Default buffer size for the Managed_buffer.
  Size_t m_default_buffer_size;
  /// Shared pointer to Managed_buffer that holds the output.  This
  /// will be shared with API clients.  Therefore, API clients can use
  /// the returned buffer as long as they like.  The next time this
  /// objects needs a buffer to write the output, it uses the shared
  /// object if the API clients have stopped using it; otherwise
  /// allocates a new Managed_buffer.
  Managed_buffer_ptr_t m_managed_buffer_ptr;

  // Operation and status.

  /// Decompressor.
  std::unique_ptr<Decompressor_t> m_decompressor{nullptr};
  /// Error status.
  Status_t m_status{Status_t::success};
  /// Error string.
  std::string m_error_str;
#ifndef NDEBUG
  /// True if a read has failed but neither `get_error_str`,
  /// `has_error`, nor `get_status` has been called.
  mutable bool m_outstanding_error{false};
#endif
};

}  // namespace mysql::binlog::event::compression

/// @}

#endif  // ifndef
        // BINARY_LOG_TRANSACTION_COMPRESSION_PAYLOAD_EVENT_BUFFER_ISTREAM_H_
