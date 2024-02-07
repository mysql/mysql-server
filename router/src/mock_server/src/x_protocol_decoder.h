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

#ifndef MYSQLD_MOCK_X_PROTOCOL_DECODER_INCLUDED
#define MYSQLD_MOCK_X_PROTOCOL_DECODER_INCLUDED

#include <stdint.h>
#include <string>
#include <vector>

#include "mysqlxclient/xprotocol.h"

namespace server_mock {

class XProtocolDecoder {
 public:
  // throws std::runtime_error
  std::unique_ptr<xcl::XProtocol::Message> decode_message(
      const uint8_t mid, const uint8_t *payload,
      const std::size_t payload_size) const;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_X_PROTOCOL_DECODER_INCLUDED
