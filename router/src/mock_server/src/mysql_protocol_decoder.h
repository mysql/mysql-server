/*
  Copyright (c) 2017, 2020, Oracle and/or its affiliates.

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

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mysql/harness/net_ts/internet.h"
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
  stdx::expected<size_t, std::error_code> read_header(
      const net::const_buffer &buf);

  /** @brief Reads single packet from the network socket.
   **/
  void read_message(const net::const_buffer &buf);

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

  /**
   * get payload of the mysql protocol frame
   *
   * @pre read_message() returned successfully
   */
  std::vector<uint8_t> get_payload() const { return packet_.packet_buffer; }

 private:
  /** @brief Single protocol packet data.
   **/
  struct ProtocolPacketType {
    // packet sequence number
    uint8_t packet_seq{0};
    // raw packet data
    std::vector<uint8_t> packet_buffer;
  };

  ProtocolPacketType packet_;
  mysql_protocol::Capabilities::Flags capabilities_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_PROTOCOL_DECODER_INCLUDED
