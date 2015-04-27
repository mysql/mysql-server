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
#include "sql_string.h"
#include "sql_show.h"
#include "opt_hints.h"

struct LEX;


struct Hint_param_table
{
  LEX_CSTRING table;
  LEX_CSTRING opt_query_block;
};


typedef Mem_root_array_YY<LEX_CSTRING, true> Hint_param_index_list;
typedef Mem_root_array_YY<Hint_param_table, true> Hint_param_table_list;


/**
  The class is a base class for representation of the
  different types of the hints. For the complex hints
  it is also used as a container for additional argumnets.
*/
class PT_hint : public Parse_tree_node
{
  opt_hints_enum hint_type; // Hint type
  bool state;                    // true if hints is on, false otherwise
public:
  PT_hint(opt_hints_enum hint_type_arg, bool switch_state_arg)
    : hint_type(hint_type_arg), state(switch_state_arg)
  {}

  opt_hints_enum type() const { return hint_type; }
  bool switch_on() const { return state; }
  /**
    Print warning issuing in processing of the hint.

    @param thd             Pointer to THD object
    @param err_code        Error code
    @param qb_name_arg     QB name
    @param table_name_arg  table name
    @param key_name_arg    key name
    @param hint            Pointer to hint object
  */
  virtual void print_warn(THD *thd, uint err_code,
                          const LEX_CSTRING *qb_name_arg,
                          LEX_CSTRING *table_name_arg,
                          LEX_CSTRING *key_name_arg,
                          PT_hint *hint) const;
  /**
    Append additional hint arguments.

    @param thd             Pointer to THD object
    @param str             Pointer to String object
  */
  virtual void append_args(THD *thd, String *str) const {}
};


class PT_hint_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Mem_root_array<PT_hint *, true> hints;

public:
  explicit PT_hint_list(MEM_ROOT *mem_root) : hints(mem_root) {}

  /**
    Function handles list of the hints we get after
    parse procedure. It also creates query block hint
    object(Opt_hints_qb) if it does not exists.

    @param pc   Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);

  bool push_back(PT_hint *hint) { return hints.push_back(hint); }
};


/**
  Parse tree hint object for query block level hints.
*/
class PT_qb_level_hint : public PT_hint
{
  const LEX_CSTRING qb_name;  //< Name of query block
  uint args;                  //< Bit mask of arguments to hint

  typedef PT_hint super;
public:
  PT_qb_level_hint(const LEX_CSTRING qb_name_arg, bool switch_state_arg,
                   enum opt_hints_enum hint_type_arg, uint arg)
    : PT_hint(hint_type_arg, switch_state_arg),
      qb_name(qb_name_arg), args(arg)
  {}

  uint get_args() const { return args; }

  /**
    Function handles query block level hint. It also creates query block hint
    object (Opt_hints_qb) if it does not exist.

    @param pc  Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);

  /**
    Append hint arguments to given string

    @param thd             Pointer to THD object
    @param str             Pointer to String object
  */
  virtual void append_args(THD *thd, String *str) const;
};


/**
  Parse tree hint object for table level hints.
*/

class PT_table_level_hint : public PT_hint
{
  const LEX_CSTRING qb_name;
  Hint_param_table_list table_list;

  typedef PT_hint super;
public:
  PT_table_level_hint(const LEX_CSTRING qb_name_arg,
                      const Hint_param_table_list &table_list_arg,
                      bool switch_state_arg,
                      opt_hints_enum hint_type_arg)
    : PT_hint(hint_type_arg, switch_state_arg),
      qb_name(qb_name_arg), table_list(table_list_arg)
  {}

  /**
    Function handles table level hint. It also creates
    table hint object (Opt_hints_table) if it does not
    exist.

    @param pc  Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);
};


/**
  Parse tree hint object for key level hints.
*/

class PT_key_level_hint : public PT_hint
{
  Hint_param_table table_name;
  Hint_param_index_list key_list;

  typedef PT_hint super;
public:
  PT_key_level_hint(Hint_param_table &table_name_arg,
                    const Hint_param_index_list &key_list_arg,
                    bool switch_state_arg,
                    opt_hints_enum hint_type_arg)
    : PT_hint(hint_type_arg, switch_state_arg),
      table_name(table_name_arg), key_list(key_list_arg)
  {}

  /**
    Function handles key level hint.
    It also creates key hint object
    (Opt_hints_key) if it does not
    exist.

    @param pc  Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);
};


/**
  Parse tree hint object for QB_NAME hint.
*/

class PT_hint_qb_name : public PT_hint
{
  const LEX_CSTRING qb_name;

  typedef PT_hint super;
public:
  PT_hint_qb_name(const LEX_CSTRING qb_name_arg)
    : PT_hint(QB_NAME_HINT_ENUM, true), qb_name(qb_name_arg)
  {}

  /**
    Function sets query block name.

    @param pc  Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);
  virtual void append_args(THD *thd, String *str) const
  {
    append_identifier(thd, str, qb_name.str, qb_name.length);
  }

};


/**
  Parse tree hint object for MAX_EXECUTION_TIME hint.
*/

class PT_hint_max_execution_time : public PT_hint
{
  typedef PT_hint super;
public:
  ulong milliseconds;

  explicit PT_hint_max_execution_time(ulong milliseconds_arg)
    : PT_hint(MAX_EXEC_TIME_HINT_ENUM, true), milliseconds(milliseconds_arg)
  {}
  /**
    Function initializes MAX_EXECUTION_TIME hint

    @param pc   Pointer to Parse_context object

    @return  true in case of error,
             false otherwise
  */
  virtual bool contextualize(Parse_context *pc);
  virtual void append_args(THD *thd, String *str) const
  {
    str->append_ulonglong(milliseconds);
  }
};


#endif /* PARSE_TREE_HINTS_INCLUDED */
