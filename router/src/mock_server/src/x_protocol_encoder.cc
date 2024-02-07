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

#include "x_protocol_encoder.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "mysql_protocol_common.h"  // MySQLColumnType

namespace server_mock {

void XProtocolEncoder::encode_row_field(
    Mysqlx::Resultset::Row &row_msg,
    const Mysqlx::Resultset::ColumnMetaData_FieldType type,
    const std::string &value, const bool is_null) {
  using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;
  using StringOutputStream = ::google::protobuf::io::StringOutputStream;

  std::string out_str;
  {
    StringOutputStream string_out_stream(&out_str);
    CodedOutputStream out_stream(&string_out_stream);

    if (is_null) {
      out_stream.WriteVarint32(0);
    } else {
      switch (type) {
        case Mysqlx::Resultset::ColumnMetaData_FieldType_BYTES: {
          out_stream.WriteRaw(value.c_str(), static_cast<int>(value.length()));
          char zero = '\0';
          out_stream.WriteRaw(&zero, 1);
          break;
        }
        case Mysqlx::Resultset::ColumnMetaData_FieldType_SINT: {
          // always signed now
          google::protobuf::uint64 encoded =
              google::protobuf::internal::WireFormatLite::ZigZagEncode64(
                  atoi(value.c_str()));
          out_stream.WriteVarint64(encoded);
          break;
        }
        case Mysqlx::Resultset::ColumnMetaData_FieldType_FLOAT: {
          out_stream.WriteLittleEndian32(
              google::protobuf::internal::WireFormatLite::EncodeFloat(
                  atof(value.c_str())));
          break;
        }
        case Mysqlx::Resultset::ColumnMetaData_FieldType_DOUBLE: {
          out_stream.WriteLittleEndian64(
              google::protobuf::internal::WireFormatLite::EncodeDouble(
                  atof(value.c_str())));
          break;
        }
        default:
          throw std::runtime_error("Unsupported column type: " +
                                   std::to_string(static_cast<int>(type)));
      }
    }
  }
  row_msg.add_field(out_str);
}

void XProtocolEncoder::encode_metadata(
    Mysqlx::Resultset::ColumnMetaData &metadata_msg,
    const classic_protocol::message::server::ColumnMeta &column) {
  metadata_msg.set_type(column_type_to_x(column.type()));
  metadata_msg.set_name(column.name());
  metadata_msg.set_original_name(column.orig_name());
  metadata_msg.set_table(column.table());
  metadata_msg.set_original_table(column.orig_table());
  metadata_msg.set_schema(column.schema());
  metadata_msg.set_catalog(column.catalog());
  metadata_msg.set_collation(column.collation());
  metadata_msg.set_fractional_digits(column.decimals());
  metadata_msg.set_length(column.column_length());
  metadata_msg.set_flags(column.flags().to_ullong());
}

void XProtocolEncoder::encode_error(Mysqlx::Error &err_msg,
                                    const uint16_t error_code,
                                    const std::string &error_txt,
                                    const std::string &sql_state) {
  err_msg.set_sql_state(sql_state);
  err_msg.set_code(error_code);
  err_msg.set_msg(error_txt);
}

Mysqlx::Resultset::ColumnMetaData_FieldType XProtocolEncoder::column_type_to_x(
    const uint8_t column_type) {
  switch (MySQLColumnType(column_type)) {
    case MySQLColumnType::DECIMAL:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_DECIMAL;
    case MySQLColumnType::TINY:
    case MySQLColumnType::SHORT:
    case MySQLColumnType::LONG:
    case MySQLColumnType::LONGLONG:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_SINT;
    case MySQLColumnType::FLOAT:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_FLOAT;
    case MySQLColumnType::DOUBLE:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_DOUBLE;
    case MySQLColumnType::DATE:
    case MySQLColumnType::DATETIME:
    case MySQLColumnType::TIMESTAMP:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_DATETIME;
    case MySQLColumnType::TIME:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_TIME;
    case MySQLColumnType::BIT:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_BIT;
    case MySQLColumnType::ENUM:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_ENUM;
    case MySQLColumnType::VAR_STRING:
    case MySQLColumnType::STRING:
    case MySQLColumnType::LONG_BLOB:
    case MySQLColumnType::TINY_BLOB:
    case MySQLColumnType::MEDIUM_BLOB:
    case MySQLColumnType::BLOB:
    case MySQLColumnType::GEOMETRY:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_BYTES;
    case MySQLColumnType::SET:
      return Mysqlx::Resultset::ColumnMetaData_FieldType_SET;
    default:;
  }
  throw std::runtime_error("Unsupported column type: " +
                           std::to_string(static_cast<int>(column_type)));
}

}  // namespace server_mock
