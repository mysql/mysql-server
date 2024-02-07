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

#ifndef SQL_JOIN_OPTIMIZER_OPTIMIZER_TRACE_H_
#define SQL_JOIN_OPTIMIZER_OPTIMIZER_TRACE_ 1

#include <array>
#include <cassert>
#include <deque>
#include <ostream>

#include "sql/opt_trace_context.h"
#include "sql/sql_lex.h"

/**
   This class is used for storing unstructured optimizer trace text
   (as used by the Hypergraph optimizer). It is used as the
   std::streambuf object of an associated std::ostream (which writes
   the formatted text into a TraceBuffer object).
   The text is stored in a non-consecutive sequence of segments, where
   each segment has a chunk of consecutive memory. That way, the
   buffer can grow without having to copy the text into ever bigger
   buffers of consecutive memory.
*/
class TraceBuffer final : public std::streambuf {
 public:
  /// The size of each consecutive buffer.
  static constexpr size_t kSegmentSize = 4096;

 private:
  /// A consecutive buffer.
  using Segment = std::array<char, kSegmentSize>;
  /// The sequence of consecutive buffers.
  using DequeType = std::deque<Segment>;

 public:
  /**
     Called by std::ostream if the current segment is full.
     Allocate a new segment and put 'ch' at the beginning of it.
   */
  int_type overflow(int_type ch) override {
    Segment &segment = m_segments.emplace_back();
    segment[0] = ch;

    setp(std::to_address(segment.begin()) + 1, std::to_address(segment.end()));

    // Anything but EOF means 'ok'.
    return traits_type::not_eof(ch);
  }

  /// Apply 'sink' to each character in the trace text.
  template <typename Sink>
  void ForEach(Sink sink) const {
    for (const Segment &segment : m_segments) {
      for (const char &ch : segment) {
        if (&ch == pptr()) {
          break;
        }
        sink(ch);
      }
    }
  }

  /**
     Apply 'sink' to each character in the trace text. Free each segment
     when its contents have been consumed.
  */
  template <typename Sink>
  void ForEachRemove(Sink sink) {
    if (m_segments.empty()) {
      assert(pptr() == nullptr && pbase() == nullptr && epptr() == nullptr);
    } else {
      while (!m_segments.empty()) {
        for (char &ch : m_segments.front()) {
          if (&ch == pptr()) {
            setp(nullptr, nullptr);
            assert(m_segments.size() == 1);
            break;
          }
          sink(ch);
        }
        m_segments.pop_front();
      }
    }
  }

  /// Return a copy of the contents as a string. This may be expensive for
  /// large traces, and is only intended for unit tests.
  std::string ToString() const {
    std::string result;
    ForEach([&](char ch) { result += ch; });
    return result;
  }

 private:
  /// The sequence of segments.
  DequeType m_segments;
};

/**
   Trace in the form of plain text (i.e. no JSON tree), as used by
   the Hypergraph optimizer.
*/
class UnstructuredTrace final {
 public:
  UnstructuredTrace() : m_stream{&m_buffer} {}

  /// Get the stream in which to put the trace text.
  std::ostream &stream() { return m_stream; }

  TraceBuffer &contents() { return m_buffer; }
  const TraceBuffer &contents() const { return m_buffer; }

 private:
  /// The trace text.
  TraceBuffer m_buffer;

  /// The stream that formats text and appends it to m_buffer.
  std::ostream m_stream;
};

// Shorthand functions.

/// Fetch the ostream that we write optimizer trace into.
inline std::ostream &Trace(THD *thd) {
  return thd->opt_trace.unstructured_trace()->stream();
}

/// @returns 'true' if unstructured optimizer trace (as used by Hypergraph)
/// is started.
inline bool TraceStarted(THD *thd) {
  return thd->opt_trace.is_started() &&
         thd->opt_trace.unstructured_trace() != nullptr;
}
#endif
