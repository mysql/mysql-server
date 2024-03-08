/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_COMMAND_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_COMMAND_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/processor/execution_context.h"

class Command {
 public:
  enum class Result { Continue, Stop_with_success, Stop_with_failure };
  enum class Metadata_policy { Default, Store, Use_stored };

  using Any = ::Mysqlx::Datatypes::Any;

 public:
  Command();

  bool is_command_registred(const std::string &command_line,
                            std::string *out_command_name = nullptr,
                            bool *out_is_single_line_command = nullptr) const;

  Result process(std::istream &input, Execution_context *context,
                 const std::string &command);

 private:
  using Command_method = Result (Command::*)(std::istream &,
                                             Execution_context *,
                                             const std::string &);
  using Value_callback = std::function<bool(std::string)>;
  using Command_map = std::map<std::string, Command_method>;

  struct Loop_do {
    std::streampos block_begin;
    int iterations;
    int value;
    std::string variable_name;
  };

  Command_map m_commands;
  std::list<Loop_do> m_loop_stack;
  static xpl::chrono::Time_point m_start_measure;

  Result cmd_echo(std::istream &input, Execution_context *context,
                  const std::string &args);
  Result cmd_title(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_recvtype(std::istream &input, Execution_context *context,
                      const std::string &args);
  Result cmd_recvok(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_recvmessage(std::istream &input, Execution_context *context,
                         const std::string &args);
  Result cmd_recverror(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_recvtovar(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_recvresult(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_recvresult(std::istream &input, Execution_context *context,
                        const std::string &args, Value_callback value_callback,
                        const Metadata_policy = Metadata_policy::Default);
  Result cmd_recvuntil(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_do_ssl_handshake(std::istream &input, Execution_context *context,
                              const std::string &args);
  Result cmd_stmtsql(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_stmtadmin(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_sleep(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_login(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_repeat(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_endrepeat(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_loginerror(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_system(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_recv_all_until_disc(std::istream &input,
                                 Execution_context *context,
                                 const std::string &args);
  Result cmd_enable_compression(std::istream &input, Execution_context *context,
                                const std::string &args);
  Result cmd_peerdisc(std::istream &input, Execution_context *context,
                      const std::string &args);
  Result cmd_recv(std::istream &input, Execution_context *context,
                  const std::string &args);
  Result cmd_exit(std::istream &input, Execution_context *context,
                  const std::string &args);
  Result cmd_abort(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_shutdown_on_classic(std::istream &input,
                                 Execution_context *context,
                                 const std::string &args);
  Result cmd_shutdown_server(std::istream &input, Execution_context *context,
                             const std::string &args);
  Result cmd_reconnect(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_nowarnings(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_yeswarnings(std::istream &input, Execution_context *context,
                         const std::string &args);
  Result cmd_fatalerrors(std::istream &input, Execution_context *context,
                         const std::string &args);
  Result cmd_fatalwarnings(std::istream &input, Execution_context *context,
                           const std::string &args);
  Result cmd_nofatalerrors(std::istream &input, Execution_context *context,
                           const std::string &args);
  Result cmd_newsession_mysql41(std::istream &input, Execution_context *context,
                                const std::string &args);
  Result cmd_newsession_memory(std::istream &input, Execution_context *context,
                               const std::string &args);
  Result cmd_newsession_plain(std::istream &input, Execution_context *context,
                              const std::string &args);
  Result cmd_newsession(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_setsession(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_closesession(std::istream &input, Execution_context *context,
                          const std::string &args);
  Result cmd_expecterror(std::istream &input, Execution_context *context,
                         const std::string &args);
  Result cmd_expectwarnings(std::istream &input, Execution_context *context,
                            const std::string &args);
  Result cmd_measure(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_endmeasure(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_quiet(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_noquiet(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_varsub(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_varreplace(std::istream &input, Execution_context *context,
                        const std::string &args);
  Result cmd_varlet(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_varinc(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_vargen(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_varfile(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_varescape(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_binsend(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_hexsend(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_binsendoffset(std::istream &input, Execution_context *context,
                           const std::string &args);
  Result cmd_callmacro(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_macro_delimiter_compress(std::istream &input,
                                      Execution_context *context,
                                      const std::string &args);
  Result cmd_assert(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_assert_eq(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_assert_ne(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_assert_gt(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_assert_ge(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_assert_le(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_assert_lt(std::istream &input, Execution_context *context,
                       const std::string &args);
  Result cmd_query(std::istream &input, Execution_context *context,
                   const std::string &args);
  Result cmd_noquery(std::istream &input, Execution_context *context,
                     const std::string &args);
  Result cmd_compress(std::istream &input, Execution_context *context,
                      const std::string &args);
  Result cmd_wait_for(std::istream &input, Execution_context *context,
                      const std::string &args);
  Result cmd_import(std::istream &input, Execution_context *context,
                    const std::string &args);
  Result cmd_received(std::istream &input, Execution_context *context,
                      const std::string &args);
  Result cmd_clear_received(std::istream &input, Execution_context *context,
                            const std::string &args);
  Result cmd_recvresult_store_metadata(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args);
  Result cmd_recv_with_stored_metadata(std::istream &input,
                                       Execution_context *context,
                                       const std::string &args);
  Result cmd_clear_stored_metadata(std::istream &input,
                                   Execution_context *context,
                                   const std::string &args);
  Result cmd_env(std::istream &input, Execution_context *context,
                 const std::string &args);

  Result do_newsession(std::istream &input, Execution_context *context,
                       const std::string &args,
                       const std::vector<std::string> &auth_methods);
  size_t value_to_offset(const std::string &data, const size_t maximum_value);
  bool json_string_to_any(const std::string &json_string, Any *any) const;
  void print_resultset(Execution_context *context, Result_fetcher *result,
                       const std::vector<std::string> &columns,
                       Value_callback value_callback, const bool quiet,
                       const bool print_column_info);

  Result get_sql_variable(Execution_context *context, const std::string &name,
                          std::string *out_var);
  static void try_result(Result result);

  template <typename Equal_operator>
  Result cmd_assert_generic(std::istream & /*input*/,
                            Execution_context *context,
                            const std::string &args) {
    std::vector<std::string> vargs;

    try {
      aux::split(vargs, args, "\t", true);

      if (2 != vargs.size()) {
        context->print_error(context->m_script_stack,
                             "Specified invalid number of arguments for ",
                             context->m_command_name,
                             " command: ", vargs.size(), " expecting 2.\n",
                             "Executed with following argument: \"", args,
                             "\"");
        return Result::Stop_with_failure;
      }

      context->m_variables->replace(&vargs[0]);
      context->m_variables->replace(&vargs[1]);

      Equal_operator op;
      if (op(vargs[0], vargs[1])) return Result::Continue;
    } catch (const std::exception &) {
    }

    context->print_error(
        context->m_script_stack, "Execution of '", context->m_command_name, " ",
        context->m_command_arguments, "', resulted in an error.\n");
    context->print_error("Where argument0='", vargs[0], "', argument1='",
                         vargs[1], "'\n");

    return context->m_options.m_fatal_errors ? Result::Stop_with_failure
                                             : Result::Continue;
  }
};

void print_help_commands();

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_COMMAND_H_
