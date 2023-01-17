/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_LZ4_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_LZ4_H_

#include <lz4frame.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"

namespace protocol {

class Compression_algorithm_lz4 : public Compression_algorithm_interface {
 public:
  explicit Compression_algorithm_lz4(const int32_t level) {
    LZ4F_createCompressionContext(&m_ctxt, LZ4F_VERSION);
    m_lz4f_frame_preferences.frameInfo.contentSize = 0;
    m_lz4f_frame_preferences.autoFlush = 0;
    m_lz4f_frame_preferences.compressionLevel = level;

    // Get compression buffer size for average input data size
    const auto k_compress_bound =
        LZ4F_compressBound(k_average_input_size, &m_lz4f_frame_preferences);

    // Get how many data we can maximally put into selected compression buffer
    // size
    m_input_buffer_data_size_max = get_max_size_of_input();

    m_compression_buffer_size = k_compress_bound + k_lz4f_frame_begin;
    m_compression_buffer_sptr.reset(new uint8_t[m_compression_buffer_size]);
  }

  ~Compression_algorithm_lz4() override { LZ4F_freeCompressionContext(m_ctxt); }

  void set_pledged_source_size(const int /*src_size*/) override {}

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_LOG("debug", "set_input (size:" << in_size << ")");
    m_input_buffer_data_size = in_size;
    m_input_buffer = in_ptr;
  }

  bool compress(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    if (m_compression_buffer_offset) {
      copy_compression_buffer_to(out_ptr, out_size);
      return true;
    }

    if (!m_input_buffer_data_size) {
      *out_size = 0;
      return true;
    }

    DBUG_LOG("debug", "m_input_buffer_data_size: " << m_input_buffer_data_size);
    const size_t k_needed_size = LZ4F_compressBound(m_input_buffer_data_size,
                                                    &m_lz4f_frame_preferences) +
                                 k_lz4f_frame_begin;

    if (!use_secure_buffer<&Compression_algorithm_lz4::unsecure_compress>(
            k_needed_size, out_ptr, out_size))
      return false;

    return true;
  }

  bool flush(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    if (m_compression_buffer_offset) {
      copy_compression_buffer_to(out_ptr, out_size);
      return true;
    }

    const int k_needed_size =
        LZ4F_compressBound(0, &m_lz4f_frame_preferences) + k_lz4f_frame_begin;

    if (!use_secure_buffer<&Compression_algorithm_lz4::unsecure_flush>(
            k_needed_size, out_ptr, out_size))
      return false;

    return true;
  }

  static int32_t get_level_min() { return 0; }
  static int32_t get_level_max() { return 16; }

 private:
  using Compression_method = bool (Compression_algorithm_lz4::*)(uint8_t *,
                                                                 int *,
                                                                 const bool);
  int get_max_size_of_input() {
    static int max_size_of_input =
        find_max_size_of_input(k_average_input_size, &m_lz4f_frame_preferences);

    return max_size_of_input;
  }
  int find_max_size_of_input(const int input_buffer_size,
                             LZ4F_preferences_t *fpref) {
    const auto preallocated_compressed_buffer_size =
        LZ4F_compressBound(input_buffer_size, fpref);

    int maximum_input_buffer_size = preallocated_compressed_buffer_size;
    size_t compressed_buffer_size = 0;

    do {
      compressed_buffer_size =
          LZ4F_compressBound(--maximum_input_buffer_size, fpref);

      // Check if after compression the data fill match the pre-allocated
      // compression buffer
    } while (compressed_buffer_size <= preallocated_compressed_buffer_size);

    return input_buffer_size;
  }

  template <Compression_method method>
  bool use_secure_buffer(const size_t needed_size, uint8_t *out_ptr,
                         int *out_size) {
    DBUG_TRACE;
    DBUG_LOG("debug", "needed_size(" << needed_size << ") <= out_size("
                                     << *out_size << ")");
    const bool k_use_output_buffer = static_cast<int>(needed_size) <= *out_size;

    if (k_use_output_buffer) {
      if (!(this->*method)(out_ptr, out_size, false)) return false;
    } else {
      DBUG_LOG("debug", "using compression-buffer");
      int compression_offset = m_compression_buffer_size;
      m_compression_buffer_offset = 0;
      m_compression_buffer_ptr = m_compression_buffer_sptr.get();

      if (!(this->*method)(m_compression_buffer_ptr, &compression_offset, true))
        return false;

      m_compression_buffer_offset = compression_offset;

      copy_compression_buffer_to(out_ptr, out_size);
    }

    return true;
  }

  bool unsecure_compress(uint8_t *output_ptr, int *output_size,
                         const bool limit_input) {
    DBUG_TRACE;
    auto data_ptr = output_ptr;
    auto data_size = *output_size;

    *output_size = 0;

    if (!m_frame_opened) {
      m_frame_opened = true;

      DBUG_LOG("debug", "Opened LZ4Frame");
      const auto written = LZ4F_compressBegin(m_ctxt, data_ptr, data_size,
                                              &m_lz4f_frame_preferences);

      if (LZ4F_isError(written)) {
        DBUG_LOG("debug", "LZ4F_compressBegin fail with error: "
                              << LZ4F_getErrorName(written));
        return false;
      }

      data_ptr += written;
      data_size -= written;
      *output_size += written;
    }

    const auto input_size = limit_input ? std::min(m_input_buffer_data_size,
                                                   m_input_buffer_data_size_max)
                                        : m_input_buffer_data_size;

    const auto written = LZ4F_compressUpdate(
        m_ctxt, data_ptr, data_size, m_input_buffer, input_size, nullptr);

    m_input_buffer_data_size -= input_size;
    m_input_buffer += input_size;

    DBUG_LOG("debug",
             "LZ4F_compressUpdate(input_size:"
                 << input_size << "), moved m_input_buffer_data_size to "
                 << m_input_buffer_data_size << ", and generated " << written);

    if (LZ4F_isError(written)) {
      DBUG_LOG("debug", "LZ4F_compressUpdate fail with error: "
                            << LZ4F_getErrorName(written));
      return false;
    }

    *output_size += written;

    return true;
  }

  bool unsecure_flush(uint8_t *output_ptr, int *output_size,
                      const bool /*limit_intput*/) {
    DBUG_TRACE;
    auto data_ptr = output_ptr;
    auto data_size = *output_size;
    *output_size = 0;

    if (m_frame_opened) {
      auto written = LZ4F_flush(m_ctxt, data_ptr, data_size, nullptr);

      DBUG_LOG("debug", "LZ4F_flush generated " << written);

      if (LZ4F_isError(written)) {
        DBUG_LOG("debug",
                 "LZ4F_flush fail with error: " << LZ4F_getErrorName(written));
        return false;
      }

      *output_size += written;

      if (0 == written) {
        m_frame_opened = false;
        data_ptr += written;
        data_size -= written;

        DBUG_LOG("debug", "Closed LZ4Frame");
        written = LZ4F_compressEnd(m_ctxt, data_ptr, data_size, nullptr);

        if (LZ4F_isError(written)) {
          DBUG_LOG("debug", "LZ4F_compressEnd fail with error: "
                                << LZ4F_getErrorName(written));
          return false;
        }

        *output_size += written;
      }
    }

    return true;
  }

  void copy_compression_buffer_to(uint8_t *out_ptr, int *out_size) {
    DBUG_TRACE;

    const auto k_to_copy = std::min(m_compression_buffer_offset, *out_size);

    std::memcpy(out_ptr, m_compression_buffer_ptr, k_to_copy);

    m_compression_buffer_ptr += k_to_copy;
    m_compression_buffer_offset -= k_to_copy;
    *out_size = k_to_copy;
  }

  LZ4F_compressionContext_t m_ctxt;
  LZ4F_preferences_t m_lz4f_frame_preferences{};

  std::unique_ptr<uint8_t[]> m_compression_buffer_sptr;
  uint8_t *m_compression_buffer_ptr = nullptr;
  int m_compression_buffer_size = 0;
  int m_compression_buffer_offset = 0;

  uint8_t *m_input_buffer;
  int m_input_buffer_data_size = 0;
  int m_input_buffer_data_size_max = 0;

  bool m_frame_opened = false;
  const int k_lz4f_frame_begin = 15;
  const int k_average_input_size = 1000;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_LZ4_H_
