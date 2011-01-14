/**
  @file

  @brief
    Subquery optimization code here.

*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include "opt_subselect.h"

#include <my_bit.h>

// Our own:
static
bool subquery_types_allow_materialization(Item_in_subselect *in_subs);
static bool replace_where_subcondition(JOIN *join, Item **expr, 
                                       Item *old_cond, Item *new_cond,
                                       bool do_fix_fields);
static int subq_sj_candidate_cmp(Item_in_subselect* const *el1, 
                                 Item_in_subselect* const *el2);
static bool convert_subq_to_sj(JOIN *parent_join, Item_in_subselect *subq_pred);
static TABLE_LIST *alloc_join_nest(THD *thd);
static 
void fix_list_after_tbl_changes(SELECT_LEX *new_parent, List<TABLE_LIST> *tlist);
static uint get_tmp_table_rec_length(List<Item> &items);
bool find_eq_ref_candidate(TABLE *table, table_map sj_inner_tables);
static SJ_MATERIALIZATION_INFO *
at_sjmat_pos(const JOIN *join, table_map remaining_tables, const JOIN_TAB *tab,
             uint idx, bool *loose_scan);
void best_access_path(JOIN *join, JOIN_TAB *s, 
                             table_map remaining_tables, uint idx, 
                             bool disable_jbuf, double record_count,
                             POSITION *pos, POSITION *loose_scan_pos);

static Item *create_subq_in_equalities(THD *thd, SJ_MATERIALIZATION_INFO *sjm, 
                                Item_in_subselect *subq_pred);
static void remove_sj_conds(Item **tree);
static bool is_cond_sj_in_equality(Item *item);
static bool sj_table_is_included(JOIN *join, JOIN_TAB *join_tab);
static Item *remove_additional_cond(Item* conds);
static void remove_subq_pushed_predicates(JOIN *join, Item **where);


/*
  Check if we need JOIN::prepare()-phase subquery rewrites and if yes, do them

  DESCRIPTION
    Check if we need to do
     - subquery->semi-join rewrite
     - if the subquery can be handled with materialization
     - 'substitution' rewrite for table-less subqueries like "(select 1)"

    and mark appropriately

  RETURN
     0  - OK
    -1  - Some sort of query error
*/

int check_and_do_in_subquery_rewrites(JOIN *join)
{
  THD *thd=join->thd;
  st_select_lex *select_lex= join->select_lex;
  DBUG_ENTER("check_and_do_in_subquery_rewrites");
  /*
    If 
      1) this join is inside a subquery (of any type except FROM-clause 
         subquery) and
      2) we aren't just normalizing a VIEW

    Then perform early unconditional subquery transformations:
     - Convert subquery predicate into semi-join, or
     - Mark the subquery for execution using materialization, or
     - Perform IN->EXISTS transformation, or
     - Perform more/less ALL/ANY -> MIN/MAX rewrite
     - Substitute trivial scalar-context subquery with its value

    TODO: for PS, make the whole block execute only on the first execution
  */
  Item_subselect *subselect;
  if (!thd->lex->view_prepare_mode &&                  // (1)
    (subselect= select_lex->master_unit()->item))      // (2)
  {
    Item_in_subselect *in_subs= NULL;
    if (subselect->substype() == Item_subselect::IN_SUBS)
      in_subs= (Item_in_subselect*)subselect;

    /* Resolve expressions and perform semantic analysis for IN query */
    if (in_subs != NULL)
      /*
        TODO: Add the condition below to this if statement when we have proper
        support for is_correlated handling for materialized semijoins.
        If we were to add this condition now, the fix_fields() call in
        convert_subq_to_sj() would force the flag is_correlated to be set
        erroneously for prepared queries.

        thd->stmt_arena->state != Query_arena::PREPARED)
      */
    {
      /*
        Check if the left and right expressions have the same # of
        columns, i.e. we don't have a case like 
          (oe1, oe2) IN (SELECT ie1, ie2, ie3 ...)

        TODO why do we have this duplicated in IN->EXISTS transformers?
        psergey-todo: fix these: grep for duplicated_subselect_card_check
      */
      if (select_lex->item_list.elements != in_subs->left_expr->cols())
      {
        my_error(ER_OPERAND_COLUMNS, MYF(0), in_subs->left_expr->cols());
        DBUG_RETURN(-1);
      }

      SELECT_LEX *current= thd->lex->current_select;
      thd->lex->current_select= current->return_after_parsing();
      char const *save_where= thd->where;
      thd->where= "IN/ALL/ANY subquery";
        
      bool failure= !in_subs->left_expr->fixed &&
                     in_subs->left_expr->fix_fields(thd, &in_subs->left_expr);
      thd->lex->current_select= current;
      thd->where= save_where;
      if (failure)
        DBUG_RETURN(-1); /* purecov: deadcode */
    }
    DBUG_PRINT("info", ("Checking if subq can be converted to semi-join"));
    /*
      Check if we're in subquery that is a candidate for flattening into a
      semi-join (which is done in flatten_subqueries()). The
      requirements are:
        1. Subquery predicate is an IN/=ANY subq predicate
        2. Subquery is a single SELECT (not a UNION)
        3. Subquery does not have GROUP BY or ORDER BY
        4. Subquery does not use aggregate functions or HAVING
        5. Subquery predicate is at the AND-top-level of ON/WHERE clause
        6. We are not in a subquery of a single table UPDATE/DELETE that 
             doesn't have a JOIN (TODO: We should handle this at some
             point by switching to multi-table UPDATE/DELETE)
        7. We're not in a table-less subquery like "SELECT 1"
        8. No execution method was already chosen (by a prepared statement)
        9. Parent select is not a table-less select
        10. Neither parent nor child select have STRAIGHT_JOIN option.
    */
    if (optimizer_flag(thd, OPTIMIZER_SWITCH_SEMIJOIN) &&
        in_subs &&                                                    // 1
        !select_lex->is_part_of_union() &&                            // 2
        !select_lex->group_list.elements && !join->order &&           // 3
        !join->having && !select_lex->with_sum_func &&                // 4
        thd->thd_marker.emb_on_expr_nest &&                           // 5
        select_lex->outer_select()->join &&                           // 6
        select_lex->master_unit()->first_select()->leaf_tables &&     // 7
        in_subs->exec_method == Item_in_subselect::NOT_TRANSFORMED && // 8
        select_lex->outer_select()->leaf_tables &&                    // 9
        !((join->select_options |                                     // 10
           select_lex->outer_select()->join->select_options)          // 10
          & SELECT_STRAIGHT_JOIN))                                    // 10
    {
      DBUG_PRINT("info", ("Subquery is semi-join conversion candidate"));

      (void)subquery_types_allow_materialization(in_subs);

      in_subs->emb_on_expr_nest= thd->thd_marker.emb_on_expr_nest;

      /* Register the subquery for further processing in flatten_subqueries() */
      select_lex->
        outer_select()->join->sj_subselects.append(thd->mem_root, in_subs);
      in_subs->expr_join_nest= thd->thd_marker.emb_on_expr_nest;
    }
    else
    {
      DBUG_PRINT("info", ("Subquery can't be converted to semi-join"));
      /*
        Check if the subquery predicate can be executed via materialization.
        The required conditions are:
        1. Subquery predicate is an IN/=ANY subq predicate
        2. Subquery is a single SELECT (not a UNION)
        3. Subquery is not a table-less query. In this case there is no
           point in materializing.
          3A The upper query is not a table-less SELECT ... FROM DUAL. We
             can't do materialization for SELECT .. FROM DUAL because it
             does not call setup_subquery_materialization(). We could make 
             SELECT ... FROM DUAL call that function but that doesn't seem
             to be the case that is worth handling.
        4. Either the subquery predicate is a top-level predicate, or at
           least one partial match strategy is enabled. If no partial match
           strategy is enabled, then materialization cannot be used for
           non-top-level queries because it cannot handle NULLs correctly.
        5. Subquery is non-correlated
           TODO:
           This is an overly restrictive condition. It can be extended to:
           (Subquery is non-correlated ||
            Subquery is correlated to any query outer to IN predicate ||
            (Subquery is correlated to the immediate outer query &&
             Subquery !contains {GROUP BY, ORDER BY [LIMIT],
             aggregate functions}) && subquery predicate is not under "NOT IN"))
        6. No execution method was already chosen (by a prepared statement).

        (*) The subquery must be part of a SELECT statement. The current
             condition also excludes multi-table update statements.

        Determine whether we will perform subquery materialization before
        calling the IN=>EXISTS transformation, so that we know whether to
        perform the whole transformation or only that part of it which wraps
        Item_in_subselect in an Item_in_optimizer.
      */
      if (optimizer_flag(thd, OPTIMIZER_SWITCH_MATERIALIZATION)  && 
          in_subs  &&                                                   // 1
          !select_lex->is_part_of_union() &&                            // 2
          select_lex->master_unit()->first_select()->leaf_tables &&     // 3
          thd->lex->sql_command == SQLCOM_SELECT &&                     // *
          select_lex->outer_select()->leaf_tables &&                    // 3A
          subquery_types_allow_materialization(in_subs) &&
          // psergey-todo: duplicated_subselect_card_check: where it's done?
          (in_subs->is_top_level_item() ||
           optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_ROWID_MERGE) ||
           optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_TABLE_SCAN)) &&//4
          !in_subs->is_correlated &&                                  // 5
          in_subs->exec_method == Item_in_subselect::NOT_TRANSFORMED) // 6
      {
          in_subs->exec_method= Item_in_subselect::MATERIALIZATION;
      }

      Item_subselect::trans_res trans_res;
      if ((trans_res= subselect->select_transformer(join)) !=
          Item_subselect::RES_OK)
      {
        DBUG_RETURN((trans_res == Item_subselect::RES_ERROR));
      }
    }
  }
  DBUG_RETURN(0);
}


/**
  @brief Check if subquery's compared types allow materialization.

  @param in_subs Subquery predicate, updated as follows:
    types_allow_materialization TRUE if subquery materialization is allowed.
    sjm_scan_allowed            If types_allow_materialization is TRUE,
                                indicates whether it is possible to use subquery
                                materialization and scan the materialized table.

  @retval TRUE   If subquery types allow materialization.
  @retval FALSE  Otherwise.

  @details
    This is a temporary fix for BUG#36752.
    
    There are two subquery materialization strategies:

    1. Materialize and do index lookups in the materialized table. See 
       BUG#36752 for description of restrictions we need to put on the
       compared expressions.

    2. Materialize and then do a full scan of the materialized table. At the
       moment, this strategy's applicability criteria are even stricter than
       in #1.

       This is so because of the following: consider an uncorrelated subquery
       
       ...WHERE (ot1.col1, ot2.col2 ...) IN (SELECT ie1,ie2,... FROM it1 ...)

       and a join order that could be used to do sjm-materialization: 
          
          SJM-Scan(it1, it1), ot1, ot2
       
       IN-equalities will be parts of conditions attached to the outer tables:

         ot1:  ot1.col1 = ie1 AND ... (C1)
         ot2:  ot1.col2 = ie2 AND ... (C2)
       
       besides those there may be additional references to ie1 and ie2
       generated by equality propagation. The problem with evaluating C1 and
       C2 is that ie{1,2} refer to subquery tables' columns, while we only have 
       current value of materialization temptable. Our solution is to 
        * require that all ie{N} are table column references. This allows 
          to copy the values of materialization temptable columns to the
          original table's columns (see setup_sj_materialization for more
          details)
        * require that compared columns have exactly the same type. This is
          a temporary measure to avoid BUG#36752-type problems.
*/

static 
bool subquery_types_allow_materialization(Item_in_subselect *in_subs)
{
  DBUG_ENTER("subquery_types_allow_materialization");

  DBUG_ASSERT(in_subs->left_expr->fixed);

  List_iterator<Item> it(in_subs->unit->first_select()->item_list);
  uint elements= in_subs->unit->first_select()->item_list.elements;

  in_subs->types_allow_materialization= FALSE;  // Assign default values
  in_subs->sjm_scan_allowed= FALSE;
  
  bool all_are_fields= TRUE;
  for (uint i= 0; i < elements; i++)
  {
    Item *outer= in_subs->left_expr->element_index(i);
    Item *inner= it++;
    all_are_fields &= (outer->real_item()->type() == Item::FIELD_ITEM && 
                       inner->real_item()->type() == Item::FIELD_ITEM);
    if (outer->result_type() != inner->result_type())
      DBUG_RETURN(FALSE);
    switch (outer->result_type()) {
    case STRING_RESULT:
      if (outer->is_datetime() != inner->is_datetime())
        DBUG_RETURN(FALSE);

      if (!(outer->collation.collation == inner->collation.collation 
          /*&& outer->max_length <= inner->max_length */))
        DBUG_RETURN(FALSE);
    /*case INT_RESULT:
      if (!(outer->unsigned_flag ^ inner->unsigned_flag))
        DBUG_RETURN(FALSE); */
    default:
      ;/* suitable for materialization */
    }

    // Materialization does not work with BLOB columns
    if (inner->field_type() == MYSQL_TYPE_BLOB || 
	inner->field_type() == MYSQL_TYPE_GEOMETRY)
        DBUG_RETURN(FALSE);
  }
    
  in_subs->types_allow_materialization= TRUE;
  in_subs->sjm_scan_allowed= all_are_fields;
  DBUG_PRINT("info",("subquery_types_allow_materialization: ok, allowed"));
  DBUG_RETURN(TRUE);
}


/*
  Convert semi-join subquery predicates into semi-join join nests

  SYNOPSIS
    convert_join_subqueries_to_semijoins()
 
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
    child selects exceeds MAX_TABLES.
    We deal with this problem by flattening children's subqueries first and
    then using a heuristic rule to determine each subquery predicate's
    "priority".

  RETURN 
    FALSE  OK
    TRUE   Error
*/

bool convert_join_subqueries_to_semijoins(JOIN *join)
{
  Query_arena *arena, backup;
  Item_in_subselect **in_subq;
  Item_in_subselect **in_subq_end;
  THD *thd= join->thd;
  DBUG_ENTER("convert_join_subqueries_to_semijoins");

  if (join->sj_subselects.elements() == 0)
    DBUG_RETURN(FALSE);

  /* First, convert child join's subqueries. We proceed bottom-up here */
  for (in_subq= join->sj_subselects.front(), 
       in_subq_end= join->sj_subselects.back(); 
       in_subq != in_subq_end; 
       in_subq++)
  {
    st_select_lex *child_select= (*in_subq)->get_select_lex();
    JOIN *child_join= child_select->join;
    child_join->outer_tables = child_join->tables;

    /*
      child_select->where contains only the WHERE predicate of the
      subquery itself here. We may be selecting from a VIEW, which has its
      own predicate. The combined predicates are available in child_join->conds,
      which was built by setup_conds() doing prepare_where() for all views.
    */
    child_select->where= child_join->conds;

    if (convert_join_subqueries_to_semijoins(child_join))
      DBUG_RETURN(TRUE);
    (*in_subq)->sj_convert_priority= 
      (*in_subq)->is_correlated * MAX_TABLES + child_join->outer_tables;
  }
  
  // Temporary measure: disable semi-joins when they are together with outer
  // joins.
  for (TABLE_LIST *tbl= join->select_lex->leaf_tables; tbl; tbl=tbl->next_leaf)
  {
    TABLE_LIST *embedding= tbl->embedding;
    if (tbl->on_expr || (tbl->embedding && !(embedding->sj_on_expr && 
                                            !embedding->embedding)))
    {
      in_subq= join->sj_subselects.front();
      arena= thd->activate_stmt_arena_if_needed(&backup);
      goto skip_conversion;
    }
  }

  //dump_TABLE_LIST_struct(select_lex, select_lex->leaf_tables);
  /* 
    2. Pick which subqueries to convert:
      sort the subquery array
      - prefer correlated subqueries over uncorrelated;
      - prefer subqueries that have greater number of outer tables;
  */
  join->sj_subselects.sort(subq_sj_candidate_cmp);
  // #tables-in-parent-query + #tables-in-subquery < MAX_TABLES
  /* Replace all subqueries to be flattened with Item_int(1) */
  arena= thd->activate_stmt_arena_if_needed(&backup);
  for (in_subq= join->sj_subselects.front(); 
       in_subq != in_subq_end && 
       join->tables + (*in_subq)->unit->first_select()->join->tables < MAX_TABLES;
       in_subq++)
  {
    Item **tree= ((*in_subq)->emb_on_expr_nest == (TABLE_LIST*)1)?
                   &join->conds : &((*in_subq)->emb_on_expr_nest->on_expr);
    if (replace_where_subcondition(join, tree, *in_subq, new Item_int(1),
                                   FALSE))
      DBUG_RETURN(TRUE); /* purecov: inspected */
  }
 
  for (in_subq= join->sj_subselects.front(); 
       in_subq != in_subq_end && 
       join->tables + (*in_subq)->unit->first_select()->join->tables < MAX_TABLES;
       in_subq++)
  {
    if (convert_subq_to_sj(join, *in_subq))
      DBUG_RETURN(TRUE);
  }
skip_conversion:
  /* 
    3. Finalize (perform IN->EXISTS rewrite) the subqueries that we didn't
    convert:
  */
  for (; in_subq!= in_subq_end; in_subq++)
  {
    JOIN *child_join= (*in_subq)->unit->first_select()->join;
    Item_subselect::trans_res res;
    (*in_subq)->changed= 0;
    (*in_subq)->fixed= 0;

    SELECT_LEX *save_select_lex= thd->lex->current_select;
    thd->lex->current_select= (*in_subq)->unit->first_select();

    res= (*in_subq)->select_transformer(child_join);

    thd->lex->current_select= save_select_lex;

    if (res == Item_subselect::RES_ERROR)
      DBUG_RETURN(TRUE);

    (*in_subq)->changed= 1;
    (*in_subq)->fixed= 1;

    Item *substitute= (*in_subq)->substitution;
    bool do_fix_fields= !(*in_subq)->substitution->fixed;
    Item **tree= ((*in_subq)->emb_on_expr_nest == (TABLE_LIST*)1)?
                   &join->conds : &((*in_subq)->emb_on_expr_nest->on_expr);
    if (replace_where_subcondition(join, tree, *in_subq, substitute, 
                                   do_fix_fields))
      DBUG_RETURN(TRUE);
    (*in_subq)->substitution= NULL;
     
    if (!thd->stmt_arena->is_conventional())
    {
      tree= ((*in_subq)->emb_on_expr_nest == (TABLE_LIST*)1)?
             &join->select_lex->prep_where : 
             &((*in_subq)->emb_on_expr_nest->prep_on_expr);

      if (replace_where_subcondition(join, tree, *in_subq, substitute, 
                                     FALSE))
        DBUG_RETURN(TRUE);
    }
  }

  if (arena)
    thd->restore_active_arena(arena, &backup);
  join->sj_subselects.clear();
  DBUG_RETURN(FALSE);
}

/**
   @brief Replaces an expression destructively inside the expression tree of
   the WHERE clase.

   @note Because of current requirements for semijoin flattening, we do not
   need to recurse here, hence this function will only examine the top-level
   AND conditions. (see JOIN::prepare, comment starting with "Check if the 
   subquery predicate can be executed via materialization".
   
   @param join The top-level query.
   @param old_cond The expression to be replaced.
   @param new_cond The expression to be substituted.
   @param do_fix_fields If true, Item::fix_fields(THD*, Item**) is called for
   the new expression.
   @return <code>true</code> if there was an error, <code>false</code> if
   successful.
*/
static bool replace_where_subcondition(JOIN *join, Item **expr, 
                                       Item *old_cond, Item *new_cond,
                                       bool do_fix_fields)
{
  //Item **expr= (emb_nest == (TABLE_LIST*)1)? &join->conds : &emb_nest->on_expr;
  if (*expr == old_cond)
  {
    *expr= new_cond;
    if (do_fix_fields)
      new_cond->fix_fields(join->thd, expr);
    return FALSE;
  }
  
  if ((*expr)->type() == Item::COND_ITEM) 
  {
    List_iterator<Item> li(*((Item_cond*)(*expr))->argument_list());
    Item *item;
    while ((item= li++))
    {
      if (item == old_cond) 
      {
        li.replace(new_cond);
        if (do_fix_fields)
          new_cond->fix_fields(join->thd, li.ref());
        return FALSE;
      }
    }
  }
  // If we came here it means there were an error during prerequisites check.
  DBUG_ASSERT(0);
  return TRUE;
}

static int subq_sj_candidate_cmp(Item_in_subselect* const *el1, 
                                 Item_in_subselect* const *el2)
{
  return ((*el1)->sj_convert_priority < (*el2)->sj_convert_priority) ? 1 : 
         ( ((*el1)->sj_convert_priority == (*el2)->sj_convert_priority)? 0 : -1);
}


/*
  Convert a subquery predicate into a TABLE_LIST semi-join nest

  SYNOPSIS
    convert_subq_to_sj()
       parent_join  Parent join, the one that has subq_pred in its WHERE/ON 
                    clause
       subq_pred    Subquery predicate to be converted
  
  DESCRIPTION
    Convert a subquery predicate into a TABLE_LIST semi-join nest. All the 
    prerequisites are already checked, so the conversion is always successfull.

    Prepared Statements: the transformation is permanent:
     - Changes in TABLE_LIST structures are naturally permanent
     - Item tree changes are performed on statement MEM_ROOT:
        = we activate statement MEM_ROOT 
        = this function is called before the first fix_prepare_information
          call.

    This is intended because the criteria for subquery-to-sj conversion remain
    constant for the lifetime of the Prepared Statement.

  RETURN
    FALSE  OK
    TRUE   Out of memory error
*/

static bool convert_subq_to_sj(JOIN *parent_join, Item_in_subselect *subq_pred)
{
  SELECT_LEX *parent_lex= parent_join->select_lex;
  TABLE_LIST *emb_tbl_nest= NULL;
  List<TABLE_LIST> *emb_join_list= &parent_lex->top_join_list;
  THD *thd= parent_join->thd;
  DBUG_ENTER("convert_subq_to_sj");

  /*
    1. Find out where to put the predicate into.
     Note: for "t1 LEFT JOIN t2" this will be t2, a leaf.
  */
  if ((void*)subq_pred->expr_join_nest != (void*)1)
  {
    if (subq_pred->expr_join_nest->nested_join)
    {
      /*
        We're dealing with

          ... [LEFT] JOIN  ( ... ) ON (subquery AND whatever) ...

        The sj-nest will be inserted into the brackets nest.
      */
      emb_tbl_nest=  subq_pred->expr_join_nest;
      emb_join_list= &emb_tbl_nest->nested_join->join_list;
    }
    else if (!subq_pred->expr_join_nest->outer_join)
    {
      /*
        We're dealing with

          ... INNER JOIN tblX ON (subquery AND whatever) ...

        The sj-nest will be tblX's "sibling", i.e. another child of its
        parent. This is ok because tblX is joined as an inner join.
      */
      emb_tbl_nest= subq_pred->expr_join_nest->embedding;
      if (emb_tbl_nest)
        emb_join_list= &emb_tbl_nest->nested_join->join_list;
    }
    else if (!subq_pred->expr_join_nest->nested_join)
    {
      TABLE_LIST *outer_tbl= subq_pred->expr_join_nest;      
      TABLE_LIST *wrap_nest;
      /*
        We're dealing with

          ... LEFT JOIN tbl ON (on_expr AND subq_pred) ...

        we'll need to convert it into:

          ... LEFT JOIN ( tbl SJ (subq_tables) ) ON (on_expr AND subq_pred) ...
                        |                      |
                        |<----- wrap_nest ---->|
        
        Q:  other subqueries may be pointing to this element. What to do?
        A1: simple solution: copy *subq_pred->expr_join_nest= *parent_nest.
            But we'll need to fix other pointers.
        A2: Another way: have TABLE_LIST::next_ptr so the following
            subqueries know the table has been nested.
        A3: changes in the TABLE_LIST::outer_join will make everything work
            automatically.
      */
      if (!(wrap_nest= alloc_join_nest(parent_join->thd)))
      {
        DBUG_RETURN(TRUE);
      }
      wrap_nest->embedding= outer_tbl->embedding;
      wrap_nest->join_list= outer_tbl->join_list;
      wrap_nest->alias= (char*) "(sj-wrap)";

      wrap_nest->nested_join->join_list.empty();
      wrap_nest->nested_join->join_list.push_back(outer_tbl);

      outer_tbl->embedding= wrap_nest;
      outer_tbl->join_list= &wrap_nest->nested_join->join_list;

      /*
        wrap_nest will take place of outer_tbl, so move the outer join flag
        and on_expr
      */
      wrap_nest->outer_join= outer_tbl->outer_join;
      outer_tbl->outer_join= 0;

      wrap_nest->on_expr= outer_tbl->on_expr;
      outer_tbl->on_expr= NULL;

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

  TABLE_LIST *sj_nest;
  NESTED_JOIN *nested_join;
  if (!(sj_nest= alloc_join_nest(parent_join->thd)))
  {
    DBUG_RETURN(TRUE);
  }
  nested_join= sj_nest->nested_join;

  sj_nest->join_list= emb_join_list;
  sj_nest->embedding= emb_tbl_nest;
  sj_nest->alias= (char*) "(sj-nest)";
  sj_nest->sj_subq_pred= subq_pred;
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
  st_select_lex *subq_lex= subq_pred->unit->first_select();
  nested_join->join_list.empty();
  List_iterator_fast<TABLE_LIST> li(subq_lex->top_join_list);
  TABLE_LIST *tl, *last_leaf;
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
  for (tl= parent_lex->leaf_tables; tl->next_leaf; tl= tl->next_leaf) ;
  tl->next_leaf= subq_lex->leaf_tables;
  last_leaf= tl;

  /*
    Same as above for next_local chain
    (a theory: a next_local chain always starts with ::leaf_tables
     because view's tables are inserted after the view)
  */
  for (tl= parent_lex->leaf_tables; tl->next_local; tl= tl->next_local) ;
  tl->next_local= subq_lex->leaf_tables;

  /* A theory: no need to re-connect the next_global chain */

  /* 3. Remove the original subquery predicate from the WHERE/ON */

  // The subqueries were replaced for Item_int(1) earlier
  subq_pred->exec_method=
    Item_in_subselect::SEMI_JOIN;         // for subsequent executions
  /*TODO: also reset the 'with_subselect' there. */

  /* n. Adjust the parent_join->tables counter */
  uint table_no= parent_join->tables;
  /* n. Walk through child's tables and adjust table->map */
  for (tl= subq_lex->leaf_tables; tl; tl= tl->next_leaf, table_no++)
  {
    tl->table->tablenr= table_no;
    tl->table->map= ((table_map)1) << table_no;
    SELECT_LEX *old_sl= tl->select_lex;
    tl->select_lex= parent_join->select_lex; 
    for (TABLE_LIST *emb= tl->embedding;
         emb && emb->select_lex == old_sl;
         emb= emb->embedding)
      emb->select_lex= parent_join->select_lex;
  }
  parent_join->tables += subq_lex->join->tables;

  /* 
    Put the subquery's WHERE into semi-join's sj_on_expr
    Add the subquery-induced equalities too.
  */
  SELECT_LEX *save_lex= thd->lex->current_select;
  thd->lex->current_select=subq_lex;
  if (!subq_pred->left_expr->fixed &&
       subq_pred->left_expr->fix_fields(thd, &subq_pred->left_expr))
    DBUG_RETURN(TRUE);
  thd->lex->current_select=save_lex;

  sj_nest->nested_join->sj_corr_tables= subq_pred->used_tables();
  sj_nest->nested_join->sj_depends_on=  subq_pred->used_tables() |
                                        subq_pred->left_expr->used_tables();
  sj_nest->sj_on_expr= subq_lex->join->conds;

  /*
    Create the IN-equalities and inject them into semi-join's ON expression.
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
  sj_nest->sj_in_exprs= subq_pred->left_expr->cols();
  sj_nest->nested_join->sj_outer_expr_list.empty();

  if (subq_pred->left_expr->cols() == 1)
  {
    nested_join->sj_outer_expr_list.push_back(subq_pred->left_expr);
    Item_func_eq *item_eq=
      new Item_func_eq(subq_pred->left_expr, subq_lex->ref_pointer_array[0]);
    item_eq->in_equality_no= 0;
    sj_nest->sj_on_expr= and_items(sj_nest->sj_on_expr, item_eq);
  }
  else
  {
    for (uint i= 0; i < subq_pred->left_expr->cols(); i++)
    {
      nested_join->sj_outer_expr_list.push_back(subq_pred->left_expr->
                                                element_index(i));
      Item_func_eq *item_eq= 
        new Item_func_eq(subq_pred->left_expr->element_index(i), 
                         subq_lex->ref_pointer_array[i]);
      item_eq->in_equality_no= i;
      sj_nest->sj_on_expr= and_items(sj_nest->sj_on_expr, item_eq);
    }
  }
  /* Fix the created equality and AND */
  sj_nest->sj_on_expr->fix_fields(parent_join->thd, &sj_nest->sj_on_expr);

  /*
    Walk through sj nest's WHERE and ON expressions and call
    item->fix_table_changes() for all items.
  */
  sj_nest->sj_on_expr->fix_after_pullout(parent_lex, &sj_nest->sj_on_expr);
  fix_list_after_tbl_changes(parent_lex, &sj_nest->nested_join->join_list);


  /* Unlink the child select_lex so it doesn't show up in EXPLAIN: */
  subq_lex->master_unit()->exclude_level();

  DBUG_EXECUTE("where",
               print_where(sj_nest->sj_on_expr,"SJ-EXPR", QT_ORDINARY););

  /* Inject sj_on_expr into the parent's WHERE or ON */
  if (emb_tbl_nest)
  {
    emb_tbl_nest->on_expr= and_items(emb_tbl_nest->on_expr, 
                                     sj_nest->sj_on_expr);
    emb_tbl_nest->on_expr->fix_fields(parent_join->thd, &emb_tbl_nest->on_expr);
  }
  else
  {
    /* Inject into the WHERE */
    parent_join->conds= and_items(parent_join->conds, sj_nest->sj_on_expr);
    parent_join->conds->fix_fields(parent_join->thd, &parent_join->conds);
    parent_join->select_lex->where= parent_join->conds;
  }

  if (subq_lex->ftfunc_list->elements)
  {
    Item_func_match *ifm;
    List_iterator_fast<Item_func_match> li(*(subq_lex->ftfunc_list));
    while ((ifm= li++))
      parent_lex->ftfunc_list->push_front(ifm);
  }

  DBUG_RETURN(FALSE);
}

static TABLE_LIST *alloc_join_nest(THD *thd)
{
  TABLE_LIST *tbl;
  if (!(tbl= (TABLE_LIST*) thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST))+
                                       sizeof(NESTED_JOIN))))
    return NULL;
  tbl->nested_join= (NESTED_JOIN*) ((uchar*)tbl + 
                                    ALIGN_SIZE(sizeof(TABLE_LIST)));
  return tbl;
}


static
void fix_list_after_tbl_changes(SELECT_LEX *new_parent, List<TABLE_LIST> *tlist)
{
  List_iterator<TABLE_LIST> it(*tlist);
  TABLE_LIST *table;
  while ((table= it++))
  {
    if (table->on_expr)
      table->on_expr->fix_after_pullout(new_parent, &table->on_expr);
    if (table->nested_join)
      fix_list_after_tbl_changes(new_parent, &table->nested_join->join_list);
  }
}


/*
  Pull tables out of semi-join nests, if possible

  SYNOPSIS
    pull_out_semijoin_tables()
      join  The join where to do the semi-join flattening

  DESCRIPTION
    Try to pull tables out of semi-join nests.
     
    PRECONDITIONS
    When this function is called, the join may have several semi-join nests
    but it is guaranteed that one semi-join nest does not contain another.
   
    ACTION
    A table can be pulled out of the semi-join nest if
     - It is a constant table, or
     - It is accessed via eq_ref(outer_tables)

    POSTCONDITIONS
     * Tables that were pulled out have JOIN_TAB::emb_sj_nest == NULL
     * Tables that were not pulled out have JOIN_TAB::emb_sj_nest pointing 
       to semi-join nest they are in.
     * Semi-join nests' TABLE_LIST::sj_inner_tables is updated accordingly

    This operation is (and should be) performed at each PS execution since
    tables may become/cease to be constant across PS reexecutions.
    
  NOTE
    Table pullout may make uncorrelated subquery correlated. Consider this
    example:
    
     ... WHERE oe IN (SELECT it1.primary_key WHERE p(it1, it2) ... ) 
    
    here table it1 can be pulled out (we have it1.primary_key=oe which gives
    us functional dependency). Once it1 is pulled out, all references to it1
    from p(it1, it2) become references to outside of the subquery and thus
    make the subquery (i.e. its semi-join nest) correlated.
    Making the subquery (i.e. its semi-join nest) correlated prevents us from
    using Materialization or LooseScan to execute it. 

  RETURN 
    0 - OK
    1 - Out of memory error
*/

int pull_out_semijoin_tables(JOIN *join)
{
  TABLE_LIST *sj_nest;
  DBUG_ENTER("pull_out_semijoin_tables");
  List_iterator<TABLE_LIST> sj_list_it(join->select_lex->sj_nests);
   
  /* Try pulling out of the each of the semi-joins */
  while ((sj_nest= sj_list_it++))
  {
    /* Action #1: Mark the constant tables to be pulled out */
    table_map pulled_tables= 0;
    List_iterator<TABLE_LIST> child_li(sj_nest->nested_join->join_list);
    TABLE_LIST *tbl;
    while ((tbl= child_li++))
    {
      if (tbl->table)
      {
        tbl->table->reginfo.join_tab->emb_sj_nest= sj_nest;
#if 0 
        /* 
          Do not pull out tables because they are constant. This operation has
          a problem:
          - Some constant tables may become/cease to be constant across PS
            re-executions
          - Contrary to our initial assumption, it turned out that table pullout 
            operation is not easily undoable.

          The solution is to leave constant tables where they are. This will
          affect only constant tables that are 1-row or empty, tables that are
          constant because they are accessed via eq_ref(const) access will
          still be pulled out as functionally-dependent.

          This will cause us to miss the chance to flatten some of the 
          subqueries, but since const tables do not generate many duplicates,
          it really doesn't matter that much whether they were pulled out or
          not.

          All of this was done as fix for BUG#43768.
        */
        if (tbl->table->map & join->const_table_map)
        {
          pulled_tables |= tbl->table->map;
          DBUG_PRINT("info", ("Table %s pulled out (reason: constant)",
                              tbl->table->alias));
        }
#endif
      }
    }
    
    /*
      Action #2: Find which tables we can pull out based on
      update_ref_and_keys() data. Note that pulling one table out can allow
      us to pull out some other tables too.
    */
    bool pulled_a_table;
    do 
    {
      pulled_a_table= FALSE;
      child_li.rewind();
      while ((tbl= child_li++))
      {
        if (tbl->table && !(pulled_tables & tbl->table->map))
        {
          if (find_eq_ref_candidate(tbl->table, 
                                    sj_nest->nested_join->used_tables & 
                                    ~pulled_tables))
          {
            pulled_a_table= TRUE;
            pulled_tables |= tbl->table->map;
            DBUG_PRINT("info", ("Table %s pulled out (reason: func dep)",
                                tbl->table->alias.c_ptr()));
            /*
              Pulling a table out of uncorrelated subquery in general makes
              makes it correlated. See the NOTE to this funtion. 
            */
            sj_nest->sj_subq_pred->is_correlated= TRUE;
            sj_nest->nested_join->sj_corr_tables|= tbl->table->map;
            sj_nest->nested_join->sj_depends_on|= tbl->table->map;
          }
        }
      }
    } while (pulled_a_table);
 
    child_li.rewind();
    /*
      Action #3: Move the pulled out TABLE_LIST elements to the parents.
    */
    table_map inner_tables= sj_nest->nested_join->used_tables & 
                            ~pulled_tables;
    /* Record the bitmap of inner tables */
    sj_nest->sj_inner_tables= inner_tables;
    if (pulled_tables)
    {
      List<TABLE_LIST> *upper_join_list= (sj_nest->embedding != NULL)?
                                           (&sj_nest->embedding->nested_join->join_list): 
                                           (&join->select_lex->top_join_list);
      Query_arena *arena, backup;
      arena= join->thd->activate_stmt_arena_if_needed(&backup);
      while ((tbl= child_li++))
      {
        if (tbl->table)
        {
          if (inner_tables & tbl->table->map)
          {
            /* This table is not pulled out */
            tbl->table->reginfo.join_tab->emb_sj_nest= sj_nest;
          }
          else
          {
            /* This table has been pulled out of the semi-join nest */
            tbl->table->reginfo.join_tab->emb_sj_nest= NULL;
            /*
              Pull the table up in the same way as simplify_joins() does:
              update join_list and embedding pointers but keep next[_local]
              pointers.
            */
            child_li.remove();
            sj_nest->nested_join->used_tables &= ~tbl->table->map;
            upper_join_list->push_back(tbl);
            tbl->join_list= upper_join_list;
            tbl->embedding= sj_nest->embedding;
          }
        }
      }

      /* Remove the sj-nest itself if we've removed everything from it */
      if (!inner_tables)
      {
        List_iterator<TABLE_LIST> li(*upper_join_list);
        /* Find the sj_nest in the list. */
        while (sj_nest != li++) ;
        li.remove();
        /* Also remove it from the list of SJ-nests: */
        sj_list_it.remove();
      }

      if (arena)
        join->thd->restore_active_arena(arena, &backup);
    }
  }
  DBUG_RETURN(0);
}


/* 
  Optimize semi-join nests that could be run with sj-materialization

  SYNOPSIS
    optimize_semijoin_nests()
      join           The join to optimize semi-join nests for
      all_table_map  Bitmap of all tables in the join

  DESCRIPTION
    Optimize each of the semi-join nests that can be run with
    materialization. For each of the nests, we
     - Generate the best join order for this "sub-join" and remember it;
     - Remember the sub-join execution cost (it's part of materialization
       cost);
     - Calculate other costs that will be incurred if we decide 
       to use materialization strategy for this semi-join nest.

    All obtained information is saved and will be used by the main join
    optimization pass.

  RETURN
    FALSE  Ok 
    TRUE   Out of memory error
*/

bool optimize_semijoin_nests(JOIN *join, table_map all_table_map)
{
  DBUG_ENTER("optimize_semijoin_nests");
  List_iterator<TABLE_LIST> sj_list_it(join->select_lex->sj_nests);
  TABLE_LIST *sj_nest;
  while ((sj_nest= sj_list_it++))
  {
    /* semi-join nests with only constant tables are not valid */
   /// DBUG_ASSERT(sj_nest->sj_inner_tables & ~join->const_table_map);

    sj_nest->sj_mat_info= NULL;
    /*
      The statement may have been executed with 'semijoin=on' earlier.
      We need to verify that 'semijoin=on' still holds.
     */
    if (optimizer_flag(join->thd, OPTIMIZER_SWITCH_SEMIJOIN) &&
        optimizer_flag(join->thd, OPTIMIZER_SWITCH_MATERIALIZATION))
    {
      if ((sj_nest->sj_inner_tables  & ~join->const_table_map) && /* not everything was pulled out */
          !sj_nest->sj_subq_pred->is_correlated && 
           sj_nest->sj_subq_pred->types_allow_materialization)
      {
        join->emb_sjm_nest= sj_nest;
        if (choose_plan(join, all_table_map &~join->const_table_map))
          DBUG_RETURN(TRUE); /* purecov: inspected */
        /*
          The best plan to run the subquery is now in join->best_positions,
          save it.
        */
        uint n_tables= my_count_bits(sj_nest->sj_inner_tables & ~join->const_table_map);
        SJ_MATERIALIZATION_INFO* sjm;
        if (!(sjm= new SJ_MATERIALIZATION_INFO) ||
            !(sjm->positions= (POSITION*)join->thd->alloc(sizeof(POSITION)*
                                                          n_tables)))
          DBUG_RETURN(TRUE); /* purecov: inspected */
        sjm->tables= n_tables;
        sjm->is_used= FALSE;
        double subjoin_out_rows, subjoin_read_time;
        get_partial_join_cost(join, n_tables,
                              &subjoin_read_time, &subjoin_out_rows);

        sjm->materialization_cost.convert_from_cost(subjoin_read_time);
        sjm->rows= subjoin_out_rows;

        List<Item> &right_expr_list= 
          sj_nest->sj_subq_pred->unit->first_select()->item_list;
        /*
          Adjust output cardinality estimates. If the subquery has form

           ... oe IN (SELECT t1.colX, t2.colY, func(X,Y,Z) )

           then the number of distinct output record combinations has an
           upper bound of product of number of records matching the tables 
           that are used by the SELECT clause.
           TODO:
             We can get a more precise estimate if we
              - use rec_per_key cardinality estimates. For simple cases like 
                "oe IN (SELECT t.key ...)" it is trivial. 
              - Functional dependencies between the tables in the semi-join
                nest (the payoff is probably less here?)
        */
        {
          for (uint i=0 ; i < join->const_tables + sjm->tables ; i++)
          {
            JOIN_TAB *tab= join->best_positions[i].table;
            join->map2table[tab->table->tablenr]= tab;
          }
          List_iterator<Item> it(right_expr_list);
          Item *item;
          table_map map= 0;
          while ((item= it++))
            map |= item->used_tables();
          map= map & ~PSEUDO_TABLE_BITS;
          Table_map_iterator tm_it(map);
          int tableno;
          double rows= 1.0;
          while ((tableno = tm_it.next_bit()) != Table_map_iterator::BITMAP_END)
            rows *= join->map2table[tableno]->table->quick_condition_rows;
          sjm->rows= min(sjm->rows, rows);
        }
        memcpy(sjm->positions, join->best_positions + join->const_tables, 
               sizeof(POSITION) * n_tables);

        /*
          Calculate temporary table parameters and usage costs
        */
        uint rowlen= get_tmp_table_rec_length(right_expr_list);
        double lookup_cost;
        if (rowlen * subjoin_out_rows< join->thd->variables.max_heap_table_size)
          lookup_cost= HEAP_TEMPTABLE_LOOKUP_COST;
        else
          lookup_cost= DISK_TEMPTABLE_LOOKUP_COST;

        /*
          Let materialization cost include the cost to write the data into the
          temporary table:
        */ 
        sjm->materialization_cost.add_io(subjoin_out_rows, lookup_cost);
        
        /*
          Set the cost to do a full scan of the temptable (will need this to 
          consider doing sjm-scan):
        */ 
        sjm->scan_cost.zero();
        sjm->scan_cost.add_io(sjm->rows, lookup_cost);

        sjm->lookup_cost.convert_from_cost(lookup_cost);
        sj_nest->sj_mat_info= sjm;
        DBUG_EXECUTE("opt", print_sjm(sjm););
      }
    }
  }
  join->emb_sjm_nest= NULL;
  DBUG_RETURN(FALSE);
}

/*
  Get estimated record length for semi-join materialization temptable
  
  SYNOPSIS
    get_tmp_table_rec_length()
      items  IN subquery's select list.

  DESCRIPTION
    Calculate estimated record length for semi-join materialization
    temptable. It's an estimate because we don't follow every bit of
    create_tmp_table()'s logic. This isn't necessary as the return value of
    this function is used only for cost calculations.

  RETURN
    Length of the temptable record, in bytes
*/

static uint get_tmp_table_rec_length(List<Item> &items)
{
  uint len= 0;
  Item *item;
  List_iterator<Item> it(items);
  while ((item= it++))
  {
    switch (item->result_type()) {
    case REAL_RESULT:
      len += sizeof(double);
      break;
    case INT_RESULT:
      if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
        len += 8;
      else
        len += 4;
      break;
    case STRING_RESULT:
      enum enum_field_types type;
      /* DATE/TIME and GEOMETRY fields have STRING_RESULT result type.  */
      if ((type= item->field_type()) == MYSQL_TYPE_DATETIME ||
          type == MYSQL_TYPE_TIME || type == MYSQL_TYPE_DATE ||
          type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_GEOMETRY)
        len += 8;
      else
        len += item->max_length;
      break;
    case DECIMAL_RESULT:
      len += 10;
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0); /* purecov: deadcode */
      break;
    }
  }
  return len;
}

//psergey-todo: is the below a kind of table elimination??
/*
  Check if table's KEYUSE elements have an eq_ref(outer_tables) candidate

  SYNOPSIS
    find_eq_ref_candidate()
      table             Table to be checked
      sj_inner_tables   Bitmap of inner tables. eq_ref(inner_table) doesn't
                        count.

  DESCRIPTION
    Check if table's KEYUSE elements have an eq_ref(outer_tables) candidate

  TODO
    Check again if it is feasible to factor common parts with constant table
    search

  RETURN
    TRUE  - There exists an eq_ref(outer-tables) candidate
    FALSE - Otherwise
*/

bool find_eq_ref_candidate(TABLE *table, table_map sj_inner_tables)
{
  KEYUSE *keyuse= table->reginfo.join_tab->keyuse;
  uint key;

  if (keyuse)
  {
    while (1) /* For each key */
    {
      key= keyuse->key;
      KEY *keyinfo= table->key_info + key;
      key_part_map bound_parts= 0;
      if (keyinfo->flags & HA_NOSAME)
      {
        do  /* For all equalities on all key parts */
        {
          /* Check if this is "t.keypart = expr(outer_tables) */
          if (!(keyuse->used_tables & sj_inner_tables) &&
              !(keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL))
          {
            bound_parts |= 1 << keyuse->keypart;
          }
          keyuse++;
        } while (keyuse->key == key && keyuse->table == table);

        if (bound_parts == PREV_BITS(uint, keyinfo->key_parts))
          return TRUE;
        if (keyuse->table != table)
          return FALSE;
      }
      else
      {
        do
        {
          keyuse++;
          if (keyuse->table != table)
            return FALSE;
        }
        while (keyuse->key == key);
      }
    }
  }
  return FALSE;
}

/*
  Do semi-join optimization step after we've added a new tab to join prefix

  SYNOPSIS
    advance_sj_state()
      join                        The join we're optimizing
      remaining_tables            Tables not in the join prefix
      new_join_tab                Join tab we've just added to the join prefix
      idx                         Index of this join tab (i.e. number of tables
                                  in the prefix minus one)
      current_record_count INOUT  Estimate of #records in join prefix's output
      current_read_time    INOUT  Cost to execute the join prefix
      loose_scan_pos       IN     A POSITION with LooseScan plan to access 
                                  table new_join_tab
                                  (produced by the last best_access_path call)

  DESCRIPTION
    Update semi-join optimization state after we've added another tab (table 
    and access method) to the join prefix.
    
    The state is maintained in join->positions[#prefix_size]. Each of the
    available strategies has its own state variables.
    
    for each semi-join strategy
    {
      update strategy's state variables;

      if (join prefix has all the tables that are needed to consider
          using this strategy for the semi-join(s))
      {
        calculate cost of using the strategy
        if ((this is the first strategy to handle the semi-join nest(s)  ||
            the cost is less than other strategies))
        {
          // Pick this strategy
          pos->sj_strategy= ..
          ..
        }
      }

    Most of the new state is saved join->positions[idx] (and hence no undo
    is necessary). Several members of class JOIN are updated also, these
    changes can be rolled back with restore_prev_sj_state().

    See setup_semijoin_dups_elimination() for a description of what kinds of
    join prefixes each strategy can handle.
*/

void advance_sj_state(JOIN *join, table_map remaining_tables, 
                      const JOIN_TAB *new_join_tab, uint idx, 
                      double *current_record_count, double *current_read_time, 
                      POSITION *loose_scan_pos)
{
  TABLE_LIST *emb_sj_nest;
  POSITION *pos= join->positions + idx;
  remaining_tables &= ~new_join_tab->table->map;

  pos->prefix_cost.convert_from_cost(*current_read_time);
  pos->prefix_record_count= *current_record_count;
  pos->sj_strategy= SJ_OPT_NONE;
  
  /* Initialize the state or copy it from prev. tables */
  if (idx == join->const_tables)
  {
    pos->first_firstmatch_table= MAX_TABLES;
    pos->first_loosescan_table= MAX_TABLES; 
    pos->dupsweedout_tables= 0;
    pos->sjm_scan_need_tables= 0;
    LINT_INIT(pos->sjm_scan_last_inner);
  }
  else
  {
    // FirstMatch
    pos->first_firstmatch_table=
      (pos[-1].sj_strategy == SJ_OPT_FIRST_MATCH) ?
      MAX_TABLES : pos[-1].first_firstmatch_table;
    pos->first_firstmatch_rtbl= pos[-1].first_firstmatch_rtbl;
    pos->firstmatch_need_tables= pos[-1].firstmatch_need_tables;

    // LooseScan
    pos->first_loosescan_table=
      (pos[-1].sj_strategy == SJ_OPT_LOOSE_SCAN) ?
      MAX_TABLES : pos[-1].first_loosescan_table;
    pos->loosescan_need_tables= pos[-1].loosescan_need_tables;

    // SJ-Materialization Scan
    pos->sjm_scan_need_tables=
      (pos[-1].sj_strategy == SJ_OPT_MATERIALIZE_SCAN) ?
      0 : pos[-1].sjm_scan_need_tables;
    pos->sjm_scan_last_inner= pos[-1].sjm_scan_last_inner;

    // Duplicate Weedout
    pos->dupsweedout_tables=      pos[-1].dupsweedout_tables;
    pos->first_dupsweedout_table= pos[-1].first_dupsweedout_table;
  }
  
  table_map handled_by_fm_or_ls= 0;
  /* FirstMatch Strategy */
  if (new_join_tab->emb_sj_nest &&
      optimizer_flag(join->thd, OPTIMIZER_SWITCH_FIRSTMATCH))
  {
    const table_map outer_corr_tables=
      new_join_tab->emb_sj_nest->nested_join->sj_corr_tables |
      new_join_tab->emb_sj_nest->nested_join->sj_depends_on;
    const table_map sj_inner_tables=
      new_join_tab->emb_sj_nest->sj_inner_tables & ~join->const_table_map;

    /* 
      Enter condition:
       1. The next join tab belongs to semi-join nest
          (verified for the encompassing code block above).
       2. We're not in a duplicate producer range yet
       3. All outer tables that
           - the subquery is correlated with, or
           - referred to from the outer_expr 
          are in the join prefix
       4. All inner tables are still part of remaining_tables.
    */
    if (!join->cur_sj_inner_tables &&              // (2)
        !(remaining_tables & outer_corr_tables) && // (3)
        (sj_inner_tables ==                        // (4)
         ((remaining_tables | new_join_tab->table->map) & sj_inner_tables)))
    {
      /* Start tracking potential FirstMatch range */
      pos->first_firstmatch_table= idx;
      pos->firstmatch_need_tables= sj_inner_tables;
      pos->first_firstmatch_rtbl= remaining_tables;
    }

    if (pos->first_firstmatch_table != MAX_TABLES)
    {
      if (outer_corr_tables & pos->first_firstmatch_rtbl)
      {
        /*
          Trying to add an sj-inner table whose sj-nest has an outer correlated 
          table that was not in the prefix. This means FirstMatch can't be used.
        */
        pos->first_firstmatch_table= MAX_TABLES;
      }
      else
      {
        /* Record that we need all of this semi-join's inner tables, too */
        pos->firstmatch_need_tables|= sj_inner_tables;
      }
    
      if (!(pos->firstmatch_need_tables & remaining_tables))
      {
        /*
          Got a complete FirstMatch range.
            Calculate correct costs and fanout
        */
        optimize_wo_join_buffering(join, pos->first_firstmatch_table, idx,
                                   remaining_tables, FALSE, idx,
                                   current_record_count, 
                                   current_read_time);
        /*
          We don't yet know what are the other strategies, so pick the
          FirstMatch.

          We ought to save the alternate POSITIONs produced by
          optimize_wo_join_buffering but the problem is that providing save
          space uses too much space. Instead, we will re-calculate the
          alternate POSITIONs after we've picked the best QEP.
        */
        pos->sj_strategy= SJ_OPT_FIRST_MATCH;
        handled_by_fm_or_ls=  pos->firstmatch_need_tables;
      }
    }
  }

  /* LooseScan Strategy */
  {
    POSITION *first=join->positions+pos->first_loosescan_table; 
    /* 
      LooseScan strategy can't handle interleaving between tables from the 
      semi-join that LooseScan is handling and any other tables.

      If we were considering LooseScan for the join prefix (1)
         and the table we're adding creates an interleaving (2)
      then 
         stop considering loose scan
    */
    if ((pos->first_loosescan_table != MAX_TABLES) &&   // (1)
        (first->table->emb_sj_nest->sj_inner_tables & remaining_tables) && //(2)
        new_join_tab->emb_sj_nest != first->table->emb_sj_nest) //(2)
    {
      pos->first_loosescan_table= MAX_TABLES;
    }

    /*
      If we got an option to use LooseScan for the current table, start
      considering using LooseScan strategy
    */
    if (loose_scan_pos->read_time != DBL_MAX)
    {
      pos->first_loosescan_table= idx;
      pos->loosescan_need_tables=
        new_join_tab->emb_sj_nest->sj_inner_tables | 
        new_join_tab->emb_sj_nest->nested_join->sj_depends_on |
        new_join_tab->emb_sj_nest->nested_join->sj_corr_tables;
    }
    
    if ((pos->first_loosescan_table != MAX_TABLES) && 
        !(remaining_tables & pos->loosescan_need_tables))
    {
      /* 
        Ok we have LooseScan plan and also have all LooseScan sj-nest's
        inner tables and outer correlated tables into the prefix.
      */

      first=join->positions + pos->first_loosescan_table; 
      uint n_tables= my_count_bits(first->table->emb_sj_nest->sj_inner_tables);
      /* Got a complete LooseScan range. Calculate its cost */
      /*
        The same problem as with FirstMatch - we need to save POSITIONs
        somewhere but reserving space for all cases would require too
        much space. We will re-calculate POSITION structures later on. 
      */
      optimize_wo_join_buffering(join, pos->first_loosescan_table, idx,
                                 remaining_tables, 
                                 TRUE,  //first_alt
                                 pos->first_loosescan_table + n_tables,
                                 current_record_count,
                                 current_read_time);
      /*
        We don't yet have any other strategies that could handle this
        semi-join nest (the other options are Duplicate Elimination or
        Materialization, which need at least the same set of tables in 
        the join prefix to be considered) so unconditionally pick the 
        LooseScan.
      */
      pos->sj_strategy= SJ_OPT_LOOSE_SCAN;
      handled_by_fm_or_ls= first->table->emb_sj_nest->sj_inner_tables;
    }
  }

  /* 
    Update join->cur_sj_inner_tables (Used by FirstMatch in this function and
    LooseScan detector in best_access_path)
  */
  if ((emb_sj_nest= new_join_tab->emb_sj_nest))
  {
    join->cur_sj_inner_tables |= emb_sj_nest->sj_inner_tables;
    join->cur_dups_producing_tables |= emb_sj_nest->sj_inner_tables;

    /* Remove the sj_nest if all of its SJ-inner tables are in cur_table_map */
    if (!(remaining_tables &
          emb_sj_nest->sj_inner_tables & ~new_join_tab->table->map))
      join->cur_sj_inner_tables &= ~emb_sj_nest->sj_inner_tables;
  }
  join->cur_dups_producing_tables &= ~handled_by_fm_or_ls;

  /* 4. SJ-Materialization and SJ-Materialization-scan strategy handler */
  bool sjm_scan;
  SJ_MATERIALIZATION_INFO *mat_info;
  if ((mat_info= at_sjmat_pos(join, remaining_tables,
                              new_join_tab, idx, &sjm_scan)))
  {
    if (sjm_scan)
    {
      /*
        We can't yet evaluate this option yet. This is because we can't
        accout for fanout of sj-inner tables yet:

          ntX  SJM-SCAN(it1 ... itN) | ot1 ... otN  |
                                     ^(1)           ^(2)

        we're now at position (1). SJM temptable in general has multiple
        records, so at point (1) we'll get the fanout from sj-inner tables (ie
        there will be multiple record combinations).

        The final join result will not contain any semi-join produced
        fanout, i.e. tables within SJM-SCAN(...) will not contribute to
        the cardinality of the join output.  Extra fanout produced by 
        SJM-SCAN(...) will be 'absorbed' into fanout produced by ot1 ...  otN.

        The simple way to model this is to remove SJM-SCAN(...) fanout once
        we reach the point #2.
      */
      pos->sjm_scan_need_tables=
        new_join_tab->emb_sj_nest->sj_inner_tables | 
        new_join_tab->emb_sj_nest->nested_join->sj_depends_on |
        new_join_tab->emb_sj_nest->nested_join->sj_corr_tables;
      pos->sjm_scan_last_inner= idx;
    }
    else
    {
      /* This is SJ-Materialization with lookups */
      COST_VECT prefix_cost; 
      signed int first_tab= (int)idx - mat_info->tables;
      double prefix_rec_count;
      if (first_tab < (int)join->const_tables)
      {
        prefix_cost.zero();
        prefix_rec_count= 1.0;
      }
      else
      {
        prefix_cost= join->positions[first_tab].prefix_cost;
        prefix_rec_count= join->positions[first_tab].prefix_record_count;
      }

      double mat_read_time= prefix_cost.total_cost();
      mat_read_time += mat_info->materialization_cost.total_cost() +
                       prefix_rec_count * mat_info->lookup_cost.total_cost();

      if (mat_read_time < *current_read_time || join->cur_dups_producing_tables)
      {
        /*
          NOTE: When we pick to use SJM[-Scan] we don't memcpy its POSITION
          elements to join->positions as that makes it hard to return things
          back when making one step back in join optimization. That's done 
          after the QEP has been chosen.
        */
        pos->sj_strategy= SJ_OPT_MATERIALIZE;
        *current_read_time=    mat_read_time;
        *current_record_count= prefix_rec_count;
        join->cur_dups_producing_tables&=
          ~new_join_tab->emb_sj_nest->sj_inner_tables;
      }
    }
  }
  
  /* 4.A SJM-Scan second phase check */
  if (pos->sjm_scan_need_tables && /* Have SJM-Scan prefix */
      !(pos->sjm_scan_need_tables & remaining_tables))
  {
    TABLE_LIST *mat_nest= 
      join->positions[pos->sjm_scan_last_inner].table->emb_sj_nest;
    SJ_MATERIALIZATION_INFO *mat_info= mat_nest->sj_mat_info;

    double prefix_cost;
    double prefix_rec_count;
    int first_tab= pos->sjm_scan_last_inner + 1 - mat_info->tables;
    /* Get the prefix cost */
    if (first_tab == (int)join->const_tables)
    {
      prefix_rec_count= 1.0;
      prefix_cost= 0.0;
    }
    else
    {
      prefix_cost= join->positions[first_tab - 1].prefix_cost.total_cost();
      prefix_rec_count= join->positions[first_tab - 1].prefix_record_count;
    }

    /* Add materialization cost */
    prefix_cost += mat_info->materialization_cost.total_cost() +
                   prefix_rec_count * mat_info->scan_cost.total_cost();
    prefix_rec_count *= mat_info->rows;
    
    uint i;
    table_map rem_tables= remaining_tables;
    for (i= idx; i != (first_tab + mat_info->tables - 1); i--)
      rem_tables |= join->positions[i].table->table->map;

    POSITION curpos, dummy;
    /* Need to re-run best-access-path as we prefix_rec_count has changed */
    for (i= first_tab + mat_info->tables; i <= idx; i++)
    {
      best_access_path(join, join->positions[i].table, rem_tables, i, FALSE,
                       prefix_rec_count, &curpos, &dummy);
      prefix_rec_count *= curpos.records_read;
      prefix_cost += curpos.read_time;
    }

    /*
      Use the strategy if 
       * it is cheaper then what we've had, or
       * we haven't picked any other semi-join strategy yet
      In the second case, we pick this strategy unconditionally because
      comparing cost without semi-join duplicate removal with cost with
      duplicate removal is not an apples-to-apples comparison.
    */
    if (prefix_cost < *current_read_time || join->cur_dups_producing_tables)
    {
      pos->sj_strategy= SJ_OPT_MATERIALIZE_SCAN;
      *current_read_time=    prefix_cost;
      *current_record_count= prefix_rec_count;
      join->cur_dups_producing_tables&= ~mat_nest->sj_inner_tables;

    }
  }

  /* 5. Duplicate Weedout strategy handler */
  {
    /* 
       Duplicate weedout can be applied after all ON-correlated and 
       correlated 
    */
    TABLE_LIST *nest;
    if ((nest= new_join_tab->emb_sj_nest))
    {
      if (!pos->dupsweedout_tables)
        pos->first_dupsweedout_table= idx;

      pos->dupsweedout_tables |= nest->sj_inner_tables |
                                 nest->nested_join->sj_depends_on |
                                 nest->nested_join->sj_corr_tables;
    }

    if (pos->dupsweedout_tables && 
        !(remaining_tables &
          ~new_join_tab->table->map & pos->dupsweedout_tables))
    {
      /*
        Ok, reached a state where we could put a dups weedout point.
        Walk back and calculate
          - the join cost (this is needed as the accumulated cost may assume 
            some other duplicate elimination method)
          - extra fanout that will be removed by duplicate elimination
          - duplicate elimination cost
        There are two cases:
          1. We have other strategy/ies to remove all of the duplicates.
          2. We don't.
        
        We need to calculate the cost in case #2 also because we need to make
        choice between this join order and others.
      */
      uint first_tab= pos->first_dupsweedout_table;
      double dups_cost;
      double prefix_rec_count;
      double sj_inner_fanout= 1.0;
      double sj_outer_fanout= 1.0;
      uint temptable_rec_size;
      if (first_tab == join->const_tables)
      {
        prefix_rec_count= 1.0;
        temptable_rec_size= 0;
        dups_cost= 0.0;
      }
      else
      {
        dups_cost= join->positions[first_tab - 1].prefix_cost.total_cost();
        prefix_rec_count= join->positions[first_tab - 1].prefix_record_count;
        temptable_rec_size= 8; /* This is not true but we'll make it so */
      }
      
      table_map dups_removed_fanout= 0;
      for (uint j= pos->first_dupsweedout_table; j <= idx; j++)
      {
        POSITION *p= join->positions + j;
        dups_cost += p->read_time;
        if (p->table->emb_sj_nest)
        {
          sj_inner_fanout *= p->records_read;
          dups_removed_fanout |= p->table->table->map;
        }
        else
        {
          sj_outer_fanout *= p->records_read;
          temptable_rec_size += p->table->table->file->ref_length;
        }
      }

      /*
        Add the cost of temptable use. The table will have sj_outer_fanout
        records, and we will make 
        - sj_outer_fanout table writes
        - sj_inner_fanout*sj_outer_fanout  lookups.

      */
      double one_lookup_cost;
      if (sj_outer_fanout*temptable_rec_size > 
          join->thd->variables.max_heap_table_size)
        one_lookup_cost= DISK_TEMPTABLE_LOOKUP_COST;
      else
        one_lookup_cost= HEAP_TEMPTABLE_LOOKUP_COST;

      double write_cost= join->positions[first_tab].prefix_record_count* 
                         sj_outer_fanout * one_lookup_cost;
      double full_lookup_cost= join->positions[first_tab].prefix_record_count* 
                               sj_outer_fanout* sj_inner_fanout * 
                               one_lookup_cost;
      dups_cost += write_cost + full_lookup_cost;
      
      /*
        Use the strategy if 
         * it is cheaper then what we've had, or
         * we haven't picked any other semi-join strategy yet
        The second part is necessary because this strategy is the last one
        to consider (it needs "the most" tables in the prefix) and we can't
        leave duplicate-producing tables not handled by any strategy.
      */
      if (dups_cost < *current_read_time || join->cur_dups_producing_tables)
      {
        pos->sj_strategy= SJ_OPT_DUPS_WEEDOUT;
        *current_read_time= dups_cost;
        *current_record_count= prefix_rec_count * sj_outer_fanout;
        join->cur_dups_producing_tables &= ~dups_removed_fanout;
      }
    }
  }
}


/*
  Remove the last join tab from from join->cur_sj_inner_tables bitmap
  we assume remaining_tables doesnt contain @tab.
*/

void restore_prev_sj_state(const table_map remaining_tables, 
                                  const JOIN_TAB *tab, uint idx)
{
  TABLE_LIST *emb_sj_nest;
  if ((emb_sj_nest= tab->emb_sj_nest))
  {
    /* If we're removing the last SJ-inner table, remove the sj-nest */
    if ((remaining_tables & emb_sj_nest->sj_inner_tables) == 
        (emb_sj_nest->sj_inner_tables & ~tab->table->map))
    {
      tab->join->cur_sj_inner_tables &= ~emb_sj_nest->sj_inner_tables;
    }
  }
}


/*
  Given a semi-join nest, find out which of the IN-equalities are bound

  SYNOPSIS
    get_bound_sj_equalities()
      sj_nest           Semi-join nest
      remaining_tables  Tables that are not yet bound

  DESCRIPTION
    Given a semi-join nest, find out which of the IN-equalities have their
    left part expression bound (i.e. the said expression doesn't refer to
    any of remaining_tables and can be evaluated).

  RETURN
    Bitmap of bound IN-equalities.
*/

ulonglong get_bound_sj_equalities(TABLE_LIST *sj_nest, 
                                  table_map remaining_tables)
{
  List_iterator<Item> li(sj_nest->nested_join->sj_outer_expr_list);
  Item *item;
  uint i= 0;
  ulonglong res= 0;
  while ((item= li++))
  {
    /*
      Q: should this take into account equality propagation and how?
      A: If e->outer_side is an Item_field, walk over the equality
         class and see if there is an element that is bound?
      (this is an optional feature)
    */
    if (!(item->used_tables() & remaining_tables))
    {
      res |= 1ULL << i;
    }
  }
  return res;
}


/*
  Check if the last tables of the partial join order allow to use
  sj-materialization strategy for them

  SYNOPSIS
    at_sjmat_pos()
      join              
      remaining_tables
      tab                the last table's join tab
      idx                last table's index
      loose_scan    OUT  TRUE <=> use LooseScan

  RETURN
    TRUE   Yes, can apply sj-materialization
    FALSE  No, some of the requirements are not met
*/

static SJ_MATERIALIZATION_INFO *
at_sjmat_pos(const JOIN *join, table_map remaining_tables, const JOIN_TAB *tab,
             uint idx, bool *loose_scan)
{
  /*
   Check if 
    1. We're in a semi-join nest that can be run with SJ-materialization
    2. All the tables correlated through the IN subquery are in the prefix
  */
  TABLE_LIST *emb_sj_nest= tab->emb_sj_nest;
  table_map suffix= remaining_tables & ~tab->table->map;
  if (emb_sj_nest && emb_sj_nest->sj_mat_info &&
      !(suffix & emb_sj_nest->sj_inner_tables))
  {
    /* 
      Walk back and check if all immediately preceding tables are from
      this semi-join.
    */
    uint n_tables= my_count_bits(tab->emb_sj_nest->sj_inner_tables);
    for (uint i= 1; i < n_tables ; i++)
    {
      if (join->positions[idx - i].table->emb_sj_nest != tab->emb_sj_nest)
        return NULL;
    }
    *loose_scan= test(remaining_tables & ~tab->table->map &
                             (emb_sj_nest->sj_inner_tables |
                              emb_sj_nest->nested_join->sj_depends_on));
    if (*loose_scan && !emb_sj_nest->sj_subq_pred->sjm_scan_allowed)
      return NULL;
    else
      return emb_sj_nest->sj_mat_info;
  }
  return NULL;
}



/*
  Fix semi-join strategies for the picked join order

  SYNOPSIS
    fix_semijoin_strategies_for_picked_join_order()
      join  The join with the picked join order

  DESCRIPTION
    Fix semi-join strategies for the picked join order. This is a step that
    needs to be done right after we have fixed the join order. What we do
    here is switch join's semi-join strategy description from backward-based
    to forwards based.
    
    When join optimization is in progress, we re-consider semi-join
    strategies after we've added another table. Here's an illustration.
    Suppose the join optimization is underway:

    1) ot1  it1  it2 
                 sjX  -- looking at (ot1, it1, it2) join prefix, we decide
                         to use semi-join strategy sjX.

    2) ot1  it1  it2  ot2 
                 sjX  sjY -- Having added table ot2, we now may consider
                             another semi-join strategy and decide to use a 
                             different strategy sjY. Note that the record
                             of sjX has remained under it2. That is
                             necessary because we need to be able to get
                             back to (ot1, it1, it2) join prefix.
      what makes things even worse is that there are cases where the choice
      of sjY changes the way we should access it2. 

    3) [ot1  it1  it2  ot2  ot3]
                  sjX  sjY  -- This means that after join optimization is
                               finished, semi-join info should be read
                               right-to-left (while nearly all plan refinement
                               functions, EXPLAIN, etc proceed from left to 
                               right)

    This function does the needed reversal, making it possible to read the
    join and semi-join order from left to right.
*/    

void fix_semijoin_strategies_for_picked_join_order(JOIN *join)
{
  uint table_count=join->tables;
  uint tablenr;
  table_map remaining_tables= 0;
  table_map handled_tabs= 0;
  for (tablenr= table_count - 1 ; tablenr != join->const_tables - 1; tablenr--)
  {
    POSITION *pos= join->best_positions + tablenr;
    JOIN_TAB *s= pos->table;
    uint first;
    LINT_INIT(first); // Set by every branch except SJ_OPT_NONE which doesn't use it

    if ((handled_tabs & s->table->map) || pos->sj_strategy == SJ_OPT_NONE)
    {
      remaining_tables |= s->table->map;
      continue;
    }
    
    if (pos->sj_strategy == SJ_OPT_MATERIALIZE)
    {
      SJ_MATERIALIZATION_INFO *sjm= s->emb_sj_nest->sj_mat_info;
      sjm->is_used= TRUE;
      sjm->is_sj_scan= FALSE;
      memcpy(pos - sjm->tables + 1, sjm->positions, 
             sizeof(POSITION) * sjm->tables);
      first= tablenr - sjm->tables + 1;
      join->best_positions[first].n_sj_tables= sjm->tables;
      join->best_positions[first].sj_strategy= SJ_OPT_MATERIALIZE;
    }
    else if (pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN)
    {
      POSITION *first_inner= join->best_positions + pos->sjm_scan_last_inner;
      SJ_MATERIALIZATION_INFO *sjm= first_inner->table->emb_sj_nest->sj_mat_info;
      sjm->is_used= TRUE;
      sjm->is_sj_scan= TRUE;
      first= pos->sjm_scan_last_inner - sjm->tables + 1;
      memcpy(join->best_positions + first, 
             sjm->positions, sizeof(POSITION) * sjm->tables);
      join->best_positions[first].sj_strategy= SJ_OPT_MATERIALIZE_SCAN;
      join->best_positions[first].n_sj_tables= sjm->tables;
      /* 
        Do what advance_sj_state did: re-run best_access_path for every table
        in the [last_inner_table + 1; pos..) range
      */
      double prefix_rec_count;
      /* Get the prefix record count */
      if (first == join->const_tables)
        prefix_rec_count= 1.0;
      else
        prefix_rec_count= join->best_positions[first-1].prefix_record_count;
      
      /* Add materialization record count*/
      prefix_rec_count *= sjm->rows;
      
      uint i;
      table_map rem_tables= remaining_tables;
      for (i= tablenr; i != (first + sjm->tables - 1); i--)
        rem_tables |= join->best_positions[i].table->table->map;

      POSITION dummy;
      join->cur_sj_inner_tables= 0;
      for (i= first + sjm->tables; i <= tablenr; i++)
      {
        best_access_path(join, join->best_positions[i].table, rem_tables, i, FALSE,
                         prefix_rec_count, join->best_positions + i, &dummy);
        prefix_rec_count *= join->best_positions[i].records_read;
        rem_tables &= ~join->best_positions[i].table->table->map;
      }
    }
 
    if (pos->sj_strategy == SJ_OPT_FIRST_MATCH)
    {
      first= pos->first_firstmatch_table;
      join->best_positions[first].sj_strategy= SJ_OPT_FIRST_MATCH;
      join->best_positions[first].n_sj_tables= tablenr - first + 1;
      POSITION dummy; // For loose scan paths
      double record_count= (first== join->const_tables)? 1.0: 
                           join->best_positions[tablenr - 1].prefix_record_count;
      
      table_map rem_tables= remaining_tables;
      uint idx;
      for (idx= first; idx <= tablenr; idx++)
      {
        rem_tables |= join->best_positions[idx].table->table->map;
      }
      /*
        Re-run best_access_path to produce best access methods that do not use
        join buffering
      */ 
      join->cur_sj_inner_tables= 0;
      for (idx= first; idx <= tablenr; idx++)
      {
        if (join->best_positions[idx].use_join_buffer)
        {
           best_access_path(join, join->best_positions[idx].table, 
                            rem_tables, idx, TRUE /* no jbuf */,
                            record_count, join->best_positions + idx, &dummy);
        }
        record_count *= join->best_positions[idx].records_read;
        rem_tables &= ~join->best_positions[idx].table->table->map;
      }
    }

    if (pos->sj_strategy == SJ_OPT_LOOSE_SCAN) 
    {
      first= pos->first_loosescan_table;
      POSITION *first_pos= join->best_positions + first;
      POSITION loose_scan_pos; // For loose scan paths
      double record_count= (first== join->const_tables)? 1.0: 
                           join->best_positions[tablenr - 1].prefix_record_count;
      
      table_map rem_tables= remaining_tables;
      uint idx;
      for (idx= first; idx <= tablenr; idx++)
        rem_tables |= join->best_positions[idx].table->table->map;
      /*
        Re-run best_access_path to produce best access methods that do not use
        join buffering
      */ 
      join->cur_sj_inner_tables= 0;
      for (idx= first; idx <= tablenr; idx++)
      {
        if (join->best_positions[idx].use_join_buffer || (idx == first))
        {
           best_access_path(join, join->best_positions[idx].table,
                            rem_tables, idx, TRUE /* no jbuf */,
                            record_count, join->best_positions + idx,
                            &loose_scan_pos);
           if (idx==first)
             join->best_positions[idx]= loose_scan_pos;
        }
        rem_tables &= ~join->best_positions[idx].table->table->map;
        record_count *= join->best_positions[idx].records_read;
      }
      first_pos->sj_strategy= SJ_OPT_LOOSE_SCAN;
      first_pos->n_sj_tables= my_count_bits(first_pos->table->emb_sj_nest->sj_inner_tables);
    }

    if (pos->sj_strategy == SJ_OPT_DUPS_WEEDOUT)
    {
      /* 
        Duplicate Weedout starting at pos->first_dupsweedout_table, ending at
        this table.
      */
      first= pos->first_dupsweedout_table;
      join->best_positions[first].sj_strategy= SJ_OPT_DUPS_WEEDOUT;
      join->best_positions[first].n_sj_tables= tablenr - first + 1;
    }
    
    uint i_end= first + join->best_positions[first].n_sj_tables;
    for (uint i= first; i < i_end; i++)
    {
      if (i != first)
        join->best_positions[i].sj_strategy= SJ_OPT_NONE;
      handled_tabs |= join->best_positions[i].table->table->map;
    }

    if (tablenr != first)
      pos->sj_strategy= SJ_OPT_NONE;
    remaining_tables |= s->table->map;
    //s->sj_strategy= pos->sj_strategy;
    join->join_tab[first].sj_strategy= join->best_positions[first].sj_strategy;
  }
}

/*
  Setup semi-join materialization strategy for one semi-join nest
  
  SYNOPSIS

  setup_sj_materialization()
    tab  The first tab in the semi-join

  DESCRIPTION
    Setup execution structures for one semi-join materialization nest:
    - Create the materialization temporary table
    - If we're going to do index lookups
        create TABLE_REF structure to make the lookus
    - else (if we're going to do a full scan of the temptable)
        create Copy_field structures to do copying.

  RETURN
    FALSE  Ok
    TRUE   Error
*/

bool setup_sj_materialization(JOIN_TAB *tab)
{
  uint i;
  DBUG_ENTER("setup_sj_materialization");
  TABLE_LIST *emb_sj_nest= tab->table->pos_in_table_list->embedding;
  SJ_MATERIALIZATION_INFO *sjm= emb_sj_nest->sj_mat_info;
  THD *thd= tab->join->thd;
  /* First the calls come to the materialization function */
  List<Item> &item_list= emb_sj_nest->sj_subq_pred->unit->first_select()->item_list;

  /* 
    Set up the table to write to, do as select_union::create_result_table does
  */
  sjm->sjm_table_param.init();
  sjm->sjm_table_param.field_count= item_list.elements;
  sjm->sjm_table_param.bit_fields_as_long= TRUE;
  List_iterator<Item> it(item_list);
  Item *right_expr;
  while((right_expr= it++))
    sjm->sjm_table_cols.push_back(right_expr);

  if (!(sjm->table= create_tmp_table(thd, &sjm->sjm_table_param, 
                                     sjm->sjm_table_cols, (ORDER*) 0, 
                                     TRUE /* distinct */, 
                                     1, /*save_sum_fields*/
                                     thd->options | TMP_TABLE_ALL_COLUMNS, 
                                     HA_POS_ERROR /*rows_limit */, 
                                     (char*)"sj-materialize")))
    DBUG_RETURN(TRUE); /* purecov: inspected */
  sjm->table->file->extra(HA_EXTRA_WRITE_CACHE);
  sjm->table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  tab->join->sj_tmp_tables.push_back(sjm->table);
  tab->join->sjm_info_list.push_back(sjm);
  
  sjm->materialized= FALSE;
  if (!sjm->is_sj_scan)
  {
    KEY           *tmp_key; /* The only index on the temporary table. */
    uint          tmp_key_parts; /* Number of keyparts in tmp_key. */
    tmp_key= sjm->table->key_info;
    tmp_key_parts= tmp_key->key_parts;
    
    /*
      Create/initialize everything we will need to index lookups into the
      temptable.
    */
    TABLE_REF *tab_ref;
    if (!(tab_ref= (TABLE_REF*) thd->alloc(sizeof(TABLE_REF))))
      DBUG_RETURN(TRUE); /* purecov: inspected */
    tab_ref->key= 0; /* The only temp table index. */
    tab_ref->key_length= tmp_key->key_length;
    if (!(tab_ref->key_buff=
          (uchar*) thd->calloc(ALIGN_SIZE(tmp_key->key_length) * 2)) ||
        !(tab_ref->key_copy=
          (store_key**) thd->alloc((sizeof(store_key*) *
                                    (tmp_key_parts + 1)))) ||
        !(tab_ref->items=
          (Item**) thd->alloc(sizeof(Item*) * tmp_key_parts)))
      DBUG_RETURN(TRUE); /* purecov: inspected */

    tab_ref->key_buff2=tab_ref->key_buff+ALIGN_SIZE(tmp_key->key_length);
    tab_ref->key_err=1;
    tab_ref->null_rejecting= 1;
    tab_ref->disable_cache= FALSE;

    KEY_PART_INFO *cur_key_part= tmp_key->key_part;
    store_key **ref_key= tab_ref->key_copy;
    uchar *cur_ref_buff= tab_ref->key_buff;
    
    for (i= 0; i < tmp_key_parts; i++, cur_key_part++, ref_key++)
    {
      tab_ref->items[i]= emb_sj_nest->sj_subq_pred->left_expr->element_index(i);
      int null_count= test(cur_key_part->field->real_maybe_null());
      *ref_key= new store_key_item(thd, cur_key_part->field,
                                   /* TODO:
                                      the NULL byte is taken into account in
                                      cur_key_part->store_length, so instead of
                                      cur_ref_buff + test(maybe_null), we could
                                      use that information instead.
                                   */
                                   cur_ref_buff + null_count,
                                   null_count ? tab_ref->key_buff : 0,
                                   cur_key_part->length, tab_ref->items[i],
                                   FALSE);
      cur_ref_buff+= cur_key_part->store_length;
    }
    *ref_key= NULL; /* End marker. */
    tab_ref->key_err= 1;
    tab_ref->key_parts= tmp_key_parts;
    sjm->tab_ref= tab_ref;

    /*
      Remove the injected semi-join IN-equalities from join_tab conds. This
      needs to be done because the IN-equalities refer to columns of
      sj-inner tables which are not available after the materialization
      has been finished.
    */
    for (i= 0; i < sjm->tables; i++)
    {
      remove_sj_conds(&tab[i].select_cond);
      if (tab[i].select)
        remove_sj_conds(&tab[i].select->cond);
    }
    if (!(sjm->in_equality= create_subq_in_equalities(thd, sjm,
                                                      emb_sj_nest->sj_subq_pred)))
      DBUG_RETURN(TRUE); /* purecov: inspected */
  }
  else
  {
    /*
      We'll be doing full scan of the temptable.  
      Setup copying of temptable columns back to the record buffers
      for their source tables. We need this because IN-equalities
      refer to the original tables.

      EXAMPLE

      Consider the query:
        SELECT * FROM ot WHERE ot.col1 IN (SELECT it.col2 FROM it)
      
      Suppose it's executed with SJ-Materialization-scan. We choose to do scan
      if we can't do the lookup, i.e. the join order is (it, ot). The plan
      would look as follows:

        table    access method      condition
         it      materialize+scan    -
         ot      (whatever)          ot1.col1=it.col2 (C2)

      The condition C2 refers to current row of table it. The problem is
      that by the time we evaluate C2, we would have finished with scanning
      it itself and will be scanning the temptable. 

      At the moment, our solution is to copy back: when we get the next
      temptable record, we copy its columns to their corresponding columns
      in the record buffers for the source tables. 
    */
    sjm->copy_field= new Copy_field[sjm->sjm_table_cols.elements];
    it.rewind();
    for (uint i=0; i < sjm->sjm_table_cols.elements; i++)
    {
      bool dummy;
      Item_equal *item_eq;
      Item *item= (it++)->real_item();
      DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
      Field *copy_to= ((Item_field*)item)->field;
      /*
        Tricks with Item_equal are due to the following: suppose we have a
        query:
        
        ... WHERE cond(ot.col) AND ot.col IN (SELECT it2.col FROM it1,it2
                                               WHERE it1.col= it2.col)
         then equality propagation will create an 
         
           Item_equal(it1.col, it2.col, ot.col) 
         
         then substitute_for_best_equal_field() will change the conditions
         according to the join order:

           it1
           it2    it1.col=it2.col
           ot     cond(it1.col)

         although we've originally had "SELECT it2.col", conditions attached 
         to subsequent outer tables will refer to it1.col, so SJM-Scan will
         need to unpack data to there. 
         That is, if an element from subquery's select list participates in 
         equality propagation, then we need to unpack it to the first
         element equality propagation member that refers to table that is
         within the subquery.
      */
      item_eq= find_item_equal(tab->join->cond_equal, copy_to, &dummy);

      if (item_eq)
      {
        List_iterator<Item_field> it(item_eq->fields);
        Item_field *item;
        while ((item= it++))
        {
          if (!(item->used_tables() & ~emb_sj_nest->sj_inner_tables))
          {
            copy_to= item->field;
            break;
          }
        }
      }
      sjm->copy_field[i].set(copy_to, sjm->table->field[i], FALSE);
      /* The write_set for source tables must be set up to allow the copying */
      bitmap_set_bit(copy_to->table->write_set, copy_to->field_index);
    }
  }

  DBUG_RETURN(FALSE);
}



/*
  Create subquery IN-equalities assuming use of materialization strategy
  
  SYNOPSIS
    create_subq_in_equalities()
      thd        Thread handle
      sjm        Semi-join materialization structure
      subq_pred  The subquery predicate

  DESCRIPTION
    Create subquery IN-equality predicates. That is, for a subquery
    
      (oe1, oe2, ...) IN (SELECT ie1, ie2, ... FROM ...)
    
    create "oe1=ie1 AND ie1=ie2 AND ..." expression, such that ie1, ie2, ..
    refer to the columns of the table that's used to materialize the
    subquery.

  RETURN 
    Created condition
*/

static Item *create_subq_in_equalities(THD *thd, SJ_MATERIALIZATION_INFO *sjm, 
                                Item_in_subselect *subq_pred)
{
  Item *res= NULL;
  if (subq_pred->left_expr->cols() == 1)
  {
    if (!(res= new Item_func_eq(subq_pred->left_expr,
                                new Item_field(sjm->table->field[0]))))
      return NULL; /* purecov: inspected */
  }
  else
  {
    Item *conj;
    for (uint i= 0; i < subq_pred->left_expr->cols(); i++)
    {
      if (!(conj= new Item_func_eq(subq_pred->left_expr->element_index(i), 
                                   new Item_field(sjm->table->field[i]))) ||
          !(res= and_items(res, conj)))
        return NULL; /* purecov: inspected */
    }
  }
  if (res->fix_fields(thd, &res))
    return NULL; /* purecov: inspected */
  return res;
}




static void remove_sj_conds(Item **tree)
{
  if (*tree)
  {
    if (is_cond_sj_in_equality(*tree))
    {
      *tree= NULL;
      return;
    }
    else if ((*tree)->type() == Item::COND_ITEM) 
    {
      Item *item;
      List_iterator<Item> li(*(((Item_cond*)*tree)->argument_list()));
      while ((item= li++))
      {
        if (is_cond_sj_in_equality(item))
          li.replace(new Item_int(1));
      }
    }
  }
}

/* Check if given Item was injected by semi-join equality */
static bool is_cond_sj_in_equality(Item *item)
{
  if (item->type() == Item::FUNC_ITEM &&
      ((Item_func*)item)->functype()== Item_func::EQ_FUNC)
  {
    Item_func_eq *item_eq= (Item_func_eq*)item;
    return test(item_eq->in_equality_no != UINT_MAX);
  }
  return FALSE;
}


/*
  Create a temporary table to weed out duplicate rowid combinations

  SYNOPSIS

    create_duplicate_weedout_tmp_table()
      thd                    Thread handle
      uniq_tuple_length_arg  Length of the table's column
      sjtbl                  Update sjtbl->[start_]recinfo values which 
                             will be needed if we'll need to convert the 
                             created temptable from HEAP to MyISAM/Maria.

  DESCRIPTION
    Create a temporary table to weed out duplicate rowid combinations. The
    table has a single column that is a concatenation of all rowids in the
    combination. 

    Depending on the needed length, there are two cases:

    1. When the length of the column < max_key_length:

      CREATE TABLE tmp (col VARBINARY(n) NOT NULL, UNIQUE KEY(col));

    2. Otherwise (not a valid SQL syntax but internally supported):

      CREATE TABLE tmp (col VARBINARY NOT NULL, UNIQUE CONSTRAINT(col));

    The code in this function was produced by extraction of relevant parts
    from create_tmp_table().

  RETURN
    created table
    NULL on error
*/

TABLE *create_duplicate_weedout_tmp_table(THD *thd, 
                                          uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl)
{
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  uint  temp_pool_slot=MY_BIT_NONE;
  char	*tmpname,path[FN_REFLEN];
  Field **reg_field;
  KEY_PART_INFO *key_part_info;
  KEY *keyinfo;
  uchar *group_buff;
  uchar *bitmaps;
  uint *blob_field;
  ENGINE_COLUMNDEF *recinfo, *start_recinfo;
  bool using_unique_constraint=FALSE;
  bool use_packed_rows= FALSE;
  Field *field, *key_field;
  uint blob_count, null_pack_length, null_count;
  uchar *null_flags;
  uchar *pos;
  DBUG_ENTER("create_duplicate_weedout_tmp_table");
  DBUG_ASSERT(!sjtbl->is_degenerate);
  /*
    STEP 1: Get temporary table name
  */
  statistic_increment(thd->status_var.created_tmp_tables, &LOCK_status);
  if (use_temp_pool && !(test_flags & TEST_KEEP_TMP_TABLES))
    temp_pool_slot = bitmap_lock_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", tmp_file_prefix,
	    current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    sprintf(path,"%s%lx_%lx_%x", tmp_file_prefix,current_pid,
            thd->thread_id, thd->tmp_table++);
  }
  fn_format(path, path, mysql_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  /* STEP 2: Figure if we'll be using a key or blob+constraint */
  if (uniq_tuple_length_arg >= CONVERT_IF_BIGGER_TO_BLOB)
    using_unique_constraint= TRUE;

  /* STEP 3: Allocate memory for temptable description */
  init_sql_alloc(&own_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (1+1),
                        &blob_field, sizeof(uint)*2,
                        &keyinfo, sizeof(*keyinfo),
                        &key_part_info, sizeof(*key_part_info) * 2,
                        &start_recinfo,
                        sizeof(*recinfo)*(1*2+4),
                        &tmpname, (uint) strlen(path)+1,
                        &group_buff, (!using_unique_constraint ?
                                      uniq_tuple_length_arg : 0),
                        &bitmaps, bitmap_buffer_size(1)*3,
                        NullS))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    DBUG_RETURN(NULL);
  }
  strmov(tmpname,path);
  

  /* STEP 4: Create TABLE description */
  bzero((char*) table,sizeof(*table));
  bzero((char*) reg_field,sizeof(Field*)*2);

  table->mem_root= own_root;
  mem_root_save= thd->mem_root;
  thd->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias.set("weedout-tmp", sizeof("weedout-tmp")-1,
                   table_alias_charset);
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= thd;
  table->quick_keys.init();
  table->covering_keys.init();
  table->keys_in_use_for_query.init();

  table->s= share;
  init_tmp_table_share(thd, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= NULL;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();

  blob_count= 0;

  /* Create the field */
  {
    /*
      For the sake of uniformity, always use Field_varstring (altough we could
      use Field_string for shorter keys)
    */
    field= new Field_varstring(uniq_tuple_length_arg, FALSE, "rowids", share,
                               &my_charset_bin);
    if (!field)
      DBUG_RETURN(0);
    field->table= table;
    field->key_start.init(0);
    field->part_of_key.init(0);
    field->part_of_sortkey.init(0);
    field->unireg_check= Field::NONE;
    field->flags= (NOT_NULL_FLAG | BINARY_FLAG | NO_DEFAULT_VALUE_FLAG);
    field->reset_fields();
    field->init(table);
    field->orig_table= NULL;
     
    field->field_index= 0;
    
    *(reg_field++)= field;
    *blob_field= 0;
    *reg_field= 0;

    share->fields= 1;
    share->blob_fields= 0;
  }

  uint reclength= field->pack_length();
  if (using_unique_constraint)
  { 
    share->db_plugin= ha_lock_engine(0, TMP_ENGINE_HTON);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
    DBUG_ASSERT(uniq_tuple_length_arg <= table->file->max_key_length());
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  if (!table->file)
    goto err;

  null_count=1;
  
  null_pack_length= 1;
  reclength += null_pack_length;

  share->reclength= reclength;
  {
    uint alloc_length=ALIGN_SIZE(share->reclength + MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (uchar*)
                            alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo= start_recinfo;
  null_flags=(uchar*) table->record[0];
  pos=table->record[0]+ null_pack_length;
  if (null_pack_length)
  {
    bzero((uchar*) recinfo,sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    bfill(null_flags,null_pack_length,255);	// Set null fields

    table->null_flags= (uchar*) table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }
  null_count=1;

  {
    //Field *field= *reg_field;
    uint length;
    bzero((uchar*) recinfo,sizeof(*recinfo));
    field->move_field(pos,(uchar*) 0,0);

    field->reset();
    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    // Initialize the table field:
    bzero(field->ptr, field->pack_length());

    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= FIELD_BLOB;
    else if (use_packed_rows &&
             field->real_type() == MYSQL_TYPE_STRING &&
	     length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type=FIELD_SKIP_ENDSPACE;
    else
      recinfo->type=FIELD_NORMAL;

    field->set_table_name(&table->alias);
  }

  if (thd->variables.tmp_table_size == ~ (ulonglong) 0)		// No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 min(thd->variables.tmp_table_size,
                                     thd->variables.max_heap_table_size) :
                                 thd->variables.tmp_table_size) /
			         share->reclength);
  set_if_bigger(share->max_rows,1);		// For dummy start options


  //// keyinfo= param->keyinfo;
  if (TRUE)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    share->keys=1;
    share->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts= keyinfo->key_parts= 1;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "weedout_key";
    {
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->record[0]);
      key_part_info->length= (uint16) field->key_length();
      key_part_info->type=   (uint8) field->key_type();
      key_part_info->key_type = FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
	if (!(key_field= field->new_key_field(thd->mem_root, table,
                                              group_buff,
                                              field->null_ptr,
                                              field->null_bit)))
	  goto err;
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL; //todo need this?
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (thd->is_fatal_error)				// If end of memory
    goto err;
  share->db_record_offset= 1;
  if (share->db_type() == TMP_ENGINE_HTON)
  {
    recinfo++;
    if (create_internal_tmp_table(table, keyinfo, start_recinfo, &recinfo, 0))
      goto err;
  }
  sjtbl->start_recinfo= start_recinfo;
  sjtbl->recinfo=       recinfo;
  if (open_tmp_table(table))
    goto err;

  thd->mem_root= mem_root_save;
  DBUG_RETURN(table);

err:
  thd->mem_root= mem_root_save;
  free_tmp_table(thd,table);                    /* purecov: inspected */
  if (temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
  DBUG_RETURN(NULL);				/* purecov: inspected */
}


/*
  SemiJoinDuplicateElimination: Reset the temporary table
*/

int do_sj_reset(SJ_TMP_TABLE *sj_tbl)
{
  DBUG_ENTER("do_sj_reset");
  if (sj_tbl->tmp_table)
  {
    int rc= sj_tbl->tmp_table->file->ha_delete_all_rows();
    DBUG_RETURN(rc);
  }
  sj_tbl->have_degenerate_row= FALSE;
  DBUG_RETURN(0);
}

/*
  SemiJoinDuplicateElimination: Weed out duplicate row combinations

  SYNPOSIS
    do_sj_dups_weedout()
      thd    Thread handle
      sjtbl  Duplicate weedout table

  DESCRIPTION
    Try storing current record combination of outer tables (i.e. their
    rowids) in the temporary table. This records the fact that we've seen 
    this record combination and also tells us if we've seen it before.

  RETURN
    -1  Error
    1   The row combination is a duplicate (discard it)
    0   The row combination is not a duplicate (continue)
*/

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl) 
{
  int error;
  SJ_TMP_TABLE::TAB *tab= sjtbl->tabs;
  SJ_TMP_TABLE::TAB *tab_end= sjtbl->tabs_end;
  uchar *ptr;
  uchar *nulls_ptr;

  DBUG_ENTER("do_sj_dups_weedout");

  if (sjtbl->is_degenerate)
  {
    if (sjtbl->have_degenerate_row) 
      DBUG_RETURN(1);

    sjtbl->have_degenerate_row= TRUE;
    DBUG_RETURN(0);
  }

  ptr= sjtbl->tmp_table->record[0] + 1;
  nulls_ptr= ptr;

  /* Put the the rowids tuple into table->record[0]: */

  // 1. Store the length 
  if (((Field_varstring*)(sjtbl->tmp_table->field[0]))->length_bytes == 1)
  {
    *ptr= (uchar)(sjtbl->rowid_len + sjtbl->null_bytes);
    ptr++;
  }
  else
  {
    int2store(ptr, sjtbl->rowid_len + sjtbl->null_bytes);
    ptr += 2;
  }

  // 2. Zero the null bytes 
  if (sjtbl->null_bytes)
  {
    bzero(ptr, sjtbl->null_bytes);
    ptr += sjtbl->null_bytes; 
  }

  // 3. Put the rowids
  for (uint i=0; tab != tab_end; tab++, i++)
  {
    handler *h= tab->join_tab->table->file;
    if (tab->join_tab->table->maybe_null && tab->join_tab->table->null_row)
    {
      /* It's a NULL-complemented row */
      *(nulls_ptr + tab->null_byte) |= tab->null_bit;
      bzero(ptr + tab->rowid_offset, h->ref_length);
    }
    else
    {
      /* Copy the rowid value */
      memcpy(ptr + tab->rowid_offset, h->ref, h->ref_length);
    }
  }

  error= sjtbl->tmp_table->file->ha_write_tmp_row(sjtbl->tmp_table->record[0]);
  if (error)
  {
    /* create_internal_tmp_table_from_heap will generate error if needed */
    if (!sjtbl->tmp_table->file->is_fatal_error(error, HA_CHECK_DUP))
      DBUG_RETURN(1); /* Duplicate */
    if (create_internal_tmp_table_from_heap(thd, sjtbl->tmp_table,
                                            sjtbl->start_recinfo,
                                            &sjtbl->recinfo, error, 1))
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


/*
  Setup the strategies to eliminate semi-join duplicates.
  
  SYNOPSIS
    setup_semijoin_dups_elimination()
      join           Join to process
      options        Join options (needed to see if join buffering will be 
                     used or not)
      no_jbuf_after  Another bit of information re where join buffering will
                     be used.

  DESCRIPTION
    Setup the strategies to eliminate semi-join duplicates. ATM there are 4
    strategies:

    1. DuplicateWeedout (use of temptable to remove duplicates based on rowids
                         of row combinations)
    2. FirstMatch (pick only the 1st matching row combination of inner tables)
    3. LooseScan (scanning the sj-inner table in a way that groups duplicates
                  together and picking the 1st one)
    4. SJ-Materialization.
    
    The join order has "duplicate-generating ranges", and every range is
    served by one strategy or a combination of FirstMatch with with some
    other strategy.
    
    "Duplicate-generating range" is defined as a range within the join order
    that contains all of the inner tables of a semi-join. All ranges must be
    disjoint, if tables of several semi-joins are interleaved, then the ranges
    are joined together, which is equivalent to converting
      SELECT ... WHERE oe1 IN (SELECT ie1 ...) AND oe2 IN (SELECT ie2 )
    to
      SELECT ... WHERE (oe1, oe2) IN (SELECT ie1, ie2 ... ...)
    .

    Applicability conditions are as follows:

    DuplicateWeedout strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~

      (ot|nt)*  [ it ((it|ot|nt)* (it|ot))]  (nt)*
      +------+  +=========================+  +---+
        (1)                 (2)               (3)

       (1) - Prefix of OuterTables (those that participate in 
             IN-equality and/or are correlated with subquery) and outer 
             Non-correlated tables.
       (2) - The handled range. The range starts with the first sj-inner
             table, and covers all sj-inner and outer tables 
             Within the range,  Inner, Outer, outer non-correlated tables
             may follow in any order.
       (3) - The suffix of outer non-correlated tables.
    
    FirstMatch strategy
    ~~~~~~~~~~~~~~~~~~~

      (ot|nt)*  [ it ((it|nt)* it) ]  (nt)*
      +------+  +==================+  +---+
        (1)             (2)          (3)

      (1) - Prefix of outer and non-correlated tables
      (2) - The handled range, which may contain only inner and
            non-correlated tables.
      (3) - The suffix of outer non-correlated tables.

    LooseScan strategy 
    ~~~~~~~~~~~~~~~~~~

     (ot|ct|nt) [ loosescan_tbl (ot|nt|it)* it ]  (ot|nt)*
     +--------+   +===========+ +=============+   +------+
        (1)           (2)          (3)              (4)
     
      (1) - Prefix that may contain any outer tables. The prefix must contain
            all the non-trivially correlated outer tables. (non-trivially means
            that the correlation is not just through the IN-equality).
      
      (2) - Inner table for which the LooseScan scan is performed.

      (3) - The remainder of the duplicate-generating range. It is served by 
            application of FirstMatch strategy, with the exception that
            outer IN-correlated tables are considered to be non-correlated.

      (4) - THe suffix of outer and outer non-correlated tables.

  
  The choice between the strategies is made by the join optimizer (see
  advance_sj_state() and fix_semijoin_strategies_for_picked_join_order()).
  This function sets up all fields/structures/etc needed for execution except
  for setup/initialization of semi-join materialization which is done in 
  setup_sj_materialization() (todo: can't we move that to here also?)

  RETURN
    FALSE  OK 
    TRUE   Out of memory error
*/

int setup_semijoin_dups_elimination(JOIN *join, ulonglong options, 
                                    uint no_jbuf_after)
{
  uint i;
  THD *thd= join->thd;
  DBUG_ENTER("setup_semijoin_dups_elimination");

  for (i= join->const_tables ; i < join->tables; )
  {
    JOIN_TAB *tab=join->join_tab + i;
    POSITION *pos= join->best_positions + i;
    uint keylen, keyno;
    switch (pos->sj_strategy) {
      case SJ_OPT_MATERIALIZE:
      case SJ_OPT_MATERIALIZE_SCAN:
        /* Do nothing */
        i+= pos->n_sj_tables;
        break;
      case SJ_OPT_LOOSE_SCAN:
      {
        /* We jump from the last table to the first one */
        tab->loosescan_match_tab= tab + pos->n_sj_tables - 1;

        /* Calculate key length */
        keylen= 0;
        keyno= pos->loosescan_key;
        for (uint kp=0; kp < pos->loosescan_parts; kp++)
          keylen += tab->table->key_info[keyno].key_part[kp].store_length;

        tab->loosescan_key_len= keylen;
        if (pos->n_sj_tables > 1) 
          tab[pos->n_sj_tables - 1].do_firstmatch= tab;
        i+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_DUPS_WEEDOUT:
      {
        /*
          Check for join buffering. If there is one, move the first table
          forwards, but do not destroy other duplicate elimination methods.
        */
        uint first_table= i;
        uint join_cache_level= join->thd->variables.join_cache_level;
        for (uint j= i; j < i + pos->n_sj_tables; j++)
        {
          /*
            When we'll properly take join buffering into account during
            join optimization, the below check should be changed to 
            "if (join->best_positions[j].use_join_buffer && 
                 j <= no_jbuf_after)".
            For now, use a rough criteria:
          */
          JOIN_TAB *js_tab=join->join_tab + j; 
          if (j != join->const_tables && js_tab->use_quick != 2 &&
              j <= no_jbuf_after &&
              ((js_tab->type == JT_ALL && join_cache_level != 0) ||
               (join_cache_level > 4 && (tab->type == JT_REF || 
                                         tab->type == JT_EQ_REF))))
          {
            /* Looks like we'll be using join buffer */
            first_table= join->const_tables;
            break;
          }
        }

        SJ_TMP_TABLE::TAB sjtabs[MAX_TABLES];
        SJ_TMP_TABLE::TAB *last_tab= sjtabs;
        uint jt_rowid_offset= 0; // # tuple bytes are already occupied (w/o NULL bytes)
        uint jt_null_bits= 0;    // # null bits in tuple bytes
        /*
          Walk through the range and remember
           - tables that need their rowids to be put into temptable
           - the last outer table
        */
        for (JOIN_TAB *j=join->join_tab + first_table; 
             j < join->join_tab + i + pos->n_sj_tables; j++)
        {
          if (sj_table_is_included(join, j))
          {
            last_tab->join_tab= j;
            last_tab->rowid_offset= jt_rowid_offset;
            jt_rowid_offset += j->table->file->ref_length;
            if (j->table->maybe_null)
            {
              last_tab->null_byte= jt_null_bits / 8;
              last_tab->null_bit= jt_null_bits++;
            }
            last_tab++;
            j->table->prepare_for_position();
            j->keep_current_rowid= TRUE;
          }
        }

        SJ_TMP_TABLE *sjtbl;
        if (jt_rowid_offset) /* Temptable has at least one rowid */
        {
          uint tabs_size= (last_tab - sjtabs) * sizeof(SJ_TMP_TABLE::TAB);
          if (!(sjtbl= (SJ_TMP_TABLE*)thd->alloc(sizeof(SJ_TMP_TABLE))) ||
              !(sjtbl->tabs= (SJ_TMP_TABLE::TAB*) thd->alloc(tabs_size)))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          memcpy(sjtbl->tabs, sjtabs, tabs_size);
          sjtbl->is_degenerate= FALSE;
          sjtbl->tabs_end= sjtbl->tabs + (last_tab - sjtabs);
          sjtbl->rowid_len= jt_rowid_offset;
          sjtbl->null_bits= jt_null_bits;
          sjtbl->null_bytes= (jt_null_bits + 7)/8;
          sjtbl->tmp_table= 
            create_duplicate_weedout_tmp_table(thd, 
                                               sjtbl->rowid_len + 
                                               sjtbl->null_bytes,
                                               sjtbl);
          join->sj_tmp_tables.push_back(sjtbl->tmp_table);
        }
        else
        {
          /* 
            This is a special case where the entire subquery predicate does 
            not depend on anything at all, ie this is 
              WHERE const IN (uncorrelated select)
          */
          if (!(sjtbl= (SJ_TMP_TABLE*)thd->alloc(sizeof(SJ_TMP_TABLE))))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          sjtbl->tmp_table= NULL;
          sjtbl->is_degenerate= TRUE;
          sjtbl->have_degenerate_row= FALSE;
        }
        join->join_tab[first_table].flush_weedout_table= sjtbl;
        join->join_tab[i + pos->n_sj_tables - 1].check_weed_out_table= sjtbl;

        i+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_FIRST_MATCH:
      {
        JOIN_TAB *j, *jump_to= tab-1;
        for (j= tab; j != tab + pos->n_sj_tables; j++)
        {
          /*
            NOTE: this loop probably doesn't do the right thing for the case 
            where FirstMatch's duplicate-generating range is interleaved with
            "unrelated" tables (as specified in WL#3750, section 2.2).
          */
          if (!j->emb_sj_nest)
            jump_to= tab;
          else
          {
            j->first_sj_inner_tab= tab;
            j->last_sj_inner_tab= tab + pos->n_sj_tables - 1;
          }
        }
        j[-1].do_firstmatch= jump_to;
        i+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_NONE:
        i++;
        break;
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Destroy all temporary tables created by NL-semijoin runtime
*/

void destroy_sj_tmp_tables(JOIN *join)
{
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table= it++))
  {
    /* 
      SJ-Materialization tables are initialized for either sequential reading 
      or index lookup, DuplicateWeedout tables are not initialized for read 
      (we only write to them), so need to call ha_index_or_rnd_end.
    */
    table->file->ha_index_or_rnd_end();
    free_tmp_table(join->thd, table);
  }
  join->sj_tmp_tables.empty();
  join->sjm_info_list.empty();
}


/*
  Remove all records from all temp tables used by NL-semijoin runtime

  SYNOPSIS
    clear_sj_tmp_tables()
      join  The join to remove tables for

  DESCRIPTION
    Remove all records from all temp tables used by NL-semijoin runtime. This 
    must be done before every join re-execution.
*/

int clear_sj_tmp_tables(JOIN *join)
{
  int res;
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table= it++))
  {
    if ((res= table->file->ha_delete_all_rows()))
      return res; /* purecov: inspected */
  }

  SJ_MATERIALIZATION_INFO *sjm;
  List_iterator<SJ_MATERIALIZATION_INFO> it2(join->sjm_info_list);
  while ((sjm= it2++))
  {
    sjm->materialized= FALSE;
  }
  return 0;
}


/*
  Check if the table's rowid is included in the temptable

  SYNOPSIS
    sj_table_is_included()
      join      The join
      join_tab  The table to be checked

  DESCRIPTION
    SemiJoinDuplicateElimination: check the table's rowid should be included
    in the temptable. This is so if

    1. The table is not embedded within some semi-join nest
    2. The has been pulled out of a semi-join nest, or

    3. The table is functionally dependent on some previous table

    [4. This is also true for constant tables that can't be
        NULL-complemented but this function is not called for such tables]

  RETURN
    TRUE  - Include table's rowid
    FALSE - Don't
*/

static bool sj_table_is_included(JOIN *join, JOIN_TAB *join_tab)
{
  if (join_tab->emb_sj_nest)
    return FALSE;
  
  /* Check if this table is functionally dependent on the tables that
     are within the same outer join nest
  */
  TABLE_LIST *embedding= join_tab->table->pos_in_table_list->embedding;
  if (join_tab->type == JT_EQ_REF)
  {
    table_map depends_on= 0;
    uint idx;

    for (uint kp= 0; kp < join_tab->ref.key_parts; kp++)
      depends_on |= join_tab->ref.items[kp]->used_tables();

    Table_map_iterator it(depends_on & ~PSEUDO_TABLE_BITS);
    while ((idx= it.next_bit())!=Table_map_iterator::BITMAP_END)
    {
      JOIN_TAB *ref_tab= join->map2table[idx];
      if (embedding != ref_tab->table->pos_in_table_list->embedding)
        return TRUE;
    }
    /* Ok, functionally dependent */
    return FALSE;
  }
  /* Not functionally dependent => need to include*/
  return TRUE;
}


/*
  Index lookup-based subquery: save some flags for EXPLAIN output

  SYNOPSIS
    save_index_subquery_explain_info()
      join_tab  Subquery's join tab (there is only one as index lookup is
                only used for subqueries that are single-table SELECTs)
      where     Subquery's WHERE clause

  DESCRIPTION
    For index lookup-based subquery (i.e. one executed with
    subselect_uniquesubquery_engine or subselect_indexsubquery_engine),
    check its EXPLAIN output row should contain 
      "Using index" (TAB_INFO_FULL_SCAN_ON_NULL) 
      "Using Where" (TAB_INFO_USING_WHERE)
      "Full scan on NULL key" (TAB_INFO_FULL_SCAN_ON_NULL)
    and set appropriate flags in join_tab->packed_info.
*/

static void save_index_subquery_explain_info(JOIN_TAB *join_tab, Item* where)
{
  join_tab->packed_info= TAB_INFO_HAVE_VALUE;
  if (join_tab->table->covering_keys.is_set(join_tab->ref.key))
    join_tab->packed_info |= TAB_INFO_USING_INDEX;
  if (where)
    join_tab->packed_info |= TAB_INFO_USING_WHERE;
  for (uint i = 0; i < join_tab->ref.key_parts; i++)
  {
    if (join_tab->ref.cond_guards[i])
    {
      join_tab->packed_info |= TAB_INFO_FULL_SCAN_ON_NULL;
      break;
    }
  }
}


/*
  Check if the join can be rewritten to [unique_]indexsubquery_engine

  DESCRIPTION
    Check if the join can be changed into [unique_]indexsubquery_engine.

    The check is done after join optimization, the idea is that if the join
    has only one table and uses a [eq_]ref access generated from subselect's
    IN-equality then we replace it with a subselect_indexsubquery_engine or a
    subselect_uniquesubquery_engine.

  RETURN 
    0 - Ok, rewrite done (stop join optimization and return)
    1 - Fatal error (stop join optimization and return)
   -1 - No rewrite performed, continue with join optimization
*/

int rewrite_to_index_subquery_engine(JOIN *join)
{
  THD *thd= join->thd;
  JOIN_TAB* join_tab=join->join_tab;
  SELECT_LEX_UNIT *unit= join->unit;
  DBUG_ENTER("rewrite_to_index_subquery_engine");
  /*
    is this simple IN subquery?
  */
  if (!join->group_list && !join->order &&
      join->unit->item && 
      join->unit->item->substype() == Item_subselect::IN_SUBS &&
      join->tables == 1 && join->conds &&
      !join->unit->is_union())
  {
    if (!join->having)
    {
      Item *where= join->conds;
      if (join_tab[0].type == JT_EQ_REF &&
	  join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
        remove_subq_pushed_predicates(join, &where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= JT_UNIQUE_SUBQUERY;
        join->error= 0;
        DBUG_RETURN(unit->item->
                    change_engine(new
                                  subselect_uniquesubquery_engine(thd,
                                                                  join_tab,
                                                                  unit->item,
                                                                  where)));
      }
      else if (join_tab[0].type == JT_REF &&
	       join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
	remove_subq_pushed_predicates(join, &where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= JT_INDEX_SUBQUERY;
        join->error= 0;
        DBUG_RETURN(unit->item->
                    change_engine(new
                                  subselect_indexsubquery_engine(thd,
                                                                 join_tab,
                                                                 unit->item,
                                                                 where,
                                                                 NULL,
                                                                 0)));
      }
    } else if (join_tab[0].type == JT_REF_OR_NULL &&
	       join_tab[0].ref.items[0]->name == in_left_expr_name &&
               join->having->name == in_having_cond)
    {
      join_tab[0].type= JT_INDEX_SUBQUERY;
      join->error= 0;
      join->conds= remove_additional_cond(join->conds);
      save_index_subquery_explain_info(join_tab, join->conds);
      DBUG_RETURN(unit->item->
		  change_engine(new subselect_indexsubquery_engine(thd,
								   join_tab,
								   unit->item,
								   join->conds,
                                                                   join->having,
								   1)));
    }
  }

  DBUG_RETURN(-1); /* Haven't done the rewrite */
}


/**
  Remove additional condition inserted by IN/ALL/ANY transformation.

  @param conds   condition for processing

  @return
    new conditions
*/

static Item *remove_additional_cond(Item* conds)
{
  if (conds->name == in_additional_cond)
    return 0;
  if (conds->type() == Item::COND_ITEM)
  {
    Item_cond *cnd= (Item_cond*) conds;
    List_iterator<Item> li(*(cnd->argument_list()));
    Item *item;
    while ((item= li++))
    {
      if (item->name == in_additional_cond)
      {
	li.remove();
	if (cnd->argument_list()->elements == 1)
	  return cnd->argument_list()->head();
	return conds;
      }
    }
  }
  return conds;
}


/*
  Remove the predicates pushed down into the subquery

  SYNOPSIS
    remove_subq_pushed_predicates()
      where   IN  Must be NULL
              OUT The remaining WHERE condition, or NULL

  DESCRIPTION
    Given that this join will be executed using (unique|index)_subquery,
    without "checking NULL", remove the predicates that were pushed down
    into the subquery.

    If the subquery compares scalar values, we can remove the condition that
    was wrapped into trig_cond (it will be checked when needed by the subquery
    engine)

    If the subquery compares row values, we need to keep the wrapped
    equalities in the WHERE clause: when the left (outer) tuple has both NULL
    and non-NULL values, we'll do a full table scan and will rely on the
    equalities corresponding to non-NULL parts of left tuple to filter out
    non-matching records.

    TODO: We can remove the equalities that will be guaranteed to be true by the
    fact that subquery engine will be using index lookup. This must be done only
    for cases where there are no conversion errors of significance, e.g. 257
    that is searched in a byte. But this requires homogenization of the return 
    codes of all Field*::store() methods.
*/

static void remove_subq_pushed_predicates(JOIN *join, Item **where)
{
  if (join->conds->type() == Item::FUNC_ITEM &&
      ((Item_func *)join->conds)->functype() == Item_func::EQ_FUNC &&
      ((Item_func *)join->conds)->arguments()[0]->type() == Item::REF_ITEM &&
      ((Item_func *)join->conds)->arguments()[1]->type() == Item::FIELD_ITEM &&
      test_if_ref (join->conds,
                   (Item_field *)((Item_func *)join->conds)->arguments()[1],
                   ((Item_func *)join->conds)->arguments()[0]))
  {
    *where= 0;
    return;
  }
}


