/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/prepare_command_handler.h"

#include <cinttypes>
#include <limits>
#include <string>

#include <my_byteorder.h>

#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/prepared_statement_builder.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {
namespace {
class Prepare_resultset : public Process_resultset {
 public:
  Prepare_resultset() = default;
  uint32_t get_stmt_id() const { return m_stmt_id; }

 protected:
  Row *start_row() override {
    m_row.clear();
    return &m_row;
  }

  bool end_row(Row *row) override {
    if (row->fields.empty()) return false;
    m_stmt_id = row->fields[0]->value.v_long;
    return true;
  }

 private:
  Row m_row;
  uint32_t m_stmt_id{0};
};

class Any_to_param_handler {
 public:
  explicit Any_to_param_handler(
      Prepare_command_handler::Param_list *params,
      Prepare_command_handler::Param_value_list *param_values)
      : m_params(params), m_param_values(param_values) {}

  void operator()() {
    m_params->push_back({true, MYSQL_TYPE_NULL, false, nullptr, 0ul});
  }
  void operator()(const int64_t value) {
    m_params->push_back(
        {false, MYSQL_TYPE_LONGLONG, false, store_value(value), sizeof(value)});
  }
  void operator()(const uint64_t value) {
    m_params->push_back(
        {false, MYSQL_TYPE_LONGLONG, true, store_value(value), sizeof(value)});
  }
  void operator()(const std::string &value) {
    m_params->push_back(
        {false, MYSQL_TYPE_STRING, false,
         reinterpret_cast<const unsigned char *>(value.data()),
         static_cast<unsigned long>(value.size())});  // NOLINT(runtime/int)
  }
  void operator()(const double value) {
    m_params->push_back(
        {false, MYSQL_TYPE_DOUBLE, false, store_value(value), sizeof(value)});
  }
  void operator()(const float value) {
    m_params->push_back(
        {false, MYSQL_TYPE_FLOAT, false, store_value(value), sizeof(value)});
  }
  void operator()(const bool value) {
    m_params->push_back(
        {false, MYSQL_TYPE_TINY, false, store_value(value), 1ul});
  }

 private:
  template <typename T>
  const unsigned char *store_value(const T value) const {
    Prepare_command_handler::Param_value_list::value_type empty_val = {};
    auto pos = m_param_values->insert(m_param_values->end(), empty_val);
    store(pos->data(), value);
    return pos->data();
  }

  void store(unsigned char *buf, const uint64_t v) const { int8store(buf, v); }
  void store(unsigned char *buf, const int64_t v) const { int8store(buf, v); }
  void store(unsigned char *buf, const double v) const { float8store(buf, v); }
  void store(unsigned char *buf, const float v) const { float4store(buf, v); }
  void store(unsigned char *buf, const bool v) const { buf[0] = v ? 1 : 0; }

  Prepare_command_handler::Param_list *m_params;
  Prepare_command_handler::Param_value_list *m_param_values;
};

inline bool is_table_model(const Prepare_command_handler::Prepare &msg) {
  switch (msg.stmt().type()) {
    case Prepare_command_handler::Prepare::OneOfMessage::FIND:
      return is_table_data_model(msg.stmt().find());
    case Prepare_command_handler::Prepare::OneOfMessage::INSERT:
      return is_table_data_model(msg.stmt().insert());
    case Prepare_command_handler::Prepare::OneOfMessage::UPDATE:
      return is_table_data_model(msg.stmt().update());
    case Prepare_command_handler::Prepare::OneOfMessage::DELETE:
      return is_table_data_model(msg.stmt().delete_());
    case Prepare_command_handler::Prepare::OneOfMessage::STMT: {
    }
  }
  return true;
}

}  // namespace

using Cursor_info = Prepare_command_handler::Cursor_info;
using Prepared_stmt_info = Prepare_command_handler::Prepared_stmt_info;

ngs::Error_code Prepare_command_handler::execute_prepare(const Prepare &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_prep_prepare);
  const auto client_stmt_id = msg.stmt_id();

  auto prep_stmt_info = get_stmt_if_allocated(client_stmt_id);

  if (nullptr != prep_stmt_info) {
    ngs::Error_code error =
        execute_deallocate_impl(client_stmt_id, prep_stmt_info);
    if (error) return error;
  }

  Placeholder_id_list placeholder_ids;
  uint32_t args_offset = 0;
  ngs::Error_code error =
      build_query(msg.stmt(), &placeholder_ids, &args_offset);
  if (error) return error;

  Prepare_resultset rset;
  error = m_session->data_context().prepare_prep_stmt(
      m_qb.get().data(), m_qb.get().length(), &rset);
  if (error) return error;

  insert_prepared_statement(
      msg.stmt_id(),
      {rset.get_stmt_id(), msg.stmt().type(), placeholder_ids, args_offset,
       is_table_model(msg), false, std::numeric_limits<Id_type>::max()});

  m_session->proto().send_ok();
  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::execute_execute(const Execute &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_prep_execute);

  Prepared_stmt_info *prep_stmt_info = get_stmt_if_allocated(msg.stmt_id());
  if (nullptr == prep_stmt_info)
    return ngs::Error(ER_X_BAD_STATEMENT_ID,
                      "Statement with ID=%" PRIu32 " was not prepared",
                      msg.stmt_id());

  Streaming_resultset<Prepare_command_delegate> rset(m_session,
                                                     msg.compact_metadata());
  rset.get_delegate().set_notice_level(get_notice_level_flags(prep_stmt_info));

  return execute_execute_impl(msg, rset, prep_stmt_info);
}

ngs::Error_code Prepare_command_handler::execute_execute_impl(
    const Execute &msg, ngs::Resultset_interface &rset,
    Prepared_stmt_info *prep_stmt_info) {
  ngs::Error_code error = check_argument_placeholder_consistency(
      msg.args_size(), prep_stmt_info->m_placeholder_ids,
      prep_stmt_info->m_args_offset);
  if (error) return error;

  // Lets prepare a list of parameters accepted by MySQL session service.
  // Still the parameter list contains pointer data, thus we need to
  // supply additional container to hold the data, which is `param_values`.
  //
  // `param_values` holds only pointers to numeric types, string types
  // are owned by `msg`.
  Param_list params;
  Param_value_list param_values;
  error = prepare_parameters(msg.args(), prep_stmt_info->m_placeholder_ids,
                             &params, &param_values);
  if (error) return error;

  ngs::Document_id_aggregator_interface::Retention_guard g(
      prep_stmt_info->m_type == Prepare::OneOfMessage::INSERT
          ? &m_session->get_document_id_aggregator()
          : nullptr);

  error = m_session->data_context().execute_prep_stmt(
      prep_stmt_info->m_server_stmt_id, prep_stmt_info->m_has_cursor,
      params.data(), params.size(), &rset);
  if (error) return error;

  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::execute_deallocate(
    const Deallocate &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_prep_deallocate);
  const auto client_stmt_id = msg.stmt_id();
  Prepared_stmt_info *prep_stmt_info = get_stmt_if_allocated(client_stmt_id);

  if (nullptr == prep_stmt_info) {
    return ngs::Error(ER_X_BAD_STATEMENT_ID,
                      "Statement with ID=%" PRIu32 " was not prepared",
                      client_stmt_id);
  }

  ngs::Error_code error =
      execute_deallocate_impl(client_stmt_id, prep_stmt_info);
  if (error) return error;

  m_session->proto().send_ok();
  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::build_query(
    const Prepare::OneOfMessage &msg, std::vector<uint32_t> *ids,
    uint32_t *args_offset) {
  Prepared_statement_builder builder(&m_qb, ids);
  switch (msg.type()) {
    case Prepare::OneOfMessage::FIND:
      *args_offset = msg.find().args_size();
      return builder.build(msg.find());
    case Prepare::OneOfMessage::INSERT:
      *args_offset = msg.insert().args_size();
      return builder.build(msg.insert());
    case Prepare::OneOfMessage::UPDATE:
      *args_offset = msg.update().args_size();
      return builder.build(msg.update());
    case Prepare::OneOfMessage::DELETE:
      *args_offset = msg.delete_().args_size();
      return builder.build(msg.delete_());
    case Prepare::OneOfMessage::STMT:
      *args_offset = msg.stmt_execute().args_size();
      return builder.build(msg.stmt_execute());
  }
  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::execute_deallocate_impl(
    const Id_type client_stmt_id, const Prepared_stmt_info *prep_stmt_info) {
  Empty_resultset rset;
  const auto error = m_session->data_context().deallocate_prep_stmt(
      prep_stmt_info->m_server_stmt_id, &rset);

  if (!error) {
    if (prep_stmt_info->m_has_cursor)
      m_cursors_info.erase(prep_stmt_info->m_cursor_id);

    m_prepared_stmt_info.erase(client_stmt_id);
  }

  return error;
}

Cursor_info *Prepare_command_handler::get_cursor_if_allocated(
    const Id_type cursor_id) {
  auto iterator = m_cursors_info.find(cursor_id);

  if (m_cursors_info.end() == iterator) return nullptr;

  return &iterator->second;
}

Prepared_stmt_info *Prepare_command_handler::get_stmt_if_allocated(
    const Id_type client_stmt_id) {
  auto iterator = m_prepared_stmt_info.find(client_stmt_id);

  if (m_prepared_stmt_info.end() == iterator) return nullptr;

  return &iterator->second;
}

Prepare_command_delegate::Notice_level
Prepare_command_handler::get_notice_level_flags(
    const Prepared_stmt_info *stmt_info) const {
  using Notice_level_flags = Prepare_command_delegate::Notice_level_flags;
  Prepare_command_delegate::Notice_level retval;

  if (stmt_info->m_type != Prepare::OneOfMessage::FIND)
    retval.set(Notice_level_flags::k_send_affected_rows);

  if (stmt_info->m_type == Prepare::OneOfMessage::INSERT ||
      stmt_info->m_type == Prepare::OneOfMessage::STMT) {
    retval.set(stmt_info->m_is_table_model
                   ? Notice_level_flags::k_send_generated_insert_id
                   : Notice_level_flags::k_send_generated_document_ids);
  }
  return retval;
}

void Prepare_command_handler::send_notices(
    const Prepared_stmt_info *stmt_info,
    const ngs::Resultset_interface::Info &info, const bool is_eof) const {
  const auto &notice_config = m_session->get_notice_configuration();
  if (info.num_warnings > 0 &&
      notice_config.is_notice_enabled(ngs::Notice_type::k_warning))
    notices::send_warnings(m_session->data_context(), m_session->proto());

  if (!is_eof) return;

  if (!info.message.empty())
    notices::send_message(m_session->proto(), info.message);

  if (stmt_info->m_type != Prepare::OneOfMessage::FIND)
    notices::send_rows_affected(m_session->proto(), info.affected_rows);

  if (stmt_info->m_type == Prepare::OneOfMessage::INSERT ||
      stmt_info->m_type == Prepare::OneOfMessage::STMT) {
    if (stmt_info->m_is_table_model) {
      if (info.last_insert_id > 0)
        notices::send_generated_insert_id(m_session->proto(),
                                          info.last_insert_id);
    } else
      notices::send_generated_document_ids(
          m_session->proto(),
          m_session->get_document_id_aggregator().get_ids());
  }
}

ngs::Error_code Prepare_command_handler::prepare_parameters(
    const Arg_list &args, const Placeholder_id_list &phs,
    Param_list *out_params, Param_value_list *out_param_values) {
  Param_list params_tmp;
  params_tmp.reserve(args.size());
  out_param_values->reserve(args.size());
  Any_to_param_handler handler(&params_tmp, out_param_values);
  for (const auto &arg : args) {
    try {
      ngs::Getter_any::put_scalar_value_to_functor(arg, handler);
    } catch (const ngs::Error_code &e) {
      return ngs::Error(ER_X_PREPARED_EXECUTE_ARGUMENT_NOT_SUPPORTED,
                        "Argument at index '%i' and of type '%s' is not "
                        "supported for binding to prepared statement",
                        static_cast<int>(params_tmp.size()),
                        arg.has_scalar() ? arg.scalar().GetTypeName().c_str()
                                         : arg.GetTypeName().c_str());
    }
  }

  out_params->reserve(phs.size());
  for (const auto placeholder : phs)
    out_params->push_back(params_tmp[placeholder]);

  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::check_argument_placeholder_consistency(
    const Arg_list::size_type args_size, const Placeholder_id_list &phs,
    const uint32_t args_offset) {
  for (auto ph : phs)
    if (ph >= static_cast<Placeholder_id_list::value_type>(args_size))
      return ngs::Error(ER_X_PREPARED_EXECUTE_ARGUMENT_CONSISTENCY,
                        "There is no argument for statement placeholder "
                        "at position: %" PRIu32,
                        (ph + args_offset));
  return ngs::Success();
}

// - Cursor ------------------

ngs::Error_code Prepare_command_handler::execute_cursor_open(const Open &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_cursor_open);
  DBUG_ASSERT(msg.stmt().type() == Open::OneOfMessage::PREPARE_EXECUTE);

  const auto cursor_id = msg.cursor_id();
  const auto client_statement_id = msg.stmt().prepare_execute().stmt_id();
  auto cursor_info = get_cursor_if_allocated(cursor_id);
  auto statement_info = get_stmt_if_allocated(client_statement_id);

  if (cursor_info) {
    m_cursors_info.erase(cursor_id);
    cursor_info = nullptr;
  }

  if (nullptr == statement_info)
    return ngs::Error(ER_X_BAD_STATEMENT_ID,
                      "Statement with ID=%" PRIu32 " was not prepared.",
                      client_statement_id);

  if (statement_info->m_has_cursor) {
    // Statement has a cursor already, lets close it and use a new cursor.
    m_cursors_info.erase(statement_info->m_cursor_id);
  }

  auto &prepare_execute = msg.stmt().prepare_execute();
  auto compact_metadata = prepare_execute.compact_metadata();
  const bool not_suspend_resultset = msg.fetch_rows() > 0;

  cursor_info = insert_cursor(cursor_id, client_statement_id, compact_metadata,
                              not_suspend_resultset);
  statement_info->m_has_cursor = true;
  statement_info->m_cursor_id = cursor_id;

  auto error = execute_execute_impl(prepare_execute, cursor_info->m_resultset,
                                    statement_info);

  send_notices(statement_info, cursor_info->m_resultset.get_info(),
               cursor_info->m_resultset.get_callbacks().got_eof());

  if (error) {
    m_cursors_info.erase(cursor_id);
    cursor_info = nullptr;
  }

  if (!error && not_suspend_resultset) {
    const bool no_more_data =
        cursor_info->m_resultset.get_callbacks().got_eof();

    if (!no_more_data)
      error =
          execute_cursor_fetch_impl(cursor_id, cursor_info, msg.fetch_rows());
  }

  if (!error) m_session->proto().send_exec_ok();

  return error;
}

ngs::Error_code Prepare_command_handler::execute_cursor_close(
    const Close &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_cursor_close);
  const auto cursor_id = msg.cursor_id();
  auto cursor_info = get_cursor_if_allocated(cursor_id);
  if (nullptr == cursor_info)
    return ngs::Error(ER_X_BAD_CURSOR_ID,
                      "Cursor with ID=%" PRIu32 " was not opened.", cursor_id);

  const auto client_statement_id = cursor_info->m_client_stmt_id;
  auto &prepared_stmt = m_prepared_stmt_info[client_statement_id];

  prepared_stmt.m_has_cursor = false;
  m_prepared_stmt_info[client_statement_id].m_cursor_id =
      std::numeric_limits<Id_type>::max();
  m_cursors_info.erase(cursor_id);
  m_session->proto().send_ok();
  return ngs::Success();
}

ngs::Error_code Prepare_command_handler::execute_cursor_fetch(
    const Fetch &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_cursor_fetch);
  const auto cursor_id = msg.cursor_id();
  const auto fetch_rows = msg.fetch_rows();
  auto cursor_info = get_cursor_if_allocated(cursor_id);
  if (nullptr == cursor_info)
    return ngs::Error(ER_X_BAD_CURSOR_ID,
                      "Cursor with ID=%" PRIu32 " was not opened.", cursor_id);

  const auto error =
      execute_cursor_fetch_impl(cursor_id, cursor_info, fetch_rows);

  if (!error) m_session->proto().send_exec_ok();

  return error;
}

ngs::Error_code Prepare_command_handler::execute_cursor_fetch_impl(
    const Id_type cursor_id, Cursor_info *cursor_info,
    const uint64 fetch_rows) {
  if (cursor_info->m_resultset.get_callbacks().got_eof())
    return ngs::Error(ER_X_CURSOR_REACHED_EOF,
                      "No more data in cursor (cursor id:%" PRIu32 ")",
                      cursor_id);

  const auto prep_stmt_info =
      get_stmt_if_allocated(cursor_info->m_client_stmt_id);
  DBUG_ASSERT(nullptr != prep_stmt_info);
  const auto server_stmt_id = prep_stmt_info->m_server_stmt_id;
  auto error = m_session->data_context().fetch_cursor(
      server_stmt_id, fetch_rows, &cursor_info->m_resultset);

  send_notices(prep_stmt_info, cursor_info->m_resultset.get_info(),
               cursor_info->m_resultset.get_callbacks().got_eof());

  return error;
}

Prepare_command_handler::Cursor_info *Prepare_command_handler::insert_cursor(
    const Id_type cursor_id, const Id_type client_statement_id,
    const bool compact_metadata, const bool ignore_fetch_suspended) {
  auto result = m_cursors_info.emplace(
      cursor_id, Cursor_info{client_statement_id,
                             Cursor_resultset{m_session, compact_metadata,
                                              ignore_fetch_suspended}});

  return &((*result.first).second);
}
}  // namespace xpl
