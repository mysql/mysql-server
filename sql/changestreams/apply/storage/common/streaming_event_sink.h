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

#ifndef MYSQL_CSA_STREAMING_EVENT_SINK_H
#define MYSQL_CSA_STREAMING_EVENT_SINK_H

#include <cstddef>
#include <string>

namespace mysql::csa {

/// @brief Abstract destination the receiver streams a transaction's events
/// into, uniformly across storage paths.
///
/// The receiver appends a transaction's raw event bytes, as received, one event
/// at a time, then seals (or truncates) the stream at the terminal event. Each
/// sink chooses its own representation.
class Streaming_event_sink {
 public:
  /// @brief Appends one event's raw encoded bytes to the currently open stream.
  /// @param buf Encoded event bytes, as received. The sink copies them
  ///        synchronously within this call, so @p buf may be a transient or
  ///        reused buffer.
  /// @param len Number of bytes at @p buf.
  /// @param seal_after When true, the stream is sealed together with the
  ///        append (the appended event is the terminal event of the batch).
  /// @retval false the event was accepted, or the stream is already closed to
  ///         appends (a benign no-op).
  /// @retval true  a write error occurred while recording the event; the sink
  ///         has recorded the reason (io_error_message()) and truncated its
  ///         stream. The receiver surfaces this as a queuing error.
  virtual bool append_event(const char *buf, std::size_t len,
                            bool seal_after = false) = 0;

  /// @brief Seals the open stream: no further events will be appended and the
  /// consumer may finish once it drains the buffered events.
  virtual void seal_stream() = 0;

  /// @brief Marks the open stream as truncated (incomplete): the consumer stops
  /// at the events buffered so far.
  virtual void set_stream_truncated() = 0;

  /// @brief Whether the sink's own writing failed: provisioning (open), write
  /// or flush. Never reports a consumer's read failure. The in-memory sink
  /// returns false.
  /// @return true if the sink's writing failed, false otherwise (default).
  virtual bool has_io_error() const { return false; }

  /// @brief Reason for the sink's own IO failure (see has_io_error() and
  /// append_event()), or an empty string when there was none.
  virtual const std::string &io_error_message() const {
    static const std::string kNone;
    return kNone;
  }

  /// @brief Virtual destructor.
  virtual ~Streaming_event_sink() = default;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STREAMING_EVENT_SINK_H
