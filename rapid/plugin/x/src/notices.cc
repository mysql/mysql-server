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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "notices.h"

#include <vector>

#include "callback_command_delegate.h"
#include "ngs_common/bind.h"
#include "ngs_common/protocol_protobuf.h"
#include "ngs/protocol_monitor.h"
#include "protocol.h"
#include "sql_data_context.h"

namespace xpl {

namespace notices {

namespace {

Callback_command_delegate::Row_data *start_warning_row(
    Callback_command_delegate::Row_data *row_data) {
  row_data->clear();
  return row_data;
}

inline Mysqlx::Notice::Warning::Level get_warning_level(
    const std::string &level) {
  static const char *const ERROR_STRING = "Error";
  static const char *const WARNING_STRING = "Warning";
  if (level == WARNING_STRING) return Mysqlx::Notice::Warning::WARNING;
  if (level == ERROR_STRING) return Mysqlx::Notice::Warning::ERROR;
  return Mysqlx::Notice::Warning::NOTE;
}

bool end_warning_row(Callback_command_delegate::Row_data *row,
                     ngs::Protocol_encoder &proto, bool skip_single_error,
                     std::string &last_error, unsigned int &num_errors) {
  typedef Mysqlx::Notice::Warning Warning;

  if (!last_error.empty()) {
    proto.send_local_warning(last_error);
    last_error.clear();
  }

  std::vector<Callback_command_delegate::Field_value *> &fields = row->fields;
  if (fields.size() != 3) return false;

  const Warning::Level level = get_warning_level(*fields[0]->value.v_string);

  Warning warning;
  warning.set_level(level);
  warning.set_code(
      static_cast<google::protobuf::uint32>(fields[1]->value.v_long));
  warning.set_msg(*fields[2]->value.v_string);

  std::string data;
  warning.SerializeToString(&data);

  if (level == Warning::ERROR) {
    ++num_errors;
    if (skip_single_error && (num_errors <= 1)) {
      last_error = data;
      return true;
    }
  }

  proto.send_local_warning(data);
  return true;
}

inline void send_local_notice(const Mysqlx::Notice::SessionStateChanged &notice,
                              ngs::Protocol_encoder *proto) {
  std::string data;
  notice.SerializeToString(&data);
  proto->send_local_notice(
      ngs::Protocol_encoder::k_notice_session_state_changed, data);
}
}  // namespace

ngs::Error_code send_warnings(Sql_data_context &da,
                              ngs::Protocol_encoder &proto,
                              bool skip_single_error) {
  Callback_command_delegate::Row_data row_data;
  Sql_data_context::Result_info winfo;
  static std::string q = "SHOW WARNINGS";
  std::string last_error;
  unsigned int num_errors = 0u;

  // send warnings as notices
  return da.execute_sql_and_process_results(
      q.data(), q.length(), ngs::bind(start_warning_row, &row_data),
      ngs::bind(end_warning_row, ngs::placeholders::_1, ngs::ref(proto),
                skip_single_error, last_error, num_errors),
      winfo);
}

ngs::Error_code send_account_expired(ngs::Protocol_encoder &proto) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_generated_insert_id(ngs::Protocol_encoder &proto,
                                         uint64_t i) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  change.mutable_value()->set_v_unsigned_int(i);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_rows_affected(ngs::Protocol_encoder &proto, uint64_t i) {
  proto.send_rows_affected(i);
  return ngs::Success();
}

ngs::Error_code send_client_id(ngs::Protocol_encoder &proto, uint64_t i) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  change.mutable_value()->set_v_unsigned_int(i);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_message(ngs::Protocol_encoder &proto,
                             const std::string &message) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
  change.mutable_value()->mutable_v_string()->set_value(message);
  send_local_notice(change, &proto);
  return ngs::Success();
}

}  // namespace notices
}  // namespace xpl
