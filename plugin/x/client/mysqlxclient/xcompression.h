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

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XCOMPRESSION_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XCOMPRESSION_H_

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include <google/protobuf/io/coded_stream.h>
MY_COMPILER_DIAGNOSTIC_POP()
#include <memory>
#include <set>

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"
#include "plugin/x/protocol/stream/compression/decompression_algorithm_interface.h"

namespace xcl {

/**
  Defines if negotiation algorithm for compression should be used, and its
  tolerance level.
*/
enum class Compression_negotiation { k_disabled, k_preferred, k_required };

/**
  Enum that defines the compression algorithm that is used by
  X Protocol in 'uplink' and 'downlink'.
*/
enum class Compression_algorithm { k_none, k_deflate, k_lz4, k_zstd };

/**
  Interface defining X Compression operations.

  This interface should be used internally by XProtocol and XConnection
  implementations, still it was extracted to give user a possibility
  to compress raw data and serialize them inside COMPRESSION_SINGLE
  and other messages.
*/
class XCompression {
 public:
  using Output_stream = google::protobuf::io::ZeroCopyOutputStream;
  using Input_stream = google::protobuf::io::ZeroCopyInputStream;

  using Output_stream_ptr = std::shared_ptr<Output_stream>;
  using Input_stream_ptr = std::shared_ptr<Input_stream>;

 public:
  virtual ~XCompression() = default;

  /**
    Reinitialize 'uplink' and 'downlink' compression context using set
    algorithm.

    Some compression algorithm may be only set before session or capability
    setup, in that case setting such algorithm may fail. Also some algorithms
    set once may be not changeable.
  */
  virtual bool reinitialize(const Compression_algorithm algorithm) = 0;

  /**
    Reinitialize 'uplink' and 'downlink' compression context using set
    algorithm and compression level.

    Some compression algorithm may be only set before session or capability
    setup, in that case setting such algorithm may fail. Also some algorithms
    set once may be not changeable.
  */
  virtual bool reinitialize(const Compression_algorithm algorithm,
                            const int32_t level) = 0;

  /**
    Downlink compression stream

    This method returns a stream that can be used with compression done
    on X message level. Other compression styles may operate on lower
    layers without possibility for user interaction.
  */
  virtual Input_stream_ptr downlink(Input_stream *data_stream) = 0;

  /**
    Uplink compression stream

    This method returns a stream that can be used with compression done
    on X message level. Other compression styles may operate on lower
    layers without possibility for user interaction.
  */
  virtual Output_stream_ptr uplink(Output_stream *data_stream) = 0;

  /**
    Uplink compression algorithm

    This method returns an algorithm that can be used with compression done
    on X message level.
  */
  virtual protocol::Compression_algorithm_interface *compression_algorithm()
      const = 0;

  /**
    Downlink compression algorithm

    This method returns an algorithm that can be used with decompression done
    on X message level.
  */
  virtual protocol::Decompression_algorithm_interface *decompression_algorithm()
      const = 0;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XCOMPRESSION_H_
