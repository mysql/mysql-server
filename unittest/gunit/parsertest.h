#ifndef PARSERTEST_INCLUDED
#define PARSERTEST_INCLUDED
/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file parsertest.h

*/

#include "sql/sql_class.h"
#include "sql/sql_parse.h"
#include "unittest/gunit/test_utils.h"

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

/*
  A class for unit testing the parser.
*/
class ParserTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() const { return initializer.thd(); }

  Server_initializer initializer;

  /*
    Parses a query and returns a parse tree. In our parser this is
    called a SELECT_LEX.
  */
  SELECT_LEX *parse(const char *query, int expected_error_code) const
  {
    Parser_state state;

    size_t length= strlen(query);
    char *mutable_query= const_cast<char*>(query);

    state.init(thd(), mutable_query, length);

    /*
      This tricks the server to parse the query and then stop,
      without executing.
    */
    initializer.set_expected_error(ER_MUST_CHANGE_PASSWORD);
    thd()->security_context()->set_password_expired(true);

    Mock_error_handler handler(thd(), expected_error_code);
    lex_start(thd());

    if (thd()->db().str == NULL)
    {
      // The THD DTOR will do my_free() on this.
      char *db= static_cast<char*>(my_malloc(PSI_NOT_INSTRUMENTED, 3, MYF(0)));
      sprintf(db, "db");
      LEX_CSTRING db_lex_cstr= { db, strlen(db) };
      thd()->reset_db(db_lex_cstr);
    }

    lex_start(thd());
    mysql_reset_thd_for_next_command(thd());
    bool err= parse_sql(thd(), &state, NULL);
    assert_eq(0, err);
    return thd()->lex->current_select();
  }

  void assert_eq(int x, int y) const
  {
    ASSERT_EQ(x, y);
  }

  SELECT_LEX *parse(const char *query) const { return parse(query, 0); }

};


#endif // PARSERTEST_INCLUDED
