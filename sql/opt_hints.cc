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
#include "mysqld.h"        // table_alias_charset
#include "sql_lex.h"


/**
  Information about hints. Sould be
  synchronized with opt_hints_enum enum.

  Note: Hint name depends on hint state. 'NO_' prefix is added
  if appropriate hint state bit(see Opt_hints_map::hints) is not
  set. Depending on 'switch_state_arg' argument in 'parse tree
  object' constructors(see parse_tree_hints.[h,cc]) implementor
  can control wishful form of the hint name.
*/

struct st_opt_hint_info opt_hint_info[]=
{
  {"BKA", true, true},
  {"BNL", true, true},
  {"ICP", true, true},
  {"MRR", true, true},
  {"NO_RANGE_OPTIMIZATION", true, true},
  {"MAX_EXECUTION_TIME", false, false},
  {"QB_NAME", false, false},
  {"SEMIJOIN", false, false},
  {"SUBQUERY", false, false},
  {0, 0, 0}
};


/**
  Prefix for system generated query block name.
  Used in information warning in EXPLAIN oputput.
*/

const LEX_CSTRING sys_qb_prefix=  {"select#", 7};


/*
  Compare LEX_CSTRING objects.

  @param s     Pointer to LEX_CSTRING
  @param t     Pointer to LEX_CSTRING
  @param cs    Pointer to character set

  @return  0 if strings are equal
           1 if s is greater
          -1 if t is greater
*/

static int cmp_lex_string(const LEX_CSTRING *s,
                          const LEX_CSTRING *t,
                          const CHARSET_INFO *cs)
{
  return cs->coll->strnncollsp(cs,
                               (uchar *) s->str, s->length,
                               (uchar *) t->str, t->length, 0);
}


bool Opt_hints::get_switch(opt_hints_enum type_arg) const
{
  if (is_specified(type_arg))
    return hints_map.switch_on(type_arg);

  if (opt_hint_info[type_arg].check_upper_lvl)
    return parent->get_switch(type_arg);

  return false;
}


Opt_hints* Opt_hints::find_by_name(const LEX_CSTRING *name_arg,
                                   const CHARSET_INFO *cs) const
{
  for (uint i= 0; i < child_array.size(); i++)
  {
    const LEX_CSTRING *name= child_array[i]->get_name();
    if (name && !cmp_lex_string(name, name_arg, cs))
      return child_array[i];
  }
  return NULL;
}


void Opt_hints::print(THD *thd, String *str, enum_query_type query_type)
{
  for (uint i= 0; i < MAX_HINT_ENUM; i++)
  {
    opt_hints_enum hint= static_cast<opt_hints_enum>(i);
    /*
       If printing a normalized query, also unresolved hints will be printed.
       (This is needed by query rewrite plugins which request
       normalized form before resolving has been performed.)
    */
    if (is_specified(hint) &&
        (is_resolved() || query_type == QT_NORMALIZED_FORMAT))
    {
      append_hint_type(str, hint);
      str->append(STRING_WITH_LEN("("));
      append_name(thd, str);
      if (!opt_hint_info[i].switch_hint)
        get_complex_hints(hint)->append_args(thd, str);
      str->append(STRING_WITH_LEN(") "));
    }
  }

  for (uint i= 0; i < child_array.size(); i++)
    child_array[i]->print(thd, str, query_type);
}


void Opt_hints::append_hint_type(String *str, opt_hints_enum type)
{
  const char* hint_name= opt_hint_info[type].hint_name;
  if(!hints_map.switch_on(type))
    str->append(STRING_WITH_LEN("NO_"));
  str->append(hint_name);
}


void Opt_hints::print_warn_unresolved(THD *thd)
{
  String hint_name_str, hint_type_str;
  append_name(thd, &hint_name_str);

  for (uint i= 0; i < MAX_HINT_ENUM; i++)
  {
    if (is_specified(static_cast<opt_hints_enum>(i)))
    {
      hint_type_str.length(0);
      append_hint_type(&hint_type_str, static_cast<opt_hints_enum>(i));
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_UNRESOLVED_HINT_NAME,
                          ER_THD(thd, ER_UNRESOLVED_HINT_NAME),
                          hint_name_str.c_ptr_safe(),
                          hint_type_str.c_ptr_safe());
    }
  }
}


void Opt_hints::check_unresolved(THD *thd)
{
  if (!is_resolved())
    print_warn_unresolved(thd);

  if (!is_all_resolved())
  {
    for (uint i= 0; i < child_array.size(); i++)
      child_array[i]->check_unresolved(thd);
  }
}


PT_hint *Opt_hints_global::get_complex_hints(opt_hints_enum type)
{
  if (type == MAX_EXEC_TIME_HINT_ENUM)
    return max_exec_time;

  DBUG_ASSERT(0);
  return NULL;
}


Opt_hints_qb::Opt_hints_qb(Opt_hints *opt_hints_arg,
                           MEM_ROOT *mem_root_arg,
                           uint select_number_arg)
  : Opt_hints(NULL, opt_hints_arg, mem_root_arg),
    select_number(select_number_arg), subquery_hint(NULL), semijoin_hint(NULL)
{
  sys_name.str= buff;
  sys_name.length= my_snprintf(buff, sizeof(buff), "%s%lx",
                               sys_qb_prefix.str, select_number);
}


PT_hint *Opt_hints_qb::get_complex_hints(opt_hints_enum type)
{
  if (type == SEMIJOIN_HINT_ENUM)
    return semijoin_hint;

  if (type == SUBQUERY_HINT_ENUM)
    return subquery_hint;

  DBUG_ASSERT(0);
  return NULL;
}


Opt_hints_table *Opt_hints_qb::adjust_table_hints(TABLE *table,
                                                  const char *alias)
{
  const LEX_CSTRING str= { alias, strlen(alias) };
  Opt_hints_table *tab=
    static_cast<Opt_hints_table *>(find_by_name(&str, table_alias_charset));

  table->pos_in_table_list->opt_hints_qb= this;

  if (!tab)                            // Tables not found
    return NULL;

  tab->adjust_key_hints(table);
  return tab;
}


bool Opt_hints_qb::semijoin_enabled(THD *thd) const
{
  if (subquery_hint) // SUBQUERY hint disables semi-join
    return false;

  if (semijoin_hint)
  {
    // SEMIJOIN hint will always force semijoin regardless of optimizer_switch
    if (semijoin_hint->switch_on())
      return true;

    // NO_SEMIJOIN hint.  If strategy list is empty, do not use SEMIJOIN
    if (semijoin_hint->get_args() == 0)
      return false;

    // Fall through: NO_SEMIJOIN w/ strategies neither turns SEMIJOIN off nor on
  }

  return thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SEMIJOIN);
}


uint Opt_hints_qb::sj_enabled_strategies(uint opt_switches) const
{
  // Hints override switches
  if (semijoin_hint)
  {
    const uint strategies= semijoin_hint->get_args();
    if (semijoin_hint->switch_on())  // SEMIJOIN hint
      return (strategies == 0) ? opt_switches : strategies;

    // NO_SEMIJOIN hint. Hints and optimizer_switch both affect strategies
    return ~strategies & opt_switches;
  }

  return opt_switches;
}


Item_exists_subselect::enum_exec_method
Opt_hints_qb::subquery_strategy() const
{
  if (subquery_hint)
    return static_cast<Item_exists_subselect::enum_exec_method>
      (subquery_hint->get_args());

  return Item_exists_subselect::EXEC_UNSPECIFIED;
}


void Opt_hints_table::adjust_key_hints(TABLE *table)
{
  set_resolved();
  if (child_array_ptr()->size() == 0)  // No key level hints
  {
    get_parent()->incr_resolved_children();
    return;
  }

  /*
    Make sure that adjustement is done only once.
    Table has already been processed if keyinfo_array is not empty.
  */
  if (keyinfo_array.size())
    return;

  keyinfo_array.resize(table->s->keys, NULL);

  for (Opt_hints** hint= child_array_ptr()->begin();
       hint < child_array_ptr()->end(); ++hint) 
  {
    KEY *key_info= table->key_info;
    for (uint j= 0 ; j < table->s->keys ; j++, key_info++)
    {
      const LEX_CSTRING key_name= { key_info->name, strlen(key_info->name) };
      if (!cmp_lex_string((*hint)->get_name(), &key_name, system_charset_info))
      {
        (*hint)->set_resolved();
        keyinfo_array[j]= static_cast<Opt_hints_key *>(*hint);
        incr_resolved_children();
      }
    }
  }

  /*
   Do not increase number of resolved tables
   if there are unresolved key objects. It's
   important for check_unresolved() function.
  */
  if (is_all_resolved())
    get_parent()->incr_resolved_children();
}


/**
  Function returns hint value depending on
  the specfied hint level. If hint is specified
  on current level, current level hint value is
  returned, otherwise parent level hint is checked.
  
  @param hint              Pointer to the hint object
  @param parent_hint       Pointer to the parent hint object,
                           should never be NULL
  @param type_arg          hint type
  @param OUT ret_val       hint value depending on
                           what hint level is used

  @return true if hint is specified, false otherwise
*/

static bool get_hint_state(Opt_hints *hint,
                           Opt_hints *parent_hint,
                           opt_hints_enum type_arg,
                           bool *ret_val)
{
  DBUG_ASSERT(parent_hint);

  if (opt_hint_info[type_arg].switch_hint)
  {
    if (hint && hint->is_specified(type_arg))
    {
      *ret_val= hint->get_switch(type_arg);
      return true;
    }
    else if (opt_hint_info[type_arg].check_upper_lvl &&
             parent_hint->is_specified(type_arg))
    {
      *ret_val= parent_hint->get_switch(type_arg);
      return true;
    }
  }
  else
  {
    /* Complex hint, not implemented atm */
    DBUG_ASSERT(0);
  }
  return false;
}


bool hint_key_state(const THD *thd, const TABLE *table,
                    uint keyno, opt_hints_enum type_arg,
                    uint optimizer_switch)
{
  Opt_hints_table *table_hints= table->pos_in_table_list->opt_hints_table;

  /* Parent should always be initialized */
  if (table_hints && keyno != MAX_KEY)
  {
    Opt_hints_key *key_hints= table_hints->keyinfo_array.size() > 0 ?
      table_hints->keyinfo_array[keyno] : NULL;
    bool ret_val= false;
    if (get_hint_state(key_hints, table_hints, type_arg, &ret_val))
      return ret_val;
  }

  return thd->optimizer_switch_flag(optimizer_switch);
}


bool hint_table_state(const THD *thd, const TABLE *table,
                      opt_hints_enum type_arg,
                      uint optimizer_switch)
{
  TABLE_LIST *table_list= table->pos_in_table_list;
  if (table_list->opt_hints_qb)
  {
    bool ret_val= false;
    if (get_hint_state(table_list->opt_hints_table,
                       table_list->opt_hints_qb,
                       type_arg, &ret_val))
      return ret_val;
  }

  return thd->optimizer_switch_flag(optimizer_switch);
}
