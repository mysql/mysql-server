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

/*
  Parse tree node classes for optimizer hint syntax
*/


#ifndef PARSE_TREE_HINTS_INCLUDED
#define PARSE_TREE_HINTS_INCLUDED

#include "my_config.h"
#include "parse_tree_node_base.h"
#include "sql_alloc.h"
#include "sql_list.h"
#include "mem_root_array.h"

class LEX;


struct Hint_param_table
{
  LEX_CSTRING table;
  LEX_CSTRING opt_query_block;
};


typedef Mem_root_array<LEX_CSTRING, true> Hint_param_index_list;
typedef Mem_root_array<Hint_param_table, true> Hint_param_table_list;


class PT_hint : public Parse_tree_node
{
};


class PT_hint_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Mem_root_array<PT_hint *, true> hints;

public:
  explicit PT_hint_list(MEM_ROOT *mem_root) : hints(mem_root) {}

  virtual bool contextualize(Parse_context *pc);

  bool push_back(PT_hint *hint) { return hints.push_back(hint); }
};


class PT_hint_max_execution_time : public PT_hint
{
  typedef PT_hint super;
public:
  ulong milliseconds;

  explicit PT_hint_max_execution_time(ulong milliseconds_arg)
  : milliseconds(milliseconds_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_hint_debug1 : public PT_hint
{
  const LEX_CSTRING opt_qb_name;
  Mem_root_array<Hint_param_table, true> *table_list;

public:
  PT_hint_debug1(const LEX_CSTRING &opt_qb_name_arg,
                 Mem_root_array<Hint_param_table, true> *table_list_arg)
  : opt_qb_name(opt_qb_name_arg),
    table_list(table_list_arg)
  {}
};


class PT_hint_debug2 : public PT_hint
{
  Mem_root_array<LEX_CSTRING, true> *opt_index_list;

public:
  explicit
  PT_hint_debug2(Mem_root_array<LEX_CSTRING, true> *opt_index_list_arg)
  : opt_index_list(opt_index_list_arg)
  {}
};

#endif /* PARSE_TREE_HINTS_INCLUDED */
