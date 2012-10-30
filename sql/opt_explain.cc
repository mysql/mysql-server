/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

/** @file "EXPLAIN <command>" implementation */ 

#include "opt_explain.h"
#include "sql_select.h"
#include "sql_optimizer.h" // JOIN
#include "sql_partition.h" // for make_used_partitions_str()
#include "sql_join_buffer.h" // JOIN_CACHE
#include "filesort.h"        // Filesort
#include "opt_explain_format.h"
#include "sql_base.h"      // lock_tables

typedef qep_row::extra extra;

static bool mysql_explain_unit(THD *thd, SELECT_LEX_UNIT *unit,
                               select_result *result);
static void propagate_explain_option(THD *thd, SELECT_LEX_UNIT *unit);

const char *join_type_str[]={ "UNKNOWN","system","const","eq_ref","ref",
			      "ALL","range","index","fulltext",
			      "ref_or_null","unique_subquery","index_subquery",
                              "index_merge"
};


/**
  A base for all Explain_* classes

  Explain_* classes collect and output EXPLAIN data.

  This class hierarchy is a successor of the old select_describe() function of 5.5.
*/

class Explain
{
protected:
  THD *const thd; ///< cached THD pointer
  const CHARSET_INFO *const cs; ///< cached pointer to system_charset_info
  JOIN *const join; ///< top-level JOIN (if any) provided by caller

  select_result *const external_result; ///< stream (if any) provided by caller

  Explain_format *const fmt; ///< shortcut for thd->lex->explain_format
  Explain_context_enum context_type; ///< associated value for struct. explain

  JOIN::ORDER_with_src order_list; //< ORDER BY item tree list
  JOIN::ORDER_with_src group_list; //< GROUP BY item tee list

protected:
  class Lazy_condition: public Lazy
  {
    Item *const condition;
  public:
    Lazy_condition(Item *condition_arg): condition(condition_arg) {}
    virtual bool eval(String *ret)
    {
      ret->length(0);
      if (condition)
        condition->print(ret, QT_ORDINARY);
      return false;
    }
  };

  explicit Explain(Explain_context_enum context_type_arg,
                   THD *thd_arg, JOIN *join_arg= NULL)
  : thd(thd_arg),
    cs(system_charset_info),
    join(join_arg),
    external_result(join ? join->result : NULL),
    fmt(thd->lex->explain_format),
    context_type(context_type_arg),
    order_list(NULL),
    group_list(NULL)
  {
    if (join)
    {
      order_list= join->order;
      group_list= join->group_list;
    }
    else
    {
      if (select_lex()->order_list.elements)
        order_list= JOIN::ORDER_with_src(select_lex()->order_list.first,
                                         ESC_ORDER_BY);
      if (select_lex()->group_list.elements)
        group_list= JOIN::ORDER_with_src(select_lex()->group_list.first,
                                         ESC_GROUP_BY);
    }
  }

public:
  virtual ~Explain() {}

  bool send();

protected:
  /**
    Explain everything but subqueries
  */
  virtual bool shallow_explain();
  /**
    Explain the rest of things after the @c shallow_explain() call
  */
  bool explain_subqueries(select_result *result);
  bool mark_subqueries(Item *item, qep_row *destination,
                       Explain_context_enum type);
  bool mark_order_subqueries(const JOIN::ORDER_with_src &order);
  bool prepare_columns();
  bool describe(uint8 mask) const { return thd->lex->describe & mask; }

  SELECT_LEX *select_lex() const
  {
    return join ? join->select_lex : &thd->lex->select_lex;
  }


  /**
    Prepare the self-allocated result object

    For queries with top-level JOIN the caller provides pre-allocated
    select_send object. Then that JOIN object prepares the select_send
    object calling result->prepare() in JOIN::prepare(),
    result->initalize_tables() in JOIN::optimize() and result->prepare2()
    in JOIN::exec().
    However without the presence of the top-level JOIN we have to
    prepare/initialize select_send object manually.
  */
  bool prepare(select_result *result)
  {
    DBUG_ASSERT(join == NULL);
    List<Item> dummy;
    return result->prepare(dummy, select_lex()->master_unit()) ||
           result->prepare2();
  }

  /**
    Push a part of the "extra" column into formatter

    Traditional formatter outputs traditional_extra_tags[tag] as is.
    Hierarchical formatter outputs a property with the json_extra_tags[tag] name
    and a boolean value of true.

    @param      tag     type of the "extra" part

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag)
  {
    extra *e= new extra(tag);
    return e == NULL || fmt->entry()->col_extra.push_back(e);
  }

  /**
    Push a part of the "extra" column into formatter

    @param      tag     type of the "extra" part
    @param      arg     for traditional formatter: rest of the part text,
                        for hierarchical format: string value of the property

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag, const String &arg)
  {
    if (arg.is_empty())
      return push_extra(tag);
    extra *e= new extra(tag, arg.dup(thd->mem_root));
    return !e || !e->data || fmt->entry()->col_extra.push_back(e);
  }

  /**
    Push a part of the "extra" column into formatter

    @param      tag     type of the "extra" part
    @param      arg     for traditional formatter: rest of the part text,
                        for hierarchical format: string value of the property

    NOTE: arg must be a long-living string constant.

    @retval     false   Ok
    @retval     true    Error (OOM)
  */
  bool push_extra(Extra_tag tag, const char *arg)
  {
    extra *e= new extra(tag, arg);
    return !e || fmt->entry()->col_extra.push_back(e);
  }

  /*
    Rest of the functions are overloadable functions, those calculate and fill
    "col_*" fields with Items for further sending as EXPLAIN columns.

    "explain_*" functions return false on success and true on error (usually OOM).
  */
  virtual bool explain_id();
  virtual bool explain_select_type();
  virtual bool explain_table_name() { return false; }
  virtual bool explain_partitions() { return false; }
  virtual bool explain_join_type() { return false; }
  virtual bool explain_possible_keys() { return false; }
  /** fill col_key and and col_key_len fields together */
  virtual bool explain_key_and_len() { return false; }
  virtual bool explain_ref() { return false; }
  /** fill col_rows and col_filtered fields together */
  virtual bool explain_rows_and_filtered() { return false; }
  virtual bool explain_extra() { return false; }
  virtual bool explain_modify_flags() { return false; }
};


/**
  Explain_no_table class outputs a trivial EXPLAIN row with "extra" column

  This class is intended for simple cases to produce EXPLAIN output
  with "No tables used", "No matching records" etc.
  Optionally it can output number of estimated rows in the "row"
  column.

  @note This class also produces EXPLAIN rows for inner units (if any).
*/

class Explain_no_table: public Explain
{
private:
  const char *message; ///< cached "message" argument
  const ha_rows rows; ///< HA_POS_ERROR or cached "rows" argument

public:
  Explain_no_table(THD *thd_arg, JOIN *join_arg, const char *message_arg)
  : Explain(CTX_JOIN, thd_arg, join_arg),
    message(message_arg), rows(HA_POS_ERROR)
  {}

  Explain_no_table(THD *thd_arg, const char *message_arg,
                   ha_rows rows_arg= HA_POS_ERROR)
  : Explain(CTX_JOIN, thd_arg),
    message(message_arg), rows(rows_arg)
  {}

protected:
  virtual bool shallow_explain();

  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
};


/**
  Explain_union_result class outputs EXPLAIN row for UNION
*/

class Explain_union_result : public Explain
{
public:
  Explain_union_result(THD *thd_arg, JOIN *join_arg)
  : Explain(CTX_UNION_RESULT, thd_arg, join_arg)
  {
    /* it's a UNION: */
    DBUG_ASSERT(join_arg->select_lex == join_arg->unit->fake_select_lex);
  }

protected:
  virtual bool explain_id();
  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_extra();
};



/**
  Common base class for Explain_join and Explain_table
*/

class Explain_table_base : public Explain {
protected:
  const TABLE *table;
  key_map usable_keys;

  Explain_table_base(Explain_context_enum context_type_arg,
                     THD *const thd_arg, JOIN *const join_arg)
  : Explain(context_type_arg, thd_arg, join_arg), table(NULL)
  {}

  Explain_table_base(Explain_context_enum context_type_arg,
                     THD *const thd_arg, TABLE *const table_arg)
  : Explain(context_type_arg, thd_arg), table(table_arg)
  {}

  virtual bool explain_partitions();
  virtual bool explain_possible_keys();

  bool explain_key_parts(int key, uint key_parts);
  bool explain_key_and_len_quick(const SQL_SELECT *select);
  bool explain_key_and_len_index(int key);
  bool explain_key_and_len_index(int key, uint key_length, uint key_parts);
  bool explain_extra_common(const SQL_SELECT *select,
                            const JOIN_TAB *tab,
                            int quick_type,
                            uint keyno);
  bool explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                     bool need_sort_arg);
  virtual bool explain_modify_flags();
};


/**
  Explain_join class produces EXPLAIN output for JOINs
*/

class Explain_join : public Explain_table_base
{
private:
  bool need_tmp_table; ///< add "Using temporary" to "extra" if true
  bool need_order; ///< add "Using filesort"" to "extra" if true
  const bool distinct; ///< add "Distinct" string to "extra" column if true

  uint tabnum; ///< current tab number in join->join_tab[]
  JOIN_TAB *tab; ///< current JOIN_TAB
  SQL_SELECT *select; ///< current SQL_SELECT
  int quick_type; ///< current quick type, see anon. enum at QUICK_SELECT_I
  table_map used_tables; ///< accumulate used tables bitmap

public:
  Explain_join(THD *thd_arg, JOIN *join_arg,
               bool need_tmp_table_arg, bool need_order_arg,
               bool distinct_arg)
  : Explain_table_base(CTX_JOIN, thd_arg, join_arg),
    need_tmp_table(need_tmp_table_arg),
    need_order(need_order_arg), distinct(distinct_arg),
    tabnum(0), select(0), used_tables(0)
  {
    /* it is not UNION: */
    DBUG_ASSERT(join_arg->select_lex != join_arg->unit->fake_select_lex);
  }

private:
  // Next 4 functions begin and end context for GROUP BY, ORDER BY and DISTINC
  bool begin_sort_context(Explain_sort_clause clause, Explain_context_enum ctx);
  bool end_sort_context(Explain_sort_clause clause, Explain_context_enum ctx);
  bool begin_simple_sort_context(Explain_sort_clause clause,
                                 Explain_context_enum ctx);
  bool end_simple_sort_context(Explain_sort_clause clause,
                               Explain_context_enum ctx);
  bool explain_join_tab(size_t tab_num);

protected:
  virtual bool shallow_explain();

  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_key_and_len();
  virtual bool explain_ref();
  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
  virtual bool explain_select_type();
  virtual bool explain_id();
};


/**
  Explain_table class produce EXPLAIN output for queries without top-level JOIN

  This class is a simplified version of the Explain_join class. It works in the
  context of queries which implementation lacks top-level JOIN object (EXPLAIN
  single-table UPDATE and DELETE).
*/

class Explain_table: public Explain_table_base
{
private:
  const SQL_SELECT *const select; ///< cached "select" argument
  const uint       key;        ///< cached "key" number argument
  const ha_rows    limit;      ///< HA_POS_ERROR or cached "limit" argument
  const bool       need_tmp_table; ///< cached need_tmp_table argument
  const bool       need_sort;  ///< cached need_sort argument
  const bool       is_update; // is_update ? UPDATE command : DELETE command
  const bool       used_key_is_modified; ///< UPDATE command updates used key

public:
  Explain_table(THD *const thd_arg, TABLE *const table_arg,
                const SQL_SELECT *select_arg,
                uint key_arg, ha_rows limit_arg,
                bool need_tmp_table_arg, bool need_sort_arg,
                bool is_update_arg, bool used_key_is_modified_arg)
  : Explain_table_base(CTX_JOIN, thd_arg, table_arg),
    select(select_arg), key(key_arg),
    limit(limit_arg),
    need_tmp_table(need_tmp_table_arg), need_sort(need_sort_arg),
    is_update(is_update_arg), used_key_is_modified(used_key_is_modified_arg)
  {
    usable_keys= table->keys_in_use_for_query;
  }

  virtual bool explain_modify_flags();

private:
  virtual bool explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                             bool need_sort_arg);
  virtual bool shallow_explain();

  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_key_and_len();
  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
};


static join_type calc_join_type(int quick_type)
{
  if ((quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION))
    return JT_INDEX_MERGE;
  else
    return JT_RANGE;
}


/* Explain class functions ****************************************************/


bool Explain::shallow_explain()
{
  return prepare_columns() || fmt->flush_entry();
}


/**
  Qualify subqueries with WHERE/HAVING/ORDER BY/GROUP BY clause type marker

  @param item           Item tree to find subqueries
  @param destination    For WHERE clauses
  @param type           Clause type

  @note WHERE clause belongs to TABLE or JOIN_TAB. The @c destination parameter
        provides a pointer to QEP data for such a table to associate a future
        subquery EXPLAIN output with table QEP provided.

  @retval false         OK
  @retval true          Error
*/

bool Explain::mark_subqueries(Item *item, qep_row *destination,
                              Explain_context_enum type)
{
  if (item == NULL || !fmt->is_hierarchical())
    return false;

  Explain_subquery_marker marker(destination, type);
  Explain_subquery_marker *marker_ptr= &marker;

  item->compile(&Item::explain_subquery_checker,
                reinterpret_cast<uchar **>(&marker_ptr),
                &Item::explain_subquery_propagator,
                NULL);
  return false;
}


bool Explain::mark_order_subqueries(const JOIN::ORDER_with_src &order)
{
  if (!order)
    return false;

  Explain_context_enum sq_context;
  switch (order.src) {
  case ESC_ORDER_BY:
    sq_context= CTX_ORDER_BY_SQ;
    break;
  case ESC_GROUP_BY:
    sq_context= CTX_GROUP_BY_SQ;
    break;
  case ESC_DISTINCT:
    // DISTINCT can't have subqueries, but we can get here when
    // DISTINCT is converted to GROUP BY
    return false;
  default:
    DBUG_ASSERT(0);
    return true;
  }
  for (const ORDER *o= order; o; o= o->next)
  {
    if (mark_subqueries(*o->item, NULL, sq_context))
      return true;
  }
  return false;
}

static bool explain_ref_key(Explain_format *fmt,
                            uint key_parts, store_key *key_copy[])
{
  if (key_parts == 0)
    return false;

  for (uint part_no= 0; part_no < key_parts; part_no++)
  {
    const store_key *const s_key= key_copy[part_no];
    if (s_key == NULL)
      continue;
    if (fmt->entry()->col_ref.push_back(s_key->name()))
      return true;
  }
  return false;
}


/**
  Traverses SQL clauses of this query specification to identify children
  subqueries, marks each of them with the clause they belong to.
  Then goes though all children subqueries and produces their EXPLAIN
  output, attached to the proper clause's context.

  @param        result  result stream

  @retval       false   Ok
  @retval       true    Error (OOM)
*/
bool Explain::explain_subqueries(select_result *result)
{
  if (join)
  {
    if (mark_subqueries(join->having, NULL, CTX_HAVING))
      return true;

    if (mark_order_subqueries(group_list))
      return true;

    if (!join->fields_list.is_empty())
    {
      List_iterator<Item> it(join->fields_list);
      Item *item;
      while ((item= it++))
      {
        if (mark_subqueries(item, NULL, CTX_SELECT_LIST))
          return true;
      }
    }
  }
  if (&thd->lex->select_lex == select_lex() &&
      !thd->lex->value_list.is_empty())
  {
    /*
      Collect subqueries from UPDATE ... SET foo=subquery and
      INSERT ... SELECT ... ON DUPLICATE KEY UPDATE x=(SELECT...)
    */
    DBUG_ASSERT(thd->lex->sql_command == SQLCOM_UPDATE ||
                thd->lex->sql_command == SQLCOM_UPDATE_MULTI ||
                thd->lex->sql_command == SQLCOM_INSERT ||
                thd->lex->sql_command == SQLCOM_INSERT_SELECT);
    List_iterator<Item> it(thd->lex->value_list);
    Item *item;
    while ((item= it++))
    {
      if (mark_subqueries(item, NULL, CTX_UPDATE_VALUE_LIST))
        return true;
    }
  }

  if (mark_order_subqueries(order_list))
    return true;

  for (SELECT_LEX_UNIT *unit= select_lex()->first_inner_unit();
       unit;
       unit= unit->next_unit())
  {
    SELECT_LEX *sl= unit->first_select();
    Explain_context_enum context;
    if (sl->type(thd) == SELECT_LEX::SLT_DERIVED)
    {
      DBUG_ASSERT(unit->explain_marker == CTX_NONE);
      context= CTX_DERIVED;
    }
    else if (unit->explain_marker == CTX_NONE)
      context= CTX_OPTIMIZED_AWAY_SUBQUERY;
    else
      context= static_cast<Explain_context_enum>(unit->explain_marker);

    if (fmt->begin_context(context, unit))
      return true;

    if (mysql_explain_unit(thd, unit, result))
      return true;

    /*
      This must be after mysql_explain_unit() so that JOIN::optimize() has run
      and had a chance to choose materialization.
    */
    if (fmt->is_hierarchical() && 
        (context == CTX_WHERE || context == CTX_HAVING ||
         context == CTX_SELECT_LIST ||
         context == CTX_GROUP_BY_SQ || context == CTX_ORDER_BY_SQ) &&
        unit->item &&
        (unit->item->get_engine_for_explain()->engine_type() ==
         subselect_engine::HASH_SJ_ENGINE))
    {
      fmt->entry()->is_materialized_from_subquery= true;
      fmt->entry()->col_table_name.set_const("<materialized_subquery>");
      fmt->entry()->using_temporary= true;
      fmt->entry()->col_join_type.set_const(join_type_str[JT_EQ_REF]);
      fmt->entry()->col_key.set_const("<auto_key>");

      const subselect_hash_sj_engine * const engine=
        static_cast<const subselect_hash_sj_engine *>
        (unit->item->get_engine_for_explain());
      const JOIN_TAB * const tmp_tab= engine->get_join_tab();

      char buff_key_len[24];
      fmt->entry()->col_key_len.set(buff_key_len,
                                    longlong2str(tmp_tab->table->key_info[0].key_length,
                                                 buff_key_len, 10) - buff_key_len);

      if (explain_ref_key(fmt, tmp_tab->ref.key_parts,
                          tmp_tab->ref.key_copy))
        return true;

      fmt->entry()->col_rows.set(1);
      /*
       The value to look up depends on the outer value, so the materialized
       subquery is dependent and not cacheable:
      */
      fmt->entry()->is_dependent= true;
      fmt->entry()->is_cacheable= false;
    }

    if (fmt->end_context(context))
      return true;
  }
  return false;
}


/**
  Pre-calculate table property values for further EXPLAIN output
*/
bool Explain::prepare_columns()
{
  return explain_id() ||
    explain_select_type() ||
    explain_table_name() ||
    explain_partitions() ||
    explain_join_type() ||
    explain_possible_keys() ||
    explain_key_and_len() ||
    explain_ref() ||
    explain_rows_and_filtered() ||
    explain_extra() ||
    explain_modify_flags();
}


/**
  Explain class main function

  This function:
    a) allocates a select_send object (if no one pre-allocated available),
    b) calculates and sends whole EXPLAIN data.

  @return false if success, true if error
*/

bool Explain::send()
{
  DBUG_ENTER("Explain::send");

  if (fmt->begin_context(context_type, NULL))
    DBUG_RETURN(true);

  /* Don't log this into the slow query log */
  thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                         SERVER_QUERY_NO_GOOD_INDEX_USED);

  select_result *result;
  if (external_result == NULL)
  {
    /* Create select_result object if the caller doesn't provide one: */
    if (!(result= new select_send))
      DBUG_RETURN(true); /* purecov: inspected */
    if (fmt->send_headers(result) || prepare(result))
      DBUG_RETURN(true);
  }
  else
  {
    result= external_result;
    external_result->reset_offset_limit_cnt();
  }

  for (SELECT_LEX_UNIT *unit= select_lex()->first_inner_unit();
       unit;
       unit= unit->next_unit())
    propagate_explain_option(thd, unit);

  bool ret= shallow_explain() || explain_subqueries(result);

  if (!ret)
    ret= fmt->end_context(context_type);

  if (ret && join)
    join->error= 1; /* purecov: inspected */

  if (external_result == NULL)
  {
    if (ret)
      result->abort_result_set();
    else
      result->send_eof();
    delete result;
  }

  DBUG_RETURN(ret);
}


bool Explain::explain_id()
{
  fmt->entry()->col_id.set(select_lex()->select_number);
  return false;
}


bool Explain::explain_select_type()
{
  if (&thd->lex->select_lex != select_lex()) // ignore top-level SELECT_LEXes
  {
    fmt->entry()->is_dependent= select_lex()->is_dependent();
    if (select_lex()->type(thd) != SELECT_LEX::SLT_DERIVED)
      fmt->entry()->is_cacheable= select_lex()->is_cacheable();
  }
  fmt->entry()->col_select_type.set(select_lex()->type(thd));
  return false;
}


/* Explain_no_table class functions *******************************************/


bool Explain_no_table::shallow_explain()
{
  return (fmt->begin_context(CTX_MESSAGE) ||
          Explain::shallow_explain() ||
          mark_subqueries(select_lex()->where, fmt->entry(), CTX_WHERE) ||
          fmt->end_context(CTX_MESSAGE));
}


bool Explain_no_table::explain_rows_and_filtered()
{
  if (rows == HA_POS_ERROR)
    return false;
  fmt->entry()->col_rows.set(rows);
  return false;
}


bool Explain_no_table::explain_extra()
{
  return fmt->entry()->col_message.set(message);
}


/* Explain_union_result class functions ****************************************/


bool Explain_union_result::explain_id()
{
  return false;
}


bool Explain_union_result::explain_table_name()
{
  SELECT_LEX *last_select= join->unit->first_select()->last_select();
  // # characters needed to print select_number of last select
  int last_length= (int)log10((double)last_select->select_number)+1;

  SELECT_LEX *sl= join->unit->first_select();
  uint len= 6, lastop= 0;
  char table_name_buffer[NAME_LEN];
  memcpy(table_name_buffer, STRING_WITH_LEN("<union"));
  /*
    - len + lastop: current position in table_name_buffer
    - 6 + last_length: the number of characters needed to print
      '...,'<last_select->select_number>'>\0'
  */
  for (;
       sl && len + lastop + 6 + last_length < NAME_CHAR_LEN;
       sl= sl->next_select())
  {
    len+= lastop;
    lastop= my_snprintf(table_name_buffer + len, NAME_CHAR_LEN - len,
                        "%u,", sl->select_number);
  }
  if (sl || len + lastop >= NAME_CHAR_LEN)
  {
    memcpy(table_name_buffer + len, STRING_WITH_LEN("...,"));
    len+= 4;
    lastop= my_snprintf(table_name_buffer + len, NAME_CHAR_LEN - len,
                        "%u,", last_select->select_number);
  }
  len+= lastop;
  table_name_buffer[len - 1]= '>';  // change ',' to '>'

  return fmt->entry()->col_table_name.set(table_name_buffer, len);
}


bool Explain_union_result::explain_join_type()
{
  fmt->entry()->col_join_type.set_const(join_type_str[JT_ALL]);
  return false;
}


bool Explain_union_result::explain_extra()
{
  if (!fmt->is_hierarchical())
  {
    /*
     Currently we always use temporary table for UNION result
    */
    if (push_extra(ET_USING_TEMPORARY))
      return true;
    /*
      here we assume that the query will return at least two rows, so we
      show "filesort" in EXPLAIN. Of course, sometimes we'll be wrong
      and no filesort will be actually done, but executing all selects in
      the UNION to provide precise EXPLAIN information will hardly be
      appreciated :)
    */
    if (join->unit->global_parameters->order_list.first)
    {
      return push_extra(ET_USING_FILESORT);
    }
  }
  return Explain::explain_extra();
}


/* Explain_table_base class functions *****************************************/


bool Explain_table_base::explain_partitions()
{
#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (!table->pos_in_table_list->derived && table->part_info)
    return make_used_partitions_str(table->part_info,
                                    &fmt->entry()->col_partitions);
#endif
  return false;
}


bool Explain_table_base::explain_possible_keys()
{
  if (usable_keys.is_clear_all())
    return false;

  for (uint j= 0 ; j < table->s->keys ; j++)
  {
    if (usable_keys.is_set(j) &&
        fmt->entry()->col_possible_keys.push_back(table->key_info[j].name))
      return true;
  }
  return false;
}


bool Explain_table_base::explain_key_parts(int key, uint key_parts)
{
  KEY_PART_INFO *kp= table->key_info[key].key_part;
  for (uint i= 0; i < key_parts; i++, kp++)
    if (fmt->entry()->col_key_parts.push_back(kp->field->field_name))
      return true;
  return false;
}


bool Explain_table_base::explain_key_and_len_quick(const SQL_SELECT *select)
{
  DBUG_ASSERT(select && select->quick);

  bool ret= false;
  StringBuffer<512> str_key(cs);
  StringBuffer<512> str_key_len(cs);

  if (select->quick->index != MAX_KEY)
    ret= explain_key_parts(select->quick->index,
                           select->quick->used_key_parts);
  select->quick->add_keys_and_lengths(&str_key, &str_key_len);
  return (ret || fmt->entry()->col_key.set(str_key) ||
          fmt->entry()->col_key_len.set(str_key_len));
}


bool Explain_table_base::explain_key_and_len_index(int key)
{
  DBUG_ASSERT(key != MAX_KEY);
  return explain_key_and_len_index(key, table->key_info[key].key_length,
                                   table->key_info[key].user_defined_key_parts);
}


bool Explain_table_base::explain_key_and_len_index(int key, uint key_length,
                                                   uint key_parts)
{
  DBUG_ASSERT(key != MAX_KEY);

  char buff_key_len[24];
  const KEY *key_info= table->key_info + key;
  const int length= longlong2str(key_length, buff_key_len, 10) - buff_key_len;
  const bool ret= explain_key_parts(key, key_parts);
  return (ret || fmt->entry()->col_key.set(key_info->name) ||
          fmt->entry()->col_key_len.set(buff_key_len, length));
}


bool Explain_table_base::explain_extra_common(const SQL_SELECT *select,
                                              const JOIN_TAB *tab,
                                              int quick_type,
                                              uint keyno)
{
  if (((keyno != MAX_KEY &&
        keyno == table->file->pushed_idx_cond_keyno &&
        table->file->pushed_idx_cond) ||
       (tab && tab->cache_idx_cond)))
  {
    StringBuffer<160> buff(cs);
    if (fmt->is_hierarchical())
    {
      if (table->file->pushed_idx_cond)
        table->file->pushed_idx_cond->print(&buff, QT_ORDINARY);
      else
        tab->cache_idx_cond->print(&buff, QT_ORDINARY);
    }
    if (push_extra(ET_USING_INDEX_CONDITION, buff))
    return true;
  }

  const TABLE* pushed_root= table->file->root_of_pushed_join();
  if (pushed_root)
  {
    char buf[128];
    int len;
    int pushed_id= 0;

    for (JOIN_TAB* prev= join->join_tab; prev <= tab; prev++)
    {
      const TABLE* prev_root= prev->table->file->root_of_pushed_join();
      if (prev_root == prev->table)
      {
        pushed_id++;
        if (prev_root == pushed_root)
          break;
      }
    }
    if (pushed_root == table)
    {
      uint pushed_count= tab->table->file->number_of_pushed_joins();
      len= my_snprintf(buf, sizeof(buf)-1,
                       "Parent of %d pushed join@%d",
                       pushed_count, pushed_id);
    }
    else
    {
      len= my_snprintf(buf, sizeof(buf)-1,
                       "Child of '%s' in pushed join@%d",
                       tab->table->file->parent_of_pushed_join()->alias,
                       pushed_id);
    }

    {
      StringBuffer<128> buff(cs);
      buff.append(buf,len);
      if (push_extra(ET_PUSHED_JOIN, buff))
        return true;
    }
  }

  switch (quick_type) {
  case QUICK_SELECT_I::QS_TYPE_ROR_UNION:
  case QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT:
  case QUICK_SELECT_I::QS_TYPE_INDEX_MERGE:
    {
      StringBuffer<32> buff(cs);
      select->quick->add_info_string(&buff);
      if (fmt->is_hierarchical())
      {
        /*
          We are replacing existing col_key value with a quickselect info,
          but not the reverse:
        */
        DBUG_ASSERT(fmt->entry()->col_key.length);
        if (fmt->entry()->col_key.set(buff)) // keep col_key_len intact
          return true;
      }
      else
      {
        if (push_extra(ET_USING, buff))
          return true;
      }
    }
    break;
  default: ;
  }

  if (select)
  {
    if (tab && tab->use_quick == QS_DYNAMIC_RANGE)
    {
      StringBuffer<64> str(STRING_WITH_LEN("index map: 0x"), cs);
      /* 4 bits per 1 hex digit + terminating '\0' */
      char buf[MAX_KEY / 4 + 1];
      str.append(tab->keys.print(buf));
      if (push_extra(ET_RANGE_CHECKED_FOR_EACH_RECORD, str))
        return true;
    }
    else if (select->cond)
    {
      const Item *pushed_cond= table->file->pushed_cond;

      if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
          pushed_cond)
      {
        StringBuffer<64> buff(cs);
        if (describe(DESCRIBE_EXTENDED))
          ((Item *)pushed_cond)->print(&buff, QT_ORDINARY);
        if (push_extra(ET_USING_WHERE_WITH_PUSHED_CONDITION, buff))
          return true;
      }
      else
      {
        if (fmt->is_hierarchical())
        {
          Lazy_condition *c= new Lazy_condition(tab && !tab->filesort ?
                                                tab->condition() :
                                                select->cond);
          if (c == NULL)
            return true;
          fmt->entry()->col_attached_condition.set(c);
        }
        else if (push_extra(ET_USING_WHERE))
          return true;
      }
    }
    else
      DBUG_ASSERT(!tab || !tab->condition());
  }
  if (table->reginfo.not_exists_optimize && push_extra(ET_NOT_EXISTS))
    return true;

  if (quick_type == QUICK_SELECT_I::QS_TYPE_RANGE)
  {
    uint mrr_flags=
      ((QUICK_RANGE_SELECT*)(select->quick))->mrr_flags;

    /*
      During normal execution of a query, multi_range_read_init() is
      called to initialize MRR. If HA_MRR_SORTED is set at this point,
      multi_range_read_init() for any native MRR implementation will
      revert to default MRR if not HA_MRR_SUPPORT_SORTED.
      Calling multi_range_read_init() can potentially be costly, so it
      is not done when executing an EXPLAIN. We therefore simulate
      its effect here:
    */
    if (mrr_flags & HA_MRR_SORTED && !(mrr_flags & HA_MRR_SUPPORT_SORTED))
      mrr_flags|= HA_MRR_USE_DEFAULT_IMPL;

    if (!(mrr_flags & HA_MRR_USE_DEFAULT_IMPL) && push_extra(ET_USING_MRR))
      return true;
  }
  return false;
}

bool Explain_table_base::explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                                       bool need_sort_arg)
{
  /*
    For hierarchical EXPLAIN we output "Using temporary" and
    "Using filesort" with related ORDER BY, GROUP BY or DISTINCT
  */
  if (fmt->is_hierarchical())
    return false; 

  if (need_tmp_table_arg && push_extra(ET_USING_TEMPORARY))
    return true;
  if (need_sort_arg && push_extra(ET_USING_FILESORT))
    return true;
  return false;
}


bool Explain_table_base::explain_modify_flags()
{
  if (!fmt->is_hierarchical())
      return false;
  switch (thd->lex->sql_command) {
  case SQLCOM_UPDATE_MULTI:
    if (!bitmap_is_clear_all(table->write_set) &&
        table->s->table_category != TABLE_CATEGORY_TEMPORARY)
      fmt->entry()->is_update= true;
    break;
  case SQLCOM_DELETE_MULTI:
    {
      TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
      for (TABLE_LIST *at= aux_tables; at; at= at->next_local)
      {
        if (at->table == table)
        {
          fmt->entry()->is_delete= true;
          break;
        }
      }
      break;
    }
  default: ;
  };
  return false;
}


/* Explain_join class functions ***********************************************/

bool Explain_join::begin_sort_context(Explain_sort_clause clause,
                                      Explain_context_enum ctx)
{
  const Explain_format_flags *flags= &join->explain_flags;
  return (flags->get(clause, ESP_EXISTS) &&
          !flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->begin_context(ctx, NULL, flags));
}


bool Explain_join::end_sort_context(Explain_sort_clause clause,
                                    Explain_context_enum ctx)
{
  const Explain_format_flags *flags= &join->explain_flags;
  return (flags->get(clause, ESP_EXISTS) &&
          !flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->end_context(ctx));
}


bool Explain_join::begin_simple_sort_context(Explain_sort_clause clause,
                                             Explain_context_enum ctx)
{
  const Explain_format_flags *flags= &join->explain_flags;
  return (flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->begin_context(ctx, NULL, flags));
}


bool Explain_join::end_simple_sort_context(Explain_sort_clause clause,
                                           Explain_context_enum ctx)
{
  const Explain_format_flags *flags= &join->explain_flags;
  return (flags->get(clause, ESP_IS_SIMPLE) &&
          fmt->end_context(ctx));
}


bool Explain_join::shallow_explain()
{
  if (begin_sort_context(ESC_ORDER_BY, CTX_ORDER_BY))
    return true;
  if (begin_sort_context(ESC_DISTINCT, CTX_DISTINCT))
    return true;
  if (begin_sort_context(ESC_GROUP_BY, CTX_GROUP_BY))
    return true;
  if (begin_sort_context(ESC_BUFFER_RESULT, CTX_BUFFER_RESULT))
    return true;

  for (size_t t= 0,
       cnt= fmt->is_hierarchical() ? join->primary_tables : join->tables;
       t < cnt; t++)
  {
    if (explain_join_tab(t))
      return true;
  }

  if (end_sort_context(ESC_BUFFER_RESULT, CTX_BUFFER_RESULT))
    return true;
  if (end_sort_context(ESC_GROUP_BY, CTX_GROUP_BY))
    return true;
  if (end_sort_context(ESC_DISTINCT, CTX_DISTINCT))
    return true;
  if (end_sort_context(ESC_ORDER_BY, CTX_ORDER_BY))
    return true;
    
  return false;
}


bool Explain_join::explain_join_tab(size_t tab_num)
{
  tabnum= tab_num;
  tab= join->join_tab + tabnum;
  table= tab->table;
  if (!tab->position)
    return false;
  usable_keys= tab->keys;
  quick_type= -1;
  select= (tab->filesort && tab->filesort->select) ?
           tab->filesort->select : tab->select;

  if (tab->type == JT_ALL && select && select->quick)
  {
    quick_type= select->quick->get_type();
    tab->type= calc_join_type(quick_type);
  }

  if (tab->starts_weedout())
    fmt->begin_context(CTX_DUPLICATES_WEEDOUT);

  const bool first_non_const= tabnum == join->const_tables;
  
  if (first_non_const)
  {
    if (begin_simple_sort_context(ESC_ORDER_BY, CTX_SIMPLE_ORDER_BY))
      return true;
    if (begin_simple_sort_context(ESC_DISTINCT, CTX_SIMPLE_DISTINCT))
      return true;
    if (begin_simple_sort_context(ESC_GROUP_BY, CTX_SIMPLE_GROUP_BY))
      return true;
  }

  Semijoin_mat_exec *sjm= tab->sj_mat_exec;
  Explain_context_enum c= sjm ? CTX_MATERIALIZATION : CTX_JOIN_TAB;

  if (fmt->begin_context(c) || prepare_columns())
    return true;
  
  fmt->entry()->query_block_id= table->pos_in_table_list->query_block_id();

  if (sjm)
  {
    if (sjm->is_scan)
    {
      fmt->entry()->col_rows.cleanup(); // TODO: set(something reasonable)
    }
    else
    {
      fmt->entry()->col_rows.set(1);
    }
  }

  if (fmt->flush_entry() ||
      mark_subqueries(tab->condition(), fmt->entry(), CTX_WHERE))
    return true;

  if (sjm && fmt->is_hierarchical())
  {
    for (size_t sjt= sjm->inner_table_index, end= sjt + sjm->table_count;
         sjt < end; sjt++)
    {
      if (explain_join_tab(sjt))
        return true;
    }
  }

  if (fmt->end_context(c))
    return true;

  if (first_non_const)
  {
    if (end_simple_sort_context(ESC_GROUP_BY, CTX_SIMPLE_GROUP_BY))
      return true;
    if (end_simple_sort_context(ESC_DISTINCT, CTX_SIMPLE_DISTINCT))
      return true;
    if (end_simple_sort_context(ESC_ORDER_BY, CTX_SIMPLE_ORDER_BY))
      return true;
  }

  if (tab->check_weed_out_table &&
      fmt->end_context(CTX_DUPLICATES_WEEDOUT))
    return true;

  used_tables|= table->map;

  return false;
}


bool Explain_join::explain_table_name()
{
  if (table->pos_in_table_list->derived && !fmt->is_hierarchical())
  {
    /* Derived table name generation */
    char table_name_buffer[NAME_LEN];
    const size_t len= my_snprintf(table_name_buffer,
                                  sizeof(table_name_buffer) - 1,
                                  "<derived%u>",
                                  table->pos_in_table_list->query_block_id());
    return fmt->entry()->col_table_name.set(table_name_buffer, len);
  }
  else
    return fmt->entry()->col_table_name.set(table->pos_in_table_list->alias);
}


bool Explain_join::explain_select_type()
{
  if (sj_is_materialize_strategy(tab->get_sj_strategy()))
    fmt->entry()->col_select_type.set(st_select_lex::SLT_MATERIALIZED);
  else
    return Explain::explain_select_type();
  return false;
}


bool Explain_join::explain_id()
{
  if (sj_is_materialize_strategy(tab->get_sj_strategy()))
    fmt->entry()->col_id.set(tab->sjm_query_block_id());
  else
    return Explain::explain_id();
  return false;
}


bool Explain_join::explain_join_type()
{
  fmt->entry()->col_join_type.set_const(join_type_str[tab->type]);
  return false;
}


bool Explain_join::explain_key_and_len()
{
  if (tab->ref.key_parts)
    return explain_key_and_len_index(tab->ref.key, tab->ref.key_length,
                                     tab->ref.key_parts);
  else if (tab->type == JT_INDEX_SCAN)
    return explain_key_and_len_index(tab->index);
  else if (select && select->quick)
    return explain_key_and_len_quick(select);
  else
  {
    const TABLE_LIST *table_list= table->pos_in_table_list;
    if (table_list->schema_table &&
        table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
    {
      StringBuffer<512> str_key(cs);
      const char *f_name;
      int f_idx;
      if (table_list->has_db_lookup_value)
      {
        f_idx= table_list->schema_table->idx_field1;
        f_name= table_list->schema_table->fields_info[f_idx].field_name;
        str_key.append(f_name, strlen(f_name), cs);
      }
      if (table_list->has_table_lookup_value)
      {
        if (table_list->has_db_lookup_value)
          str_key.append(',');
        f_idx= table_list->schema_table->idx_field2;
        f_name= table_list->schema_table->fields_info[f_idx].field_name;
        str_key.append(f_name, strlen(f_name), cs);
      }
      if (str_key.length())
        return fmt->entry()->col_key.set(str_key);
    }
  }
  return false;
}


bool Explain_join::explain_ref()
{
  return explain_ref_key(fmt, tab->ref.key_parts, tab->ref.key_copy);
}


bool Explain_join::explain_rows_and_filtered()
{
  if (table->pos_in_table_list->schema_table)
    return false;

  double examined_rows;
  if (select && select->quick)
    examined_rows= rows2double(select->quick->records);
  else if (tab->type == JT_INDEX_SCAN || tab->type == JT_ALL)
  {
    if (tab->limit)
      examined_rows= rows2double(tab->limit);
    else
    {
      table->pos_in_table_list->fetch_number_of_rows();
      examined_rows= rows2double(table->file->stats.records);
    }
  }
  else
    examined_rows= tab->position->records_read;

  fmt->entry()->col_rows.set(static_cast<longlong>(examined_rows));

  /* Add "filtered" field */
  if (describe(DESCRIBE_EXTENDED))
  {
    float f= 0.0;
    if (examined_rows)
      f= 100.0 * tab->position->records_read / examined_rows;
    fmt->entry()->col_filtered.set(f);
  }
  return false;
}


bool Explain_join::explain_extra()
{
  if (tab->info)
  {
    if (push_extra(tab->info))
      return true;
  }
  else if (tab->packed_info & TAB_INFO_HAVE_VALUE)
  {
    if (tab->packed_info & TAB_INFO_USING_INDEX)
    {
      if (push_extra(ET_USING_INDEX))
        return true;
    }
    if (tab->packed_info & TAB_INFO_USING_WHERE)
    {
      if (fmt->is_hierarchical())
      {
        Lazy_condition *c= new Lazy_condition(tab->condition());
        if (c == NULL)
          return true;
        fmt->entry()->col_attached_condition.set(c);
      }
      else if (push_extra(ET_USING_WHERE))
        return true;
    }
    if (tab->packed_info & TAB_INFO_FULL_SCAN_ON_NULL)
    {
      if (fmt->entry()->col_extra.push_back(new
                                            extra(ET_FULL_SCAN_ON_NULL_KEY)))
        return true;
    }
  }
  else
  {
    uint keyno= MAX_KEY;
    if (tab->ref.key_parts)
      keyno= tab->ref.key;
    else if (select && select->quick)
      keyno = select->quick->index;

    if (explain_extra_common(select, tab, quick_type, keyno))
      return true;

    const TABLE_LIST *table_list= table->pos_in_table_list;
    if (table_list->schema_table &&
        table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
    {
      if (!table_list->table_open_method)
      {
        if (push_extra(ET_SKIP_OPEN_TABLE))
          return true;
      }
      else if (table_list->table_open_method == OPEN_FRM_ONLY)
      {
        if (push_extra(ET_OPEN_FRM_ONLY))
          return true;
      }
      else
      {
        if (push_extra(ET_OPEN_FULL_TABLE))
          return true;
      }
      
      StringBuffer<32> buff(cs);
      if (table_list->has_db_lookup_value &&
          table_list->has_table_lookup_value)
      {
        if (push_extra(ET_SCANNED_DATABASES, "0"))
          return true;
      }
      else if (table_list->has_db_lookup_value ||
               table_list->has_table_lookup_value)
      {
        if (push_extra(ET_SCANNED_DATABASES, "1"))
          return true;
      }
      else
      {
        if (push_extra(ET_SCANNED_DATABASES, "all"))
          return true;
      }
    }
    if (((tab->type == JT_INDEX_SCAN || tab->type == JT_CONST) &&
         table->covering_keys.is_set(tab->index)) ||
        (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT &&
         !((QUICK_ROR_INTERSECT_SELECT*) select->quick)->need_to_fetch_row) ||
        table->key_read)
    {
      if (quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
      {
        QUICK_GROUP_MIN_MAX_SELECT *qgs=
          (QUICK_GROUP_MIN_MAX_SELECT *) select->quick;
        StringBuffer<64> buff(cs);
        qgs->append_loose_scan_type(&buff);
        if (push_extra(ET_USING_INDEX_FOR_GROUP_BY, buff))
          return true;
      }
      else
      {
        if (push_extra(ET_USING_INDEX))
          return true;
      }
    }

    if (explain_tmptable_and_filesort(need_tmp_table, need_order))
      return true;
    need_tmp_table= need_order= false;

    if (distinct && test_all_bits(used_tables, thd->lex->used_tables) &&
        push_extra(ET_DISTINCT))
      return true;

    if (tab->do_loosescan() && push_extra(ET_LOOSESCAN))
      return true;

    if (tab->starts_weedout())
    {
      if (!fmt->is_hierarchical() && push_extra(ET_START_TEMPORARY))
        return true;
    }
    if (tab->finishes_weedout())
    {
      if (!fmt->is_hierarchical() && push_extra(ET_END_TEMPORARY))
        return true;
    }
    else if (tab->do_firstmatch())
    {
      if (tab->firstmatch_return == join->join_tab - 1)
      {
        if (push_extra(ET_FIRST_MATCH))
          return true;
      }
      else
      {
        StringBuffer<64> buff(cs);
        TABLE *prev_table= tab->firstmatch_return->table;
        if (prev_table->pos_in_table_list->query_block_id() &&
            !fmt->is_hierarchical() &&
            prev_table->pos_in_table_list->derived)
        {
          char namebuf[NAME_LEN];
          /* Derived table name generation */
          int len= my_snprintf(namebuf, sizeof(namebuf)-1,
              "<derived%u>",
              prev_table->pos_in_table_list->query_block_id());
          buff.append(namebuf, len);
        }
        else
          buff.append(prev_table->pos_in_table_list->alias);
        if (push_extra(ET_FIRST_MATCH, buff))
          return true;
      }
    }

    if (tab->has_guarded_conds() && push_extra(ET_FULL_SCAN_ON_NULL_KEY))
      return true;

    if (tabnum > 0 && tab->use_join_cache != JOIN_CACHE::ALG_NONE)
    {
      StringBuffer<64> buff(cs);
      if ((tab->use_join_cache & JOIN_CACHE::ALG_BNL))
        buff.append("Block Nested Loop");
      else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA))
        buff.append("Batched Key Access");
      else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA_UNIQUE))
        buff.append("Batched Key Access (unique)");
      else
        DBUG_ASSERT(0); /* purecov: inspected */
      if (push_extra(ET_USING_JOIN_BUFFER, buff))
        return true;
    }
  }
  return false;
}


/* Explain_table class functions **********************************************/

bool Explain_table::explain_modify_flags()
{
  if (!fmt->is_hierarchical())
      return false;
  if (is_update)
    fmt->entry()->is_update= true;
  else
    fmt->entry()->is_delete= true;
  return false;
}


bool Explain_table::explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                                  bool need_sort_arg)
{
  if (fmt->is_hierarchical())
  {
    /*
      For hierarchical EXPLAIN we output "using_temporary_table" and
      "using_filesort" with related ORDER BY, GROUP BY or DISTINCT
      (excluding the single-table UPDATE command that updates used key --
      in this case we output "using_temporary_table: for update"
      at the "table" node)
    */
    if (need_tmp_table_arg)
    {
      DBUG_ASSERT(used_key_is_modified || order_list);
      if (used_key_is_modified && push_extra(ET_USING_TEMPORARY, "for update"))
        return true;
    }
  }
  else
  {
    if (need_tmp_table_arg && push_extra(ET_USING_TEMPORARY))
      return true;

    if (need_sort_arg && push_extra(ET_USING_FILESORT))
      return true;
  }

  return false;
}


bool Explain_table::shallow_explain()
{
  Explain_format_flags flags;
  if (order_list)
  {
    flags.set(ESC_ORDER_BY, ESP_EXISTS);
    if (need_sort)
      flags.set(ESC_ORDER_BY, ESP_USING_FILESORT);
    if (!used_key_is_modified && need_tmp_table)
      flags.set(ESC_ORDER_BY, ESP_USING_TMPTABLE);
  }

  if (order_list && fmt->begin_context(CTX_ORDER_BY, NULL, &flags))
    return true;

  if (fmt->begin_context(CTX_JOIN_TAB))
    return true;

  if (Explain::shallow_explain() ||
      mark_subqueries(select_lex()->where, fmt->entry(), CTX_WHERE))
    return true;

  if (fmt->end_context(CTX_JOIN_TAB))
    return true;

  if (order_list && fmt->end_context(CTX_ORDER_BY))
    return true;

  return false;
}


bool Explain_table::explain_table_name()
{
  return fmt->entry()->col_table_name.set(table->alias);
}


bool Explain_table::explain_join_type()
{
  join_type jt;
  if (select && select->quick)
    jt= calc_join_type(select->quick->get_type());
  else
    jt= JT_ALL;

  fmt->entry()->col_join_type.set_const(join_type_str[jt]);
  return false;
}


bool Explain_table::explain_key_and_len()
{
  if (select && select->quick)
    return explain_key_and_len_quick(select);
  else if (key != MAX_KEY)
    return explain_key_and_len_index(key);
  return false;
}


bool Explain_table::explain_rows_and_filtered()
{
  double examined_rows;
  if (select && select->quick)
    examined_rows= rows2double(select->quick->records);
  else if (!select && !need_sort && limit != HA_POS_ERROR)
    examined_rows= rows2double(limit);
  else
  {
    table->pos_in_table_list->fetch_number_of_rows();
    examined_rows= rows2double(table->file->stats.records);

  }
  fmt->entry()->col_rows.set(static_cast<long long>(examined_rows));

  if (describe(DESCRIBE_EXTENDED))
    fmt->entry()->col_filtered.set(100.0);
  
  return false;
}


bool Explain_table::explain_extra()
{
  const uint keyno= (select && select->quick) ? select->quick->index : key;
  const int quick_type= (select && select->quick) ? select->quick->get_type() 
                                                  : -1;
  return (explain_extra_common(select, NULL, quick_type, keyno) ||
          explain_tmptable_and_filesort(need_tmp_table, need_sort));
}


/**
  EXPLAIN functionality for insert_select, multi_update and multi_delete

  This class objects substitute insert_select, multi_update and multi_delete
  data interceptor objects to implement EXPLAIN for INSERT, REPLACE and
  multi-table UPDATE and DELETE queries.
  explain_send class object initializes tables like insert_select, multi_update
  or multi_delete data interceptor do, but it suppress table data modification
  by the underlying interceptor object.
  Thus, we can use explain_send object in the context of EXPLAIN INSERT/
  REPLACE/UPDATE/DELETE query like we use select_send in the context of
  EXPLAIN SELECT command:
    1) in presence of lex->describe flag we pass explain_send object to the
       mysql_select() function,
    2) it call prepare(), prepare2() and initialize_tables() functions to
       mark modified tables etc.

*/

class explain_send : public select_send {
protected:
  /*
    As far as we use explain_send object in a place of select_send, explain_send
    have to pass multiple invocation of its prepare(), prepare2() and
    initialize_tables() functions, since JOIN::exec() of subqueries runs
    these functions of select_send multiple times by design.
    insert_select, multi_update and multi_delete class functions are not intended
    for multiple invocations, so "prepared", "prepared2" and "initialized" flags
    guard data interceptor object from function re-invocation.
  */
  bool prepared;    ///< prepare() is done
  bool prepared2;   ///< prepare2() is done
  bool initialized; ///< initialize_tables() is done
  
  /**
    Pointer to underlying insert_select, multi_update or multi_delete object
  */
  select_result_interceptor *interceptor;

public:
  explain_send(select_result_interceptor *interceptor_arg)
  : prepared(false), prepared2(false), initialized(false),
    interceptor(interceptor_arg)
  {}

protected:
  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u)
  {
    if (prepared)
      return false;
    prepared= true;
    return select_send::prepare(list, u) || interceptor->prepare(list, u);
  }

  virtual int prepare2(void)
  {
    if (prepared2)
      return false;
    prepared2= true;
    return select_send::prepare2() || interceptor->prepare2();
  }

  virtual bool initialize_tables(JOIN *join)
  {
    if (initialized)
      return false;
    initialized= true;
    return select_send::initialize_tables(join) ||
           interceptor->initialize_tables(join);
  }

  virtual void cleanup()
  {
    select_send::cleanup();
    interceptor->cleanup();
  }
};


/******************************************************************************
  External function implementations
******************************************************************************/


/**
  Send a message as an "extra" column value

  This function forms the 1st row of the QEP output with a simple text message.
  This is useful to explain such trivial cases as "No tables used" etc.

  @note Also this function explains the rest of QEP (subqueries or joined
        tables if any).

  @param thd      current THD
  @param join     JOIN
  @param message  text message for the "extra" column.

  @return false if success, true if error
*/

bool explain_no_table(THD *thd, JOIN *join, const char *message)
{
  DBUG_ENTER("explain_no_table");
  const bool ret= Explain_no_table(thd, join, message).send();
  DBUG_RETURN(ret);
}


/**
  Send a message as an "extra" column value

  This function forms the 1st row of the QEP output with a simple text message.
  This is useful to explain such trivial cases as "No tables used" etc.

  @note Also this function explains the rest of QEP (subqueries if any).

  @param thd      current THD
  @param message  text message for the "extra" column.
  @param rows     HA_POS_ERROR or a value for the "rows" column.

  @return false if success, true if error
*/

bool explain_no_table(THD *thd, const char *message, ha_rows rows)
{
  DBUG_ENTER("explain_no_table");
  const bool ret= Explain_no_table(thd, message, rows).send();
  DBUG_RETURN(ret);
}


/**
  EXPLAIN handling for single-table UPDATE and DELETE queries

  Send to the client a QEP data set for single-table EXPLAIN UPDATE/DELETE
  queries. As far as single-table UPDATE/DELETE are implemented without
  the regular JOIN tree, we can't reuse explain_unit() directly,
  thus we deal with this single table in a special way and then call
  explain_unit() for subqueries (if any).

  @param thd            current THD
  @param table          TABLE object to update/delete rows in the UPDATE/DELETE
                        query.
  @param select         SQL_SELECT object that represents quick access functions
                        and WHERE clause.
  @param key            MAX_KEY or and index number of the key that was chosen
                        to access table data.
  @param limit          HA_POS_ERROR or LIMIT value.
  @param need_tmp_table true if it requires temporary table -- "Using temporary"
                        string in the "extra" column.
  @param need_sort      true if it requires filesort() -- "Using filesort"
                        string in the "extra" column.
  @param is_update      is_update ? UPDATE command : DELETE command
  @param used_key_is_modified   UPDATE updates used key column

  @return false if success, true if error
*/

bool explain_single_table_modification(THD *thd,
                                       TABLE *table,
                                       const SQL_SELECT *select,
                                       uint key,
                                       ha_rows limit,
                                       bool need_tmp_table,
                                       bool need_sort,
                                       bool is_update,
                                       bool used_key_is_modified)
{
  DBUG_ENTER("explain_single_table_modification");
  const bool ret= Explain_table(thd, table, select, key, limit,
                                need_tmp_table, need_sort, is_update,
                                used_key_is_modified).send();
  DBUG_RETURN(ret);
}


/**
  EXPLAIN handling for EXPLAIN SELECT queries

  Send QEP to the client.

  @param thd             current THD
  @param join            JOIN
  @param need_tmp_table  true if it requires a temporary table --
                         "Using temporary" string in the "extra" column.
  @param need_order      true if it requires filesort() -- "Using filesort"
                         string in the "extra" column.
  @param distinct        true if there is the DISTINCT clause (not optimized
                         out) -- "Distinct" string in the "extra" column.

  @return false if success, true if error
*/

bool explain_query_specification(THD *thd, JOIN *join)
{
  const Explain_format_flags *flags= &join->explain_flags;
  const bool need_tmp_table= flags->any(ESP_USING_TMPTABLE);
  const bool need_order= flags->any(ESP_USING_FILESORT);
  const bool distinct= flags->get(ESC_DISTINCT, ESP_EXISTS);

  DBUG_ENTER("explain_query_specification");
  DBUG_PRINT("info", ("Select %p, type %s",
                      join->select_lex, join->select_lex->get_type_str(thd)));
  bool ret;
  if (join->select_lex == join->unit->fake_select_lex)
    ret= Explain_union_result(thd, join).send();
  else
    ret= Explain_join(thd, join, need_tmp_table, need_order, distinct).send();
  DBUG_RETURN(ret);
}


/**
  EXPLAIN handling for INSERT, REPLACE and multi-table UPDATE/DELETE queries

  Send to the client a QEP data set for data-modifying commands those have a
  regular JOIN tree (INSERT...SELECT, REPLACE...SELECT and multi-table
  UPDATE and DELETE queries) like mysql_select() does for SELECT queries in
  the "describe" mode.

  @note see explain_single_table_modification() for single-table
        UPDATE/DELETE EXPLAIN handling.

  @note Unlike the mysql_select function, explain_multi_table_modification
        calls abort_result_set() itself in the case of failure (OOM etc.)
        since explain_multi_table_modification() uses internally created
        select_result stream.

  @param thd     current THD
  @param result  pointer to select_insert, multi_delete or multi_update object:
                 the function uses it to call result->prepare(),
                 result->prepare2() and result->initialize_tables() only but
                 not to modify table data or to send a result to client.
  @return false if success, true if error
*/

bool explain_multi_table_modification(THD *thd,
                                      select_result_interceptor *result)
{
  DBUG_ENTER("explain_multi_table_modification");
  explain_send explain(result);
  bool res= explain_query_expression(thd, &explain);
  DBUG_RETURN(res);
}


/**
  EXPLAIN handling for SELECT and table-modifying queries that have JOIN

  Send to the client a QEP data set for SELECT or data-modifying commands
  those have a regular JOIN tree (INSERT...SELECT, REPLACE...SELECT and
  multi-table UPDATE and DELETE queries) like mysql_select() does for SELECT
  queries in the "describe" mode.

  @note see explain_single_table_modification() for single-table
        UPDATE/DELETE EXPLAIN handling.

  @note explain_query_expression() calls abort_result_set() itself in the
        case of failure (OOM etc.) since explain_multi_table_modification()
        uses internally created select_result stream.

  @param thd     current THD
  @param result  pointer to select_result, select_insert, multi_delete or
                 multi_update object: the function uses it to call
                 result->prepare(), result->prepare2() and
                 result->initialize_tables() only but not to modify table data
                 or to send a result to client.
  @return false if success, true if error
*/

bool explain_query_expression(THD *thd, select_result *result)
{
  DBUG_ENTER("explain_query_expression");
  const bool res= thd->lex->explain_format->send_headers(result) ||
                  mysql_explain_unit(thd, &thd->lex->unit, result) ||
                  thd->is_error();
  /*
    The code which prints the extended description is not robust
    against malformed queries, so skip it if we have an error.
  */
  if (!res && (thd->lex->describe & DESCRIBE_EXTENDED) &&
      thd->lex->sql_command == SQLCOM_SELECT) // TODO: implement for INSERT/etc
  {
    StringBuffer<1024> str;
    /*
      The warnings system requires input in utf8, see mysqld_show_warnings().
    */
    thd->lex->unit.print(&str, enum_query_type(QT_TO_SYSTEM_CHARSET |
                                               QT_SHOW_SELECT_NUMBER));
    str.append('\0');
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE, ER_YES, str.ptr());
  }
  if (res)
    result->abort_result_set();
  else
    result->send_eof();
  DBUG_RETURN(res);
}


/**
  Set SELECT_DESCRIBE flag for all unit's SELECT_LEXes

  @param thd    THD
  @param unit   unit of SELECT_LEXes
*/
static void propagate_explain_option(THD *thd, SELECT_LEX_UNIT *unit)
{
  for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
    sl->options|= SELECT_DESCRIBE;
}


/**
  Explain UNION or subqueries of the unit

  If the unit is a UNION, explain it as a UNION. Otherwise explain nested
  subselects.

  @param thd            thread object
  @param unit           unit object
  @param result         result stream to send QEP dataset

  @return false if success, true if error
*/
bool mysql_explain_unit(THD *thd, SELECT_LEX_UNIT *unit, select_result *result)
{
  DBUG_ENTER("mysql_explain_unit");
  bool res= 0;

  propagate_explain_option(thd, unit);

  if (unit->is_union())
  {
    unit->fake_select_lex->select_number= UINT_MAX; // just for initialization
    unit->fake_select_lex->options|= SELECT_DESCRIBE;

    res= unit->prepare(thd, result, SELECT_NO_UNLOCK | SELECT_DESCRIBE);

    if (res)
      DBUG_RETURN(res);

    /*
      If tables are not locked at this point, it means that we have delayed
      this step until after prepare stage (now), in order to do better
      partition pruning.

      We need to lock tables now in order to proceed with the remaning
      stages of query optimization.
    */
    if (! thd->lex->is_query_tables_locked() &&
        lock_tables(thd, thd->lex->query_tables, thd->lex->table_count, 0))
      DBUG_RETURN(true);

    res= unit->optimize();

    if (!res)
      unit->explain();
  }
  else
  {
    SELECT_LEX *first= unit->first_select();
    thd->lex->current_select= first;
    unit->set_limit(unit->global_parameters);
    res= mysql_select(thd,
                      first->table_list.first,
                      first->with_wild, first->item_list,
                      first->where,
                      &first->order_list,
                      &first->group_list,
                      first->having,
                      first->options | thd->variables.option_bits | SELECT_DESCRIBE,
                      result, unit, first);
  }
  DBUG_RETURN(res || thd->is_error());
}


