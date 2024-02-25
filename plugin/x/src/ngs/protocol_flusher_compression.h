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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_COMPRESSION_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_COMPRESSION_H_

#include <array>
#include <functional>
#include <memory>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/encoders/encoding_xrow.h"
#include "plugin/x/src/interface/protocol_flusher.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/ngs/compression_types.h"

namespace encoding {

class Encoding_buffer;
class XProtocol_encoder;

}  // namespace encoding

namespace ngs {

using Error_handler = std::function<void(int error)>;

class Protocol_flusher_compression : public xpl::iface::Protocol_flusher {
 public:
  Protocol_flusher_compression(
      std::unique_ptr<xpl::iface::Protocol_flusher> ptr,
      protocol::XMessage_encoder *encoder,
      xpl::iface::Protocol_monitor *monitor, const Error_handler &error_handler,
      Memory_block_pool *memory_block);
  /**
    Force that next `try_flush` is going to dispatch data.
   */
  void trigger_flush_required() override;
  void trigger_on_message(const uint8 type) override;

  /**
    Check if flush is required and try to execute it

    Flush is not going to be executed when the flusher is locked or
    when no other conditions to flush were fulfilled.

    @return result of flush operation
      @retval == k_flushed     flush was successful
      @retval == k_not_flushed nothing important to flush
      @retval == k_error       flush IO was failed
   */
  Result try_flush() override;

  bool is_going_to_flush() override;

  void set_write_timeout(const uint32_t timeout) override;

  xpl::iface::Vio *get_connection() override {
    return m_flusher->get_connection();
  }

  void set_compression_options(const Compression_algorithm algo,
                               const Compression_style style,
                               const int64_t max_num_of_messages,
                               const int32_t level);

  void handle_compression(const uint8_t id, const bool can_be_compressed);
  void abort_last_compressed();

 private:
  void begin_compression(const uint8_t id);
  void end_compression();
  std::unique_ptr<xpl::iface::Protocol_flusher> m_flusher;
  bool m_compression_ongoing = false;
  bool m_compression_stop = false;
  bool m_fata_compression_error = false;
  int m_compressed_messages = 0;
  int64_t m_max_compressed_messages = -1;
  ::protocol::XMessage_encoder *m_encoder;
  xpl::iface::Protocol_monitor *m_monitor;
  ::protocol::Compression_type m_comp_type =
      ::protocol::Compression_type::k_single;
  Error_handler m_on_error_handler;
  ::protocol::Encoding_pool m_pool;
  ::protocol::Encoding_buffer m_comp_buffor{&m_pool};
  ::protocol::XMessage_encoder::Compression_position m_comp_position;
  std::unique_ptr<::protocol::Compression_buffer_interface> m_comp_algorithm;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_COMPRESSION_H_
