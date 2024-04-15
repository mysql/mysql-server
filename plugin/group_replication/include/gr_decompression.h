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

#ifndef GROUP_REPLICATION_DECOMPRESSION_INCLUDE
#define GROUP_REPLICATION_DECOMPRESSION_INCLUDE

#include "mysql/binlog/event/compression/decompress_status.h"
#include "mysql/binlog/event/compression/decompressor.h"
#include "mysql/binlog/event/compression/factory.h"
#include "mysql/containers/buffers/managed_buffer.h"
#include "plugin/group_replication/include/gr_compression.h"

/*
  Implements Decompressor.
*/
class GR_decompress {
 public:
  using Decompressor_t = mysql::binlog::event::compression::Decompressor;
  using Managed_buffer_t = Decompressor_t::Managed_buffer_t;
  using Decompress_status_t =
      mysql::binlog::event::compression::Decompress_status;

  /**
    Decompression Error
  */
  enum class enum_decompression_error {
    /* No Error */
    DECOMPRESSION_OK = 0,
    /* Unkown compression type. */
    ER_COMPRESSION_TYPE_UNKOWN = 1,
    /* Compression library initialization error. */
    ER_DECOMPRESSION_INIT_FAILURE = 2,
    /* Exceed max output buffer size. */
    ER_DECOMPRESSION_EXCEEDS_MAX_BUFFER_SIZE = 3,
    /* Memory allocation failure. */
    ER_DECOMPRESSION_OUT_OF_MEMORY = 4,
    /* No remaining bytes, less output than requested */
    ER_DECOMPRESSION_TRUNCATED = 5,
    /* Compression library reported an error. */
    ER_DECOMPRESSION_CORRUPTED = 6,
    /* No more input bytes to consume, output unchanged. */
    ER_DECOMPRESSION_EOF = 7
  };

  /*
    Constructor

    @param[in] compression_type  the GR_compress::enum_compression_type
                                 compression type
  */
  GR_decompress(GR_compress::enum_compression_type compression_type =
                    GR_compress::enum_compression_type::ZSTD_COMPRESSION);

  /*
    Destructor
  */
  ~GR_decompress();

  /**
    This shall decompress the buffer provided and put the
    decompressed payload into the output buffer i.e. m_managed_buffer.

    @param compressed_data         the pointer to the input buffer holding the
                                   compressed data which needs to decompress.
    @param compressed_data_length  the size of the input data to decompress.
    @param output_size             the exact size of output decompressed data.

    Note: The caller needs to provide exact expected decompressed data in
          in param 'output_size'.
          The ZSTD library api in libbinlogevents does have other api's which
          can be used to get decompressed data in several loops. But in this
          implementation user has to provide exact size he expects after
          decompressing data.

    @return GR_compress::enum_decompression_error error type.
  */
  GR_decompress::enum_decompression_error decompress(
      const unsigned char *compressed_data, size_t compressed_data_length,
      size_t output_size);

  /*
    Return the uncompressed data and size.

    @return a pair containing the uncompressed data and size.
            In case of error the pair containing nullptr and 0 size is returned.
  */
  std::pair<unsigned char *, std::size_t> get_buffer();

 private:
  /** ZSTD decompressor class object. */
  Decompressor_t *m_decompressor{nullptr};

  /** The compression type. */
  GR_compress::enum_compression_type m_compression_type{
      GR_compress::enum_compression_type::ZSTD_COMPRESSION};

  /** The compression library name. */
  std::string m_compressor_name{"Zstandard"};

  /** The compression status. */
  Decompress_status_t m_status;

  /** The buffer holding decompressed data. */
  Managed_buffer_t m_managed_buffer;
};
#endif /* GROUP_REPLICATION_DECOMPRESSION_INCLUDE */
