/* Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/* Execute DO statement */

#include "transaction.h"
#include "sql_do.h"
#include "sql_base.h"                 // open_tables_for_query
#include "sql_select.h"               // handle_query
#include "auth_common.h"              // check_table_access
 
bool mysql_do(THD *thd, LEX *lex)
{
  DBUG_ENTER("mysql_do");

  if (check_table_access(thd, SELECT_ACL, lex->query_tables, false, UINT_MAX,
                         false))
    DBUG_RETURN(true);

  DBUG_ASSERT(!lex->unit->global_parameters()->explicit_limit);

  if (open_tables_for_query(thd, lex->query_tables, 0))
    DBUG_RETURN(true);

  DBUG_ASSERT(!lex->describe);

  Query_result *result= new Query_result_do(thd);
  if (!result)
    DBUG_RETURN(true);

  if (handle_query(thd, lex, result, 0, 0))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

bool Query_result_do::send_data(List<Item> &items)
{
  DBUG_ENTER("Query_result_do::send_data");

  char buffer[MAX_FIELD_WIDTH];
  String str_buffer(buffer, sizeof (buffer), &my_charset_bin);
  List_iterator_fast<Item> it(items);

  // Evaluate all fields, but do not send them
  for (Item *item= it++; item; item= it++)
  {
    if (item->evaluate(thd, &str_buffer))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


bool Query_result_do::send_eof()
{
  /* 
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error())
    return true;
  ::my_ok(thd);
  return false;
}
