/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef LIBBINLOGEVENTS_COMPRESSION_COMPRESSOR_H_INCLUDED
#define LIBBINLOGEVENTS_COMPRESSION_COMPRESSOR_H_INCLUDED

#include <tuple>
#include "base.h"                                            // type
#include "libbinlogevents/include/buffer/grow_constraint.h"  // Grow_constraint
#include "libbinlogevents/include/buffer/managed_buffer_sequence.h"  // Managed_buffer_sequence
#include "libbinlogevents/include/nodiscard.h"  // NODISCARD

#include <limits>  // std::numeric_limits

namespace binary_log::transaction::compression {

using Compress_status = mysqlns::buffer::Grow_status;

/// Abstract base class for compressors.
///
/// Each subclass normally corresponds to a compression algorithm, and
/// maintains the algorithm-specific state for it.
///
/// An instance of this class can be reused to compress several
/// *frames*.  A frame is a self-contained segment of data, in the
/// sense that it can be decompressed without knowing about other
/// frames, and compression does not take advantage of patterns that
/// repeat between frames.
///
/// Input for a frame can be provided in pieces.  All pieces for a
/// frame will be compressed together; the decompressor will take
/// advantage of patterns across in different pieces within the frame.
/// Providing a frame in pieces is useful when not all input is known
/// at once.
///
/// To compress one frame, use the API as follows:
///
/// 1. Repeat as many times as needed:
///    1.1. Call @c feed to provide a piece of input.
///    1.2. Call @c compress to consume the piece of input and possibly
///         produce a prefix of the output.
/// 2. Choose one of the following:
///    2.1. Call @c finish to produce the remainder of the output for this
///         frame.
///    2.2. Call @c reset to abort this frame.
///
/// @note After 1.2, although the compression library has read all
/// input given so far, it may not have produced all corresponding
/// output.  It usually holds some data in internal buffers, since it
/// may be more compressible when more data has been given. Therefore,
/// step 2.1 is always necessary in order to complete the frame.
///
/// @note To reuse the compressor object for another input, repeat the
/// above procedure as many times as needed.
///
/// This class requires that the user provides a @c
/// mysqlns::buffer::Managed_buffer_sequence to store output.
class Compressor {
 public:
  using Managed_buffer_sequence_t = mysqlns::buffer::Managed_buffer_sequence<>;
  using Char_t = Managed_buffer_sequence_t::Char_t;
  using Size_t = Managed_buffer_sequence_t::Size_t;
  using Grow_constraint_t = mysqlns::buffer::Grow_constraint;
  static constexpr Size_t pledged_input_size_unset =
      std::numeric_limits<Size_t>::max();

  Compressor() = default;
  Compressor(const Compressor &other) = delete;
  Compressor(Compressor &&other) = delete;
  Compressor &operator=(const Compressor &other) = delete;
  Compressor &operator=(Compressor &&other) = delete;

  virtual ~Compressor() = default;

  /// @return the compression type.
  type get_type_code() const;

  /// Reset the frame.
  ///
  /// This cancels the current frame and starts a new one.
  ///
  /// This is allowed but unnecessary if the current frame has been
  /// reset by @c finish or by an out_of_memory error from @c
  /// compress.
  void reset();

  /// Submit data to be compressed.
  ///
  /// This will not consume any of the input; it should be followed by
  /// a call to @c compress or @c finish.
  ///
  /// @note This object will not copy the input; the caller must
  /// ensure that the input lives until it has been consumed or the
  /// frame has been reset.
  ///
  /// @note Must not be called when there is still non-consumed input
  /// left after a previous call to @c feed.
  ///
  /// @param input_data Data to be compressed. This object will keep a
  /// shallow copy of the data and use it in subsequent calls to @c
  /// compress or @c finish.
  ///
  /// @param input_size Size of data to be compressed.
  template <class Input_char_t>
  void feed(const Input_char_t *input_data, Size_t input_size) {
    feed_char_t(reinterpret_cast<const Char_t *>(input_data), input_size);
  }

  /// Consume all input previously given in the feed function.
  ///
  /// This will consume the input, but may not produce all output;
  /// there may be output still in compression library buffers.  Use
  /// the @c finish function to flush the output and end the frame.
  ///
  /// @param out Storage for compressed bytes.  This may grow, if
  /// needed.
  ///
  /// @retval success All input was consumed.
  ///
  /// @retval out_of_memory The operation failed due to an out of
  /// memory error.  The frame has been reset.
  ///
  /// @retval exceeds_max_size The @c out buffer was already at its
  /// max capacity, and filled, and there were more bytes left to
  /// produce.  The frame has not been reset and it is not guaranteed
  /// that all input has been consumed.  The caller may resume
  /// compression e.g.  after increasing the capacity, or resetting
  /// the output buffer (perhaps after moving existing data
  /// elsewhere), or using a different output buffer, or similar.
  [[NODISCARD]] Compress_status compress(Managed_buffer_sequence_t &out);

  /// Consume all input, produce all output, and end the frame.
  ///
  /// This will consume all input previously given by @c feed (it
  /// internally calls @c compress).  Then it ends the frame and
  /// flushes the output, ensuring that all data that may reside in
  /// the compression library's internal buffers gets compressed and
  /// written to the output.
  ///
  /// The next call to @c feed will start a new frame.
  ///
  /// @param out Storage for compressed bytes.  This may grow, if
  /// needed.
  ///
  /// @retval success All input was consumed, all output was produced,
  /// and the frame was reset.
  ///
  /// @retval out_of_memory The operation failed due to an out of
  /// memory error, and the frame has been reset.
  ///
  /// @retval exceeds_max_size The @c out buffer was already at its
  /// max capacity, and filled, and there were more bytes left to
  /// produce.  The frame has not been reset and it is not guaranteed
  /// that all input has been consumed.  The caller may resume
  /// compression e.g.  after increasing the capacity, or resetting
  /// the output buffer (perhaps after moving existing data
  /// elsewhere), or using a different output buffer, or similar.
  [[NODISCARD]] Compress_status finish(Managed_buffer_sequence_t &out);

  /// Return a `Grow_constraint` that may be used with the
  /// Managed_buffer_sequence storing the output, in order to
  /// optimize memory usage for a particular compression algorithm.
  ///
  /// This may be implemented by subclasses such that it depends on
  /// the pledged input size.  Therefore, for the most optimal grow
  /// constraint, call this after set_pledged_input_size.
  Grow_constraint_t get_grow_constraint_hint() const;

  /// Declare that the input size will be exactly as given.
  ///
  /// This may allow compressors and decompressors to use memory more
  /// efficiently.
  ///
  /// This function may only be called if `feed` has never been
  /// called, or if the compressor has been reset since the last call
  /// to `feed`.  The pledged size will be set back to
  /// pledged_input_size_unset next time this compressor is reset.
  ///
  /// It is required that the total number of bytes passed to `feed`
  /// before the call to `finish` matches the pledged number.
  /// Otherwise, the behavior of `finish` is undefined.
  void set_pledged_input_size(Size_t size);

  /// Return the size previously provided to `set_pledged_input_size`,
  /// or `pledged_input_size_unset` if no pledged size has been set.
  Size_t get_pledged_input_size() const;

 private:
  /// Worker function for @c feed, requiring the correct Char_t type.
  ///
  /// @see feed.
  void feed_char_t(const Char_t *input_data, Size_t input_size);

  /// implement @c get_type_code.
  virtual type do_get_type_code() const = 0;

  /// Implement @c reset.
  virtual void do_reset() = 0;

  /// Implement @c feed.
  ///
  /// This differs from @c feed in that it does not have to reset the
  /// frame when returning out_of_memory; the caller does that.
  virtual void do_feed(const Char_t *input_data, Size_t input_size) = 0;

  /// Implement @c compress.
  ///
  /// This differs from @c compress in that it does not have to reset
  /// the frame when returning out_of_memory; the caller does that.
  [[NODISCARD]] virtual Compress_status do_compress(
      Managed_buffer_sequence_t &out) = 0;

  /// Implement @c finish.
  ///
  /// This differs from @c finish in that it does not have to reset
  /// the frame when returning out_of_memory; the caller does that.
  ///
  /// Implementations may assume that @c compress has been called,
  /// since @c finish does that.
  [[NODISCARD]] virtual Compress_status do_finish(
      Managed_buffer_sequence_t &out) = 0;

  /// Implement @c get_grow_constraint_hint.
  ///
  /// In this base class, the function returns a default-constructed
  /// Grow_constraint, i.e., one which does not limit the
  /// Grow_calculator.
  virtual Grow_constraint_t do_get_grow_constraint_hint() const;

  /// Implement @c set_pledged_input_size
  ///
  /// By default, this does nothing.
  virtual void do_set_pledged_input_size([[maybe_unused]] Size_t size);

  /// True when user has provided input that has not yet been consumed.
  bool m_pending_input = false;

  /// True when user has not provided any input since the last reset.
  bool m_empty = true;

  /// The number of bytes
  Size_t m_pledged_input_size = pledged_input_size_unset;
};

}  // namespace binary_log::transaction::compression

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_COMPRESSOR_H_INCLUDED
