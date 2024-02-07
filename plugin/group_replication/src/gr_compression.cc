/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/gr_compression.h"
#include "plugin/group_replication/include/plugin.h"

#include <mysql/components/services/log_builtins.h>
#include <zstd.h>
#include "my_byteorder.h"
#include "my_dbug.h"
#include "mysqld_error.h"

GR_compress::GR_compress(enum_compression_type compression_type)
    : m_compressor(nullptr), m_compression_type(compression_type) {
  switch (compression_type) {
    case enum_compression_type::ZSTD_COMPRESSION:
    default: {
      m_compressor_name.assign("Zstandard");
      mysql::binlog::event::compression::type compression_type_aux =
          mysql::binlog::event::compression::ZSTD;
      auto comp = mysql::binlog::event::compression::Factory::build_compressor(
          compression_type_aux);
      if (comp != nullptr) {
        m_compressor = comp.release();
      } else {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_ERROR_COMPRESS_INITIALIZE,
                     m_compressor_name.c_str());
      }
    } break;

    case enum_compression_type::NO_COMPRESSION: {
      m_compressor_name.assign("No Compression");
      mysql::binlog::event::compression::type compression_type_aux =
          mysql::binlog::event::compression::NONE;
      auto comp = mysql::binlog::event::compression::Factory::build_compressor(
          compression_type_aux);
      if (comp != nullptr) {
        m_compressor = comp.release();
      } else {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_ERROR_COMPRESS_INITIALIZE,
                     m_compressor_name.c_str());
      }
    } break;
  }
}

GR_compress::~GR_compress() {
  if (m_compressor != nullptr) {
    delete m_compressor;
    m_compressor = nullptr;
  }
}

GR_compress::enum_compression_error GR_compress::compress(unsigned char *data,
                                                          size_t length) {
  enum_compression_error error{
      enum_compression_error::ER_COMPRESSION_INIT_FAILURE};

  if (m_compression_type == enum_compression_type::ZSTD_COMPRESSION ||
      m_compression_type == enum_compression_type::NO_COMPRESSION) {
    if (m_compressor != nullptr) {
      m_compressor->feed(data, length);
      m_status = m_compressor->compress(m_managed_buffer_sequence);

      if (m_status == Compress_status_t::success) {
        m_status = m_compressor->finish(m_managed_buffer_sequence);
        if (m_status == Compress_status_t::success) {
          m_uncompressed_data_size = length;
          m_compressed_data_size = m_managed_buffer_sequence.read_part().size();

#if !defined(NDEBUG)
          // In case of No Compression compression type compressed input length
          // should be same as decompressed output length.
          if (m_compression_type ==
              GR_compress::enum_compression_type::NO_COMPRESSION) {
            assert(m_compressed_data_size == m_uncompressed_data_size);
          }
#endif

          return enum_compression_error::COMPRESSION_OK;
        }
      }

      // Error buffer exceed max configured size.
      else if (m_status == Compress_status_t::exceeds_max_size) {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_COMPRESS_EXCEEDS_MAX_SIZE,
                     m_compressor_name.c_str());
        error = enum_compression_error::ER_COMPRESSION_EXCEEDS_MAX_BUFFER_SIZE;
      }

      // Error memory allocation failed.
      else if (m_status == Compress_status_t::out_of_memory) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_COMPRESS_OUT_OF_MEMORY,
                     m_compressor_name.c_str());
        error = enum_compression_error::ER_COMPRESSION_OUT_OF_MEMORY;
      }

    } else {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_ERROR_COMPRESS_INITIALIZE,
                   m_compressor_name.c_str());
      error = enum_compression_error::ER_COMPRESSION_INIT_FAILURE;
    }
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_UNKOWN_COMPRESSION_TYPE);
    error = enum_compression_error::ER_COMPRESSION_TYPE_UNKOWN;
  }

  m_uncompressed_data_size = 0;
  m_compressed_data_size = 0;
  return error;
}

std::pair<unsigned char *, std::size_t> GR_compress::allocate_and_get_buffer() {
  if (m_compression_type == enum_compression_type::ZSTD_COMPRESSION ||
      m_compression_type == enum_compression_type::NO_COMPRESSION) {
    if (m_status != Compress_status_t::success) {
      return std::make_pair(nullptr, 0);
    }

    DBUG_EXECUTE_IF("gr_compression_get_empty_buffer",
                    { return std::make_pair(nullptr, 0); });

    // Get contiguous output buffer from managed_buffer_sequence
    unsigned char *buffer = static_cast<Char_t *>(
        my_malloc(key_compression_data, m_compressed_data_size, MYF(0)));
    if (buffer == nullptr) {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                   "getting contiguous output buffer from "
                   "managed_buffer_sequence of compression process");
      return std::make_pair(nullptr, 0);
    }
    m_managed_buffer_sequence.read_part().copy(buffer);
    return std::make_pair(buffer, m_compressed_data_size);
  }

  return std::make_pair(nullptr, 0);
}

size_t GR_compress::get_uncompressed_data_size() {
  if (m_compression_type == enum_compression_type::ZSTD_COMPRESSION ||
      m_compression_type == enum_compression_type::NO_COMPRESSION) {
    return m_uncompressed_data_size;
  }

  return 0;
}
