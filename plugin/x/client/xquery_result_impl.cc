/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/client/xquery_result_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "errmsg.h"
#include "my_compiler.h"
#include "plugin/x/client/mysqlxclient/mysqlxclient_error.h"
#include "plugin/x/client/mysqlxclient/xrow.h"

namespace details {

static xcl::Column_metadata unwrap_column_metadata(
    const Mysqlx::Resultset::ColumnMetaData *column_data) {
  xcl::Column_metadata column;

  switch (column_data->type()) {
    case Mysqlx::Resultset::ColumnMetaData::SINT:
      column.type = xcl::Column_type::SINT;
      break;

    case Mysqlx::Resultset::ColumnMetaData::UINT:
      column.type = xcl::Column_type::UINT;
      break;

    case Mysqlx::Resultset::ColumnMetaData::DOUBLE:
      column.type = xcl::Column_type::DOUBLE;
      break;

    case Mysqlx::Resultset::ColumnMetaData::FLOAT:
      column.type = xcl::Column_type::FLOAT;
      break;

    case Mysqlx::Resultset::ColumnMetaData::BYTES:
      column.type = xcl::Column_type::BYTES;
      break;

    case Mysqlx::Resultset::ColumnMetaData::TIME:
      column.type = xcl::Column_type::TIME;
      break;

    case Mysqlx::Resultset::ColumnMetaData::DATETIME:
      column.type = xcl::Column_type::DATETIME;
      break;

    case Mysqlx::Resultset::ColumnMetaData::SET:
      column.type = xcl::Column_type::SET;
      break;

    case Mysqlx::Resultset::ColumnMetaData::ENUM:
      column.type = xcl::Column_type::ENUM;
      break;

    case Mysqlx::Resultset::ColumnMetaData::BIT:
      column.type = xcl::Column_type::BIT;
      break;

    case Mysqlx::Resultset::ColumnMetaData::DECIMAL:
      column.type = xcl::Column_type::DECIMAL;
      break;
  }

  column.name = column_data->name();
  column.original_name = column_data->original_name();

  column.table = column_data->table();
  column.original_table = column_data->original_table();

  column.schema = column_data->schema();
  column.catalog = column_data->catalog();

  column.collation =
      column_data->has_collation() ? column_data->collation() : 0;

  column.fractional_digits = column_data->fractional_digits();

  column.length = column_data->length();

  column.flags = column_data->flags();
  column.has_content_type = column_data->has_content_type();
  column.content_type = column_data->content_type();

  return column;
}

template <typename Src_type, typename Dst_type>
void unique_ptr_cast(Src_type &src, Dst_type &dst) {  // NOLINT
  if (!src) {
    dst.reset();
    return;
  }

  auto dst_ptr = reinterpret_cast<typename Dst_type::pointer>(src.release());

  dst.reset(dst_ptr);
}

}  // namespace details

namespace xcl {

const char *const ERR_LAST_COMMAND_UNFINISHED =
    "Fetching wrong result set, there is previous command pending.";

Query_result::Query_result(std::shared_ptr<XProtocol> protocol,
                           Query_instances *query_instances,
                           std::shared_ptr<Context> context)
    : m_protocol(protocol),
      m_holder(protocol.get()),
      m_row(&m_metadata, context.get()),
      m_query_instances(query_instances),
      m_instance_id(m_query_instances->instances_fetch_begin()),
      m_context(context) {
  m_notice_handler_id = m_protocol->add_notice_handler(
      [this](XProtocol *protocol [[maybe_unused]], const bool is_global,
             const Mysqlx::Notice::Frame::Type type, const char *payload,
             const uint32_t payload_size) -> Handler_result {
        if (is_global) return Handler_result::Continue;

        return handle_notice(type, payload, payload_size);
      });
}

Query_result::~Query_result() {
  while (had_fetch_not_ended()) {
    next_resultset(&m_error);
  }
}

bool Query_result::had_fetch_not_ended() const {
  return !m_error && !m_received_fetch_done;
}

bool Query_result::try_get_last_insert_id(uint64_t *out_value) const {
  return m_last_insert_id.get_value(out_value);
}

bool Query_result::try_get_affected_rows(uint64_t *out_value) const {
  return m_affected_rows.get_value(out_value);
}

bool Query_result::try_get_info_message(std::string *out_value) const {
  return m_producted_message.get_value(out_value);
}

bool Query_result::try_get_generated_document_ids(
    std::vector<std::string> *out_ids) const {
  if (m_generated_document_ids.empty()) return false;
  *out_ids = m_generated_document_ids;
  return true;
}

bool Query_result::next_resultset(XError *out_error) {
  m_metadata.clear();

  if (!had_fetch_not_ended()) {
    if (nullptr != out_error) *out_error = m_error;

    return false;
  }

  if (!verify_current_instance(out_error)) {
    return false;
  }

  if (check_if_fetch_done()) {
    return false;
  }

  const bool is_end_result_msg = m_holder.is_one_of(
      {::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
       ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS,
       ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
       ::Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED});

  if (!is_end_result_msg) {
    check_error(m_holder.read_until_expected_msg_received(
        {::Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED},
        {::Mysqlx::ServerMessages::NOTICE,
         ::Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
         ::Mysqlx::ServerMessages::RESULTSET_ROW}));
  }

  // Accept another series of
  // RESULTSET_COLUMN_META_DATA
  m_read_metadata = true;

  if (m_error) {
    if (nullptr != out_error) *out_error = m_error;

    return false;
  }

  if (m_holder.is_one_of(
          {::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS})) {
    m_is_out_param_resultset = true;
  }

  if (!m_holder.is_one_of({::Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
                           ::Mysqlx::ServerMessages::RESULTSET_ROW,
                           ::Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK})) {
    m_holder.clear_cached_message();
  }

  check_if_stmt_ok();

  if (m_error) {
    if (nullptr != out_error) *out_error = m_error;

    return false;
  }

  return had_fetch_not_ended();
}

Handler_result Query_result::handle_notice(
    const Mysqlx::Notice::Frame::Type type, const char *payload,
    const uint32_t payload_size) {
  switch (type) {
    case Mysqlx::Notice::Frame_Type_WARNING: {
      Mysqlx::Notice::Warning warning;
      warning.ParseFromArray(payload, payload_size);

      if (!warning.IsInitialized()) return Handler_result::Error;

      m_warnings.push_back(warning);

      return Handler_result::Consumed;
    }

    case Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED: {
      Mysqlx::Notice::SessionStateChanged change;
      change.ParseFromArray(payload, payload_size);
      if (!change.IsInitialized()) return Handler_result::Error;

      switch (change.param()) {
        case Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID:
          if (change.value_size() != 1) return Handler_result::Error;
          if (change.value(0).type() == Mysqlx::Datatypes::Scalar::V_UINT)
            m_last_insert_id = change.value(0).v_unsigned_int();
          break;

        case Mysqlx::Notice::SessionStateChanged::ROWS_AFFECTED:
          if (change.value_size() != 1) return Handler_result::Error;
          if (change.value(0).type() == Mysqlx::Datatypes::Scalar::V_UINT)
            m_affected_rows = change.value(0).v_unsigned_int();
          break;

        case Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE:
          if (change.value_size() != 1) return Handler_result::Error;
          if (change.value(0).type() == Mysqlx::Datatypes::Scalar::V_STRING)
            m_producted_message = change.value(0).v_string().value();
          break;

        case Mysqlx::Notice::SessionStateChanged::GENERATED_DOCUMENT_IDS:
          m_generated_document_ids.clear();
          m_generated_document_ids.reserve(change.value_size());
          for (const auto &value : change.value())
            if (value.type() == Mysqlx::Datatypes::Scalar::V_OCTETS)
              m_generated_document_ids.push_back(value.v_octets().value());
          break;

        default:
          return Handler_result::Continue;
      }
      return Handler_result::Consumed;
    }

    default:
      return Handler_result::Continue;
  }
}

void Query_result::check_if_stmt_ok() {
  const auto message_id = m_holder.get_cached_message_id();
  if (!m_error &&
      (Mysqlx::ServerMessages::RESULTSET_FETCH_DONE == message_id ||
       Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED == message_id)) {
    m_holder.clear_cached_message();
    check_error(m_holder.read_until_expected_msg_received(
        {Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK},
        {Mysqlx::ServerMessages::NOTICE}));
  }

  if (m_error) return;

  check_if_fetch_done();
}

const XQuery_result::Metadata &Query_result::get_metadata(XError *out_error) {
  if (had_fetch_not_ended()) {
    if (!verify_current_instance(out_error)) return m_metadata;

    read_if_needed_metadata();
    check_if_fetch_done();

    if (out_error && m_error) *out_error = m_error;
  }

  return m_metadata;
}

void Query_result::set_metadata(const Metadata &metadata) {
  m_metadata = metadata;
  m_read_metadata = false;
}

const XQuery_result::Warnings &Query_result::get_warnings() {
  return m_warnings;
}

Query_result::Row_ptr Query_result::get_next_row_raw(XError *out_error) {
  if (!had_fetch_not_ended()) return {};

  if (!verify_current_instance(out_error)) return {};

  read_if_needed_metadata();
  auto row = read_row();
  check_if_stmt_ok();

  if (out_error) *out_error = m_error;

  return row;
}

bool Query_result::get_next_row(const XRow **out_row, XError *out_error) {
  m_row.clean();
  m_row.set_row(get_next_row_raw(out_error));

  if (m_row.valid()) {
    *out_row = &m_row;

    return true;
  }

  return false;
}

const XRow *Query_result::get_next_row(XError *out_error) {
  const XRow *row;
  if (get_next_row(&row, out_error)) return row;
  return nullptr;
}

bool Query_result::has_resultset(XError *out_error) {
  return !get_metadata(out_error).empty();
}

void Query_result::read_if_needed_metadata() {
  if (!m_error && m_read_metadata) {
    m_read_metadata = false;
    check_error(m_holder.read_until_expected_msg_received(
        {Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
         Mysqlx::ServerMessages::RESULTSET_ROW,
         Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
         Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
         Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS,
         Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED},
        [this](const XProtocol::Server_message_type_id message_id,
               std::unique_ptr<XProtocol::Message> &message) -> XError {
          return read_metadata(message_id, message);
        }));
  }
}

Query_result::Row_ptr Query_result::read_row() {
  std::unique_ptr<Mysqlx::Resultset::Row> row;

  if (!m_holder.has_cached_message()) {
    check_error(m_holder.read_until_expected_msg_received(
        {::Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
         ::Mysqlx::ServerMessages::RESULTSET_ROW,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED},
        {::Mysqlx::ServerMessages::NOTICE}));
  }

  if (!m_error && Mysqlx::ServerMessages::RESULTSET_ROW ==
                      m_holder.get_cached_message_id()) {
    details::unique_ptr_cast(m_holder.m_message, row);

    check_error(m_holder.read_until_expected_msg_received(
        {::Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
         ::Mysqlx::ServerMessages::RESULTSET_ROW,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS,
         ::Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED},
        {::Mysqlx::ServerMessages::NOTICE}));
  }

  return row;
}

XError Query_result::read_metadata(
    const XProtocol::Server_message_type_id msg_id,
    std::unique_ptr<XProtocol::Message> &msg) {
  if (Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA == msg_id) {
    auto column_metadata =
        reinterpret_cast<Mysqlx::Resultset::ColumnMetaData *>(msg.get());

    m_metadata.push_back(details::unwrap_column_metadata(column_metadata));
  }

  return {};
}

bool Query_result::verify_current_instance(XError *out_error) {
  if (!m_query_instances->is_instance_active(m_instance_id)) {
    m_context->m_global_error = m_error =
        XError{CR_X_LAST_COMMAND_UNFINISHED, ERR_LAST_COMMAND_UNFINISHED};

    if (nullptr != out_error) *out_error = m_error;

    return false;
  }

  return true;
}

void Query_result::check_error(const XError &error) {
  if (error && !m_error) {
    m_error = error;

    if (!m_received_fetch_done) {
      m_query_instances->instances_fetch_end();
      m_protocol->remove_notice_handler(m_notice_handler_id);
    }
  }
}

bool Query_result::check_if_fetch_done() {
  if (!m_error && !m_received_fetch_done) {
    if (m_holder.is_one_of({Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK})) {
      m_query_instances->instances_fetch_end();
      m_protocol->remove_notice_handler(m_notice_handler_id);
      m_received_fetch_done = true;
    }
  }

  return m_received_fetch_done;
}

bool Query_result::is_out_parameter_resultset() const {
  return m_is_out_param_resultset;
}

}  // namespace xcl
