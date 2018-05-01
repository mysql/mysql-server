/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/tests/driver/processor/commands/command.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "plugin/x/ngs/include/ngs_common/to_string.h"
#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"
#include "plugin/x/tests/driver/formatters/message_formatter.h"
#include "plugin/x/tests/driver/json_to_any_handler.h"
#include "plugin/x/tests/driver/processor/commands/mysqlxtest_error_names.h"
#include "plugin/x/tests/driver/processor/comment_processor.h"
#include "plugin/x/tests/driver/processor/indigestion_processor.h"
#include "plugin/x/tests/driver/processor/macro_block_processor.h"
#include "plugin/x/tests/driver/processor/stream_processor.h"

namespace {

const char *const CMD_ARG_BE_QUIET = "be-quiet";
const char *const CMD_ARG_SHOW_RECEIVED = "show-received";
const char CMD_ARG_SEPARATOR = '\t';
const std::string CMD_PREFIX = "-->";

std::string bindump_to_data(const std::string &bindump,
                            const Script_stack *stack, const Console &console) {
  std::string res;
  for (size_t i = 0; i < bindump.length(); i++) {
    if (bindump[i] == '\\') {
      if (bindump[i + 1] == '\\') {
        res.push_back('\\');
        ++i;
      } else if (bindump[i + 1] == 'x') {
        int value = 0;
        const char *hex = aux::ALLOWED_HEX_CHARACTERS.c_str();
        const char *p = strchr(hex, bindump[i + 2]);
        if (p) {
          value = static_cast<int>(p - hex) << 4;
        } else {
          console.print_error(*stack, "Invalid bindump char at ", i + 2, '\n');
          break;
        }
        p = strchr(hex, bindump[i + 3]);
        if (p) {
          value |= p - hex;
        } else {
          console.print_error(*stack, "Invalid bindump char at ", i + 3, '\n');
          break;
        }
        i += 3;
        res.push_back(value);
      }
    } else {
      res.push_back(bindump[i]);
    }
  }
  return res;
}

template <typename T>
class Backup_and_restore {
 public:
  Backup_and_restore(T *variable, const T &temporaru_value)
      : m_variable(variable), m_value(*variable) {
    *m_variable = temporaru_value;
  }

  ~Backup_and_restore() { *m_variable = m_value; }

 private:
  T *m_variable;
  T m_value;
};

}  // namespace

ngs::chrono::time_point Command::m_start_measure;

Command::Command() {
  m_commands["title"] = &Command::cmd_title;
  m_commands["echo"] = &Command::cmd_echo;
  m_commands["recvtype"] = &Command::cmd_recvtype;
  m_commands["recvok"] = &Command::cmd_recvok;
  m_commands["recverror"] = &Command::cmd_recverror;
  m_commands["recvresult"] = &Command::cmd_recvresult;
  m_commands["recvtovar"] = &Command::cmd_recvtovar;
  m_commands["recvuntil"] = &Command::cmd_recvuntil;
  m_commands["recvuntildisc"] = &Command::cmd_recv_all_until_disc;
  m_commands["do_ssl_handshake"] = &Command::cmd_do_ssl_handshake;
  m_commands["sleep"] = &Command::cmd_sleep;
  m_commands["login"] = &Command::cmd_login;
  m_commands["stmtadmin"] = &Command::cmd_stmtadmin;
  m_commands["stmtsql"] = &Command::cmd_stmtsql;
  m_commands["loginerror"] = &Command::cmd_loginerror;
  m_commands["repeat"] = &Command::cmd_repeat;
  m_commands["endrepeat"] = &Command::cmd_endrepeat;
  m_commands["system"] = &Command::cmd_system;
  m_commands["peerdisc"] = &Command::cmd_peerdisc;
  m_commands["recv"] = &Command::cmd_recv;
  m_commands["exit"] = &Command::cmd_exit;
  m_commands["abort"] = &Command::cmd_abort;
  m_commands["nowarnings"] = &Command::cmd_nowarnings;
  m_commands["yeswarnings"] = &Command::cmd_yeswarnings;
  m_commands["fatalerrors"] = &Command::cmd_fatalerrors;
  m_commands["nofatalerrors"] = &Command::cmd_nofatalerrors;
  m_commands["newsession"] = &Command::cmd_newsession;
  m_commands["newsession_plain"] = &Command::cmd_newsession_plain;
  m_commands["newsession_mysql41"] = &Command::cmd_newsession_mysql41;
  m_commands["newsession_memory"] = &Command::cmd_newsession_memory;
  m_commands["setsession"] = &Command::cmd_setsession;
  m_commands["closesession"] = &Command::cmd_closesession;
  m_commands["expecterror"] = &Command::cmd_expecterror;
  m_commands["measure"] = &Command::cmd_measure;
  m_commands["endmeasure"] = &Command::cmd_endmeasure;
  m_commands["quiet"] = &Command::cmd_quiet;
  m_commands["noquiet"] = &Command::cmd_noquiet;
  m_commands["varfile"] = &Command::cmd_varfile;
  m_commands["varlet"] = &Command::cmd_varlet;
  m_commands["varinc"] = &Command::cmd_varinc;
  m_commands["varsub"] = &Command::cmd_varsub;
  m_commands["vargen"] = &Command::cmd_vargen;
  m_commands["varescape"] = &Command::cmd_varescape;
  m_commands["binsend"] = &Command::cmd_binsend;
  m_commands["hexsend"] = &Command::cmd_hexsend;
  m_commands["binsendoffset"] = &Command::cmd_binsendoffset;
  m_commands["callmacro"] = &Command::cmd_callmacro;
  m_commands["macro_delimiter_compress"] =
      &Command::cmd_macro_delimiter_compress;
  m_commands["import"] = &Command::cmd_import;
  m_commands["assert_eq"] = &Command::cmd_assert_eq;
  m_commands["assert_ne"] = &Command::cmd_assert_ne;
  m_commands["assert_gt"] = &Command::cmd_assert_gt;
  m_commands["assert_ge"] = &Command::cmd_assert_ge;
  m_commands["query_result"] = &Command::cmd_query;
  m_commands["noquery_result"] = &Command::cmd_noquery;
  m_commands["wait_for"] = &Command::cmd_wait_for;
  m_commands["received"] = &Command::cmd_received;
}

bool Command::is_command_registred(const std::string &command_line,
                                   std::string *out_command_name,
                                   bool *out_is_single_line_command) const {
  auto command_name_start = command_line.begin();
  const bool has_prefix = 0 == strncmp(command_line.c_str(), CMD_PREFIX.c_str(),
                                       CMD_PREFIX.length());

  if (out_is_single_line_command) *out_is_single_line_command = has_prefix;

  if (has_prefix) command_name_start += CMD_PREFIX.length();

  const auto command_name_end = std::find_if(
      command_line.begin(), command_line.end(), [](const char element) -> bool {
        return element == ' ' || element == ';';
      });

  std::string command_name(command_name_start, command_name_end);

  if (out_command_name) *out_command_name = command_name;

  return m_commands.count(command_name) > 0;
}

Command::Result Command::process(std::istream &input,
                                 Execution_context *context,
                                 const std::string &command_line) {
  std::string out_command_name;
  bool out_has_prefix;

  if (!is_command_registred(command_line, &out_command_name, &out_has_prefix)) {
    context->print_error("Unknown command_line \"", command_line, "\"\n");
    return Result::Stop_with_failure;
  }

  context->print_verbose("Execute ", command_line, "\n");

  context->m_command_name = out_command_name;

  const char *arguments = command_line.c_str() + out_command_name.length();

  if (out_has_prefix) arguments += CMD_PREFIX.length();

  if (' ' == *arguments) arguments++;

  return (this->*m_commands[out_command_name])(input, context, arguments);
}

Command::Result Command::cmd_echo(std::istream &input,
                                  Execution_context *context,
                                  const std::string &args) {
  std::string s = args;
  context->m_variables->replace(&s);
  context->print(s, "\n");

  return Result::Continue;
}

Command::Result Command::cmd_title(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  if (!args.empty()) {
    context->print("\n", args.substr(1), "\n");
    std::string sep(args.length() - 1, args[0]);
    context->print(sep, "\n");
  } else {
    context->print("\n\n");
  }

  return Result::Continue;
}

Command::Result Command::cmd_recvtype(std::istream &input,
                                      Execution_context *context,
                                      const std::string &args) {
  std::vector<std::string> vargs;
  aux::split(vargs, args, " ", true);

  if (1 != vargs.size() && 2 != vargs.size() && 3 != vargs.size()) {
    std::stringstream error_message;
    error_message << "Received wrong number of arguments, got:" << vargs.size();
    throw std::logic_error(error_message.str());
  }

  bool be_quiet = false;
  xcl::XProtocol::Server_message_type_id msgid;
  xcl::XError error;
  Message_ptr msg(
      context->session()->get_protocol().recv_single_message(&msgid, &error));

  int number_of_arguments = vargs.size() - 1;
  if (1 < vargs.size()) {
    if (vargs[number_of_arguments] == CMD_ARG_BE_QUIET) {
      be_quiet = true;
      --number_of_arguments;
    }
  }

  if (nullptr == msg.get())
    return context->m_options.m_fatal_errors ? Result::Stop_with_failure
                                             : Result::Continue;

  try {
    const std::string expected_message_name = vargs[0];
    const std::string field_filter = number_of_arguments > 0 ? vargs[1] : "";
    const std::string expected_field_value =
        number_of_arguments > 1 ? vargs[2] : "";
    bool is_ok = msg->GetDescriptor()->full_name() == expected_message_name;

    if (!expected_field_value.empty()) {
      const bool k_dont_show_message_name = false;
      const std::string field_value =
          context->m_variables->unreplace(formatter::message_to_text(
              *msg, field_filter, k_dont_show_message_name));

      if (field_value != expected_field_value) {
        is_ok = false;
      }
    }

    if (!is_ok) {
      const std::string message_in_text = formatter::message_to_text(*msg);
      std::string expected_message = expected_message_name;

      if (!field_filter.empty()) expected_message += "(" + field_filter + ")";
      if (!expected_field_value.empty())
        expected_message += " = " + expected_field_value;

      context->m_variables->clear_unreplace();

      context->print("Received unexpected message. Was expecting:\n    ",
                     expected_message, "\nbut got:\n");
      context->print(message_in_text, "\n");

      return context->m_options.m_fatal_errors ? Result::Stop_with_failure
                                               : Result::Continue;
    }

    if (context->m_options.m_show_query_result && !be_quiet) {
      const std::string message_in_text = context->m_variables->unreplace(
          formatter::message_to_text(*msg, field_filter));
      context->print(message_in_text, "\n");
    }

    context->m_variables->clear_unreplace();
  } catch (std::exception &e) {
    context->print_error_red(context->m_script_stack, e, '\n');
    if (context->m_options.m_fatal_errors) return Result::Stop_with_success;
  }

  return Result::Continue;
}

Command::Result Command::cmd_recvok(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  xcl::XError error;
  xcl::XProtocol::Server_message_type_id out_msgid;

  Message_ptr msg{context->session()->get_protocol().recv_single_message(
      &out_msgid, &error)};

  context->print("RUN recvok\n");

  if (error) {
    context->m_console.print_error(error);

    return context->m_options.m_fatal_errors ? Result::Stop_with_failure
                                             : Result::Continue;
  }

  if (nullptr == msg.get()) {
    context->print("Command recvok didn't receive any data.\n");
    return Result::Stop_with_failure;
  }

  if (Mysqlx::ServerMessages::OK != out_msgid) {
    if (Mysqlx::ServerMessages::ERROR != out_msgid) {
      context->print("Got unexpected message:\n");
      context->print(formatter::message_to_text(*msg), "\n");

      return context->m_options.m_fatal_errors ? Result::Stop_with_failure
                                               : Result::Continue;
    }

    auto msg_error = static_cast<Mysqlx::Error *>(msg.get());

    if (!context->m_expected_error.check_error(
            xcl::XError(msg_error->code(), msg_error->msg())))
      return Result::Stop_with_failure;
  } else {
    if (!context->m_expected_error.check_ok()) return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_recverror(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  xcl::XProtocol::Server_message_type_id msgid;
  xcl::XError xerror;
  Message_ptr msg(
      context->session()->get_protocol().recv_single_message(&msgid, &xerror));

  if (args.empty()) {
    context->print_error(
        "'recverror' command, requires an integer argument.\n");
    return Result::Stop_with_failure;
  }

  if (msg.get()) {
    bool failed = false;
    try {
      const int expected_error_code = mysqlxtest::get_error_code_by_text(args);
      if (msg->GetDescriptor()->full_name() != "Mysqlx.Error" ||
          expected_error_code !=
              static_cast<int>(
                  static_cast<Mysqlx::Error *>(msg.get())->code())) {
        context->print_error(context->m_script_stack, "Was expecting Error ",
                             args, ", but got:\n");
        failed = true;
      } else {
        context->print("Got expected error:\n");
      }

      context->print(*msg, "\n");

      if (failed && context->m_options.m_fatal_errors) {
        return Result::Stop_with_success;
      }
    } catch (std::exception &e) {
      context->print_error_red(context->m_script_stack, e, '\n');
      if (context->m_options.m_fatal_errors) return Result::Stop_with_success;
    }
  }

  return Result::Continue;
}

Command::Result Command::cmd_recvtovar(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  if (args.empty()) {
    context->print_error("'recvtovar' command, requires an argument.\n");
    return Result::Stop_with_failure;
  }

  std::string args_cmd = args;
  std::vector<std::string> args_array;
  aux::trim(args_cmd);

  aux::split(args_array, args_cmd, " ", false);

  args_cmd = CMD_ARG_BE_QUIET;

  if (args_array.size() > 1) {
    args_cmd += " ";
    args_cmd += args_array.at(1);
  }

  cmd_recvresult(input, context, args_cmd,
                 std::bind(&Variable_container::set, context->m_variables,
                           args_array.at(0), std::placeholders::_1));

  return Result::Continue;
}

Command::Result Command::cmd_recvresult(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  return cmd_recvresult(input, context, args, Value_callback());
}

Command::Result Command::cmd_recvresult(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args,
                                        Value_callback value_callback) {
  try {
    std::vector<std::string> columns;
    std::string cmd_args = args;

    aux::trim(cmd_args);

    if (cmd_args.size()) aux::split(columns, cmd_args, " ", false);

    std::vector<std::string>::iterator i =
        std::find(columns.begin(), columns.end(), "print-columnsinfo");
    const bool print_colinfo = i != columns.end();
    if (print_colinfo) columns.erase(i);

    i = std::find(columns.begin(), columns.end(), CMD_ARG_BE_QUIET);
    const bool quiet = i != columns.end();
    if (quiet) columns.erase(i);

    Result_fetcher result{context->session()->get_protocol().recv_resultset()};

    const bool force_quiet = !context->m_options.m_show_query_result || quiet;
    print_resultset(context, &result, columns, value_callback, force_quiet);

    auto error = result.get_last_error();

    if (error) {
      if (!context->m_expected_error.check_error(error)) {
        return Result::Stop_with_failure;
      }

      return Result::Continue;
    }

    if (print_colinfo) context->print(result.column_metadata());

    context->m_variables->clear_unreplace();
    if (!force_quiet) {
      int64_t x = result.affected_rows();
      if (x >= 0)
        context->print(x, " rows affected\n");
      else
        context->print("command ok\n");
      if (result.last_insert_id() > 0)
        context->print("last insert id: ", result.last_insert_id(), "\n");

      std::vector<std::string> document_ids = result.generated_document_ids();
      if (!document_ids.empty()) {
        context->print("auto-generated id(s): ");
        std::vector<std::string>::const_iterator i = document_ids.begin();
        context->print(*i++);
        for (; i != document_ids.end(); ++i) context->print(",", *i);
        context->print("\n");
      }

      if (!result.info_message().empty())
        context->print(result.info_message(), "\n");
      {
        std::vector<Result_fetcher::Warning> warnings(result.get_warnings());
        if (!warnings.empty()) context->print("Warnings generated:\n");
        for (auto w = warnings.begin(); w != warnings.end(); ++w) {
          context->print((w->m_is_note ? "NOTE" : "WARNING"), " | ", w->m_code,
                         " | ", w->m_text, "\n");
        }
      }
    }

    if (!context->m_expected_error.check_ok()) return Result::Stop_with_failure;
  } catch (xcl::XError &) {
  }
  return Result::Continue;
}

Command::Result Command::cmd_recvuntil(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  if (args.empty()) {
    context->print_error(
        "'recvuntil' command, requires at last one argument.\n");
    return Result::Stop_with_failure;
  }

  xcl::XProtocol::Server_message_type_id msgid;
  std::vector<std::string> argl;

  aux::split(argl, args, " ", true);

  bool show = true, stop = false;

  if (argl.size() > 1) {
    const char *argument_do_not_print = argl[1].c_str();
    show = false;

    if (0 != strcmp(argument_do_not_print, "do_not_show_intermediate")) {
      context->print_error("Invalid argument received: ", argl[1], '\n');
      return Result::Stop_with_failure;
    }
  }

  Message_by_full_name::iterator iterator_msg_name =
      server_msgs_by_full_name.find(argl[0]);

  if (server_msgs_by_full_name.end() == iterator_msg_name) {
    context->print_error("Unknown message name: ", argl[0], " ",
                         server_msgs_by_full_name.size(), '\n');
    return Result::Stop_with_failure;
  }

  Message_server_by_name::iterator iterator_msg_id =
      server_msgs_by_name.find(iterator_msg_name->second);

  if (server_msgs_by_name.end() == iterator_msg_id) {
    context->print_error(
        "Invalid data in internal message list, entry not found:",
        iterator_msg_name->second, '\n');
    return Result::Stop_with_failure;
  }

  const xcl::XProtocol::Server_message_type_id expected_msg_id{
      iterator_msg_id->second.second};

  do {
    xcl::XError error;
    Message_ptr msg(
        context->session()->get_protocol().recv_single_message(&msgid, &error));

    if (msg.get()) {
      if (msg->GetDescriptor()->full_name() == argl[0] ||
          msgid == Mysqlx::ServerMessages::ERROR) {
        show = true;
        stop = true;
      }

      try {
        if (show) context->print(*msg, "\n");
      } catch (std::exception &e) {
        context->print_error_red(context->m_script_stack, e, '\n');
        if (context->m_options.m_fatal_errors) return Result::Stop_with_success;
      }
    }
  } while (!stop);

  context->m_variables->clear_unreplace();

  if (Mysqlx::ServerMessages::ERROR == msgid &&
      Mysqlx::ServerMessages::ERROR != expected_msg_id)
    return Result::Stop_with_failure;

  return Result::Continue;
}

Command::Result Command::cmd_do_ssl_handshake(std::istream &input,
                                              Execution_context *context,
                                              const std::string &args) {
  xcl::XError error =
      context->session()->get_protocol().get_connection().activate_tls();
  if (error) {
    context->print_error_red(context->m_script_stack, error, '\n');
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_stmtsql(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  if (args.empty()) {
    context->print_error("'stmtsql' command, requires a string argument.\n");
    return Result::Stop_with_failure;
  }

  Mysqlx::Sql::StmtExecute stmt;

  std::string command = args;
  context->m_variables->replace(&command);

  stmt.set_stmt(command);
  stmt.set_namespace_("sql");

  context->session()->get_protocol().send(stmt);

  if (!context->m_options.m_quiet) context->print("RUN ", command, "\n");

  return Result::Continue;
}

Command::Result Command::cmd_stmtadmin(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  if (args.empty()) {
    context->print_error(
        "'stmtadmin' command, requires at last one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string tmp = args;
  context->m_variables->replace(&tmp);
  std::vector<std::string> params;
  aux::split(params, tmp, "\t", true);
  if (params.empty()) {
    context->print_error("Invalid empty admin command", '\n');
    return Result::Stop_with_failure;
  }

  aux::trim(params[0]);

  Mysqlx::Sql::StmtExecute stmt;
  stmt.set_stmt(params[0]);
  stmt.set_namespace_("mysqlx");

  if (params.size() == 2) {
    Any obj;
    if (!json_string_to_any(params[1], &obj)) {
      context->print_error("Invalid argument for '", params[0],
                           "' command; json object expected\n");
      return Result::Stop_with_failure;
    }
    stmt.add_args()->CopyFrom(obj);
  }

  context->session()->get_protocol().send(stmt);

  return Result::Continue;
}

Command::Result Command::cmd_sleep(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  if (args.empty()) {
    context->print_error("'sleep' command, requires an integer argument.\n");
    return Result::Stop_with_failure;
  }

  std::string tmp = args;
  context->m_variables->replace(&tmp);
  const double delay_in_seconds = ngs::stod(tmp);
#ifdef _WIN32
  const int delay_in_milliseconds = static_cast<int>(delay_in_seconds * 1000);
  Sleep(delay_in_milliseconds);
#else
  const int delay_in_microseconds =
      static_cast<int>(delay_in_seconds * 1000000);
  usleep(delay_in_microseconds);
#endif
  return Result::Continue;
}

Command::Result Command::cmd_login(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  std::string user, pass, db, auth_meth = "MYSQL41";

  if (args.empty()) {
    context->m_connection->get_credentials(&user, &pass);
  } else {
    std::string s = args;
    context->m_variables->replace(&s);

    std::string::size_type p = s.find(CMD_ARG_SEPARATOR);
    if (p != std::string::npos) {
      user = s.substr(0, p);
      s = s.substr(p + 1);
      p = s.find(CMD_ARG_SEPARATOR);
      if (p != std::string::npos) {
        pass = s.substr(0, p);
        s = s.substr(p + 1);
        p = s.find(CMD_ARG_SEPARATOR);
        if (p != std::string::npos) {
          db = s.substr(0, p);
          auth_meth = s.substr(p + 1);
        } else {
          db = s;
        }
      } else {
        pass = s;
      }
    } else {
      user = s;
    }
  }

  auto protocol = context->m_connection->active_xprotocol();

  for (auto &c : auth_meth) c = toupper(c);

  auto error = protocol->execute_authenticate(user, pass, db, auth_meth);

  context->m_connection->active_holder().remove_notice_handler();

  if (error) {
    if (CR_X_UNSUPPORTED_OPTION_VALUE == error.error()) {
      context->print_error("Wrong authentication method", '\n');
      return Result::Stop_with_failure;
    }

    if (!context->m_expected_error.check_error(error)) {
      return Result::Stop_with_failure;
    }

    return Result::Continue;
  }

  context->m_connection->setup_variables(
      context->m_connection->active_xsession());

  context->print("Login OK\n");
  return Result::Continue;
}

Command::Result Command::cmd_repeat(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  if (args.empty()) {
    context->print_error("'repeat' command, requires at last one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string variable_name = "";
  std::vector<std::string> argl;

  aux::split(argl, args, "\t", true);

  if (argl.size() > 1) {
    variable_name = argl[1];
  }

  // Allow use of variables as a source of number of iterations
  context->m_variables->replace(&argl[0]);

  Loop_do loop = {input.tellg(), ngs::stoi(argl[0]), 0, variable_name};

  m_loop_stack.push_back(loop);

  if (variable_name.length())
    context->m_variables->set(variable_name, ngs::to_string(loop.value));

  return Result::Continue;
}

Command::Result Command::cmd_endrepeat(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  while (m_loop_stack.size()) {
    Loop_do &ld = m_loop_stack.back();

    --ld.iterations;
    ++ld.value;

    if (ld.variable_name.length())
      context->m_variables->set(ld.variable_name, ngs::to_string(ld.value));

    if (1 > ld.iterations) {
      m_loop_stack.pop_back();
      break;
    }

    input.seekg(ld.block_begin);
    break;
  }

  return Result::Continue;
}

Command::Result Command::cmd_loginerror(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  std::string s = args;
  std::string expected, user, pass, db;
  int expected_error_code = 0;

  context->m_variables->replace(&s);
  std::string::size_type p = s.find('\t');
  if (p != std::string::npos) {
    expected = s.substr(0, p);
    s = s.substr(p + 1);
    p = s.find('\t');
    if (p != std::string::npos) {
      user = s.substr(0, p);
      s = s.substr(p + 1);
      p = s.find('\t');
      if (p != std::string::npos) {
        pass = s.substr(0, p + 1);
        db = s.substr(p + 1);
      } else {
        pass = s;
      }
    } else {
      user = s;
    }
  } else {
    context->print_error(context->m_script_stack,
                         "Missing arguments to -->loginerror\n");
    return Result::Stop_with_failure;
  }
  try {
    context->m_variables->replace(&expected);
    aux::trim(expected);
    auto protocol = context->m_connection->active_xprotocol();
    expected_error_code = mysqlxtest::get_error_code_by_text(expected);
    auto err = protocol->execute_authenticate(user, pass, db, "MYSQL41");
    context->m_connection->active_holder().remove_notice_handler();
    if (err) {
      if (err.error() == expected_error_code) {
        context->print("error (as expected): ", err, "\n");
      } else {
        context->print_error(context->m_script_stack,
                             "was expecting: ", expected_error_code,
                             " but got: ", err, '\n');
        if (context->m_options.m_fatal_errors) return Result::Stop_with_failure;
      }
      return Result::Continue;
    }
    context->print_error(context->m_script_stack,
                         "Login succeeded, but an error was expected\n");
    if (context->m_options.m_fatal_errors) return Result::Stop_with_failure;
  } catch (const std::exception &e) {
    context->print_error(e, '\n');
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_system(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  if (args.empty()) {
    context->print_error("'system' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  // command used only at dev level
  // example of usage
  // -->system (sleep 3; echo "Killing"; ps aux | grep mysqld | egrep -v "gdb
  // .+mysqld" | grep -v  "kdeinit4"| awk '{print($2)}' | xargs kill -s
  // SIGQUIT)&
  if (0 == system(args.c_str())) return Result::Continue;

  return Result::Stop_with_failure;
}

Command::Result Command::cmd_recv_all_until_disc(std::istream &input,
                                                 Execution_context *context,
                                                 const std::string &args) {
  xcl::XProtocol::Server_message_type_id msgid;
  xcl::XError error;
  bool show_all_received_messages = false;

  if (!args.empty()) {
    std::string copy_arg = args;
    aux::trim(copy_arg);

    if (copy_arg != CMD_ARG_SHOW_RECEIVED) {
      context->print_error(
          "'recvuntildisc' command, accepts zero or one argument. "
          "Acceptable value for the argument is \"",
          CMD_ARG_SHOW_RECEIVED, "\"\n");
      return Result::Stop_with_failure;
    }

    show_all_received_messages = true;
  }

  try {
    while (true) {
      Message_ptr msg{
          context->m_connection->active_xprotocol()->recv_single_message(
              &msgid, &error)};

      if (error) throw error;

      if (msg.get() && show_all_received_messages)
        context->print(context->m_variables->unreplace(
                           formatter::message_to_text(*msg), true),
                       "\n");
    }
  } catch (xcl::XError &) {
    context->print_error("Server disconnected", '\n');
  }

  /* Ensure that connection is closed. This is going to stop XSession from
   executing disconnection flow */
  context->m_connection->active_xconnection()->close();

  if (context->m_connection->is_default_active()) {
    return Result::Stop_with_success;
  }

  context->m_connection->close_active(false);

  return Result::Continue;
}

Command::Result Command::cmd_peerdisc(std::istream &input,
                                      Execution_context *context,
                                      const std::string &args) {
  int expected_delta_time;
  int tolerance;
  int result = sscanf(args.c_str(), "%i %i", &expected_delta_time, &tolerance);

  if (result < 1 || result > 2) {
    context->print_error("ERROR: Invalid use of command", '\n');

    return Result::Stop_with_failure;
  }

  if (1 == result) {
    tolerance = 10 * expected_delta_time / 100;
  }

  ngs::chrono::time_point start_time = ngs::chrono::now();
  try {
    xcl::XProtocol::Server_message_type_id msgid;
    context->m_connection->active_xconnection()->set_read_timeout(
        2 * expected_delta_time);

    xcl::XError err;
    Message_ptr msg(
        context->m_connection->active_xprotocol()->recv_single_message(&msgid,
                                                                       &err));
    if (err) throw err;

    if (msg.get()) {
      context->print_error("ERROR: Received unexpected message.\n", *msg, '\n');
    } else {
      context->print_error(
          "ERROR: Timeout occur while waiting for disconnection.\n");
    }

    return Result::Stop_with_failure;
  } catch (const xcl::XError &ec) {
    if (CR_SERVER_GONE_ERROR != ec.error()) {
      /** Peer disconnected, connector shouldn't try
      to execute closure flow. Lets close it. */
      context->m_connection->active_xconnection()->close();
      context->m_console.print_error_red(context->m_script_stack, ec, '\n');
      return Result::Stop_with_failure;
    }
  }

  int execution_delta_time = static_cast<int>(
      ngs::chrono::to_milliseconds(ngs::chrono::now() - start_time));

  if (abs(execution_delta_time - expected_delta_time) > tolerance) {
    context->print_error(
        "ERROR: Peer disconnected after: ", execution_delta_time,
        "[ms], expected: ", expected_delta_time, "[ms]\n");
    return Result::Stop_with_failure;
  }

  context->m_connection->active_xconnection()->close();

  if (context->m_connection->is_default_active()) {
    return Result::Stop_with_success;
  }

  context->m_connection->close_active(false);

  return Result::Continue;
}

Command::Result Command::cmd_recv(std::istream &input,
                                  Execution_context *context,
                                  const std::string &args) {
  xcl::XProtocol::Server_message_type_id msgid;
  bool quiet = false;
  std::string args_copy(args);

  aux::trim(args_copy);

  if ("quiet" == args_copy) {
    quiet = true;
    args_copy = "";
  }

  try {
    xcl::XError error;

    Message_ptr msg{
        context->m_connection->active_xprotocol()->recv_single_message(&msgid,
                                                                       &error)};

    if (error) {
      if (!quiet && !context->m_expected_error.check_error(
                        error))  // TODO(owner) do we need
                                 // this !quiet ?
        return Result::Stop_with_failure;
      return Result::Continue;
    }

    if (msg.get() && (context->m_options.m_show_query_result && !quiet))
      context->print(context->m_variables->unreplace(
                         formatter::message_to_text(*msg, args_copy), true),
                     "\n");

    if (!context->m_expected_error.check_ok()) return Result::Stop_with_failure;
  } catch (std::exception &e) {
    context->print_error("ERROR: ", e, '\n');

    if (context->m_options.m_fatal_errors) return Result::Stop_with_failure;
  }
  return Result::Continue;
}

Command::Result Command::cmd_exit(std::istream &input,
                                  Execution_context *context,
                                  const std::string &args) {
  return Result::Stop_with_success;
}

Command::Result Command::cmd_abort(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  exit(2);
  return Result::Stop_with_success;
}

Command::Result Command::cmd_nowarnings(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  context->m_options.m_show_warnings = false;
  return Result::Continue;
}

Command::Result Command::cmd_yeswarnings(std::istream &input,
                                         Execution_context *context,
                                         const std::string &args) {
  context->m_options.m_show_warnings = true;
  return Result::Continue;
}

Command::Result Command::cmd_fatalerrors(std::istream &input,
                                         Execution_context *context,
                                         const std::string &args) {
  context->m_options.m_fatal_errors = true;
  return Result::Continue;
}

Command::Result Command::cmd_nofatalerrors(std::istream &input,
                                           Execution_context *context,
                                           const std::string &args) {
  context->m_options.m_fatal_errors = false;
  return Result::Continue;
}

Command::Result Command::cmd_newsession_memory(std::istream &input,
                                               Execution_context *context,
                                               const std::string &args) {
  return do_newsession(input, context, args, {"SHA256_MEMORY"});
}

Command::Result Command::cmd_newsession_mysql41(std::istream &input,
                                                Execution_context *context,
                                                const std::string &args) {
  return do_newsession(input, context, args, {"MYSQL41"});
}

Command::Result Command::cmd_newsession_plain(std::istream &input,
                                              Execution_context *context,
                                              const std::string &args) {
  return do_newsession(input, context, args, {"PLAIN"});
}

Command::Result Command::cmd_newsession(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  return do_newsession(input, context, args, {});
}

Command::Result Command::do_newsession(
    std::istream &input, Execution_context *context, const std::string &args,
    const std::vector<std::string> &auth_methods) {
  if (args.empty()) {
    context->print_error(
        "'newsession' command, requires at "
        "last one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string s = args;
  std::string user, pass, db, name;

  context->m_variables->replace(&s);

  std::string::size_type p = s.find(CMD_ARG_SEPARATOR);

  if (p != std::string::npos) {
    name = s.substr(0, p);
    s = s.substr(p + 1);
    p = s.find(CMD_ARG_SEPARATOR);
    if (p != std::string::npos) {
      user = s.substr(0, p);
      s = s.substr(p + 1);
      p = s.find(CMD_ARG_SEPARATOR);
      if (p != std::string::npos) {
        pass = s.substr(0, p);
        db = s.substr(p + 1);
      } else {
        pass = s;
      }
    } else {
      user = s;
    }
  } else {
    name = s;
  }

  try {
    context->m_connection->create(name, user, pass, db, auth_methods);
    if (!context->m_expected_error.check_ok()) return Result::Stop_with_failure;
  } catch (xcl::XError &err) {
    if (!context->m_expected_error.check_error(err)) {
      return Result::Stop_with_failure;
    }
  }

  return Result::Continue;
}

Command::Result Command::cmd_setsession(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  std::string s = args;

  context->m_variables->replace(&s);

  if (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
    context->m_connection->set_active(s.substr(1));
  else
    context->m_connection->set_active(s);
  return Result::Continue;
}

Command::Result Command::cmd_closesession(std::istream &input,
                                          Execution_context *context,
                                          const std::string &args) {
  try {
    if (args == "abort")
      context->m_connection->abort_active();
    else
      context->m_connection->close_active();

    if (!context->m_expected_error.check_ok()) {
      return Result::Stop_with_failure;
    }
  } catch (xcl::XError &err) {
    if (!context->m_expected_error.check_error(err)) {
      return Result::Stop_with_failure;
    }
  }
  return Result::Continue;
}

Command::Result Command::cmd_expecterror(std::istream &input,
                                         Execution_context *context,
                                         const std::string &args) {
  if (args.empty()) {
    context->print_error("'expecterror' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  try {
    std::vector<std::string> argl;

    aux::split(argl, args, ",", true);

    for (std::vector<std::string>::const_iterator arg = argl.begin();
         arg != argl.end(); ++arg) {
      std::string value = *arg;

      context->m_variables->replace(&value);
      aux::trim(value);

      const int error_code = mysqlxtest::get_error_code_by_text(value);

      context->m_expected_error.expect_errno(error_code);
    }
  } catch (const std::exception &e) {
    context->print_error(e, '\n');

    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_measure(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  m_start_measure = ngs::chrono::now();
  return Result::Continue;
}

Command::Result Command::cmd_endmeasure(std::istream &input,
                                        Execution_context *context,
                                        const std::string &args) {
  if (!ngs::chrono::is_valid(m_start_measure)) {
    context->print_error("Time measurement, wasn't initialized", '\n');
    return Result::Stop_with_failure;
  }

  std::vector<std::string> argl;
  aux::split(argl, args, " ", true);
  if (argl.size() != 2 && argl.size() != 1) {
    context->print_error(
        "Invalid number of arguments for command endmeasure\n");
    return Result::Stop_with_failure;
  }

  const int64_t expected_msec = ngs::stoi(argl[0]);
  const int64_t msec =
      ngs::chrono::to_milliseconds(ngs::chrono::now() - m_start_measure);

  int64_t tolerance = expected_msec * 10 / 100;

  if (2 == argl.size()) tolerance = ngs::stoi(argl[1]);

  if (abs(static_cast<int>(expected_msec - msec)) > tolerance) {
    context->print_error("Timeout should occur after ", expected_msec,
                         "ms, but it was ", msec, "ms.  \n");
    return Result::Stop_with_failure;
  }

  m_start_measure = ngs::chrono::time_point();
  return Result::Continue;
}

Command::Result Command::cmd_quiet(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  context->m_options.m_quiet = true;

  return Result::Continue;
}

Command::Result Command::cmd_noquiet(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  context->m_options.m_quiet = false;

  return Result::Continue;
}

Command::Result Command::cmd_varsub(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  if (args.empty()) {
    context->print_error("'varsub' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->push_unreplace(args);
  return Result::Continue;
}

Command::Result Command::cmd_varlet(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  if (args.empty()) {
    context->print_error("'varlet' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string::size_type p = args.find(' ');

  if (p == std::string::npos) {
    context->m_variables->set(args, "");
  } else {
    const std::string name = args.substr(0, p);
    std::string value = args.substr(p + 1);

    context->m_variables->replace(&value);

    if (!context->m_variables->set(name, value)) {
      context->print_error("'varlet' command failed, when setting the '", name,
                           "' variable.\n");

      return Result::Stop_with_failure;
    }
  }
  return Result::Continue;
}

Command::Result Command::cmd_varinc(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  std::vector<std::string> argl;
  aux::split(argl, args, " ", true);
  if (argl.size() != 2) {
    context->print_error("Invalid number of arguments for command varinc\n");
    return Result::Stop_with_failure;
  }

  if (!context->m_variables->is_present(argl[0])) {
    context->print_error("Invalid variable ", argl[0], '\n');
    return Result::Stop_with_failure;
  }

  std::string val = context->m_variables->get(argl[0]);
  char *c;
  std::string inc_by = argl[1].c_str();

  context->m_variables->replace(&inc_by);

  int64_t int_val = strtol(val.c_str(), &c, 10);
  int64_t int_n = strtol(inc_by.c_str(), &c, 10);
  int_val += int_n;
  val = ngs::to_string(int_val);
  context->m_variables->set(argl[0], val);

  return Result::Continue;
}

Command::Result Command::cmd_vargen(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  std::vector<std::string> argl;
  aux::split(argl, args, " ", true);
  if (argl.size() != 3) {
    context->print_error("Invalid number of arguments for command vargen\n");
    return Result::Stop_with_failure;
  }
  std::string data(ngs::stoi(argl[2]), *argl[1].c_str());
  context->m_variables->set(argl[0], data);
  return Result::Continue;
}

Command::Result Command::cmd_varfile(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  std::vector<std::string> argl;
  aux::split(argl, args, " ", true);
  if (argl.size() != 2) {
    context->print_error("Invalid number of arguments for command varfile ",
                         args, '\n');
    return Result::Stop_with_failure;
  }

  std::string path_to_file = argl[1];
  context->m_variables->replace(&path_to_file);

  std::ifstream file(path_to_file.c_str());
  if (!file.is_open()) {
    context->print_error("Couldn't not open file ", path_to_file, '\n');
    return Result::Stop_with_failure;
  }

  file.seekg(0, file.end);
  size_t len = file.tellg();
  file.seekg(0);

  char *buffer = new char[len];
  file.read(buffer, len);
  context->m_variables->set(argl[0], std::string(buffer, len));
  delete[] buffer;

  return Result::Continue;
}

Command::Result Command::cmd_varescape(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  if (args.empty()) {
    context->print_error("'varescape' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  if (!context->m_variables->is_present(args)) {
    context->print_error("'varescape' command,",
                         "argument needs to be a variable.\n");
    return Result::Stop_with_failure;
  }

  std::string variable_value = context->m_variables->get(args);

  aux::replace_all(variable_value, "\"", "\\\"");
  aux::replace_all(variable_value, "\n", "\\n");

  context->m_variables->set(args, variable_value);

  return Result::Continue;
}

Command::Result Command::cmd_binsend(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  if (args.empty()) {
    context->print_error("'binsend' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string args_copy = args;
  context->m_variables->replace(&args_copy);
  std::string data =
      bindump_to_data(args_copy, &context->m_script_stack, context->m_console);

  context->print("Sending ", data.length(), " bytes raw data...\n");
  context->m_connection->active_xconnection()->write(
      reinterpret_cast<const uint8_t *>(data.c_str()), data.length());

  return Result::Continue;
}

Command::Result Command::cmd_hexsend(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  std::string args_copy = args;
  context->m_variables->replace(&args_copy);

  if (0 == args_copy.length()) {
    context->print_error("Data should not be present", '\n');
    return Result::Stop_with_failure;
  }

  if (0 != args_copy.length() % 2) {
    context->print_error(
        "Size of data should be a multiplication of two, current length:",
        args_copy.length(), '\n');
    return Result::Stop_with_failure;
  }

  std::string data;
  try {
    aux::unhex(args_copy, data);
  } catch (const std::exception &) {
    context->print_error("Hex string is invalid", '\n');
    return Result::Stop_with_failure;
  }

  context->print("Sending ", data.length(), " bytes raw data...\n");
  context->m_connection->active_xconnection()->write(
      reinterpret_cast<const uint8_t *>(data.c_str()), data.length());

  return Result::Continue;
}

size_t Command::value_to_offset(const std::string &data,
                                const size_t maximum_value) {
  if ('%' == *data.rbegin()) {
    size_t percent = ngs::stoi(data);

    return maximum_value * percent / 100;
  }

  return ngs::stoi(data);
}

Command::Result Command::cmd_binsendoffset(std::istream &input,
                                           Execution_context *context,
                                           const std::string &args) {
  if (args.empty()) {
    context->print_error(
        "'binsendoffset' command, requires at last one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string args_copy = args;
  context->m_variables->replace(&args_copy);

  std::vector<std::string> argl;
  aux::split(argl, args_copy, " ", true);

  size_t begin_bin = 0;
  size_t end_bin = 0;
  std::string data;

  try {
    data =
        bindump_to_data(argl[0], &context->m_script_stack, context->m_console);
    end_bin = data.length();

    if (argl.size() > 1) {
      begin_bin = value_to_offset(argl[1], data.length());
      if (argl.size() > 2) {
        end_bin = value_to_offset(argl[2], data.length());

        if (argl.size() > 3) throw std::out_of_range("Too many arguments");
      }
    }
  } catch (const std::out_of_range &) {
    context->print_error(
        "Invalid number of arguments for command binsendoffset:", argl.size(),
        '\n');
    return Result::Stop_with_failure;
  }

  context->print("Sending ", end_bin, " bytes raw data...\n");
  data = data.substr(begin_bin, end_bin - begin_bin);

  context->m_connection->active_xconnection()->write(
      reinterpret_cast<const uint8_t *>(data.c_str()), data.length());

  return Result::Continue;
}

Command::Result Command::cmd_callmacro(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  if (args.empty()) {
    context->print_error(
        "'callmacro' command, requires at last one argument.\n");
    return Result::Stop_with_failure;
  }

  if (context->m_macros.call(context, args)) return Result::Continue;

  return Result::Stop_with_failure;
}

Command::Result Command::cmd_macro_delimiter_compress(
    std::istream &input, Execution_context *context, const std::string &args) {
  if (args.empty()) {
    context->print_error(
        "'macro_delimiter_compress' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string copy_args = args;
  aux::trim(copy_args);

  std::map<std::string, bool> allowed_values{
      {"true", true}, {"false", false}, {"0", false}, {"1", true}};

  if (0 == allowed_values.count(copy_args)) {
    context->print_error(
        "'macro_delimiter_compress' received unknown argument value '",
        copy_args, "'.\n");
    return Result::Stop_with_failure;
  }

  context->m_macros.set_compress_option(allowed_values[copy_args]);

  return Result::Continue;
}

Command::Result Command::cmd_assert_eq(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  std::vector<std::string> vargs;

  aux::split(vargs, args, "\t", true);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command assert_eq:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->replace(&vargs[0]);
  context->m_variables->replace(&vargs[1]);

  if (vargs[0] != vargs[1]) {
    context->print_error("Execution of '", args, "', resulted in an error:\n");
    context->print_error("Expecting '", vargs[0], "', but received '", vargs[1],
                         "'\n");
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_assert_ne(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  std::vector<std::string> vargs;

  aux::split(vargs, args, "\t", true);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command assert_eq:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->replace(&vargs[0]);
  context->m_variables->replace(&vargs[1]);

  if (vargs[0] == vargs[1]) {
    context->print_error("Expecting '", vargs[0], "', to be different from '",
                         vargs[1], "'\n");
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_assert_gt(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  std::vector<std::string> vargs;

  aux::split(vargs, args, "\t", true);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command assert_gt:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->replace(&vargs[0]);
  context->m_variables->replace(&vargs[1]);

  if (ngs::stoi(vargs[0]) <= ngs::stoi(vargs[1])) {
    context->print_error("Expecting '", vargs[0], "' to be greater than '",
                         vargs[1], "'\n");
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_assert_ge(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args) {
  std::vector<std::string> vargs;
  char *end_string = nullptr;

  aux::split(vargs, args, "\t", true);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command assert_gt:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->replace(&vargs[0]);
  context->m_variables->replace(&vargs[1]);

  if (strtoll(vargs[0].c_str(), &end_string, 10) <
      strtoll(vargs[1].c_str(), &end_string, 10)) {
    context->print_error("assert_gt(", args, ") failed!\n", "Expecting '",
                         vargs[0], "' to be greater or equal to '", vargs[1],
                         "'\n");
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_query(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args) {
  context->m_options.m_show_query_result = true;
  return Result::Continue;
}

Command::Result Command::cmd_noquery(std::istream &input,
                                     Execution_context *context,
                                     const std::string &args) {
  context->m_options.m_show_query_result = false;
  return Result::Continue;
}

bool Command::put_variable_to(std::string *result, const std::string &value) {
  *result = value;

  return true;
}

void Command::try_result(Result result) {
  if (result != Result::Continue) throw result;
}

Command::Result Command::cmd_wait_for(std::istream &input,
                                      Execution_context *context,
                                      const std::string &args) {
  bool match = false;
  const int countdown_start_value = 30;
  int countdown_retries = countdown_start_value;

  std::string args_variables_replaced = args;
  std::vector<std::string> vargs;

  context->m_variables->replace(&args_variables_replaced);
  aux::split(vargs, args_variables_replaced, "\t", true);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command wait_for:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  const std::string &expected_value = vargs[0];
  std::string value;

  try {
    do {
      Backup_and_restore<bool> backup_and_restore_fatal_errors(
          &context->m_options.m_fatal_errors, true);
      Backup_and_restore<bool> backup_and_restore_query(
          &context->m_options.m_show_query_result, false);
      Backup_and_restore<std::string> backup_and_restore_command_name(
          &context->m_command_name, "sql");

      try_result(cmd_stmtsql(input, context, vargs[1]));
      try_result(cmd_recvresult(
          input, context, "",
          std::bind(&Command::put_variable_to, &value, std::placeholders::_1)));
      try_result(cmd_sleep(input, context, "1"));

      match = (value == expected_value);
    } while (!match && --countdown_retries);
  } catch (const Result result) {
    context->print_error(
        "'Wait_for' failed because one of subsequent commands failed\n");
    return result;
  }

  if (!match) {
    context->print_error("Query didn't return expected value, tried ",
                         countdown_start_value, " times\n", "Expected '",
                         expected_value, "', received '", value, "'\n");
    return Result::Stop_with_failure;
  }

  return Result::Continue;
}

Command::Result Command::cmd_received(std::istream &input,
                                      Execution_context *context,
                                      const std::string &args) {
  std::string cargs(args);
  std::vector<std::string> vargs;
  aux::split(vargs, cargs, " \t", true);
  context->m_variables->replace(&vargs[0]);

  if (2 != vargs.size()) {
    context->print_error(
        "Specified invalid number of arguments for command received:",
        vargs.size(), " expecting 2\n");
    return Result::Stop_with_failure;
  }

  context->m_variables->set(
      vargs[1],
      ngs::to_string(
          context->m_connection->active_session_messages_received(vargs[0])));

  return Result::Continue;
}

bool Command::json_string_to_any(const std::string &json_string,
                                 Any *any) const {
  Json_to_any_handler handler(any);
  rapidjson::Reader reader;
  rapidjson::StringStream ss(json_string.c_str());
  return !reader.Parse(ss, handler).IsError();
}

static bool try_open_file_on_different_paths(
    std::ifstream &stream, const std::string &filename,
    const std::vector<std::string> &paths) {
  for (const auto &path : paths) {
    stream.open(path + filename);

    // Lets access the file to make the "fs.flags" contain
    // valid values.
    stream.peek();

    if (stream.good()) return true;
  }

  return stream.good();
}

Command::Result Command::cmd_import(std::istream &input,
                                    Execution_context *context,
                                    const std::string &args) {
  if (args.empty()) {
    context->print_error("'import' command, requires one argument.\n");
    return Result::Stop_with_failure;
  }

  std::string filename(args);
  context->m_variables->replace(&filename);

  std::ifstream stream;

  try_open_file_on_different_paths(stream, filename,
                                   {context->m_options.m_import_path, ""});

  // After the "peek", good can be checked
  if (!stream.good()) {
    context->print_error(context->m_script_stack, "Could not open macro file ",
                         args, " (aka ", filename, ")\n");
    return Result::Stop_with_failure;
  }

  context->m_script_stack.push({0, args});

  std::vector<Block_processor_ptr> processors{
      std::make_shared<Macro_block_processor>(context),
      std::make_shared<Comment_processor>(),
      std::make_shared<Indigestion_processor>(context)};

  bool r = process_client_input(stream, &processors, &context->m_script_stack,
                                context->m_console) == 0;
  context->m_script_stack.pop();

  return r ? Result::Continue : Result::Stop_with_failure;
}

void Command::print_resultset(Execution_context *context,
                              Result_fetcher *result,
                              const std::vector<std::string> &columns,
                              Value_callback value_callback, const bool quiet) {
  std::vector<xcl::Column_metadata> meta(result->column_metadata());
  std::vector<int> column_indexes;
  int column_index = -1;
  bool first = true;

  if (result->get_last_error()) return;

  for (auto col = meta.begin(); col != meta.end(); ++col) {
    ++column_index;

    if (!first) {
      if (!quiet) context->print("\t");
    } else {
      first = false;
    }

    if (!columns.empty() &&
        columns.end() == std::find(columns.begin(), columns.end(), col->name))
      continue;

    column_indexes.push_back(column_index);
    if (!quiet) context->print(col->name);
  }
  if (!quiet) context->print("\n");

  for (;;) {
    const xcl::XRow *row(result->next());

    if (!row) break;

    try {
      std::vector<int>::iterator i = column_indexes.begin();
      const auto field_count = row->get_number_of_fields();
      for (; i != column_indexes.end() && (*i) < field_count; ++i) {
        std::string out_result;

        if (!row->get_field_as_string(*i, &out_result))
          throw std::runtime_error("Data decoder failed");

        int field = (*i);
        if (field != 0)
          if (!quiet) context->print("\t");
        std::string str = context->m_variables->unreplace(out_result, false);
        if (!quiet) context->print(str);
        if (value_callback) {
          value_callback(str);
          Value_callback().swap(value_callback);
        }
      }
    } catch (std::exception &e) {
      context->print_error("ERROR: ", e, '\n');
    }
    if (!quiet) context->print("\n");
  }
}

void print_help_commands() {
  std::cout << "Input may be a file (or if no --file is specified, it stdin "
               "will be used)\n";
  std::cout << "The following commands may appear in the input script:\n";
  std::cout << "-->echo <text>\n";
  std::cout << "  Prints the text (allows variables)\n";
  std::cout << "-->title <c><text>\n";
  std::cout << "  Prints the text with an underline, using the character <c>\n";
  std::cout << "-->sql\n";
  std::cout << "  Begins SQL block. SQL statements that appear will be "
               "executed and results printed (allows variables).\n";
  std::cout << "-->endsql\n";
  std::cout << "  End SQL block. End a block of SQL started by -->sql\n";
  std::cout << "-->macro <macroname> <argname1> ...\n";
  std::cout << "  Start a block of text to be defined as a macro. Must be "
               "terminated with -->endmacro\n";
  std::cout << "-->endmacro\n";
  std::cout << "  Ends a macro block\n";
  std::cout << "-->callmacro <macro>\t<argvalue1>\t...\n";
  std::cout << "  Executes the macro text, substituting argument values with "
               "the provided ones (args separated by tabs).\n";
  std::cout << "-->import <macrofile>\n";
  std::cout << "  Loads macros from the specified file. The file must be in "
               "the directory specified by --import option in command "
               "line.\n";
  std::cout << "-->macro_delimiter_compress TRUE|FALSE|0|1\n";
  std::cout << "  Enable/disable grouping of adjacent delimiters into\n";
  std::cout << "  single one at \"callmacro\" command.\n";
  std::cout << "-->do_ssl_handshake\n";
  std::cout << "  Execute SSL handshake, enables SSL on current connection\n";
  std::cout << "<protomsg>\n";
  std::cout << "  Encodes the text format protobuf message and sends it to "
               "the server (allows variables).\n";
  std::cout << "-->recv [quiet|<FIELD PATH>]\n";
  std::cout << "  quiet        - received message isn't printed\n";
  std::cout
      << "  <FIELD PATH> - print only selected part of the message using\n";
  std::cout << "                 \"field-path\" filter:\n";
  std::cout << "                 * field_name1\n";
  std::cout << "                 * field_name1.field_name2\n";
  std::cout << "                 * repeated_field_name1[1].field_name1\n";

  std::cout << "-->recvresult [print-columnsinfo] [" << CMD_ARG_BE_QUIET
            << "]\n";
  std::cout << "  Read and print one resultset from the server; if "
               "print-columnsinfo is present also print short columns "
               "status\n";
  std::cout << "-->recvtovar <varname> [COLUMN_NAME]\n";
  std::cout << "  Read first row and first column (or column with name "
               "COLUMN_NAME) of resultset\n";
  std::cout << "  and set the variable <varname>\n";
  std::cout << "-->recverror <errno>\n";
  std::cout << "  Read a message and ensure that it's an error of the "
               "expected type\n";
  std::cout << "-->recvtype <msgtype> (<msg_fied>|" << CMD_ARG_BE_QUIET
            << "|<msg_fied> " << CMD_ARG_BE_QUIET
            << "|<msg_fied> <expected_field_value>|)\n";
  std::cout << "  Read one message and print it, checking that its type is "
               "the specified one\n";
  std::cout << "-->recvok\n";
  std::cout << "  Expect to receive 'Mysqlx.Ok' message. Works with "
               "'expecterror' command.\n";
  std::cout << "-->recvuntil <msgtype> [do_not_show_intermediate]\n";
  std::cout << "  Read messages and print them, until a msg of the specified "
               "type (or Error) is received\n";
  std::cout << "  do_not_show_intermediate - if this argument is present "
               "then printing of intermediate message should be omitted\n";
  std::cout << "-->repeat <N> [<VARIABLE_NAME>]\n";
  std::cout
      << "  Begin block of instructions that should be repeated N times\n";
  std::cout << "-->endrepeat\n";
  std::cout << "  End block of instructions that should be repeated - next "
               "iteration\n";
  std::cout << "-->stmtsql <CMD>\n";
  std::cout << "  Send StmtExecute with sql command\n";
  std::cout << "-->stmtadmin <CMD> [json_string]\n";
  std::cout << "  Send StmtExecute with admin command with given aguments "
               "(formated as json object)\n";
  std::cout << "-->system <CMD>\n";
  std::cout << "  Execute application or script (dev only)\n";
  std::cout << "-->exit\n";
  std::cout << "  Stops reading commands, disconnects and exits (same as "
               "<eof>/^D)\n";
  std::cout << "-->abort\n";
  std::cout << "  Exit immediately, without performing cleanup\n";
  std::cout << "-->nowarnings/-->yeswarnings\n";
  std::cout << "  Whether to print warnings generated by the statement "
               "(default no)\n";
  std::cout << "-->recvuntildisc [" << CMD_ARG_SHOW_RECEIVED << "]\n";
  std::cout
      << "  Receive all messages until server drops current connection.\n";
  std::cout << "  " << CMD_ARG_SHOW_RECEIVED
            << " - received messages are printed to standard output.\n";
  std::cout << "-->peerdisc <MILLISECONDS> [TOLERANCE]\n";
  std::cout << "  Expect that xplugin disconnects after given number of "
               "milliseconds and tolerance\n";
  std::cout << "-->sleep <SECONDS>\n";
  std::cout << "  Stops execution of mysqlxtest for given number of seconds "
               "(may be fractional)\n";
  std::cout
      << "-->login <user>\t<pass>\t<db>\t<mysql41|plain|sha256_memory>]\n";
  std::cout << "  Performs authentication steps (use with --no-auth)\n";
  std::cout << "-->loginerror <errno>\t<user>\t<pass>\t<db>\n";
  std::cout << "  Performs authentication steps expecting an error (use with "
               "--no-auth)\n";
  std::cout << "-->fatalerrors/nofatalerrors\n";
  std::cout << "  Whether to immediately exit on MySQL errors\n";
  std::cout << "-->expecterror <errno>\n";
  std::cout << "  Expect a specific error for the next command and fail if "
               "something else occurs\n";
  std::cout << "  Works for: newsession, closesession, recvresult, recvok\n";
  std::cout << "-->newsession <name>\t<user>\t<pass>\t<db>\n";
  std::cout << "  Create a new connection which is going to be authenticate"
               " using sequence of mechanisms (AUTO). Use '-' in place of"
               " the user for raw connection.\n";
  std::cout << "-->newsession_mysql41 <name>\t<user>\t<pass>\t<db>\n";
  std::cout << "  Create a new connection which is going to be authenticate"
               " using MYSQL41 mechanism.\n";
  std::cout << "-->newsession_memory <name>\t<user>\t<pass>\t<db>\n";
  std::cout << "  Create a new connection which is going to be authenticate"
               " using SHA256_MEMORY mechanism.\n";
  std::cout << "-->newsession_plain <name>\t<user>\t<pass>\t<db>\n";
  std::cout << "  Create a new connection which is going to be authenticate"
               " using PLAIN mechanism.\n";
  std::cout << "-->setsession <name>\n";
  std::cout << "  Activate the named session\n";
  std::cout << "-->closesession [abort]\n";
  std::cout << "  Close the active session (unless its the default session)\n";
  std::cout << "-->wait_for <VALUE_EXPECTED>\t<SQL QUERY>\n";
  std::cout << "  Wait until SQL query returns value matches expected value "
               "(time limit 30 second)\n";
  std::cout << "-->assert_eq <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
  std::cout << "  Ensure that 'TESTED' value equals 'EXPECTED' by comparing "
               "strings lexicographically\n";
  std::cout << "-->assert_ne <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
  std::cout << "  Ensure that 'TESTED' value doesn't equals 'EXPECTED' by"
               " comparing strings lexicographically\n";
  std::cout << "-->assert_gt <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
  std::cout << "  Ensure that 'TESTED' value is greater than 'EXPECTED' "
               "(only when the both are numeric values)\n";
  std::cout << "-->assert_ge <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
  std::cout << "  Ensure that 'TESTED' value is greater  or equal to "
               "'EXPECTED' (only when the both are numeric values)\n";
  std::cout << "-->varfile <varname> <datafile>\n";
  std::cout << "  Assigns the contents of the file to the named variable\n";
  std::cout << "-->varlet <varname> <value>\n";
  std::cout << "  Assign the value (can be another variable) to the variable\n";
  std::cout << "-->varinc <varname> <n>\n";
  std::cout << "  Increment the value of varname by n (assuming both "
               "convert to integral)\n";
  std::cout << "-->varsub <varname>\n";
  std::cout << "  Add a variable to the list of variables to replace for "
               "the next recv or sql command (value is replaced by the "
               "name)\n";
  std::cout << "-->varescape <varname>\n";
  std::cout << "  Escape end-line and backslash characters.\n";
  std::cout << "-->binsend <bindump>[<bindump>...]\n";
  std::cout << "  Sends one or more binary message dumps to the server "
               "(generate those with --bindump)\n";
  std::cout << "-->binsendoffset <srcvar> [offset-begin[percent]> "
               "[offset-end[percent]]]\n";
  std::cout
      << "  Same as binsend with begin and end offset of data to be send\n";
  std::cout << "-->binparse MESSAGE.NAME {\n";
  std::cout << "    MESSAGE.DATA\n";
  std::cout << "}\n";
  std::cout << "  Dump given message to variable %MESSAGE_DUMP%\n";
  std::cout << "-->quiet/noquiet\n";
  std::cout << "  Toggle verbose messages\n";
  std::cout << "-->query_result/noquery_result\n";
  std::cout << "  Toggle visibility for query results\n";
  std::cout << "-->received <msgtype>\t<varname>\n";
  std::cout << "  Assigns number of received messages of indicated type (in "
               "active session) to a variable\n";
  std::cout << "# comment\n";
}
