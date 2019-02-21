/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include <type_traits>

#include "decimal.h"
#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/interface/notice_output_queue_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/column_info_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_const.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/ngs/include/ngs/protocol/row_builder.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

using google::protobuf::io::CodedOutputStream;

namespace {

inline bool is_value_charset_valid(const CHARSET_INFO *resultset_cs,
                                   const CHARSET_INFO *value_cs) {
  return !resultset_cs || !value_cs ||
         my_charset_same(resultset_cs, value_cs) ||
         (resultset_cs == &my_charset_bin) || (value_cs == &my_charset_bin);
}

inline uint get_valid_charset_collation(const CHARSET_INFO *resultset_cs,
                                        const CHARSET_INFO *value_cs) {
  const CHARSET_INFO *cs =
      is_value_charset_valid(resultset_cs, value_cs) ? value_cs : resultset_cs;
  return cs ? cs->number : 0;
}

class Convert_if_necessary {
 public:
  Convert_if_necessary(const CHARSET_INFO *resultset_cs, const char *value,
                       const size_t value_length,
                       const CHARSET_INFO *value_cs) {
    if (is_value_charset_valid(resultset_cs, value_cs)) {
      m_ptr = value;
      m_len = value_length;
      return;
    }
    size_t result_length =
        resultset_cs->mbmaxlen * value_length / value_cs->mbminlen;
    m_buff.reset(new char[result_length]());
    uint errors = 0;
    result_length = my_convert(m_buff.get(), result_length, resultset_cs, value,
                               value_length, value_cs, &errors);
    if (errors) {
      log_debug("Error conversion data: %s(%s)", value, value_cs->csname);
      m_ptr = value;
      m_len = value_length;
    } else {
      m_ptr = m_buff.get();
      m_len = result_length;
    }
  }
  const char *get_ptr() const { return m_ptr; }
  size_t get_length() const { return m_len; }

 private:
  const char *m_ptr;
  size_t m_len;
  std::unique_ptr<char[]> m_buff;
};

}  // namespace

Streaming_command_delegate::Streaming_command_delegate(
    ngs::Session_interface *session)
    : m_proto(&session->proto()),
      m_notice_queue(&session->get_notice_output_queue()),
      m_sent_result(false),
      m_compact_metadata(false),
      m_session(session) {}

Streaming_command_delegate::~Streaming_command_delegate() { on_destruction(); }

void Streaming_command_delegate::reset() {
  log_debug("Streaming_command_delegate::reset");
  m_sent_result = false;
  m_resultcs = NULL;
  m_handle_ok_received = false;
  Command_delegate::reset();
}

int Streaming_command_delegate::start_result_metadata(
    uint num_cols, uint flags, const CHARSET_INFO *resultcs) {
  log_debug("Streaming_command_delegate::start_result_metadata flags:%i",
            (int)flags);
  if (Command_delegate::start_result_metadata(num_cols, flags, resultcs))
    return true;

  m_sent_result = true;
  m_resultcs = resultcs;
  m_proto->get_metadata_builder()->start_metadata_encoding();
  return false;
}

int Streaming_command_delegate::field_metadata(struct st_send_field *field,
                                               const CHARSET_INFO *charset) {
  log_debug("Streaming_command_delegate::field_metadata");
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
          get_valid_charset_collation(m_resultcs, charset));
      break;

    case MYSQL_TYPE_SET:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::SET);
      column_info.set_collation(
          get_valid_charset_collation(m_resultcs, charset));
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
          get_valid_charset_collation(m_resultcs, charset));
      break;

    case MYSQL_TYPE_JSON:
      column_info.set_type(Mysqlx::Resultset::ColumnMetaData::BYTES);
      column_info.set_content_type(Mysqlx::Resultset::JSON);
      column_info.set_length(field->length);
      column_info.set_collation(
          get_valid_charset_collation(m_resultcs, charset));
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
          get_valid_charset_collation(m_resultcs, charset));
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

  m_proto->get_metadata_builder()->encode_metadata(&column_info.get());

  return false;
}

int Streaming_command_delegate::end_result_metadata(uint server_status,
                                                    uint warn_count) {
  log_debug("Streaming_command_delegate::end_result_metadata server_status:%i",
            static_cast<int>(server_status));
  Command_delegate::end_result_metadata(server_status, warn_count);

  m_handle_ok_received = false;

  const auto &meta = m_proto->get_metadata_builder()->stop_metadata_encoding();

  CodedOutputStream(m_proto->get_buffer()).WriteString(meta);

  if (0 == meta.size()) return false;

  m_proto->get_flusher()->on_message(
      Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);

  if (m_proto->get_flusher()->try_flush()) return false;

  my_message(ER_IO_WRITE_ERROR, "Connection reset by peer", MYF(0));

  return true;
}

int Streaming_command_delegate::start_row() {
  log_debug("Streaming_command_delegate::start_row");
  if (!m_streaming_metadata) m_proto->start_row();
  return false;
}

int Streaming_command_delegate::end_row() {
  log_debug("Streaming_command_delegate::end_row");
  if (m_streaming_metadata) return false;

  if (m_proto->send_row()) {
    if (m_notice_queue) {
      const bool dont_force_flush_on_last_item = false;
      m_notice_queue->encode_queued_items(dont_force_flush_on_last_item);
    }
    return false;
  }

  my_message(ER_IO_WRITE_ERROR, "Connection reset by peer", MYF(0));
  return true;
}

void Streaming_command_delegate::abort_row() {
  log_debug("Streaming_command_delegate::abort_row");
  // Called when a resultset is being sent but an error occurs
  // For example, select 1, password('') while validate_password is ON;
  m_proto->abort_row();
}

ulong Streaming_command_delegate::get_client_capabilities() {
  return CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS | CLIENT_DEPRECATE_EOF |
         CLIENT_PS_MULTI_RESULTS;
}

/****** Getting data ******/
int Streaming_command_delegate::get_null() {
  log_debug("Streaming_command_delegate::get_time");
  m_proto->row_builder().add_null_field();

  return false;
}

int Streaming_command_delegate::get_integer(longlong value) {
  log_debug("Streaming_command_delegate::get_int %i", (int)value);
  bool unsigned_flag =
      (m_field_types[m_proto->row_builder().get_num_fields()].flags &
       UNSIGNED_FLAG) != 0;

  return get_longlong(value, unsigned_flag);
}

int Streaming_command_delegate::get_longlong(longlong value,
                                             uint unsigned_flag) {
  log_debug("Streaming_command_delegate::get_longlong %i", (int)value);
  // This is a hack to workaround server bugs similar to #77787:
  // Sometimes, server will not report a column to be UNSIGNED in the
  // metadata, but will send the data as unsigned anyway. That will cause the
  // client to receive messed up data because signed ints use zigzag encoding,
  // while the client will not be expecting that. So we add some
  // bug-compatibility code here, so that if column metadata reports column to
  // be SIGNED, we will force the data to actually be SIGNED.
  if (unsigned_flag &&
      (m_field_types[m_proto->row_builder().get_num_fields()].flags &
       UNSIGNED_FLAG) == 0)
    unsigned_flag = 0;

  // This is a hack to workaround server bug that causes wrong values being
  // sent for TINYINT UNSIGNED type, can be removed when it is fixed.
  if (unsigned_flag &&
      (m_field_types[m_proto->row_builder().get_num_fields()].type ==
       MYSQL_TYPE_TINY)) {
    value &= 0xff;
  }

  m_proto->row_builder().add_longlong_field(value, unsigned_flag);

  return false;
}

int Streaming_command_delegate::get_decimal(const decimal_t *value) {
  log_debug("Streaming_command_delegate::get_decimal");
  m_proto->row_builder().add_decimal_field(value);

  return false;
}

int Streaming_command_delegate::get_double(double value, uint32) {
  log_debug("Streaming_command_delegate::get_duble");
  if (m_field_types[m_proto->row_builder().get_num_fields()].type ==
      MYSQL_TYPE_FLOAT)
    m_proto->row_builder().add_float_field(static_cast<float>(value));
  else
    m_proto->row_builder().add_double_field(value);
  return false;
}

int Streaming_command_delegate::get_date(const MYSQL_TIME *value) {
  log_debug("Streaming_command_delegate::get_date");
  m_proto->row_builder().add_date_field(value);

  return false;
}

int Streaming_command_delegate::get_time(const MYSQL_TIME *value,
                                         uint decimals) {
  log_debug("Streaming_command_delegate::get_time");
  m_proto->row_builder().add_time_field(value, decimals);

  return false;
}

int Streaming_command_delegate::get_datetime(const MYSQL_TIME *value,
                                             uint decimals) {
  log_debug("Streaming_command_delegate::get_datetime");
  m_proto->row_builder().add_datetime_field(value, decimals);

  return false;
}

int Streaming_command_delegate::get_string(const char *const value,
                                           size_t length,
                                           const CHARSET_INFO *const valuecs) {
  log_debug("Streaming_command_delegate::get_string");
  const enum_field_types type =
      m_field_types[m_proto->row_builder().get_num_fields()].type;
  const unsigned int flags =
      m_field_types[m_proto->row_builder().get_num_fields()].flags;

  switch (type) {
    case MYSQL_TYPE_NEWDECIMAL:
      m_proto->row_builder().add_decimal_field(value, length);
      break;
    case MYSQL_TYPE_SET: {
      Convert_if_necessary conv(m_resultcs, value, length, valuecs);
      m_proto->row_builder().add_set_field(conv.get_ptr(), conv.get_length());
      break;
    }
    case MYSQL_TYPE_BIT:
      m_proto->row_builder().add_bit_field(value, length);
      break;
    case MYSQL_TYPE_STRING:
      if (flags & SET_FLAG) {
        Convert_if_necessary conv(m_resultcs, value, length, valuecs);
        m_proto->row_builder().add_set_field(conv.get_ptr(), conv.get_length());
        break;
      }
      /* fall through */
    default: {
      Convert_if_necessary conv(m_resultcs, value, length, valuecs);
      m_proto->row_builder().add_string_field(conv.get_ptr(),
                                              conv.get_length());
      break;
    }
  }
  return false;
}

/****** Getting execution status ******/
void Streaming_command_delegate::handle_ok(uint32_t server_status,
                                           uint32_t statement_warn_count,
                                           uint64_t affected_rows,
                                           uint64_t last_insert_id,
                                           const char *const message) {
  log_debug(
      "Streaming_command_delegate::handle_ok %i, warnings: %i, "
      "affected_rows:%i, last_insert_id: %i, msg: %s",
      (int)server_status, (int)statement_warn_count, (int)affected_rows,
      (int)last_insert_id, message);

  if (m_sent_result && !(server_status & SERVER_MORE_RESULTS_EXISTS)) {
    m_wait_for_fetch_done = false;
    m_proto->send_result_fetch_done();
  }

  if (!m_handle_ok_received && !m_wait_for_fetch_done &&
      try_send_notices(server_status, statement_warn_count, affected_rows,
                       last_insert_id, message))
    m_proto->send_exec_ok();
}

void Streaming_command_delegate::handle_error(uint sql_errno,
                                              const char *const err_msg,
                                              const char *const sqlstate) {
  if (m_handle_ok_received) {
    m_proto->send_result_fetch_done_more_results();
  }
  m_handle_ok_received = false;

  Command_delegate::handle_error(sql_errno, err_msg, sqlstate);
}

bool Streaming_command_delegate::try_send_notices(
    const uint32_t server_status, const uint32_t statement_warn_count,
    const uint64_t affected_rows, const uint64_t last_insert_id,
    const char *const message) {
  Command_delegate::handle_ok(server_status, statement_warn_count,
                              affected_rows, last_insert_id, message);
  return true;
}

void Streaming_command_delegate::on_destruction() {
  if (m_send_notice_deferred) {
    try_send_notices(m_info.server_status, m_info.num_warnings,
                     m_info.affected_rows, m_info.last_insert_id,
                     m_info.message.c_str());
    m_proto->send_exec_ok();
    m_send_notice_deferred = false;
  }
}

bool Streaming_command_delegate::defer_on_warning(
    const uint32_t server_status, const uint32_t statement_warn_count,
    const uint64_t affected_rows, const uint64_t last_insert_id,
    const char *const message) {
  if (!m_send_notice_deferred) {
    Command_delegate::handle_ok(server_status, statement_warn_count,
                                affected_rows, last_insert_id, message);
    bool show_warnings =
        m_session->get_notice_configuration().is_notice_enabled(
            ngs::Notice_type::k_warning);
    if (statement_warn_count > 0 && show_warnings) {
      // We cannot send a warning at this point because it would use
      // m_session->data_context() in here and we are already in
      // data_context.execute(). That is why we will deffer the whole notice
      // sending after we are done.
      m_send_notice_deferred = true;
      return true;
    }
  } else {
    notices::send_warnings(m_session->data_context(), *m_proto);
  }
  return false;
}

void Streaming_command_delegate::handle_fetch_done_more_results(
    uint32_t server_status) {
  const bool out_params = server_status & SERVER_PS_OUT_PARAMS;
  if (m_handle_ok_received && !out_params) {
    m_proto->send_result_fetch_done_more_results();
  }
}

void Streaming_command_delegate::end_result_metadata_handle_fetch(
    uint32_t server_status) {
  if (server_status & SERVER_PS_OUT_PARAMS)
    m_proto->send_result_fetch_done_more_out_params();
  handle_fetch_done_more_results(server_status);
}

void Streaming_command_delegate::handle_out_param_in_handle_ok(
    uint32_t server_status) {
  handle_fetch_done_more_results(server_status);

  const bool out_params = server_status & SERVER_PS_OUT_PARAMS;
  if (out_params) m_wait_for_fetch_done = true;

  const bool more_results = server_status & SERVER_MORE_RESULTS_EXISTS;
  m_handle_ok_received =
      (m_sent_result && more_results && !out_params) ? true : false;
}

}  // namespace xpl
