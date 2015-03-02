/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

Parse_context::Parse_context(THD *thd, st_select_lex *select)
: thd(thd),
  mem_root(thd->mem_root),
  select(select)
{}


/**
  my_syntax_error() function replacement for deferred reporting of syntax
  errors

  @param      pc      current parse context
  @param      pos     location of the error in lexical scanner buffers
  @param      msg     error message: NULL default means ER(ER_SYNTAX_ERROR)
*/
void Parse_tree_node::error(Parse_context *pc,
                            const POS &position,
                            const char * msg) const
{
  pc->thd->parse_error_at(position, msg);
}
