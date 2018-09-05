/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_ERROR_PACKET_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_ERROR_PACKET_INCLUDED

#include "base_packet.h"

namespace mysql_protocol {

/** @class ErrorPacket
 * @brief Creates a MySQL error packet
 *
 * This class creates a MySQL error packet which is send to the MySQL Client.
 *
 */
class MYSQL_PROTOCOL_API ErrorPacket final : public Packet {
 public:
  /** @brief Constructor
   *
   * @note The default constructor will set the error code to 1105, message
   * "Unknown error", and SQL State "HY000". These values come from the
   * MySQL Server server errors.
   */
  ErrorPacket()
      : Packet(0), code_(1105), message_("Unknown error"), sql_state_("HY000") {
    prepare_packet();
  }

  /** @overload
   *
   * @param buffer bytes of the error packet
   */
  ErrorPacket(const std::vector<uint8_t> &buffer)
      : ErrorPacket(buffer, Capabilities::ALL_ZEROS) {}

  ErrorPacket(const std::vector<uint8_t> &buffer,
              Capabilities::Flags capabilities);

  /** @overload
   *
   * @param sequence_id MySQL Packet number
   * @param err_code Error code provided to MySQL client
   * @param err_msg Error message provided to MySQL client
   * @param sql_state SQL State used in error message
   * @param capabilities Server/Client capability flags (default 0)
   */
  ErrorPacket(uint8_t sequence_id, uint16_t err_code,
              const std::string &err_msg, const std::string &sql_state,
              Capabilities::Flags capabilities = Capabilities::ALL_ZEROS);

  /** @brief Gets error code
   *
   * Gets the MySQL error code of the MySQL error packet.
   *
   * @return unsigned short
   */
  unsigned short get_code() const noexcept { return code_; }

  /** @brief Gets error message
   *
   * Gets the MySQL error message of the MySQL error packet.
   *
   * @return const std::string reference
   */
  const std::string &get_message() const noexcept { return message_; }

  /** @brief Gets SQL state
   *
   * Gets the SQL state of the MySQL error packet.
   *
   * @return const std::string reference
   */
  const std::string &get_sql_state() const noexcept { return sql_state_; }

 private:
  /** @brief Prepares the packet
   *
   * Prepares the actual MySQL Error packet and stores it. The header is
   * created using the sequence id and the size of the payload.
   */
  void prepare_packet();

  /** @brief Parses the packet
   *
   * Parses the packet from the given buffer.
   */
  void parse_payload();

  /** @brief MySQL error code */
  unsigned short code_;

  /** @brief MySQL error message */
  std::string message_;

  /** @brief MySQL SQL state */
  std::string sql_state_;
};

}  // namespace mysql_protocol

#endif  // MYSQLROUTER_MYSQL_PROTOCOL_ERROR_PACKET_INCLUDED
