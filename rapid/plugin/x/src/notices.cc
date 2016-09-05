/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "notices.h"
#include "callback_command_delegate.h"
#include "sql_data_context.h"
#include "protocol.h"
#include "ngs_common/protocol_protobuf.h"
#include "ngs/protocol_monitor.h"
#include "ngs_common/bind.h"

#include <vector>

static xpl::Callback_command_delegate::Row_data *start_warning_row(xpl::Callback_command_delegate::Row_data *row_data)
{
  row_data->clear();
  return row_data;
}


static bool end_warning_row(xpl::Callback_command_delegate::Row_data *row, ngs::Protocol_encoder &proto,
  bool skip_single_error, std::string& last_error, unsigned int& num_errors)
{
  static const char * const ERROR_STRING = "Error";
  static const char * const WARNING_STRING = "Warning";

  Mysqlx::Notice::Warning warning;
  ngs::Protocol_monitor_interface &protocol_monitor = proto.get_protocol_monitor();

  if (!last_error.empty())
  {
    proto.send_local_notice(1, last_error);
    last_error.clear();
  }

  if (row->fields[0]->value.v_string->compare(ERROR_STRING) == 0)
  {
    warning.set_level(Mysqlx::Notice::Warning::ERROR);
    ++num_errors;
  }
  else if (row->fields[0]->value.v_string->compare(WARNING_STRING) == 0)
  {
    warning.set_level(Mysqlx::Notice::Warning::WARNING);
    protocol_monitor.on_notice_warning_send();
  }
  else
  {
    warning.set_level(Mysqlx::Notice::Warning::NOTE);
    protocol_monitor.on_notice_other_send();
  }

  warning.set_code(static_cast<google::protobuf::uint32>(row->fields[1]->value.v_long));
  warning.set_msg(*row->fields[2]->value.v_string);

  std::string data;
  warning.SerializeToString(&data);

  if (skip_single_error && (row->fields[0]->value.v_string->compare(ERROR_STRING) == 0) && (num_errors <= 1))
    last_error = data;
  else
    proto.send_local_notice(1, data);
  return true;
}


ngs::Error_code xpl::notices::send_warnings(Sql_data_context &da, ngs::Protocol_encoder &proto,
  bool skip_single_error)
{
  Callback_command_delegate::Row_data row_data;
  Sql_data_context::Result_info winfo;
  static std::string q = "SHOW WARNINGS";
  std::string last_error;
  unsigned int num_errors = 0u;

  // send warnings as notices
  return da.execute_sql_and_process_results(q.data(), q.length(),
              ngs::bind(start_warning_row, &row_data),
              ngs::bind(end_warning_row, ngs::placeholders::_1, ngs::ref(proto), skip_single_error, last_error, num_errors),
              winfo);
}


ngs::Error_code xpl::notices::send_account_expired(ngs::Protocol_encoder &proto)
{
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED);

  std::string data;
  change.SerializeToString(&data);

  proto.send_local_notice(3, data, true);
  return ngs::Success();
}


ngs::Error_code xpl::notices::send_generated_insert_id(ngs::Protocol_encoder &proto, uint64_t i)
{
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  change.mutable_value()->set_v_unsigned_int(i);

  std::string data;
  change.SerializeToString(&data);

  proto.send_local_notice(3, data);
  return ngs::Success();
}

ngs::Error_code xpl::notices::send_rows_affected(ngs::Protocol_encoder &proto, uint64_t i)
{
  proto.send_rows_affected(i);

  return ngs::Success();
}


ngs::Error_code xpl::notices::send_client_id(ngs::Protocol_encoder &proto, uint64_t i)
{
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
  change.mutable_value()->set_v_unsigned_int(i);

  std::string data;
  change.SerializeToString(&data);

  proto.send_local_notice(3, data);
  return ngs::Success();
}

ngs::Error_code xpl::notices::send_message(ngs::Protocol_encoder &proto, const std::string &message)
{
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE);
  change.mutable_value()->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
  change.mutable_value()->mutable_v_string()->set_value(message);

  std::string data;
  change.SerializeToString(&data);

  proto.send_local_notice(3, data);
  return ngs::Success();
}
