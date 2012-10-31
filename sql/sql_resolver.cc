/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_acl.h"
#include "opt_explain_format.h"

static void remove_redundant_subquery_clauses(st_select_lex *subq_select_lex);
static inline int 
setup_without_group(THD *thd, Ref_ptr_array ref_pointer_array,
                    TABLE_LIST *tables,
                    TABLE_LIST *leaves,
                    List<Item> &fields,
                    List<Item> &all_fields,
                    Item **conds,
                    ORDER *order,
                    ORDER *group, bool *hidden_group_fields);
static bool resolve_subquery(THD *thd, JOIN *join);
static int
setup_group(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
            List<Item> &fields, List<Item> &all_fields, ORDER *order);
static bool
match_exprs_for_only_full_group_by(THD *thd, List<Item> &all_fields,
                                   int hidden_group_exprs_count,
                                   int hidden_order_exprs_count,
                                   int select_exprs_count,
                                   ORDER *group_exprs);


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
int
JOIN::prepare(TABLE_LIST *tables_init,
	      uint wild_num, Item *conds_init, uint og_num,
	      ORDER *order_init, ORDER *group_init,
	      Item *having_init,
	      SELECT_LEX *select_lex_arg,
	      SELECT_LEX_UNIT *unit_arg)
{
  DBUG_ENTER("JOIN::prepare");

  // to prevent double initialization on EXPLAIN
  if (optimized)
    DBUG_RETURN(0);

  if (order_init)
    explain_flags.set(ESC_ORDER_BY, ESP_EXISTS);
  if (group_init)
    explain_flags.set(ESC_GROUP_BY, ESP_EXISTS);
  if (select_options & SELECT_DISTINCT)
    explain_flags.set(ESC_DISTINCT, ESP_EXISTS);

  conds= conds_init;
  order= ORDER_with_src(order_init, ESC_ORDER_BY);
  group_list= ORDER_with_src(group_init, ESC_GROUP_BY);
  having= having_for_explain= having_init;
  tables_list= tables_init;
  select_lex= select_lex_arg;
  select_lex->join= this;
  join_list= &select_lex->top_join_list;
  union_part= unit_arg->is_union();

  thd->lex->current_select->is_item_list_lookup= 1;
  /*
    If we have already executed SELECT, then it have not sense to prevent
    its table from update (see unique_table())
  */
  if (thd->derived_tables_processing)
    select_lex->exclude_from_table_unique_test= TRUE;

  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_prepare(trace, "join_preparation");
  trace_prepare.add_select_number(select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");

  /* Check that all tables, fields, conds and order are ok */

  if (!(select_options & OPTION_SETUP_TABLES_DONE) &&
      setup_tables_and_check_access(thd, &select_lex->context, join_list,
                                    tables_list, &select_lex->leaf_tables,
                                    FALSE, SELECT_ACL, SELECT_ACL))
      DBUG_RETURN(-1);
 
  TABLE_LIST *table_ptr;
  for (table_ptr= select_lex->leaf_tables;
       table_ptr;
       table_ptr= table_ptr->next_leaf)
    primary_tables++;           // Count the primary input tables of the query

  tables= primary_tables;       // This is currently the total number of tables

  /*
    Item and Item_field CTORs will both increment some counters
    in current_select, based on the current parsing context.
    We are not parsing anymore: any new Items created now are due to
    query rewriting, so stop incrementing counters.
   */
  DBUG_ASSERT(select_lex->parsing_place == NO_MATTER);
  select_lex->parsing_place= NO_MATTER;

  if (setup_wild(thd, tables_list, fields_list, &all_fields, wild_num))
    DBUG_RETURN(-1);
  if (select_lex->setup_ref_array(thd, og_num))
    DBUG_RETURN(-1);

  ref_ptrs= ref_ptr_array_slice(0);
  
  if (setup_fields(thd, ref_ptrs, fields_list, MARK_COLUMNS_READ,
		   &all_fields, 1))
    DBUG_RETURN(-1);
  if (setup_without_group(thd, ref_ptrs, tables_list,
			  select_lex->leaf_tables, fields_list,
			  all_fields, &conds, order, group_list,
			  &hidden_group_fields))
    DBUG_RETURN(-1);

  /*
    Permanently remove redundant parts from the query if
      1) This is a subquery
      2) This is the first time this query is optimized (since the
         transformation is permanent)
      3) Not normalizing a view. Removal should take place when a
         query involving a view is optimized, not when the view
         is created
  */
  if (select_lex->master_unit()->item &&                               // 1)
      select_lex->first_cond_optimization &&                           // 2)
      !(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW)) // 3)
  {
    remove_redundant_subquery_clauses(select_lex);
  }

  if (having)
  {
    nesting_map save_allow_sum_func= thd->lex->allow_sum_func;
    thd->where="having clause";
    thd->lex->allow_sum_func|= 1 << select_lex_arg->nest_level;
    select_lex->having_fix_field= 1;
    select_lex->resolve_place= st_select_lex::RESOLVE_HAVING;
    bool having_fix_rc= (!having->fixed &&
			 (having->fix_fields(thd, &having) ||
			  having->check_cols(1)));
    select_lex->having_fix_field= 0;
    select_lex->having= having;

    select_lex->resolve_place= st_select_lex::RESOLVE_NONE;
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
    opt_trace_print_expanded_query(thd, select_lex, &trace_wrapper);
  }

  /*
    When normalizing a view (like when writing a view's body to the FRM),
    subquery transformations don't apply (if they did, IN->EXISTS could not be
    undone in favour of materialization, when optimizing a later statement
    using the view)
  */
  if (select_lex->master_unit()->item &&    // This is a subquery
                                            // Not normalizing a view
      !(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      !(select_options & SELECT_DESCRIBE))  // Not within a describe
  {
    /* Join object is a subquery within an IN/ANY/ALL/EXISTS predicate */
    if (resolve_subquery(thd, this))
      DBUG_RETURN(-1);
  }

  select_lex->fix_prepare_information(thd, &conds, &having);

  if (order)
  {
    bool real_order= FALSE;
    ORDER *ord;
    for (ord= order; ord; ord= ord->next)
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
        item->split_sum_func(thd, ref_ptrs, all_fields);
    }
    if (!real_order)
      order= NULL;
  }

  if (having && having->with_sum_func)
    having->split_sum_func2(thd, ref_ptrs,
                            all_fields, &having, TRUE);
  if (select_lex->inner_sum_func_list)
  {
    Item_sum *end=select_lex->inner_sum_func_list;
    Item_sum *item_sum= end;  
    do
    { 
      item_sum= item_sum->next;
      item_sum->split_sum_func2(thd, ref_ptrs,
                                all_fields, item_sum->ref_by, FALSE);
    } while (item_sum != end);
  }

  if (select_lex->inner_refs_list.elements &&
      fix_inner_refs(thd, all_fields, select_lex, ref_ptrs,
                     group_list))
    DBUG_RETURN(-1);

  if (group_list)
  {
    /*
      Because HEAP tables can't index BIT fields we need to use an
      additional hidden field for grouping because later it will be
      converted to a LONG field. Original field will remain of the
      BIT type and will be returned to a client.
    */
    for (ORDER *ord= group_list; ord; ord= ord->next)
    {
      if ((*ord->item)->type() == Item::FIELD_ITEM &&
          (*ord->item)->field_type() == MYSQL_TYPE_BIT)
      {
        Item_field *field= new Item_field(thd, *(Item_field**)ord->item);
        int el= all_fields.elements;
        ref_ptrs[el]= field;
        all_fields.push_front(field);
        ord->item= &ref_ptrs[el];
      }
    }
  }

  if (setup_ftfuncs(select_lex)) /* should be after having->fix_fields */
    DBUG_RETURN(-1);
  

  /*
    Check if there are references to un-aggregated columns when computing 
    aggregate functions with implicit grouping (there is no GROUP BY).
  */
  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY && !group_list &&
      select_lex->non_agg_field_used() &&
      select_lex->agg_func_used())
  {
    my_message(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,
               ER(ER_MIX_OF_GROUP_FUNC_AND_FIELDS), MYF(0));
    DBUG_RETURN(-1);
  }
  {
    /* Caclulate the number of groups */
    send_group_parts= 0;
    for (ORDER *group_tmp= group_list ; group_tmp ; group_tmp= group_tmp->next)
      send_group_parts++;
  }


  if (result && result->prepare(fields_list, unit_arg))
    goto err;					/* purecov: inspected */

  /* Init join struct */
  count_field_types(select_lex, &tmp_table_param, all_fields, false, false);
  this->group= group_list != 0;
  unit= unit_arg;

  if (tmp_table_param.sum_func_count && !group_list)
  {
    implicit_grouping= TRUE;
    // Result will contain zero or one row - ordering is meaningless
    order= NULL;
  }

#ifdef RESTRICTED_GROUP
  if (implicit_grouping)
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT),MYF(0));
    goto err;
  }
#endif
  if (select_lex->olap == ROLLUP_TYPE && rollup_init())
    goto err;
  if (alloc_func_list())
    goto err;

#ifdef WITH_PARTITION_STORAGE_ENGINE
  {
    TABLE_LIST *tbl;
    for (tbl= select_lex->leaf_tables; tbl; tbl= tbl->next_leaf)
    {
      /* 
        This will only prune constant conditions, which will be used for
        lock pruning.
      */
      Item *prune_cond= tbl->join_cond() ? tbl->join_cond() : conds;
      if (prune_partitions(thd, tbl->table, prune_cond))
        goto err;
    }
  }
#endif

  DBUG_RETURN(0); // All OK

err:
  DBUG_RETURN(-1);				/* purecov: inspected */
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
  else if (predicate->originally_dependent())
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
  @param join    Join that is part of a subquery predicate.

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

static bool resolve_subquery(THD *thd, JOIN *join)
{
  DBUG_ENTER("resolve_subquery");

  bool chose_semijoin= false;
  SELECT_LEX *const select_lex= join->select_lex;
  SELECT_LEX *const outer= select_lex->outer_select();

  /*
    @todo for PS, make the whole block execute only on the first execution.
    resolve_subquery() is only invoked in the first execution for subqueries
    that are transformed to semijoin, but for other subqueries, this function
    is called for every execution. One solution is perhaps to define
    exec_method in class Item_subselect and exit immediately if unequal to
    EXEC_UNSPECIFIED.
  */
  Item_subselect *subq_predicate= select_lex->master_unit()->item;
  DBUG_ASSERT(subq_predicate);
  /**
    @note
    In this case: IN (SELECT ... UNION SELECT ...), JOIN::prepare() is
    called for each of the two UNION members, and in those two calls,
    subq_predicate is the same, not sure this is desired (double work?).
  */

  Item_in_subselect * const in_predicate=
    (subq_predicate->substype() == Item_subselect::IN_SUBS) ?
    static_cast<Item_in_subselect *>(subq_predicate) : NULL;

  if (in_predicate)
  {
    /*
      Check if the left and right expressions have the same # of
      columns, i.e. we don't have a case like 
        (oe1, oe2) IN (SELECT ie1, ie2, ie3 ...)

      TODO why do we have this duplicated in IN->EXISTS transformers?
      psergey-todo: fix these: grep for duplicated_subselect_card_check
    */
    if (select_lex->item_list.elements != in_predicate->left_expr->cols())
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), in_predicate->left_expr->cols());
      DBUG_RETURN(TRUE);
    }

    DBUG_ASSERT(select_lex == thd->lex->current_select);
    thd->lex->current_select= outer;
    char const *save_where= thd->where;
    thd->where= "IN/ALL/ANY subquery";
        
    bool result= !in_predicate->left_expr->fixed &&
                  in_predicate->left_expr->fix_fields(thd,
                                                     &in_predicate->left_expr);
    thd->lex->current_select= select_lex;
    thd->where= save_where;
    if (result)
      DBUG_RETURN(TRUE); /* purecov: deadcode */
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
      6. We are not in a subquery of a single table UPDATE/DELETE that 
           doesn't have a JOIN (TODO: We should handle this at some
           point by switching to multi-table UPDATE/DELETE)
      7. We're not in a confluent table-less subquery, like "SELECT 1".
      8. No execution method was already chosen (by a prepared statement)
      9. Parent select is not a confluent table-less select
      10. Neither parent nor child select have STRAIGHT_JOIN option.
  */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SEMIJOIN) &&
      in_predicate &&                                                   // 1
      !select_lex->is_part_of_union() &&                                // 2
      !select_lex->group_list.elements &&                               // 3
      !join->having && !select_lex->with_sum_func &&                    // 4
      (outer->resolve_place == st_select_lex::RESOLVE_CONDITION ||      // 5
       outer->resolve_place == st_select_lex::RESOLVE_JOIN_NEST) &&     // 5
      outer->join &&                                                    // 6
      select_lex->master_unit()->first_select()->leaf_tables &&         // 7
      in_predicate->exec_method ==
                           Item_exists_subselect::EXEC_UNSPECIFIED &&   // 8
      outer->leaf_tables &&                                             // 9
      !((join->select_options | outer->join->select_options)
        & SELECT_STRAIGHT_JOIN))                                        // 10
  {
    DBUG_PRINT("info", ("Subquery is semi-join conversion candidate"));

    /* Notify in the subquery predicate where it belongs in the query graph */
    in_predicate->embedding_join_nest= outer->resolve_nest;

    /* Register the subquery for further processing in flatten_subqueries() */
    outer->join->sj_subselects.push_back(in_predicate);
    chose_semijoin= true;
  }

  if (in_predicate)
  {
    Opt_trace_context * const trace= &join->thd->opt_trace;
    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        select_lex->select_number, "IN (SELECT)", "semijoin");
    oto1.add("chosen", chose_semijoin);
  }

  if (!chose_semijoin &&
      subq_predicate->select_transformer(join) == Item_subselect::RES_ERROR)
    DBUG_RETURN(TRUE);

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
        if ((*group->item)->walk(&Item::find_item_processor, TRUE,
                                 (uchar *) ref))
        {
          direct_ref= TRUE;
          break;
        }
      }
    }
    new_ref= direct_ref ?
              new Item_direct_ref(ref->context, item_ref, ref->table_name,
                          ref->field_name, ref->alias_name_used) :
              new Item_ref(ref->context, item_ref, ref->table_name,
                          ref->field_name, ref->alias_name_used);
    if (!new_ref)
      return TRUE;
    ref->outer_ref= new_ref;
    ref->ref= &ref->outer_ref;

    if (!ref->fixed && ref->fix_fields(thd, 0))
      return TRUE;
    thd->lex->used_tables|= item->used_tables();
    thd->lex->current_select->select_list_tables|= item->used_tables();
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
*/

static
void remove_redundant_subquery_clauses(st_select_lex *subq_select_lex)
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

  bool order_with_sum_func= false;
  for (ORDER *o= subq_select_lex->join->order; o != NULL; o= o->next)
    order_with_sum_func|= (*o->item)->with_sum_func;
  if (subq_select_lex->order_list.elements)
  {
    changelog|= REMOVE_ORDER;
    subq_select_lex->join->order= NULL;
    /*
      If the ORDER BY clause contains aggregate functions, we cannot
      remove it from subq_select_lex->order_list since the aggregate
      function still appears in the inner_sum_func_list for some
      SELECT_LEX. Clearing subq_select_lex->join->order has made sure
      it won't be executed.
     */
    if (!order_with_sum_func)
      subq_select_lex->order_list.empty();
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
      !subq_select_lex->with_sum_func && !subq_select_lex->join->having)
  {
    changelog|= REMOVE_GROUP;
    subq_select_lex->join->group_list= NULL;
    subq_select_lex->group_list.empty();
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
                    TABLE_LIST *leaves,
                    List<Item> &fields,
                    List<Item> &all_fields,
                    Item **conds,
                    ORDER *order,
                    ORDER *group, bool *hidden_group_fields)
{
  int res;
  nesting_map save_allow_sum_func=thd->lex->allow_sum_func ;
  /* 
    Need to save the value, so we can turn off only any new non_agg_field_used
    additions coming from the WHERE
  */
  const bool saved_non_agg_field_used=
    thd->lex->current_select->non_agg_field_used();
  DBUG_ENTER("setup_without_group");

  thd->lex->allow_sum_func&= ~(1 << thd->lex->current_select->nest_level);
  res= setup_conds(thd, tables, leaves, conds);

  /* it's not wrong to have non-aggregated columns in a WHERE */
  thd->lex->current_select->set_non_agg_field_used(saved_non_agg_field_used);

  thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;

  int all_fields_count= all_fields.elements;

  res= res || setup_order(thd, ref_pointer_array, tables, fields, all_fields,
                          order);

  const int hidden_order_fields_count= all_fields.elements - all_fields_count;
  all_fields_count= all_fields.elements;

  thd->lex->allow_sum_func&= ~(1 << thd->lex->current_select->nest_level);

  res= res || setup_group(thd, ref_pointer_array, tables, fields, all_fields,
                          group);
  const int hidden_group_fields_count= all_fields.elements - all_fields_count;
  *hidden_group_fields= hidden_group_fields_count != 0;

  res= res || match_exprs_for_only_full_group_by(thd, all_fields,
                                                 hidden_group_fields_count,
                                                 hidden_order_fields_count,
                                                 fields.elements, group);

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
    order->counter= count;
    order->counter_used= 1;
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
      original field name, we should additionaly check if we have conflict
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
      */
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
  bool save_group_fix_field= thd->lex->current_select->group_fix_field;
  if (is_group_field)
    thd->lex->current_select->group_fix_field= TRUE;
  bool ret= (!order_item->fixed &&
      (order_item->fix_fields(thd, order->item) ||
       (order_item= *order->item)->check_cols(1) ||
       thd->is_fatal_error));
  thd->lex->current_select->group_fix_field= save_group_fix_field;
  if (ret)
    return TRUE; /* Wrong field. */

  uint el= all_fields.elements;
  all_fields.push_front(order_item); /* Add new field to field list. */
  ref_pointer_array[el]= order_item;
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
  thd->where="order clause";
  DBUG_ASSERT(thd->lex->current_select->cur_pos_in_all_fields ==
              SELECT_LEX::ALL_FIELDS_UNDEF_POS);
  for (; order; order=order->next)
  {
    thd->lex->current_select->cur_pos_in_all_fields=
      fields.elements - all_fields.elements - 1;
    if (find_order_in_list(thd, ref_pointer_array, tables, order, fields,
			   all_fields, FALSE))
      return 1;
  }
  thd->lex->current_select->cur_pos_in_all_fields=
		SELECT_LEX::ALL_FIELDS_UNDEF_POS;
  return 0;
}


/**
   Scans the SELECT list and ORDER BY list: for each expression, if it is not
   present in GROUP BY, examines the non-aggregated columns contained in the
   expression; if those columns are not all in GROUP BY, raise an error.

   Examples:
   1) "SELECT a+1 FROM t GROUP BY a+1"
   "a+1" in SELECT list was found, by setup_group() (exactly
   find_order_in_list()), to be the same as "a+1" in GROUP BY; as it is a
   GROUP BY expression, setup_group() has marked this expression with
   ALL_FIELDS_UNDEF_POS (item->marker= ALL_FIELDS_UNDEF_POS).
   2) "SELECT a+1 FROM t GROUP BY a"
   "a+1" is not found in GROUP BY; its non-aggregated column is "a", "a" is
   present in GROUP BY so it's ok.

   A "hidden" GROUP BY / ORDER BY expression is a member of GROUP BY / ORDER
   BY which was not found (by setup_order() or setup_group()) to be also
   present in the SELECT list. setup_order() and setup_group() have thus added
   the expression to the front of JOIN::all_fields.

   @param  thd                     THD pointer
   @param  all_fields              list of expressions, including SELECT list
                                   and hidden ORDER BY expressions
   @param  hidden_group_exprs_count the list starts with that many hidden
                                   GROUP BY expressions
   @param  hidden_order_exprs_count and continues with that many hidden ORDER
                                   BY expressions
   @param  select_exprs_cout       and ends with that many SELECT list
                                   expressions (there may be a gap between
                                   hidden ORDER BY expressions and SELECT list
                                   expressions)
   @param  group_exprs             GROUP BY expressions

   @returns true if ONLY_FULL_GROUP_BY is violated.
*/

static bool
match_exprs_for_only_full_group_by(THD *thd, List<Item> &all_fields,
                                   int hidden_group_exprs_count,
                                   int hidden_order_exprs_count,
                                   int select_exprs_count,
                                   ORDER *group_exprs)
{
  if (!group_exprs ||
      !(thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY))
    return false;

  /*
    For all expressions of the SELECT list and ORDER BY, a list of columns
    which aren't under an aggregate function, 'select_lex->non_agg_fields',
    has been created (see Item_field::fix_fields()). Each column in that list
    keeps in Item::marker the position, in all_fields, of the (SELECT
    list or ORDER BY) expression which it belongs to (see
    st_select_lex::cur_pos_in_all_fields). all_fields looks like this:
    (front) HIDDEN GROUP BY - HIDDEN ORDER BY - gap - SELECT LIST (back)
    "Gap" may contain some aggregate expressions (see Item::split_sum_func2())
    which are irrelevant to us.

    We take an expressions of the SELECT list or a hidden ORDER BY expression
    ('expr' variable).
    - (1) If it also belongs to the GROUP BY list, it's ok.
    - (2) If it is an aggregate function, it's ok.
    - (3) If is is a constant, it's ok.
    - (4) If it is a column resolved to an outer SELECT it's ok;
    indeed, it is a constant from the point of view of one execution of the
    inner SELECT - it does not introduce any randomness in the result.
    - Otherwise we scan the list of non-aggregated columns and if we find at
    least one column belonging to this expression and NOT occuring
    in the GROUP BY list, we throw an error.
  */
  List_iterator<Item> exprs_it(all_fields);
  /*
    All "idx*" variables below are indices in all_fields, with "index of
    front" = 0 and "index of back" = all_fields.elements - 1.
  */
  int idx= -1;
  const int idx_of_first_hidden_order= hidden_group_exprs_count;
  const int idx_of_last_hidden_order= idx_of_first_hidden_order +
    hidden_order_exprs_count - 1;
  const int idx_of_first_select= all_fields.elements - select_exprs_count;
  /*
    Also an index in all_fields, but with the same counting convention as
    st_select_lex::cur_pos_in_all_fields.
  */
  int cur_pos_in_all_fields;
  Item *expr;
  Item_field *non_agg_field;
  List_iterator<Item_field>
    non_agg_fields_it(thd->lex->current_select->non_agg_fields);

  non_agg_field= non_agg_fields_it++;
  while (non_agg_field && (expr= exprs_it++))
  {
    idx++;
    if (idx < idx_of_first_hidden_order ||      // In hidden GROUP BY.
        (idx > idx_of_last_hidden_order &&      // After hidden ORDER BY,
         idx < idx_of_first_select))            // but not yet in SELECT list
      continue;
    cur_pos_in_all_fields= idx - idx_of_first_select;

    if ((expr->marker == SELECT_LEX::ALL_FIELDS_UNDEF_POS) ||  // (1)
        expr->type() == Item::SUM_FUNC_ITEM ||                 // (2)
        expr->const_item() ||                                  // (3)
        (expr->real_item()->type() == Item::FIELD_ITEM &&
         expr->used_tables() & OUTER_REF_TABLE_BIT))           // (4)
      continue; // Ignore this expression.

    while (non_agg_field)
    {
      /*
        All non-aggregated columns contained in 'expr' have their
        'marker' equal to 'cur_pos_in_all_fields' OR equal to
        ALL_FIELDS_UNDEF_POS. The latter case happens in:
        "SELECT a FROM t GROUP BY a"
        when setup_group() finds that "a" in GROUP BY is also in the
        SELECT list ('fields' list); setup_group() marks the "a" expression
        with ALL_FIELDS_UNDEF_POS; at the same time, "a" is also a
        non-aggregated column of the "a" expression; thus, non-aggregated
        column "a" had its marker change from >=0 to
        ALL_FIELDS_UNDEF_POS. Such non-aggregated column can be ignored (and
        that is why ALL_FIELDS_UNDEF_POS is a very negative number).
      */
      if (non_agg_field->marker < cur_pos_in_all_fields)
      {
        /*
          Ignorable column, or the owning expression was found to be
          ignorable (cases 1-2-3-4 above); ignore it and switch to next
          column.
        */
        goto next_non_agg_field;
      }
      if (non_agg_field->marker > cur_pos_in_all_fields)
      {
        /*
          'expr' has been passed (we have scanned all its non-aggregated
          columns and are seeing one which belongs to a next expression),
          switch to next expression.
        */
        break;
      }
      // Check whether the non-aggregated column occurs in the GROUP BY list
      for (ORDER *grp= group_exprs; grp; grp= grp->next)
        if ((*grp->item)->eq(static_cast<Item *>(non_agg_field), false))
        {
          // column is in GROUP BY so is ok; check the next
          goto next_non_agg_field;
        }
      /*
        If we come here, one non-aggregated column belonging to 'expr' was
        not found in GROUP BY, we raise an error.
        TODO: change ER_WRONG_FIELD_WITH_GROUP to more detailed
        ER_NON_GROUPING_FIELD_USED
      */
      my_error(ER_WRONG_FIELD_WITH_GROUP, MYF(0), non_agg_field->full_name());
      return true;
  next_non_agg_field:
      non_agg_field= non_agg_fields_it++;
    }
  }
  return false;
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
  @param hidden_group_fields	Pointer to flag that is set to 1 if we added
                               any fields to all_fields.

  @todo
    change ER_WRONG_FIELD_WITH_GROUP to more detailed
    ER_NON_GROUPING_FIELD_USED

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
    // ONLY_FULL_GROUP_BY needn't verify this expression:
    (*ord->item)->marker= SELECT_LEX::ALL_FIELDS_UNDEF_POS;
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
    Name_resolution_context *context= &thd->lex->current_select->context;
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
