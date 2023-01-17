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

#ifndef LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_INCLUDED
#define LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_INCLUDED

#include <zstd.h>
#include <cstddef>
#include <vector>

#include "decompressor.h"

namespace binary_log {
namespace transaction {
namespace compression {

/**
  This class implements a ZSTD decompressor.
 */
class Zstd_dec : public Decompressor {
 private:
  Zstd_dec &operator=(const Zstd_dec &rhs) = delete;
  Zstd_dec(const Zstd_dec &) = delete;

 protected:
  ZSTD_DStream *m_ctx{nullptr};

 public:
  Zstd_dec();
  ~Zstd_dec() override;

  /**
    Shall return the compression type code.

    @return the compression type code.
   */
  type compression_type_code() override;

  /**
    Shall open the decompressor. This member function must be called
    before any decompression operation takes place over the buffer
    supplied.

    @return false on success, true otherwise.
   */
  bool open() override;

  /**
    This member function shall decompress the buffer provided and put the
    decompressed payload into the output buffer.

    @param data a pointer to the buffer holding the data to decompress
    @param length the size of the data to decompress.

    @return false on success, true otherwise.
   */
  std::tuple<std::size_t, bool> decompress(const unsigned char *data,
                                           size_t length) override;

  /**
    This member function shall close the decompressor. It must be called
    after this decompressor is not needed anymore. It shall free the
    resources it has used for the decompression activities.

    @return false on success, true otherwise.
   */
  bool close() override;
};

}  // namespace compression
}  // end of namespace transaction
}  // end of namespace binary_log

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_ZSTD_DEC_H_INCLUDED
