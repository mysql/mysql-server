/* Copyright (c) 2001, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/* Execute DO statement */

#include "sql_priv.h"
#include "transaction.h"
#include "unireg.h"
#include "sql_do.h"
#include "sql_base.h"                           // setup_fields
#include "sql_select.h"                         // free_underlaid_joins

bool mysql_do(THD *thd, List<Item> &values)
{
  List_iterator<Item> li(values);
  Item *value;
  DBUG_ENTER("mysql_do");
  if (setup_fields(thd, Ref_ptr_array(), values, MARK_COLUMNS_NONE, 0, 0))
    DBUG_RETURN(TRUE);
  while ((value = li++))
    value->val_int();
  free_underlaid_joins(thd, &thd->lex->select_lex);

  if (thd->is_error())
  {
    /*
      Rollback the effect of the statement, since next instruction
      will clear the error and the rollback in the end of
      mysql_execute_command() won't work.
    */
    if (! thd->in_sub_stmt)
      trans_rollback_stmt(thd);
    thd->clear_error(); // DO always is OK
  }
  my_ok(thd);
  DBUG_RETURN(FALSE);
}
