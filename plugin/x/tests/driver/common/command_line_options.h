/*
 * Copyright (c) 2014, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_
#define PLUGIN_X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_

class Command_line_options {
 public:
  int exit_code;
  bool needs_password;

 protected:
  Command_line_options(const int argc, char **argv) : exit_code(0) {}

  bool check_arg(char **argv, int &argi, const char *arg, const char *larg);

  bool is_quote_char(const char single_char);
  bool should_remove_qoutes(const char first, const char last);
  bool check_arg_with_value(char **argv, int &argi, const char *arg,
                            const char *larg, char *&value);
};

#endif  // PLUGIN_X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_
