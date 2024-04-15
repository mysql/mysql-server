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

#ifndef GROUP_REPLICATION_COMPRESSION_INCLUDE
#define GROUP_REPLICATION_COMPRESSION_INCLUDE

#include "mysql/binlog/event/compression/compressor.h"
#include "mysql/binlog/event/compression/factory.h"
#include "mysql/containers/buffers/managed_buffer.h"

/*
  Implements Compressor.
*/
class GR_compress {
 public:
  using Compressor_t = mysql::binlog::event::compression::Compressor;
  using Compress_status_t = mysql::containers::buffers::Grow_status;
  using Managed_buffer_sequence_t =
      mysql::containers::buffers::Managed_buffer_sequence<>;
  using Char_t = Managed_buffer_sequence_t::Char_t;

  /**
    Compression Type
  */
  enum class enum_compression_type {
    /* Data not compressed */
    NO_COMPRESSION = 0,
    /* Data Compressed using ZSTD compression */
    ZSTD_COMPRESSION = 1
  };

  /**
    Compression Error
  */
  enum class enum_compression_error {
    /* No Error */
    COMPRESSION_OK = 0,
    /* Unkown compression type. */
    ER_COMPRESSION_TYPE_UNKOWN = 1,
    /* Compression library initialization error. */
    ER_COMPRESSION_INIT_FAILURE = 2,
    /* Exceed max output buffer size. */
    ER_COMPRESSION_EXCEEDS_MAX_BUFFER_SIZE = 3,
    /* Memory allocation failure. */
    ER_COMPRESSION_OUT_OF_MEMORY = 4
  };

  /*
    Constructor

    @param[in] compression_type  the enum_compression_type compression type
  */
  GR_compress(enum_compression_type compression_type =
                  enum_compression_type::ZSTD_COMPRESSION);

  /*
    Destructor
  */
  ~GR_compress();

  /**
    This shall compress the buffer provided and put the compressed payload
    into the m_managed_buffer_sequence which is non-contiguous growable memory
    buffer.

    @param data a pointer to the buffer holding the data to compress
    @param length the size of the data to compress.

    @return enum_compression_error error type. The compressed data is stored in
            m_managed_buffer_sequence which can be retrieved using
            allocate_and_get_buffer().
  */
  GR_compress::enum_compression_error compress(unsigned char *data,
                                               size_t length);

  /*
    Return the pointer to compressed data and the size.
    The compressed data is stored in managed_buffer_sequence which is
    non-contiguous growable memory buffer. So to return the compressed
    contiguous output from managed_buffer_sequence, a buffer is allocated in
    this function. The compressed data is added and returned in it.

    Note: A buffer shall be allocated in this function to return the compressed
          data and it's the caller responsibility to release allocates memory.

    @return a pair containing the pointer to the compressed data and it's size.
            In case of error or memory allocation failure the pair containing
            nullptr and 0 size is returned.
  */
  std::pair<unsigned char *, std::size_t> allocate_and_get_buffer();

  /*
    Return the uncompressed data size.

    @return the the size of the uncompressed data
  */
  size_t get_uncompressed_data_size();

 private:
  /** ZSTD compressor class object. */
  Compressor_t *m_compressor{nullptr};

  /** The compression type. */
  enum_compression_type m_compression_type{
      enum_compression_type::ZSTD_COMPRESSION};

  /** The compression library name. */
  std::string m_compressor_name{"Zstandard"};

  /** The compression status. */
  Compress_status_t m_status;

  /** The buffer holding compressed data. */
  Managed_buffer_sequence_t m_managed_buffer_sequence;

  /** The uncompressed data size. */
  size_t m_uncompressed_data_size{0};

  /** The compressed data size. */
  size_t m_compressed_data_size{0};
};
#endif /* GROUP_REPLICATION_COMPRESSION_INCLUDE */
