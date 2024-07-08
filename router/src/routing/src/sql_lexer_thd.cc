/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sql_lexer_thd.h"

// initialize all charsets via "get_charset()" to ensure the charset subsystem
// is properly initialized.
//
// &my_charset_latin1 could be used too, but leads to garbage-pointers on
// windows if linked against a shared library.

THD::System_variables::System_variables()
    : character_set_client(get_charset(8, 0)),            // latin1
      default_collation_for_utf8mb4(get_charset(255, 0))  // utf8mb4_0900_ai_ci
{}
