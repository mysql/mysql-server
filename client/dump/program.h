/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PROGRAM_INCLUDED
#define PROGRAM_INCLUDED

#include <atomic>
#include <optional>

#include "client/base/abstract_connection_program.h"
#include "client/dump/mysql_chain_element_options.h"
#include "client/dump/mysqldump_tool_chain_maker_options.h"

namespace Mysql {
namespace Tools {
namespace Dump {

class Program : public Mysql::Tools::Base::Abstract_connection_program {
 public:
  Program();

  ~Program() override;

  std::string get_version() override;

  int get_first_release_year() override;

  std::string get_description() override;

  int execute(const std::vector<std::string> &positional_options) override;

  void create_options() override;

  void error(const Mysql::Tools::Base::Message_data &message) override;

  void short_usage() override;

  void check_mutually_exclusive_options();

  int get_total_connections();

  int get_error_code() override;

 private:
  bool message_handler(const Mysql::Tools::Base::Message_data &message);
  void error_log_file_callback(char *);
  void close_redirected_stderr();

  Mysql_chain_element_options *m_mysql_chain_element_options;
  Mysqldump_tool_chain_maker_options *m_mysqldump_tool_chain_maker_options;
  bool m_single_transaction;
  bool m_watch_progress;
  std::optional<std::string> m_error_log_file;
  FILE *m_stderr;
  std::atomic<uint32_t> m_error_code;
};

}  // namespace Dump
}  // namespace Tools
}  // namespace Mysql

#endif
