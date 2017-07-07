/*
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_
#define X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_


class Command_line_options {
 public:
  int exit_code;
  bool needs_password;

 protected:
  Command_line_options(const int argc, char **argv) : exit_code(0) {}

  bool check_arg(char **argv,
                 int &argi,
                 const char *arg,
                 const char *larg);

  bool is_quote_char(const char single_char);
  bool should_remove_qoutes(const char first, const char last);
  bool check_arg_with_value(char **argv,
                            int &argi,
                            const char *arg,
                            const char *larg,
                            char *&value);
};

#endif  // X_TESTS_DRIVER_COMMON_COMMAND_LINE_OPTIONS_H_
