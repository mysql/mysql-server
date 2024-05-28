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

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_NONE_COMP_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_NONE_COMP_H

#include "mysql/binlog/event/compression/compressor.h"
#include "mysql/utils/nodiscard.h"

namespace mysql::binlog::event::compression {

/// Compressor subclass that only copies input to output without
/// compressing it.
class None_comp : public Compressor {
 public:
  using typename Compressor::Char_t;
  using typename Compressor::Managed_buffer_sequence_t;
  using typename Compressor::Size_t;
  static constexpr type type_code = NONE;

 private:
  /// @return NONE
  type do_get_type_code() const override;

  /// @copydoc Compressor::do_reset
  ///
  /// No-op for this class.
  void do_reset() override;

  /// @copydoc Compressor::do_feed
  void do_feed(const Char_t *input_data, Size_t input_size) override;

  /// @copydoc Compressor::do_compress
  ///
  /// For None_comp, this is guaranteed to produce all output on
  /// success.
  [[NODISCARD]] Compress_status do_compress(
      Managed_buffer_sequence_t &out) override;

  /// @copydoc Compressor::do_finish
  ///
  /// For None_comp, this is equivalent to @c compress.
  [[NODISCARD]] Compress_status do_finish(
      Managed_buffer_sequence_t &out) override;

  /// Data previously provided to @c do_feed.
  const Char_t *m_input_data{nullptr};

  /// Size data previously provided to @c do_feed.
  Size_t m_input_size{0};
};

}  // namespace mysql::binlog::event::compression

#endif  // MYSQL_BINLOG_EVENT_COMPRESSION_NONE_COMP_H
