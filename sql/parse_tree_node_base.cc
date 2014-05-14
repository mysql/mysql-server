/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include "parse_tree_node_base.h"
#include "sql_parse.h"

void Parse_tree_node::error(Parse_context *pc, const POS &position) const
{
  Lex_input_stream *lip= &pc->thd->m_parser_state->m_lip;
  uint lineno= position.raw.start ? lip->get_lineno(position.raw.start) : 1;
  const char *pos= position.raw.start ? position.raw.start : "";
  ErrConvString err(pos, pc->thd->variables.character_set_client);
  my_printf_error(ER_PARSE_ERROR,  ER(ER_PARSE_ERROR), MYF(0),
                  ER(ER_SYNTAX_ERROR), pos, lineno);
}


bool Parse_tree_node::contextualize(Parse_context *pc)
{
#ifndef DBUG_OFF
  if (transitional)
  {
    DBUG_ASSERT(contextualized);
    return false;
  }
#endif//DBUG_OFF

  uchar dummy;
  if (check_stack_overrun(pc->thd, STACK_MIN_SIZE, &dummy))
    return true;

#ifndef DBUG_OFF
  DBUG_ASSERT(!contextualized);
  contextualized= true;
#endif//DBUG_OFF

  return false;
}


Parse_context::Parse_context(THD *thd, st_select_lex *select)
: thd(thd),
  mem_root(thd->mem_root),
  select(select)
{}
