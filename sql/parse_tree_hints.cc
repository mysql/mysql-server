/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "parse_tree_hints.h"
#include "sql_class.h"
#include "sql_lex.h"

bool PT_hint_list::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;
  for (PT_hint **h= hints.begin(), **end= hints.end(); h < end; h++)
  {
    if (*h != NULL && (*h)->contextualize(pc))
      return true;
  }
  return false;
}


bool PT_hint_max_execution_time::contextualize(Parse_context *pc)
{
  if (super::contextualize(pc))
    return true;
  if (pc->thd->lex->sql_command == SQLCOM_SELECT)
    pc->thd->lex->max_statement_time= milliseconds;
  else
    push_warning(pc->thd, Sql_condition::SL_WARNING,
        ER_WARN_UNSUPPORTED_MAX_EXECUTION_TIME,
        ER_THD(pc->thd, ER_WARN_UNSUPPORTED_MAX_EXECUTION_TIME));
  return false;
}

