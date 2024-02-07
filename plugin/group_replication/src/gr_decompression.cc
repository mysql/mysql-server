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

#include "plugin/group_replication/include/gr_decompression.h"

#include <mysql/components/services/log_builtins.h>
#include <zstd.h>
#include "my_byteorder.h"
#include "my_dbug.h"
#include "mysqld_error.h"

GR_decompress::GR_decompress(
    GR_compress::enum_compression_type compression_type)
    : m_decompressor(nullptr), m_compression_type(compression_type) {
  switch (compression_type) {
    case GR_compress::enum_compression_type::ZSTD_COMPRESSION:
    default: {
      m_compressor_name.assign("Zstandard");
      mysql::binlog::event::compression::type compression_type_aux =
          mysql::binlog::event::compression::ZSTD;
      auto decomp =
          mysql::binlog::event::compression::Factory::build_decompressor(
              compression_type_aux);
      if (decomp != nullptr) {
        m_decompressor = decomp.release();
      } else {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_ERROR_DECOMPRESS_INITIALIZE,
                     m_compressor_name.c_str());
      }
    } break;

    case GR_compress::enum_compression_type::NO_COMPRESSION: {
      m_compressor_name.assign("No Compression");
      mysql::binlog::event::compression::type compression_type_aux =
          mysql::binlog::event::compression::NONE;
      auto decomp =
          mysql::binlog::event::compression::Factory::build_decompressor(
              compression_type_aux);
      if (decomp != nullptr) {
        m_decompressor = decomp.release();
      } else {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_ERROR_DECOMPRESS_INITIALIZE,
                     m_compressor_name.c_str());
      }
    } break;
  }
}

GR_decompress::~GR_decompress() {
  if (m_decompressor != nullptr) {
    delete m_decompressor;
    m_decompressor = nullptr;
  }
}

GR_decompress::enum_decompression_error GR_decompress::decompress(
    const unsigned char *compressed_data, size_t compressed_data_length,
    size_t output_size) {
  GR_decompress::enum_decompression_error error{
      GR_decompress::enum_decompression_error::ER_DECOMPRESSION_INIT_FAILURE};

  DBUG_EXECUTE_IF("group_replication_no_vcle_no_compression", {
    assert(m_compression_type ==
           GR_compress::enum_compression_type::NO_COMPRESSION);
  });

#if !defined(NDEBUG)
  // In case of No Compression compression type compressed input length should
  // be same as decompressed output length.
  if (m_compression_type ==
      GR_compress::enum_compression_type::NO_COMPRESSION) {
    assert(compressed_data_length == output_size);
  }

  // In case of ZSTD Compression compression type compressed input length should
  // be less then decompressed output length.
  if (m_compression_type ==
      GR_compress::enum_compression_type::ZSTD_COMPRESSION) {
    assert(compressed_data_length <= output_size);
  }
#endif

  if (m_compression_type ==
          GR_compress::enum_compression_type::ZSTD_COMPRESSION ||
      m_compression_type ==
          GR_compress::enum_compression_type::NO_COMPRESSION) {
    if (m_decompressor != nullptr) {
      m_decompressor->feed(compressed_data, compressed_data_length);
      m_status = m_decompressor->decompress(m_managed_buffer, output_size);

      // Success
      if (m_status == Decompress_status_t::success) {
        assert(m_managed_buffer.read_part().size() == output_size);
        error = GR_decompress::enum_decompression_error::DECOMPRESSION_OK;
      }

      // Error output buffer exceed max configured size (1GB default).
      else if (m_status == Decompress_status_t::exceeds_max_size) {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_DECOMPRESS_EXCEEDS_MAX_SIZE,
                     m_compressor_name.c_str());
        error = GR_decompress::enum_decompression_error::
            ER_DECOMPRESSION_EXCEEDS_MAX_BUFFER_SIZE;
      }

      // Error memory allocation failed.
      else if (m_status == Decompress_status_t::out_of_memory) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_DECOMPRESS_OUT_OF_MEMORY,
                     m_compressor_name.c_str());
        error = GR_decompress::enum_decompression_error::
            ER_DECOMPRESSION_OUT_OF_MEMORY;
      }

      // All input was consumed, but produced less output than requested. The
      // output buffer has been changed and the frame has not been reset. The
      // caller may resume decompression after calling feed().
      else if (m_status == Decompress_status_t::truncated) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_DECOMPRESS_TRUNCATED,
                     m_compressor_name.c_str());
        error =
            GR_decompress::enum_decompression_error::ER_DECOMPRESSION_TRUNCATED;
      }

      // Error compression library reported an error.
      else if (m_status == Decompress_status_t::corrupted) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_DECOMPRESS_CORRUPTED,
                     m_compressor_name.c_str());
        error =
            GR_decompress::enum_decompression_error::ER_DECOMPRESSION_CORRUPTED;
      }

      // Error zero remaining bytes, no more input bytes to decompress. The
      // output buffer has not been changed.
      else if (m_status == Decompress_status_t::end) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_DECOMPRESS_END,
                     m_compressor_name.c_str());
        error = GR_decompress::enum_decompression_error::ER_DECOMPRESSION_EOF;
      }

    } else {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_ERROR_COMPRESS_INITIALIZE,
                   m_compressor_name.c_str());
      error = GR_decompress::enum_decompression_error::
          ER_DECOMPRESSION_INIT_FAILURE;
    }
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_UNKOWN_COMPRESSION_TYPE);
    error = GR_decompress::enum_decompression_error::ER_COMPRESSION_TYPE_UNKOWN;
  }

  return error;
}

std::pair<unsigned char *, std::size_t> GR_decompress::get_buffer() {
  if (m_compression_type ==
          GR_compress::enum_compression_type::ZSTD_COMPRESSION ||
      m_compression_type ==
          GR_compress::enum_compression_type::NO_COMPRESSION) {
    if (m_status != Decompress_status_t::success)
      return std::make_pair(nullptr, 0);

    return std::make_pair(m_managed_buffer.read_part().data(),
                          m_managed_buffer.read_part().size());
  }

  return std::make_pair(nullptr, 0);
}
