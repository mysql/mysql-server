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

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_DECOMPRESSOR_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_DECOMPRESSOR_H

#include "mysql/binlog/event/compression/base.h"               // type
#include "mysql/binlog/event/compression/decompress_status.h"  // Decompress_status
#include "mysql/containers/buffers/grow_constraint.h"  // Grow_constraint
#include "mysql/containers/buffers/managed_buffer.h"   // Managed_buffer
#include "mysql/utils/nodiscard.h"                     // NODISCARD

namespace mysql::binlog::event::compression {

/// Abstract base class for decompressors.
///
/// Each subclass normally corresponds to a compression
/// algorithm, and maintains the algorithm-specific state for it.
///
/// An instance of this class can be reused to decompress several
/// different *frames*.  Each frame input may be split into multiple
/// pieces. @see Compressor.
///
/// To decompress one or more frames in one piece, use this API as
/// follows:
/// 1. Call @c feed to provide all input
/// 2. Repeatedly call @c decompress, each time producing as much
///    output as you need.
///
/// To decompress one or more frames in multiple pieces, use this API
/// as follows:
/// 1. Repeatedly call @c decompress, each time producing as much
///    output as you need. Whenever it returns eof or truncated, call
///    @c feed to provide more input, and try again.
///
/// This class uses a @c buffer::Managed_buffer to store output.
class Decompressor {
 public:
  using Char_t = unsigned char;
  using Size_t = mysql::containers::buffers::Buffer_view<Char_t>::Size_t;
  using Managed_buffer_t = mysql::containers::buffers::Managed_buffer<Char_t>;
  using Grow_constraint_t = mysql::containers::buffers::Grow_constraint;

 private:
  using Grow_status_t = mysql::containers::buffers::Grow_status;

 public:
  Decompressor() = default;
  Decompressor(const Decompressor &) = delete;
  Decompressor(const Decompressor &&) = delete;
  const Decompressor &operator=(const Decompressor &) = delete;
  const Decompressor &operator=(const Decompressor &&) = delete;
  virtual ~Decompressor() = default;

  /// @return the compression type.
  type get_type_code() const;

  /// Reset the frame.
  ///
  /// This cancels the current frame and starts a new one.
  void reset();

  /// Submit data to decompress.
  ///
  /// @param input_data The buffer of input data that this decompressor
  /// will read.
  ///
  /// @param input_size The size of the input buffer.
  template <class Input_char_t>
  void feed(const Input_char_t *input_data, Size_t input_size) {
    feed_char_t(reinterpret_cast<const Char_t *>(input_data), input_size);
  }

  /// Decompress an exact, given number of bytes.
  ///
  /// @param out The output buffer.  This function first grows the
  /// write part of `out` to at least `output_size` bytes, then
  /// decompresses into the write part, and then moves the
  /// decompressed bytes from the beginning of the write part to the
  /// end of the read part.
  ///
  /// @param output_size Number of bytes to decompress.
  ///
  /// @retval success Decompression succeeded.  The requested bytes
  /// are available in @c out.
  ///
  /// @retval eof There were no more bytes to decompress.  The out
  /// buffer has not been changed and the frame has not been reset.
  ///
  /// @retval truncated All input was consumed, but produced less
  /// output than requested.  The out buffer has been changed and the
  /// frame has not been reset.  The caller may resume decompression
  /// after calling @c feed.
  ///
  /// @retval corrupted The compression library detected that the data
  /// was corrupted.  The frame has been reset.
  ///
  /// @retval out_of_memory The operation failed due to an out of
  /// memory error.  The frame has been reset.
  ///
  /// @retval exceeds_max_capacity The requested size exceeds the
  /// maximium capacity configured for the Managed_buffer.  The out
  /// buffer has not been changed and the frame has not been reset.
  /// The caller may resume decompression after increasing the
  /// capacity, or resetting the buffer (perhaps after moving the data
  /// elsewhere), or using a different output buffer, or similar.
  [[NODISCARD]] Decompress_status decompress(Managed_buffer_t &out,
                                             Size_t output_size);

  /// Decompress an exact, given number of bytes.
  ///
  /// @param out The output buffer.
  ///
  /// @param output_size The number of bytes to decompress.
  ///
  /// @returns a pair where the first component is a Decompress_status
  /// value and the second component is the number of bytes that were
  /// successfully stored in out.  The Decompress_status has the same
  /// possible values as for @c decompress(Managed_buffer_t,
  /// output_size), except it cannot take the value
  /// exceeds_max_capacity.  The size will be equal to output_size if
  /// the status is success; strictly between 0 and output_size if the
  /// status is truncated; and 0 for the other cases.
  [[NODISCARD]] std::pair<Decompress_status, Size_t> decompress(
      Char_t *out, Size_t output_size);

  /// Return a `Grow_constraint` that may be used with the
  /// Managed_buffer storing the output, in order to optimize memory
  /// usage for a particular compression algorithm.
  Grow_constraint_t get_grow_constraint_hint() const;

 private:
  /// Implement @c get_type_code.
  virtual type do_get_type_code() const = 0;

  /// Implement @c do_reset.
  virtual void do_reset() = 0;

  void feed_char_t(const Char_t *input_data, Size_t input_size);

  /// Implement @c feed.
  virtual void do_feed(const Char_t *input_data, Size_t input_size) = 0;

  /// Implement @c decompress.
  ///
  /// This differs from @c decompress in that it does not have to
  /// reset the frame when returning out_of_memory or corrupted; the
  /// caller does that.
  [[NODISCARD]] virtual std::pair<Decompress_status, Size_t> do_decompress(
      Char_t *out, Size_t output_size) = 0;

  /// Implement @c get_grow_constraint_hint.
  ///
  /// In this base class, the function returns a default-constructed
  /// Grow_constraint, i.e., one which does not limit the
  /// Grow_calculator.
  virtual Grow_constraint_t do_get_grow_constraint_hint() const;
};

}  // namespace mysql::binlog::event::compression

#endif  // ifndef MYSQL_BINLOG_EVENT_COMPRESSION_DECOMPRESSOR_H
