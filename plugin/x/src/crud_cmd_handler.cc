/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/crud_cmd_handler.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/delete_statement_builder.h"
#include "plugin/x/src/expr_generator.h"
#include "plugin/x/src/find_statement_builder.h"
#include "plugin/x/src/insert_statement_builder.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/update_statement_builder.h"
#include "plugin/x/src/view_statement_builder.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_resultset.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {

template <typename B, typename M>
ngs::Error_code Crud_command_handler::execute(
    Session &session, const B &builder, const M &msg,
    ngs::Resultset_interface &resultset, Status_variable variable,
    bool (ngs::Protocol_encoder_interface::*send_ok)()) {
  session.update_status(variable);
  m_qb.clear();
  try {
    builder.build(msg);
  } catch (const Expression_generator::Error &exc) {
    return ngs::Error(exc.error(), "%s", exc.what());
  } catch (const ngs::Error_code &error) {
    return error;
  }
  log_debug("CRUD query: %s", m_qb.get().c_str());
  ngs::Error_code error = session.data_context().execute(
      m_qb.get().data(), m_qb.get().length(), &resultset);
  if (error) return error_handling(error, msg);
  notice_handling(session, resultset.get_info(), builder, msg);
  (session.proto().*send_ok)();
  return ngs::Success();
}

template <typename B, typename M>
void Crud_command_handler::notice_handling(
    Session &session, const ngs::Resultset_interface::Info &info,
    const B & /*builder*/, const M & /*msg*/) const {
  notice_handling_common(session, info);
}

void Crud_command_handler::notice_handling_common(
    Session &session, const ngs::Resultset_interface::Info &info) const {
  const auto &notice_config = session.get_notice_configuration();
  if (info.num_warnings > 0 &&
      notice_config.is_notice_enabled(ngs::Notice_type::k_warning))
    notices::send_warnings(session.data_context(), session.proto());

  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
}

namespace {
inline bool check_message(const std::string &msg, const char *pattern,
                          std::string::size_type *pos) {
  return (*pos = msg.find(pattern)) != std::string::npos;
}
}  // namespace

// -- Insert
ngs::Error_code Crud_command_handler::execute_crud_insert(
    Session &session, const Mysqlx::Crud::Insert &msg) {
  const auto &server = session.client().server();
  Insert_statement_builder::Document_id_list id_list;
  Insert_statement_builder::Document_id_aggregator id_agg(
      &server.get_document_id_generator(), &id_list);
  ngs::Error_code error = id_agg.configue(&session.data_context());
  if (error) return error;

  Expression_generator gen(&m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  Empty_resultset rset;
  return execute(session, Insert_statement_builder(gen, &id_agg), msg, rset,
                 &ngs::Common_status_variables::m_crud_insert,
                 &ngs::Protocol_encoder_interface::send_exec_ok);
}

template <>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Insert &msg) const {
  if (is_table_data_model(msg)) return error;

  switch (error.error) {
    case ER_BAD_NULL_ERROR:
      return ngs::Error(ER_X_DOC_ID_MISSING,
                        "Document is missing a required field");

    case ER_BAD_FIELD_ERROR:
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                        "Table '%s' is not a document collection",
                        msg.collection().name().c_str());

    case ER_DUP_ENTRY:
      return ngs::Error(
          ER_X_DOC_ID_DUPLICATE,
          "Document contains a field value that is not unique but "
          "required to be");

    case ER_X_BAD_UPSERT_DATA:
      return ngs::Error(ER_X_BAD_UPSERT_DATA,
                        "Unable upsert data in document collection '%s'",
                        msg.collection().name().c_str());
  }
  return error;
}

template <>
void Crud_command_handler::notice_handling(
    Session &session, const ngs::Resultset_interface::Info &info,
    const Insert_statement_builder &builder,
    const Mysqlx::Crud::Insert &msg) const {
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
  if (is_table_data_model(msg))
    notices::send_generated_insert_id(session.proto(), info.last_insert_id);
  else
    notices::send_generated_document_ids(session.proto(),
                                         builder.get_document_ids());
}

// -- Update
ngs::Error_code Crud_command_handler::execute_crud_update(
    Session &session, const Mysqlx::Crud::Update &msg) {
  Expression_generator gen(&m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  Empty_resultset rset;
  return execute(session, Update_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_update,
                 &ngs::Protocol_encoder_interface::send_exec_ok);
}

template <>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Update &msg) const {
  if (is_table_data_model(msg)) return error;

  switch (error.error) {
    case ER_BAD_NULL_ERROR:
      return ngs::Error(ER_X_DOC_ID_MISSING,
                        "Document is missing a required field");

    case ER_INVALID_JSON_TEXT_IN_PARAM:
      return ngs::Error(ER_X_BAD_UPDATE_DATA,
                        "Invalid data for update operation on "
                        "document collection table");
  }
  return error;
}

template <>
void Crud_command_handler::notice_handling(
    Session &session, const ngs::Resultset_interface::Info &info,
    const Update_statement_builder & /*builder*/,
    const Mysqlx::Crud::Update & /*msg*/) const {
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
}

// -- Delete
ngs::Error_code Crud_command_handler::execute_crud_delete(
    Session &session, const Mysqlx::Crud::Delete &msg) {
  Expression_generator gen(&m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  Empty_resultset rset;
  return execute(session, Delete_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_delete,
                 &ngs::Protocol_encoder_interface::send_exec_ok);
}

template <>
void Crud_command_handler::notice_handling(
    Session &session, const ngs::Resultset_interface::Info &info,
    const Delete_statement_builder & /*builder*/,
    const Mysqlx::Crud::Delete & /*msg*/) const {
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
}

// -- Find
ngs::Error_code Crud_command_handler::execute_crud_find(
    Session &session, const Mysqlx::Crud::Find &msg) {
  Expression_generator gen(&m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  Streaming_resultset rset(&session.proto(), &session.get_notice_output_queue(),
                           false);
  return execute(session, Find_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_find,
                 &ngs::Protocol_encoder_interface::send_exec_ok);
}

template <>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Find &msg) const {
  if (is_table_data_model(msg)) return error;

  switch (error.error) {
    case ER_BAD_FIELD_ERROR:
      std::string::size_type pos = std::string::npos;
      if (check_message(error.message, "having clause", &pos))
        return ngs::Error(ER_X_EXPR_BAD_VALUE,
                          "Invalid expression in grouping criteria");

      if (check_message(error.message, "where clause", &pos))
        return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                          "%sselection criteria",
                          error.message.substr(0, pos - 1).c_str());

      if (check_message(error.message, "field list", &pos))
        return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING, "%scollection",
                          error.message.substr(0, pos - 1).c_str());
  }
  return error;
}

// -- View
ngs::Error_code Crud_command_handler::execute_create_view(
    Session &session, const Mysqlx::Crud::CreateView &msg) {
  Expression_generator gen(&m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  Empty_resultset rset;
  return execute(session, View_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_create_view,
                 &ngs::Protocol_encoder_interface::send_ok);
}

ngs::Error_code Crud_command_handler::execute_modify_view(
    Session &session, const Mysqlx::Crud::ModifyView &msg) {
  Expression_generator gen(&m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  Empty_resultset rset;
  return execute(session, View_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_modify_view,
                 &ngs::Protocol_encoder_interface::send_ok);
}

ngs::Error_code Crud_command_handler::execute_drop_view(
    Session &session, const Mysqlx::Crud::DropView &msg) {
  Expression_generator gen(&m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  Empty_resultset rset;
  return execute(session, View_statement_builder(gen), msg, rset,
                 &ngs::Common_status_variables::m_crud_drop_view,
                 &ngs::Protocol_encoder_interface::send_ok);
}

}  // namespace xpl
