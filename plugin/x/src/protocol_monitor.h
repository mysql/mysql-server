/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_PROTOCOL_MONITOR_H_
#define PLUGIN_X_SRC_PROTOCOL_MONITOR_H_

#include "plugin/x/src/interface/protocol_monitor.h"

namespace xpl {

class Protocol_monitor : public iface::Protocol_monitor {
 public:
  Protocol_monitor() = default;
  void init(iface::Client *client) override;

  void on_notice_warning_send() override;
  void on_notice_other_send() override;
  void on_notice_global_send() override;
  void on_error_send() override;
  void on_fatal_error_send() override;
  void on_init_error_send() override;
  void on_row_send() override;
  void on_send(const uint32_t bytes_transferred) override;
  void on_send_compressed(const uint32_t bytes_transferred) override;
  void on_send_before_compression(const uint32_t bytes_transferred) override;
  void on_receive(const uint32_t bytes_transferred) override;
  void on_error_unknown_msg_type() override;
  void on_receive_compressed(const uint32_t bytes_transferred) override;
  void on_receive_after_decompression(
      const uint32_t bytes_transferred) override;
  void on_messages_sent(const uint32_t messages) override;

 private:
  iface::Client *m_client{nullptr};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_PROTOCOL_MONITOR_H_
