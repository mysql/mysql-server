/* Copyright (c) 2023, 2024, Oracle and/or its affiliates. */

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
