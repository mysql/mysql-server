/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PARSE_TREE_NODES_INCLUDED
#define PARSE_TREE_NODES_INCLUDED

#include "my_global.h"
#include "parse_tree_helpers.h"      // PT_item_list
#include "parse_tree_hints.h"
#include "sp_head.h"                 // sp_head
#include "sql_class.h"               // THD
#include "sql_lex.h"                 // LEX
#include "sql_parse.h"               // add_join_natural
#include "sql_update.h"              // Sql_cmd_update
#include "sql_admin.h"               // Sql_cmd_shutdown etc.


template<enum_parsing_context Context> class PTI_context;


class PT_statement : public Parse_tree_node
{
public:
  virtual Sql_cmd *make_cmd(THD *thd)= 0;
};


class PT_select_lex : public Parse_tree_node
{
public:
  SELECT_LEX *value;
};


class PT_subselect : public PT_select_lex
{
  typedef PT_select_lex super;

  POS pos;
  PT_select_lex *query_expression_body;

public:
  explicit PT_subselect(const POS &pos,
                        PT_select_lex *query_expression_body_arg)
  : pos(pos), query_expression_body(query_expression_body_arg)
  {}
  
  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    LEX *lex= pc->thd->lex;
    if (!lex->expr_allows_subselect ||
       lex->sql_command == (int)SQLCOM_PURGE)
    {
      error(pc, pos);
      return true;
    }
    /* 
      we are making a "derived table" for the parenthesis
      as we need to have a lex level to fit the union 
      after the parenthesis, e.g. 
      (SELECT .. ) UNION ...  becomes 
      SELECT * FROM ((SELECT ...) UNION ...)
    */
    SELECT_LEX *child= lex->new_query(pc->select);
    if (child == NULL)
      return true;

    Parse_context inner_pc(pc->thd, child);
    if (query_expression_body->contextualize(&inner_pc))
      return true;

    lex->pop_context();
    pc->select->n_child_sum_items += child->n_sum_items;
    /*
      A subselect can add fields to an outer select. Reserve space for
      them.
    */
    pc->select->select_n_where_fields+= child->select_n_where_fields;
    pc->select->select_n_having_items+= child->select_n_having_items;
    value= query_expression_body->value;
    return false;
  }
};


class PT_order_expr : public Parse_tree_node, public ORDER
{
  typedef Parse_tree_node super;

public:
  PT_order_expr(Item *item_arg, bool is_asc)
  {
    item_ptr= item_arg;
    direction= is_asc ? ORDER::ORDER_ASC : ORDER::ORDER_DESC;
  }

  virtual bool contextualize(Parse_context *pc)
  {
    return super::contextualize(pc) || item_ptr->itemize(pc, &item_ptr);
  }
};


class PT_order_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

public:
  SQL_I_List<ORDER> value;

public:
  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
    for (ORDER *o= value.first; o != NULL; o= o->next)
    {
      if (static_cast<PT_order_expr *>(o)->contextualize(pc))
        return true;
    }
    return false;
  }

  void push_back(PT_order_expr *order)
  {
    order->item= &order->item_ptr;
    order->used_alias= false;
    order->used= 0;
    order->is_position= false;
    value.link_in_list(order, &order->next);
  }
};


class PT_gorder_list : public PT_order_list
{
  typedef PT_order_list super;

public:
  virtual bool contextualize(Parse_context *pc)
  {
    SELECT_LEX *sel= pc->select;
    if (sel->linkage != GLOBAL_OPTIONS_TYPE &&
        sel->olap != UNSPECIFIED_OLAP_TYPE &&
        (sel->linkage != UNION_TYPE || sel->braces))
    {
      my_error(ER_WRONG_USAGE, MYF(0),
               "CUBE/ROLLUP", "ORDER BY");
      return true;
    }

    return super::contextualize(pc);
  }
};


class PT_select_item_list : public PT_item_list
{
  typedef PT_item_list super;

public:
  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    pc->select->item_list= value;
    return false;
  }
};


class PT_limit_clause : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Limit_options limit_options;

public:
  PT_limit_clause(const Limit_options &limit_options_arg)
  : limit_options(limit_options_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    if (pc->select->master_unit()->is_union() && !pc->select->braces)
    {
      pc->select= pc->select->master_unit()->fake_select_lex;
      DBUG_ASSERT(pc->select != NULL);
    }

    if (limit_options.is_offset_first && limit_options.opt_offset != NULL &&
        limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
      return true;

    if (limit_options.limit->itemize(pc, &limit_options.limit))
      return true;

    if (!limit_options.is_offset_first && limit_options.opt_offset != NULL &&
        limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
      return true;

    pc->select->select_limit= limit_options.limit;
    pc->select->offset_limit= limit_options.opt_offset;
    pc->select->explicit_limit= true;

    pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
    return false;
  }
};


class PT_table_list : public Parse_tree_node
{
public:
  TABLE_LIST *value;
};


class PT_table_factor_table_ident : public PT_table_list
{
  typedef PT_table_list super;

  Table_ident *table_ident;
  List<String> *opt_use_partition;
  LEX_STRING *opt_table_alias;
  List<Index_hint> *opt_key_definition;

public:
  PT_table_factor_table_ident(Table_ident *table_ident_arg,
                              List<String> *opt_use_partition_arg,
                              LEX_STRING *opt_table_alias_arg,
                              List<Index_hint> *opt_key_definition_arg)
  : table_ident(table_ident_arg),
    opt_use_partition(opt_use_partition_arg),
    opt_table_alias(opt_table_alias_arg),
    opt_key_definition(opt_key_definition_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
    
    THD *thd= pc->thd;
    Yacc_state *yyps= &thd->m_parser_state->m_yacc;

    value= pc->select->add_table_to_list(thd, table_ident, opt_table_alias, 0,
                                         yyps->m_lock_type,
                                         yyps->m_mdl_type,
                                         opt_key_definition,
                                         opt_use_partition);
    if (value == NULL)
      return true;
    pc->select->add_joined_table(value);
    return false;
  }
};


enum PT_join_table_type
{
  JTT_NORMAL            = 0x01,
  JTT_STRAIGHT          = 0x02,
  JTT_NATURAL           = 0x04,
  JTT_LEFT              = 0x08,
  JTT_RIGHT             = 0x10,

  JTT_NATURAL_LEFT      = JTT_NATURAL | JTT_LEFT,
  JTT_NATURAL_RIGHT     = JTT_NATURAL | JTT_RIGHT
};


template<PT_join_table_type Type>
class PT_join_table : public Parse_tree_node
{
  typedef Parse_tree_node super;

protected:
  PT_table_list *tab1_node;
  POS join_pos;
  PT_table_list *tab2_node;

  TABLE_LIST *tr1;
  TABLE_LIST *tr2;


public:
  PT_join_table(PT_table_list *tab1_node_arg, const POS &join_pos_arg,
                PT_table_list *tab2_node_arg)
  : tab1_node(tab1_node_arg), join_pos(join_pos_arg), tab2_node(tab2_node_arg),
    tr1(NULL), tr2(NULL)
  {
    DBUG_ASSERT(dbug_exclusive_flags(JTT_NORMAL | JTT_STRAIGHT | JTT_NATURAL));
    DBUG_ASSERT(dbug_exclusive_flags(JTT_LEFT | JTT_RIGHT));
  }

#ifndef DBUG_OFF
  bool dbug_exclusive_flags(unsigned int mask)
  {
#ifdef __GNUC__
    return __builtin_popcount(Type & mask) <= 1;
#else
    return true;
#endif
  }
#endif

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || contextualize_tabs(pc))
      return true;

    if (Type & (JTT_LEFT | JTT_RIGHT))
    {
      if (Type & JTT_LEFT)
        tr2->outer_join|= JOIN_TYPE_LEFT;
      else
      {
        TABLE_LIST *inner_table= pc->select->convert_right_join();
        /* swap tr1 and tr2 */
        DBUG_ASSERT(inner_table == tr1);
        tr1= tr2;
        tr2= inner_table;
      }
    }

    if (Type & JTT_NATURAL)
      add_join_natural(tr1, tr2, NULL, pc->select);
    
    if (Type & JTT_STRAIGHT)
      tr2->straight= true;

    return false;
  }

protected:
  bool contextualize_tabs(Parse_context *pc)
  {
    if (tr1 != NULL)
      return false; // already done
      
    if (tab1_node->contextualize(pc) || tab2_node->contextualize(pc))
      return true;

    tr1= tab1_node->value;
    tr2= tab2_node->value;

    if (tr1 == NULL || tr2 == NULL)
    {
      error(pc, join_pos);
      return true;
    }
    return false;
  }
};


template<PT_join_table_type Type>
class PT_join_table_on : public PT_join_table<Type>
{
  typedef PT_join_table<Type> super;

  Item *on;

public:
  PT_join_table_on(PT_table_list *tab1_node_arg, const POS &join_pos_arg,
                   PT_table_list *tab2_node_arg, Item *on_arg)
  : super(tab1_node_arg, join_pos_arg, tab2_node_arg), on(on_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (this->contextualize_tabs(pc))
      return true;

    if (push_new_name_resolution_context(pc, this->tr1, this->tr2))
    {
      this->error(pc, this->join_pos);
      return true;
    }

    SELECT_LEX *sel= pc->select;
    sel->parsing_place= CTX_ON;

    if (super::contextualize(pc) || on->itemize(pc, &on))
      return true;
    DBUG_ASSERT(sel == pc->select);

    add_join_on(this->tr2, on);
    pc->thd->lex->pop_context();
    DBUG_ASSERT(sel->parsing_place == CTX_ON);
    sel->parsing_place= CTX_NONE;
    return false;
  }
};


template<PT_join_table_type Type>
class PT_join_table_using : public PT_join_table<Type>
{
  typedef PT_join_table<Type> super;

  List<String> *using_fields;

public:
  PT_join_table_using(PT_table_list *tab1_node_arg, const POS &join_pos_arg,
                      PT_table_list *tab2_node_arg,
                       List<String> *using_fields_arg)
  : super(tab1_node_arg, join_pos_arg, tab2_node_arg),
    using_fields(using_fields_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    add_join_natural(this->tr1, this->tr2, using_fields, pc->select);
    return false;
  }
};


class PT_table_ref_join_table : public PT_table_list
{
  typedef PT_table_list super;

  Parse_tree_node *join_table;

public:
  explicit PT_table_ref_join_table(Parse_tree_node *join_table_arg)
  : join_table(join_table_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || join_table->contextualize(pc))
      return true;

    value= pc->select->nest_last_join(pc->thd);
    return value == NULL;
  }
};


class PT_select_part2_derived : public Parse_tree_node
{
  typedef Parse_tree_node super;

  ulonglong opt_query_spec_options;
  PT_item_list *select_item_list;

public:
  PT_select_part2_derived(ulonglong opt_query_spec_options_arg,
                          PT_item_list *select_item_list_arg)
  : opt_query_spec_options(opt_query_spec_options_arg),
    select_item_list(select_item_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    SELECT_LEX *select= pc->select;

    select->parsing_place= CTX_SELECT_LIST;

    if (select->validate_base_options(thd->lex, opt_query_spec_options))
      return true;
    select->set_base_options(opt_query_spec_options);
    if (opt_query_spec_options & SELECT_HIGH_PRIORITY)
    {
      Yacc_state *yyps= &thd->m_parser_state->m_yacc;
      yyps->m_lock_type= TL_READ_HIGH_PRIORITY;
      yyps->m_mdl_type= MDL_SHARED_READ;
    }

    if (select_item_list->contextualize(pc))
      return true;
    DBUG_ASSERT(select == pc->select);

    // Ensure we're resetting parsing place of the right select
    DBUG_ASSERT(select->parsing_place == CTX_SELECT_LIST);
    select->parsing_place= CTX_NONE;
    return false;
  }
};


class PT_group : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_order_list *group_list;
  olap_type olap;

public:
  PT_group(PT_order_list *group_list_arg, olap_type olap_arg)
  : group_list(group_list_arg), olap(olap_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_order : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_order_list *order_list;

public:

  explicit PT_order(PT_order_list *order_list_arg)
  : order_list(order_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_procedure_analyse : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Proc_analyse_params params;

public:
  PT_procedure_analyse(const Proc_analyse_params &params_arg)
  : params(params_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
          
    THD *thd= pc->thd;
    LEX *lex= thd->lex;

    if (!lex->parsing_options.allows_select_procedure)
    {
      my_error(ER_VIEW_SELECT_CLAUSE, MYF(0), "PROCEDURE");
      return true;
    }

    if (lex->select_lex != pc->select)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", "subquery");
      return true;
    }

    lex->proc_analyse= &params;
    lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
    return false;
  }
};


class PT_order_or_limit_order : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_order *order;
  PT_limit_clause *opt_limit;

public:
  PT_order_or_limit_order(PT_order *order_arg, PT_limit_clause *opt_limit_arg)
  : order(order_arg), opt_limit(opt_limit_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    return super::contextualize(pc) || order->contextualize(pc) ||
           (opt_limit != NULL && opt_limit->contextualize(pc));
  }
};


class PT_union_order_or_limit : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Parse_tree_node *order_or_limit;

public:
  PT_union_order_or_limit(Parse_tree_node *order_or_limit_arg)
  : order_or_limit(order_or_limit_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    DBUG_ASSERT(pc->select->linkage != GLOBAL_OPTIONS_TYPE);
    SELECT_LEX *fake= pc->select->master_unit()->fake_select_lex;
    if (fake)
    {
      fake->no_table_names_allowed= true;
      pc->select= fake;
    }
    pc->thd->where= "global ORDER clause";

    if (order_or_limit->contextualize(pc))
      return true;

    pc->select->no_table_names_allowed= 0;
    pc->thd->where= "";
    return false;
  }
};


class PT_table_expression : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Parse_tree_node *opt_from_clause;
  Item *opt_where;
  PT_group *opt_group;
  Item *opt_having;
  PT_order *opt_order;
  PT_limit_clause *opt_limit;
  PT_procedure_analyse *opt_procedure_analyse;
  Select_lock_type opt_select_lock_type;

public:
  PT_table_expression(Parse_tree_node *opt_from_clause_arg,
                      Item *opt_where_arg,
                      PT_group *opt_group_arg,
                      Item *opt_having_arg,
                      PT_order *opt_order_arg,
                      PT_limit_clause *opt_limit_arg,
                      PT_procedure_analyse *opt_procedure_analyse_arg,
                      const Select_lock_type &opt_select_lock_type_arg)
   : opt_from_clause(opt_from_clause_arg),
     opt_where(opt_where_arg),
     opt_group(opt_group_arg),
     opt_having(opt_having_arg),
     opt_order(opt_order_arg),
     opt_limit(opt_limit_arg),
     opt_procedure_analyse(opt_procedure_analyse_arg),
     opt_select_lock_type(opt_select_lock_type_arg)
    {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        (opt_from_clause != NULL && opt_from_clause->contextualize(pc)) ||
        (opt_where != NULL && opt_where->itemize(pc, &opt_where)) ||
        (opt_group != NULL && opt_group->contextualize(pc)) ||
        (opt_having != NULL && opt_having->itemize(pc, &opt_having)))
      return true;

    pc->select->set_where_cond(opt_where);
    pc->select->set_having_cond(opt_having);

    if ((opt_order != NULL && opt_order->contextualize(pc)) ||
        (opt_limit != NULL && opt_limit->contextualize(pc)) ||
        (opt_procedure_analyse != NULL &&
         opt_procedure_analyse->contextualize(pc)))
      return true;

    /*
      @todo: explain should not affect how we construct the query data
      structure. Instead, consider to let lock_tables() adjust lock
      requests according to the explain flag.
    */
    if (opt_select_lock_type.is_set && !pc->thd->lex->is_explain())
    {
      pc->select->set_lock_for_tables(opt_select_lock_type.lock_type);
      pc->thd->lex->safe_to_cache_query=
        opt_select_lock_type.is_safe_to_cache_query;
    }
    return false;
  }
};


class PT_table_factor_select_sym : public PT_table_list
{
  typedef PT_table_list super;

  POS pos;
  PT_hint_list *opt_hint_list;
  Query_options select_options;
  PT_item_list *select_item_list;
  PT_table_expression *table_expression;

public:
  PT_table_factor_select_sym(const POS &pos,
                             PT_hint_list *opt_hint_list_arg,
                             Query_options select_options_arg,
                             PT_item_list *select_item_list_arg,
                             PT_table_expression *table_expression_arg)
  : pos(pos),
    opt_hint_list(opt_hint_list_arg),
    select_options(select_options_arg),
    select_item_list(select_item_list_arg),
    table_expression(table_expression_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_select_derived_union_select : public PT_table_list
{
  typedef PT_table_list super;

  PT_table_list *select_derived;
  Parse_tree_node *opt_union_order_or_limit;
  POS union_or_limit_pos;

public:
  PT_select_derived_union_select(PT_table_list *select_derived_arg,
                                 Parse_tree_node *opt_union_order_or_limit_arg,
                                 const POS &union_or_limit_pos_arg)
  : select_derived(select_derived_arg),
    opt_union_order_or_limit(opt_union_order_or_limit_arg),
    union_or_limit_pos(union_or_limit_pos_arg)
  {}


  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        select_derived->contextualize(pc) ||
        (opt_union_order_or_limit != NULL &&
         opt_union_order_or_limit->contextualize(pc)))
      return true;

    if (select_derived->value != NULL && opt_union_order_or_limit != NULL)
    {
      error(pc, union_or_limit_pos);
      return true;
    }

    value= select_derived->value;
    return false;
  }
};


class PT_select_derived_union_union : public PT_table_list
{
  typedef PT_table_list super;

  PT_table_list *select_derived_union;
  POS union_pos;
  bool is_distinct;
  PT_select_lex *query_specification;

public:
  
  PT_select_derived_union_union(PT_table_list *select_derived_union_arg,
                                const POS &union_pos_arg,
                                bool is_distinct_arg,
                                PT_select_lex *query_specification_arg)
  : select_derived_union(select_derived_union_arg),
    union_pos(union_pos_arg),
    is_distinct(is_distinct_arg),
    query_specification(query_specification_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || select_derived_union->contextualize(pc))
      return true;

    pc->select= pc->thd->lex->new_union_query(pc->select, is_distinct);
    if (pc->select == NULL)
      return true;

    if (query_specification->contextualize(pc))
      return true;

    /*
      Remove from the name resolution context stack the context of the
      last query block in the union.
     */
    pc->thd->lex->pop_context();

    if (select_derived_union->value != NULL)
    {
      error(pc, union_pos);
      return true;
    }
    value= NULL;
    return false;
  }
};


class PT_table_factor_parenthesis : public PT_table_list
{
  typedef PT_table_list super;

  PT_table_list *select_derived_union;
  LEX_STRING *opt_table_alias;
  POS alias_pos;

public:

  PT_table_factor_parenthesis(PT_table_list *select_derived_union_arg,
                               LEX_STRING *opt_table_alias_arg,
                               const POS &alias_pos_arg)
  : select_derived_union(select_derived_union_arg),
    opt_table_alias(opt_table_alias_arg),
    alias_pos(alias_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_derived_table_list : public PT_table_list
{
  typedef PT_table_list super;

  POS pos;
  PT_table_list *head;
  PT_table_list *tail;

public:
  PT_derived_table_list(const POS &pos,
                        PT_table_list *head_arg, PT_table_list *tail_arg)
  : pos(pos), head(head_arg), tail(tail_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        head->contextualize(pc) || tail->contextualize(pc))
      return true;

    if (head->value == NULL || tail->value == NULL)
    {
      error(pc, pos);
      return true;
    }
    value= tail->value;
    return false;
  }
};


class PT_select_derived : public PT_table_list
{
  typedef PT_table_list super;

  POS pos;
  PT_table_list *derived_table_list;

public:
   
  PT_select_derived(const POS &pos, PT_table_list *derived_table_list_arg)
  : pos(pos), derived_table_list(derived_table_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    SELECT_LEX * const outer_select= pc->select;

    if (outer_select->init_nested_join(pc->thd))
      return true;

    if (derived_table_list->contextualize(pc))
      return true;

    /*
      for normal joins, derived_table_list->value != NULL and
      end_nested_join() != NULL, for derived tables, both must equal NULL
    */

    value= outer_select->end_nested_join(pc->thd);

    if (value == NULL && derived_table_list->value != NULL)
    {
      error(pc, pos);
      return true;
    }

    if (derived_table_list->value == NULL && value != NULL)
    {
      error(pc, pos);
      return true;
    }
    return false;
  }
};


class PT_join_table_list : public PT_table_list
{
  typedef PT_table_list super;

  POS pos;
  PT_table_list *derived_table_list;

public:
  PT_join_table_list(const POS &pos, PT_table_list *derived_table_list_arg)
  : pos(pos), derived_table_list(derived_table_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || derived_table_list->contextualize(pc))
      return true;

    if (derived_table_list->value == NULL)
    {
      error(pc, pos);
      return true;
    }
    value= derived_table_list->value;
    return false;
  }
};


class PT_table_reference_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_join_table_list *join_table_list;

public:
  PT_table_reference_list(PT_join_table_list *join_table_list_arg)
  : join_table_list(join_table_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || join_table_list->contextualize(pc))
      return true;

    SELECT_LEX *sel= pc->select;
    sel->context.table_list=
      sel->context.first_name_resolution_table=
        sel->table_list.first;
    return false;
  }
};


class PT_query_specification_select : public PT_select_lex
{
  typedef Parse_tree_node super;

  PT_hint_list *opt_hint_list;
  PT_select_part2_derived *select_part2_derived;
  PT_table_expression *table_expression;

public:
  PT_query_specification_select(
    PT_hint_list *opt_hint_list_arg,
    PT_select_part2_derived *select_part2_derived_arg,
    PT_table_expression *table_expression_arg)
  : opt_hint_list(opt_hint_list_arg),
    select_part2_derived(select_part2_derived_arg),
    table_expression(table_expression_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || select_part2_derived->contextualize(pc))
      return true;

    // Parentheses carry no meaning here.
    pc->select->set_braces(false);

    if (table_expression->contextualize(pc))
      return true;

    value= pc->select->master_unit()->first_select();

    if (opt_hint_list != NULL && opt_hint_list->contextualize(pc))
      return true;

    return false;
  }
};


class PT_select_paren_derived : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_hint_list *opt_hint_list;
  PT_select_part2_derived *select_part2_derived;
  PT_table_expression *table_expression;

public:
  PT_select_paren_derived(PT_hint_list *opt_hint_list_arg,
                          PT_select_part2_derived *select_part2_derived_arg,
                          PT_table_expression *table_expression_arg)
  : opt_hint_list(opt_hint_list_arg),
    select_part2_derived(select_part2_derived_arg),
    table_expression(table_expression_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    pc->select->set_braces(true);

    if (select_part2_derived->contextualize(pc) ||
        table_expression->contextualize(pc))
      return true;

    if (setup_select_in_parentheses(pc->select))
      return true;

    if (opt_hint_list != NULL && opt_hint_list->contextualize(pc))
      return true;

    return false;
  }
};


class PT_query_specification_parenthesis : public PT_select_lex
{
  typedef PT_select_lex super;

  PT_select_paren_derived *select_paren_derived;
  Parse_tree_node *opt_union_order_or_limit;

public:
  PT_query_specification_parenthesis(
    PT_select_paren_derived *select_paren_derived_arg,
    Parse_tree_node *opt_union_order_or_limit_arg)
  : select_paren_derived(select_paren_derived_arg),
    opt_union_order_or_limit(opt_union_order_or_limit_arg)
  {}


  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        select_paren_derived->contextualize(pc) ||
        (opt_union_order_or_limit != NULL &&
         opt_union_order_or_limit->contextualize(pc)))
      return true;

    value= pc->select->master_unit()->first_select();
    return false;
  }
};


class PT_query_expression_body_union : public PT_select_lex
{
  typedef PT_select_lex super;

  POS pos;
  PT_select_lex *query_expression_body;
  bool is_distinct;
  PT_select_lex *query_specification;

public:
  PT_query_expression_body_union(const POS &pos,
                                 PT_select_lex *query_expression_body_arg,
                                 bool is_distinct_arg,
                                 PT_select_lex *query_specification_arg)
  : pos(pos),
    query_expression_body(query_expression_body_arg),
    is_distinct(is_distinct_arg),
    query_specification(query_specification_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || query_expression_body->contextualize(pc))
      return true;

    LEX *lex= pc->thd->lex;

    if (pc->select->linkage == GLOBAL_OPTIONS_TYPE)
    {
      error(pc, pos);
      return true;
    }
    pc->select= lex->new_union_query(pc->select, is_distinct);
    if (pc->select == NULL)
      return true;

    if (query_specification->contextualize(pc))
      return true;

    lex->pop_context();
    value= query_expression_body->value;
    return false;
  }
};


class PT_internal_variable_name : public Parse_tree_node
{
public:
  sys_var_with_base value;
};


class PT_internal_variable_name_1d : public PT_internal_variable_name
{
  typedef PT_internal_variable_name super;

  LEX_STRING ident;

public:
  PT_internal_variable_name_1d(const LEX_STRING &ident_arg)
  : ident(ident_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
    sp_variable *spv;

    value.var= NULL;
    value.base_name= ident;

    /* Best effort lookup for system variable. */
    if (!pctx || !(spv= pctx->find_variable(ident, false)))
    {
      /* Not an SP local variable */
      if (find_sys_var_null_base(thd, &value))
        return true;
    }
    else
    {
      /*
        Possibly an SP local variable (or a shadowed sysvar).
        Will depend on the context of the SET statement.
      */
    }
    return false;
  }
};


/**
  Parse tree node class for 2-dimentional variable names (example: @global.x)
*/
class PT_internal_variable_name_2d : public PT_internal_variable_name
{
  typedef PT_internal_variable_name super;

  POS pos;
  LEX_STRING ident1;
  LEX_STRING ident2;

public:
  PT_internal_variable_name_2d(const POS &pos,
                                const LEX_STRING &ident1_arg,
                                const LEX_STRING &ident2_arg)
  : pos(pos), ident1(ident1_arg), ident2(ident2_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_internal_variable_name_default : public PT_internal_variable_name
{
  typedef PT_internal_variable_name super;

  LEX_STRING ident;

public:
  PT_internal_variable_name_default(const LEX_STRING &ident_arg)
  : ident(ident_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    sys_var *tmp=find_sys_var(pc->thd, ident.str, ident.length);
    if (!tmp)
      return true;
    if (!tmp->is_struct())
    {
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), ident.str);
      return true;
    }
    value.var= tmp;
    value.base_name.str=    (char*) "default";
    value.base_name.length= 7;
    return false;
  }
};


class PT_option_value_following_option_type : public Parse_tree_node
{
  typedef Parse_tree_node super;

  POS pos;
  PT_internal_variable_name *name;
  Item *opt_expr;

public:
  PT_option_value_following_option_type(const POS &pos,
                                        PT_internal_variable_name *name_arg,
                                        Item *opt_expr_arg)
  : pos(pos), name(name_arg), opt_expr(opt_expr_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || name->contextualize(pc) ||
        (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr)))
      return true;

    if (name->value.var && name->value.var != trg_new_row_fake_var)
    {
      /* It is a system variable. */
      if (set_system_variable(pc->thd, &name->value, pc->thd->lex->option_type,
                              opt_expr))
        return true;
    }
    else
    {
      /*
        Not in trigger assigning value to new row,
        and option_type preceding local variable is illegal.
      */
      error(pc, pos);
      return true;
    }
    return false;
  }
};


class PT_option_value_no_option_type : public Parse_tree_node {};


class PT_option_value_no_option_type_internal :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  PT_internal_variable_name *name;
  Item *opt_expr;
  POS expr_pos;

public:
  PT_option_value_no_option_type_internal(PT_internal_variable_name *name_arg,
                                          Item *opt_expr_arg,
                                          const POS &expr_pos_arg)
  : name(name_arg), opt_expr(opt_expr_arg), expr_pos(expr_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_option_value_no_option_type_user_var :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  LEX_STRING name;
  Item *expr;

public:
  PT_option_value_no_option_type_user_var(const LEX_STRING &name_arg,
                                          Item *expr_arg)
  : name(name_arg), expr(expr_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || expr->itemize(pc, &expr))
      return true;

    THD *thd= pc->thd;
    Item_func_set_user_var *item;
    item= new (pc->mem_root) Item_func_set_user_var(name, expr, false);
    if (item == NULL)
      return true;
    set_var_user *var= new set_var_user(item);
    if (var == NULL)
      return true;
    thd->lex->var_list.push_back(var);
    return false;
  }
};


class PT_option_value_no_option_type_sys_var :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  enum_var_type type;
  PT_internal_variable_name *name;
  Item *opt_expr;

public:
  PT_option_value_no_option_type_sys_var(enum_var_type type_arg,
                                          PT_internal_variable_name *name_arg,
                                          Item *opt_expr_arg)
  : type(type_arg), name(name_arg), opt_expr(opt_expr_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || name->contextualize(pc) ||
        (opt_expr != NULL && opt_expr->itemize(pc, &opt_expr)))
      return true;

    THD *thd= pc->thd;
    struct sys_var_with_base tmp= name->value;
    /* Lookup if necessary: must be a system variable. */
    if (tmp.var == NULL)
    {
      if (find_sys_var_null_base(thd, &tmp))
        return true;
    }
    if (set_system_variable(thd, &tmp, type, opt_expr))
      return true;
    return false;
  }
};


class PT_option_value_no_option_type_charset :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  const CHARSET_INFO *opt_charset;

public:
  PT_option_value_no_option_type_charset(const CHARSET_INFO *opt_charset_arg)
  : opt_charset(opt_charset_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    int flags= opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT;
    const CHARSET_INFO *cs2;
    cs2= opt_charset ? opt_charset
                     : global_system_variables.character_set_client;
    set_var_collation_client *var;
    var= new set_var_collation_client(flags,
                                      cs2,
                                      thd->variables.collation_database,
                                      cs2);
    if (var == NULL)
      return true;
    lex->var_list.push_back(var);
    return false;
  }
};


class PT_option_value_no_option_type_names :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  POS pos;

public:
  explicit PT_option_value_no_option_type_names(const POS &pos) : pos(pos) {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
    LEX_STRING names= { C_STRING_WITH_LEN("names") };

    if (pctx && pctx->find_variable(names, false))
      my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), names.str);
    else
      error(pc, pos);

    return true; // alwais fails with an error
  }
};


class PT_option_value_no_option_type_names_charset :
  public PT_option_value_no_option_type
{
  typedef PT_option_value_no_option_type super;

  const CHARSET_INFO *opt_charset;
  const CHARSET_INFO *opt_collation;

public:
  PT_option_value_no_option_type_names_charset(
    const CHARSET_INFO *opt_charset_arg,
    const CHARSET_INFO *opt_collation_arg)
  : opt_charset(opt_charset_arg), opt_collation(opt_collation_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    const CHARSET_INFO *cs2;
    const CHARSET_INFO *cs3;
    int flags= set_var_collation_client::SET_CS_NAMES
               | (opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT)
               | (opt_collation ? set_var_collation_client::SET_CS_COLLATE : 0);
    cs2= opt_charset ? opt_charset 
                     : global_system_variables.character_set_client;
    cs3= opt_collation ? opt_collation : cs2;
    if (!my_charset_same(cs2, cs3))
    {
      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
               cs3->name, cs2->csname);
      return true;
    }
    set_var_collation_client *var;
    var= new set_var_collation_client(flags, cs3, cs3, cs3);
    if (var == NULL)
      return true;
    lex->var_list.push_back(var);
    return false;
  }
};


class PT_start_option_value_list : public Parse_tree_node {};


class PT_option_value_no_option_type_password :
  public PT_start_option_value_list
{
  typedef PT_start_option_value_list super;

  const char *password;
  POS expr_pos;

public:
  explicit PT_option_value_no_option_type_password(const char *password_arg,
                                                   const POS &expr_pos_arg)
  : password(password_arg), expr_pos(expr_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_option_value_no_option_type_password_for :
  public PT_start_option_value_list 
{
  typedef PT_start_option_value_list super;

  LEX_USER *user;
  const char *password;
  POS expr_pos;

public:
  PT_option_value_no_option_type_password_for(LEX_USER *user_arg,
                                              const char *password_arg,
                                              const POS &expr_pos_arg)
  : user(user_arg), password(password_arg), expr_pos(expr_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    set_var_password *var;

    /*
      In case of anonymous user, user->user is set to empty string with
      length 0. But there might be case when user->user.str could be NULL.
      For Ex: "set password for current_user() = password('xyz');".
      In this case, set user information as of the current user.
    */
    if (!user->user.str)
    {
      LEX_CSTRING sctx_priv_user= thd->security_context()->priv_user();
      DBUG_ASSERT(sctx_priv_user.str);
      user->user.str= sctx_priv_user.str;
      user->user.length= sctx_priv_user.length;
    }
    if (!user->host.str)
    {
      LEX_CSTRING sctx_priv_host= thd->security_context()->priv_host();
      DBUG_ASSERT(sctx_priv_host.str);
      user->host.str= (char *) sctx_priv_host.str;
      user->host.length= sctx_priv_host.length;
    }

    var= new set_var_password(user, const_cast<char *>(password));
    if (var == NULL)
      return true;
    lex->var_list.push_back(var);
    lex->autocommit= TRUE;
    lex->is_set_password_sql= true;
    if (lex->sphead)
      lex->sphead->m_flags|= sp_head::HAS_SET_AUTOCOMMIT_STMT;
    if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end))
      return true;
    return false;
  }
};


class PT_option_value_type : public Parse_tree_node
{
  typedef Parse_tree_node super;

  enum_var_type type;
  PT_option_value_following_option_type *value;

public:
  PT_option_value_type(enum_var_type type_arg,
                        PT_option_value_following_option_type *value_arg)
  : type(type_arg), value(value_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    pc->thd->lex->option_type= type;
    return super::contextualize(pc) || value->contextualize(pc);
  }
};


class PT_option_value_list_head : public Parse_tree_node
{
  typedef Parse_tree_node super;

  POS delimiter_pos;
  Parse_tree_node *value;
  POS value_pos;

public:
  PT_option_value_list_head(const POS &delimiter_pos_arg,
                            Parse_tree_node *value_arg,
                            const POS &value_pos_arg)
  : delimiter_pos(delimiter_pos_arg), value(value_arg), value_pos(value_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
#ifndef DBUG_OFF
    LEX *old_lex= thd->lex;
#endif//DBUG_OFF

    sp_create_assignment_lex(thd, delimiter_pos.raw.end);
    DBUG_ASSERT(thd->lex->select_lex == thd->lex->current_select());
    Parse_context inner_pc(pc->thd, thd->lex->select_lex);

    if (value->contextualize(&inner_pc))
      return true;

    if (sp_create_assignment_instr(pc->thd, value_pos.raw.end))
      return true;
    DBUG_ASSERT(thd->lex == old_lex &&
                thd->lex->current_select() == pc->select);

    return false;
  }
};


class PT_option_value_list : public PT_option_value_list_head
{
  typedef PT_option_value_list_head super;

  PT_option_value_list_head *head;

public:
  PT_option_value_list(PT_option_value_list_head *head_arg,
                       const POS &delimiter_pos_arg,
                       Parse_tree_node *tail, const POS &tail_pos)
  : super(delimiter_pos_arg, tail, tail_pos), head(head_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    return head->contextualize(pc) || super::contextualize(pc);
  }
};


class PT_start_option_value_list_no_type : public PT_start_option_value_list
{
  typedef PT_start_option_value_list super;

  PT_option_value_no_option_type *head;
  POS head_pos;
  PT_option_value_list_head *tail;

public:
  PT_start_option_value_list_no_type(PT_option_value_no_option_type *head_arg,
                                     const POS &head_pos_arg,
                                     PT_option_value_list_head *tail_arg)
  : head(head_arg), head_pos(head_pos_arg), tail(tail_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || head->contextualize(pc))
      return true;

    if (sp_create_assignment_instr(pc->thd, head_pos.raw.end))
      return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select= pc->thd->lex->select_lex;

    if (tail != NULL && tail->contextualize(pc))
      return true;

    return false;
  }
};


class PT_transaction_characteristic : public Parse_tree_node
{
  typedef Parse_tree_node super;

  const char *name;
  int32 value;

public:
  PT_transaction_characteristic(const char *name_arg, int32 value_arg)
  : name(name_arg), value(value_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    Item *item= new (pc->mem_root) Item_int(value);
    if (item == NULL)
      return true;
    set_var *var= new set_var(lex->option_type,
                              find_sys_var(thd, name),
                              &null_lex_str,
                              item);
    if (var == NULL)
      return true;
    lex->var_list.push_back(var);
    return false;
  }

};


class PT_transaction_access_mode : public PT_transaction_characteristic
{
  typedef PT_transaction_characteristic super;

public:
  explicit PT_transaction_access_mode(bool is_read_only)
  : super("transaction_read_only", (int32) is_read_only)
  {}
};


class PT_isolation_level : public PT_transaction_characteristic
{
  typedef PT_transaction_characteristic super;

public:
  explicit PT_isolation_level(enum_tx_isolation level)
  : super("transaction_isolation", (int32) level)
  {}
};


class PT_transaction_characteristics : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_transaction_characteristic *head;
  PT_transaction_characteristic *opt_tail;

public:
  PT_transaction_characteristics(PT_transaction_characteristic *head_arg,
                                 PT_transaction_characteristic *opt_tail_arg)
  : head(head_arg), opt_tail(opt_tail_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    return (super::contextualize(pc) || head->contextualize(pc) ||
            (opt_tail != NULL && opt_tail->contextualize(pc)));
  }
};


class PT_start_option_value_list_transaction :
  public PT_start_option_value_list
{
  typedef PT_start_option_value_list super;

  PT_transaction_characteristics * characteristics;
  POS end_pos;

public:
  PT_start_option_value_list_transaction(
    PT_transaction_characteristics * characteristics_arg,
    const POS &end_pos_arg)
  : characteristics(characteristics_arg), end_pos(end_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
  
    THD *thd= pc->thd;
    thd->lex->option_type= OPT_DEFAULT;
    if (characteristics->contextualize(pc))
      return true;

    if (sp_create_assignment_instr(thd, end_pos.raw.end))
      return true;
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select= pc->thd->lex->select_lex;

    return false;
  }
};


class PT_start_option_value_list_following_option_type :
  public Parse_tree_node
{};


class PT_start_option_value_list_following_option_type_eq :
  public PT_start_option_value_list_following_option_type
{
  typedef PT_start_option_value_list_following_option_type super;

  PT_option_value_following_option_type *head;
  POS head_pos;
  PT_option_value_list_head *opt_tail;

public:
  PT_start_option_value_list_following_option_type_eq(
    PT_option_value_following_option_type *head_arg,
    const POS &head_pos_arg,
    PT_option_value_list_head *opt_tail_arg)
  : head(head_arg), head_pos(head_pos_arg), opt_tail(opt_tail_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || head->contextualize(pc))
      return true;

    if (sp_create_assignment_instr(pc->thd, head_pos.raw.end))
      return true; 
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select= pc->thd->lex->select_lex;

    if (opt_tail != NULL && opt_tail->contextualize(pc))
      return true;

    return false;
  }
};


class PT_start_option_value_list_following_option_type_transaction :
  public PT_start_option_value_list_following_option_type
{
  typedef PT_start_option_value_list_following_option_type super;

  PT_transaction_characteristics *characteristics;
  POS characteristics_pos;

public:
  PT_start_option_value_list_following_option_type_transaction(
    PT_transaction_characteristics *characteristics_arg,
    const POS &characteristics_pos_arg)
  : characteristics(characteristics_arg),
    characteristics_pos(characteristics_pos_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) || characteristics->contextualize(pc))
      return true;

    if (sp_create_assignment_instr(pc->thd, characteristics_pos.raw.end))
      return true; 
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select= pc->thd->lex->select_lex;

    return false;
  }
};


class PT_start_option_value_list_type : public PT_start_option_value_list
{
  typedef PT_start_option_value_list super;

  enum_var_type type;
  PT_start_option_value_list_following_option_type *list;

public:
  PT_start_option_value_list_type(
    enum_var_type type_arg,
    PT_start_option_value_list_following_option_type *list_arg)
  : type(type_arg), list(list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    pc->thd->lex->option_type= type;
    return super::contextualize(pc) || list->contextualize(pc);
  }
};


class PT_set : public Parse_tree_node
{
  typedef Parse_tree_node super;

  POS set_pos;
  PT_start_option_value_list *list;

public:
  PT_set(const POS &set_pos_arg, PT_start_option_value_list *list_arg)
  : set_pos(set_pos_arg), list(list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
  
    THD *thd= pc->thd;
    LEX *lex= thd->lex;
    lex->sql_command= SQLCOM_SET_OPTION;
    lex->option_type= OPT_SESSION;
    lex->var_list.empty();
    lex->autocommit= false;

    sp_create_assignment_lex(thd, set_pos.raw.end);
    DBUG_ASSERT(pc->thd->lex->select_lex == pc->thd->lex->current_select());
    pc->select= pc->thd->lex->select_lex;

    return list->contextualize(pc);
  }
};


class PT_select_init : public Parse_tree_node {};


class PT_union_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  bool is_distinct;
  PT_select_init *select_init;

public:
  PT_union_list(bool is_distinct_arg, PT_select_init *select_init_arg)
  : is_distinct(is_distinct_arg), select_init(select_init_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
  
    pc->select= pc->thd->lex->new_union_query(pc->select, is_distinct);
    if (pc->select == NULL)
      return true;

    if (select_init->contextualize(pc))
      return true;
    /*
      Remove from the name resolution context stack the context of the
      last query block in the union.
    */
    pc->thd->lex->pop_context();
    return false;
  }
};


class PT_into_destination : public Parse_tree_node
{
  typedef Parse_tree_node super;

public:
  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    if (!pc->thd->lex->parsing_options.allows_select_into)
    {
      my_error(ER_VIEW_SELECT_CLAUSE, MYF(0), "INTO");
      return true;
    }
    return false;
  }
};


class PT_into_destination_outfile : public PT_into_destination
{
  typedef PT_into_destination super;

  const char *file_name;
  const CHARSET_INFO *charset;
  const Field_separators field_term;
  const Line_separators line_term;

public:
  PT_into_destination_outfile(const LEX_STRING &file_name_arg,
                              const CHARSET_INFO *charset_arg,
                              const Field_separators &field_term_arg,
                              const Line_separators &line_term_arg)
  : file_name(file_name_arg.str),
    charset(charset_arg),
    field_term(field_term_arg),
    line_term(line_term_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    LEX *lex= pc->thd->lex;
    lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
    if (!(lex->exchange= new sql_exchange(file_name, 0)) ||
        !(lex->result= new Query_result_export(lex->exchange)))
      return true;

    lex->exchange->cs= charset;
    lex->exchange->field.merge_field_separators(field_term);
    lex->exchange->line.merge_line_separators(line_term);
    return false;
  }
};


class PT_into_destination_dumpfile : public PT_into_destination
{
  typedef PT_into_destination super;

  const char *file_name;

public:
  explicit PT_into_destination_dumpfile(const LEX_STRING &file_name_arg)
  : file_name(file_name_arg.str)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    LEX *lex= pc->thd->lex;
    if (!lex->describe)
    {
      lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
      if (!(lex->exchange= new sql_exchange(file_name, 1)))
        return true;
      if (!(lex->result= new Query_result_dump(lex->exchange)))
        return true;
    }
    return false;
  }
};


class PT_select_var : public Parse_tree_node
{
public:
  const LEX_STRING name;

  explicit PT_select_var(const LEX_STRING &name_arg) : name(name_arg) {}

  virtual bool is_local() const { return false; }
  virtual uint get_offset() const { DBUG_ASSERT(0); return 0; }
};


class PT_select_sp_var : public PT_select_var
{
  typedef PT_select_var super;

  uint offset;

#ifndef DBUG_OFF
  /*
    Routine to which this Item_splocal belongs. Used for checking if correct
    runtime context is used for variable handling.
  */
  sp_head *sp;
#endif

public:
  PT_select_sp_var(const LEX_STRING &name_arg) : super(name_arg) {}

  virtual bool is_local() const { return true; }
  virtual uint get_offset() const { return offset; }

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    LEX *lex= pc->thd->lex;
#ifndef DBUG_OFF
    sp= lex->sphead;
#endif
    sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
    sp_variable *spv;

    if (!pctx || !(spv= pctx->find_variable(name, false)))
    {
      my_error(ER_SP_UNDECLARED_VAR, MYF(0), name.str);
      return true;
    }

    offset= spv->offset;
    
    return false;
  }
};


class PT_select_var_list : public PT_into_destination
{
  typedef PT_into_destination super;

public:
  List<PT_select_var> value;

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    List_iterator<PT_select_var> it(value);
    PT_select_var *var;
    while ((var= it++))
    {
      if (var->contextualize(pc))
        return true;
    }

    LEX * const lex= pc->thd->lex;
    if (lex->describe)
      return false;

    Query_dumpvar *dumpvar= new (pc->mem_root) Query_dumpvar;
    if (dumpvar == NULL)
      return true;

    dumpvar->var_list= value;
    lex->result= dumpvar;
    lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  
    return false;
  }

  bool push_back(PT_select_var *var) { return value.push_back(var); }
};


class PT_select_options_and_item_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  Query_options options;
  PT_item_list *item_list;

public:
  PT_select_options_and_item_list(const Query_options &options_arg,
                                  PT_item_list *item_list_arg)
  : options(options_arg), item_list(item_list_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    pc->select->parsing_place= CTX_SELECT_LIST;

    if (options.query_spec_options & SELECT_HIGH_PRIORITY)
    {
      Yacc_state *yyps= &pc->thd->m_parser_state->m_yacc;
      yyps->m_lock_type= TL_READ_HIGH_PRIORITY;
      yyps->m_mdl_type= MDL_SHARED_READ;
    }
    if (options.save_to(pc))
      return true;

    if (item_list->contextualize(pc))
      return true;

    // Ensure we're resetting parsing place of the right select
    DBUG_ASSERT(pc->select->parsing_place == CTX_SELECT_LIST);
    pc->select->parsing_place= CTX_NONE;
    return false;
  }
};


class PT_select_part2 : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_select_options_and_item_list *select_options_and_item_list;
  PT_into_destination *opt_into1;
  PT_table_reference_list *from_clause; // actually is optional (NULL) for DUAL
  Item *opt_where_clause;
  PT_group *opt_group_clause;
  Item *opt_having_clause;
  PT_order *opt_order_clause;
  PT_limit_clause *opt_limit_clause;
  PT_procedure_analyse *opt_procedure_analyse_clause;
  PT_into_destination *opt_into2;
  Select_lock_type opt_select_lock_type;

public:
  PT_select_part2(
    PT_select_options_and_item_list *select_options_and_item_list_arg,
    PT_into_destination *opt_into1_arg,
    PT_table_reference_list *from_clause_arg,
    Item *opt_where_clause_arg,
    PT_group *opt_group_clause_arg,
    Item *opt_having_clause_arg,
    PT_order *opt_order_clause_arg,
    PT_limit_clause *opt_limit_clause_arg,
    PT_procedure_analyse *opt_procedure_analyse_clause_arg,
    PT_into_destination *opt_into2_arg,
    const Select_lock_type &opt_select_lock_type_arg)
  : select_options_and_item_list(select_options_and_item_list_arg),
    opt_into1(opt_into1_arg),
    from_clause(from_clause_arg),
    opt_where_clause(opt_where_clause_arg),
    opt_group_clause(opt_group_clause_arg),
    opt_having_clause(opt_having_clause_arg),
    opt_order_clause(opt_order_clause_arg),
    opt_limit_clause(opt_limit_clause_arg),
    opt_procedure_analyse_clause(opt_procedure_analyse_clause_arg),
    opt_into2(opt_into2_arg),
    opt_select_lock_type(opt_select_lock_type_arg)
  {}
  explicit PT_select_part2(
    PT_select_options_and_item_list *select_options_and_item_list_arg)
  : select_options_and_item_list(select_options_and_item_list_arg),
    opt_into1(NULL),
    from_clause(NULL),
    opt_where_clause(NULL),
    opt_group_clause(NULL),
    opt_having_clause(NULL),
    opt_order_clause(NULL),
    opt_limit_clause(NULL),
    opt_procedure_analyse_clause(NULL),
    opt_into2(NULL),
    opt_select_lock_type()
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        select_options_and_item_list->contextualize(pc) ||
        (opt_into1 != NULL &&
         opt_into1->contextualize(pc)) ||
        (from_clause != NULL &&
         from_clause->contextualize(pc)) ||
        (opt_where_clause != NULL &&
         opt_where_clause->itemize(pc, &opt_where_clause)) ||
        (opt_group_clause != NULL &&
         opt_group_clause->contextualize(pc)) ||
        (opt_having_clause != NULL &&
         opt_having_clause->itemize(pc, &opt_having_clause)))
      return true;

    pc->select->set_where_cond(opt_where_clause);
    pc->select->set_having_cond(opt_having_clause);

    if ((opt_order_clause != NULL &&
         opt_order_clause->contextualize(pc)) ||
        (opt_limit_clause != NULL &&
         opt_limit_clause->contextualize(pc)) ||
        (opt_procedure_analyse_clause != NULL &&
         opt_procedure_analyse_clause->contextualize(pc)) ||
        (opt_into2 != NULL &&
         opt_into2->contextualize(pc)))
      return true;

    DBUG_ASSERT(opt_into1 == NULL || opt_into2 == NULL);
    DBUG_ASSERT(opt_procedure_analyse_clause == NULL ||
                (opt_into1 == NULL && opt_into2 == NULL));

    /*
      @todo: explain should not affect how we construct the query data
      structure. Instead, consider to let lock_tables() adjust lock
      requests according to the explain flag.
    */
    if (opt_select_lock_type.is_set && !pc->thd->lex->is_explain())
    {
      pc->select->set_lock_for_tables(opt_select_lock_type.lock_type);
      pc->thd->lex->safe_to_cache_query=
        opt_select_lock_type.is_safe_to_cache_query;
    }
    return false;
  }
};


class PT_select_paren : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_hint_list *opt_hint_list;
  PT_select_part2 *select_part2;

public:
  PT_select_paren(PT_hint_list *opt_hint_list_arg,
                  PT_select_part2 *select_part2_arg)
  : opt_hint_list(opt_hint_list_arg), select_part2(select_part2_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    /*
      In order to correctly process UNION's global ORDER BY we need to
      set braces before parsing the clause.
    */
    pc->select->set_braces(true);

    if (select_part2->contextualize(pc))
      return true;

    if (setup_select_in_parentheses(pc->select))
      return true;

    if (opt_hint_list != NULL && opt_hint_list->contextualize(pc))
      return true;

    return false;
  }
};


class PT_select_init_parenthesis : public PT_select_init
{
  typedef PT_select_init super;

  PT_select_paren *select_paren;
  Parse_tree_node *union_opt;

public:
  PT_select_init_parenthesis(PT_select_paren *select_paren_arg,
                             Parse_tree_node *union_opt_arg)
  : select_paren(select_paren_arg), union_opt(union_opt_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    return (super::contextualize(pc) || select_paren->contextualize(pc) ||
            (union_opt != NULL && union_opt->contextualize(pc)));
  }
};


class PT_select_init2 : public PT_select_init
{
  typedef PT_select_init super;

  PT_hint_list *opt_hint_list;
  PT_select_part2 *select_part2;
  PT_union_list *opt_union_clause;

public:
  PT_select_init2(PT_hint_list *opt_hint_list_arg,
                  PT_select_part2 *select_part2_arg,
                  PT_union_list *opt_union_clause_arg)
  : opt_hint_list(opt_hint_list_arg),
    select_part2(select_part2_arg),
    opt_union_clause(opt_union_clause_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc) ||
        select_part2->contextualize(pc))
      return true;

    // Parentheses carry no meaning here.
    pc->select->set_braces(false);

    if (opt_hint_list != NULL && opt_hint_list->contextualize(pc))
      return true;

    if (opt_union_clause != NULL && opt_union_clause->contextualize(pc))
      return true;

    return false;
  }
};


class PT_select : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_select_init *select_init;
  enum_sql_command sql_command;

public:
  explicit PT_select(PT_select_init *select_init_arg,
                     enum_sql_command sql_command_arg)
  : select_init(select_init_arg), sql_command(sql_command_arg)
  {}

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;

    pc->thd->lex->sql_command= sql_command;

    if (select_init->contextualize(pc))
      return true;

    return false;
  }
};


class PT_delete : public PT_statement
{
  typedef PT_statement super;

  PT_hint_list *opt_hints;
  const int opt_delete_options;
  Table_ident *table_ident;
  Mem_root_array_YY<Table_ident *> table_list;
  List<String> *opt_use_partition;
  PT_join_table_list *join_table_list;
  Item *opt_where_clause;
  PT_order *opt_order_clause;
  Item *opt_delete_limit_clause;

public:
  // single-table DELETE node constructor:
  PT_delete(MEM_ROOT *mem_root,
            PT_hint_list *opt_hints_arg,
            int opt_delete_options_arg,
            Table_ident *table_ident_arg,
            List<String> *opt_use_partition_arg,
            Item *opt_where_clause_arg,
            PT_order *opt_order_clause_arg,
            Item *opt_delete_limit_clause_arg)
  : opt_hints(opt_hints_arg),
    opt_delete_options(opt_delete_options_arg),
    table_ident(table_ident_arg),
    opt_use_partition(opt_use_partition_arg),
    join_table_list(NULL),
    opt_where_clause(opt_where_clause_arg),
    opt_order_clause(opt_order_clause_arg),
    opt_delete_limit_clause(opt_delete_limit_clause_arg)
  {
    table_list.init(mem_root);
  }

  // multi-table DELETE node constructor:
  PT_delete(PT_hint_list *opt_hints_arg,
            int opt_delete_options_arg,
            const Mem_root_array_YY<Table_ident *> &table_list_arg,
            PT_join_table_list *join_table_list_arg,
            Item *opt_where_clause_arg)
  : opt_hints(opt_hints_arg),
    opt_delete_options(opt_delete_options_arg),
    table_ident(NULL),
    table_list(table_list_arg),
    opt_use_partition(NULL),
    join_table_list(join_table_list_arg),
    opt_where_clause(opt_where_clause_arg),
    opt_order_clause(NULL),
    opt_delete_limit_clause(NULL)
  {}

  virtual bool contextualize(Parse_context *pc);

  virtual Sql_cmd *make_cmd(THD *thd);

  bool is_multitable() const
  {
    DBUG_ASSERT((table_ident != NULL) ^ (table_list.size() > 0));
    return table_ident == NULL;
  }

private:
  bool add_table(Parse_context *pc, Table_ident *table);
};


class PT_update : public PT_statement
{
  typedef PT_statement super;

  PT_hint_list *opt_hints;
  thr_lock_type opt_low_priority;
  bool opt_ignore;
  PT_join_table_list *join_table_list;
  PT_item_list *column_list;
  PT_item_list *value_list;
  Item *opt_where_clause;
  PT_order *opt_order_clause;
  Item *opt_limit_clause;

  Sql_cmd_update sql_cmd;

public:
  PT_update(PT_hint_list *opt_hints_arg,
            thr_lock_type opt_low_priority_arg,
            bool opt_ignore_arg,
            PT_join_table_list *join_table_list_arg,
            PT_item_list *column_list_arg,
            PT_item_list *value_list_arg,
            Item *opt_where_clause_arg,
            PT_order *opt_order_clause_arg,
            Item *opt_limit_clause_arg)
  : opt_hints(opt_hints_arg),
    opt_low_priority(opt_low_priority_arg),
    opt_ignore(opt_ignore_arg),
    join_table_list(join_table_list_arg),
    column_list(column_list_arg),
    value_list(value_list_arg),
    opt_where_clause(opt_where_clause_arg),
    opt_order_clause(opt_order_clause_arg),
    opt_limit_clause(opt_limit_clause_arg)
  {}

  virtual bool contextualize(Parse_context *pc);

  virtual Sql_cmd *make_cmd(THD *thd);
};


class PT_create_select : public Parse_tree_node
{
  typedef Parse_tree_node super;

  PT_hint_list *opt_hints;
  Query_options options;
  PT_item_list *item_list;
  PT_table_expression *table_expression;

public:
  PT_create_select(PT_hint_list *opt_hints_arg,
                   const Query_options &options_arg,
                   PT_item_list *item_list_arg,
                   PT_table_expression *table_expression_arg)
  : opt_hints(opt_hints_arg),
    options(options_arg),
    item_list(item_list_arg),
    table_expression(table_expression_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_insert_values_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

  List<List_item> many_values;

public:
  virtual bool contextualize(Parse_context *pc);

  bool push_back(List<Item> *x) { return many_values.push_back(x); }

  virtual List<List_item> &get_many_values()
  {
    DBUG_ASSERT(is_contextualized());
    return many_values;
  }
};


class PT_insert_query_expression : public Parse_tree_node
{
  typedef Parse_tree_node super;

  bool braces;
  PT_create_select *create_select;
  Parse_tree_node * opt_union;

public:
  PT_insert_query_expression(bool braces_arg,
                             PT_create_select *create_select_arg,
                             Parse_tree_node * opt_union_arg)
  : braces(braces_arg),
    create_select(create_select_arg),
    opt_union(opt_union_arg)
  {}

  virtual bool contextualize(Parse_context *pc);
};


class PT_insert : public PT_statement
{
  typedef PT_statement super;

  const bool is_replace;
  PT_hint_list *opt_hints;
  const thr_lock_type lock_option;
  const bool ignore;
  Table_ident * const table_ident;
  List<String> * const opt_use_partition;
  PT_item_list * const column_list;
  PT_insert_values_list * const row_value_list;
  PT_insert_query_expression * const insert_query_expression;
  PT_item_list * const opt_on_duplicate_column_list;
  PT_item_list * const opt_on_duplicate_value_list;

public:
  PT_insert(bool is_replace_arg,
            PT_hint_list *opt_hints_arg,
            thr_lock_type lock_option_arg,
            bool ignore_arg,
            Table_ident *table_ident_arg,
            List<String> *opt_use_partition_arg,
            PT_item_list *column_list_arg,
	    PT_insert_values_list *row_value_list_arg,
            PT_insert_query_expression *insert_query_expression_arg,
            PT_item_list *opt_on_duplicate_column_list_arg,
            PT_item_list *opt_on_duplicate_value_list_arg)
  : is_replace(is_replace_arg),
    opt_hints(opt_hints_arg),
    lock_option(lock_option_arg),
    ignore(ignore_arg),
    table_ident(table_ident_arg),
    opt_use_partition(opt_use_partition_arg),
    column_list(column_list_arg),
    row_value_list(row_value_list_arg),
    insert_query_expression(insert_query_expression_arg),
    opt_on_duplicate_column_list(opt_on_duplicate_column_list_arg),
    opt_on_duplicate_value_list(opt_on_duplicate_value_list_arg)
  {
    // REPLACE statement can't have IGNORE flag:
    DBUG_ASSERT(!is_replace || !ignore);
    // REPLACE statement can't have ON DUPLICATE KEY UPDATE clause:
    DBUG_ASSERT(!is_replace || opt_on_duplicate_column_list == NULL);
    // INSERT/REPLACE ... SELECT can't have VALUES clause:
    DBUG_ASSERT((row_value_list != NULL) ^ (insert_query_expression != NULL));
    // ON DUPLICATE KEY UPDATE: column and value arrays must have same sizes:
    DBUG_ASSERT((opt_on_duplicate_column_list == NULL &&
                 opt_on_duplicate_value_list == NULL) ||
                (opt_on_duplicate_column_list->elements() ==
                 opt_on_duplicate_value_list->elements()));
  }

  virtual bool contextualize(Parse_context *pc);

  virtual Sql_cmd *make_cmd(THD *thd);

private:
  bool has_select() const { return insert_query_expression != NULL; }
};


class PT_shutdown : public PT_statement
{
  Sql_cmd_shutdown sql_cmd;

public:
  virtual Sql_cmd *make_cmd(THD *) { return &sql_cmd; }
};


class PT_alter_instance : public PT_statement
{
  typedef PT_statement super;

  Sql_cmd_alter_instance sql_cmd;

public:
  explicit PT_alter_instance(enum alter_instance_action_enum alter_instance_action)
    : sql_cmd(alter_instance_action)
  {}

  virtual Sql_cmd *make_cmd(THD *thd);
  virtual bool contextualize(Parse_context *pc);
};

#endif /* PARSE_TREE_NODES_INCLUDED */
