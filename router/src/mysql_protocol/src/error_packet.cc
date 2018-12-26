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

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace mysql_protocol {

ErrorPacket::ErrorPacket(uint8_t sequence_id, uint16_t err_code,
                         const std::string &err_msg,
                         const std::string &sql_state,
                         Capabilities::Flags capabilities)
    : Packet(sequence_id, capabilities),
      code_(err_code),
      message_(err_msg),
      sql_state_(sql_state) {
  prepare_packet();
}

ErrorPacket::ErrorPacket(const std::vector<uint8_t> &buffer,
                         Capabilities::Flags capabilities)
    : Packet(buffer, capabilities) {
  parse_payload();
}

static constexpr uint8_t kHashChar = 0x23;  // 0x23 == '#'

void ErrorPacket::prepare_packet() {
  assert(sql_state_.size() == 5);

  reset();
  position_ = size();

  reserve(size() + sizeof(uint8_t) +  // error identifier byte
          sizeof(uint16_t) +          // error code
          sizeof(uint8_t) +           // SQL state
          message_.size()             // the message
  );

  // Error identifier byte
  write_int<uint8_t>(0xff);

  // error code
  write_int<uint16_t>(code_);

  // SQL State
  if (capability_flags_.test(Capabilities::PROTOCOL_41)) {
    write_int<uint8_t>(kHashChar);
    if (sql_state_.size() != 5) {
      write_string("HY000");
    } else {
      write_string(sql_state_);
    }
  }

  // The message
  write_string(message_);

  // Update the payload size in the header
  update_packet_size();
}

void ErrorPacket::parse_payload() {
  bool prot41 = capability_flags_.test(Capabilities::PROTOCOL_41);
  // Sanity checks
  if (!((*this)[4] == 0xff && (*this)[6])) {
    throw packet_error("Error packet marker 0xff not found");
  }
  // Check if SQLState is available when CLIENT_PROTOCOL_41 flag is set
  if (prot41 && (*this)[7] != kHashChar) {
    throw packet_error("Error packet does not contain SQL state");
  }

  unsigned long pos = 5;
  code_ = read_int_from<uint16_t>(pos);
  pos += 2;
  if ((*this)[7] == kHashChar) {
    // We get the SQLState even when CLIENT_PROTOCOL_41 flag was not set
    // This is needed in cases when the server sends an
    // error to the client instead of the handshake.
    sql_state_ = read_string_from(++pos, 5);  // We skip kHashChar ('#')
    pos += 5;
  } else {
    sql_state_ = "";
  }
  message_ = read_string_from(pos);
}

}  // namespace mysql_protocol
