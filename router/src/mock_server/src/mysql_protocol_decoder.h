/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLD_MOCK_MYSQL_PROTOCOL_DECODER_INCLUDED
#define MYSQLD_MOCK_MYSQL_PROTOCOL_DECODER_INCLUDED

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

#include "mysql_protocol_common.h"

namespace server_mock {

using byte = uint8_t;

/** @class MySQLProtocolDecoder
 *
 * @brief Responsible for decoding classic MySQL protocol packets
 *
 **/
class MySQLProtocolDecoder {
 public:
  /** @brief Callback used to read more data from the socket
   **/
  using ReadCallback =
      std::function<void(int, uint8_t *data, size_t size, int)>;

  /** @brief Constructor
   *
   * @param read_clb Callback to use to read more data from the socket
   **/
  MySQLProtocolDecoder(const ReadCallback &read_clb);

  /** @brief Reads single packet from the network socket.
   **/
  void read_message(socket_t client_socket, int flags = 0);

  /** @brief Retrieves sequence number of the packet
   *
   * @returns sequence number
   */
  uint8_t packet_seq() const { return packet_.packet_seq; }

  /** @brief Retrieves command type from the packet sent by the client.
   *
   * @returns command type
   **/
  mysql_protocol::Command get_command_type() const;

  /** @brief Retrieves SQL statement from the packet sent by the client.
   *
   * The method assumes that the packet is MySQL QUERY command.
   *
   * @returns SQL statement
   **/
  std::string get_statement() const;

 private:
  /** @brief Single protocol packet data.
   **/
  struct ProtocolPacketType {
    // packet sequence number
    uint8_t packet_seq{0};
    // raw packet data
    std::vector<byte> packet_buffer;
  };

  const ReadCallback read_callback_;
  ProtocolPacketType packet_;
  mysql_protocol::Capabilities::Flags capabilities_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_PROTOCOL_DECODER_INCLUDED
