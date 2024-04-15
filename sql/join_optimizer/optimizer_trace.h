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
#define SQL_JOIN_OPTIMIZER_OPTIMIZER_TRACE_H_ 1

#include <array>
#include <cassert>
#include <deque>
#include <memory>
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
  static constexpr int64_t kSegmentSize{4096};

 private:
  /// A consecutive buffer.
  using Segment = std::array<char, kSegmentSize>;

 public:
  /// @param max_bytes The maximal number of trace bytes, as given by the
  /// optimizer_trace_max_mem_size system variable.
  explicit TraceBuffer(int64_t max_bytes)
      :  // We round upwards so that we can hold at least 'max_bytes' bytes.
        m_max_segments{max_bytes / kSegmentSize +
                       (max_bytes % kSegmentSize == 0 ? 0 : 1)} {
    assert(max_bytes >= 0);
    assert(m_max_segments >= 0);
  }
  /**
     Called by std::ostream if the current segment is full. Allocate
     a new segment (or use m_excess_segment if we have reached
     m_max_segments) and put 'ch' at the beginning of it.
   */
  int_type overflow(int_type ch) override;

  /**
     Apply 'sink' to each character in the trace text. Free each segment
     when its contents have been consumed. (That way, we avoid storing two
     copies of a potentially huge trace at the same time.)
  */
  template <typename Sink>
  void Consume(Sink sink) {
    assert(!m_segments.empty() || m_excess_segment != nullptr ||
           (pptr() == nullptr && pbase() == nullptr && epptr() == nullptr));

    while (!m_segments.empty()) {
      for (char &ch : m_segments.front()) {
        // If the last segment is allocated directly before another segment,
        // then last_segment.end() == other_segment.begin(). For that reason,
        // we need to check if m_segments.size() == 1 to know that we are on
        // the last segment. Otherwise, we get pptr()==other_segment.begin()
        // if the the total trace volume is a multiple of kSegmentSize.
        if (m_segments.size() == 1 && &ch == pptr()) {
          setp(nullptr, nullptr);
          break;
        }
        sink(ch);
      }
      m_segments.pop_front();
    }
  }

  /// Get the number of bytes that did not fit in m_segments.
  int64_t excess_bytes() const {
    return m_excess_segment == nullptr
               ? 0
               : kSegmentSize * m_full_excess_segments +
                     (pptr() - std::to_address(m_excess_segment->cbegin()));
  }

  /// Return a copy of the contents as a string. This may be expensive for
  /// large traces, and is only intended for unit tests.
  std::string ToString() const {
    std::string result;

    for (auto segment = m_segments.cbegin(); segment < m_segments.cend();
         segment++) {
      for (const char &ch : *segment) {
        // See Consume().
        if (segment + 1 == m_segments.cend() && &ch == pptr()) {
          break;
        }
        result += ch;
      }
    }
    return result;
  }

 private:
  /// Max number of segments (as given by the optimizer_trace_max_mem_size
  /// system variable).
  int64_t m_max_segments;

  /// The sequence of segments.
  std::deque<Segment> m_segments;

  /// If we fill m_max_segments, allocate a single extra segment that is
  /// repeatedly overwritten with any additional data. This field will point
  /// to that segment.
  std::unique_ptr<Segment> m_excess_segment;

  /// The number of full segments that did not fit in m_segments.
  int64_t m_full_excess_segments{0};
};

/**
   Trace in the form of plain text (i.e. no JSON tree), as used by
   the Hypergraph optimizer.
*/
class UnstructuredTrace final {
 public:
  /// @param max_bytes The maximal number of trace bytes, as given by the
  /// optimizer_trace_max_mem_size system variable.
  explicit UnstructuredTrace(int64_t max_bytes)
      : m_buffer{max_bytes}, m_stream{&m_buffer} {}

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
