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

#ifndef LIBBINLOGEVENTS_COMPRESSION_BASE_H_INCLUDED
#define LIBBINLOGEVENTS_COMPRESSION_BASE_H_INCLUDED

#include <cstddef>
#include <string>
#include <tuple>

namespace binary_log {
namespace transaction {
namespace compression {

const static size_t BLOCK_BYTES = 8 * 1024;

enum type {
  /* ZSTD compression. */
  ZSTD = 0,

  /* No compression. */
  NONE = 255,
};

std::string type_to_string(type t);

/**
  Base class for compressors and decompressors.

  It establishes the interface and implements part of the shared
  behavior.
 */
class Base_compressor_decompressor {
 protected:
  /**
    The pointer to the buffer holding the data to compress/decompress.
   */
  unsigned char *m_buffer{nullptr};

  /**
    The buffer capacity.
   */
  std::size_t m_buffer_capacity{0};

  /**
    A cursor over the buffer. It is used internally to calculate the
    amount of buffer used for instance.
   */
  unsigned char *m_buffer_cursor{nullptr};

 public:
  Base_compressor_decompressor();
  virtual ~Base_compressor_decompressor();

  /**
    The compression algorithm type. E.g., LZ4, ZSTD, ZLIB.
    @return the compression type.
   */
  virtual type compression_type_code() = 0;

  /**
   This member function SHALL open the compressor or decompressor.
   Every compressor/decompressor must be opened before it is utilized.

   @return false on success, true otherwise
  */
  virtual bool open() = 0;

  /**
   This member function SHALL close the compressor/decompressor.
   Every compressor/decompressor MUST be closed once the stream
   is over.
   */
  virtual bool close() = 0;

  /**
   This member function returns the size of the compressed/decompressed
   data.

   @return the size of the actual data.
   */
  virtual size_t size();

  /**
   This member function returns the capacity of the buffer.

   @return the capacity of the buffer.
  */
  virtual std::size_t capacity();

  /**
    This member function SHALL set the buffer into which the compressor or
    decompressor shall write the data after it processes the stream.

    @param buffer the buffer itself.
    @param capacity   the capacity of the buffer.

    @return true if something went wrong, false otherwise.
  */
  virtual bool set_buffer(unsigned char *buffer, std::size_t capacity);

  /**
     This member function SHALL return the buffer, the size of its data
     and its capacity.

     @return a tuple containing the buffer, size and capacity.
   */
  virtual std::tuple<unsigned char *, std::size_t, std::size_t> get_buffer();

  /**
   This member function expands the buffer by a number of bytes.
   Expansion is aligned to the block size and it is only done if
   needed.

   @param bytes The number of bytes to expand the buffer.
   @return false on success, true otherwise.
   */
  virtual bool reserve(std::size_t bytes);
};

}  // namespace compression
}  // namespace transaction
}  // namespace binary_log

#endif  // ifndef LIBBINLOGEVENTS_COMPRESSION_BASE_H_INCLUDED
