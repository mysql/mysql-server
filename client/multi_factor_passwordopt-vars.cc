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

/**
  @file include/multi_factor_passwordopt-vars.h
*/

#include "client/include/multi_factor_passwordopt-vars.h"
#include "my_getopt.h"
#include "mysql.h"
#include "mysql/service_mysql_alloc.h"  // my_free, my_strdup
#include "nulls.h"

char *opt_password[MAX_AUTH_FACTORS] = {nullptr};
bool tty_password[MAX_AUTH_FACTORS] = {false};

/**
  Helper method used by clients to parse password set as part of command line.
  This method checks if password value is specified or not. If not then a flag
  is set to let client accept password from terminal.

  @param opt            password option
  @param argument       value specified for --password<1,2,3> or --password
*/
void parse_command_line_password_option(const struct my_option *opt,
                                        char *argument) {
  if (argument == disabled_my_option) {
    // Don't require password
    static char empty_password[] = {'\0'};
    assert(empty_password[0] ==
           '\0');  // Check that it has not been overwritten
    argument = empty_password;
  }
  /*
    password options can be --password or --password1 or --password2 or
    --password3. Thus extract factor from option.
  */
  unsigned int factor = 0;
  if (strcmp(opt->name, "password"))
    factor = opt->name[strlen("password")] - '0' - 1;
  if (argument) {
    char *start = argument;
    my_free(opt_password[factor]);
    opt_password[factor] =
        my_strdup(PSI_NOT_INSTRUMENTED, argument, MYF(MY_FAE));
    while (*argument) *argument++ = 'x';  // Destroy argument
    if (*start) start[1] = 0;
    tty_password[factor] = false;
  } else
    tty_password[factor] = true;
}

/**
  Helper method used by clients to set password in mysql->options
*/
void set_password_options(MYSQL *mysql) {
  for (unsigned int factor = 1; factor <= MAX_AUTH_FACTORS; factor++) {
    /**
     If tty_password is true get password from terminal and update in
     opt_password and set tty_password to false
    */
    if (tty_password[factor - 1]) {
      opt_password[factor - 1] = get_tty_password(NullS);
      tty_password[factor - 1] = false;
    }
    /**
    If opt_password is populated call mysql_options4()
    */
    if (opt_password[factor - 1]) {
      mysql_options4(mysql, MYSQL_OPT_USER_PASSWORD, &factor,
                     opt_password[factor - 1]);
    }
  }
}

void free_passwords() {
  for (unsigned int factor = 1; factor <= MAX_AUTH_FACTORS; factor++) {
    if (opt_password[factor - 1]) {
      my_free(opt_password[factor - 1]);
      opt_password[factor - 1] = nullptr;
    }
  }
}
