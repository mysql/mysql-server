/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_PORTLIB_NDB_PASSWORD_H
#define NDB_PORTLIB_NDB_PASSWORD_H

#include <stddef.h>

/*
 * ndb_get_password_from_tty and ndb_get_password_from_stdin
 *
 * The functions read one line of input and take as password.
 *
 * Line must end with NL, on Windows CR+NL is also valid.
 *
 * Only printable ASCII are allowed in password.
 *
 * Too long password is not truncated, rather function fails.
 *
 * If input is a terminal and stdout or stderr is also a terminal, prompt will
 * be written to terminal.
 *
 * On success function will return number of characters in password, excluding
 * the terminating NUL character.  buf must have space for size characters
 * including the terminating NUL.
 *
 * On failure it returns a ndb_get_password_error as int.
 */

enum class ndb_get_password_error : int {
  ok = 0,
  system_error = -1,
  too_long = -2,
  bad_char = -3,
  no_end = -4
};
int ndb_get_password_from_tty(const char prompt[], char buf[], size_t size);
int ndb_get_password_from_stdin(const char prompt[], char buf[], size_t size);
#endif
