/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "x_protocol_decoder.h"

namespace server_mock {

std::unique_ptr<xcl::XProtocol::Message> XProtocolDecoder::decode_message(
    const uint8_t mid, const uint8_t *payload,
    const std::size_t payload_size) const {
  std::unique_ptr<xcl::XProtocol::Message> ret_val;

  switch (static_cast<Mysqlx::ClientMessages::Type>(mid)) {
    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      ret_val.reset(new Mysqlx::Connection::CapabilitiesGet());
      break;
    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      ret_val.reset(new Mysqlx::Connection::CapabilitiesSet());
      break;
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      ret_val.reset(new Mysqlx::Session::AuthenticateStart());
      break;
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE:
      ret_val.reset(new Mysqlx::Session::AuthenticateContinue());
      break;
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
      ret_val.reset(new Mysqlx::Sql::StmtExecute());
      break;
    case Mysqlx::ClientMessages::CON_CLOSE:
      ret_val.reset(new Mysqlx::Connection::Close());
      break;
    default:
      throw std::runtime_error(
          "Got unsupported message from the client; msg_id=" +
          std::to_string(mid));
  }

  // Parse the received message
  ret_val->ParseFromArray(reinterpret_cast<const char *>(payload),
                          static_cast<int>(payload_size));

  if (!ret_val->IsInitialized()) {
    throw std::runtime_error(
        "Error parsing the message from the client; msg_id=" +
        std::to_string(mid));
  }

  return ret_val;
}

}  // namespace server_mock
