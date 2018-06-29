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

#include "plugin/x/src/xpl_dispatcher.h"

#include "plugin/x/ngs/include/ngs/interface/notice_output_queue_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/admin_cmd_handler.h"
#include "plugin/x/src/crud_cmd_handler.h"
#include "plugin/x/src/expect/expect_stack.h"
#include "plugin/x/src/expr_generator.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_resultset.h"
#include "plugin/x/src/xpl_server.h"
#include "plugin/x/src/xpl_session.h"

namespace {

class Stmt {
 public:
  ngs::Error_code execute(
      ngs::Sql_session_interface &da, ngs::Protocol_encoder_interface &proto,
      ngs::Notice_output_queue_interface *notice_queue,
      const bool show_warnings, const bool compact_metadata,
      const std::string &query,
      const ::google::protobuf::RepeatedPtrField<::Mysqlx::Datatypes::Any>
          &args) {
    const int args_size = args.size();

    if (0 == args_size)
      return execute(da, proto, notice_queue, show_warnings, compact_metadata,
                     query.data(), query.length());

    m_qb.clear();
    m_qb.put(query);

    try {
      for (int i = 0; i < args_size; ++i) {
        ngs::Getter_any::put_scalar_value_to_functor(args.Get(i), *this);
      }
    } catch (const ngs::Error_code &error) {
      return error;
    }

    return execute(da, proto, notice_queue, show_warnings, compact_metadata,
                   m_qb.get().data(), m_qb.get().length());
  }

  ngs::Error_code execute(ngs::Sql_session_interface &da,
                          ngs::Protocol_encoder_interface &proto,
                          ngs::Notice_output_queue_interface *notice_queue,
                          const bool show_warnings, const bool compact_metadata,
                          const char *query, std::size_t query_len) {
    xpl::Streaming_resultset resultset(&proto, notice_queue, compact_metadata);
    ngs::Error_code error = da.execute(query, query_len, &resultset);
    if (!error) {
      const xpl::Streaming_resultset::Info &info = resultset.get_info();
      if (info.num_warnings > 0 && show_warnings)
        xpl::notices::send_warnings(da, proto);
      xpl::notices::send_rows_affected(proto, info.affected_rows);
      if (info.last_insert_id > 0)
        xpl::notices::send_generated_insert_id(proto, info.last_insert_id);
      if (!info.message.empty())
        xpl::notices::send_message(proto, info.message);
      proto.send_exec_ok();
    } else {
      if (show_warnings) xpl::notices::send_warnings(da, proto, true);
    }

    return error;
  }

  void operator()() {
    static const char *value_null = "NULL";

    m_qb.format() % xpl::Query_formatter::No_escape<const char *>(value_null);
  }

  template <typename Value_type>
  void operator()(const Value_type &value) {
    m_qb.format() % value;
  }

 private:
  xpl::Query_string_builder m_qb;
};

ngs::Error_code on_stmt_execute(xpl::Session &session,
                                const Mysqlx::Sql::StmtExecute &msg) {
  log_debug("%s: %s", session.client().client_id(), msg.stmt().c_str());
  auto &notice_config = session.get_notice_configuration();

  if (msg.namespace_() == "sql" || !msg.has_namespace_()) {
    session.update_status<&ngs::Common_status_variables::m_stmt_execute_sql>();
    return Stmt().execute(
        session.data_context(), session.proto(),
        &session.get_notice_output_queue(),
        notice_config.is_notice_enabled(ngs::Notice_type::k_warning),
        msg.compact_metadata(), msg.stmt(), msg.args());
  }

  if (msg.namespace_() == "xplugin") {
    session
        .update_status<&ngs::Common_status_variables::m_stmt_execute_xplugin>();
    if (notice_config.is_notice_enabled(
            ngs::Notice_type::k_xplugin_deprecation)) {
      xpl::notices::send_message(
          session.proto(),
          "Namespace 'xplugin' is deprecated, please use 'mysqlx' instead");
      notice_config.set_notice(ngs::Notice_type::k_xplugin_deprecation, false);
    }
    xpl::Admin_command_arguments_list args(msg.args());
    return xpl::Admin_command_handler(&session).execute(msg.namespace_(),
                                                        msg.stmt(), &args);
  }

  if (msg.namespace_() == xpl::Admin_command_handler::MYSQLX_NAMESPACE) {
    session
        .update_status<&ngs::Common_status_variables::m_stmt_execute_mysqlx>();
    xpl::Admin_command_arguments_object args(msg.args());
    return xpl::Admin_command_handler(&session).execute(msg.namespace_(),
                                                        msg.stmt(), &args);
  }

  return ngs::Error(ER_X_INVALID_NAMESPACE, "Unknown namespace %s",
                    msg.namespace_().c_str());
}

ngs::Error_code on_expect_open(xpl::Session &session,
                               xpl::Expectation_stack &expect,
                               const Mysqlx::Expect::Open &msg) {
  session.update_status<&ngs::Common_status_variables::m_expect_open>();

  ngs::Error_code error = expect.open(msg);
  if (!error) session.proto().send_ok();
  return error;
}

ngs::Error_code on_expect_close(xpl::Session &session,
                                xpl::Expectation_stack &expect,
                                const Mysqlx::Expect::Close &) {
  session.update_status<&ngs::Common_status_variables::m_expect_close>();

  ngs::Error_code error = expect.close();
  if (!error) session.proto().send_ok();
  return error;
}

ngs::Error_code do_dispatch_command(xpl::Session &session,
                                    xpl::Crud_command_handler &crudh,
                                    xpl::Expectation_stack &expect,
                                    ngs::Message_request &command) {
  switch (command.get_message_type()) {
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
      return on_stmt_execute(session,
                             static_cast<const Mysqlx::Sql::StmtExecute &>(
                                 *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_FIND:
      return crudh.execute_crud_find(
          session,
          static_cast<const Mysqlx::Crud::Find &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_INSERT:
      return crudh.execute_crud_insert(
          session,
          static_cast<const Mysqlx::Crud::Insert &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_UPDATE:
      return crudh.execute_crud_update(
          session,
          static_cast<const Mysqlx::Crud::Update &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_DELETE:
      return crudh.execute_crud_delete(
          session,
          static_cast<const Mysqlx::Crud::Delete &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_CREATE_VIEW:
      return crudh.execute_create_view(
          session, static_cast<const Mysqlx::Crud::CreateView &>(
                       *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_MODIFY_VIEW:
      return crudh.execute_modify_view(
          session, static_cast<const Mysqlx::Crud::ModifyView &>(
                       *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_DROP_VIEW:
      return crudh.execute_drop_view(
          session,
          static_cast<const Mysqlx::Crud::DropView &>(*command.get_message()));

    case Mysqlx::ClientMessages::EXPECT_OPEN:
      return on_expect_open(
          session, expect,
          static_cast<const Mysqlx::Expect::Open &>(*command.get_message()));

    case Mysqlx::ClientMessages::EXPECT_CLOSE:
      return on_expect_close(
          session, expect,
          static_cast<const Mysqlx::Expect::Close &>(*command.get_message()));
  }

  session.proto().get_protocol_monitor().on_error_unknown_msg_type();
  return ngs::Error(ER_UNKNOWN_COM_ERROR, "Unexpected message received");
}

}  // namespace

bool xpl::dispatcher::dispatch_command(Session &session,
                                       Crud_command_handler &crudh,
                                       Expectation_stack &expect,
                                       ngs::Message_request &command) {
  ngs::Error_code error = expect.pre_client_stmt(command.get_message_type());
  if (!error) {
    error = do_dispatch_command(session, crudh, expect, command);
    if (error) session.proto().send_result(error);
    expect.post_client_stmt(command.get_message_type(), error);
  } else
    session.proto().send_result(error);
  return error.error != ER_UNKNOWN_COM_ERROR;
}
