/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/streaming_command_delegate.h"

#include <stddef.h>
#include <iostream>
#include <string>

#include "decimal.h"
#include "my_dbug.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/column_info_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/row_builder.h"
#include "plugin/x/ngs/include/ngs_common/protocol_const.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/xpl_log.h"

using namespace xpl;

Streaming_command_delegate::Streaming_command_delegate(
    ngs::Protocol_encoder_interface *proto)
    : m_proto(proto), m_sent_result(false), m_compact_metadata(false) {}

Streaming_command_delegate::~Streaming_command_delegate() {}

void Streaming_command_delegate::reset() {
  m_sent_result = false;
  m_resultcs = NULL;
  Command_delegate::reset();
}

int Streaming_command_delegate::start_result_metadata(
    uint num_cols, uint flags, const CHARSET_INFO *resultcs) {
  if (Command_delegate::start_result_metadata(num_cols, flags, resultcs))
    return true;

  m_sent_result = true;
  m_resultcs = resultcs;
  return false;
}

int Streaming_command_delegate::field_metadata(struct st_send_field *field,
                                               const CHARSET_INFO *charset) {
  if (Command_delegate::field_metadata(field, charset)) return true;

  enum_field_types type = field->type;
  int32_t flags = 0;
  ngs::Column_info_builder column_info;

  if (field->flags & NOT_NULL_FLAG) flags |= MYSQLX_COLUMN_FLAGS_NOT_NULL;

  if (field->flags & PRI_KEY_FLAG) flags |= MYSQLX_COLUMN_FLAGS_PRIMARY_KEY;

  if (field->flags & UNIQUE_KEY_FLAG) flags |= MYSQLX_COLUMN_FLAGS_UNIQUE_KEY;

  if (field->flags & MULTIPLE_KEY_FLAG)
    flags |= MYSQLX_COLUMN_FLAGS_MULTIPLE_KEY;

  if (field->flags & AUTO_INCREMENT_FLAG)
    flags |= MYSQLX_COLUMN_FLAGS_AUTO_INCREMENT;

  if (MYSQL_TYPE_STRING == type) {
    if (field->flags & SET_FLAG)
      type = MYSQL_TYPE_SET;
    else if (field->flags & ENUM_FLAG)
      type = MYSQL_TYPE_ENUM;
  }

  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      column_info.set_length(field->length);

      if (field->flags & UNSIGNED_FLAG)
        column_info.set_type(Mysqlx::Resultset::ColumnMetaData::UINT);
      else
        column_info.set_type(Mysqlx::Resultset::ColumnMetaData::SINT);

      if (field->flags & ZEROFILL_FLAG)
        flags |= MYSQLX_COLUMN_FLAGS_UINT_ZEROFILL;
      break;

    case MYSQL_TYPE_FLOAT:
      if (field->flags & UNSIGNED_FLAG)
        flags |= MYSQLX_COLUMN_FLAGS_FLOAT_UNSIGNED;
      column_info.set_decimals(field->decimals);
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::FLOAT);
      break;

    case MYSQL_TYPE_DOUBLE:
      if (field->flags & UNSIGNED_FLAG)
        flags |= MYSQLX_COLUMN_FLAGS_DOUBLE_UNSIGNED;
      column_info.set_decimals(field->decimals);
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::DOUBLE);
      break;

    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      if (field->flags & UNSIGNED_FLAG)
        flags |= MYSQLX_COLUMN_FLAGS_DECIMAL_UNSIGNED;
      column_info.set_decimals(field->decimals);
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::DECIMAL);
      break;

    case MYSQL_TYPE_STRING:
      flags |= MYSQLX_COLUMN_FLAGS_BYTES_RIGHTPAD;
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      column_info.set_length(field->length);
      column_info.set_collation(
          charset ? charset->number : (m_resultcs ? m_resultcs->number : 0));
      break;

    case MYSQL_TYPE_SET:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::SET);
      column_info.set_collation(
          charset ? charset->number : (m_resultcs ? m_resultcs->number : 0));
      break;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      column_info.set_collation(
          charset ? charset->number : (m_resultcs ? m_resultcs->number : 0));
      break;

    case MYSQL_TYPE_JSON:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      column_info.set_content_type(Mysqlx::Resultset::JSON);
      column_info.set_length(field->length);
      column_info.set_collation(
          charset ? charset->number : (m_resultcs ? m_resultcs->number : 0));
      break;

    case MYSQL_TYPE_GEOMETRY:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      column_info.set_content_type(Mysqlx::Resultset::GEOMETRY);
      break;

    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::TIME);
      break;

    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::DATETIME);
      column_info.set_content_type(Mysqlx::Resultset::DATE);
      break;

    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::DATETIME);
      column_info.set_content_type(Mysqlx::Resultset::DATETIME);
      break;

    case MYSQL_TYPE_YEAR:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::UINT);
      break;

    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      flags |= MYSQLX_COLUMN_FLAGS_DATETIME_TIMESTAMP;
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::DATETIME);
      column_info.set_content_type(Mysqlx::Resultset::DATETIME);
      break;

    case MYSQL_TYPE_ENUM:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::ENUM);
      column_info.set_collation(
          charset ? charset->number : (m_resultcs ? m_resultcs->number : 0));
      break;

    case MYSQL_TYPE_NULL:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      break;

    case MYSQL_TYPE_BIT:
      column_info.set_length(field->length);
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BIT);
      break;
  }

  DBUG_ASSERT(column_info.get().m_type !=
              (Mysqlx::Resultset::ColumnMetaData::FieldType)0);

  if (!m_compact_metadata) {
    column_info.set_non_compact_data("def", field->col_name, field->table_name,
                                     field->db_name, field->org_col_name,
                                     field->org_table_name);
  }

  if (flags) column_info.set_flags(flags);

  if (m_proto->send_column_metadata(&column_info.get())) return false;

  my_message(ER_IO_WRITE_ERROR, "Connection reset by peer", MYF(0));
  return true;
}

int Streaming_command_delegate::end_result_metadata(uint server_status,
                                                    uint warn_count) {
  Command_delegate::end_result_metadata(server_status, warn_count);
  return false;
}

int Streaming_command_delegate::start_row() {
  if (!m_streaming_metadata) m_proto->start_row();
  return false;
}

int Streaming_command_delegate::end_row() {
  if (m_streaming_metadata) return false;

  if (m_proto->send_row()) return false;

  my_message(ER_IO_WRITE_ERROR, "Connection reset by peer", MYF(0));
  return true;
}

void Streaming_command_delegate::abort_row() {
  // Called when a resultset is being sent but an error occurs
  // For example, select 1, password('') while validate_password is ON;
  m_proto->abort_row();
}

ulong Streaming_command_delegate::get_client_capabilities() {
  return CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS | CLIENT_DEPRECATE_EOF;
}

/****** Getting data ******/
int Streaming_command_delegate::get_null() {
  m_proto->row_builder().add_null_field();

  return false;
}

int Streaming_command_delegate::get_integer(longlong value) {
  bool unsigned_flag =
      (m_field_types[m_proto->row_builder().get_num_fields()].flags &
       UNSIGNED_FLAG) != 0;

  return get_longlong(value, unsigned_flag);
}

int Streaming_command_delegate::get_longlong(longlong value,
                                             uint unsigned_flag) {
  // This is a hack to workaround server bugs similar to #77787:
  // Sometimes, server will not report a column to be UNSIGNED in the metadata,
  // but will send the data as unsigned anyway. That will cause the client to
  // receive messed up data because signed ints use zigzag encoding, while the
  // client will not be expecting that. So we add some bug-compatibility code
  // here, so that if column metadata reports column to be SIGNED, we will force
  // the data to actually be SIGNED.
  if (unsigned_flag &&
      (m_field_types[m_proto->row_builder().get_num_fields()].flags &
       UNSIGNED_FLAG) == 0)
    unsigned_flag = 0;

  // This is a hack to workaround server bug that causes wrong values being sent
  // for TINYINT UNSIGNED type, can be removed when it is fixed.
  if (unsigned_flag &&
      (m_field_types[m_proto->row_builder().get_num_fields()].type ==
       MYSQL_TYPE_TINY)) {
    value &= 0xff;
  }

  m_proto->row_builder().add_longlong_field(value, unsigned_flag);

  return false;
}

int Streaming_command_delegate::get_decimal(const decimal_t *value) {
  m_proto->row_builder().add_decimal_field(value);

  return false;
}

int Streaming_command_delegate::get_double(double value, uint32) {
  if (m_field_types[m_proto->row_builder().get_num_fields()].type ==
      MYSQL_TYPE_FLOAT)
    m_proto->row_builder().add_float_field(static_cast<float>(value));
  else
    m_proto->row_builder().add_double_field(value);
  return false;
}

int Streaming_command_delegate::get_date(const MYSQL_TIME *value) {
  m_proto->row_builder().add_date_field(value);

  return false;
}

int Streaming_command_delegate::get_time(const MYSQL_TIME *value,
                                         uint decimals) {
  m_proto->row_builder().add_time_field(value, decimals);

  return false;
}

int Streaming_command_delegate::get_datetime(const MYSQL_TIME *value,
                                             uint decimals) {
  m_proto->row_builder().add_datetime_field(value, decimals);

  return false;
}

int Streaming_command_delegate::get_string(const char *const value,
                                           size_t length,
                                           const CHARSET_INFO *const valuecs) {
  enum_field_types type =
      m_field_types[m_proto->row_builder().get_num_fields()].type;
  unsigned int flags =
      m_field_types[m_proto->row_builder().get_num_fields()].flags;

  switch (type) {
    case MYSQL_TYPE_NEWDECIMAL:
      m_proto->row_builder().add_decimal_field(value, length);
      break;
    case MYSQL_TYPE_SET:
      m_proto->row_builder().add_set_field(value, length, valuecs);
      break;
    case MYSQL_TYPE_BIT:
      m_proto->row_builder().add_bit_field(value, length, valuecs);
      break;
    case MYSQL_TYPE_STRING:
      if (flags & SET_FLAG) {
        m_proto->row_builder().add_set_field(value, length, valuecs);
        break;
      }
      /* fall through */
    default:
      m_proto->row_builder().add_string_field(value, length, valuecs);
      break;
  }
  return false;
}

/****** Getting execution status ******/
void Streaming_command_delegate::handle_ok(uint server_status,
                                           uint statement_warn_count,
                                           ulonglong affected_rows,
                                           ulonglong last_insert_id,
                                           const char *const message) {
  if (m_sent_result) {
    if (server_status & SERVER_MORE_RESULTS_EXISTS)
      m_proto->send_result_fetch_done_more_results();
    else
      m_proto->send_result_fetch_done();
  }
  Command_delegate::handle_ok(server_status, statement_warn_count,
                              affected_rows, last_insert_id, message);
}
