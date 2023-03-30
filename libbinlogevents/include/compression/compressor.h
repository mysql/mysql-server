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
#include "base.h"

namespace binary_log {
namespace transaction {
namespace compression {

/**
  The base compressor abstract class.

  It establishes the interface for compressors.
 */
class Compressor : public Base_compressor_decompressor {
 public:
  /**
    Sets the compression level for this compressor. It is
    only effective if done before opening the compressor.
    After opening the compressor setting the compression
    level, it is only effective when the compressor is
    closed and opened again.

    @param compression_level the compression level for this compressor.
  */
  virtual void set_compression_level(unsigned int compression_level) = 0;

  /**
    This member function SHALL compress the data provided with the given
    length. Note that the buffer to store the compressed data must have
    already have been set through @c set_buffer.

    If the output buffer is not large enough an error shall be returned.
    The contents of the output buffer may still have been modified in
    that case.

    @param data the data to compress
    @param length the length of the data.

    @return a tuple containing the bytes not compressed and an error state. If
            all bytes were decompressed, then it is returned 0 in the first
            element and false in the second. If not all bytes were compressed,
            it returns the number of remaining bytes not processed and false on
            the second element. If there was an error, then the second element
            returns true, and the first element returns the number of bytes
            processed until the error happened.

   */
  virtual std::tuple<std::size_t, bool> compress(const unsigned char *data,
                                                 size_t length) = 0;
};

}  // namespace compression
}  // namespace transaction
}  // namespace binary_log

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_COMPRESSOR_H_INCLUDED
