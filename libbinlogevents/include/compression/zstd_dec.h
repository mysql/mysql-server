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

#ifndef LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_
#define LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_

#include <zstd.h>

#include "decompressor.h"
#include "libbinlogevents/include/nodiscard.h"

namespace binary_log {
namespace transaction {
namespace compression {

/// Decompressor class that uses the ZSTD library.
class Zstd_dec : public Decompressor {
 public:
  using typename Decompressor::Char_t;
  using typename Decompressor::Grow_constraint_t;
  using typename Decompressor::Size_t;
  static constexpr type type_code = ZSTD;

  Zstd_dec();
  ~Zstd_dec() override;

  Zstd_dec(const Zstd_dec &) = delete;
  Zstd_dec(const Zstd_dec &&) = delete;
  Zstd_dec &operator=(const Zstd_dec &) = delete;
  Zstd_dec &operator=(const Zstd_dec &&) = delete;

 private:
  /// @return ZSTD
  type do_get_type_code() const override;

  /// @copydoc Decompressor::do_reset
  void do_reset() override;

  /// @copydoc Decompressor::do_feed
  void do_feed(const Char_t *input_data, Size_t input_size) override;

  /// @copydoc Decompressor::do_decompress
  [[NODISCARD]] std::pair<Decompress_status, Size_t> do_decompress(
      Char_t *out, Size_t output_size) override;

  /// @copydoc Decompressor::do_get_grow_constraint_hint
  Grow_constraint_t do_get_grow_constraint_hint() const override;

  /// Deallocate the ZSTD decompression context.
  void destroy();

  /// ZSTD decompression context object.
  ZSTD_DStream *m_ctx{nullptr};

  /// ZSTD input buffer.
  ZSTD_inBuffer m_ibuf{nullptr, 0, 0};

  bool m_frame_boundary = false;
};

}  // namespace compression
}  // end of namespace transaction
}  // end of namespace binary_log

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_
