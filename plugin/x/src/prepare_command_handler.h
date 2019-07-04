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

#ifndef PLUGIN_X_SRC_PREPARE_COMMAND_HANDLER_H_
#define PLUGIN_X_SRC_PREPARE_COMMAND_HANDLER_H_

#include <array>
#include <map>
#include <vector>

#include "mysql/com_data.h"
#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/xpl_resultset.h"

namespace xpl {

class Prepare_command_handler {
 public:
  using Prepare = Mysqlx::Prepare::Prepare;
  using Execute = Mysqlx::Prepare::Execute;
  using Deallocate = Mysqlx::Prepare::Deallocate;
  using Open = Mysqlx::Cursor::Open;
  using Close = Mysqlx::Cursor::Close;
  using Fetch = Mysqlx::Cursor::Fetch;
  using Message_type = Mysqlx::Prepare::Prepare::OneOfMessage::Type;
  using Any = Mysqlx::Datatypes::Any;
  using Arg_list = google::protobuf::RepeatedPtrField<Any>;
  using Param_list = std::vector<PS_PARAM>;
  using Placeholder_id_list = std::vector<uint32_t>;
  using Param_value_list = std::vector<std::array<unsigned char, 18>>;
  using Id_type = std::uint32_t;

  struct Prepared_stmt_info {
    Id_type m_server_stmt_id;
    Message_type m_type;
    Placeholder_id_list m_placeholder_ids;
    uint32_t m_args_offset;
    bool m_is_table_model;
    bool m_has_cursor;
    Id_type m_cursor_id;
  };
  using Prepared_stmt_info_list = std::map<Id_type, Prepared_stmt_info>;

  struct Cursor_info {
    Id_type m_client_stmt_id;
    Cursor_resultset m_resultset;
  };
  using Cursor_info_list = std::map<Id_type, Cursor_info>;

  explicit Prepare_command_handler(ngs::Session_interface *session)
      : m_session{session} {}

  ngs::Error_code execute_prepare(const Prepare &msg);
  ngs::Error_code execute_execute(const Execute &msg);
  ngs::Error_code execute_deallocate(const Deallocate &msg);
  ngs::Error_code execute_cursor_open(const Open &msg);
  ngs::Error_code execute_cursor_close(const Close &msg);
  ngs::Error_code execute_cursor_fetch(const Fetch &msg);

  const Prepared_stmt_info_list &get_prepared_stmt_info() const {
    return m_prepared_stmt_info;
  }

  Cursor_info *insert_cursor(const Id_type cursor_id,
                             const Id_type statement_id,
                             const bool compact_metadata,
                             const bool ignore_fetch_suspended);
  void insert_prepared_statement(const Id_type id,
                                 const Prepared_stmt_info &prep_stmt) {
    m_prepared_stmt_info.emplace(id, prep_stmt);
  }

  std::size_t cursors_count() const { return m_cursors_info.size(); }

  Prepared_stmt_info *get_stmt_if_allocated(const Id_type client_stmt_id);
  Cursor_info *get_cursor_if_allocated(const Id_type client_stmt_id);

 protected:
  ngs::Error_code build_query(const Prepare::OneOfMessage &msg,
                              std::vector<uint32_t> *ids,
                              uint32_t *args_offset);

  Prepare_command_delegate::Notice_level get_notice_level_flags(
      const Prepared_stmt_info *stmt_info) const;

  void send_notices(const Prepared_stmt_info *stmt_info,
                    const ngs::Resultset_interface::Info &info,
                    const bool is_eof) const;

  ngs::Error_code prepare_parameters(const Arg_list &args,
                                     const Placeholder_id_list &phs,
                                     Param_list *params,
                                     Param_value_list *param_values);
  ngs::Error_code check_argument_placeholder_consistency(
      const Arg_list::size_type args_size, const Placeholder_id_list &phs,
      const uint32_t args_offset);

  ngs::Error_code execute_execute_impl(const Execute &msg,
                                       ngs::Resultset_interface &rset,
                                       Prepared_stmt_info *prep_stmt_info);
  ngs::Error_code execute_deallocate_impl(const Id_type client_stmt_id,
                                          const Prepared_stmt_info *stmt_info);
  ngs::Error_code execute_cursor_fetch_impl(const Id_type cursor_id,
                                            Cursor_info *cursor_info,
                                            const uint64 fetch_rows);

 private:
  ngs::Session_interface *m_session;
  Query_string_builder m_qb{1024};
  Prepared_stmt_info_list m_prepared_stmt_info;
  Cursor_info_list m_cursors_info;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_PREPARE_COMMAND_HANDLER_H_
