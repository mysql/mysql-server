/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Implementation of name resolution stage


  @defgroup Query_Resolver  Query Resolver
  @{
*/

#include "sql_select.h"
#include "sql_resolver.h"
#include "sql_optimizer.h"
#include "opt_trace.h"
#include "sql_base.h"
#include "auth_common.h"
#include "opt_explain_format.h"
#include "sql_view.h"            // repoint_contexts_of_join_nests
#include "sql_test.h"            // print_where
#include "aggregate_check.h"

static const Item::enum_walk walk_subquery=
  Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY);

static void remove_redundant_subquery_clauses(st_select_lex *subq_select_lex,
                                              int hidden_group_field_count,
                                              int hidden_order_field_count,
                                              List<Item> &fields,
                                              Ref_ptr_array ref_pointer_array);
static inline int 
setup_without_group(THD *thd, Ref_ptr_array ref_pointer_array,
                    TABLE_LIST *tables,
                    List<Item> &fields,
                    List<Item> &all_fields,
                    ORDER *order,
                    ORDER *group,
                    int *hidden_group_field_count,
                    int *hidden_order_field_count);
static int
setup_group(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
            List<Item> &fields, List<Item> &all_fields, ORDER *order);
uint build_bitmap_for_nested_joins(List<TABLE_LIST> *join_list,
                                   uint first_unused);


/**
  Prepare of whole select (including sub queries in future).

  @todo
    Add check of calculation of GROUP functions and fields:
    SELECT COUNT(*)+table.col1 from table1;

  @retval
    -1   on error
  @retval
    0   on success
*/
int SELECT_LEX::prepare(JOIN *join)
{
  DBUG_ENTER("SELECT_LEX::prepare");

  // to prevent double initialization on EXPLAIN
  if (join->optimized)
    DBUG_RETURN(0);

  THD *const thd= join->thd;

  // We may do subquery transformation, or Item substitution:
  Prepare_error_tracker tracker(thd);

  {
    ORDER *const first_order= order_list.first;
    ORDER *const first_group= group_list.first;
    join->order= JOIN::ORDER_with_src(first_order, ESC_ORDER_BY);
    join->group_list= JOIN::ORDER_with_src(first_group, ESC_GROUP_BY);
    if (first_order)
      join->explain_flags.set(ESC_ORDER_BY, ESP_EXISTS);
    if (first_group)
      join->explain_flags.set(ESC_GROUP_BY, ESP_EXISTS);
    if (join->select_options & SELECT_DISTINCT)
      join->explain_flags.set(ESC_DISTINCT, ESP_EXISTS);
  }

  // Those members should still be un-initialized at this point:
  DBUG_ASSERT(join->where_cond == (Item*)1 &&
              join->having_cond == (Item*)1 &&
              join->having_for_explain == (Item*)1 &&
              join->tables_list == (TABLE_LIST*)1);

  join->select_lex= this;
  DBUG_ASSERT(this == thd->lex->current_select());
  set_join(join);
  SELECT_LEX_UNIT *const unit= master_unit();
  join->union_part= unit->is_union();

  is_item_list_lookup= 1;
  /*
    If we have already executed SELECT, then it have not sense to prevent
    its table from update (see unique_table())
  */
  if (thd->derived_tables_processing)
    exclude_from_table_unique_test= TRUE;

  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_prepare(trace, "join_preparation");
  trace_prepare.add_select_number(select_number);
  Opt_trace_array trace_steps(trace, "steps");

  /* Check that all tables, fields, conds and order are ok */

  if (!(join->select_options & OPTION_SETUP_TABLES_DONE) &&
      setup_tables_and_check_access(thd, &context, &top_join_list,
                                    get_table_list(), &leaf_tables,
                                    FALSE, SELECT_ACL, SELECT_ACL))
      DBUG_RETURN(-1);

  derived_table_count= 0;
  materialized_table_count= 0;
  partitioned_table_count= 0;
  leaf_table_count= 0;

  TABLE_LIST *table_ptr;
  for (table_ptr= leaf_tables;
       table_ptr;
       table_ptr= table_ptr->next_leaf)
  {
    leaf_table_count++;
    if (table_ptr->derived)
      derived_table_count++;
    if (table_ptr->uses_materialization())
      materialized_table_count++;
#ifdef WITH_PARTITION_STORAGE_ENGINE
    if (table_ptr->table->part_info)
      partitioned_table_count++;
#endif
  }
  // Primary input tables of the query:
  join->primary_tables= leaf_table_count;
  join->tables= join->primary_tables; // total number of tables, for now

  Mem_root_array<Item_exists_subselect *, true>
    sj_candidates_local(thd->mem_root);
  sj_candidates= &sj_candidates_local;

  /*
    Item and Item_field CTORs will both increment some counters
    in current_select(), based on the current parsing context.
    We are not parsing anymore: any new Items created now are due to
    query rewriting, so stop incrementing counters.
   */
  DBUG_ASSERT(parsing_place == CTX_NONE);
  parsing_place= CTX_NONE;

  resolve_place= RESOLVE_SELECT_LIST;
  if (setup_wild(thd, join->fields_list, &join->all_fields, with_wild))
    DBUG_RETURN(-1);
  if (setup_ref_array(thd))
    DBUG_RETURN(-1);

  join->ref_ptrs= join->ref_ptr_array_slice(0);

  if (setup_fields(thd, join->ref_ptrs, join->fields_list, MARK_COLUMNS_READ,
                   &join->all_fields, 1))
    DBUG_RETURN(-1);

  resolve_place= RESOLVE_NONE;

  int hidden_order_field_count;
  if (setup_without_group(thd, join->ref_ptrs, get_table_list(),
                          join->fields_list,
                          join->all_fields,
                          join->order, join->group_list,
                          &join->hidden_group_field_count,
                          &hidden_order_field_count))
    DBUG_RETURN(-1);

  /*
    Permanently remove redundant parts from the query if
      1) This is a subquery
      2) This is the first time this query is prepared (since the
         transformation is permanent)
      3) Not normalizing a view. Removal should take place when a
         query involving a view is optimized, not when the view
         is created
  */
  if (master_unit()->item &&                               // 1)
      first_execution &&                                   // 2)
      !(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW)) // 3)
  {
    remove_redundant_subquery_clauses(this, join->hidden_group_field_count,
                                      hidden_order_field_count,
                                      join->all_fields, join->ref_ptrs);
  }

  if (join->group_list || agg_func_used())
  {
    if (join->hidden_group_field_count == 0 &&
        olap == UNSPECIFIED_OLAP_TYPE)
    {
      /*
        All GROUP expressions are in SELECT list, so resulting rows are
        distinct. ROLLUP is not specified, so adds no row. So all rows in the
        result set are distinct, DISTINCT is useless.
        @todo could remove DISTINCT if ROLLUP were specified and all GROUP
        expressions were non-nullable, because ROLLUP adds only NULL
        values. Currently, ROLLUP+DISTINCT is rejected because executor
        cannot handle it in all cases.
      */
      options&= ~SELECT_DISTINCT;
      join->select_distinct= false;
    }
  }

  if (m_having_cond)
  {
    nesting_map save_allow_sum_func= thd->lex->allow_sum_func;
    thd->where="having clause";
    thd->lex->allow_sum_func|= (nesting_map)1 << nest_level;
    having_fix_field= 1;
    resolve_place= RESOLVE_HAVING;
    bool having_fix_rc= (!m_having_cond->fixed &&
                         (m_having_cond->
                          fix_fields(thd, &m_having_cond) ||
                          m_having_cond->check_cols(1)));
    having_fix_field= 0;

    resolve_place= RESOLVE_NONE;
    if (having_fix_rc || thd->is_error())
      DBUG_RETURN(-1);				/* purecov: inspected */
    thd->lex->allow_sum_func= save_allow_sum_func;
  }

  /*
    Printing the expanded query should happen here and not elsewhere, because
    when a view is merged (when the view is opened in open_tables()), the
    parent query's select_lex does not yet contain a correct WHERE clause (it
    misses the view's merged WHERE clause). This is corrected only just above,
    in TABLE_LIST::prep_where(), called by
    setup_without_group()->setup_conds().
    We also have to wait for fix_fields() on HAVING, above.
    At this stage, we also have properly set up Item_ref-s.
  */
  {
    Opt_trace_object trace_wrapper(trace);
    opt_trace_print_expanded_query(thd, this, &trace_wrapper);
  }

  /*
    When normalizing a view (like when writing a view's body to the FRM),
    subquery transformations don't apply (if they did, IN->EXISTS could not be
    undone in favour of materialization, when optimizing a later statement
    using the view)
  */
  if (master_unit()->item &&    // This is a subquery
                                            // Not normalizing a view
      !(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW))
  {
    /* Join object is a subquery within an IN/ANY/ALL/EXISTS predicate */
    if (resolve_subquery(thd))
      DBUG_RETURN(-1);
  }

  if (join->order)
  {
    bool real_order= FALSE;
    ORDER *ord;
    for (ord= join->order; ord; ord= ord->next)
    {
      Item *item= *ord->item;
      /*
        Disregard sort order if there's only 
        zero length NOT NULL fields (e.g. {VAR}CHAR(0) NOT NULL") or
        zero length NOT NULL string functions there.
        Such tuples don't contain any data to sort.
      */
      if (!real_order &&
           /* Not a zero length NOT NULL field */
          ((item->type() != Item::FIELD_ITEM ||
            ((Item_field *) item)->field->maybe_null() ||
            ((Item_field *) item)->field->sort_length()) &&
           /* AND not a zero length NOT NULL string function. */
           (item->type() != Item::FUNC_ITEM ||
            item->maybe_null ||
            item->result_type() != STRING_RESULT ||
            item->max_length)))
        real_order= TRUE;

      if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
        item->split_sum_func(thd, join->ref_ptrs, join->all_fields);
    }
    if (!real_order)
      join->order= NULL;
  }

  if (m_having_cond && m_having_cond->with_sum_func)
    m_having_cond->
      split_sum_func2(thd, join->ref_ptrs,
                      join->all_fields, &m_having_cond, TRUE);
  if (inner_sum_func_list)
  {
    Item_sum *end=inner_sum_func_list;
    Item_sum *item_sum= end;  
    do
    { 
      item_sum= item_sum->next;
      item_sum->split_sum_func2(thd, join->ref_ptrs,
                                join->all_fields, item_sum->ref_by, FALSE);
    } while (item_sum != end);
  }

  if (inner_refs_list.elements &&
      fix_inner_refs(thd, join->all_fields, this, join->ref_ptrs,
                     join->group_list))
    DBUG_RETURN(-1);

  if (join->group_list)
  {
    /*
      Because HEAP tables can't index BIT fields we need to use an
      additional hidden field for grouping because later it will be
      converted to a LONG field. Original field will remain of the
      BIT type and will be returned to a client.
    */
    for (ORDER *ord= join->group_list; ord; ord= ord->next)
    {
      if ((*ord->item)->type() == Item::FIELD_ITEM &&
          (*ord->item)->field_type() == MYSQL_TYPE_BIT)
      {
        Item_field *field= new Item_field(thd, *(Item_field**)ord->item);
        int el= join->all_fields.elements;
        join->ref_ptrs[el]= field;
        join->all_fields.push_front(field);
        ord->item= &join->ref_ptrs[el];
      }
    }
  }

  if (setup_ftfuncs(this)) /* should be after having->fix_fields */
    DBUG_RETURN(-1);

  {
    /* Caclulate the number of groups */
    join->send_group_parts= 0;
    for (ORDER *group_tmp= join->group_list ;
         group_tmp ;
         group_tmp= group_tmp->next)
      join->send_group_parts++;
  }

  if (join->result && join->result->prepare(join->fields_list, unit))
    DBUG_RETURN(-1); /* purecov: inspected */

  /* Init join struct */
  count_field_types(this, &join->tmp_table_param, join->all_fields, false,
                    false);
  join->group= join->group_list != 0;
  join->unit= unit;

  if (join->tmp_table_param.sum_func_count && !join->group_list)
  {
    join->implicit_grouping= TRUE;
    // Result will contain zero or one row - ordering is meaningless
    join->order= NULL;
  }

  if (olap == ROLLUP_TYPE && join->rollup_init())
    DBUG_RETURN(-1); /* purecov: inspected */
  if (join->alloc_func_list())
    DBUG_RETURN(-1); /* purecov: inspected */

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (partitioned_table_count)
  {
    for (TABLE_LIST *tbl= leaf_tables; tbl; tbl= tbl->next_leaf)
    {
      /* 
        This will only prune constant conditions, which will be used for
        lock pruning.
      */
      if (prune_partitions(thd, tbl->table,
                           tbl->join_cond() ? tbl->join_cond() :
                                              m_where_cond))
        DBUG_RETURN(-1); /* purecov: inspected */
    }
  }
#endif

  {
    uint count= leaf_table_count;
    if (flatten_subqueries())
      DBUG_RETURN(-1); /* purecov: inspected */
    count= leaf_table_count - count;
    join->tables+= count;
    join->primary_tables+= count;
    sj_candidates= NULL;
  }

  SELECT_LEX *const parent= outer_select();
  if (!parent || !parent->join)
  {
    /*
      We come here in several cases:
      - if this is the definition of a derived table
      - if this is the top query block of a SELECT or multi-table
      UPDATE/DELETE statement.
      - if this is one of highest-level subqueries, if the statement is
      something else; like subq-i in:
        UPDATE t1 SET col1=(subq-1), col2=(subq-2);

      Local transforms are applied after query block merging.
      This means that we avoid unecessary invocations, as local transforms
      would otherwise have been performed first before query block merging and
      then another time after query block merging.
      Thus, apply_local_transforms() may run only after the top query
      is finished with query block merging. That's why
      apply_local_transforms() is initiated only by the top query, and then
      recurses into subqueries.
    */
    if (apply_local_transforms())
      DBUG_RETURN(-1);
  }

  // At end of preparation, we only have primary tables.
  DBUG_ASSERT(join->tables == join->primary_tables);

  DBUG_RETURN(0); // All OK
}


bool SELECT_LEX::apply_local_transforms()
{
  DBUG_ENTER("SELECT_LEX::apply_local_transforms");
  if (!join)
  {
    /*
      If this is UNION, SELECT_LEX::prepare() for fake_select_lex (and thus
      its subqueries-in-ORDER-BY) may be delayed to
      st_select_lex_unit::optimize (see that function and also
      st_select_unit::prepare), thus join is NULL here for
      subqueries-in-ORDER-BY.
    */
    DBUG_RETURN(false);
  }
  for (SELECT_LEX_UNIT *unit= first_inner_unit();
       unit;
       unit= unit->next_unit())
  {
    for (SELECT_LEX *sl= unit->first_select();
         sl;
         sl= sl->next_select())
    {
      if (sl->apply_local_transforms())
        DBUG_RETURN(true);
    }
    if (unit->fake_select_lex &&
        unit->fake_select_lex->apply_local_transforms())
      DBUG_RETURN(true);
  }

  if (!first_execution)
      DBUG_RETURN(false);

  THD *const thd= join->thd;
  if (!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW))
  {
    /*
      The following code will allocate the new items in a permanent
      MEMROOT for prepared statements and stored procedures.
    */
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    /* Convert all outer joins to inner joins if possible */
    if (simplify_joins(thd, &top_join_list, true, false, &m_where_cond))
      DBUG_RETURN(true);
    if (record_join_nest_info(&top_join_list))
      DBUG_RETURN(true);
    build_bitmap_for_nested_joins(&top_join_list, 0);
  }

  /*
    Here are reasons why we do the following check here (i.e. late).
    * setup_fields () may have done split_sum_func () on aggregate items of
    the SELECT list, so for reliable comparison of the ORDER BY list with the
    SELECT list, we need to wait until split_sum_func() has been done on the
    ORDER BY list.
    * we get "most of the time" fixed items, which is always a good
    thing. Some outer references may not be fixed, though.
    * we need nested_join::used_tables, and this member is set in
    simplify_joins()
    * simplify_joins() does outer-join-to-inner conversion, which increases
    opportunities for functional dependencies (weak-to-strong, which is
    unusable, becomes strong-to-strong).

    The drawback is that the checks are after resolve_subquery(), so can meet
    strange "internally added" items.
  */
  if ((thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY) &&
      ((options & SELECT_DISTINCT) || group_list.elements ||
       agg_func_used()) &&
      check_only_full_group_by(thd))
    DBUG_RETURN(true);

  fix_prepare_information(thd);
  DBUG_RETURN(false);
}


/**
  Check if the subquery predicate can be executed via materialization.

  @param predicate IN subquery predicate
  @param thd       THD
  @param select_lex SELECT_LEX of the subquery
  @param outer      Parent SELECT_LEX (outer to subquery)

  @return TRUE if subquery allows materialization, FALSE otherwise.
*/

bool subquery_allows_materialization(Item_in_subselect *predicate,
                                     THD *thd,
                                     SELECT_LEX *select_lex,
                                     const SELECT_LEX *outer)
{
  bool has_nullables= false;
  const uint elements= predicate->unit->first_select()->item_list.elements;
  DBUG_ENTER("subquery_allows_materialization");
  DBUG_ASSERT(elements >= 1);
  DBUG_ASSERT(predicate->left_expr->cols() == elements);

  OPT_TRACE_TRANSFORM(&thd->opt_trace, trace_wrapper, trace_mat,
                      select_lex->select_number,
                      "IN (SELECT)", "materialization");

  const char *cause= NULL;
  if (predicate->substype() != Item_subselect::IN_SUBS)
  {
    // Subq-mat cannot handle 'outer_expr > {ANY|ALL}(subq)'...
    cause= "not an IN predicate";
  }
  else if (select_lex->is_part_of_union())
  {
    // Subquery must be a single query specification clause (not a UNION)
    cause= "in UNION";
  }
  else if (!select_lex->master_unit()->first_select()->leaf_tables)
  {
    // Subquery has no tables, hence no point in materializing.
    cause= "no inner tables";
  }
  else if (!outer->join)
  {
    /*
      Maybe this is a subquery of a single table UPDATE/DELETE (TODO:
      handle this by switching to multi-table UPDATE/DELETE).
    */
    cause= "parent query has no JOIN";
  }
  else if (!outer->leaf_tables)
  {
    // The upper query is SELECT ... FROM DUAL. No gain in materializing.
    cause= "no tables in outer query";
  }
  else if (predicate->dependent_before_in2exists())
  {
    /*
      Subquery should not be correlated; the correlation due to predicates
      injected by IN->EXISTS does not count as we will remove them if we
      choose materialization.

      TODO:
      This is an overly restrictive condition. It can be extended to:
         (Subquery is non-correlated ||
          Subquery is correlated to any query outer to IN predicate ||
          (Subquery is correlated to the immediate outer query &&
           Subquery !contains {GROUP BY, ORDER BY [LIMIT],
           aggregate functions}) && subquery predicate is not under "NOT IN"))
    */
    cause= "correlated";
  }
  else
  {
    /*
      Check that involved expression types allow materialization.
      This is a temporary fix for BUG#36752; see bug report for
      description of restrictions we need to put on the compared expressions.
    */
    DBUG_ASSERT(predicate->left_expr->fixed);
    List_iterator<Item> it(predicate->unit->first_select()->item_list);

    for (uint i= 0; i < elements; i++)
    {
      Item * const inner= it++;
      Item * const outer= predicate->left_expr->element_index(i);
      if (!types_allow_materialization(outer, inner))
      {
        cause= "type mismatch";
        break;
      }
      if (inner->is_blob_field())                 // 6
      {
        cause= "inner blob";
        break;
      }
      has_nullables|= outer->maybe_null | inner->maybe_null;
    }

    if (!cause)
    {
      trace_mat.add("has_nullable_expressions", has_nullables);
      /*
        Subquery materialization cannot handle NULLs partial matching
        properly, yet. If the outer or inner values are NULL, the
        subselect_hash_sj_engine may reply FALSE when it should reply UNKNOWN.
        So, we must limit it to those three cases:
        - when FALSE and UNKNOWN are equivalent answers. I.e. this is a a
        top-level predicate (this implies it is not negated).
        - when outer and inner values cannot be NULL.
        - when there is a single inner column (because for this we have a
        limited implementation of NULLs partial matching).
      */
      const bool is_top_level= predicate->is_top_level_item();
      trace_mat.add("treat_UNKNOWN_as_FALSE", is_top_level);

      if (!is_top_level && has_nullables && (elements > 1))
        cause= "cannot_handle_partial_matches";
      else
      {
        trace_mat.add("possible", true);
        DBUG_RETURN(TRUE);
      }
    }
  }
  DBUG_ASSERT(cause != NULL);
  trace_mat.add("possible", false).add_alnum("cause", cause);
  DBUG_RETURN(false);
}


/**
  @brief Resolve predicate involving subquery

  @param thd     Pointer to THD.

  @retval FALSE  Success.
  @retval TRUE   Error.

  @details
  Perform early unconditional subquery transformations:
   - Convert subquery predicate into semi-join, or
   - Mark the subquery for execution using materialization, or
   - Perform IN->EXISTS transformation, or
   - Perform more/less ALL/ANY -> MIN/MAX rewrite
   - Substitute trivial scalar-context subquery with its value

  @todo for PS, make the whole block execute only on the first execution

*/

bool SELECT_LEX::resolve_subquery(THD *thd)
{
  DBUG_ENTER("resolve_subquery");

  bool chose_semijoin= false;
  SELECT_LEX *const outer= outer_select();

  /*
    @todo for PS, make the whole block execute only on the first execution.
    resolve_subquery() is only invoked in the first execution for subqueries
    that are transformed to semijoin, but for other subqueries, this function
    is called for every execution. One solution is perhaps to define
    exec_method in class Item_subselect and exit immediately if unequal to
    EXEC_UNSPECIFIED.
  */
  Item_subselect *subq_predicate= master_unit()->item;
  DBUG_ASSERT(subq_predicate);
  /**
    @note
    In this case: IN (SELECT ... UNION SELECT ...), SELECT_LEX::prepare() is
    called for each of the two UNION members, and in those two calls,
    subq_predicate is the same, not sure this is desired (double work?).
  */

  Item_in_subselect * const in_predicate=
    (subq_predicate->substype() == Item_subselect::IN_SUBS) ?
    static_cast<Item_in_subselect *>(subq_predicate) : NULL;

  if (in_predicate)
  {
    thd->lex->set_current_select(outer);
    char const *save_where= thd->where;
    thd->where= "IN/ALL/ANY subquery";

    bool result= !in_predicate->left_expr->fixed &&
                  in_predicate->left_expr->fix_fields(thd,
                                                     &in_predicate->left_expr);
    thd->lex->set_current_select(this);
    thd->where= save_where;
    if (result)
      DBUG_RETURN(TRUE); /* purecov: deadcode */

    /*
      Check if the left and right expressions have the same # of
      columns, i.e. we don't have a case like 
        (oe1, oe2) IN (SELECT ie1, ie2, ie3 ...)

      TODO why do we have this duplicated in IN->EXISTS transformers?
      psergey-todo: fix these: grep for duplicated_subselect_card_check
    */
    if (item_list.elements != in_predicate->left_expr->cols())
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), in_predicate->left_expr->cols());
      DBUG_RETURN(TRUE);
    }
  }

  DBUG_PRINT("info", ("Checking if subq can be converted to semi-join"));
  /*
    Check if we're in subquery that is a candidate for flattening into a
    semi-join (which is done in flatten_subqueries()). The requirements are:
      1. Subquery predicate is an IN/=ANY subquery predicate
      2. Subquery is a single SELECT (not a UNION)
      3. Subquery does not have GROUP BY
      4. Subquery does not use aggregate functions or HAVING
      5. Subquery predicate is at the AND-top-level of ON/WHERE clause
      6. Parent query block accepts semijoins (i.e we are not in a subquery of
      a single table UPDATE/DELETE (TODO: We should handle this at some
      point by switching to multi-table UPDATE/DELETE)
      7. We're not in a confluent table-less subquery, like "SELECT 1".
      8. No execution method was already chosen (by a prepared statement)
      9. Parent select is not a confluent table-less select
      10. Neither parent nor child select have STRAIGHT_JOIN option.
  */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SEMIJOIN) &&
      in_predicate &&                                                   // 1
      !is_part_of_union() &&                                            // 2
      !group_list.elements &&                                           // 3
      !m_having_cond && !with_sum_func &&                                 // 4
      (outer->resolve_place == st_select_lex::RESOLVE_CONDITION ||      // 5
       outer->resolve_place == st_select_lex::RESOLVE_JOIN_NEST) &&     // 5
      outer->sj_candidates &&                                           // 6
      leaf_table_count &&                                               // 7
      in_predicate->exec_method ==
                           Item_exists_subselect::EXEC_UNSPECIFIED &&   // 8
      outer->leaf_table_count &&                                        // 9
      !((options | outer->options) & SELECT_STRAIGHT_JOIN))             // 10
  {
    DBUG_PRINT("info", ("Subquery is semi-join conversion candidate"));

    /* Notify in the subquery predicate where it belongs in the query graph */
    in_predicate->embedding_join_nest= outer->resolve_nest;

    /* Register the subquery for further processing in flatten_subqueries() */
    outer->sj_candidates->push_back(in_predicate);
    chose_semijoin= true;
  }

  if (in_predicate)
  {
    Opt_trace_context * const trace= &thd->opt_trace;
    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        select_number, "IN (SELECT)", "semijoin");
    oto1.add("chosen", chose_semijoin);
  }

  // This is the only part of the function which depends on JOIN:
  if (!chose_semijoin &&
      subq_predicate->select_transformer(join) ==
      Item_subselect::RES_ERROR)
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}


/**
  Fix all conditions and outer join expressions.

  @param  thd     thread handler

  @returns 0 if OK, 1 if error
*/
int SELECT_LEX::setup_conds(THD *thd)
{
  TABLE_LIST *table= NULL;	// For HP compilers
  /*
    it_is_update set to TRUE when tables of primary SELECT_LEX (SELECT_LEX
    which belong to LEX, i.e. most up SELECT) will be updated by
    INSERT/UPDATE/LOAD
    NOTE: using this condition helps to prevent call of prepare_check_option()
    from subquery of VIEW, because tables of subquery belongs to VIEW
    (see condition before prepare_check_option() call)
  */
  bool it_is_update= (this == thd->lex->select_lex) &&
    thd->lex->which_check_option_applicable();
  bool save_is_item_list_lookup= is_item_list_lookup;
  is_item_list_lookup= 0;
  DBUG_ENTER("setup_conds");

  thd->mark_used_columns= MARK_COLUMNS_READ;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  cond_count= 0;
  between_count= 0;
  max_equal_elems= 0;

  for (table= get_table_list(); table; table= table->next_local)
  {
    resolve_place= st_select_lex::RESOLVE_CONDITION;
    /*
      Walk up tree of join nests and try to find outer join nest.
      This is needed because simplify_joins() has not yet been called,
      and hence inner join nests have not yet been removed.
    */
    for (TABLE_LIST *embedding= table;
         embedding;
         embedding= embedding->embedding)
    {
      if (embedding->outer_join)
      {
        /*
          The join condition belongs to an outer join next.
          Record this fact and the outer join nest for possible transformation
          of subqueries into semi-joins.
        */  
        resolve_place= st_select_lex::RESOLVE_JOIN_NEST;
        resolve_nest= embedding;
        break;
      }
    }
    if (table->prepare_where(thd, &m_where_cond, FALSE))
      goto err_no_arena;
    resolve_place= st_select_lex::RESOLVE_NONE;
    resolve_nest= NULL;
  }

  if (m_where_cond)
  {
    resolve_place= st_select_lex::RESOLVE_CONDITION;
    thd->where="where clause";
    if ((!m_where_cond->fixed &&
         m_where_cond->fix_fields(thd, &m_where_cond)) ||
	m_where_cond->check_cols(1))
      goto err_no_arena;
    resolve_place= st_select_lex::RESOLVE_NONE;
  }

  /*
    Apply fix_fields() to all ON clauses at all levels of nesting,
    including the ones inside view definitions.
  */
  for (table= leaf_tables; table; table= table->next_leaf)
  {
    TABLE_LIST *embedded; /* The table at the current level of nesting. */
    TABLE_LIST *embedding= table; /* The parent nested table reference. */
    do
    {
      embedded= embedding;
      if (embedded->join_cond())
      {
        /* Make a join an a expression */
        resolve_place= st_select_lex::RESOLVE_JOIN_NEST;
        resolve_nest= embedded;
        thd->where="on clause";
        if ((!embedded->join_cond()->fixed &&
           embedded->join_cond()->fix_fields(thd, embedded->join_cond_ref())) ||
	   embedded->join_cond()->check_cols(1))
	  goto err_no_arena;
        cond_count++;
        resolve_place= st_select_lex::RESOLVE_NONE;
        resolve_nest= NULL;
      }
      embedding= embedded->embedding;
    }
    while (embedding &&
           embedding->nested_join->join_list.head() == embedded);

    /* process CHECK OPTION */
    if (it_is_update)
    {
      TABLE_LIST *view= table->top_table();
      if (view->effective_with_check)
      {
        if (view->prepare_check_option(thd))
          goto err_no_arena;
        thd->change_item_tree(&table->check_option, view->check_option);
      }
    }
  }

  thd->lex->current_select()->is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(MY_TEST(thd->is_error()));

err_no_arena:
  is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(1);
}


/**
  Set NESTED_JOIN::counter=0 in all nested joins in passed list.

  @param join_list  Pass NULL. Non-NULL is reserved for recursive inner calls,
  then it is a list of nested joins to process, and may also contain base
  tables which will be ignored.
*/

void SELECT_LEX::reset_nj_counters(List<TABLE_LIST> *join_list)
{
  if (join_list == NULL)
    join_list= &top_join_list;
  List_iterator<TABLE_LIST> li(*join_list);
  TABLE_LIST *table;
  DBUG_ENTER("reset_nj_counters");
  while ((table= li++))
  {
    NESTED_JOIN *nested_join;
    if ((nested_join= table->nested_join))
    {
      nested_join->nj_counter= 0;
      reset_nj_counters(&nested_join->join_list);
    }
  }
  DBUG_VOID_RETURN;
}


/**
  Simplify joins replacing outer joins by inner joins whenever it's
  possible.

    The function, during a retrieval of join_list,  eliminates those
    outer joins that can be converted into inner join, possibly nested.
    It also moves the join conditions for the converted outer joins
    and from inner joins to conds.
    The function also calculates some attributes for nested joins:
    - used_tables    
    - not_null_tables
    - dep_tables.
    - on_expr_dep_tables
    The first two attributes are used to test whether an outer join can
    be substituted for an inner join. The third attribute represents the
    relation 'to be dependent on' for tables. If table t2 is dependent
    on table t1, then in any evaluated execution plan table access to
    table t2 must precede access to table t2. This relation is used also
    to check whether the query contains  invalid cross-references.
    The forth attribute is an auxiliary one and is used to calculate
    dep_tables.
    As the attribute dep_tables qualifies possibles orders of tables in the
    execution plan, the dependencies required by the straight join
    modifiers are reflected in this attribute as well.
    The function also removes all braces that can be removed from the join
    expression without changing its meaning.

  @note
    An outer join can be replaced by an inner join if the where condition
    or the join condition for an embedding nested join contains a conjunctive
    predicate rejecting null values for some attribute of the inner tables.

    E.g. in the query:    
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    the predicate t2.b < 5 rejects nulls.
    The query is converted first to:
    @code
      SELECT * FROM t1 INNER JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    then to the equivalent form:
    @code
      SELECT * FROM t1, t2 ON t2.a=t1.a WHERE t2.b < 5 AND t2.a=t1.a
    @endcode


    Similarly the following query:
    @code
      SELECT * from t1 LEFT JOIN (t2, t3) ON t2.a=t1.a t3.b=t1.b
        WHERE t2.c < 5  
    @endcode
    is converted to:
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a t3.b=t1.b 

    @endcode

    One conversion might trigger another:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a
                       LEFT JOIN t3 ON t3.b=t2.b
        WHERE t3 IS NOT NULL =>
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b => 
      SELECT * FROM t1, t2, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b AND t2.a=t1.a
  @endcode

    The function removes all unnecessary braces from the expression
    produced by the conversions.
    E.g.
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b
    @endcode
    finally is converted to: 
    @code
      SELECT * FROM t1, t2, t3 WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b

    @endcode


    It also will remove braces from the following queries:
    @code
      SELECT * from (t1 LEFT JOIN t2 ON t2.a=t1.a) LEFT JOIN t3 ON t3.b=t2.b
      SELECT * from (t1, (t2,t3)) WHERE t1.a=t2.a AND t2.b=t3.b.
    @endcode

    The benefit of this simplification procedure is that it might return 
    a query for which the optimizer can evaluate execution plan with more
    join orders. With a left join operation the optimizer does not
    consider any plan where one of the inner tables is before some of outer
    tables.

  IMPLEMENTATION
    The function is implemented by a recursive procedure.  On the recursive
    ascent all attributes are calculated, all outer joins that can be
    converted are replaced and then all unnecessary braces are removed.
    As join list contains join tables in the reverse order sequential
    elimination of outer joins does not require extra recursive calls.

  SEMI-JOIN NOTES
    Remove all semi-joins that have are within another semi-join (i.e. have
    an "ancestor" semi-join nest)

  EXAMPLES
    Here is an example of a join query with invalid cross references:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t3.a LEFT JOIN t3 ON t3.b=t1.b 
    @endcode

  @param thd         thread handler
  @param join_list   list representation of the join to be converted
  @param top         true <=> cond is the where condition
  @param in_sj       TRUE <=> processing semi-join nest's children
  @param[in,out] cond  In: condition to which the join condition for converted
  outer joins is to be added; out: new condition
  @param changelog   Don't specify this parameter, it is reserved for
                     recursive calls inside this function

  @returns true for error, false for success
*/
bool
SELECT_LEX::simplify_joins(THD *thd,
                           List<TABLE_LIST> *join_list, bool top,
                           bool in_sj, Item **cond, uint *changelog)
{
  /*
    Each type of change done by this function, or its recursive calls, is
    tracked in a bitmap:
  */
  enum change
  {
    NONE= 0,
    OUTER_JOIN_TO_INNER= 1 << 0,
    JOIN_COND_TO_WHERE= 1 << 1,
    PAREN_REMOVAL= 1 << 2,
    SEMIJOIN= 1 << 3
  };
  uint changes= 0; // To keep track of changes.
  if (changelog == NULL) // This is the top call.
    changelog= &changes;

  TABLE_LIST *table;
  NESTED_JOIN *nested_join;
  TABLE_LIST *prev_table= 0;
  List_iterator<TABLE_LIST> li(*join_list);
  const bool straight_join= options & SELECT_STRAIGHT_JOIN;
  DBUG_ENTER("simplify_joins");

  /* 
    Try to simplify join operations from join_list.
    The most outer join operation is checked for conversion first. 
  */
  while ((table= li++))
  {
    table_map used_tables;
    table_map not_null_tables= (table_map) 0;

    if ((nested_join= table->nested_join))
    {
      /* 
         If the element of join_list is a nested join apply
         the procedure to its nested join list first.
      */
      if (table->join_cond())
      {
        Item *join_cond= table->join_cond();
        /* 
           If a join condition JC is attached to the table, 
           check all null rejected predicates in this condition.
           If such a predicate over an attribute belonging to
           an inner table of an embedded outer join is found,
           the outer join is converted to an inner join and
           the corresponding join condition is added to JC. 
	*/ 
        if (simplify_joins(thd, &nested_join->join_list,
                           false, in_sj || table->sj_on_expr,
                           &join_cond, changelog))
          DBUG_RETURN(true);

        if (join_cond != table->join_cond())
        {
          DBUG_ASSERT(join_cond);
          table->set_join_cond(join_cond);
        }
      }
      nested_join->used_tables= (table_map) 0;
      nested_join->not_null_tables=(table_map) 0;
      if (simplify_joins(thd, &nested_join->join_list, top,
                         in_sj || table->sj_on_expr, cond, changelog))
        DBUG_RETURN(true);
      used_tables= nested_join->used_tables;
      not_null_tables= nested_join->not_null_tables;  
    }
    else
    {
      used_tables= table->map();
      if (*cond)
        not_null_tables= (*cond)->not_null_tables();
    }
      
    if (table->embedding)
    {
      table->embedding->nested_join->used_tables|= used_tables;
      table->embedding->nested_join->not_null_tables|= not_null_tables;
    }

    if (!table->outer_join || (used_tables & not_null_tables))
    {
      /* 
        For some of the inner tables there are conjunctive predicates
        that reject nulls => the outer join can be replaced by an inner join.
      */
      if (table->outer_join)
      {
        *changelog|= OUTER_JOIN_TO_INNER;
        table->outer_join= 0;
      }
      if (table->join_cond())
      {
        *changelog|= JOIN_COND_TO_WHERE;
        /* Add join condition to the WHERE or upper-level join condition. */
        if (*cond)
        {
          Item_cond_and *new_cond=
            static_cast<Item_cond_and*>(and_conds(*cond, table->join_cond()));
          if (!new_cond)
            DBUG_RETURN(true);
          new_cond->top_level_item();
          /*
            It is always a new item as both the upper-level condition and a
            join condition existed
          */
          DBUG_ASSERT(!new_cond->fixed);
          if (new_cond->fix_fields(thd, NULL))
            DBUG_RETURN(true);

          /* If join condition has a pending rollback in THD::change_list */
          List_iterator<Item> lit(*new_cond->argument_list());
          Item *arg;
          while ((arg= lit++))
          {
            /*
              The join condition isn't necessarily the second argument anymore,
              since fix_fields may have merged it into an existing AND expr.
            */
            if (arg == table->join_cond())
              thd->change_item_tree_place(table->join_cond_ref(), lit.ref());
            else if (arg == *cond)
              thd->change_item_tree_place(cond, lit.ref());
          }
          *cond= new_cond;
        }
        else
        {
          *cond= table->join_cond();
          /* If join condition has a pending rollback in THD::change_list */
          thd->change_item_tree_place(table->join_cond_ref(), cond);
        }
        table->set_join_cond(NULL);
      }
    }

    if (!top)
      continue;

    /* 
      Only inner tables of non-convertible outer joins remain with
      the join condition.
    */ 
    if (table->join_cond())
    {
      table->dep_tables|= table->join_cond()->used_tables();

      // At this point the joined tables always have an embedding join nest:
      DBUG_ASSERT(table->embedding);

      table->dep_tables&= ~table->embedding->nested_join->used_tables;

      // Embedding table depends on tables used in embedded join conditions. 
      table->embedding->on_expr_dep_tables|= table->join_cond()->used_tables();
    }

    if (prev_table)
    {
      /* The order of tables is reverse: prev_table follows table */
      if (prev_table->straight || straight_join)
        prev_table->dep_tables|= used_tables;
      if (prev_table->join_cond())
      {
        prev_table->dep_tables|= table->on_expr_dep_tables;
        table_map prev_used_tables= prev_table->nested_join ?
	                            prev_table->nested_join->used_tables :
	                            prev_table->map();
        /* 
          If join condition contains only references to inner tables
          we still make the inner tables dependent on the outer tables.
          It would be enough to set dependency only on one outer table
          for them. Yet this is really a rare case.
          Note:
          PSEUDO_TABLE_BITS mask should not be counted as it
          prevents update of inner table dependencies.
          For example it might happen if RAND()/COUNT(*) function
          is used in JOIN ON clause.
	*/  
        if (!((prev_table->join_cond()->used_tables() & ~PSEUDO_TABLE_BITS) &
              ~prev_used_tables))
          prev_table->dep_tables|= used_tables;
      }
    }
    prev_table= table;
  }

  /*
    Flatten nested joins that can be flattened.
    no join condition and not a semi-join => can be flattened.
  */
  li.rewind();
  while ((table= li++))
  {
    nested_join= table->nested_join;
    if (table->sj_on_expr && !in_sj)
    {
       /*
         If this is a semi-join that is not contained within another semi-join, 
         leave it intact (otherwise it is flattened)
       */
      *changelog|= SEMIJOIN;
    }
    else if (nested_join && !table->join_cond())
    {
      *changelog|= PAREN_REMOVAL;
      TABLE_LIST *tbl;
      List_iterator<TABLE_LIST> it(nested_join->join_list);
      while ((tbl= it++))
      {
        tbl->embedding= table->embedding;
        tbl->join_list= table->join_list;
        tbl->dep_tables|= table->dep_tables;
      }
      li.replace(nested_join->join_list);
    }
  }

  if (changes)
  {
    Opt_trace_context * trace= &thd->opt_trace;
    if (unlikely(trace->is_started()))
    {
      Opt_trace_object trace_wrapper(trace);
      Opt_trace_object trace_object(trace, "transformations_to_nested_joins");
      {
        Opt_trace_array trace_changes(trace, "transformations");
        if (changes & SEMIJOIN)
          trace_changes.add_alnum("semijoin");
        if (changes & OUTER_JOIN_TO_INNER)
          trace_changes.add_alnum("outer_join_to_inner_join");
        if (changes & JOIN_COND_TO_WHERE)
          trace_changes.add_alnum("JOIN_condition_to_WHERE");
        if (changes & PAREN_REMOVAL)
          trace_changes.add_alnum("parenthesis_removal");
      }
      // the newly transformed query is worth printing
      opt_trace_print_expanded_query(thd, this, &trace_object);
    }
  }
  DBUG_RETURN(false);
}


/**
  Record join nest info in the select block.

  After simplification of inner join, outer join and semi-join structures:
   - record the remaining semi-join structures in the enclosing query block.
   - record transformed join conditions in TABLE_LIST objects.

  This function is called recursively for each join nest and/or table
  in the query block.

  @param select The query block
  @param tables List of tables and join nests

  @return False if successful, True if failure
*/
bool SELECT_LEX::record_join_nest_info(List<TABLE_LIST> *tables)
{
  TABLE_LIST *table;
  List_iterator<TABLE_LIST> li(*tables);
  DBUG_ENTER("record_join_nest_info");

  while ((table= li++))
  {
    if (table->nested_join == NULL)
    {
      if (table->join_cond())
        outer_join|= table->map();
      continue;
    }

    if (record_join_nest_info(&table->nested_join->join_list))
      DBUG_RETURN(true);
    /*
      sj_inner_tables is set properly later in pull_out_semijoin_tables().
      This assignment is required in case pull_out_semijoin_tables()
      is not called.
    */
    if (table->sj_on_expr)
      table->sj_inner_tables= table->nested_join->used_tables;
    if (table->sj_on_expr && sj_nests.push_back(table))
      DBUG_RETURN(true);

    if (table->join_cond())
      outer_join|= table->nested_join->used_tables;
  }
  DBUG_RETURN(false);
}


static int subq_sj_candidate_cmp(Item_exists_subselect* const *el1, 
                                 Item_exists_subselect* const *el2)
{
  /*
    Remove this assert when we support semijoin on non-IN subqueries.
  */
  DBUG_ASSERT((*el1)->substype() == Item_subselect::IN_SUBS &&
              (*el2)->substype() == Item_subselect::IN_SUBS);
  return ((*el1)->sj_convert_priority < (*el2)->sj_convert_priority) ? 1 : 
         ( ((*el1)->sj_convert_priority == (*el2)->sj_convert_priority)? 0 : -1);
}


static void fix_list_after_tbl_changes(st_select_lex *parent_select,
                                       st_select_lex *removed_select,
                                       List<TABLE_LIST> *tlist)
{
  List_iterator<TABLE_LIST> it(*tlist);
  TABLE_LIST *table;
  while ((table= it++))
  {
    if (table->join_cond())
      table->join_cond()->fix_after_pullout(parent_select, removed_select);
    if (table->nested_join)
      fix_list_after_tbl_changes(parent_select, removed_select,
                                 &table->nested_join->join_list);
  }
}


/**
  Convert a subquery predicate of this query block into a TABLE_LIST semi-join
  nest.

  @param subq_pred   Subquery predicate to be converted.
                     This is either an IN, =ANY or EXISTS predicate.

  @retval FALSE OK
  @retval TRUE  Error

  @details

  The following transformations are performed:

  1. IN/=ANY predicates on the form:

  SELECT ...
  FROM ot1 ... otN
  WHERE (oe1, ... oeM) IN (SELECT ie1, ..., ieM)
                           FROM it1 ... itK
                          [WHERE inner-cond])
   [AND outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]

  are transformed into:

  SELECT ...
  FROM (ot1 ... otN) SJ (it1 ... itK)
                     ON (oe1, ... oeM) = (ie1, ..., ieM)
                        [AND inner-cond]
  [WHERE outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]

  Notice that the inner-cond may contain correlated and non-correlated
  expressions. Further transformations will analyze and break up such
  expressions.

  Prepared Statements: the transformation is permanent:
   - Changes in TABLE_LIST structures are naturally permanent
   - Item tree changes are performed on statement MEM_ROOT:
      = we activate statement MEM_ROOT
      = this function is called before the first fix_prepare_information call.

  This is intended because the criteria for subquery-to-sj conversion remain
  constant for the lifetime of the Prepared Statement.
*/
bool
SELECT_LEX::convert_subquery_to_semijoin(Item_exists_subselect *subq_pred)
{
  TABLE_LIST *emb_tbl_nest= NULL;
  List<TABLE_LIST> *emb_join_list= &top_join_list;
  THD *const thd= subq_pred->unit->thd;
  DBUG_ENTER("convert_subquery_to_semijoin");

  DBUG_ASSERT(subq_pred->substype() == Item_subselect::IN_SUBS);

  /*
    Find out where to insert the semi-join nest and the generated condition.

    For t1 LEFT JOIN t2, embedding_join_nest will be t2.
    Note that t2 may be a simple table or may itself be a join nest
    (e.g. in the case t1 LEFT JOIN (t2 JOIN t3))
  */
  if ((void*)subq_pred->embedding_join_nest != NULL)
  {
    if (subq_pred->embedding_join_nest->nested_join)
    {
      /*
        We're dealing with

          ... [LEFT] JOIN  ( ... ) ON (subquery AND condition) ...

        The sj-nest will be inserted into the brackets nest.
      */
      emb_tbl_nest=  subq_pred->embedding_join_nest;
      emb_join_list= &emb_tbl_nest->nested_join->join_list;
    }
    else if (!subq_pred->embedding_join_nest->outer_join)
    {
      /*
        We're dealing with

          ... INNER JOIN tblX ON (subquery AND condition) ...

        The sj-nest will be tblX's "sibling", i.e. another child of its
        parent. This is ok because tblX is joined as an inner join.
      */
      emb_tbl_nest= subq_pred->embedding_join_nest->embedding;
      if (emb_tbl_nest)
        emb_join_list= &emb_tbl_nest->nested_join->join_list;
    }
    else if (!subq_pred->embedding_join_nest->nested_join)
    {
      TABLE_LIST *outer_tbl= subq_pred->embedding_join_nest;      
      /*
        We're dealing with

          ... LEFT JOIN tbl ON (on_expr AND subq_pred) ...

        we'll need to convert it into:

          ... LEFT JOIN ( tbl SJ (subq_tables) ) ON (on_expr AND subq_pred) ...
                        |                      |
                        |<----- wrap_nest ---->|
        
        Q:  other subqueries may be pointing to this element. What to do?
        A1: simple solution: copy *subq_pred->embedding_join_nest= *parent_nest.
            But we'll need to fix other pointers.
        A2: Another way: have TABLE_LIST::next_ptr so the following
            subqueries know the table has been nested.
        A3: changes in the TABLE_LIST::outer_join will make everything work
            automatically.
      */
      TABLE_LIST *const wrap_nest=
        TABLE_LIST::new_nested_join(thd->mem_root, "(sj-wrap)",
                                    outer_tbl->embedding, outer_tbl->join_list,
                                    this);
      if (wrap_nest == NULL)
        DBUG_RETURN(true);

      wrap_nest->nested_join->join_list.push_back(outer_tbl);

      outer_tbl->embedding= wrap_nest;
      outer_tbl->join_list= &wrap_nest->nested_join->join_list;

      /*
        An important note, if this 'PREPARE stmt'.
        The FROM clause of the outer query now looks like
        CONCAT(original FROM clause of outer query, sj-nest).
        Given that the original FROM clause is reversed, this list is
        interpreted as "sj-nest is first".
        Thus, at a next execution, setup_natural_join_types() will decide that
        the name resolution context of the FROM clause should start at the
        first inner table in sj-nest.
        However, note that in the present function we do not change
        first_name_resolution_table (and friends) of sj-inner tables.
        So, at the next execution, name resolution for columns of
        outer-table columns is bound to fail (the first inner table does
        not have outer tables in its chain of resolution).
        Fortunately, Item_field::cached_table, which is set during resolution
        of 'PREPARE stmt', gives us the answer and avoids a failing search.
      */

      /*
        wrap_nest will take place of outer_tbl, so move the outer join flag
        and join condition.
      */
      wrap_nest->outer_join= outer_tbl->outer_join;
      outer_tbl->outer_join= 0;

      // There are item-rollback problems in this function: see bug#16926177
      wrap_nest->set_join_cond(outer_tbl->join_cond()->real_item());
      outer_tbl->set_join_cond(NULL);

      List_iterator<TABLE_LIST> li(*wrap_nest->join_list);
      TABLE_LIST *tbl;
      while ((tbl= li++))
      {
        if (tbl == outer_tbl)
        {
          li.replace(wrap_nest);
          break;
        }
      }
      /*
        Ok now wrap_nest 'contains' outer_tbl and we're ready to add the 
        semi-join nest into it
      */
      emb_join_list= &wrap_nest->nested_join->join_list;
      emb_tbl_nest=  wrap_nest;
    }
  }

  TABLE_LIST *const sj_nest=
    TABLE_LIST::new_nested_join(thd->mem_root, "(sj-nest)",
                                emb_tbl_nest, emb_join_list, this);
  if (sj_nest == NULL)
    DBUG_RETURN(true);       /* purecov: inspected */

  NESTED_JOIN *const nested_join= sj_nest->nested_join;

  /* Nests do not participate in those 'chains', so: */
  /* sj_nest->next_leaf= sj_nest->next_local= sj_nest->next_global == NULL*/
  emb_join_list->push_back(sj_nest);

  /* 
    nested_join->used_tables and nested_join->not_null_tables are
    initialized in simplify_joins().
  */
  
  /* 
    2. Walk through subquery's top list and set 'embedding' to point to the
       sj-nest.
  */
  st_select_lex *const subq_select= subq_pred->unit->first_select();

  nested_join->query_block_id= subq_select->select_number;
  nested_join->join_list.empty();
  List_iterator_fast<TABLE_LIST> li(subq_select->top_join_list);
  TABLE_LIST *tl;
  while ((tl= li++))
  {
    tl->embedding= sj_nest;
    tl->join_list= &nested_join->join_list;
    nested_join->join_list.push_back(tl);
  }
  
  /*
    Reconnect the next_leaf chain.
    TODO: Do we have to put subquery's tables at the end of the chain?
          Inserting them at the beginning would be a bit faster.
    NOTE: We actually insert them at the front! That's because the order is
          reversed in this list.
  */
  for (tl= leaf_tables; tl->next_leaf; tl= tl->next_leaf)
  {}
  tl->next_leaf= subq_select->leaf_tables;

  /*
    Same as above for next_local chain. This needed only for re-execution.
    (The next_local chain always starts with SELECT_LEX::table_list)
  */
  for (tl= get_table_list(); tl->next_local; tl= tl->next_local)
  {}
  tl->next_local= subq_select->get_table_list();

  /* A theory: no need to re-connect the next_global chain */

  /* 3. Remove the original subquery predicate from the WHERE/ON */

  // The subqueries were replaced for Item_int(1) earlier
  /*TODO: also reset the 'with_subselect' there. */

  /* n. Adjust the parent_join->tables counter */
  uint table_no= leaf_table_count;
  /* n. Walk through child's tables and adjust table->map */
  for (tl= subq_select->leaf_tables; tl; tl= tl->next_leaf, table_no++)
    tl->set_tableno(table_no);

  derived_table_count+= subq_select->derived_table_count;
  materialized_table_count+=
    subq_select->materialized_table_count;
  partitioned_table_count+= subq_select->partitioned_table_count;
  leaf_table_count+= subq_select->leaf_table_count;

  nested_join->sj_outer_exprs.empty();
  nested_join->sj_inner_exprs.empty();

  /*
    @todo: Add similar conversion for subqueries other than IN.
  */
  if (subq_pred->substype() == Item_subselect::IN_SUBS)
  {
    Item_in_subselect *in_subq_pred= (Item_in_subselect *)subq_pred;

    DBUG_ASSERT(is_fixed_or_outer_ref(in_subq_pred->left_expr));

    in_subq_pred->exec_method= Item_exists_subselect::EXEC_SEMI_JOIN;
    /*
      sj_corr_tables is supposed to contain non-trivially correlated tables,
      but here it is set to contain all correlated tables.
      @todo: Add analysis step that assigns only the set of non-trivially
      correlated tables to sj_corr_tables.
    */
    nested_join->sj_corr_tables= subq_pred->used_tables();
    /*
      sj_depends_on contains the set of outer tables referred in the
      subquery's WHERE clause as well as tables referred in the IN predicate's
      left-hand side.
    */
    nested_join->sj_depends_on=  subq_pred->used_tables() |
                                 in_subq_pred->left_expr->used_tables();
    /* Put the subquery's WHERE into semi-join's condition. */
    sj_nest->sj_on_expr= subq_select->where_cond();

    /*
    Create the IN-equalities and inject them into semi-join's ON condition.
    Additionally, for LooseScan strategy
     - Record the number of IN-equalities.
     - Create list of pointers to (oe1, ..., ieN). We'll need the list to
       see which of the expressions are bound and which are not (for those
       we'll produce a distinct stream of (ie_i1,...ie_ik).

       (TODO: can we just create a list of pointers and hope the expressions
       will not substitute themselves on fix_fields()? or we need to wrap
       them into Item_direct_view_refs and store pointers to those. The
       pointers to Item_direct_view_refs are guaranteed to be stable as 
       Item_direct_view_refs doesn't substitute itself with anything in 
       Item_direct_view_ref::fix_fields.
    */

    if (in_subq_pred->left_expr->type() == Item::SUBSELECT_ITEM)
    {
      List<Item> ref_list;
      uint i;

      Item *header= subq_select->ref_pointer_array[0];
      for (i= 1; i < in_subq_pred->left_expr->cols(); i++)
      {
        ref_list.push_back(subq_select->ref_pointer_array[i]);
      }

      Item_row *right_expr= new Item_row(header, ref_list);

      nested_join->sj_outer_exprs.push_back(in_subq_pred->left_expr);
      nested_join->sj_inner_exprs.push_back(right_expr);
      Item_func_eq *item_eq=
        new Item_func_eq(in_subq_pred->left_expr,
                         right_expr);
      if (item_eq == NULL)
        DBUG_RETURN(TRUE);

      sj_nest->sj_on_expr= and_items(sj_nest->sj_on_expr, item_eq);
      if (sj_nest->sj_on_expr == NULL)
        DBUG_RETURN(TRUE);
    }
    else
    {
      for (uint i= 0; i < in_subq_pred->left_expr->cols(); i++)
      {
        nested_join->sj_outer_exprs.push_back(in_subq_pred->left_expr->
                                              element_index(i));
        nested_join->sj_inner_exprs.push_back(subq_select->ref_pointer_array[i]);

        Item_func_eq *item_eq= 
          new Item_func_eq(in_subq_pred->left_expr->element_index(i), 
                           subq_select->ref_pointer_array[i]);
        if (item_eq == NULL)
          DBUG_RETURN(TRUE);

        sj_nest->sj_on_expr= and_items(sj_nest->sj_on_expr, item_eq);
        if (sj_nest->sj_on_expr == NULL)
          DBUG_RETURN(TRUE);
      }
    }
    /* Fix the created equality and AND */

    Opt_trace_array sj_on_trace(&thd->opt_trace,
                                "evaluating_constant_semijoin_conditions");
    sj_nest->sj_on_expr->top_level_item();
    if (sj_nest->sj_on_expr->fix_fields(thd, &sj_nest->sj_on_expr))
      DBUG_RETURN(true);
  }

  /* Unlink the child select_lex: */
  subq_select->master_unit()->exclude_level();
  removed_select= subq_select;
  /*
    Update the resolver context - needed for Item_field objects that have been
    replaced in the item tree for this execution, but are still needed for
    subsequent executions.
  */
  for (st_select_lex *select= removed_select;
       select != NULL;
       select= select->removed_select)
    select->context.select_lex= this;

  repoint_contexts_of_join_nests(subq_select->top_join_list,
                                 subq_select, this);

  /*
    Walk through sj nest's WHERE and ON expressions and call
    item->fix_table_changes() for all items.
  */
  sj_nest->sj_on_expr->fix_after_pullout(this, subq_select);
  fix_list_after_tbl_changes(this, subq_select,
                             &sj_nest->nested_join->join_list);

  //TODO fix QT_
  DBUG_EXECUTE("where",
               print_where(sj_nest->sj_on_expr,"SJ-EXPR", QT_ORDINARY););

  if (emb_tbl_nest)
  {
    /* Inject sj_on_expr into the parent's ON condition */
    emb_tbl_nest->set_join_cond(and_items(emb_tbl_nest->join_cond(),
                                          sj_nest->sj_on_expr));
    if (emb_tbl_nest->join_cond() == NULL)
      DBUG_RETURN(true);
    emb_tbl_nest->join_cond()->top_level_item();
    if (!emb_tbl_nest->join_cond()->fixed &&
        emb_tbl_nest->join_cond()->fix_fields(thd,
                                              emb_tbl_nest->join_cond_ref()))
      DBUG_RETURN(true);
  }
  else
  {
    /* Inject sj_on_expr into the parent's WHERE condition */
    m_where_cond= and_items(m_where_cond, sj_nest->sj_on_expr);
    if (m_where_cond == NULL)
      DBUG_RETURN(true);
    m_where_cond->top_level_item();
    if (m_where_cond->fix_fields(thd, &m_where_cond))
      DBUG_RETURN(true);
  }

  if (subq_select->ftfunc_list->elements)
  {
    Item_func_match *ifm;
    List_iterator_fast<Item_func_match> li(*(subq_select->ftfunc_list));
    while ((ifm= li++))
      ftfunc_list->push_front(ifm);
  }

  DBUG_RETURN(false);
}


/**
   Destructively replaces a sub-condition inside a condition tree. The
   parse tree is also altered.

   @note Because of current requirements for semijoin flattening, we do not
   need to recurse here, hence this function will only examine the top-level
   AND conditions. (see SELECT_LEX::prepare, comment starting with "Check if
   the subquery predicate can be executed via materialization".)
   
   @param thd  thread handler

   @param tree Must be the handle to the top level condition. This is needed
   when the top-level condition changes.

   @param old_cond The condition to be replaced.

   @param new_cond The condition to be substituted.

   @param do_fix_fields If true, Item::fix_fields(THD*, Item**) is called for
   the new condition.

   @return error status

   @retval true If there was an error.
   @retval false If successful.
*/

static bool replace_subcondition(THD *thd, Item **tree,
                                 Item *old_cond, Item *new_cond,
                                 bool do_fix_fields)
{
  if (*tree == old_cond)
  {
    *tree= new_cond;
    if (do_fix_fields && new_cond->fix_fields(thd, tree))
      return TRUE;
    return FALSE;
  }
  else if ((*tree)->type() == Item::COND_ITEM) 
  {
    List_iterator<Item> li(*((Item_cond*)(*tree))->argument_list());
    Item *item;
    while ((item= li++))
    {
      if (item == old_cond) 
      {
        li.replace(new_cond);
        if (do_fix_fields && new_cond->fix_fields(thd, li.ref()))
          return TRUE;
        return FALSE;
      }
    }
  }
  else
    // If we came here it means there were an error during prerequisites check.
    DBUG_ASSERT(FALSE);

  return TRUE;
}


/*
  Convert semi-join subquery predicates into semi-join join nests
 
  DESCRIPTION

    Convert candidate subquery predicates into semi-join join nests. This 
    transformation is performed once in query lifetime and is irreversible.
    
    Conversion of one subquery predicate
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    We start with a join that has a semi-join subquery:

      SELECT ...
      FROM ot, ...
      WHERE oe IN (SELECT ie FROM it1 ... itN WHERE subq_where) AND outer_where

    and convert it into a semi-join nest:

      SELECT ...
      FROM ot SEMI JOIN (it1 ... itN), ...
      WHERE outer_where AND subq_where AND oe=ie

    that is, in order to do the conversion, we need to 

     * Create the "SEMI JOIN (it1 .. itN)" part and add it into the parent
       query's FROM structure.
     * Add "AND subq_where AND oe=ie" into parent query's WHERE (or ON if
       the subquery predicate was in an ON expression)
     * Remove the subquery predicate from the parent query's WHERE

    Considerations when converting many predicates
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    A join may have at most MAX_TABLES tables. This may prevent us from
    flattening all subqueries when the total number of tables in parent and
    child selects exceeds MAX_TABLES. In addition, one slot is reserved per
    semi-join nest, in case the subquery needs to be materialized in a
    temporary table.
    We deal with this problem by flattening children's subqueries first and
    then using a heuristic rule to determine each subquery predicate's
    "priority".

  RETURN 
    FALSE  OK
    TRUE   Error
*/
bool SELECT_LEX::flatten_subqueries()
{
  DBUG_ENTER("flatten_subqueries");

  if (sj_candidates->empty())
    DBUG_RETURN(FALSE);

  Item_exists_subselect **subq,
    **subq_begin= sj_candidates->begin(),
    **subq_end= sj_candidates->end();

  THD *const thd= (*subq_begin)->unit->thd;
  Opt_trace_context *const trace= &thd->opt_trace;

  /*
    Semijoin flattening is bottom-up. Indeed, we have this execution flow,
    for SELECT#1 WHERE X IN (SELECT #2 WHERE Y IN (SELECT#3)) :

    SELECT_LEX::prepare() (select#1)
       -> fix_fields() on IN condition
           -> SELECT_LEX::prepare() on subquery (select#2)
               -> fix_fields() on IN condition
                    -> SELECT_LEX::prepare() on subquery (select#3)
                    <- SELECT_LEX::prepare()
               <- fix_fields()
               -> flatten_subqueries: merge #3 in #2
               <- flatten_subqueries
           <- SELECT_LEX::prepare()
       <- fix_fields()
       -> flatten_subqueries: merge #2 in #1

    Note that flattening of #(N) is done by its parent JOIN#(N-1), because
    there are cases where flattening is not possible and only the parent can
    know.
   */
  for (subq= subq_begin; subq < subq_end; subq++)
  {
    /*
      Currently, we only support transformation of IN subqueries.
    */
    DBUG_ASSERT((*subq)->substype() == Item_subselect::IN_SUBS);

    st_select_lex *child_select= (*subq)->unit->first_select();

    // Check that we proceeded bottom-up
    DBUG_ASSERT(child_select->sj_candidates == NULL);

    (*subq)->sj_convert_priority= 
      (((*subq)->unit->uncacheable & UNCACHEABLE_DEPENDENT) ? MAX_TABLES : 0) +
      child_select->leaf_table_count;
  }

  /* 
    2. Pick which subqueries to convert:
      sort the subquery array
      - prefer correlated subqueries over uncorrelated;
      - prefer subqueries that have greater number of outer tables;
  */
  my_qsort(subq_begin,
           sj_candidates->size(), sj_candidates->element_size(),
           reinterpret_cast<qsort_cmp>(subq_sj_candidate_cmp));

  // A permanent transformation is going to start, so:
  Prepared_stmt_arena_holder ps_arena_holder(thd);

  // #tables-in-parent-query + #tables-in-subquery + sj nests <= MAX_TABLES
  /* Replace all subqueries to be flattened with Item_int(1) */

  uint table_count= leaf_table_count;
  for (subq= subq_begin; subq < subq_end; subq++)
  {
    // Add the tables in the subquery nest plus one in case of materialization:
    const uint tables_added=
      (*subq)->unit->first_select()->leaf_table_count + 1;
    (*subq)->sj_chosen= table_count + tables_added <= MAX_TABLES;

    if (!(*subq)->sj_chosen)
      continue;

    table_count+= tables_added;

    // In WHERE/ON of parent query, replace IN(subq) with "1" (<=>TRUE)
    Item **tree= ((*subq)->embedding_join_nest == NULL) ?
                 &m_where_cond :
                (*subq)->embedding_join_nest->join_cond_ref();
    if (replace_subcondition(thd, tree, *subq, new Item_int(1), FALSE))
      DBUG_RETURN(TRUE); /* purecov: inspected */
  }

  for (subq= subq_begin; subq < subq_end; subq++)
  {
    if (!(*subq)->sj_chosen)
      continue;

    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        (*subq)->unit->first_select()->select_number,
                        "IN (SELECT)", "semijoin");
    oto1.add("chosen", true);
    if (convert_subquery_to_semijoin(*subq))
      DBUG_RETURN(TRUE);
  }
  /* 
    3. Finalize the subqueries that we did not convert,
       ie. perform IN->EXISTS rewrite.
  */
  for (subq= subq_begin; subq < subq_end; subq++)
  {
    if ((*subq)->sj_chosen)
      continue;
    {
      OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                          (*subq)->unit->first_select()->select_number,
                          "IN (SELECT)", "semijoin");
      oto1.add("chosen", false);
    }
    Item_subselect::trans_res res;
    (*subq)->changed= 0;
    (*subq)->fixed= 0;

    SELECT_LEX *save_select_lex= thd->lex->current_select();
    thd->lex->set_current_select((*subq)->unit->first_select());

    // This is the only part of the function which uses a JOIN.
    res= (*subq)->select_transformer((*subq)->unit->first_select()->join);

    thd->lex->set_current_select(save_select_lex);

    if (res == Item_subselect::RES_ERROR)
      DBUG_RETURN(TRUE);

    (*subq)->changed= 1;
    (*subq)->fixed= 1;

    Item *substitute= (*subq)->substitution;
    const bool do_fix_fields= !(*subq)->substitution->fixed;
    const bool subquery_in_join_clause= (*subq)->embedding_join_nest != NULL;

    Item **tree= subquery_in_join_clause ?
      ((*subq)->embedding_join_nest->join_cond_ref()) : &m_where_cond;
    if (replace_subcondition(thd, tree, *subq, substitute, do_fix_fields))
      DBUG_RETURN(TRUE);
    (*subq)->substitution= NULL;
  }

  sj_candidates->clear();
  DBUG_RETURN(FALSE);
}


/**
  Fix fields referenced from inner selects.

  @param thd               Thread handle
  @param all_fields        List of all fields used in select
  @param select            Current select
  @param ref_pointer_array Array of references to Items used in current select
  @param group_list        GROUP BY list (is NULL by default)

  @details
    The function serves 3 purposes

    - adds fields referenced from inner query blocks to the current select list

    - Decides which class to use to reference the items (Item_ref or
      Item_direct_ref)

    - fixes references (Item_ref objects) to these fields.

    If a field isn't already on the select list and the ref_pointer_array
    is provided then it is added to the all_fields list and the pointer to
    it is saved in the ref_pointer_array.

    The class to access the outer field is determined by the following rules:

    -#. If the outer field isn't used under an aggregate function then the
        Item_ref class should be used.

    -#. If the outer field is used under an aggregate function and this
        function is, in turn, aggregated in the query block where the outer
        field was resolved or some query nested therein, then the
        Item_direct_ref class should be used. Also it should be used if we are
        grouping by a subquery containing the outer field.

    The resolution is done here and not at the fix_fields() stage as
    it can be done only after aggregate functions are fixed and pulled up to
    selects where they are to be aggregated.

    When the class is chosen it substitutes the original field in the
    Item_outer_ref object.

    After this we proceed with fixing references (Item_outer_ref objects) to
    this field from inner subqueries.

  @return Status
  @retval true An error occured.
  @retval false OK.
 */

bool
fix_inner_refs(THD *thd, List<Item> &all_fields, SELECT_LEX *select,
               Ref_ptr_array ref_pointer_array, ORDER *group_list)
{
  Item_outer_ref *ref;

  List_iterator<Item_outer_ref> ref_it(select->inner_refs_list);
  while ((ref= ref_it++))
  {
    bool direct_ref= false;
    Item *item= ref->outer_ref;
    Item **item_ref= ref->ref;
    Item_ref *new_ref;
    /*
      TODO: this field item already might be present in the select list.
      In this case instead of adding new field item we could use an
      existing one. The change will lead to less operations for copying fields,
      smaller temporary tables and less data passed through filesort.
    */
    if (!ref_pointer_array.is_null() && !ref->found_in_select_list)
    {
      int el= all_fields.elements;
      ref_pointer_array[el]= item;
      /* Add the field item to the select list of the current select. */
      all_fields.push_front(item);
      /*
        If it's needed reset each Item_ref item that refers this field with
        a new reference taken from ref_pointer_array.
      */
      item_ref= &ref_pointer_array[el];
    }

    if (ref->in_sum_func)
    {
      Item_sum *sum_func;
      if (ref->in_sum_func->nest_level > select->nest_level)
        direct_ref= TRUE;
      else
      {
        for (sum_func= ref->in_sum_func; sum_func &&
             sum_func->aggr_level >= select->nest_level;
             sum_func= sum_func->in_sum_func)
        {
          if (sum_func->aggr_level == select->nest_level)
          {
            direct_ref= TRUE;
            break;
          }
        }
      }
    }
    else
    {
      /*
        Check if GROUP BY item trees contain the outer ref:
        in this case we have to use Item_direct_ref instead of Item_ref.
      */
      for (ORDER *group= group_list; group; group= group->next)
      {
        if ((*group->item)->walk(&Item::find_item_processor, walk_subquery,
                                 (uchar *) ref))
        {
          direct_ref= TRUE;
          break;
        }
      }
    }
    new_ref= direct_ref ?
              new Item_direct_ref(ref->context, item_ref, ref->table_name,
                                  ref->field_name, ref->is_alias_of_expr()) :
              new Item_ref(ref->context, item_ref, ref->table_name,
                           ref->field_name, ref->is_alias_of_expr());
    if (!new_ref)
      return TRUE;
    ref->outer_ref= new_ref;
    ref->ref= &ref->outer_ref;

    if (!ref->fixed && ref->fix_fields(thd, 0))
      return TRUE;
    thd->lex->used_tables|= item->used_tables();
    thd->lex->current_select()->select_list_tables|= item->used_tables();
  }
  return false;
}


/**
   Since LIMIT is not supported for table subquery predicates
   (IN/ALL/EXISTS/etc), the following clauses are redundant for
   subqueries:

   ORDER BY
   DISTINCT
   GROUP BY   if there are no aggregate functions and no HAVING
              clause

   Because redundant clauses are removed both from JOIN and
   select_lex, the removal is permanent. Thus, it only makes sense to
   call this function for normal queries and on first execution of
   SP/PS

   @param subq_select_lex   select_lex that is part of a subquery 
                            predicate. This object and the associated 
                            join is modified.
   @param hidden_group_field_count Number of hidden group fields added
                            by setup_group().
   @param hidden_order_field_count Number of hidden order fields added
                            by setup_order().
   @param fields            Fields list from which to remove items.
   @param ref_pointer_array Pointers to top level of all_fields.
*/

static
void remove_redundant_subquery_clauses(st_select_lex *subq_select_lex,
                                       int hidden_group_field_count,
                                       int hidden_order_field_count,
                                       List<Item> &fields,
                                       Ref_ptr_array ref_pointer_array)
{
  Item_subselect *subq_predicate= subq_select_lex->master_unit()->item;
  /*
    The removal should happen for IN, ALL, ANY and EXISTS subqueries,
    which means all but single row subqueries. Example single row
    subqueries: 
       a) SELECT * FROM t1 WHERE t1.a = (<single row subquery>) 
       b) SELECT a, (<single row subquery) FROM t1
   */
  if (subq_predicate->substype() == Item_subselect::SINGLEROW_SUBS)
    return;

  // A subquery that is not single row should be one of IN/ALL/ANY/EXISTS.
  DBUG_ASSERT (subq_predicate->substype() == Item_subselect::EXISTS_SUBS ||
               subq_predicate->substype() == Item_subselect::IN_SUBS     ||
               subq_predicate->substype() == Item_subselect::ALL_SUBS    ||
               subq_predicate->substype() == Item_subselect::ANY_SUBS);

  enum change
  {
    REMOVE_NONE=0,
    REMOVE_ORDER= 1 << 0,
    REMOVE_DISTINCT= 1 << 1,
    REMOVE_GROUP= 1 << 2
  };

  uint changelog= 0;

  if (subq_select_lex->order_list.elements)
  {
    changelog|= REMOVE_ORDER;
    for (ORDER *o= subq_select_lex->order_list.first; o != NULL; o= o->next)
    {
      if (*o->item == o->item_ptr)
        (*o->item)->walk(&Item::clean_up_after_removal, walk_subquery,
                         reinterpret_cast<uchar*>(subq_select_lex));
    }
    subq_select_lex->join->order= NULL;
    subq_select_lex->order_list.empty();
    while (hidden_order_field_count-- > 0)
    {
      fields.pop();
      ref_pointer_array[fields.elements]= NULL;
    }
  }

  if (subq_select_lex->options & SELECT_DISTINCT)
  {
    changelog|= REMOVE_DISTINCT;
    subq_select_lex->join->select_distinct= false;
    subq_select_lex->options&= ~SELECT_DISTINCT;
  }

  /*
    Remove GROUP BY if there are no aggregate functions and no HAVING
    clause
  */
  if (subq_select_lex->group_list.elements &&
      !subq_select_lex->with_sum_func && !subq_select_lex->having_cond())
  {
    changelog|= REMOVE_GROUP;
    for (ORDER *g= subq_select_lex->group_list.first; g != NULL; g= g->next)
    {
      if (*g->item == g->item_ptr)
        (*g->item)->walk(&Item::clean_up_after_removal, walk_subquery,
                         reinterpret_cast<uchar*>(subq_select_lex));
    }
    subq_select_lex->join->group_list= NULL;
    subq_select_lex->group_list.empty();
    while (hidden_group_field_count-- > 0)
    {
      fields.pop();
      ref_pointer_array[fields.elements]= NULL;
    }
  }

  if (changelog)
  {
    Opt_trace_context * trace= &subq_select_lex->join->thd->opt_trace;
    if (unlikely(trace->is_started()))
    {
      Opt_trace_object trace_wrapper(trace);
      Opt_trace_array trace_changes(trace, "transformations_to_subquery");
      if (changelog & REMOVE_ORDER)
        trace_changes.add_alnum("removed_ordering");
      if (changelog & REMOVE_DISTINCT)
        trace_changes.add_alnum("removed_distinct");
      if (changelog & REMOVE_GROUP)
        trace_changes.add_alnum("removed_grouping");
    }
  }
}


/**
  Function to setup clauses without sum functions.
*/
static inline int
setup_without_group(THD *thd, Ref_ptr_array ref_pointer_array,
                    TABLE_LIST *tables,
                    List<Item> &fields,
                    List<Item> &all_fields,
                    ORDER *order,
                    ORDER *group,
                    int *hidden_group_field_count,
                    int *hidden_order_field_count)
{
  int res;
  st_select_lex *const select= thd->lex->current_select();
  nesting_map save_allow_sum_func=thd->lex->allow_sum_func;
  DBUG_ENTER("setup_without_group");

  thd->lex->allow_sum_func&= ~((nesting_map)1 << select->nest_level);
  DBUG_ASSERT(tables == select->get_table_list());
  res= select->setup_conds(thd);

  // GROUP BY
  int all_fields_count= all_fields.elements;
  res= res || setup_group(thd, ref_pointer_array, tables, fields, all_fields,
                          group);
  *hidden_group_field_count= all_fields.elements - all_fields_count;

  // ORDER BY
  all_fields_count= all_fields.elements;
  thd->lex->allow_sum_func|= (nesting_map)1 << select->nest_level;
  res= res || setup_order(thd, ref_pointer_array, tables, fields, all_fields,
                          order);
  *hidden_order_field_count= all_fields.elements - all_fields_count;
  thd->lex->allow_sum_func= save_allow_sum_func;
  DBUG_RETURN(res);
}


/*****************************************************************************
  Group and order functions
*****************************************************************************/

/**
  Resolve an ORDER BY or GROUP BY column reference.

  Given a column reference (represented by 'order') from a GROUP BY or ORDER
  BY clause, find the actual column it represents. If the column being
  resolved is from the GROUP BY clause, the procedure searches the SELECT
  list 'fields' and the columns in the FROM list 'tables'. If 'order' is from
  the ORDER BY clause, only the SELECT list is being searched.

  If 'order' is resolved to an Item, then order->item is set to the found
  Item. If there is no item for the found column (that is, it was resolved
  into a table field), order->item is 'fixed' and is added to all_fields and
  ref_pointer_array.

  ref_pointer_array and all_fields are updated.

  @param[in] thd		     Pointer to current thread structure
  @param[in,out] ref_pointer_array  All select, group and order by fields
  @param[in] tables                 List of tables to search in (usually
    FROM clause)
  @param[in] order                  Column reference to be resolved
  @param[in] fields                 List of fields to search in (usually
    SELECT list)
  @param[in,out] all_fields         All select, group and order by fields
  @param[in] is_group_field         True if order is a GROUP field, false if
    ORDER by field

  @retval
    FALSE if OK
  @retval
    TRUE  if error occurred
*/

static bool
find_order_in_list(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
                   ORDER *order, List<Item> &fields, List<Item> &all_fields,
                   bool is_group_field)
{
  Item *order_item= *order->item; /* The item from the GROUP/ORDER caluse. */
  Item::Type order_item_type;
  Item **select_item; /* The corresponding item from the SELECT clause. */
  Field *from_field;  /* The corresponding field from the FROM clause. */
  uint counter;
  enum_resolution_type resolution;

  /*
    Local SP variables may be int but are expressions, not positions.
    (And they can't be used before fix_fields is called for them).
  */
  if (order_item->type() == Item::INT_ITEM && order_item->basic_const_item())
  {						/* Order by position */
    uint count= (uint) order_item->val_int();
    if (!count || count > fields.elements)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0),
               order_item->full_name(), thd->where);
      return TRUE;
    }
    order->item= &ref_pointer_array[count - 1];
    order->in_field_list= 1;
    return FALSE;
  }
  /* Lookup the current GROUP/ORDER field in the SELECT clause. */
  select_item= find_item_in_list(order_item, fields, &counter,
                                 REPORT_EXCEPT_NOT_FOUND, &resolution);
  if (!select_item)
    return TRUE; /* The item is not unique, or some other error occured. */


  /* Check whether the resolved field is not ambiguos. */
  if (select_item != not_found_item)
  {
    Item *view_ref= NULL;
    /*
      If we have found field not by its alias in select list but by its
      original field name, we should additionally check if we have conflict
      for this name (in case if we would perform lookup in all tables).
    */
    if (resolution == RESOLVED_BEHIND_ALIAS && !order_item->fixed &&
        order_item->fix_fields(thd, order->item))
      return TRUE;

    /* Lookup the current GROUP field in the FROM clause. */
    order_item_type= order_item->type();
    from_field= (Field*) not_found_field;
    if ((is_group_field &&
        order_item_type == Item::FIELD_ITEM) ||
        order_item_type == Item::REF_ITEM)
    {
      from_field= find_field_in_tables(thd, (Item_ident*) order_item, tables,
                                       NULL, &view_ref, IGNORE_ERRORS, TRUE,
                                       FALSE);
      if (!from_field)
        from_field= (Field*) not_found_field;
    }

    if (from_field == not_found_field ||
        (from_field != view_ref_found ?
         /* it is field of base table => check that fields are same */
         ((*select_item)->type() == Item::FIELD_ITEM &&
          ((Item_field*) (*select_item))->field->eq(from_field)) :
         /*
           in is field of view table => check that references on translation
           table are same
         */
         ((*select_item)->type() == Item::REF_ITEM &&
          view_ref->type() == Item::REF_ITEM &&
          ((Item_ref *) (*select_item))->ref ==
          ((Item_ref *) view_ref)->ref)))
    {
      /*
        If there is no such field in the FROM clause, or it is the same field
        as the one found in the SELECT clause, then use the Item created for
        the SELECT field. As a result if there was a derived field that
        'shadowed' a table field with the same name, the table field will be
        chosen over the derived field.

        If we replace *order->item with one from the select list or
        from a table in the FROM list, we should clean up after
        removing the old *order->item from the query. The item has not
        been fixed (so there are no aggregation functions that need
        cleaning up), but it may contain subqueries that should be
        unlinked.
      */
      if (*order->item != *select_item)
        (*order->item)->walk(&Item::clean_up_after_removal, walk_subquery,
                             NULL);
      order->item= &ref_pointer_array[counter];
      order->in_field_list=1;
      if (resolution == RESOLVED_AGAINST_ALIAS)
        order->used_alias= true;
      return FALSE;
    }
    else
    {
      /*
        There is a field with the same name in the FROM clause. This
        is the field that will be chosen. In this case we issue a
        warning so the user knows that the field from the FROM clause
        overshadows the column reference from the SELECT list.
      */
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR),
                          ((Item_ident*) order_item)->field_name,
                          current_thd->where);
    }
  }

  order->in_field_list=0;
  /*
    The call to order_item->fix_fields() means that here we resolve
    'order_item' to a column from a table in the list 'tables', or to
    a column in some outer query. Exactly because of the second case
    we come to this point even if (select_item == not_found_item),
    inspite of that fix_fields() calls find_item_in_list() one more
    time.

    We check order_item->fixed because Item_func_group_concat can put
    arguments for which fix_fields already was called.
    
    group_fix_field= TRUE is to resolve aliases from the SELECT list
    without creating of Item_ref-s: JOIN::exec() wraps aliased items
    in SELECT list with Item_copy items. To re-evaluate such a tree
    that includes Item_copy items we have to refresh Item_copy caches,
    but:
      - filesort() never refresh Item_copy items,
      - end_send_group() checks every record for group boundary by the
        test_if_group_changed function that obtain data from these
        Item_copy items, but the copy_fields function that
        refreshes Item copy items is called after group boundaries only -
        that is a vicious circle.
    So we prevent inclusion of Item_copy items.
  */
  bool save_group_fix_field= thd->lex->current_select()->group_fix_field;
  if (is_group_field)
    thd->lex->current_select()->group_fix_field= TRUE;
  bool ret= (!order_item->fixed &&
      (order_item->fix_fields(thd, order->item) ||
       (order_item= *order->item)->check_cols(1) ||
       thd->is_fatal_error));
  thd->lex->current_select()->group_fix_field= save_group_fix_field;
  if (ret)
    return TRUE; /* Wrong field. */

  uint el= all_fields.elements;
  all_fields.push_front(order_item); /* Add new field to field list. */
  ref_pointer_array[el]= order_item;
  /*
    Currently, we assume that this assertion holds. If it turns out
    that it fails for some query, order->item has changed and the old
    item is removed from the query. In that case, we must call walk()
    with clean_up_after_removal() on the old order->item.
  */
  DBUG_ASSERT(order_item == *order->item);
  order->item= &ref_pointer_array[el];
  return FALSE;
}


/**
  Change order to point at item in select list.

  If item isn't a number and doesn't exists in the select list, add it to the
  the field list.
*/

int setup_order(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List<Item> &all_fields, ORDER *order)
{
  SELECT_LEX *const select= thd->lex->current_select();

  thd->where="order clause";

  const bool for_union= select->master_unit()->is_union() &&
                        select == select->master_unit()->fake_select_lex;
  const bool is_aggregated= select->agg_func_used() ||
                            select->group_list.elements;

  for (uint number= 1; order; order=order->next, number++)
  {
    if (find_order_in_list(thd, ref_pointer_array, tables, order, fields,
			   all_fields, FALSE))
      return 1;
    if ((*order->item)->with_sum_func)
    {
      /*
        Aggregated expressions in ORDER BY are not supported by SQL standard,
        but MySQL has some limited support for them. The limitations are
        checked below:

        1. A UNION query is not aggregated, so ordering by a set function
           is always wrong.
      */
      if (for_union)
      {
        my_error(ER_AGGREGATE_ORDER_FOR_UNION, MYF(0), number);
        return 1;
      }

      /*
        2. A non-aggregated query combined with a set function in ORDER BY
           that does not contain an outer reference is illegal, because it
           would cause the query to become aggregated.
           (Since is_aggregated is false, this expression would cause
            agg_func_used() to become true).
      */
      if (!is_aggregated && select->agg_func_used())
      {
        my_error(ER_AGGREGATE_ORDER_NON_AGG_QUERY, MYF(0), number);
        return 1;
      }
    }
  }
  return 0;
}


/**
   Runs checks mandated by ONLY_FULL_GROUP_BY

   @param  thd                     THD pointer
   @param  select                  Query block

   @returns true if ONLY_FULL_GROUP_BY is violated.
*/

bool SELECT_LEX::check_only_full_group_by(THD *thd)
{
  bool rc= false;

  if (group_list.elements || agg_func_used())
  {
    MEM_ROOT root;
    /*
      "root" has very short lifetime, and should not consume much
      => not instrumented.
    */
    init_sql_alloc(PSI_NOT_INSTRUMENTED, &root, MEM_ROOT_BLOCK_SIZE, 0);
    {
      Group_check gc(this, &root);
      rc= gc.check_query(thd);
      gc.to_opt_trace(thd);
    } // scope, to let any destructor run before free_root().
    free_root(&root, MYF(0));
  }

  if (!rc &&
      (options & SELECT_DISTINCT) &&
      // aggregate without GROUP => single-row result => don't bother user
      !(!group_list.elements && agg_func_used()))
  {
    Distinct_check dc(this);
    rc= dc.check_query(thd);
  }

  return rc;
}


/**
  Initialize the GROUP BY list.

  @param thd			Thread handler
  @param ref_pointer_array	We store references to all fields that was
                               not in 'fields' here.
  @param fields		All fields in the select part. Any item in
                               'order' that is part of these list is replaced
                               by a pointer to this fields.
  @param all_fields		Total list of all unique fields used by the
                               select. All items in 'order' that was not part
                               of fields will be added first to this list.
  @param order			The fields we should do GROUP BY on.

  @retval
    0  ok
  @retval
    1  error (probably out of memory)
*/

static int
setup_group(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
            List<Item> &fields, List<Item> &all_fields, ORDER *order)
{
  if (!order)
    return 0;				/* Everything is ok */

  thd->where="group statement";
  for (ORDER *ord= order; ord; ord= ord->next)
  {
    if (find_order_in_list(thd, ref_pointer_array, tables, ord, fields,
			   all_fields, TRUE))
      return 1;
    if ((*ord->item)->with_sum_func)
    {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), (*ord->item)->full_name());
      return 1;
    }
  }
  return 0;
}


/****************************************************************************
  ROLLUP handling
****************************************************************************/

/**
  Replace occurences of group by fields in an expression by ref items.

  The function replaces occurrences of group by fields in expr
  by ref objects for these fields unless they are under aggregate
  functions.
  The function also corrects value of the the maybe_null attribute
  for the items of all subexpressions containing group by fields.

  @b EXAMPLES
    @code
      SELECT a+1 FROM t1 GROUP BY a WITH ROLLUP
      SELECT SUM(a)+a FROM t1 GROUP BY a WITH ROLLUP 
  @endcode

  @b IMPLEMENTATION

    The function recursively traverses the tree of the expr expression,
    looks for occurrences of the group by fields that are not under
    aggregate functions and replaces them for the corresponding ref items.

  @note
    This substitution is needed GROUP BY queries with ROLLUP if
    SELECT list contains expressions over group by attributes.

  @param thd                  reference to the context
  @param expr                 expression to make replacement
  @param group_list           list of references to group by items
  @param changed        out:  returns 1 if item contains a replaced field item

  @todo
    - TODO: Some functions are not null-preserving. For those functions
    updating of the maybe_null attribute is an overkill. 

  @retval
    0	if ok
  @retval
    1   on error
*/

static bool change_group_ref(THD *thd, Item_func *expr, ORDER *group_list,
                             bool *changed)
{
  if (expr->arg_count)
  {
    Name_resolution_context *context= &thd->lex->current_select()->context;
    Item **arg,**arg_end;
    bool arg_changed= FALSE;
    for (arg= expr->arguments(),
         arg_end= expr->arguments()+expr->arg_count;
         arg != arg_end; arg++)
    {
      Item *item= *arg;
      if (item->type() == Item::FIELD_ITEM || item->type() == Item::REF_ITEM)
      {
        ORDER *group_tmp;
        for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
        {
          if (item->eq(*group_tmp->item,0))
          {
            Item *new_item;
            if (!(new_item= new Item_ref(context, group_tmp->item, 0,
                                        item->item_name.ptr())))
              return 1;                                 // fatal_error is set
            thd->change_item_tree(arg, new_item);
            arg_changed= TRUE;
          }
        }
      }
      else if (item->type() == Item::FUNC_ITEM)
      {
        if (change_group_ref(thd, (Item_func *) item, group_list, &arg_changed))
          return 1;
      }
    }
    if (arg_changed)
    {
      expr->maybe_null= 1;
      *changed= TRUE;
    }
  }
  return 0;
}


/** Allocate memory needed for other rollup functions. */

bool JOIN::rollup_init()
{
  uint i,j;
  Item **ref_array;
  ORDER *group_tmp;

  tmp_table_param.quick_group= 0;	// Can't create groups in tmp table
  rollup.state= ROLLUP::STATE_INITED;

  /*
    Create pointers to the different sum function groups
    These are updated by rollup_make_fields()
  */
  tmp_table_param.group_parts= send_group_parts;

  Item_null_result **null_items=
    static_cast<Item_null_result**>(thd->alloc(sizeof(Item*)*send_group_parts));

  rollup.null_items= Item_null_array(null_items, send_group_parts);
  rollup.ref_pointer_arrays=
    static_cast<Ref_ptr_array*>
    (thd->alloc((sizeof(Ref_ptr_array) +
                 all_fields.elements * sizeof(Item*)) * send_group_parts));
  rollup.fields=
    static_cast<List<Item>*>(thd->alloc(sizeof(List<Item>) * send_group_parts));

  if (!null_items || !rollup.ref_pointer_arrays || !rollup.fields)
    return true;

  ref_array= (Item**) (rollup.ref_pointer_arrays+send_group_parts);

  /*
    Prepare space for field list for the different levels
    These will be filled up in rollup_make_fields()
  */
  group_tmp= group_list;
  for (i= 0 ; i < send_group_parts ; i++)
  {
    rollup.null_items[i]=
      new (thd->mem_root) Item_null_result((*group_tmp->item)->field_type(),
                                           (*group_tmp->item)->result_type());
    List<Item> *rollup_fields= &rollup.fields[i];
    rollup_fields->empty();
    rollup.ref_pointer_arrays[i]= Ref_ptr_array(ref_array, all_fields.elements);
    ref_array+= all_fields.elements;
    group_tmp= group_tmp->next;
  }
  for (i= 0 ; i < send_group_parts; i++)
  {
    for (j=0 ; j < fields_list.elements ; j++)
      rollup.fields[i].push_back(rollup.null_items[i]);
  }
  List_iterator<Item> it(all_fields);
  Item *item;
  while ((item= it++))
  {
    bool found_in_group= 0;

    for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
    {
      if (*group_tmp->item == item)
      {
        item->maybe_null= 1;
        found_in_group= 1;
        break;
      }
    }
    if (item->type() == Item::FUNC_ITEM && !found_in_group)
    {
      bool changed= FALSE;
      if (change_group_ref(thd, (Item_func *) item, group_list, &changed))
        return 1;
      /*
        We have to prevent creation of a field in a temporary table for
        an expression that contains GROUP BY attributes.
        Marking the expression item as 'with_sum_func' will ensure this.
      */ 
      if (changed)
        item->with_sum_func= 1;
    }
  }
  return 0;
}


/**
  @} (end of group Query_Resolver)
*/
