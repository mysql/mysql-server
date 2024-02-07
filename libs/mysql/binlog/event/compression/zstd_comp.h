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

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_ZSTD_COMP_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_ZSTD_COMP_H

#define ZSTD_STATIC_LINKING_ONLY 1
#include <zstd.h>

#include "mysql/binlog/event/compression/buffer/buffer_sequence_view.h"
#include "mysql/binlog/event/compression/compressor.h"
#include "mysql/binlog/event/nodiscard.h"
#include "mysql/binlog/event/resource/memory_resource.h"  // Memory_resource

struct ZSTD_outBuffer_s;

namespace mysql::binlog::event::compression {

/// Compressor class that uses the ZSTD library.
class Zstd_comp : public Compressor {
 public:
  using typename Compressor::Char_t;
  using typename Compressor::Managed_buffer_sequence_t;
  using typename Compressor::Size_t;
  using Memory_resource_t = mysql::binlog::event::resource::Memory_resource;
  using Compression_level_t = int;
  static constexpr type type_code = ZSTD;

  /// The default compression level for this compressor.
  static constexpr Compression_level_t default_compression_level = 3;

  Zstd_comp(const Memory_resource_t &memory_resource = Memory_resource_t());

  ~Zstd_comp() override;

  Zstd_comp(const Zstd_comp &) = delete;
  Zstd_comp(const Zstd_comp &&) = delete;
  Zstd_comp &operator=(const Zstd_comp &rhs) = delete;
  Zstd_comp &operator=(const Zstd_comp &&rhs) = delete;

  /// Set the compression level for this compressor.
  ///
  /// This function may be invoked at any time, but will only take
  /// effect the next time a new frame starts, i.e., at the first call
  /// to @c feed after the frame has been reset.
  ///
  /// @param compression_level the new compression level.
  void set_compression_level(Compression_level_t compression_level);

 private:
  /// @return ZSTD
  type do_get_type_code() const override;

  /// @copydoc Compressor::do_reset
  void do_reset() override;

  /// @copydoc Compressor::do_feed
  void do_feed(const Char_t *input_data, Size_t input_size) override;

  /// @copydoc Compressor::do_compress
  [[NODISCARD]] Compress_status do_compress(
      Managed_buffer_sequence_t &out) override;

  /// @copydoc Compressor::do_finish
  [[NODISCARD]] Compress_status do_finish(
      Managed_buffer_sequence_t &out) override;

  /// @copydoc Compressor::do_get_grow_constraint_hint
  [[NODISCARD]] Grow_constraint_t do_get_grow_constraint_hint() const override;

  /// Deallocate the ZSTD compression context.
  void destroy();

  /// Reset just the ZSTD compressor state, not other state.
  void reset_compressor();

  /// Make the ZSTD buffer point to the next available buffer;
  /// allocate one if necessary.
  ///
  /// @param[in,out] managed_buffer_sequence owns the data and
  /// manages the growth.
  ///
  /// @param[out] obuf ZSTD buffer that will be altered to point to
  /// the next available byte in buffer_sequence.
  ///
  /// @retval success managed_buffer_sequence was not full, or its
  /// capacity has been incremented successfully.  obuf has been set
  /// to point to next available byte.
  ///
  /// @retval out_of_memory buffer_sequence was full and an out of
  /// memory error occurred.  buffer_sequence has not been modified.
  ///
  /// @retval exceeds_max_size buffer_sequence was full and at its max
  /// capacity.  buffer_sequence has not been modified.
  [[NODISCARD]] Compress_status get_obuf(
      Managed_buffer_sequence_t &managed_buffer_sequence,
      ZSTD_outBuffer_s &obuf);

  /// Account for having written to the output buffer.
  ///
  /// This moves the read/write boundary in the
  /// Managed_buffer_sequence, and also increments
  /// m_total_output_size.
  ///
  /// @param managed_buffer_sequence The buffer sequence that has
  /// been written to.
  ///
  /// @param delta The number of bytes that have been written.
  void move_position(Managed_buffer_sequence_t &managed_buffer_sequence,
                     Size_t delta);

  /// The ZSTD compression context.
  ZSTD_CStream *m_ctx{nullptr};

  /// The input buffer.
  ZSTD_inBuffer m_ibuf{nullptr, 0, 0};

  /// Value used to indicate that no compression level has been specified.
  static constexpr Compression_level_t uninitialized_compression_level{0};

  /// True when @c compress has been called and neither @c finish nor
  /// @c reset has yet been called.
  bool m_started{false};

  /// Compression level that was set in the @c m_ctx object.
  Compression_level_t m_current_compression_level{
      uninitialized_compression_level};

  /// Compression level that was given in @c set_compression_level
  Compression_level_t m_next_compression_level{default_compression_level};

  /// Instrumented memory allocator object
  Memory_resource_t m_memory_resource;

  /// ZSTD memory allocator objects and functions
  ZSTD_customMem m_zstd_custom_mem;
  static void *zstd_mem_res_alloc(void *opaque, size_t size);
  static void zstd_mem_res_free(void *opaque, void *address);
};

}  // namespace mysql::binlog::event::compression

#endif  // MYSQL_BINLOG_EVENT_COMPRESSION_ZSTD_COMP_H
