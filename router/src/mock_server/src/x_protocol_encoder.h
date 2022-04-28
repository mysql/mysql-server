/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLD_MOCK_X_PROTOCOL_ENCODER_INCLUDED
#define MYSQLD_MOCK_X_PROTOCOL_ENCODER_INCLUDED

#include <cstdint>
#include <string>
#include <vector>

#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlxclient/xprotocol.h"

namespace server_mock {

class XProtocolEncoder {
 public:
  // none of those throw directly in our code
  void encode_row_field(Mysqlx::Resultset::Row &row_msg,
                        const Mysqlx::Resultset::ColumnMetaData_FieldType type,
                        const std::string &value, const bool is_null);

  void encode_metadata(
      Mysqlx::Resultset::ColumnMetaData &metadata_msg,
      const classic_protocol::message::server::ColumnMeta &column);

  void encode_error(Mysqlx::Error &err_msg, const uint16_t error_code,
                    const std::string &error_txt, const std::string &sql_state);

  // throws std::runtime_error
  Mysqlx::Resultset::ColumnMetaData_FieldType column_type_to_x(
      const uint8_t column_type);
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_X_PROTOCOL_ENCODER_INCLUDED
