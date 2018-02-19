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

#include "plugin/x/src/notices.h"

#include <vector>

#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/xpl_resultset.h"

namespace xpl {

namespace notices {

namespace {

static xpl::Process_resultset::Row *start_warning_row(
    xpl::Process_resultset::Row *row_data) {
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

bool end_warning_row(xpl::Process_resultset::Row *row,
                     ngs::Protocol_encoder_interface &proto,
                     bool skip_single_error, std::string &last_error,
                     unsigned int &num_errors) {
  typedef Mysqlx::Notice::Warning Warning;

  if (!last_error.empty()) {
    proto.send_notice(ngs::Frame_type::WARNING, ngs::Frame_scope::LOCAL,
                      last_error);
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

  proto.send_notice(ngs::Frame_type::WARNING, ngs::Frame_scope::LOCAL, data);
  return true;
}

inline void send_local_notice(const Mysqlx::Notice::SessionStateChanged &notice,
                              ngs::Protocol_encoder_interface *proto) {
  std::string data;
  notice.SerializeToString(&data);
  proto->send_notice(ngs::Frame_type::SESSION_STATE_CHANGED,
                     ngs::Frame_scope::LOCAL, data);
}

}  // namespace

ngs::Error_code send_warnings(ngs::Sql_session_interface &da,
                              ngs::Protocol_encoder_interface &proto,
                              bool skip_single_error) {
  Process_resultset::Row row_data;
  static const std::string q = "SHOW WARNINGS";
  std::string last_error;
  unsigned int num_errors = 0u;
  Process_resultset resultset(
      ngs::bind(start_warning_row, &row_data),
      ngs::bind(end_warning_row, ngs::placeholders::_1, ngs::ref(proto),
                skip_single_error, last_error, num_errors));
  // send warnings as notices
  return da.execute(q.data(), q.length(), &resultset);
}

ngs::Error_code send_account_expired(ngs::Protocol_encoder_interface &proto) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_generated_insert_id(ngs::Protocol_encoder_interface &proto,
                                         uint64_t i) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID);
  Mysqlx::Datatypes::Scalar *v = change.mutable_value()->Add();
  v->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  v->set_v_unsigned_int(i);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_rows_affected(ngs::Protocol_encoder_interface &proto,
                                   uint64_t i) {
  proto.send_rows_affected(i);
  return ngs::Success();
}

ngs::Error_code send_client_id(ngs::Protocol_encoder_interface &proto,
                               uint64_t i) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED);
  Mysqlx::Datatypes::Scalar *v = change.mutable_value()->Add();
  v->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  v->set_v_unsigned_int(i);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_message(ngs::Protocol_encoder_interface &proto,
                             const std::string &message) {
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE);
  Mysqlx::Datatypes::Scalar *v = change.mutable_value()->Add();
  v->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
  v->mutable_v_string()->set_value(message);
  send_local_notice(change, &proto);
  return ngs::Success();
}

ngs::Error_code send_generated_document_ids(
    ngs::Protocol_encoder_interface &proto,
    const std::vector<std::string> &ids) {
  if (ids.empty()) return ngs::Success();

  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::GENERATED_DOCUMENT_IDS);
  for (const auto &id : ids) {
    Mysqlx::Datatypes::Scalar *v = change.mutable_value()->Add();
    v->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
    v->mutable_v_octets()->set_value(id);
  }

  send_local_notice(change, &proto);
  return ngs::Success();
}

}  // namespace notices
}  // namespace xpl
