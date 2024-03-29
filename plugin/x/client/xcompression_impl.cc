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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#include "plugin/x/client/xcompression_impl.h"

#include <limits>
#include <memory>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_lz4.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zlib.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zstd.h"
#include "plugin/x/protocol/stream/compression/decompression_algorithm_lz4.h"
#include "plugin/x/protocol/stream/compression/decompression_algorithm_zlib.h"
#include "plugin/x/protocol/stream/compression/decompression_algorithm_zstd.h"
#include "plugin/x/protocol/stream/compression_output_stream.h"
#include "plugin/x/protocol/stream/decompression_input_stream.h"

namespace xcl {

bool Compression_impl::reinitialize(const Compression_algorithm algorithm) {
  DBUG_LOG("debug", "Compression_impl::reinitialize(algorithm:"
                        << static_cast<int32_t>(algorithm) << ")");
  switch (algorithm) {
    case Compression_algorithm::k_deflate:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_zlib());
      m_uplink_stream.reset(new protocol::Compression_algorithm_zlib(3));
      return true;

    case Compression_algorithm::k_lz4:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_lz4());
      m_uplink_stream.reset(new protocol::Compression_algorithm_lz4(2));
      return true;

    case Compression_algorithm::k_zstd:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_zstd());
      m_uplink_stream.reset(new protocol::Compression_algorithm_zstd(3));
      return true;

    case Compression_algorithm::k_none: {
    }
  }

  return false;
}

namespace {
template <typename Compression_algorithm>
int32_t adjust_level(const int32_t level) {
  if (level < Compression_algorithm::get_level_min())
    return Compression_algorithm::get_level_min();
  if (level > Compression_algorithm::get_level_max())
    return Compression_algorithm::get_level_max();
  return level;
}
}  // namespace

bool Compression_impl::reinitialize(const Compression_algorithm algorithm,
                                    const int32_t level) {
  DBUG_LOG("debug", "Compression_impl::reinitialize(algorithm:"
                        << static_cast<int32_t>(algorithm)
                        << " level:" << static_cast<int32_t>(level) << ")");
  switch (algorithm) {
    case Compression_algorithm::k_deflate:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_zlib());
      m_uplink_stream.reset(new protocol::Compression_algorithm_zlib(
          adjust_level<protocol::Compression_algorithm_zlib>(level)));
      return true;

    case Compression_algorithm::k_lz4:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_lz4());
      m_uplink_stream.reset(new protocol::Compression_algorithm_lz4(
          adjust_level<protocol::Compression_algorithm_lz4>(level)));
      return true;

    case Compression_algorithm::k_zstd:
      m_downlink_stream.reset(new protocol::Decompression_algorithm_zstd());
      m_uplink_stream.reset(new protocol::Compression_algorithm_zstd(
          level == 0
              ? 1
              : adjust_level<protocol::Compression_algorithm_zstd>(level)));
      return true;

    case Compression_algorithm::k_none: {
    }
  }

  return false;
}

Compression_impl::Output_stream_ptr Compression_impl::uplink(
    Output_stream *source) {
  if (!m_uplink_stream) return {};

  return std::make_shared<protocol::Compression_output_stream>(
      m_uplink_stream.get(), source);
}

Compression_impl::Input_stream_ptr Compression_impl::downlink(
    Input_stream *source) {
  if (!m_uplink_stream) return {};

  return std::make_shared<protocol::Decompression_input_stream>(
      m_downlink_stream.get(), source);
}

}  // namespace xcl
