/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "sql_list.h"                 // Sql_alloc, List, List_iterator
#include "sql_cmd.h"                  // Sql_cmd
#include "sql_class.h"                // Diagnostics_area
#include "sql_get_diagnostics.h"      // Sql_cmd_get_diagnostics

/**
  Execute this GET DIAGNOSTICS statement.

  @param thd The current thread.

  @remark Errors or warnings occurring during the execution of the GET
          DIAGNOSTICS statement should not affect the diagnostics area
          of a previous statement as the diagnostics information there
          would be wiped out. Thus, in order to preserve the contents
          of the diagnostics area from which information is being
          retrieved, the GET DIAGNOSTICS statement is executed under
          a separate diagnostics area. If any errors or warnings occur
          during the execution of the GET DIAGNOSTICS statement, these
          error or warnings (conditions) are appended to the list of
          the original diagnostics area. The only exception to this is
          fatal errors, which must always cause the statement to fail.

  @retval false on success.
  @retval true on error
*/

bool
Sql_cmd_get_diagnostics::execute(THD *thd)
{
  bool rv;
  Diagnostics_area new_stmt_da(thd->query_id, false);
  Diagnostics_area *save_stmt_da= thd->get_stmt_da();
  DBUG_ENTER("Sql_cmd_get_diagnostics::execute");

  /* Disable the unneeded read-only mode of the original DA. */
  save_stmt_da->get_warning_info()->set_read_only(false);

  /* Set new diagnostics area, execute statement and restore. */
  thd->set_stmt_da(&new_stmt_da);
  rv= m_info->aggregate(thd, save_stmt_da);
  thd->set_stmt_da(save_stmt_da);

  /* Bail out early if statement succeeded. */
  if (! rv)
  {
    my_ok(thd);
    DBUG_RETURN(false);
  }

  /* Statement failed, retrieve the error information for propagation. */
  uint sql_errno= new_stmt_da.sql_errno();
  const char *message= new_stmt_da.message();
  const char *sqlstate= new_stmt_da.get_sqlstate();

  /* In case of a fatal error, set it into the original DA.*/
  if (thd->is_fatal_error)
  {
    save_stmt_da->set_error_status(thd, sql_errno, message, sqlstate);
    DBUG_RETURN(true);
  }

  /* Otherwise, just append the new error as a exception condition. */
  save_stmt_da->get_warning_info()->push_warning(thd, sql_errno, sqlstate,
                                                 MYSQL_ERROR::WARN_LEVEL_ERROR,
                                                 message);

  /* Appending might have failed. */
  if (! (rv= thd->is_error()))
    my_ok(thd);

  DBUG_RETURN(rv);
}


/**
  Set a value for this item.

  @param thd    The current thread.
  @param value  The obtained value.

  @retval false on success.
  @retval true on error.
*/

bool
Diagnostics_information_item::set_value(THD *thd, Item **value)
{
  bool rv;
  Settable_routine_parameter *srp;
  DBUG_ENTER("Diagnostics_information_item::set_value");

  /* Get a settable reference to the target. */
  srp= m_target->get_settable_routine_parameter();

  DBUG_ASSERT(srp);

  /* Set variable/parameter value. */
  rv= srp->set_value(thd, thd->spcont, value);

  DBUG_RETURN(rv);
}


/**
  Obtain statement information in the context of a given diagnostics area.

  @param thd  The current thread.
  @param da   The diagnostics area.

  @retval false on success.
  @retval true on error
*/

bool
Statement_information::aggregate(THD *thd, const Diagnostics_area *da)
{
  bool rv= false;
  Statement_information_item *stmt_info_item;
  List_iterator<Statement_information_item> it(*m_items);
  DBUG_ENTER("Statement_information::aggregate");

  /*
    Each specified target gets the value of each given
    information item obtained from the diagnostics area.
  */
  while ((stmt_info_item= it++))
  {
    if ((rv= evaluate(thd, stmt_info_item, da)))
      break;
  }

  DBUG_RETURN(rv);
}


/**
  Obtain the value of this statement information item in the context of
  a given diagnostics area.

  @param thd  The current thread.
  @param da   The diagnostics area.

  @retval Item representing the value.
  @retval NULL on error.
*/

Item *
Statement_information_item::get_value(THD *thd, const Diagnostics_area *da)
{
  Item *value= NULL;
  DBUG_ENTER("Statement_information_item::get_value");

  switch (m_name)
  {
  /*
    The number of condition areas that have information. That is,
    the number of errors and warnings within the diagnostics area.
  */
  case NUMBER:
  {
    ulong count= da->get_warning_info()->cond_count();
    value= new (thd->mem_root) Item_uint(count);
    break;
  }
  /*
    Number that shows how many rows were directly affected by
    a data-change statement (INSERT, UPDATE, DELETE, MERGE,
    REPLACE, LOAD).
  */
  case ROW_COUNT:
    value= new (thd->mem_root) Item_int(thd->get_row_count_func());
    break;
  }

  DBUG_RETURN(value);
}

