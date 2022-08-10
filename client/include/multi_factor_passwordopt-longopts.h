/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file include/multi_factor_passwordopt-longopts.h

  This file contains password options to be specified as part of command-line
  options for first,second,third authentication plugin factors for a given user.
*/

/* preserve existing option for backward compatibility */
{"password",
 'p',
 "Password to use when connecting to server. If password is not given it's "
 "asked from the tty.",
 nullptr,
 nullptr,
 nullptr,
 GET_PASSWORD,
 OPT_ARG,
 0,
 0,
 0,
 nullptr,
 0,
 nullptr},
    /*
     --password1, --password2 --password3 are new options to handle password for
     first, second and third factor authentication plugin defined for a given
     user account
   */
    {"password1",
     MYSQL_OPT_USER_PASSWORD,
     "Password for first factor authentication plugin.",
     nullptr,
     nullptr,
     nullptr,
     GET_PASSWORD,
     OPT_ARG,
     0,
     0,
     0,
     nullptr,
     0,
     nullptr},
    {"password2",
     MYSQL_OPT_USER_PASSWORD,
     "Password for second factor authentication plugin.",
     nullptr,
     nullptr,
     nullptr,
     GET_PASSWORD,
     OPT_ARG,
     0,
     0,
     0,
     nullptr,
     0,
     nullptr},
    {"password3",
     MYSQL_OPT_USER_PASSWORD,
     "Password for third factor authentication plugin.",
     nullptr,
     nullptr,
     nullptr,
     GET_PASSWORD,
     OPT_ARG,
     0,
     0,
     0,
     nullptr,
     0,
     nullptr},
