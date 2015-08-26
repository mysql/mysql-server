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


#ifndef OPT_HINTS_INCLUDED
#define OPT_HINTS_INCLUDED

#include "my_config.h"
#include "parse_tree_node_base.h"
#include "sql_alloc.h"
#include "sql_list.h"
#include "mem_root_array.h"
#include "sql_string.h"
#include "sql_bitmap.h"
#include "sql_show.h"
#include "item_subselect.h"

struct LEX;
struct TABLE;


/**
  Hint types, MAX_HINT_ENUM should be always last.
  This enum should be synchronized with opt_hint_info
  array(see opt_hints.cc).
*/
enum opt_hints_enum
{
  BKA_HINT_ENUM= 0,
  BNL_HINT_ENUM,
  ICP_HINT_ENUM,
  MRR_HINT_ENUM,
  NO_RANGE_HINT_ENUM,
  MAX_EXEC_TIME_HINT_ENUM,
  QB_NAME_HINT_ENUM,
  SEMIJOIN_HINT_ENUM,
  SUBQUERY_HINT_ENUM,
  MAX_HINT_ENUM
};


struct st_opt_hint_info
{
  const char* hint_name;  // Hint name.
  bool check_upper_lvl;   // true if upper level hint check is needed (for hints
                          // which can be specified on more than one level).
  bool switch_hint;       // true if hint is not complex.
};


/**
  Opt_hints_map contains information
  about hint state(specified or not, hint value).
*/

class Opt_hints_map : public Sql_alloc
{
  Bitmap<64> hints;           // hint state
  Bitmap<64> hints_specified; // true if hint is specified
public:

  /**
     Check if hint is specified.

     @param type_arg   hint type

     @return true if hint is specified
  */
  my_bool is_specified(opt_hints_enum type_arg) const
  {
    return hints_specified.is_set(type_arg);
  }
  /**
     Set switch value and set hint into specified state.

     @param type_arg           hint type
     @param switch_state_arg   switch value
  */
  void set_switch(opt_hints_enum type_arg,
                  bool switch_state_arg)
  {
    if (switch_state_arg)
      hints.set_bit(type_arg);
    else
      hints.clear_bit(type_arg);
    hints_specified.set_bit(type_arg);
  }
  /**
     Get switch value.

     @param type_arg    hint type

     @return switch value.
  */
  bool switch_on(opt_hints_enum type_arg) const
  {
    return hints.is_set(type_arg);
  }
};


class PT_hint;
class PT_hint_max_execution_time;
class Opt_hints_key;


/**
  Opt_hints class is used as ancestor for Opt_hints_global,
  Opt_hints_qb, Opt_hints_table, Opt_hints_key classes.

  Opt_hints_global class is hierarchical structure.
  It contains information about global hints and also
  conains array of QUERY BLOCK level objects (Opt_hints_qb class).
  Each QUERY BLOCK level object contains array of TABLE level hints
  (class Opt_hints_table). Each TABLE level hint contains array of
  KEY lelev hints (Opt_hints_key class).
  Hint information(specified, on|off state) is stored in hints_map object.
*/

class Opt_hints : public Sql_alloc
{
  /*
    Name of object referred by the hint.
    This name is empty for global level,
    query block name for query block level,
    table name for table level and key name
    for key level.
  */
  const LEX_CSTRING *name;
  /*
    Parent object. There is no parent for global level,
    for query block level parent is Opt_hints_global object,
    for table level parent is Opt_hints_qb object,
    for key level parent is Opt_hints_key object.
  */
  Opt_hints *parent;

  Opt_hints_map hints_map;   // Hint map

  /* Array of child objects. i.e. array of the lower level objects */
  Mem_root_array<Opt_hints*, true> child_array;
  /* true if hint is connected to the real object */
  bool resolved;
  /* Number of resolved children */
  uint resolved_children;

public:

  Opt_hints(const LEX_CSTRING *name_arg,
            Opt_hints *parent_arg,
            MEM_ROOT *mem_root_arg)
    : name(name_arg), parent(parent_arg), child_array(mem_root_arg),
      resolved(false), resolved_children(0)
  { }

  bool is_specified(opt_hints_enum type_arg) const
  {
    return hints_map.is_specified(type_arg);
  }

  /**
    Function sets switch hint state.

    @param switch_state_arg  switch hint state
    @param type_arg          hint type
    @param check_parent      true if hint can be on parent level

    @return  true if hint is already specified,
             false otherwise
  */
  bool set_switch(bool switch_state_arg,
                  opt_hints_enum type_arg,
                  bool check_parent)
  {
    if (is_specified(type_arg) ||
        (check_parent && parent->is_specified(type_arg)))
      return true;

    hints_map.set_switch(type_arg, switch_state_arg);
    return false;
  }

  /**
    Function returns switch hint state.

    @param type_arg          hint type

    @return  hint value if hint is specified,
            false otherwise
  */
  bool get_switch(opt_hints_enum type_arg) const;

  virtual const LEX_CSTRING *get_name() const { return name; }
  void set_name(const LEX_CSTRING *name_arg) { name= name_arg; }
  Opt_hints *get_parent() const { return parent; }
  void set_resolved() { resolved= true; }
  bool is_resolved() const { return resolved; }
  void incr_resolved_children() { resolved_children++; }
  Mem_root_array<Opt_hints*, true> *child_array_ptr() { return &child_array; }

  bool is_all_resolved() const
  {
    return child_array.size() == resolved_children;
  }

  void register_child(Opt_hints* hint_arg)
  {
    child_array.push_back(hint_arg);
  }

  /**
    Returns pointer to complex hint for a given type.

    A complex hint is a hint that has arguments.
    (It is not just an on/off switch.)

    @param type  hint type

    @return  pointer to complex hint for a given type.
  */
  virtual PT_hint *get_complex_hints(opt_hints_enum type)
  {
    DBUG_ASSERT(0);
    return NULL; /* error C4716: must return a value */
  };

  /**
    Find hint among lower-level hint objects.

    @param name_arg        hint name
    @param cs              Pointer to character set

    @return  hint if found,
             NULL otherwise
  */
  Opt_hints *find_by_name(const LEX_CSTRING *name_arg,
                          const CHARSET_INFO *cs) const;
  /**
    Print all hints except of QB_NAME hint.

    @param thd              Pointer to THD object
    @param str              Pointer to String object
    @param query_type       If query type is QT_NORMALIZED_FORMAT,
                            un-resolved hints will also be printed
  */
  void print(THD *thd, String *str, enum_query_type query_type);
  /**
    Check if there are any unresolved hint objects and
    print warnings for them.

    @param thd             Pointer to THD object
  */
  void check_unresolved(THD *thd);
  virtual void append_name(THD *thd, String *str)= 0;

private:
  /**
    Append hint type.

    @param str             Pointer to String object
    @param type            Hint type
  */
  void append_hint_type(String *str, opt_hints_enum type);
  /**
    Print warning for unresolved hint name.

    @param thd             Pointer to THD object
  */
  void print_warn_unresolved(THD *thd);
};


/**
  Global level hints.
*/

class Opt_hints_global : public Opt_hints
{

public:
  PT_hint_max_execution_time *max_exec_time;

  Opt_hints_global(MEM_ROOT *mem_root_arg)
    : Opt_hints(NULL, NULL, mem_root_arg)
  {
    max_exec_time= NULL;
  }

  virtual void append_name(THD *thd, String *str) {}
  virtual PT_hint *get_complex_hints(opt_hints_enum type);
};


class PT_qb_level_hint;

/**
  Query block level hints.
*/

class Opt_hints_qb : public Opt_hints
{
  uint select_number;     // SELECT_LEX number
  LEX_CSTRING sys_name;   // System QB name
  char buff[32];          // Buffer to hold sys name

  PT_qb_level_hint *subquery_hint, *semijoin_hint;
  // PT_qb_level_hint::contextualize sets subquery/semijoin_hint during parsing.
  friend class PT_qb_level_hint;

public:

  Opt_hints_qb(Opt_hints *opt_hints_arg,
               MEM_ROOT *mem_root_arg,
               uint select_number_arg);

  const LEX_CSTRING *get_print_name()
  {
    const LEX_CSTRING *str= Opt_hints::get_name();
    return str ? str : &sys_name;
  }

  /**
    Append query block hint.

    @param thd   pointer to THD object
    @param str   pointer to String object
  */
  void append_qb_hint(THD *thd, String *str)
  {
    if (get_name())
    {
      str->append(STRING_WITH_LEN("QB_NAME("));
      append_identifier(thd, str, get_name()->str, get_name()->length);
      str->append(STRING_WITH_LEN(") "));
    }
  }
  /**
    Append query block name.

    @param thd   pointer to THD object
    @param str   pointer to String object
  */
  virtual void append_name(THD *thd, String *str)
  {
    str->append(STRING_WITH_LEN("@"));
    append_identifier(thd, str, get_print_name()->str, get_print_name()->length);
  }

  virtual PT_hint *get_complex_hints(opt_hints_enum type);

  /**
    Function finds Opt_hints_table object corresponding to
    table alias in the query block and attaches corresponding
    key hint objects to appropriate KEY structures.

    @param table      Pointer to TABLE object
    @param alias      Table alias

    @return  pointer Opt_hints_table object if this object is found,
             NULL otherwise.
  */
  Opt_hints_table *adjust_table_hints(TABLE *table, const char *alias);

  /**
    Returns whether semi-join is enabled for this query block

    A SEMIJOIN hint will force semi-join regardless of optimizer_switch settings.
    A NO_SEMIJOIN hint will only turn off semi-join if the variant with no
    strategies is used.
    A SUBQUERY hint will turn off semi-join.
    If there is no SEMIJOIN/SUBQUERY hint, optimizer_switch setting determines
    whether SEMIJOIN is used.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch

    @return true if semijoin is enabled
  */
  bool semijoin_enabled(THD *thd) const;

  /**
    Returns bit mask of which semi-join strategies are enabled for this query
    block.

    @param opt_switches Bit map of strategies enabled by optimizer_switch

    @return Bit mask of strategies that are enabled
  */
  uint sj_enabled_strategies(uint opt_switches) const;

  /**
    Returns which subquery execution strategy has been specified by hints
    for this query block.

    @retval EXEC_MATERIALIZATION  Subquery Materialization should be used
    @retval EXEC_EXISTS In-to-exists execution should be used
    @retval EXEC_UNSPECIFIED No SUBQUERY hint for this query block
  */
  Item_exists_subselect::enum_exec_method subquery_strategy() const;
};


/**
  Table level hints.
*/

class Opt_hints_table : public Opt_hints
{
public:
  Mem_root_array<Opt_hints_key*, true> keyinfo_array;

  Opt_hints_table(const LEX_CSTRING *table_name_arg,
                  Opt_hints_qb *qb_hints_arg,
                  MEM_ROOT *mem_root_arg)
    : Opt_hints(table_name_arg, qb_hints_arg, mem_root_arg),
      keyinfo_array(mem_root_arg)
  { }

  /**
    Append table name.

    @param thd   pointer to THD object
    @param str   pointer to String object
  */
  virtual void append_name(THD *thd, String *str)
  {
    append_identifier(thd, str, get_name()->str, get_name()->length);
    get_parent()->append_name(thd, str);
  }
  /**
    Function sets correlation between key hint objects and
    appropriate KEY structures.

    @param table      Pointer to TABLE object
  */
  void adjust_key_hints(TABLE *table);
};


/**
  Key level hints.
*/

class Opt_hints_key : public Opt_hints
{
public:

  Opt_hints_key(const LEX_CSTRING *key_name_arg,
                Opt_hints_table *table_hints_arg,
                MEM_ROOT *mem_root_arg)
    : Opt_hints(key_name_arg, table_hints_arg, mem_root_arg)
  { }

  /**
    Append key name.

    @param thd   pointer to THD object
    @param str   pointer to String object
  */
  virtual void append_name(THD *thd, String *str)
  {
    get_parent()->append_name(thd, str);
    str->append(' ');
    append_identifier(thd, str, get_name()->str, get_name()->length);
  }
};


/**
  Returns key hint value if hint is specified, returns
  optimizer switch value if hint is not specified.

  @param thd               Pointer to THD object
  @param tab               Pointer to TABLE object
  @param keyno             Key number
  @param type_arg          Hint type
  @param optimizer_switch  Optimizer switch flag

  @return key hint value if hint is specified,
          otherwise optimizer switch value.
*/
bool hint_key_state(const THD *thd, const TABLE *table,
                    uint keyno, opt_hints_enum type_arg,
                    uint optimizer_switch);

/**
  Returns table hint value if hint is specified, returns
  optimizer switch value if hint is not specified.

  @param thd                Pointer to THD object
  @param tab                Pointer to TABLE object
  @param type_arg           Hint type
  @param optimizer_switch   Optimizer switch flag

  @return table hint value if hint is specified,
          otherwise optimizer switch value.
*/
bool hint_table_state(const THD *thd, const TABLE *table,
                      opt_hints_enum type_arg,
                      uint optimizer_switch);

#endif /* OPT_HINTS_INCLUDED */
