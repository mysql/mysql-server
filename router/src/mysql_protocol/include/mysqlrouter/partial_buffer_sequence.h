/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_PARTIAL_BUFFER_SEQUENCE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_PARTIAL_BUFFER_SEQUENCE_H_

#include <vector>

#include "mysql/harness/net_ts/buffer.h"

namespace classic_protocol {

/**
 * partial buffer sequence.
 *
 * a sub-range of a buffer-sequence which returns a buffer-sequence itself.
 *
 * - consume() moves the position in the buffer-sequence forward.
 * - prepare() returns a buffer-sequence from the current position up to n bytes
 *   (or end of sequence)
 */
template <class BufferSequence>
class PartialBufferSequence {
 public:
  using buffer_sequence_type = BufferSequence;
  using sequence_type = std::vector<net::const_buffer>;

  PartialBufferSequence(const buffer_sequence_type &seq)
      : seq_{seq},
        seq_cur_{net::buffer_sequence_begin(seq)},
        seq_end_{net::buffer_sequence_end(seq)} {}

  /**
   * prepare a buffer-sequence for consumption.
   *
   * skips empty buffers.
   */
  sequence_type prepare(size_t n) const noexcept {
    sequence_type buf_seq;

    size_t pos = pos_;

    for (auto seq_cur = seq_cur_; n > 0 && seq_cur != seq_end_; ++seq_cur) {
      // slice of the current buffer in the sequence
      auto b = net::buffer(net::buffer(*seq_cur) + pos, n);

      // add a buffer to output if it is not empty
      if (b.size() > 0) {
        buf_seq.push_back(b);
        n -= b.size();
        pos = 0;
      }
    }

    return buf_seq;
  }

  /**
   * consume n bytes of buffer-sequence.
   *
   * moves the position in the buffer-sequence forward.
   */
  void consume(size_t n) noexcept {
    pos_ += n;
    consumed_ += n;

    // skip buffers that are already done or empty()
    for (; seq_cur_ != seq_end_; ++seq_cur_) {
      auto buf = *seq_cur_;

      if (buf.size() <= pos_) {
        pos_ -= buf.size();
      } else {
        break;
      }
    }

    // exit-condition:
    //
    // pos_ < seq_cur_->size()
  }

  size_t total_consumed() const noexcept { return consumed_; }

 private:
  // note: only captured to call decltype() on it get the return type of
  // buffer_sequence_begin() and buffer_sequence_end(). If there is another
  // way found to
  const BufferSequence &seq_;

  // current pointer into the buffer-sequence
  decltype(net::buffer_sequence_begin(seq_)) seq_cur_;

  // end of the buffer sequence
  const decltype(net::buffer_sequence_begin(seq_)) seq_end_;

  // position into the first buffer
  size_t pos_{};

  // total consumed bytes
  size_t consumed_{};
};

/**
 * partial buffer sequence.
 *
 * specialization for the common case where the BufferSequence is a single
 * net::const_buffer.
 *
 * The partial sequence that's created by prepare() also creates a
 * net::const_buffer which allows passing it to this specialization again.
 */
template <>
class PartialBufferSequence<net::const_buffer> {
 public:
  using buffer_sequence_type = net::const_buffer;
  using sequence_type = net::const_buffer;

  PartialBufferSequence(const buffer_sequence_type &seq) : seq_{seq} {}

  sequence_type prepare(size_t n) const noexcept {
    return net::buffer(net::buffer(seq_) + pos_, n);
  }

  void consume(size_t n) noexcept { pos_ += n; }

  size_t total_consumed() const noexcept { return pos_; }

 private:
  const buffer_sequence_type &seq_;
  size_t pos_{};
};
}  // namespace classic_protocol
#endif
