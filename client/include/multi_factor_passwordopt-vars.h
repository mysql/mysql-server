/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MULTI_FACTOR_PASSWORDOPT_VARS_H
#define MULTI_FACTOR_PASSWORDOPT_VARS_H

#include "mysql.h"

extern char *opt_password[MAX_AUTH_FACTORS];
extern bool tty_password[MAX_AUTH_FACTORS];

/** parse passwords for --password or --password<N> option where N = 1,2,3 */
void parse_command_line_password_option(const struct my_option *opt,
                                        char *argument);
/** Set password in mysql->options */
void set_password_options(MYSQL *mysql);
/** Release memory for opt_password */
void free_passwords();

#define PARSE_COMMAND_LINE_PASSWORD_OPTION             \
  case 'p':                                            \
    parse_command_line_password_option(opt, argument); \
    break;                                             \
  case MYSQL_OPT_USER_PASSWORD:                        \
    parse_command_line_password_option(opt, argument); \
    break;

#endif  // MULTI_FACTOR_PASSWORDOPT_VARS_H
