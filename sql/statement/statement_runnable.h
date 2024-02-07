/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef STATEMENT_RUNNABLE_H
#define STATEMENT_RUNNABLE_H

#include "lex_string.h"

class THD;

/*******************************************************************
 * Server_runnable
 *******************************************************************/

/**
  Execute a fragment of server code in an isolated context, so that
  it doesn't leave any effect on THD. THD must have no open tables.
  The code must not leave any open tables around.
  The result of execution (if any) is stored in Ed_result.
*/

class Server_runnable {
 public:
  virtual bool execute_server_code(THD *thd) = 0;
  virtual ~Server_runnable();
};

/**
  Execute one SQL statement in an isolated context.

  Allows to execute a SQL statement from within another statement.
  Supports even executing a SQL statement from within stored program.
*/

class Statement_runnable : public Server_runnable {
 public:
  Statement_runnable(LEX_STRING sql_text);
  bool execute_server_code(THD *thd) override;

 private:
  LEX_STRING m_sql_text;
};

#endif
