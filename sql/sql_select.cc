/* Copyright (C) 2000-2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* mysql_select and join optimization */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

#include <m_ctype.h>
#include <hash.h>
#include <ft_global.h>

const char *join_type_str[]={ "UNKNOWN","system","const","eq_ref","ref",
			      "MAYBE_REF","ALL","range","index","fulltext",
			      "ref_or_null","unique_subquery","index_subquery",
                              "index_merge"
};

static void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array);
static bool make_join_statistics(JOIN *join, TABLE_LIST *leaves, COND *conds,
				 DYNAMIC_ARRAY *keyuse);
static bool update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,
				JOIN_TAB *join_tab,
                                uint tables, COND *conds,
                                COND_EQUAL *cond_equal,
				table_map table_map, SELECT_LEX *select_lex);
static int sort_keyuse(KEYUSE *a,KEYUSE *b);
static void set_position(JOIN *join,uint index,JOIN_TAB *table,KEYUSE *key);
static bool create_ref_for_key(JOIN *join, JOIN_TAB *j, KEYUSE *org_keyuse,
			       table_map used_tables);
static void choose_plan(JOIN *join,table_map join_tables);

static void best_access_path(JOIN *join, JOIN_TAB *s, THD *thd,
                             table_map remaining_tables, uint idx,
                             double record_count, double read_time);
static void optimize_straight_join(JOIN *join, table_map join_tables);
static void greedy_search(JOIN *join, table_map remaining_tables,
                             uint depth, uint prune_level);
static void best_extension_by_limited_search(JOIN *join,
                                             table_map remaining_tables,
                                             uint idx, double record_count,
                                             double read_time, uint depth,
                                             uint prune_level);
static uint determine_search_depth(JOIN* join);
static int join_tab_cmp(const void* ptr1, const void* ptr2);
static int join_tab_cmp_straight(const void* ptr1, const void* ptr2);
/*
  TODO: 'find_best' is here only temporarily until 'greedy_search' is
  tested and approved.
*/
static void find_best(JOIN *join,table_map rest_tables,uint index,
		      double record_count,double read_time);
static uint cache_record_length(JOIN *join,uint index);
static double prev_record_reads(JOIN *join,table_map found_ref);
static bool get_best_combination(JOIN *join);
static store_key *get_store_key(THD *thd,
				KEYUSE *keyuse, table_map used_tables,
				KEY_PART_INFO *key_part, char *key_buff,
				uint maybe_null);
static bool make_simple_join(JOIN *join,TABLE *tmp_table);
static void make_outerjoin_info(JOIN *join);
static bool make_join_select(JOIN *join,SQL_SELECT *select,COND *item);
static void make_join_readinfo(JOIN *join,uint options);
static bool only_eq_ref_tables(JOIN *join, ORDER *order, table_map tables);
static void update_depend_map(JOIN *join);
static void update_depend_map(JOIN *join, ORDER *order);
static ORDER *remove_const(JOIN *join,ORDER *first_order,COND *cond,
			   bool change_list, bool *simple_order);
static int return_zero_rows(JOIN *join, select_result *res,TABLE_LIST *tables,
                            List<Item> &fields, bool send_row,
                            uint select_options, const char *info,
                            Item *having);
static COND *build_equal_items(THD *thd, COND *cond,
                               COND_EQUAL *inherited,
                               List<TABLE_LIST> *join_list,
                               COND_EQUAL **cond_equal_ref);
static COND* substitute_for_best_equal_field(COND *cond,
                                             COND_EQUAL *cond_equal,
                                             void *table_join_idx);
static COND *simplify_joins(JOIN *join, List<TABLE_LIST> *join_list,
                            COND *conds, bool top);
static COND *optimize_cond(JOIN *join, COND *conds,
                           List<TABLE_LIST> *join_list,
			   Item::cond_result *cond_value);
static bool resolve_nested_join (TABLE_LIST *table);
static COND *remove_eq_conds(THD *thd, COND *cond, 
			     Item::cond_result *cond_value);
static bool const_expression_in_where(COND *conds,Item *item, Item **comp_item);
static bool open_tmp_table(TABLE *table);
static bool create_myisam_tmp_table(TABLE *table,TMP_TABLE_PARAM *param,
				    ulong options);
static Next_select_func setup_end_select_func(JOIN *join);
static int do_select(JOIN *join,List<Item> *fields,TABLE *tmp_table,
		     Procedure *proc);

static enum_nested_loop_state
sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
evaluate_join_record(JOIN *join, JOIN_TAB *join_tab,
                     int error, my_bool *report_error);
static enum_nested_loop_state
evaluate_null_complemented_join_record(JOIN *join, JOIN_TAB *join_tab);
static enum_nested_loop_state
sub_select(JOIN *join,JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
flush_cached_records(JOIN *join, JOIN_TAB *join_tab, bool skip_last);
static enum_nested_loop_state
end_send(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_send_group(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_write(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_unique_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_write_group(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);

static int test_if_group_changed(List<Cached_item> &list);
static int join_read_const_table(JOIN_TAB *tab, POSITION *pos);
static int join_read_system(JOIN_TAB *tab);
static int join_read_const(JOIN_TAB *tab);
static int join_read_key(JOIN_TAB *tab);
static int join_read_always_key(JOIN_TAB *tab);
static int join_read_last_key(JOIN_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_init_quick_read_record(JOIN_TAB *tab);
static int test_if_quick_select(JOIN_TAB *tab);
static int join_init_read_record(JOIN_TAB *tab);
static int join_read_first(JOIN_TAB *tab);
static int join_read_next(READ_RECORD *info);
static int join_read_next_same(READ_RECORD *info);
static int join_read_last(JOIN_TAB *tab);
static int join_read_prev_same(READ_RECORD *info);
static int join_read_prev(READ_RECORD *info);
static int join_ft_read_first(JOIN_TAB *tab);
static int join_ft_read_next(READ_RECORD *info);
static int join_read_always_key_or_null(JOIN_TAB *tab);
static int join_read_next_same_or_null(READ_RECORD *info);
static COND *make_cond_for_table(COND *cond,table_map table,
				 table_map used_table);
static Item* part_of_refkey(TABLE *form,Field *field);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);
static bool test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,
				    ha_rows select_limit, bool no_changes);
static int create_sort_index(THD *thd, JOIN *join, ORDER *order,
			     ha_rows filesort_limit, ha_rows select_limit);
static int remove_duplicates(JOIN *join,TABLE *entry,List<Item> &fields,
			     Item *having);
static int remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
				   ulong offset,Item *having);
static int remove_dup_with_hash_index(THD *thd,TABLE *table,
				      uint field_count, Field **first_field,

				      ulong key_length,Item *having);
static int join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count);
static ulong used_blob_length(CACHE_FIELD **ptr);
static bool store_record_in_cache(JOIN_CACHE *cache);
static void reset_cache_read(JOIN_CACHE *cache);
static void reset_cache_write(JOIN_CACHE *cache);
static void read_cached_record(JOIN_TAB *tab);
static bool cmp_buffer_with_ref(JOIN_TAB *tab);
static bool setup_new_fields(THD *thd, List<Item> &fields,
			     List<Item> &all_fields, ORDER *new_order);
static ORDER *create_distinct_group(THD *thd, Item **ref_pointer_array,
                                    ORDER *order, List<Item> &fields,
				    bool *all_order_by_fields_used);
static bool test_if_subpart(ORDER *a,ORDER *b);
static TABLE *get_sort_by_table(ORDER *a,ORDER *b,TABLE_LIST *tables);
static void calc_group_buffer(JOIN *join,ORDER *group);
static bool make_group_fields(JOIN *main_join, JOIN *curr_join);
static bool alloc_group_fields(JOIN *join,ORDER *group);
// Create list for using with tempory table
static bool change_to_use_tmp_fields(THD *thd, Item **ref_pointer_array,
				     List<Item> &new_list1,
				     List<Item> &new_list2,
				     uint elements, List<Item> &items);
// Create list for using with tempory table
static bool change_refs_to_tmp_fields(THD *thd, Item **ref_pointer_array,
				      List<Item> &new_list1,
				      List<Item> &new_list2,
				      uint elements, List<Item> &items);
static void init_tmptable_sum_functions(Item_sum **func);
static void update_tmptable_sum_func(Item_sum **func,TABLE *tmp_table);
static void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end);
static bool add_ref_to_table_cond(THD *thd, JOIN_TAB *join_tab);
static bool setup_sum_funcs(THD *thd, Item_sum **func_ptr);
static bool init_sum_functions(Item_sum **func, Item_sum **end);
static bool update_sum_func(Item_sum **func);
static void select_describe(JOIN *join, bool need_tmp_table,bool need_order,
			    bool distinct, const char *message=NullS);
static Item *remove_additional_cond(Item* conds);
static void add_group_and_distinct_keys(JOIN *join, JOIN_TAB *join_tab);


/*
  This handles SELECT with and without UNION
*/

bool handle_select(THD *thd, LEX *lex, select_result *result,
                   ulong setup_tables_done_option)
{
  bool res;
  register SELECT_LEX *select_lex = &lex->select_lex;
  DBUG_ENTER("handle_select");

  if (select_lex->next_select())
    res= mysql_union(thd, lex, result, &lex->unit, setup_tables_done_option);
  else
  {
    SELECT_LEX_UNIT *unit= &lex->unit;
    unit->set_limit(unit->global_parameters);
    /*
      'options' of mysql_select will be set in JOIN, as far as JOIN for
      every PS/SP execution new, we will not need reset this flag if 
      setup_tables_done_option changed for next rexecution
    */
    res= mysql_select(thd, &select_lex->ref_pointer_array,
		      (TABLE_LIST*) select_lex->table_list.first,
		      select_lex->with_wild, select_lex->item_list,
		      select_lex->where,
		      select_lex->order_list.elements +
		      select_lex->group_list.elements,
		      (ORDER*) select_lex->order_list.first,
		      (ORDER*) select_lex->group_list.first,
		      select_lex->having,
		      (ORDER*) lex->proc_list.first,
		      select_lex->options | thd->options |
                      setup_tables_done_option,
		      result, unit, select_lex);
  }
  DBUG_PRINT("info",("res: %d  report_error: %d", res,
		     thd->net.report_error));
  res|= thd->net.report_error;
  if (unlikely(res))
  {
    /* If we had a another error reported earlier then this will be ignored */
    result->send_error(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR));
    result->abort();
  }
  DBUG_RETURN(res);
}


/*
  Function to setup clauses without sum functions
*/
inline int setup_without_group(THD *thd, Item **ref_pointer_array,
			       TABLE_LIST *tables,
			       TABLE_LIST *leaves,
			       List<Item> &fields,
			       List<Item> &all_fields,
			       COND **conds,
			       ORDER *order,
			       ORDER *group, bool *hidden_group_fields)
{
  bool save_allow_sum_func;
  int res;
  DBUG_ENTER("setup_without_group");

  save_allow_sum_func= thd->allow_sum_func;
  thd->allow_sum_func= 0;
  res= setup_conds(thd, tables, leaves, conds);

  thd->allow_sum_func= save_allow_sum_func;
  res= res || setup_order(thd, ref_pointer_array, tables, fields, all_fields,
                          order);
  thd->allow_sum_func= 0;
  res= res || setup_group(thd, ref_pointer_array, tables, fields, all_fields,
                          group, hidden_group_fields);
  thd->allow_sum_func= save_allow_sum_func;
  DBUG_RETURN(res);
}

/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  mysql_select assumes that all tables are already opened
*****************************************************************************/

/*
  Prepare of whole select (including sub queries in future).
  return -1 on error
          0 on success
*/
int
JOIN::prepare(Item ***rref_pointer_array,
	      TABLE_LIST *tables_init,
	      uint wild_num, COND *conds_init, uint og_num,
	      ORDER *order_init, ORDER *group_init,
	      Item *having_init,
	      ORDER *proc_param_init, SELECT_LEX *select_lex_arg,
	      SELECT_LEX_UNIT *unit_arg)
{
  DBUG_ENTER("JOIN::prepare");

  // to prevent double initialization on EXPLAIN
  if (optimized)
    DBUG_RETURN(0);

  conds= conds_init;
  order= order_init;
  group_list= group_init;
  having= having_init;
  proc_param= proc_param_init;
  tables_list= tables_init;
  select_lex= select_lex_arg;
  select_lex->join= this;
  join_list= &select_lex->top_join_list;
  union_part= (unit_arg->first_select()->next_select() != 0);

  /*
    If we have already executed SELECT, then it have not sense to prevent
    its table from update (see unique_table())
  */
  if (thd->derived_tables_processing)
    select_lex->exclude_from_table_unique_test= TRUE;

  /* Check that all tables, fields, conds and order are ok */

  if ((!(select_options & OPTION_SETUP_TABLES_DONE) &&
       setup_tables(thd, &select_lex->context, join_list,
                    tables_list, &conds, &select_lex->leaf_tables,
                    FALSE)) ||
      setup_wild(thd, tables_list, fields_list, &all_fields, wild_num) ||
      select_lex->setup_ref_array(thd, og_num) ||
      setup_fields(thd, (*rref_pointer_array), fields_list, 1,
		   &all_fields, 1) ||
      setup_without_group(thd, (*rref_pointer_array), tables_list,
			  select_lex->leaf_tables, fields_list,
			  all_fields, &conds, order, group_list,
			  &hidden_group_fields))
    DBUG_RETURN(-1);				/* purecov: inspected */

  ref_pointer_array= *rref_pointer_array;
  
  if (having)
  {
    thd->where="having clause";
    thd->allow_sum_func=1;
    select_lex->having_fix_field= 1;
    bool having_fix_rc= (!having->fixed &&
			 (having->fix_fields(thd, &having) ||
			  having->check_cols(1)));
    select_lex->having_fix_field= 0;
    if (having_fix_rc || thd->net.report_error)
      DBUG_RETURN(-1);				/* purecov: inspected */
    if (having->with_sum_func)
      having->split_sum_func(thd, ref_pointer_array, all_fields);
  }

  if (!thd->lex->view_prepare_mode)
  {
    Item_subselect *subselect;
    /* Is it subselect? */
    if ((subselect= select_lex->master_unit()->item))
    {
      Item_subselect::trans_res res;
      if ((res= subselect->select_transformer(this)) !=
	  Item_subselect::RES_OK)
      {
        select_lex->fix_prepare_information(thd, &conds);
	DBUG_RETURN((res == Item_subselect::RES_ERROR));
      }
    }
  }

  if (setup_ftfuncs(select_lex)) /* should be after having->fix_fields */
    DBUG_RETURN(-1);
  

  /*
    Check if one one uses a not constant column with group functions
    and no GROUP BY.
    TODO:  Add check of calculation of GROUP functions and fields:
	   SELECT COUNT(*)+table.col1 from table1;
  */
  {
    if (!group_list)
    {
      uint flag=0;
      List_iterator_fast<Item> it(fields_list);
      Item *item;
      while ((item= it++))
      {
	if (item->with_sum_func)
	  flag|=1;
	else if (!(flag & 2) && !item->const_during_execution())
	  flag|=2;
      }
      if (flag == 3)
      {
	my_message(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,
                   ER(ER_MIX_OF_GROUP_FUNC_AND_FIELDS), MYF(0));
	DBUG_RETURN(-1);
      }
    }
    TABLE_LIST *table_ptr;
    for (table_ptr= select_lex->leaf_tables;
	 table_ptr;
	 table_ptr= table_ptr->next_leaf)
      tables++;
  }
  {
    /* Caclulate the number of groups */
    send_group_parts= 0;
    for (ORDER *group_tmp= group_list ; group_tmp ; group_tmp= group_tmp->next)
      send_group_parts++;
  }
  
  procedure= setup_procedure(thd, proc_param, result, fields_list, &error);
  if (error)
    goto err;					/* purecov: inspected */
  if (procedure)
  {
    if (setup_new_fields(thd, fields_list, all_fields,
			 procedure->param_fields))
	goto err;				/* purecov: inspected */
    if (procedure->group)
    {
      if (!test_if_subpart(procedure->group,group_list))
      {						/* purecov: inspected */
	my_message(ER_DIFF_GROUPS_PROC, ER(ER_DIFF_GROUPS_PROC),
                   MYF(0));                     /* purecov: inspected */
	goto err;				/* purecov: inspected */
      }
    }
#ifdef NOT_NEEDED
    else if (!group_list && procedure->flags & PROC_GROUP)
    {
      my_message(ER_NO_GROUP_FOR_PROC, MYF(0));
      goto err;
    }
#endif
    if (order && (procedure->flags & PROC_NO_SORT))
    {						/* purecov: inspected */
      my_message(ER_ORDER_WITH_PROC, ER(ER_ORDER_WITH_PROC),
                 MYF(0));                       /* purecov: inspected */
      goto err;					/* purecov: inspected */
    }
  }

  /* Init join struct */
  count_field_types(&tmp_table_param, all_fields, 0);
  ref_pointer_array_size= all_fields.elements*sizeof(Item*);
  this->group= group_list != 0;
  unit= unit_arg;

#ifdef RESTRICTED_GROUP
  if (sum_func_count && !group_list && (func_count || field_count))
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT),MYF(0));
    goto err;
  }
#endif
  if (!procedure && result && result->prepare(fields_list, unit_arg))
    goto err;					/* purecov: inspected */

  if (select_lex->olap == ROLLUP_TYPE && rollup_init())
    goto err;
  if (alloc_func_list())
    goto err;

  select_lex->fix_prepare_information(thd, &conds);
  DBUG_RETURN(0); // All OK

err:
  delete procedure;				/* purecov: inspected */
  procedure= 0;
  DBUG_RETURN(-1);				/* purecov: inspected */
}

/*
  test if it is known for optimisation IN subquery

  SYNOPSYS
    JOIN::test_in_subselect
    where - pointer for variable in which conditions should be
            stored if subquery is known

  RETURN
    1 - known
    0 - unknown
*/

bool JOIN::test_in_subselect(Item **where)
{
  if (conds->type() == Item::FUNC_ITEM &&
      ((Item_func *)this->conds)->functype() == Item_func::EQ_FUNC &&
      ((Item_func *)conds)->arguments()[0]->type() == Item::REF_ITEM &&
      ((Item_func *)conds)->arguments()[1]->type() == Item::FIELD_ITEM)
  {
    join_tab->info= "Using index";
    *where= 0;
    return 1;
  }
  if (conds->type() == Item::COND_ITEM &&
      ((class Item_func *)this->conds)->functype() ==
      Item_func::COND_AND_FUNC)
  {
    if ((*where= remove_additional_cond(conds)))
      join_tab->info= "Using index; Using where";
    else
      join_tab->info= "Using index";
    return 1;
  }
  return 0;
}

/*
  global select optimisation.
  return 0 - success
         1 - error
  error code saved in field 'error'
*/
int
JOIN::optimize()
{
  DBUG_ENTER("JOIN::optimize");
  // to prevent double initialization on EXPLAIN
  if (optimized)
    DBUG_RETURN(0);
  optimized= 1;

  row_limit= ((select_distinct || order || group_list) ? HA_POS_ERROR :
	      unit->select_limit_cnt);
  /* select_limit is used to decide if we are likely to scan the whole table */
  select_limit= unit->select_limit_cnt;
  if (having || (select_options & OPTION_FOUND_ROWS))
    select_limit= HA_POS_ERROR;
  do_send_rows = (unit->select_limit_cnt) ? 1 : 0;
  // Ignore errors of execution if option IGNORE present
  if (thd->lex->ignore)
    thd->lex->current_select->no_error= 1;
#ifdef HAVE_REF_TO_FIELDS			// Not done yet
  /* Add HAVING to WHERE if possible */
  if (having && !group_list && !sum_func_count)
  {
    if (!conds)
    {
      conds= having;
      having= 0;
    }
    else if ((conds=new Item_cond_and(conds,having)))
    {
      /*
        Item_cond_and can't be fixed after creation, so we do not check
        conds->fixed
      */
      conds->fix_fields(thd, &conds);
      conds->change_ref_to_fields(thd, tables_list);
      conds->top_level_item();
      having= 0;
    }
  }
#endif
  SELECT_LEX *sel= thd->lex->current_select;
  if (sel->first_cond_optimization)
  {
    /*
      The following code will allocate the new items in a permanent
      MEMROOT for prepared statements and stored procedures.
    */

    Query_arena *arena= thd->stmt_arena, backup;
    if (arena->is_conventional())
      arena= 0;                                   // For easier test
    else
      thd->set_n_backup_active_arena(arena, &backup);

    sel->first_cond_optimization= 0;

    /* Convert all outer joins to inner joins if possible */
    conds= simplify_joins(this, join_list, conds, TRUE);

    sel->prep_where= conds ? conds->copy_andor_structure(thd) : 0;

    if (arena)
      thd->restore_active_arena(arena, &backup);
  }

  conds= optimize_cond(this, conds, join_list, &cond_value);   
  if (thd->net.report_error)
  {
    error= 1;
    DBUG_PRINT("error",("Error from optimize_cond"));
    DBUG_RETURN(1);
  }

  if (cond_value == Item::COND_FALSE ||
      (!unit->select_limit_cnt && !(select_options & OPTION_FOUND_ROWS)))
  {						/* Impossible cond */
    DBUG_PRINT("info", ("Impossible WHERE"));
    zero_result_cause= "Impossible WHERE";
    error= 0;
    DBUG_RETURN(0);
  }

  /* Optimize count(*), min() and max() */
  if (tables_list && tmp_table_param.sum_func_count && ! group_list)
  {
    int res;
    /*
      opt_sum_query() returns -1 if no rows match to the WHERE conditions,
      or 1 if all items were resolved, or 0, or an error number HA_ERR_...
    */
    if ((res=opt_sum_query(select_lex->leaf_tables, all_fields, conds)))
    {
      if (res > 1)
      {
        DBUG_PRINT("error",("Error from opt_sum_query"));
	DBUG_RETURN(1);
      }
      if (res < 0)
      {
        DBUG_PRINT("info",("No matching min/max row"));
	zero_result_cause= "No matching min/max row";
	error=0;
	DBUG_RETURN(0);
      }
      DBUG_PRINT("info",("Select tables optimized away"));
      zero_result_cause= "Select tables optimized away";
      tables_list= 0;				// All tables resolved
    }
  }
  if (!tables_list)
  {
    DBUG_PRINT("info",("No tables"));
    error= 0;
    DBUG_RETURN(0);
  }
  error= -1;					// Error is sent to client
  sort_by_table= get_sort_by_table(order, group_list, select_lex->leaf_tables);

  /* Calculate how to do the join */
  thd->proc_info= "statistics";
  if (make_join_statistics(this, select_lex->leaf_tables, conds, &keyuse) ||
      thd->is_fatal_error)
  {
    DBUG_PRINT("error",("Error: make_join_statistics() failed"));
    DBUG_RETURN(1);
  }

  /* Remove distinct if only const tables */
  select_distinct= select_distinct && (const_tables != tables);
  thd->proc_info= "preparing";
  if (result->initialize_tables(this))
  {
    DBUG_PRINT("error",("Error: initialize_tables() failed"));
    DBUG_RETURN(1);				// error == -1
  }
  if (const_table_map != found_const_table_map &&
      !(select_options & SELECT_DESCRIBE) &&
      (!conds ||
       !(conds->used_tables() & RAND_TABLE_BIT) ||
       select_lex->master_unit() == &thd->lex->unit)) // upper level SELECT
  {
    zero_result_cause= "no matching row in const table";
    DBUG_PRINT("error",("Error: %s", zero_result_cause));
    error= 0;
    DBUG_RETURN(0);
  }
  if (!(thd->options & OPTION_BIG_SELECTS) &&
      best_read > (double) thd->variables.max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  {						/* purecov: inspected */
    my_message(ER_TOO_BIG_SELECT, ER(ER_TOO_BIG_SELECT), MYF(0));
    error= -1;
    DBUG_RETURN(1);
  }
  if (const_tables && !thd->locked_tables &&
      !(select_options & SELECT_NO_UNLOCK))
    mysql_unlock_some_tables(thd, table, const_tables);
  if (!conds && outer_join)
  {
    /* Handle the case where we have an OUTER JOIN without a WHERE */
    conds=new Item_int((longlong) 1,1);	// Always true
  }
  select= make_select(*table, const_table_map,
                      const_table_map, conds, 1, &error);
  if (error)
  {						/* purecov: inspected */
    error= -1;					/* purecov: inspected */
    DBUG_PRINT("error",("Error: make_select() failed"));
    DBUG_RETURN(1);
  }

  make_outerjoin_info(this);

  /*
    Among the equal fields belonging to the same multiple equality
    choose the one that is to be retrieved first and substitute
    all references to these in where condition for a reference for
    the selected field.
  */
  if (conds)
  {
    conds= substitute_for_best_equal_field(conds, cond_equal, map2table);
    conds->update_used_tables();
    DBUG_EXECUTE("where", print_where(conds, "after substitute_best_equal"););
  }
  /*
    Permorm the the optimization on fields evaluation mentioned above
    for all on expressions.
  */ 
  for (JOIN_TAB *tab= join_tab + const_tables; tab < join_tab + tables ; tab++)
  {
    if (*tab->on_expr_ref)
    {
      *tab->on_expr_ref= substitute_for_best_equal_field(*tab->on_expr_ref,
                                                         tab->cond_equal,
                                                         map2table);
      (*tab->on_expr_ref)->update_used_tables();
    }
  }

  if (make_join_select(this, select, conds))
  {
    zero_result_cause=
      "Impossible WHERE noticed after reading const tables";
    DBUG_RETURN(0);				// error == 0
  }

  error= -1;					/* if goto err */

  /* Optimize distinct away if possible */
  {
    ORDER *org_order= order;
    order=remove_const(this, order,conds,1, &simple_order);
    /*
      If we are using ORDER BY NULL or ORDER BY const_expression,
      return result in any order (even if we are using a GROUP BY)
    */
    if (!order && org_order)
      skip_sort_order= 1;
  }
  if (group_list || tmp_table_param.sum_func_count)
  {
    if (! hidden_group_fields)
      select_distinct=0;
  }
  else if (select_distinct && tables - const_tables == 1)
  {
    /*
      We are only using one table. In this case we change DISTINCT to a
      GROUP BY query if:
      - The GROUP BY can be done through indexes (no sort) and the ORDER
        BY only uses selected fields.
	(In this case we can later optimize away GROUP BY and ORDER BY)
      - We are scanning the whole table without LIMIT
        This can happen if:
        - We are using CALC_FOUND_ROWS
        - We are using an ORDER BY that can't be optimized away.

      We don't want to use this optimization when we are using LIMIT
      because in this case we can just create a temporary table that
      holds LIMIT rows and stop when this table is full.
    */
    JOIN_TAB *tab= &join_tab[const_tables];
    bool all_order_fields_used;
    if (order)
      skip_sort_order= test_if_skip_sort_order(tab, order, select_limit, 1);
    if ((group_list=create_distinct_group(thd, select_lex->ref_pointer_array,
                                          order, fields_list,
				          &all_order_fields_used)))
    {
      bool skip_group= (skip_sort_order &&
			test_if_skip_sort_order(tab, group_list, select_limit,
						1) != 0);
      if ((skip_group && all_order_fields_used) ||
	  select_limit == HA_POS_ERROR ||
	  (order && !skip_sort_order))
      {
	/*  Change DISTINCT to GROUP BY */
	select_distinct= 0;
	no_order= !order;
	if (all_order_fields_used)
	{
	  if (order && skip_sort_order)
	  {
	    /*
	      Force MySQL to read the table in sorted order to get result in
	      ORDER BY order.
	    */
	    tmp_table_param.quick_group=0;
	  }
	  order=0;
        }
	group=1;				// For end_write_group
      }
      else
	group_list= 0;
    }
    else if (thd->is_fatal_error)			// End of memory
      DBUG_RETURN(1);
  }
  simple_group= 0;
  {
    ORDER *old_group_list;
    group_list= remove_const(this, (old_group_list= group_list), conds,
                             rollup.state == ROLLUP::STATE_NONE,
			     &simple_group);
    if (old_group_list && !group_list)
      select_distinct= 0;
  }
  if (!group_list && group)
  {
    order=0;					// The output has only one row
    simple_order=1;
  }

  calc_group_buffer(this, group_list);
  send_group_parts= tmp_table_param.group_parts; /* Save org parts */
  if (procedure && procedure->group)
  {
    group_list= procedure->group= remove_const(this, procedure->group, conds,
					       1, &simple_group);
    calc_group_buffer(this, group_list);
  }

  if (test_if_subpart(group_list, order) ||
      (!group_list && tmp_table_param.sum_func_count))
    order=0;

  // Can't use sort on head table if using row cache
  if (full_join)
  {
    if (group_list)
      simple_group=0;
    if (order)
      simple_order=0;
  }

  /*
    Check if we need to create a temporary table.
    This has to be done if all tables are not already read (const tables)
    and one of the following conditions holds:
    - We are using DISTINCT (simple distinct's are already optimized away)
    - We are using an ORDER BY or GROUP BY on fields not in the first table
    - We are using different ORDER BY and GROUP BY orders
    - The user wants us to buffer the result.
  */
  need_tmp= (const_tables != tables &&
	     ((select_distinct || !simple_order || !simple_group) ||
	      (group_list && order) ||
	      test(select_options & OPTION_BUFFER_RESULT)));

  // No cache for MATCH
  make_join_readinfo(this,
		     (select_options & (SELECT_DESCRIBE |
					SELECT_NO_JOIN_CACHE)) |
		     (select_lex->ftfunc_list->elements ?
		      SELECT_NO_JOIN_CACHE : 0));

  /* Perform FULLTEXT search before all regular searches */
  if (!(select_options & SELECT_DESCRIBE))
    init_ftfuncs(thd, select_lex, test(order));

  /*
    is this simple IN subquery?
  */
  if (!group_list && !order &&
      unit->item && unit->item->substype() == Item_subselect::IN_SUBS &&
      tables == 1 && conds &&
      !unit->first_select()->next_select())
  {
    if (!having)
    {
      Item *where= 0;
      if (join_tab[0].type == JT_EQ_REF &&
	  join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
	if (test_in_subselect(&where))
	{
	  join_tab[0].type= JT_UNIQUE_SUBQUERY;
	  error= 0;
	  DBUG_RETURN(unit->item->
		      change_engine(new
				    subselect_uniquesubquery_engine(thd,
								    join_tab,
								    unit->item,
								    where)));
	}
      }
      else if (join_tab[0].type == JT_REF &&
	       join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
	if (test_in_subselect(&where))
	{
	  join_tab[0].type= JT_INDEX_SUBQUERY;
	  error= 0;
	  DBUG_RETURN(unit->item->
		      change_engine(new
				    subselect_indexsubquery_engine(thd,
								   join_tab,
								   unit->item,
								   where,
								   0)));
	}
      }
    } else if (join_tab[0].type == JT_REF_OR_NULL &&
	       join_tab[0].ref.items[0]->name == in_left_expr_name &&
	       having->type() == Item::FUNC_ITEM &&
	       ((Item_func *) having)->functype() ==
	       Item_func::ISNOTNULLTEST_FUNC)
    {
      join_tab[0].type= JT_INDEX_SUBQUERY;
      error= 0;

      if ((conds= remove_additional_cond(conds)))
	join_tab->info= "Using index; Using where";
      else
	join_tab->info= "Using index";

      DBUG_RETURN(unit->item->
		  change_engine(new subselect_indexsubquery_engine(thd,
								   join_tab,
								   unit->item,
								   conds,
								   1)));
    }

  }
  /*
    Need to tell Innobase that to play it safe, it should fetch all
    columns of the tables: this is because MySQL may build row
    pointers for the rows, and for all columns of the primary key the
    field->query_id has not necessarily been set to thd->query_id by
    MySQL.
  */

#ifdef HAVE_INNOBASE_DB
  if (need_tmp || select_distinct || group_list || order)
  {
    for (uint i_h = const_tables; i_h < tables; i_h++)
    {
      TABLE* table_h = join_tab[i_h].table;
      table_h->file->extra(HA_EXTRA_RETRIEVE_PRIMARY_KEY);
    }
  }
#endif

  DBUG_EXECUTE("info",TEST_join(this););

  if (const_tables != tables)
  {
    /*
      Because filesort always does a full table scan or a quick range scan
      we must add the removed reference to the select for the table.
      We only need to do this when we have a simple_order or simple_group
      as in other cases the join is done before the sort.
    */
    if ((order || group_list) &&
        join_tab[const_tables].type != JT_ALL &&
        join_tab[const_tables].type != JT_FT &&
        join_tab[const_tables].type != JT_REF_OR_NULL &&
        (order && simple_order || group_list && simple_group))
    {
      if (add_ref_to_table_cond(thd,&join_tab[const_tables]))
        DBUG_RETURN(1);
    }
    
    if (!(select_options & SELECT_BIG_RESULT) &&
        ((group_list &&
          (!simple_group ||
           !test_if_skip_sort_order(&join_tab[const_tables], group_list,
                                    unit->select_limit_cnt, 0))) ||
         select_distinct) &&
        tmp_table_param.quick_group && !procedure)
    {
      need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
    }
  }

  tmp_having= having;
  if (select_options & SELECT_DESCRIBE)
  {
    error= 0;
    DBUG_RETURN(0);
  }
  having= 0;

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    DBUG_PRINT("info",("Creating tmp table"));
    thd->proc_info="Creating tmp table";

    init_items_ref_array();

    tmp_table_param.hidden_field_count= (all_fields.elements -
					 fields_list.elements);
    if (!(exec_tmp_table1 =
	  create_tmp_table(thd, &tmp_table_param, all_fields,
			   ((!simple_group && !procedure &&
			     !(test_flags & TEST_NO_KEY_GROUP)) ?
			    group_list : (ORDER*) 0),
			   group_list ? 0 : select_distinct,
			   group_list && simple_group,
			   select_options,
			   (order == 0 || skip_sort_order) ? select_limit :
			   HA_POS_ERROR,
			   (char *) "")))
      DBUG_RETURN(1);

    /*
      We don't have to store rows in temp table that doesn't match HAVING if:
      - we are sorting the table and writing complete group rows to the
        temp table.
      - We are using DISTINCT without resolving the distinct as a GROUP BY
        on all columns.
      
      If having is not handled here, it will be checked before the row
      is sent to the client.
    */    
    if (tmp_having && 
	(sort_and_group || (exec_tmp_table1->distinct && !group_list)))
      having= tmp_having;

    /* if group or order on first table, sort first */
    if (group_list && simple_group)
    {
      DBUG_PRINT("info",("Sorting for group"));
      thd->proc_info="Sorting for group";
      if (create_sort_index(thd, this, group_list,
			    HA_POS_ERROR, HA_POS_ERROR) ||
	  alloc_group_fields(this, group_list) ||
          make_sum_func_list(all_fields, fields_list, 1) ||
          setup_sum_funcs(thd, sum_funcs))
	DBUG_RETURN(1);
      group_list=0;
    }
    else
    {
      if (make_sum_func_list(all_fields, fields_list, 0) ||
          setup_sum_funcs(thd, sum_funcs))
	DBUG_RETURN(1);
      if (!group_list && ! exec_tmp_table1->distinct && order && simple_order)
      {
	DBUG_PRINT("info",("Sorting for order"));
	thd->proc_info="Sorting for order";
	if (create_sort_index(thd, this, order,
                              HA_POS_ERROR, HA_POS_ERROR))
	  DBUG_RETURN(1);
	order=0;
      }
    }
    
    /*
      Optimize distinct when used on some of the tables
      SELECT DISTINCT t1.a FROM t1,t2 WHERE t1.b=t2.b
      In this case we can stop scanning t2 when we have found one t1.a
    */

    if (exec_tmp_table1->distinct)
    {
      table_map used_tables= thd->used_tables;
      JOIN_TAB *last_join_tab= join_tab+tables-1;
      do
      {
	if (used_tables & last_join_tab->table->map)
	  break;
	last_join_tab->not_used_in_distinct=1;
      } while (last_join_tab-- != join_tab);
      /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
      if (order && skip_sort_order)
      {
 	/* Should always succeed */
	if (test_if_skip_sort_order(&join_tab[const_tables],
				    order, unit->select_limit_cnt, 0))
	  order=0;
      }
    }

    if (select_lex->uncacheable && !is_top_level_join())
    {
      /* If this join belongs to an uncacheable subquery */
      if (!(tmp_join= (JOIN*)thd->alloc(sizeof(JOIN))))
	DBUG_RETURN(-1);
      error= 0;				// Ensure that tmp_join.error= 0
      restore_tmp();
    }
  }

  error= 0;
  DBUG_RETURN(0);
}


/*
  Restore values in temporary join
*/
void JOIN::restore_tmp()
{
  memcpy(tmp_join, this, (size_t) sizeof(JOIN));
}


int
JOIN::reinit()
{
  DBUG_ENTER("JOIN::reinit");
  first_record= 0;

  if (exec_tmp_table1)
  {
    exec_tmp_table1->file->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table1->file->delete_all_rows();
    free_io_cache(exec_tmp_table1);
    filesort_free_buffers(exec_tmp_table1);
  }
  if (exec_tmp_table2)
  {
    exec_tmp_table2->file->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table2->file->delete_all_rows();
    free_io_cache(exec_tmp_table2);
    filesort_free_buffers(exec_tmp_table2);
  }
  if (items0)
    set_items_ref_array(items0);

  if (join_tab_save)
    memcpy(join_tab, join_tab_save, sizeof(JOIN_TAB) * tables);

  if (tmp_join)
    restore_tmp();

  /* Reset of sum functions */
  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }

  DBUG_RETURN(0);
}


bool
JOIN::save_join_tab()
{
  if (!join_tab_save && select_lex->master_unit()->uncacheable)
  {
    if (!(join_tab_save= (JOIN_TAB*)thd->memdup((gptr) join_tab,
						sizeof(JOIN_TAB) * tables)))
      return 1;
  }
  return 0;
}


/*
  Exec select
*/
void
JOIN::exec()
{
  int      tmp_error;
  DBUG_ENTER("JOIN::exec");

  error= 0;
  if (procedure)
  {
    if (procedure->change_columns(fields_list) ||
	result->prepare(fields_list, unit))
    {
      thd->limit_found_rows= thd->examined_row_count= 0;
      DBUG_VOID_RETURN;
    }
  }
  (void) result->prepare2(); // Currently, this cannot fail.

  if (!tables_list)
  {                                           // Only test of functions
    if (select_options & SELECT_DESCRIBE)
      select_describe(this, FALSE, FALSE, FALSE,
		      (zero_result_cause?zero_result_cause:"No tables used"));
    else
    {
      result->send_fields(fields_list,
                          Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
      /*
        We have to test for 'conds' here as the WHERE may not be constant
        even if we don't have any tables for prepared statements or if
        conds uses something like 'rand()'.
      */
      if (cond_value != Item::COND_FALSE &&
          (!conds || conds->val_int()) &&
          (!having || having->val_int()))
      {
	if (do_send_rows && (procedure ? (procedure->send_row(fields_list) ||
                                          procedure->end_of_records())
                                       : result->send_data(fields_list)))
	  error= 1;
	else
	{
	  error= (int) result->send_eof();
	  send_records= ((select_options & OPTION_FOUND_ROWS) ? 1 :
                         thd->sent_row_count);
	}
      }
      else
      {
	error=(int) result->send_eof();
        send_records= 0;
      }
    }
    /* Single select (without union) always returns 0 or 1 row */
    thd->limit_found_rows= send_records;
    thd->examined_row_count= 0;
    DBUG_VOID_RETURN;
  }
  thd->limit_found_rows= thd->examined_row_count= 0;

  if (zero_result_cause)
  {
    (void) return_zero_rows(this, result, select_lex->leaf_tables, fields_list,
			    send_row_on_empty_set(),
			    select_options,
			    zero_result_cause,
			    having);
    DBUG_VOID_RETURN;
  }

  if (select_options & SELECT_DESCRIBE)
  {
    /*
      Check if we managed to optimize ORDER BY away and don't use temporary
      table to resolve ORDER BY: in that case, we only may need to do
      filesort for GROUP BY.
    */
    if (!order && !no_order && (!skip_sort_order || !need_tmp))
    {
      /*
	Reset 'order' to 'group_list' and reinit variables describing
	'order'
      */
      order= group_list;
      simple_order= simple_group;
      skip_sort_order= 0;
    }
    if (order &&
	(const_tables == tables ||
 	 ((simple_order || skip_sort_order) &&
	  test_if_skip_sort_order(&join_tab[const_tables], order,
				  select_limit, 0))))
      order=0;
    having= tmp_having;
    select_describe(this, need_tmp,
		    order != 0 && !skip_sort_order,
		    select_distinct);
    DBUG_VOID_RETURN;
  }

  JOIN *curr_join= this;
  List<Item> *curr_all_fields= &all_fields;
  List<Item> *curr_fields_list= &fields_list;
  TABLE *curr_tmp_table= 0;

  if ((curr_join->select_lex->options & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(curr_join))
  {
    DBUG_VOID_RETURN;
  }

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    if (tmp_join)
    {
      /*
        We are in a non cacheable sub query. Get the saved join structure
        after optimization.
        (curr_join may have been modified during last exection and we need
        to reset it)
      */
      curr_join= tmp_join;
    }
    curr_tmp_table= exec_tmp_table1;

    /* Copy data to the temporary table */
    thd->proc_info= "Copying to tmp table";
    DBUG_PRINT("info", ("%s", thd->proc_info));
    if ((tmp_error= do_select(curr_join, (List<Item> *) 0, curr_tmp_table, 0)))
    {
      error= tmp_error;
      DBUG_VOID_RETURN;
    }
    curr_tmp_table->file->info(HA_STATUS_VARIABLE);
    
    if (curr_join->having)
      curr_join->having= curr_join->tmp_having= 0; // Allready done
    
    /* Change sum_fields reference to calculated fields in tmp_table */
    curr_join->all_fields= *curr_all_fields;
    if (!items1)
    {
      items1= items0 + all_fields.elements;
      if (sort_and_group || curr_tmp_table->group)
      {
	if (change_to_use_tmp_fields(thd, items1,
				     tmp_fields_list1, tmp_all_fields1,
				     fields_list.elements, all_fields))
	  DBUG_VOID_RETURN;
      }
      else
      {
	if (change_refs_to_tmp_fields(thd, items1,
				      tmp_fields_list1, tmp_all_fields1,
				      fields_list.elements, all_fields))
	  DBUG_VOID_RETURN;
      }
      curr_join->tmp_all_fields1= tmp_all_fields1;
      curr_join->tmp_fields_list1= tmp_fields_list1;
      curr_join->items1= items1;
    }
    curr_all_fields= &tmp_all_fields1;
    curr_fields_list= &tmp_fields_list1;
    curr_join->set_items_ref_array(items1);
    
    if (sort_and_group || curr_tmp_table->group)
    {
      curr_join->tmp_table_param.field_count+= 
	curr_join->tmp_table_param.sum_func_count+
	curr_join->tmp_table_param.func_count;
      curr_join->tmp_table_param.sum_func_count= 
	curr_join->tmp_table_param.func_count= 0;
    }
    else
    {
      curr_join->tmp_table_param.field_count+= 
	curr_join->tmp_table_param.func_count;
      curr_join->tmp_table_param.func_count= 0;
    }
    
    // procedure can't be used inside subselect => we do nothing special for it
    if (procedure)
      procedure->update_refs();
    
    if (curr_tmp_table->group)
    {						// Already grouped
      if (!curr_join->order && !curr_join->no_order && !skip_sort_order)
	curr_join->order= curr_join->group_list;  /* order by group */
      curr_join->group_list= 0;
    }
    
    /*
      If we have different sort & group then we must sort the data by group
      and copy it to another tmp table
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)).
    */

    if (curr_join->group_list && (!test_if_subpart(curr_join->group_list,
						   curr_join->order) || 
				  curr_join->select_distinct) ||
	(curr_join->select_distinct &&
	 curr_join->tmp_table_param.using_indirect_summary_function))
    {					/* Must copy to another table */
      DBUG_PRINT("info",("Creating group table"));
      
      /* Free first data from old join */
      curr_join->join_free(0);
      if (make_simple_join(curr_join, curr_tmp_table))
	DBUG_VOID_RETURN;
      calc_group_buffer(curr_join, group_list);
      count_field_types(&curr_join->tmp_table_param,
			curr_join->tmp_all_fields1,
			curr_join->select_distinct && !curr_join->group_list);
      curr_join->tmp_table_param.hidden_field_count= 
	(curr_join->tmp_all_fields1.elements-
	 curr_join->tmp_fields_list1.elements);
      
      
      if (exec_tmp_table2)
	curr_tmp_table= exec_tmp_table2;
      else
      {
	/* group data to new table */
	if (!(curr_tmp_table=
	      exec_tmp_table2= create_tmp_table(thd,
						&curr_join->tmp_table_param,
						*curr_all_fields,
						(ORDER*) 0,
						curr_join->select_distinct && 
						!curr_join->group_list,
						1, curr_join->select_options,
						HA_POS_ERROR,
						(char *) "")))
	  DBUG_VOID_RETURN;
	curr_join->exec_tmp_table2= exec_tmp_table2;
      }
      if (curr_join->group_list)
      {
	thd->proc_info= "Creating sort index";
	if (curr_join->join_tab == join_tab && save_join_tab())
	{
	  DBUG_VOID_RETURN;
	}
	if (create_sort_index(thd, curr_join, curr_join->group_list,
			      HA_POS_ERROR, HA_POS_ERROR) ||
	    make_group_fields(this, curr_join))
	{
	  DBUG_VOID_RETURN;
	}
      }
      
      thd->proc_info="Copying to group table";
      DBUG_PRINT("info", ("%s", thd->proc_info));
      tmp_error= -1;
      if (curr_join != this)
      {
	if (sum_funcs2)
	{
	  curr_join->sum_funcs= sum_funcs2;
	  curr_join->sum_funcs_end= sum_funcs_end2; 
	}
	else
	{
	  curr_join->alloc_func_list();
	  sum_funcs2= curr_join->sum_funcs;
	  sum_funcs_end2= curr_join->sum_funcs_end;
	}
      }
      if (curr_join->make_sum_func_list(*curr_all_fields, *curr_fields_list,
					1, TRUE))
        DBUG_VOID_RETURN;
      curr_join->group_list= 0;
      if (setup_sum_funcs(curr_join->thd, curr_join->sum_funcs) ||
	  (tmp_error= do_select(curr_join, (List<Item> *) 0, curr_tmp_table,
				0)))
      {
	error= tmp_error;
	DBUG_VOID_RETURN;
      }
      end_read_record(&curr_join->join_tab->read_record);
      curr_join->const_tables= curr_join->tables; // Mark free for cleanup()
      curr_join->join_tab[0].table= 0;           // Table is freed
      
      // No sum funcs anymore
      if (!items2)
      {
	items2= items1 + all_fields.elements;
	if (change_to_use_tmp_fields(thd, items2,
				     tmp_fields_list2, tmp_all_fields2, 
				     fields_list.elements, tmp_all_fields1))
	  DBUG_VOID_RETURN;
	curr_join->tmp_fields_list2= tmp_fields_list2;
	curr_join->tmp_all_fields2= tmp_all_fields2;
      }
      curr_fields_list= &curr_join->tmp_fields_list2;
      curr_all_fields= &curr_join->tmp_all_fields2;
      curr_join->set_items_ref_array(items2);
      curr_join->tmp_table_param.field_count+= 
	curr_join->tmp_table_param.sum_func_count;
      curr_join->tmp_table_param.sum_func_count= 0;
    }
    if (curr_tmp_table->distinct)
      curr_join->select_distinct=0;		/* Each row is unique */
    
    curr_join->join_free(0);			/* Free quick selects */
    if (curr_join->select_distinct && ! curr_join->group_list)
    {
      thd->proc_info="Removing duplicates";
      if (curr_join->tmp_having)
	curr_join->tmp_having->update_used_tables();
      if (remove_duplicates(curr_join, curr_tmp_table,
			    *curr_fields_list, curr_join->tmp_having))
	DBUG_VOID_RETURN;
      curr_join->tmp_having=0;
      curr_join->select_distinct=0;
    }
    curr_tmp_table->reginfo.lock_type= TL_UNLOCK;
    if (make_simple_join(curr_join, curr_tmp_table))
      DBUG_VOID_RETURN;
    calc_group_buffer(curr_join, curr_join->group_list);
    count_field_types(&curr_join->tmp_table_param, *curr_all_fields, 0);
    
  }
  if (procedure)
    count_field_types(&curr_join->tmp_table_param, *curr_all_fields, 0);
  
  if (curr_join->group || curr_join->tmp_table_param.sum_func_count ||
      (procedure && (procedure->flags & PROC_GROUP)))
  {
    if (make_group_fields(this, curr_join))
    {
      DBUG_VOID_RETURN;
    }
    if (!items3)
    {
      if (!items0)
	init_items_ref_array();
      items3= ref_pointer_array + (all_fields.elements*4);
      setup_copy_fields(thd, &curr_join->tmp_table_param,
			items3, tmp_fields_list3, tmp_all_fields3,
			curr_fields_list->elements, *curr_all_fields);
      tmp_table_param.save_copy_funcs= curr_join->tmp_table_param.copy_funcs;
      tmp_table_param.save_copy_field= curr_join->tmp_table_param.copy_field;
      tmp_table_param.save_copy_field_end=
	curr_join->tmp_table_param.copy_field_end;
      curr_join->tmp_all_fields3= tmp_all_fields3;
      curr_join->tmp_fields_list3= tmp_fields_list3;
    }
    else
    {
      curr_join->tmp_table_param.copy_funcs= tmp_table_param.save_copy_funcs;
      curr_join->tmp_table_param.copy_field= tmp_table_param.save_copy_field;
      curr_join->tmp_table_param.copy_field_end=
	tmp_table_param.save_copy_field_end;
    }
    curr_fields_list= &tmp_fields_list3;
    curr_all_fields= &tmp_all_fields3;
    curr_join->set_items_ref_array(items3);

    if (curr_join->make_sum_func_list(*curr_all_fields, *curr_fields_list,
				      1, TRUE) || 
        setup_sum_funcs(curr_join->thd, curr_join->sum_funcs) ||
        thd->is_fatal_error)
      DBUG_VOID_RETURN;
  }
  if (curr_join->group_list || curr_join->order)
  {
    DBUG_PRINT("info",("Sorting for send_fields"));
    thd->proc_info="Sorting result";
    /* If we have already done the group, add HAVING to sorted table */
    if (curr_join->tmp_having && ! curr_join->group_list && 
	! curr_join->sort_and_group)
    {
      // Some tables may have been const
      curr_join->tmp_having->update_used_tables();
      JOIN_TAB *curr_table= &curr_join->join_tab[curr_join->const_tables];
      table_map used_tables= (curr_join->const_table_map |
			      curr_table->table->map);

      Item* sort_table_cond= make_cond_for_table(curr_join->tmp_having,
						 used_tables,
						 used_tables);
      if (sort_table_cond)
      {
	if (!curr_table->select)
	  if (!(curr_table->select= new SQL_SELECT))
	    DBUG_VOID_RETURN;
	if (!curr_table->select->cond)
	  curr_table->select->cond= sort_table_cond;
	else					// This should never happen
	{
	  if (!(curr_table->select->cond=
		new Item_cond_and(curr_table->select->cond,
				  sort_table_cond)))
	    DBUG_VOID_RETURN;
	  /*
	    Item_cond_and do not need fix_fields for execution, its parameters
	    are fixed or do not need fix_fields, too
	  */
	  curr_table->select->cond->quick_fix_field();
	}
	curr_table->select_cond= curr_table->select->cond;
	curr_table->select_cond->top_level_item();
	DBUG_EXECUTE("where",print_where(curr_table->select->cond,
					 "select and having"););
	curr_join->tmp_having= make_cond_for_table(curr_join->tmp_having,
						   ~ (table_map) 0,
						   ~used_tables);
	DBUG_EXECUTE("where",print_where(curr_join->tmp_having,
                                         "having after sort"););
      }
    }
    {
      if (group)
	curr_join->select_limit= HA_POS_ERROR;
      else
      {
	/*
	  We can abort sorting after thd->select_limit rows if we there is no
	  WHERE clause for any tables after the sorted one.
	*/
	JOIN_TAB *curr_table= &curr_join->join_tab[curr_join->const_tables+1];
	JOIN_TAB *end_table= &curr_join->join_tab[curr_join->tables];
	for (; curr_table < end_table ; curr_table++)
	{
	  /*
	    table->keyuse is set in the case there was an original WHERE clause
	    on the table that was optimized away.
	  */
	  if (curr_table->select_cond ||
	      (curr_table->keyuse && !curr_table->first_inner))
	  {
	    /* We have to sort all rows */
	    curr_join->select_limit= HA_POS_ERROR;
	    break;
	  }
	}
      }
      if (curr_join->join_tab == join_tab && save_join_tab())
      {
	DBUG_VOID_RETURN;
      }
      /*
	Here we sort rows for ORDER BY/GROUP BY clause, if the optimiser
	chose FILESORT to be faster than INDEX SCAN or there is no 
	suitable index present.
	Note, that create_sort_index calls test_if_skip_sort_order and may
	finally replace sorting with index scan if there is a LIMIT clause in
	the query. XXX: it's never shown in EXPLAIN!
	OPTION_FOUND_ROWS supersedes LIMIT and is taken into account.
      */
      if (create_sort_index(thd, curr_join,
			    curr_join->group_list ? 
			    curr_join->group_list : curr_join->order,
			    curr_join->select_limit,
			    (select_options & OPTION_FOUND_ROWS ?
			     HA_POS_ERROR : unit->select_limit_cnt)))
	DBUG_VOID_RETURN;
    }
  }
  /* XXX: When can we have here thd->net.report_error not zero? */
  if (thd->net.report_error)
  {
    error= thd->net.report_error;
    DBUG_VOID_RETURN;
  }
  curr_join->having= curr_join->tmp_having;
  curr_join->fields= curr_fields_list;
  curr_join->procedure= procedure;

  if (is_top_level_join() && thd->cursor && tables != const_tables)
  {
    /*
      We are here if this is JOIN::exec for the last select of the main unit
      and the client requested to open a cursor.
      We check that not all tables are constant because this case is not
      handled by do_select() separately, and this case is not implemented
      for cursors yet.
    */
    DBUG_ASSERT(error == 0);
    /*
      curr_join is used only for reusable joins - that is, 
      to perform SELECT for each outer row (like in subselects).
      This join is main, so we know for sure that curr_join == join.
    */
    DBUG_ASSERT(curr_join == this);
    /* Open cursor for the last join sweep */
    error= thd->cursor->open(this);
  }
  else
  {
    thd->proc_info="Sending data";
    DBUG_PRINT("info", ("%s", thd->proc_info));
    result->send_fields(*curr_fields_list,
                        Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
    error= do_select(curr_join, curr_fields_list, NULL, procedure);
    thd->limit_found_rows= curr_join->send_records;
    thd->examined_row_count= curr_join->examined_rows;
  }

  DBUG_VOID_RETURN;
}


/*
  Clean up join. Return error that hold JOIN.
*/

int
JOIN::destroy()
{
  DBUG_ENTER("JOIN::destroy");
  select_lex->join= 0;

  if (tmp_join)
  {
    if (join_tab != tmp_join->join_tab)
    {
      JOIN_TAB *tab, *end;
      for (tab= join_tab, end= tab+tables ; tab != end ; tab++)
      {
	tab->cleanup();
      }
    }
    tmp_join->tmp_join= 0;
    tmp_table_param.copy_field=0;
    DBUG_RETURN(tmp_join->destroy());
  }
  cond_equal= 0;

  cleanup(1);
  if (exec_tmp_table1)
    free_tmp_table(thd, exec_tmp_table1);
  if (exec_tmp_table2)
    free_tmp_table(thd, exec_tmp_table2);
  delete select;
  delete_dynamic(&keyuse);
  delete procedure;
  DBUG_RETURN(error);
}


/************************* Cursor ******************************************/

Cursor::Cursor(THD *thd)
  :Query_arena(&main_mem_root, INITIALIZED),
   join(0), unit(0),
   protocol(thd),
   close_at_commit(FALSE)
{
  /* We will overwrite it at open anyway. */
  init_sql_alloc(&main_mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
  thr_lock_owner_init(&lock_id, &thd->lock_info);
  bzero((void*) ht_info, sizeof(ht_info));
}


void
Cursor::init_from_thd(THD *thd)
{
  Engine_info *info;
  /*
    We need to save and reset thd->mem_root, otherwise it'll be freed
    later in mysql_parse.

    We can't just change the thd->mem_root here as we want to keep the
    things that are already allocated in thd->mem_root for Cursor::fetch()
  */
  main_mem_root=  *thd->mem_root;
  state= thd->stmt_arena->state;
  /* Allocate new memory root for thd */
  init_sql_alloc(thd->mem_root,
                 thd->variables.query_alloc_block_size,
                 thd->variables.query_prealloc_size);

  /*
    The same is true for open tables and lock: save tables and zero THD
    pointers to prevent table close in close_thread_tables (This is a part
    of the temporary solution to make cursors work with minimal changes to
    the current source base).
  */
  derived_tables= thd->derived_tables;
  open_tables=    thd->open_tables;
  lock=           thd->lock;
  query_id=       thd->query_id;
  free_list=	  thd->free_list;
  change_list=    thd->change_list;
  reset_thd(thd);
  /* Now we have an active cursor and can cause a deadlock */
  thd->lock_info.n_cursors++;

  close_at_commit= FALSE; /* reset in case we're reusing the cursor */
  info= &ht_info[0];
  for (handlerton **pht= thd->transaction.stmt.ht; *pht; pht++)
  {
    const handlerton *ht= *pht;
    close_at_commit|= test(ht->flags & HTON_CLOSE_CURSORS_AT_COMMIT);
    if (ht->create_cursor_read_view)
    {
      info->ht= ht;
      info->read_view= (ht->create_cursor_read_view)();
      ++info;
    }
  }
  /*
    XXX: thd->locked_tables is not changed.
    What problems can we have with it if cursor is open?
    TODO: must be fixed because of the prelocked mode.
  */
}


void
Cursor::reset_thd(THD *thd)
{
  thd->derived_tables= 0;
  thd->open_tables= 0;
  thd->lock= 0;
  thd->free_list= 0;
  thd->change_list.empty();
}


int
Cursor::open(JOIN *join_arg)
{
  join= join_arg;
  THD *thd= join->thd;
  /* First non-constant table */
  JOIN_TAB *join_tab= join->join_tab + join->const_tables;
  DBUG_ENTER("Cursor::open");

  /*
    Send fields description to the client; server_status is sent
    in 'EOF' packet, which ends send_fields().
  */
  thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
  join->result->send_fields(*join->fields, Protocol::SEND_NUM_ROWS);
  ::send_eof(thd);
  thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;

  /* Prepare JOIN for reading rows. */
  join->tmp_table= 0;
  join->join_tab[join->tables-1].next_select= setup_end_select_func(join);
  join->send_records= 0;
  join->fetch_limit= join->unit->offset_limit_cnt;

  /* Disable JOIN CACHE as it is not working with cursors yet */
  for (JOIN_TAB *tab= join_tab;
       tab != join->join_tab + join->tables - 1;
       tab++)
  {
    if (tab->next_select == sub_select_cache)
      tab->next_select= sub_select;
  }

  DBUG_ASSERT(join_tab->table->reginfo.not_exists_optimize == 0);
  DBUG_ASSERT(join_tab->not_used_in_distinct == 0);
  /*
    null_row is set only if row not found and it's outer join: should never
    happen for the first table in join_tab list
  */
  DBUG_ASSERT(join_tab->table->null_row == 0);
  DBUG_RETURN(0);
}


/*
  DESCRIPTION
    Fetch next num_rows rows from the cursor and sent them to the client
  PRECONDITION:
    Cursor is open
  RETURN VALUES:
    none, this function will send error or OK to network if necessary.
*/

void
Cursor::fetch(ulong num_rows)
{
  THD *thd= join->thd;
  JOIN_TAB *join_tab= join->join_tab + join->const_tables;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  Query_arena backup_arena;
  Engine_info *info;
  DBUG_ENTER("Cursor::fetch");
  DBUG_PRINT("enter",("rows: %lu", num_rows));

  DBUG_ASSERT(thd->derived_tables == 0 && thd->open_tables == 0 &&
              thd->lock == 0);

  thd->derived_tables= derived_tables;
  thd->open_tables= open_tables;
  thd->lock= lock;
  thd->query_id= query_id;
  thd->change_list= change_list;
  /* save references to memory, allocated during fetch */
  thd->set_n_backup_active_arena(this, &backup_arena);

  for (info= ht_info; info->read_view ; info++)
    (info->ht->set_cursor_read_view)(info->read_view);

  join->fetch_limit+= num_rows;

  error= sub_select(join, join_tab, 0);
  if (error == NESTED_LOOP_OK || error == NESTED_LOOP_NO_MORE_ROWS)
    error= sub_select(join,join_tab,1);
  if (error == NESTED_LOOP_QUERY_LIMIT)
    error= NESTED_LOOP_OK;                    /* select_limit used */
  if (error == NESTED_LOOP_CURSOR_LIMIT)
    join->resume_nested_loop= TRUE;

#ifdef USING_TRANSACTIONS
    ha_release_temporary_latches(thd);
#endif
  /* Grab free_list here to correctly free it in close */
  thd->restore_active_arena(this, &backup_arena);

  for (info= ht_info; info->read_view; info++)
    (info->ht->set_cursor_read_view)(0);

  if (error == NESTED_LOOP_CURSOR_LIMIT)
  {
    /* Fetch limit worked, possibly more rows are there */
    thd->server_status|= SERVER_STATUS_CURSOR_EXISTS;
    ::send_eof(thd);
    thd->server_status&= ~SERVER_STATUS_CURSOR_EXISTS;
    change_list= thd->change_list;
    reset_thd(thd);
  }
  else
  {
    close(TRUE);
    if (error == NESTED_LOOP_OK)
    {
      thd->server_status|= SERVER_STATUS_LAST_ROW_SENT;
      ::send_eof(thd);
      thd->server_status&= ~SERVER_STATUS_LAST_ROW_SENT;
    }
    else if (error != NESTED_LOOP_KILLED)
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
  }
  DBUG_VOID_RETURN;
}


void
Cursor::close(bool is_active)
{
  THD *thd= join->thd;
  DBUG_ENTER("Cursor::close");

  /*
    In case of UNIONs JOIN is freed inside of unit->cleanup(),
    otherwise in select_lex->cleanup().
  */
  if (unit)
    (void) unit->cleanup();
  else
    (void) join->select_lex->cleanup();

  for (Engine_info *info= ht_info; info->read_view; info++)
  {
    (info->ht->close_cursor_read_view)(info->read_view);
    info->read_view= 0;
    info->ht= 0;
  }

  if (is_active)
    close_thread_tables(thd);
  else
  {
    /* XXX: Another hack: closing tables used in the cursor */
    DBUG_ASSERT(lock || open_tables || derived_tables);

    TABLE *tmp_derived_tables= thd->derived_tables;
    MYSQL_LOCK *tmp_lock= thd->lock;

    thd->open_tables= open_tables;
    thd->derived_tables= derived_tables;
    thd->lock= lock;
    close_thread_tables(thd);

    thd->open_tables= tmp_derived_tables;
    thd->derived_tables= tmp_derived_tables;
    thd->lock= tmp_lock;
  }
  thd->lock_info.n_cursors--; /* Decrease the number of active cursors */
  join= 0;
  unit= 0;
  free_items();
  change_list.empty();
  DBUG_VOID_RETURN;
}


/*********************************************************************/

/*
  An entry point to single-unit select (a select without UNION).

  SYNOPSIS
    mysql_select()

    thd                  thread handler
    rref_pointer_array   a reference to ref_pointer_array of
                         the top-level select_lex for this query
    tables               list of all tables used in this query.
                         The tables have been pre-opened.
    wild_num             number of wildcards used in the top level 
                         select of this query.
                         For example statement
                         SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
                         has 3 wildcards.
    fields               list of items in SELECT list of the top-level
                         select
                         e.g. SELECT a, b, c FROM t1 will have Item_field
                         for a, b and c in this list.
    conds                top level item of an expression representing
                         WHERE clause of the top level select
    og_num               total number of ORDER BY and GROUP BY clauses
                         arguments
    order                linked list of ORDER BY agruments
    group                linked list of GROUP BY arguments
    having               top level item of HAVING expression
    proc_param           list of PROCEDUREs
    select_options       select options (BIG_RESULT, etc)
    result               an instance of result set handling class.
                         This object is responsible for send result
                         set rows to the client or inserting them
                         into a table.
    select_lex           the only SELECT_LEX of this query
    unit                 top-level UNIT of this query
                         UNIT is an artificial object created by the parser
                         for every SELECT clause.
                         e.g. SELECT * FROM t1 WHERE a1 IN (SELECT * FROM t2)
                         has 2 unions.

  RETURN VALUE
   FALSE  success
   TRUE   an error
*/

bool
mysql_select(THD *thd, Item ***rref_pointer_array,
	     TABLE_LIST *tables, uint wild_num, List<Item> &fields,
	     COND *conds, uint og_num,  ORDER *order, ORDER *group,
	     Item *having, ORDER *proc_param, ulong select_options,
	     select_result *result, SELECT_LEX_UNIT *unit,
	     SELECT_LEX *select_lex)
{
  bool err;
  bool free_join= 1;
  DBUG_ENTER("mysql_select");

  select_lex->context.resolve_in_select_list= TRUE;
  JOIN *join;
  if (select_lex->join != 0)
  {
    join= select_lex->join;
    /*
      is it single SELECT in derived table, called in derived table
      creation
    */
    if (select_lex->linkage != DERIVED_TABLE_TYPE ||
	(select_options & SELECT_DESCRIBE))
    {
      if (select_lex->linkage != GLOBAL_OPTIONS_TYPE)
      {
	//here is EXPLAIN of subselect or derived table
	if (join->change_result(result))
	{
	  DBUG_RETURN(TRUE);
	}
      }
      else
      {
	if (join->prepare(rref_pointer_array, tables, wild_num,
			  conds, og_num, order, group, having, proc_param,
			  select_lex, unit))
	{
	  goto err;
	}
      }
    }
    free_join= 0;
    join->select_options= select_options;
  }
  else
  {
    if (!(join= new JOIN(thd, fields, select_options, result)))
	DBUG_RETURN(TRUE);
    thd->proc_info="init";
    thd->used_tables=0;                         // Updated by setup_fields
    if (join->prepare(rref_pointer_array, tables, wild_num,
		      conds, og_num, order, group, having, proc_param,
		      select_lex, unit))
    {
      goto err;
    }
  }

  if ((err= join->optimize()))
  {
    goto err;					// 1
  }

  if (thd->lex->describe & DESCRIBE_EXTENDED)
  {
    join->conds_history= join->conds;
    join->having_history= (join->having?join->having:join->tmp_having);
  }

  if (thd->net.report_error)
    goto err;

  join->exec();

  if (thd->cursor && thd->cursor->is_open())
  {
    /*
      A cursor was opened for the last sweep in exec().
      We are here only if this is mysql_select for top-level SELECT_LEX_UNIT
      and there were no error.
    */
    free_join= 0;
  }

  if (thd->lex->describe & DESCRIBE_EXTENDED)
  {
    select_lex->where= join->conds_history;
    select_lex->having= join->having_history;
  }

err:
  if (free_join)
  {
    thd->proc_info="end";
    err= select_lex->cleanup();
    DBUG_RETURN(err || thd->net.report_error);
  }
  DBUG_RETURN(join->error);
}

/*****************************************************************************
  Create JOIN_TABS, make a guess about the table types,
  Approximate how many records will be used in each table
*****************************************************************************/

static ha_rows get_quick_record_count(THD *thd, SQL_SELECT *select,
				      TABLE *table,
				      const key_map *keys,ha_rows limit)
{
  int error;
  DBUG_ENTER("get_quick_record_count");
  if (select)
  {
    select->head=table;
    table->reginfo.impossible_range=0;
    if ((error= select->test_quick_select(thd, *(key_map *)keys,(table_map) 0,
                                          limit, 0)) == 1)
      DBUG_RETURN(select->quick->records);
    if (error == -1)
    {
      table->reginfo.impossible_range=1;
      DBUG_RETURN(0);
    }
    DBUG_PRINT("warning",("Couldn't use record count on const keypart"));
  }
  DBUG_RETURN(HA_POS_ERROR);			/* This shouldn't happend */
}


/*
  Calculate the best possible join and initialize the join structure

  RETURN VALUES
  0	ok
  1	Fatal error
*/

static bool
make_join_statistics(JOIN *join, TABLE_LIST *tables, COND *conds,
		     DYNAMIC_ARRAY *keyuse_array)
{
  int error;
  TABLE *table;
  uint i,table_count,const_count,key;
  table_map found_const_table_map, all_table_map, found_ref, refs;
  key_map const_ref, eq_part;
  TABLE **table_vector;
  JOIN_TAB *stat,*stat_end,*s,**stat_ref;
  KEYUSE *keyuse,*start_keyuse;
  table_map outer_join=0;
  JOIN_TAB *stat_vector[MAX_TABLES+1];
  DBUG_ENTER("make_join_statistics");

  table_count=join->tables;
  stat=(JOIN_TAB*) join->thd->calloc(sizeof(JOIN_TAB)*table_count);
  stat_ref=(JOIN_TAB**) join->thd->alloc(sizeof(JOIN_TAB*)*MAX_TABLES);
  table_vector=(TABLE**) join->thd->alloc(sizeof(TABLE*)*(table_count*2));
  if (!stat || !stat_ref || !table_vector)
    DBUG_RETURN(1);				// Eom /* purecov: inspected */

  join->best_ref=stat_vector;

  stat_end=stat+table_count;
  found_const_table_map= all_table_map=0;
  const_count=0;

  for (s= stat, i= 0;
       tables;
       s++, tables= tables->next_leaf, i++)
  {
    TABLE_LIST *embedding= tables->embedding;
    stat_vector[i]=s;
    s->keys.init();
    s->const_keys.init();
    s->checked_keys.init();
    s->needed_reg.init();
    table_vector[i]=s->table=table=tables->table;
    table->pos_in_table_list= tables;
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);// record count
    table->quick_keys.clear_all();
    table->reginfo.join_tab=s;
    table->reginfo.not_exists_optimize=0;
    bzero((char*) table->const_key_parts, sizeof(key_part_map)*table->s->keys);
    all_table_map|= table->map;
    s->join=join;
    s->info=0;					// For describe

    s->dependent= tables->dep_tables;
    s->key_dependent= 0;
    if (tables->schema_table)
      table->file->records= 2;

    s->on_expr_ref= &tables->on_expr;
    if (*s->on_expr_ref)
    {
      /* s is the only inner table of an outer join */
      if (!table->file->records && !embedding)
      {						// Empty table
        s->dependent= 0;                        // Ignore LEFT JOIN depend.
	set_position(join,const_count++,s,(KEYUSE*) 0);
	continue;
      }
      outer_join|= table->map;
      continue;
    }
    if (embedding)
    {
      /* s belongs to a nested join, maybe to several embedded joins */
      do
      {
        NESTED_JOIN *nested_join= embedding->nested_join;
        s->dependent|= embedding->dep_tables;
        embedding= embedding->embedding;
        outer_join|= nested_join->used_tables;
      }
      while (embedding);
      continue;
    }

    if ((table->s->system || table->file->records <= 1) && ! s->dependent &&
	!(table->file->table_flags() & HA_NOT_EXACT_COUNT) &&
        !table->fulltext_searched)
    {
      set_position(join,const_count++,s,(KEYUSE*) 0);
    }
  }
  stat_vector[i]=0;
  join->outer_join=outer_join;

  if (join->outer_join)
  {
    /* 
       Build transitive closure for relation 'to be dependent on'.
       This will speed up the plan search for many cases with outer joins,
       as well as allow us to catch illegal cross references/
       Warshall's algorithm is used to build the transitive closure.
       As we use bitmaps to represent the relation the complexity
       of the algorithm is O((number of tables)^2). 
    */
    for (i= 0, s= stat ; i < table_count ; i++, s++)
    {
      for (uint j= 0 ; j < table_count ; j++)
      {
        table= stat[j].table;
        if (s->dependent & table->map)
          s->dependent |= table->reginfo.join_tab->dependent;
      }
      if (s->dependent)
        s->table->maybe_null= 1;
    }
    /* Catch illegal cross references for outer joins */
    for (i= 0, s= stat ; i < table_count ; i++, s++)
    {
      if (s->dependent & s->table->map)
      {
        join->tables=0;			// Don't use join->table
        my_message(ER_WRONG_OUTER_JOIN, ER(ER_WRONG_OUTER_JOIN), MYF(0));
        DBUG_RETURN(1);
      }
      s->key_dependent= s->dependent;
    }
  }

  if (conds || outer_join)
    if (update_ref_and_keys(join->thd, keyuse_array, stat, join->tables,
                            conds, join->cond_equal,
                            ~outer_join, join->select_lex))
      DBUG_RETURN(1);

  /* Read tables with 0 or 1 rows (system tables) */
  join->const_table_map= 0;

  for (POSITION *p_pos=join->positions, *p_end=p_pos+const_count;
       p_pos < p_end ;
       p_pos++)
  {
    int tmp;
    s= p_pos->table;
    s->type=JT_SYSTEM;
    join->const_table_map|=s->table->map;
    if ((tmp=join_read_const_table(s, p_pos)))
    {
      if (tmp > 0)
	DBUG_RETURN(1);			// Fatal error
    }
    else
      found_const_table_map|= s->table->map;
  }

  /* loop until no more const tables are found */
  int ref_changed;
  do
  {
    ref_changed = 0;
    found_ref=0;

    /*
      We only have to loop from stat_vector + const_count as
      set_position() will move all const_tables first in stat_vector
    */

    for (JOIN_TAB **pos=stat_vector+const_count ; (s= *pos) ; pos++)
    {
      table=s->table;
      if (s->dependent)				// If dependent on some table
      {
	// All dep. must be constants
	if (s->dependent & ~(found_const_table_map))
	  continue;
	if (table->file->records <= 1L &&
	    !(table->file->table_flags() & HA_NOT_EXACT_COUNT) &&
            !table->pos_in_table_list->embedding)
	{					// system table
	  int tmp= 0;
	  s->type=JT_SYSTEM;
	  join->const_table_map|=table->map;
	  set_position(join,const_count++,s,(KEYUSE*) 0);
	  if ((tmp= join_read_const_table(s,join->positions+const_count-1)))
	  {
	    if (tmp > 0)
	      DBUG_RETURN(1);			// Fatal error
	  }
	  else
	    found_const_table_map|= table->map;
	  continue;
	}
      }
      /* check if table can be read by key or table only uses const refs */
      if ((keyuse=s->keyuse))
      {
	s->type= JT_REF;
	while (keyuse->table == table)
	{
	  start_keyuse=keyuse;
	  key=keyuse->key;
	  s->keys.set_bit(key);               // QQ: remove this ?

	  refs=0;
          const_ref.clear_all();
	  eq_part.clear_all();
	  do
	  {
	    if (keyuse->val->type() != Item::NULL_ITEM && !keyuse->optimize)
	    {
	      if (!((~found_const_table_map) & keyuse->used_tables))
		const_ref.set_bit(keyuse->keypart);
	      else
		refs|=keyuse->used_tables;
	      eq_part.set_bit(keyuse->keypart);
	    }
	    keyuse++;
	  } while (keyuse->table == table && keyuse->key == key);

	  if (eq_part.is_prefix(table->key_info[key].key_parts) &&
	      ((table->key_info[key].flags & (HA_NOSAME | HA_END_SPACE_KEY)) ==
	       HA_NOSAME) &&
              !table->fulltext_searched)
	  {
	    if (const_ref == eq_part)
	    {					// Found everything for ref.
	      int tmp;
	      ref_changed = 1;
	      s->type= JT_CONST;
	      join->const_table_map|=table->map;
	      set_position(join,const_count++,s,start_keyuse);
	      if (create_ref_for_key(join, s, start_keyuse,
				     found_const_table_map))
		DBUG_RETURN(1);
	      if ((tmp=join_read_const_table(s,
					     join->positions+const_count-1)))
	      {
		if (tmp > 0)
		  DBUG_RETURN(1);			// Fatal error
	      }
	      else
		found_const_table_map|= table->map;
	      break;
	    }
	    else
	      found_ref|= refs;		// Table is const if all refs are const
	  }
	}
      }
    }
  } while (join->const_table_map & found_ref && ref_changed);

  /* Calc how many (possible) matched records in each table */

  for (s=stat ; s < stat_end ; s++)
  {
    if (s->type == JT_SYSTEM || s->type == JT_CONST)
    {
      /* Only one matching row */
      s->found_records=s->records=s->read_time=1; s->worst_seeks=1.0;
      continue;
    }
    /* Approximate found rows and time to read them */
    s->found_records=s->records=s->table->file->records;
    s->read_time=(ha_rows) s->table->file->scan_time();

    /*
      Set a max range of how many seeks we can expect when using keys
      This is can't be to high as otherwise we are likely to use
      table scan.
    */
    s->worst_seeks= min((double) s->found_records / 10,
			(double) s->read_time*3);
    if (s->worst_seeks < 2.0)			// Fix for small tables
      s->worst_seeks=2.0;

    /*
      Add to stat->const_keys those indexes for which all group fields or
      all select distinct fields participate in one index.
    */
    add_group_and_distinct_keys(join, s);

    if (!s->const_keys.is_clear_all() &&
        !s->table->pos_in_table_list->embedding)
    {
      ha_rows records;
      SQL_SELECT *select;
      select= make_select(s->table, found_const_table_map,
			  found_const_table_map,
			  *s->on_expr_ref ? *s->on_expr_ref : conds,
			  1, &error);
      if (!select)
        DBUG_RETURN(1);
      records= get_quick_record_count(join->thd, select, s->table,
				      &s->const_keys, join->row_limit);
      s->quick=select->quick;
      s->needed_reg=select->needed_reg;
      select->quick=0;
      if (records == 0 && s->table->reginfo.impossible_range)
      {
	/*
	  Impossible WHERE or ON expression
	  In case of ON, we mark that the we match one empty NULL row.
	  In case of WHERE, don't set found_const_table_map to get the
	  caller to abort with a zero row result.
	*/
	join->const_table_map|= s->table->map;
	set_position(join,const_count++,s,(KEYUSE*) 0);
	s->type= JT_CONST;
	if (*s->on_expr_ref)
	{
	  /* Generate empty row */
	  s->info= "Impossible ON condition";
	  found_const_table_map|= s->table->map;
	  s->type= JT_CONST;
	  mark_as_null_row(s->table);		// All fields are NULL
	}
      }
      if (records != HA_POS_ERROR)
      {
	s->found_records=records;
	s->read_time= (ha_rows) (s->quick ? s->quick->read_time : 0.0);
      }
      delete select;
    }
  }

  join->join_tab=stat;
  join->map2table=stat_ref;
  join->table= join->all_tables=table_vector;
  join->const_tables=const_count;
  join->found_const_table_map=found_const_table_map;

  /* Find an optimal join order of the non-constant tables. */
  if (join->const_tables != join->tables)
  {
    optimize_keyuse(join, keyuse_array);
    choose_plan(join, all_table_map & ~join->const_table_map);
  }
  else
  {
    memcpy((gptr) join->best_positions,(gptr) join->positions,
	   sizeof(POSITION)*join->const_tables);
    join->best_read=1.0;
  }
  /* Generate an execution plan from the found optimal join order. */
  DBUG_RETURN(join->thd->killed || get_best_combination(join));
}


/*****************************************************************************
  Check with keys are used and with tables references with tables
  Updates in stat:
	  keys	     Bitmap of all used keys
	  const_keys Bitmap of all keys with may be used with quick_select
	  keyuse     Pointer to possible keys
*****************************************************************************/

typedef struct key_field_t {		// Used when finding key fields
  Field		*field;
  Item		*val;			// May be empty if diff constant
  uint		level;
  uint		optimize;
  bool		eq_func;
  /*
    If true, the condition this struct represents will not be satisfied
    when val IS NULL.
  */
  bool          null_rejecting; 
} KEY_FIELD;

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

/*
  Merge new key definitions to old ones, remove those not used in both

  This is called for OR between different levels

  To be able to do 'ref_or_null' we merge a comparison of a column
  and 'column IS NULL' to one test.  This is useful for sub select queries
  that are internally transformed to something like:

  SELECT * FROM t1 WHERE t1.key=outer_ref_field or t1.key IS NULL 

  KEY_FIELD::null_rejecting is processed as follows:
  result has null_rejecting=true if it is set for both ORed references.
  for example:
    (t2.key = t1.field OR t2.key  =  t1.field) -> null_rejecting=true
    (t2.key = t1.field OR t2.key <=> t1.field) -> null_rejecting=false
*/

static KEY_FIELD *
merge_key_fields(KEY_FIELD *start,KEY_FIELD *new_fields,KEY_FIELD *end,
		 uint and_level)
{
  if (start == new_fields)
    return start;				// Impossible or
  if (new_fields == end)
    return start;				// No new fields, skip all

  KEY_FIELD *first_free=new_fields;

  /* Mark all found fields in old array */
  for (; new_fields != end ; new_fields++)
  {
    for (KEY_FIELD *old=start ; old != first_free ; old++)
    {
      if (old->field == new_fields->field)
      {
	if (new_fields->val->used_tables())
	{
	  /*
	    If the value matches, we can use the key reference.
	    If not, we keep it until we have examined all new values
	  */
	  if (old->val->eq(new_fields->val, old->field->binary()))
	  {
	    old->level= and_level;
	    old->optimize= ((old->optimize & new_fields->optimize &
			     KEY_OPTIMIZE_EXISTS) |
			    ((old->optimize | new_fields->optimize) &
			     KEY_OPTIMIZE_REF_OR_NULL));
            old->null_rejecting= (old->null_rejecting &&
                                  new_fields->null_rejecting);
	  }
	}
	else if (old->eq_func && new_fields->eq_func &&
		 old->val->eq(new_fields->val, old->field->binary()))

	{
	  old->level= and_level;
	  old->optimize= ((old->optimize & new_fields->optimize &
			   KEY_OPTIMIZE_EXISTS) |
			  ((old->optimize | new_fields->optimize) &
			   KEY_OPTIMIZE_REF_OR_NULL));
          old->null_rejecting= (old->null_rejecting &&
                                new_fields->null_rejecting);
	}
	else if (old->eq_func && new_fields->eq_func &&
		 (old->val->is_null() || new_fields->val->is_null()))
	{
	  /* field = expression OR field IS NULL */
	  old->level= and_level;
	  old->optimize= KEY_OPTIMIZE_REF_OR_NULL;
	  /* Remember the NOT NULL value */
	  if (old->val->is_null())
	    old->val= new_fields->val;
          /* The referred expression can be NULL: */ 
          old->null_rejecting= 0;
	}
	else
	{
	  /*
	    We are comparing two different const.  In this case we can't
	    use a key-lookup on this so it's better to remove the value
	    and let the range optimzier handle it
	  */
	  if (old == --first_free)		// If last item
	    break;
	  *old= *first_free;			// Remove old value
	  old--;				// Retry this value
	}
      }
    }
  }
  /* Remove all not used items */
  for (KEY_FIELD *old=start ; old != first_free ;)
  {
    if (old->level != and_level)
    {						// Not used in all levels
      if (old == --first_free)
	break;
      *old= *first_free;			// Remove old value
      continue;
    }
    old++;
  }
  return first_free;
}


/*
  Add a possible key to array of possible keys if it's usable as a key

  SYNPOSIS
    add_key_field()
    key_fields			Pointer to add key, if usable
    and_level			And level, to be stored in KEY_FIELD
    cond                        Condition predicate
    field			Field used in comparision
    eq_func			True if we used =, <=> or IS NULL
    value			Value used for comparison with field
                                Is NULL for BETWEEN and IN
    usable_tables		Tables which can be used for key optimization

  NOTES
    If we are doing a NOT NULL comparison on a NOT NULL field in a outer join
    table, we store this to be able to do not exists optimization later.

  RETURN
    *key_fields is incremented if we stored a key in the array
*/

static void
add_key_field(KEY_FIELD **key_fields,uint and_level, Item_func *cond,
	      Field *field, bool eq_func, Item **value, uint num_values,
	      table_map usable_tables)
{
  uint exists_optimize= 0;
  if (!(field->flags & PART_KEY_FLAG))
  {
    // Don't remove column IS NULL on a LEFT JOIN table
    if (!eq_func || (*value)->type() != Item::NULL_ITEM ||
        !field->table->maybe_null || field->null_ptr)
      return;					// Not a key. Skip it
    exists_optimize= KEY_OPTIMIZE_EXISTS;
  }
  else
  {
    table_map used_tables=0;
    bool optimizable=0;
    for (uint i=0; i<num_values; i++)
    {
      used_tables|=(value[i])->used_tables();
      if (!((value[i])->used_tables() & (field->table->map | RAND_TABLE_BIT)))
        optimizable=1;
    }
    if (!optimizable)
      return;
    if (!(usable_tables & field->table->map))
    {
      if (!eq_func || (*value)->type() != Item::NULL_ITEM ||
          !field->table->maybe_null || field->null_ptr)
	return;					// Can't use left join optimize
      exists_optimize= KEY_OPTIMIZE_EXISTS;
    }
    else
    {
      JOIN_TAB *stat=field->table->reginfo.join_tab;
      key_map possible_keys=field->key_start;
      possible_keys.intersect(field->table->keys_in_use_for_query);
      stat[0].keys.merge(possible_keys);             // Add possible keys

      /*
	Save the following cases:
	Field op constant
	Field LIKE constant where constant doesn't start with a wildcard
	Field = field2 where field2 is in a different table
	Field op formula
	Field IS NULL
	Field IS NOT NULL
         Field BETWEEN ...
         Field IN ...
      */
      stat[0].key_dependent|=used_tables;

      bool is_const=1;
      for (uint i=0; i<num_values; i++)
        is_const&= value[i]->const_item();
      if (is_const)
        stat[0].const_keys.merge(possible_keys);
      /*
	We can't always use indexes when comparing a string index to a
	number. cmp_type() is checked to allow compare of dates to numbers.
        eq_func is NEVER true when num_values > 1
       */
      if (!eq_func)
        return;
      if (field->result_type() == STRING_RESULT)
      {
        if ((*value)->result_type() != STRING_RESULT)
        {
          if (field->cmp_type() != (*value)->result_type())
            return;
        }
        else
        {
          /*
            We can't use indexes if the effective collation
            of the operation differ from the field collation.

            We also cannot use index on a text column, as the column may
            contain 'x' 'x\t' 'x ' and 'read_next_same' will stop after
            'x' when searching for WHERE col='x '
          */
          if (field->cmp_type() == STRING_RESULT &&
              (((Field_str*)field)->charset() != cond->compare_collation() ||
               ((*value)->type() != Item::NULL_ITEM &&
                (field->flags & BLOB_FLAG) && !field->binary())))
            return;
        }
      }
    }
  }
  DBUG_ASSERT(num_values == 1);
  /*
    For the moment eq_func is always true. This slot is reserved for future
    extensions where we want to remembers other things than just eq comparisons
  */
  DBUG_ASSERT(eq_func);
  /* Store possible eq field */
  (*key_fields)->field=		field;
  (*key_fields)->eq_func=	eq_func;
  (*key_fields)->val=		*value;
  (*key_fields)->level=		and_level;
  (*key_fields)->optimize=	exists_optimize;
  /*
    If the condition has form "tbl.keypart = othertbl.field" and 
    othertbl.field can be NULL, there will be no matches if othertbl.field 
    has NULL value.
    We use null_rejecting in add_not_null_conds() to add
    'othertbl.field IS NOT NULL' to tab->select_cond.
  */
  (*key_fields)->null_rejecting= ((cond->functype() == Item_func::EQ_FUNC) &&
                                  ((*value)->type() == Item::FIELD_ITEM) &&
                                  ((Item_field*)*value)->field->maybe_null());
  (*key_fields)++;
}

/*
  Add possible keys to array of possible keys originated from a simple predicate

  SYNPOSIS
    add_key_equal_fields()
    key_fields			Pointer to add key, if usable
    and_level			And level, to be stored in KEY_FIELD
    cond                        Condition predicate
    field			Field used in comparision
    eq_func			True if we used =, <=> or IS NULL
    value			Value used for comparison with field
				Is NULL for BETWEEN and IN    
    usable_tables		Tables which can be used for key optimization

  NOTES
    If field items f1 and f2 belong to the same multiple equality and
    a key is added for f1, the the same key is added for f2.

  RETURN
    *key_fields is incremented if we stored a key in the array
*/

static void
add_key_equal_fields(KEY_FIELD **key_fields, uint and_level,
                     Item_func *cond, Item_field *field_item,
                     bool eq_func, Item **val,
                     uint num_values, table_map usable_tables)
{
  Field *field= field_item->field;
  add_key_field(key_fields, and_level, cond, field,
                eq_func, val, num_values, usable_tables);
  Item_equal *item_equal= field_item->item_equal;
  if (item_equal)
  { 
    /*
      Add to the set of possible key values every substitution of
      the field for an equal field included into item_equal
    */
    Item_equal_iterator it(*item_equal);
    Item_field *item;
    while ((item= it++))
    {
      if (!field->eq(item->field))
      {
        add_key_field(key_fields, and_level, cond, item->field,
                      eq_func, val, num_values, usable_tables);
      }
    }
  }
}

static void
add_key_fields(KEY_FIELD **key_fields,uint *and_level,
	       COND *cond, table_map usable_tables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    KEY_FIELD *org_key_fields= *key_fields;

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
	add_key_fields(key_fields,and_level,item,usable_tables);
      for (; org_key_fields != *key_fields ; org_key_fields++)
	org_key_fields->level= *and_level;
    }
    else
    {
      (*and_level)++;
      add_key_fields(key_fields,and_level,li++,usable_tables);
      Item *item;
      while ((item=li++))
      {
	KEY_FIELD *start_key_fields= *key_fields;
	(*and_level)++;
	add_key_fields(key_fields,and_level,item,usable_tables);
	*key_fields=merge_key_fields(org_key_fields,start_key_fields,
				     *key_fields,++(*and_level));
      }
    }
    return;
  }
  /* If item is of type 'field op field/constant' add it to key_fields */

  if (cond->type() != Item::FUNC_ITEM)
    return;
  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->functype() == Item_func::NOT_FUNC)
  {
    Item *item= cond_func->arguments()[0];
    /*
      At this moment all NOT before simple comparison predicates
      are eliminated. NOT IN and NOT BETWEEN are treated similar
      IN and BETWEEN respectively.
    */
    if (item->type() == Item::FUNC_ITEM &&
        ((Item_func *) item)->select_optimize() == Item_func::OPTIMIZE_KEY)
      add_key_fields(key_fields,and_level,item,usable_tables);
    return;
  }
  switch (cond_func->select_optimize()) {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
  {
    // BETWEEN, IN, NE
    if (cond_func->key_item()->real_item()->type() == Item::FIELD_ITEM &&
	!(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      Item **values= cond_func->arguments()+1;
      if (cond_func->functype() == Item_func::NE_FUNC &&
        cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
	     !(cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
        values--;
      DBUG_ASSERT(cond_func->functype() != Item_func::IN_FUNC ||
                  cond_func->argument_count() != 2);
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->key_item()->real_item()),
                           0, values,
                           cond_func->argument_count()-1,
                           usable_tables);
    }
    break;
  }
  case Item_func::OPTIMIZE_OP:
  {
    bool equal_func=(cond_func->functype() == Item_func::EQ_FUNC ||
		     cond_func->functype() == Item_func::EQUAL_FUNC);

    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
	!(cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_key_equal_fields(key_fields, *and_level, cond_func,
	                (Item_field*) (cond_func->arguments()[0])->real_item(),
		           equal_func,
		           cond_func->arguments()+1, 1, usable_tables);
    }
    if (cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
	cond_func->functype() != Item_func::LIKE_FUNC &&
	!(cond_func->arguments()[1]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_key_equal_fields(key_fields, *and_level, cond_func, 
                       (Item_field*) (cond_func->arguments()[1])->real_item(),
		           equal_func,
		           cond_func->arguments(),1,usable_tables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
	!(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      Item *tmp=new Item_null;
      if (unlikely(!tmp))                       // Should never be true
	return;
      add_key_equal_fields(key_fields, *and_level, cond_func,
		    (Item_field*) (cond_func->arguments()[0])->real_item(),
		    cond_func->functype() == Item_func::ISNULL_FUNC,
		    &tmp, 1, usable_tables);
    }
    break;
  case Item_func::OPTIMIZE_EQUAL:
    Item_equal *item_equal= (Item_equal *) cond;
    Item *const_item= item_equal->get_const();
    Item_equal_iterator it(*item_equal);
    Item_field *item;
    if (const_item)
    {
      /*
        For each field field1 from item_equal consider the equality 
        field1=const_item as a condition allowing an index access of the table
        with field1 by the keys value of field1.
      */   
      while ((item= it++))
      {
        add_key_field(key_fields, *and_level, cond_func, item->field,
                      TRUE, &const_item, 1, usable_tables);
      }
    }
    else 
    {
      /*
        Consider all pairs of different fields included into item_equal.
        For each of them (field1, field1) consider the equality 
        field1=field2 as a condition allowing an index access of the table
        with field1 by the keys value of field2.
      */   
      Item_equal_iterator fi(*item_equal);
      while ((item= fi++))
      {
        Field *field= item->field;
        while ((item= it++))
        {
          if (!field->eq(item->field))
          {
            add_key_field(key_fields, *and_level, cond_func, field,
                          TRUE, (Item **) &item, 1, usable_tables);
          }
        }
        it.rewind();
      }
    }
    break;
  }
}

/*
  Add all keys with uses 'field' for some keypart
  If field->and_level != and_level then only mark key_part as const_part
*/

static uint
max_part_bit(key_part_map bits)
{
  uint found;
  for (found=0; bits & 1 ; found++,bits>>=1) ;
  return found;
}

static void
add_key_part(DYNAMIC_ARRAY *keyuse_array,KEY_FIELD *key_field)
{
  Field *field=key_field->field;
  TABLE *form= field->table;
  KEYUSE keyuse;

  if (key_field->eq_func && !(key_field->optimize & KEY_OPTIMIZE_EXISTS))
  {
    for (uint key=0 ; key < form->s->keys ; key++)
    {
      if (!(form->keys_in_use_for_query.is_set(key)))
	continue;
      if (form->key_info[key].flags & HA_FULLTEXT)
	continue;    // ToDo: ft-keys in non-ft queries.   SerG

      uint key_parts= (uint) form->key_info[key].key_parts;
      for (uint part=0 ; part <  key_parts ; part++)
      {
	if (field->eq(form->key_info[key].key_part[part].field))
	{
	  keyuse.table= field->table;
	  keyuse.val =  key_field->val;
	  keyuse.key =  key;
	  keyuse.keypart=part;
	  keyuse.keypart_map= (key_part_map) 1 << part;
	  keyuse.used_tables=key_field->val->used_tables();
	  keyuse.optimize= key_field->optimize & KEY_OPTIMIZE_REF_OR_NULL;
          keyuse.null_rejecting= key_field->null_rejecting;
	  VOID(insert_dynamic(keyuse_array,(gptr) &keyuse));
	}
      }
    }
  }
}


#define FT_KEYPART   (MAX_REF_PARTS+10)

static void
add_ft_keys(DYNAMIC_ARRAY *keyuse_array,
            JOIN_TAB *stat,COND *cond,table_map usable_tables)
{
  Item_func_match *cond_func=NULL;

  if (!cond)
    return;

  if (cond->type() == Item::FUNC_ITEM)
  {
    Item_func *func=(Item_func *)cond;
    Item_func::Functype functype=  func->functype();
    if (functype == Item_func::FT_FUNC)
      cond_func=(Item_func_match *)cond;
    else if (func->arg_count == 2)
    {
      Item_func *arg0=(Item_func *)(func->arguments()[0]),
                *arg1=(Item_func *)(func->arguments()[1]);
      if (arg1->const_item()  &&
          ((functype == Item_func::GE_FUNC && arg1->val_real() > 0) ||
           (functype == Item_func::GT_FUNC && arg1->val_real() >=0))  &&
           arg0->type() == Item::FUNC_ITEM            &&
           arg0->functype() == Item_func::FT_FUNC)
        cond_func=(Item_func_match *) arg0;
      else if (arg0->const_item() &&
               ((functype == Item_func::LE_FUNC && arg0->val_real() > 0) ||
                (functype == Item_func::LT_FUNC && arg0->val_real() >=0)) &&
                arg1->type() == Item::FUNC_ITEM          &&
                arg1->functype() == Item_func::FT_FUNC)
        cond_func=(Item_func_match *) arg1;
    }
  }
  else if (cond->type() == Item::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
        add_ft_keys(keyuse_array,stat,item,usable_tables);
    }
  }

  if (!cond_func || cond_func->key == NO_SUCH_KEY ||
      !(usable_tables & cond_func->table->map))
    return;

  KEYUSE keyuse;
  keyuse.table= cond_func->table;
  keyuse.val =  cond_func;
  keyuse.key =  cond_func->key;
  keyuse.keypart= FT_KEYPART;
  keyuse.used_tables=cond_func->key_item()->used_tables();
  keyuse.optimize= 0;
  keyuse.keypart_map= 0;
  VOID(insert_dynamic(keyuse_array,(gptr) &keyuse));
}


static int
sort_keyuse(KEYUSE *a,KEYUSE *b)
{
  int res;
  if (a->table->tablenr != b->table->tablenr)
    return (int) (a->table->tablenr - b->table->tablenr);
  if (a->key != b->key)
    return (int) (a->key - b->key);
  if (a->keypart != b->keypart)
    return (int) (a->keypart - b->keypart);
  // Place const values before other ones
  if ((res= test((a->used_tables & ~OUTER_REF_TABLE_BIT)) -
       test((b->used_tables & ~OUTER_REF_TABLE_BIT))))
    return res;
  /* Place rows that are not 'OPTIMIZE_REF_OR_NULL' first */
  return (int) ((a->optimize & KEY_OPTIMIZE_REF_OR_NULL) -
		(b->optimize & KEY_OPTIMIZE_REF_OR_NULL));
}


/*
  Update keyuse array with all possible keys we can use to fetch rows
  
  SYNOPSIS
    update_ref_and_keys()
      thd 
      keyuse     OUT Put here ordered array of KEYUSE structures
      join_tab       Array in tablenr_order
      tables         Number of tables in join
      cond           WHERE condition (note that the function analyzes 
                     join_tab[i]->on_expr too)
      normal_tables  tables not inner w.r.t some outer join (ones for which
                     we can make ref access based the WHERE clause)
      select_lex     current SELECT
      
  RETURN 
   0 - OK
   1 - Out of memory.
*/

static bool
update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,
		    uint tables, COND *cond, COND_EQUAL *cond_equal,
                    table_map normal_tables, SELECT_LEX *select_lex)
{
  uint	and_level,i,found_eq_constant;
  KEY_FIELD *key_fields, *end, *field;
  uint m= 1;
  
  if (cond_equal && cond_equal->max_members)
    m= cond_equal->max_members;

  if (!(key_fields=(KEY_FIELD*)
	thd->alloc(sizeof(key_fields[0])*
		   (thd->lex->current_select->cond_count+1)*2*m)))
    return TRUE; /* purecov: inspected */
  and_level= 0;
  field= end= key_fields;
  if (my_init_dynamic_array(keyuse,sizeof(KEYUSE),20,64))
    return TRUE;
  if (cond)
  {
    add_key_fields(&end,&and_level,cond,normal_tables);
    for (; field != end ; field++)
    {
      add_key_part(keyuse,field);
      /* Mark that we can optimize LEFT JOIN */
      if (field->val->type() == Item::NULL_ITEM &&
	  !field->field->real_maybe_null())
	field->field->table->reginfo.not_exists_optimize=1;
    }
  }
  for (i=0 ; i < tables ; i++)
  {
    /*
      Block the creation of keys for inner tables of outer joins.
      Here only the outer joins that can not be converted to
      inner joins are left and all nests that can be eliminated
      are flattened.
      In the future when we introduce conditional accesses
      for inner tables in outer joins these keys will be taken
      into account as well.
    */ 
    if (*join_tab[i].on_expr_ref)
    {
      add_key_fields(&end,&and_level,*join_tab[i].on_expr_ref,
		     join_tab[i].table->map);
    }
    else 
    {
      TABLE_LIST *tab= join_tab[i].table->pos_in_table_list;
      TABLE_LIST *embedding= tab->embedding;
      if (embedding)
      {
        NESTED_JOIN *nested_join= embedding->nested_join;
        if (nested_join->join_list.head() == tab)
          add_key_fields(&end, &and_level, embedding->on_expr,
                         nested_join->used_tables);
      }
    }
  }
  /* fill keyuse with found key parts */
  for ( ; field != end ; field++)
    add_key_part(keyuse,field);

  if (select_lex->ftfunc_list->elements)
  {
    add_ft_keys(keyuse,join_tab,cond,normal_tables);
  }

  /*
    Sort the array of possible keys and remove the following key parts:
    - ref if there is a keypart which is a ref and a const.
      (e.g. if there is a key(a,b) and the clause is a=3 and b=7 and b=t2.d,
      then we skip the key part corresponding to b=t2.d)
    - keyparts without previous keyparts
      (e.g. if there is a key(a,b,c) but only b < 5 (or a=2 and c < 3) is
      used in the query, we drop the partial key parts from consideration).
    Special treatment for ft-keys.
  */
  if (keyuse->elements)
  {
    KEYUSE end,*prev,*save_pos,*use;

    qsort(keyuse->buffer,keyuse->elements,sizeof(KEYUSE),
	  (qsort_cmp) sort_keyuse);

    bzero((char*) &end,sizeof(end));		/* Add for easy testing */
    VOID(insert_dynamic(keyuse,(gptr) &end));

    use=save_pos=dynamic_element(keyuse,0,KEYUSE*);
    prev=&end;
    found_eq_constant=0;
    for (i=0 ; i < keyuse->elements-1 ; i++,use++)
    {
      if (!use->used_tables)
	use->table->const_key_parts[use->key]|= use->keypart_map;
      if (use->keypart != FT_KEYPART)
      {
	if (use->key == prev->key && use->table == prev->table)
	{
	  if (prev->keypart+1 < use->keypart ||
	      prev->keypart == use->keypart && found_eq_constant)
	    continue;				/* remove */
	}
	else if (use->keypart != 0)		// First found must be 0
	  continue;
      }

      *save_pos= *use;
      prev=use;
      found_eq_constant= !use->used_tables;
      /* Save ptr to first use */
      if (!use->table->reginfo.join_tab->keyuse)
	use->table->reginfo.join_tab->keyuse=save_pos;
      use->table->reginfo.join_tab->checked_keys.set_bit(use->key);
      save_pos++;
    }
    i=(uint) (save_pos-(KEYUSE*) keyuse->buffer);
    VOID(set_dynamic(keyuse,(gptr) &end,i));
    keyuse->elements=i;
  }
  return FALSE;
}

/*
  Update some values in keyuse for faster choose_plan() loop
*/

static void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array)
{
  KEYUSE *end,*keyuse= dynamic_element(keyuse_array, 0, KEYUSE*);

  for (end= keyuse+ keyuse_array->elements ; keyuse < end ; keyuse++)
  {
    table_map map;
    /*
      If we find a ref, assume this table matches a proportional
      part of this table.
      For example 100 records matching a table with 5000 records
      gives 5000/100 = 50 records per key
      Constant tables are ignored.
      To avoid bad matches, we don't make ref_table_rows less than 100.
    */
    keyuse->ref_table_rows= ~(ha_rows) 0;	// If no ref
    if (keyuse->used_tables &
	(map= (keyuse->used_tables & ~join->const_table_map &
	       ~OUTER_REF_TABLE_BIT)))
    {
      uint tablenr;
      for (tablenr=0 ; ! (map & 1) ; map>>=1, tablenr++) ;
      if (map == 1)			// Only one table
      {
	TABLE *tmp_table=join->all_tables[tablenr];
	keyuse->ref_table_rows= max(tmp_table->file->records, 100);
      }
    }
    /*
      Outer reference (external field) is constant for single executing
      of subquery
    */
    if (keyuse->used_tables == OUTER_REF_TABLE_BIT)
      keyuse->ref_table_rows= 1;
  }
}


/*
  Discover the indexes that can be used for GROUP BY or DISTINCT queries.

  SYNOPSIS
    add_group_and_distinct_keys()
    join
    join_tab

  DESCRIPTION
    If the query has a GROUP BY clause, find all indexes that contain all
    GROUP BY fields, and add those indexes to join->const_keys.
    If the query has a DISTINCT clause, find all indexes that contain all
    SELECT fields, and add those indexes to join->const_keys.
    This allows later on such queries to be processed by a
    QUICK_GROUP_MIN_MAX_SELECT.

  RETURN
    None
*/

static void
add_group_and_distinct_keys(JOIN *join, JOIN_TAB *join_tab)
{
  List<Item_field> indexed_fields;
  List_iterator<Item_field> indexed_fields_it(indexed_fields);
  ORDER      *cur_group;
  Item_field *cur_item;
  key_map possible_keys(0);

  if (join->group_list)
  { /* Collect all query fields referenced in the GROUP clause. */
    for (cur_group= join->group_list; cur_group; cur_group= cur_group->next)
      (*cur_group->item)->walk(&Item::collect_item_field_processor,
                               (byte*) &indexed_fields);
  }
  else if (join->select_distinct)
  { /* Collect all query fields referenced in the SELECT clause. */
    List<Item> &select_items= join->fields_list;
    List_iterator<Item> select_items_it(select_items);
    Item *item;
    while ((item= select_items_it++))
      item->walk(&Item::collect_item_field_processor, (byte*) &indexed_fields);
  }
  else
    return;

  if (indexed_fields.elements == 0)
    return;

  /* Intersect the keys of all group fields. */
  cur_item= indexed_fields_it++;
  possible_keys.merge(cur_item->field->part_of_key);
  while ((cur_item= indexed_fields_it++))
  {
    possible_keys.intersect(cur_item->field->part_of_key);
  }

  if (!possible_keys.is_clear_all())
    join_tab->const_keys.merge(possible_keys);
}


/*****************************************************************************
  Go through all combinations of not marked tables and find the one
  which uses least records
*****************************************************************************/

/* Save const tables first as used tables */

static void
set_position(JOIN *join,uint idx,JOIN_TAB *table,KEYUSE *key)
{
  join->positions[idx].table= table;
  join->positions[idx].key=key;
  join->positions[idx].records_read=1.0;	/* This is a const table */

  /* Move the const table as down as possible in best_ref */
  JOIN_TAB **pos=join->best_ref+idx+1;
  JOIN_TAB *next=join->best_ref[idx];
  for (;next != table ; pos++)
  {
    JOIN_TAB *tmp=pos[0];
    pos[0]=next;
    next=tmp;
  }
  join->best_ref[idx]=table;
}


/*
  Find the best access path for an extension of a partial execution plan and
  add this path to the plan.

  SYNOPSIS
    best_access_path()
    join             pointer to the structure providing all context info
                     for the query
    s                the table to be joined by the function
    thd              thread for the connection that submitted the query
    remaining_tables set of tables not included into the partial plan yet
    idx              the length of the partial plan
    record_count     estimate for the number of records returned by the partial
                     plan
    read_time        the cost of the partial plan

  DESCRIPTION
    The function finds the best access path to table 's' from the passed
    partial plan where an access path is the general term for any means to
    access the data in 's'. An access path may use either an index or a scan,
    whichever is cheaper. The input partial plan is passed via the array
    'join->positions' of length 'idx'. The chosen access method for 's' and its
    cost are stored in 'join->positions[idx]'.

  RETURN
    None
*/

static void
best_access_path(JOIN      *join,
                 JOIN_TAB  *s,
                 THD       *thd,
                 table_map remaining_tables,
                 uint      idx,
                 double    record_count,
                 double    read_time)
{
  KEYUSE *best_key=         0;
  uint best_max_key_part=   0;
  my_bool found_constraint= 0;
  double best=              DBL_MAX;
  double best_time=         DBL_MAX;
  double records=           DBL_MAX;
  double tmp;
  ha_rows rec;

  DBUG_ENTER("best_access_path");

  if (s->keyuse)
  {                                            /* Use key if possible */
    TABLE *table= s->table;
    KEYUSE *keyuse,*start_key=0;
    double best_records= DBL_MAX;
    uint max_key_part=0;

    /* Test how we can use keys */
    rec= s->records/MATCHING_ROWS_IN_OTHER_TABLE;  // Assumed records/key
    for (keyuse=s->keyuse ; keyuse->table == table ;)
    {
      key_part_map found_part= 0;
      table_map found_ref=     0;
      uint found_ref_or_null=  0;
      uint key=     keyuse->key;
      KEY *keyinfo= table->key_info+key;
      bool ft_key=  (keyuse->keypart == FT_KEYPART);

      /* Calculate how many key segments of the current key we can use */
      start_key= keyuse;
      do
      { /* for each keypart */
        uint keypart= keyuse->keypart;
        uint found_part_ref_or_null= KEY_OPTIMIZE_REF_OR_NULL;
        do
        {
          if (!(remaining_tables & keyuse->used_tables) &&
              !(found_ref_or_null & keyuse->optimize))
          {
            found_part|= keyuse->keypart_map;
            found_ref|=  keyuse->used_tables;
            if (rec > keyuse->ref_table_rows)
              rec= keyuse->ref_table_rows;
            found_part_ref_or_null&= keyuse->optimize;
          }
          keyuse++;
          found_ref_or_null|= found_part_ref_or_null;
        } while (keyuse->table == table && keyuse->key == key &&
                 keyuse->keypart == keypart);
      } while (keyuse->table == table && keyuse->key == key);

      /*
        Assume that that each key matches a proportional part of table.
      */
      if (!found_part && !ft_key)
        continue;                               // Nothing usable found

      if (rec < MATCHING_ROWS_IN_OTHER_TABLE)
        rec= MATCHING_ROWS_IN_OTHER_TABLE;      // Fix for small tables

      /*
        ft-keys require special treatment
      */
      if (ft_key)
      {
        /*
          Really, there should be records=0.0 (yes!)
          but 1.0 would be probably safer
        */
        tmp= prev_record_reads(join, found_ref);
        records= 1.0;
      }
      else
      {
        found_constraint= 1;
        /*
          Check if we found full key
        */
        if (found_part == PREV_BITS(uint,keyinfo->key_parts) &&
            !found_ref_or_null)
        {                                         /* use eq key */
          max_key_part= (uint) ~0;
          if ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
          {
            tmp = prev_record_reads(join, found_ref);
            records=1.0;
          }
          else
          {
            if (!found_ref)
            {                                     /* We found a const key */
              if (table->quick_keys.is_set(key))
                records= (double) table->quick_rows[key];
              else
              {
                /* quick_range couldn't use key! */
                records= (double) s->records/rec;
              }
            }
            else
            {
              if (!(records=keyinfo->rec_per_key[keyinfo->key_parts-1]))
              {                                   /* Prefer longer keys */
                records=
                  ((double) s->records / (double) rec *
                   (1.0 +
                    ((double) (table->s->max_key_length-keyinfo->key_length) /
                     (double) table->s->max_key_length)));
                if (records < 2.0)
                  records=2.0;               /* Can't be as good as a unique */
              }
            }
            /* Limit the number of matched rows */
            tmp = records;
            set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
            if (table->used_keys.is_set(key))
            {
              /* we can use only index tree */
              uint keys_per_block= table->file->block_size/2/
                (keyinfo->key_length+table->file->ref_length)+1;
              tmp = record_count*(tmp+keys_per_block-1)/keys_per_block;
            }
            else
              tmp = record_count*min(tmp,s->worst_seeks);
          }
        }
        else
        {
          /*
            Use as much key-parts as possible and a uniq key is better
            than a not unique key
            Set tmp to (previous record count) * (records / combination)
          */
          if ((found_part & 1) &&
              (!(table->file->index_flags(key, 0, 0) & HA_ONLY_WHOLE_INDEX) ||
               found_part == PREV_BITS(uint,keyinfo->key_parts)))
          {
            max_key_part=max_part_bit(found_part);
            /*
              Check if quick_range could determinate how many rows we
              will match
            */
            if (table->quick_keys.is_set(key) &&
                table->quick_key_parts[key] == max_key_part)
              tmp= records= (double) table->quick_rows[key];
            else
            {
              /* Check if we have statistic about the distribution */
              if ((records = keyinfo->rec_per_key[max_key_part-1]))
                tmp = records;
              else
              {
                /*
                  Assume that the first key part matches 1% of the file
                  and that the whole key matches 10 (duplicates) or 1
                  (unique) records.
                  Assume also that more key matches proportionally more
                  records
                  This gives the formula:
                  records = (x * (b-a) + a*c-b)/(c-1)

                  b = records matched by whole key
                  a = records matched by first key part (1% of all records?)
                  c = number of key parts in key
                  x = used key parts (1 <= x <= c)
                */
                double rec_per_key;
                if (!(rec_per_key=(double)
                      keyinfo->rec_per_key[keyinfo->key_parts-1]))
                  rec_per_key=(double) s->records/rec+1;

                if (!s->records)
                  tmp = 0;
                else if (rec_per_key/(double) s->records >= 0.01)
                  tmp = rec_per_key;
                else
                {
                  double a=s->records*0.01;
                  if (keyinfo->key_parts > 1)
                    tmp= (max_key_part * (rec_per_key - a) +
                          a*keyinfo->key_parts - rec_per_key)/
                         (keyinfo->key_parts-1);
                  else
                    tmp= a;
                  set_if_bigger(tmp,1.0);
                }
                records = (ulong) tmp;
              }
              /*
                If quick_select was used on a part of this key, we know
                the maximum number of rows that the key can match.
              */
              if (table->quick_keys.is_set(key) &&
                  table->quick_key_parts[key] <= max_key_part &&
                  records > (double) table->quick_rows[key])
                tmp= records= (double) table->quick_rows[key];
              else if (found_ref_or_null)
              {
                /* We need to do two key searches to find key */
                tmp *= 2.0;
                records *= 2.0;
              }
            }
            /* Limit the number of matched rows */
            set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
            if (table->used_keys.is_set(key))
            {
              /* we can use only index tree */
              uint keys_per_block= table->file->block_size/2/
                (keyinfo->key_length+table->file->ref_length)+1;
              tmp = record_count*(tmp+keys_per_block-1)/keys_per_block;
            }
            else
              tmp = record_count*min(tmp,s->worst_seeks);
          }
          else
            tmp = best_time;                    // Do nothing
        }
      } /* not ft_key */
      if (tmp < best_time - records/(double) TIME_FOR_COMPARE)
      {
        best_time= tmp + records/(double) TIME_FOR_COMPARE;
        best= tmp;
        best_records= records;
        best_key= start_key;
        best_max_key_part= max_key_part;
      }
    }
    records= best_records;
  }

  /*
    Don't test table scan if it can't be better.
    Prefer key lookup if we would use the same key for scanning.

    Don't do a table scan on InnoDB tables, if we can read the used
    parts of the row from any of the used index.
    This is because table scans uses index and we would not win
    anything by using a table scan.
  */
  if ((records >= s->found_records || best > s->read_time) &&
      !(s->quick && best_key && s->quick->index == best_key->key &&
        best_max_key_part >= s->table->quick_key_parts[best_key->key]) &&
      !((s->table->file->table_flags() & HA_TABLE_SCAN_ON_INDEX) &&
        ! s->table->used_keys.is_clear_all() && best_key) &&
      !(s->table->force_index && best_key))
  {                                             // Check full join
    ha_rows rnd_records= s->found_records;
    /*
      If there is a restriction on the table, assume that 25% of the
      rows can be skipped on next part.
      This is to force tables that this table depends on before this
      table
    */
    if (found_constraint)
      rnd_records-= rnd_records/4;

    /*
      Range optimizer never proposes a RANGE if it isn't better
      than FULL: so if RANGE is present, it's always preferred to FULL.
      Here we estimate its cost.
    */
    if (s->quick)
    {
      /*
        For each record we:
        - read record range through 'quick'
        - skip rows which does not satisfy WHERE constraints
      */
      tmp= record_count *
        (s->quick->read_time +
         (s->found_records - rnd_records)/(double) TIME_FOR_COMPARE);
    }
    else
    {
      /* Estimate cost of reading table. */
      tmp= s->table->file->scan_time();
      if (s->table->map & join->outer_join)     // Can't use join cache
      {
        /*
          For each record we have to:
          - read the whole table record 
          - skip rows which does not satisfy join condition
        */
        tmp= record_count *
          (tmp +
           (s->records - rnd_records)/(double) TIME_FOR_COMPARE);
      }
      else
      {
        /* We read the table as many times as join buffer becomes full. */
        tmp*= (1.0 + floor((double) cache_record_length(join,idx) *
                           record_count /
                           (double) thd->variables.join_buff_size));
        /* 
            We don't make full cartesian product between rows in the scanned
           table and existing records because we skip all rows from the
           scanned table, which does not satisfy join condition when 
           we read the table (see flush_cached_records for details). Here we
           take into account cost to read and skip these records.
        */
        tmp+= (s->records - rnd_records)/(double) TIME_FOR_COMPARE;
      }
    }

    /*
      We estimate the cost of evaluating WHERE clause for found records
      as record_count * rnd_records / TIME_FOR_COMPARE. This cost plus
      tmp give us total cost of using TABLE SCAN
    */
    if (best == DBL_MAX ||
        (tmp  + record_count/(double) TIME_FOR_COMPARE*rnd_records <
         best + record_count/(double) TIME_FOR_COMPARE*records))
    {
      /*
        If the table has a range (s->quick is set) make_join_select()
        will ensure that this will be used
      */
      best= tmp;
      records= rows2double(rnd_records);
      best_key= 0;
    }
  }

  /* Update the cost information for the current partial plan */
  join->positions[idx].records_read= records;
  join->positions[idx].read_time=    best;
  join->positions[idx].key=          best_key;
  join->positions[idx].table=        s;

  if (!best_key &&
      idx == join->const_tables &&
      s->table == join->sort_by_table &&
      join->unit->select_limit_cnt >= records)
    join->sort_by_table= (TABLE*) 1;  // Must use temporary table

  DBUG_VOID_RETURN;
}


/*
  Selects and invokes a search strategy for an optimal query plan.

  SYNOPSIS
    choose_plan()
    join        pointer to the structure providing all context info for
                the query
    join_tables set of the tables in the query

  DESCRIPTION
    The function checks user-configurable parameters that control the search
    strategy for an optimal plan, selects the search method and then invokes
    it. Each specific optimization procedure stores the final optimal plan in
    the array 'join->best_positions', and the cost of the plan in
    'join->best_read'.

  RETURN
    None
*/

static void
choose_plan(JOIN *join, table_map join_tables)
{
  uint search_depth= join->thd->variables.optimizer_search_depth;
  uint prune_level=  join->thd->variables.optimizer_prune_level;
  bool straight_join= join->select_options & SELECT_STRAIGHT_JOIN;
  DBUG_ENTER("choose_plan");

  /*
    if (SELECT_STRAIGHT_JOIN option is set)
      reorder tables so dependent tables come after tables they depend 
      on, otherwise keep tables in the order they were specified in the query 
    else
      Apply heuristic: pre-sort all access plans with respect to the number of
      records accessed.
  */
  qsort(join->best_ref + join->const_tables, join->tables - join->const_tables,
        sizeof(JOIN_TAB*), straight_join?join_tab_cmp_straight:join_tab_cmp);
  
  if (straight_join)
  {
    optimize_straight_join(join, join_tables);
  }
  else
  {
    if (search_depth == MAX_TABLES+2)
    { /*
        TODO: 'MAX_TABLES+2' denotes the old implementation of find_best before
        the greedy version. Will be removed when greedy_search is approved.
      */
      join->best_read= DBL_MAX;
      find_best(join, join_tables, join->const_tables, 1.0, 0.0);
    } 
    else
    {
      if (search_depth == 0)
        /* Automatically determine a reasonable value for 'search_depth' */
        search_depth= determine_search_depth(join);
      greedy_search(join, join_tables, search_depth, prune_level);
    }
  }

  /* 
    Store the cost of this query into a user variable
    Don't update last_query_cost for 'show status' command
  */
  if (join->thd->lex->orig_sql_command != SQLCOM_SHOW_STATUS)
    join->thd->status_var.last_query_cost= join->best_read;
  DBUG_VOID_RETURN;
}


/*
  Compare two JOIN_TAB objects based on the number of accessed records.

  SYNOPSIS
    join_tab_cmp()
    ptr1 pointer to first JOIN_TAB object
    ptr2 pointer to second JOIN_TAB object

  RETURN
    1  if first is bigger
    -1 if second is bigger
    0  if equal
*/

static int
join_tab_cmp(const void* ptr1, const void* ptr2)
{
  JOIN_TAB *jt1= *(JOIN_TAB**) ptr1;
  JOIN_TAB *jt2= *(JOIN_TAB**) ptr2;

  if (jt1->dependent & jt2->table->map)
    return 1;
  if (jt2->dependent & jt1->table->map)
    return -1;  
  if (jt1->found_records > jt2->found_records)
    return 1;
  if (jt1->found_records < jt2->found_records)
    return -1; 
  return jt1 > jt2 ? 1 : (jt1 < jt2 ? -1 : 0);
}


/* 
  Same as join_tab_cmp, but for use with SELECT_STRAIGHT_JOIN.
*/

static int
join_tab_cmp_straight(const void* ptr1, const void* ptr2)
{
  JOIN_TAB *jt1= *(JOIN_TAB**) ptr1;
  JOIN_TAB *jt2= *(JOIN_TAB**) ptr2;

  if (jt1->dependent & jt2->table->map)
    return 1;
  if (jt2->dependent & jt1->table->map)
    return -1;
  return jt1 > jt2 ? 1 : (jt1 < jt2 ? -1 : 0);
}

/*
  Heuristic procedure to automatically guess a reasonable degree of
  exhaustiveness for the greedy search procedure.

  SYNOPSIS
    determine_search_depth()
    join   pointer to the structure providing all context info for the query

  DESCRIPTION
    The procedure estimates the optimization time and selects a search depth
    big enough to result in a near-optimal QEP, that doesn't take too long to
    find. If the number of tables in the query exceeds some constant, then
    search_depth is set to this constant.

  NOTES
    This is an extremely simplistic implementation that serves as a stub for a
    more advanced analysis of the join. Ideally the search depth should be
    determined by learning from previous query optimizations, because it will
    depend on the CPU power (and other factors).

  RETURN
    A positive integer that specifies the search depth (and thus the
    exhaustiveness) of the depth-first search algorithm used by
    'greedy_search'.
*/

static uint
determine_search_depth(JOIN *join)
{
  uint table_count=  join->tables - join->const_tables;
  uint search_depth;
  /* TODO: this value should be determined dynamically, based on statistics: */
  uint max_tables_for_exhaustive_opt= 7;

  if (table_count <= max_tables_for_exhaustive_opt)
    search_depth= table_count+1; // use exhaustive for small number of tables
  else
    /*
      TODO: this value could be determined by some mapping of the form:
      depth : table_count -> [max_tables_for_exhaustive_opt..MAX_EXHAUSTIVE]
    */
    search_depth= max_tables_for_exhaustive_opt; // use greedy search

  return search_depth;
}


/*
  Select the best ways to access the tables in a query without reordering them.

  SYNOPSIS
    optimize_straight_join()
    join          pointer to the structure providing all context info for
                  the query
    join_tables   set of the tables in the query

  DESCRIPTION
    Find the best access paths for each query table and compute their costs
    according to their order in the array 'join->best_ref' (thus without
    reordering the join tables). The function calls sequentially
    'best_access_path' for each table in the query to select the best table
    access method. The final optimal plan is stored in the array
    'join->best_positions', and the corresponding cost in 'join->best_read'.

  NOTES
    This function can be applied to:
    - queries with STRAIGHT_JOIN
    - internally to compute the cost of an arbitrary QEP
    Thus 'optimize_straight_join' can be used at any stage of the query
    optimization process to finalize a QEP as it is.

  RETURN
    None
*/

static void
optimize_straight_join(JOIN *join, table_map join_tables)
{
  JOIN_TAB *s;
  uint idx= join->const_tables;
  double    record_count= 1.0;
  double    read_time=    0.0;
 
  for (JOIN_TAB **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    /* Find the best access method from 's' to the current partial plan */
    best_access_path(join, s, join->thd, join_tables, idx, record_count, read_time);
    /* compute the cost of the new plan extended with 's' */
    record_count*= join->positions[idx].records_read;
    read_time+=    join->positions[idx].read_time;
    join_tables&= ~(s->table->map);
    ++idx;
  }

  read_time+= record_count / (double) TIME_FOR_COMPARE;
  if (join->sort_by_table &&
      join->sort_by_table != join->positions[join->const_tables].table->table)
    read_time+= record_count;  // We have to make a temp table
  memcpy((gptr) join->best_positions, (gptr) join->positions,
         sizeof(POSITION)*idx);
  join->best_read= read_time;
}


/*
  Find a good, possibly optimal, query execution plan (QEP) by a greedy search.

  SYNOPSIS
    join             pointer to the structure providing all context info
                     for the query
    remaining_tables set of tables not included into the partial plan yet
    search_depth     controlls the exhaustiveness of the search
    prune_level      the pruning heuristics that should be applied during
                     search

  DESCRIPTION
    The search procedure uses a hybrid greedy/exhaustive search with controlled
    exhaustiveness. The search is performed in N = card(remaining_tables)
    steps. Each step evaluates how promising is each of the unoptimized tables,
    selects the most promising table, and extends the current partial QEP with
    that table.  Currenly the most 'promising' table is the one with least
    expensive extension.
    There are two extreme cases:
    1. When (card(remaining_tables) < search_depth), the estimate finds the best
       complete continuation of the partial QEP. This continuation can be
       used directly as a result of the search.
    2. When (search_depth == 1) the 'best_extension_by_limited_search'
       consideres the extension of the current QEP with each of the remaining
       unoptimized tables.
    All other cases are in-between these two extremes. Thus the parameter
    'search_depth' controlls the exhaustiveness of the search. The higher the
    value, the longer the optimizaton time and possibly the better the
    resulting plan. The lower the value, the fewer alternative plans are
    estimated, but the more likely to get a bad QEP.

    All intermediate and final results of the procedure are stored in 'join':
    join->positions      modified for every partial QEP that is explored
    join->best_positions modified for the current best complete QEP
    join->best_read      modified for the current best complete QEP
    join->best_ref       might be partially reordered
    The final optimal plan is stored in 'join->best_positions', and its
    corresponding cost in 'join->best_read'.

  NOTES
    The following pseudocode describes the algorithm of 'greedy_search':

    procedure greedy_search
    input: remaining_tables
    output: pplan;
    {
      pplan = <>;
      do {
        (t, a) = best_extension(pplan, remaining_tables);
        pplan = concat(pplan, (t, a));
        remaining_tables = remaining_tables - t;
      } while (remaining_tables != {})
      return pplan;
    }

    where 'best_extension' is a placeholder for a procedure that selects the
    most "promising" of all tables in 'remaining_tables'.
    Currently this estimate is performed by calling
    'best_extension_by_limited_search' to evaluate all extensions of the
    current QEP of size 'search_depth', thus the complexity of 'greedy_search'
    mainly depends on that of 'best_extension_by_limited_search'.

    If 'best_extension()' == 'best_extension_by_limited_search()', then the
    worst-case complexity of this algorithm is <=
    O(N*N^search_depth/search_depth). When serch_depth >= N, then the
    complexity of greedy_search is O(N!).

    In the future, 'greedy_search' might be extended to support other
    implementations of 'best_extension', e.g. some simpler quadratic procedure.

  RETURN
    None
*/

static void
greedy_search(JOIN      *join,
              table_map remaining_tables,
              uint      search_depth,
              uint      prune_level)
{
  double    record_count= 1.0;
  double    read_time=    0.0;
  uint      idx= join->const_tables; // index into 'join->best_ref'
  uint      best_idx;
  uint      rem_size;    // cardinality of remaining_tables
  POSITION  best_pos;
  JOIN_TAB  *best_table; // the next plan node to be added to the curr QEP

  DBUG_ENTER("greedy_search");

  /* number of tables that remain to be optimized */
  rem_size= my_count_bits(remaining_tables);

  do {
    /* Find the extension of the current QEP with the lowest cost */
    join->best_read= DBL_MAX;
    best_extension_by_limited_search(join, remaining_tables, idx, record_count,
                                     read_time, search_depth, prune_level);

    if (rem_size <= search_depth)
    {
      /*
        'join->best_positions' contains a complete optimal extension of the
        current partial QEP.
      */
      DBUG_EXECUTE("opt", print_plan(join, read_time, record_count,
                                     join->tables, "optimal"););
      DBUG_VOID_RETURN;
    }

    /* select the first table in the optimal extension as most promising */
    best_pos= join->best_positions[idx];
    best_table= best_pos.table;
    /*
      Each subsequent loop of 'best_extension_by_limited_search' uses
      'join->positions' for cost estimates, therefore we have to update its
      value.
    */
    join->positions[idx]= best_pos;

    /* find the position of 'best_table' in 'join->best_ref' */
    best_idx= idx;
    JOIN_TAB *pos= join->best_ref[best_idx];
    while (pos && best_table != pos)
      pos= join->best_ref[++best_idx];
    DBUG_ASSERT((pos != NULL)); // should always find 'best_table'
    /* move 'best_table' at the first free position in the array of joins */
    swap_variables(JOIN_TAB*, join->best_ref[idx], join->best_ref[best_idx]);

    /* compute the cost of the new plan extended with 'best_table' */
    record_count*= join->positions[idx].records_read;
    read_time+=    join->positions[idx].read_time;

    remaining_tables&= ~(best_table->table->map);
    --rem_size;
    ++idx;

    DBUG_EXECUTE("opt",
                 print_plan(join, read_time, record_count, idx, "extended"););
  } while (TRUE);
}


/*
  Find a good, possibly optimal, query execution plan (QEP) by a possibly
  exhaustive search.

  SYNOPSIS
    best_extension_by_limited_search()
    join             pointer to the structure providing all context info for
                     the query
    remaining_tables set of tables not included into the partial plan yet
    idx              length of the partial QEP in 'join->positions';
                     since a depth-first search is used, also corresponds to
                     the current depth of the search tree;
                     also an index in the array 'join->best_ref';
    record_count     estimate for the number of records returned by the best
                     partial plan
    read_time        the cost of the best partial plan
    search_depth     maximum depth of the recursion and thus size of the found
                     optimal plan (0 < search_depth <= join->tables+1).
    prune_level      pruning heuristics that should be applied during optimization
                     (values: 0 = EXHAUSTIVE, 1 = PRUNE_BY_TIME_OR_ROWS)

  DESCRIPTION
    The procedure searches for the optimal ordering of the query tables in set
    'remaining_tables' of size N, and the corresponding optimal access paths to each
    table. The choice of a table order and an access path for each table
    constitutes a query execution plan (QEP) that fully specifies how to
    execute the query.
   
    The maximal size of the found plan is controlled by the parameter
    'search_depth'. When search_depth == N, the resulting plan is complete and
    can be used directly as a QEP. If search_depth < N, the found plan consists
    of only some of the query tables. Such "partial" optimal plans are useful
    only as input to query optimization procedures, and cannot be used directly
    to execute a query.

    The algorithm begins with an empty partial plan stored in 'join->positions'
    and a set of N tables - 'remaining_tables'. Each step of the algorithm
    evaluates the cost of the partial plan extended by all access plans for
    each of the relations in 'remaining_tables', expands the current partial
    plan with the access plan that results in lowest cost of the expanded
    partial plan, and removes the corresponding relation from
    'remaining_tables'. The algorithm continues until it either constructs a
    complete optimal plan, or constructs an optimal plartial plan with size =
    search_depth.

    The final optimal plan is stored in 'join->best_positions'. The
    corresponding cost of the optimal plan is in 'join->best_read'.

  NOTES
    The procedure uses a recursive depth-first search where the depth of the
    recursion (and thus the exhaustiveness of the search) is controlled by the
    parameter 'search_depth'.

    The pseudocode below describes the algorithm of
    'best_extension_by_limited_search'. The worst-case complexity of this
    algorithm is O(N*N^search_depth/search_depth). When serch_depth >= N, then
    the complexity of greedy_search is O(N!).

    procedure best_extension_by_limited_search(
      pplan in,             // in, partial plan of tables-joined-so-far
      pplan_cost,           // in, cost of pplan
      remaining_tables,     // in, set of tables not referenced in pplan
      best_plan_so_far,     // in/out, best plan found so far
      best_plan_so_far_cost,// in/out, cost of best_plan_so_far
      search_depth)         // in, maximum size of the plans being considered
    {
      for each table T from remaining_tables
      {
        // Calculate the cost of using table T as above
        cost = complex-series-of-calculations;

        // Add the cost to the cost so far.
        pplan_cost+= cost;

        if (pplan_cost >= best_plan_so_far_cost)
          // pplan_cost already too great, stop search
          continue;

        pplan= expand pplan by best_access_method;
        remaining_tables= remaining_tables - table T;
        if (remaining_tables is not an empty set
            and
            search_depth > 1)
        {
          best_extension_by_limited_search(pplan, pplan_cost,
                                           remaining_tables,
                                           best_plan_so_far,
                                           best_plan_so_far_cost,
                                           search_depth - 1);
        }
        else
        {
          best_plan_so_far_cost= pplan_cost;
          best_plan_so_far= pplan;
        }
      }
    }

  IMPLEMENTATION
    When 'best_extension_by_limited_search' is called for the first time,
    'join->best_read' must be set to the largest possible value (e.g. DBL_MAX).
    The actual implementation provides a way to optionally use pruning
    heuristic (controlled by the parameter 'prune_level') to reduce the search
    space by skipping some partial plans.
    The parameter 'search_depth' provides control over the recursion
    depth, and thus the size of the resulting optimal plan.

  RETURN
    None
*/

static void
best_extension_by_limited_search(JOIN      *join,
                                 table_map remaining_tables,
                                 uint      idx,
                                 double    record_count,
                                 double    read_time,
                                 uint      search_depth,
                                 uint      prune_level)
{
  THD *thd= join->thd;
  if (thd->killed)  // Abort
    return;

  DBUG_ENTER("best_extension_by_limited_search");

  /* 
     'join' is a partial plan with lower cost than the best plan so far,
     so continue expanding it further with the tables in 'remaining_tables'.
  */
  JOIN_TAB *s;
  double best_record_count= DBL_MAX;
  double best_read_time=    DBL_MAX;

  DBUG_EXECUTE("opt",
               print_plan(join, read_time, record_count, idx, "part_plan"););

  for (JOIN_TAB **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    table_map real_table_bit= s->table->map;
    if ((remaining_tables & real_table_bit) && !(remaining_tables & s->dependent))
    {
      double current_record_count, current_read_time;

      /* Find the best access method from 's' to the current partial plan */
      best_access_path(join, s, thd, remaining_tables, idx, record_count, read_time);
      /* Compute the cost of extending the plan with 's' */
      current_record_count= record_count * join->positions[idx].records_read;
      current_read_time=    read_time + join->positions[idx].read_time;

      /* Expand only partial plans with lower cost than the best QEP so far */
      if ((current_read_time +
           current_record_count / (double) TIME_FOR_COMPARE) >= join->best_read)
      {
        DBUG_EXECUTE("opt", print_plan(join, read_time, record_count, idx,
                                       "prune_by_cost"););
        continue;
      }

      /*
        Prune some less promising partial plans. This heuristic may miss
        the optimal QEPs, thus it results in a non-exhaustive search.
      */
      if (prune_level == 1)
      {
        if (best_record_count > current_record_count ||
            best_read_time > current_read_time ||
            idx == join->const_tables &&  // 's' is the first table in the QEP
            s->table == join->sort_by_table)
        {
          if (best_record_count >= current_record_count &&
              best_read_time >= current_read_time &&
              /* TODO: What is the reasoning behind this condition? */
              (!(s->key_dependent & remaining_tables) ||
               join->positions[idx].records_read < 2.0))
          {
            best_record_count= current_record_count;
            best_read_time=    current_read_time;
          }
        }
        else
        {
          DBUG_EXECUTE("opt", print_plan(join, read_time, record_count, idx,
                                         "pruned_by_heuristic"););
          continue;
        }
      }

      if ( (search_depth > 1) && (remaining_tables & ~real_table_bit) )
      { /* Recursively expand the current partial plan */
        swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
        best_extension_by_limited_search(join,
                                         remaining_tables & ~real_table_bit,
                                         idx + 1,
                                         current_record_count,
                                         current_read_time,
                                         search_depth - 1,
                                         prune_level);
        if (thd->killed)
          DBUG_VOID_RETURN;
        swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
      }
      else
      { /*
          'join' is either the best partial QEP with 'search_depth' relations,
          or the best complete QEP so far, whichever is smaller.
        */
        current_read_time+= current_record_count / (double) TIME_FOR_COMPARE;
        if (join->sort_by_table &&
            join->sort_by_table != join->positions[join->const_tables].table->table)
          /* We have to make a temp table */
          current_read_time+= current_record_count;
        if ((search_depth == 1) || (current_read_time < join->best_read))
        {
          memcpy((gptr) join->best_positions, (gptr) join->positions,
                 sizeof(POSITION) * (idx + 1));
          join->best_read= current_read_time - 0.001;
        }
        DBUG_EXECUTE("opt",
                     print_plan(join, current_read_time, current_record_count, idx, "full_plan"););
      }
    }
  }
  DBUG_VOID_RETURN;
}


/*
  TODO: this function is here only temporarily until 'greedy_search' is
  tested and accepted.
*/
static void
find_best(JOIN *join,table_map rest_tables,uint idx,double record_count,
	  double read_time)
{
  ha_rows rec;
  double tmp;
  THD *thd= join->thd;

  if (!rest_tables)
  {
    DBUG_PRINT("best",("read_time: %g  record_count: %g",read_time,
		       record_count));

    read_time+=record_count/(double) TIME_FOR_COMPARE;
    if (join->sort_by_table &&
	join->sort_by_table !=
	join->positions[join->const_tables].table->table)
      read_time+=record_count;			// We have to make a temp table
    if (read_time < join->best_read)
    {
      memcpy((gptr) join->best_positions,(gptr) join->positions,
	     sizeof(POSITION)*idx);
      join->best_read= read_time - 0.001;
    }
    return;
  }
  if (read_time+record_count/(double) TIME_FOR_COMPARE >= join->best_read)
    return;					/* Found better before */

  JOIN_TAB *s;
  double best_record_count=DBL_MAX,best_read_time=DBL_MAX;
  for (JOIN_TAB **pos=join->best_ref+idx ; (s=*pos) ; pos++)
  {
    table_map real_table_bit=s->table->map;
    if ((rest_tables & real_table_bit) && !(rest_tables & s->dependent))
    {
      double best,best_time,records;
      best=best_time=records=DBL_MAX;
      KEYUSE *best_key=0;
      uint best_max_key_part=0;
      my_bool found_constraint= 0;

      if (s->keyuse)
      {						/* Use key if possible */
	TABLE *table=s->table;
	KEYUSE *keyuse,*start_key=0;
	double best_records=DBL_MAX;
	uint max_key_part=0;

	/* Test how we can use keys */
	rec= s->records/MATCHING_ROWS_IN_OTHER_TABLE;  // Assumed records/key
	for (keyuse=s->keyuse ; keyuse->table == table ;)
	{
	  key_part_map found_part=0;
	  table_map found_ref=0;
	  uint key=keyuse->key;
	  KEY *keyinfo=table->key_info+key;
          bool ft_key=(keyuse->keypart == FT_KEYPART);
	  uint found_ref_or_null= 0;

	  /* Calculate how many key segments of the current key we can use */
	  start_key=keyuse;
	  do
	  {
            uint keypart=keyuse->keypart;
            table_map best_part_found_ref= 0;
            double best_prev_record_reads= DBL_MAX;
	    do
	    {
	      if (!(rest_tables & keyuse->used_tables) &&
		  !(found_ref_or_null & keyuse->optimize))
	      {
		found_part|=keyuse->keypart_map;
                double tmp= prev_record_reads(join,
					      (found_ref |
					       keyuse->used_tables));
                if (tmp < best_prev_record_reads)
                {
                  best_part_found_ref= keyuse->used_tables;
                  best_prev_record_reads= tmp;
                }
		if (rec > keyuse->ref_table_rows)
		  rec= keyuse->ref_table_rows;
		/*
		  If there is one 'key_column IS NULL' expression, we can
		  use this ref_or_null optimisation of this field
		*/
		found_ref_or_null|= (keyuse->optimize &
				     KEY_OPTIMIZE_REF_OR_NULL);
              }
	      keyuse++;
	    } while (keyuse->table == table && keyuse->key == key &&
		     keyuse->keypart == keypart);
	    found_ref|= best_part_found_ref;
	  } while (keyuse->table == table && keyuse->key == key);

	  /*
	    Assume that that each key matches a proportional part of table.
	  */
          if (!found_part && !ft_key)
	    continue;				// Nothing usable found
	  if (rec < MATCHING_ROWS_IN_OTHER_TABLE)
	    rec= MATCHING_ROWS_IN_OTHER_TABLE;	// Fix for small tables

          /*
	    ft-keys require special treatment
          */
          if (ft_key)
          {
            /*
	      Really, there should be records=0.0 (yes!)
	      but 1.0 would be probably safer
            */
            tmp=prev_record_reads(join,found_ref);
            records=1.0;
          }
          else
          {
	  found_constraint= 1;
	  /*
	    Check if we found full key
	  */
	  if (found_part == PREV_BITS(uint,keyinfo->key_parts) &&
	      !found_ref_or_null)
	  {				/* use eq key */
	    max_key_part= (uint) ~0;
	    if ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY |
				   HA_END_SPACE_KEY)) == HA_NOSAME)
	    {
	      tmp=prev_record_reads(join,found_ref);
	      records=1.0;
	    }
	    else
	    {
	      if (!found_ref)
	      {					// We found a const key
		if (table->quick_keys.is_set(key))
		  records= (double) table->quick_rows[key];
		else
		{
		  /* quick_range couldn't use key! */
		  records= (double) s->records/rec;
		}
	      }
	      else
	      {
		if (!(records=keyinfo->rec_per_key[keyinfo->key_parts-1]))
		{				// Prefere longer keys
		  records=
		    ((double) s->records / (double) rec *
		     (1.0 +
		      ((double) (table->s->max_key_length-keyinfo->key_length) /
		       (double) table->s->max_key_length)));
		  if (records < 2.0)
		    records=2.0;		// Can't be as good as a unique
		}
	      }
	      /* Limit the number of matched rows */
	      tmp= records;
	      set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
	      if (table->used_keys.is_set(key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->file->block_size/2/
		  (keyinfo->key_length+table->file->ref_length)+1;
		tmp=record_count*(tmp+keys_per_block-1)/keys_per_block;
	      }
	      else
		tmp=record_count*min(tmp,s->worst_seeks);
	    }
	  }
	  else
	  {
	    /*
	      Use as much key-parts as possible and a uniq key is better
	      than a not unique key
	      Set tmp to (previous record count) * (records / combination)
	    */
	    if ((found_part & 1) &&
		(!(table->file->index_flags(key,0,0) & HA_ONLY_WHOLE_INDEX) ||
		 found_part == PREV_BITS(uint,keyinfo->key_parts)))
	    {
	      max_key_part=max_part_bit(found_part);
	      /*
		Check if quick_range could determinate how many rows we
		will match
	      */
	      if (table->quick_keys.is_set(key) &&
		  table->quick_key_parts[key] == max_key_part)
		tmp=records= (double) table->quick_rows[key];
	      else
	      {
		/* Check if we have statistic about the distribution */
		if ((records=keyinfo->rec_per_key[max_key_part-1]))
		  tmp=records;
		else
		{
		  /*
		    Assume that the first key part matches 1% of the file
		    and that the whole key matches 10 (duplicates) or 1
		    (unique) records.
		    Assume also that more key matches proportionally more
		    records
		    This gives the formula:
		    records= (x * (b-a) + a*c-b)/(c-1)

		    b = records matched by whole key
		    a = records matched by first key part (10% of all records?)
		    c = number of key parts in key
		    x = used key parts (1 <= x <= c)
		  */
		  double rec_per_key;
                  rec_per_key= keyinfo->rec_per_key[keyinfo->key_parts-1] ?
		    (double) keyinfo->rec_per_key[keyinfo->key_parts-1] :
		    (double) s->records/rec+1;   
		  if (!s->records)
		    tmp=0;
		  else if (rec_per_key/(double) s->records >= 0.01)
		    tmp=rec_per_key;
		  else
		  {
		    double a=s->records*0.01;
		    tmp=(max_key_part * (rec_per_key - a) +
			 a*keyinfo->key_parts - rec_per_key)/
		      (keyinfo->key_parts-1);
		    set_if_bigger(tmp,1.0);
		  }
		  records=(ulong) tmp;
		}
		/*
		  If quick_select was used on a part of this key, we know
		  the maximum number of rows that the key can match.
		*/
		if (table->quick_keys.is_set(key) &&
		    table->quick_key_parts[key] <= max_key_part &&
		    records > (double) table->quick_rows[key])
		  tmp= records= (double) table->quick_rows[key];
		else if (found_ref_or_null)
		{
		  /* We need to do two key searches to find key */
		  tmp*= 2.0;
		  records*= 2.0;
		}
	      }
	      /* Limit the number of matched rows */
	      set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
	      if (table->used_keys.is_set(key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->file->block_size/2/
		  (keyinfo->key_length+table->file->ref_length)+1;
		tmp=record_count*(tmp+keys_per_block-1)/keys_per_block;
	      }
	      else
		tmp=record_count*min(tmp,s->worst_seeks);
	    }
	    else
	      tmp=best_time;			// Do nothing
	  }
          } /* not ft_key */
	  if (tmp < best_time - records/(double) TIME_FOR_COMPARE)
	  {
	    best_time=tmp + records/(double) TIME_FOR_COMPARE;
	    best=tmp;
	    best_records=records;
	    best_key=start_key;
	    best_max_key_part=max_key_part;
	  }
	}
	records=best_records;
      }

      /*
	Don't test table scan if it can't be better.
	Prefer key lookup if we would use the same key for scanning.

	Don't do a table scan on InnoDB tables, if we can read the used
	parts of the row from any of the used index.
	This is because table scans uses index and we would not win
	anything by using a table scan.
      */
      if ((records >= s->found_records || best > s->read_time) &&
	  !(s->quick && best_key && s->quick->index == best_key->key &&
	    best_max_key_part >= s->table->quick_key_parts[best_key->key]) &&
	  !((s->table->file->table_flags() & HA_TABLE_SCAN_ON_INDEX) &&
	    ! s->table->used_keys.is_clear_all() && best_key) &&
	  !(s->table->force_index && best_key))
      {						// Check full join
        ha_rows rnd_records= s->found_records;
        /*
          If there is a restriction on the table, assume that 25% of the
          rows can be skipped on next part.
          This is to force tables that this table depends on before this
          table
        */
        if (found_constraint)
          rnd_records-= rnd_records/4;

        /*
          Range optimizer never proposes a RANGE if it isn't better
          than FULL: so if RANGE is present, it's always preferred to FULL.
          Here we estimate its cost.
        */
        if (s->quick)
        {
          /*
            For each record we:
             - read record range through 'quick'
             - skip rows which does not satisfy WHERE constraints
           */
          tmp= record_count *
               (s->quick->read_time +
               (s->found_records - rnd_records)/(double) TIME_FOR_COMPARE);
        }
        else
        {
          /* Estimate cost of reading table. */
          tmp= s->table->file->scan_time();
          if (s->table->map  & join->outer_join)      // Can't use join cache
          {
            /*
              For each record we have to:
              - read the whole table record 
              - skip rows which does not satisfy join condition
            */
            tmp= record_count *
                 (tmp +     
                 (s->records - rnd_records)/(double) TIME_FOR_COMPARE);
          }
          else
          {
            /* We read the table as many times as join buffer becomes full. */
            tmp*= (1.0 + floor((double) cache_record_length(join,idx) *
                               record_count /
                               (double) thd->variables.join_buff_size));
            /* 
              We don't make full cartesian product between rows in the scanned
              table and existing records because we skip all rows from the
              scanned table, which does not satisfy join condition when 
              we read the table (see flush_cached_records for details). Here we
              take into account cost to read and skip these records.
            */
            tmp+= (s->records - rnd_records)/(double) TIME_FOR_COMPARE;
          }
        }

        /*
          We estimate the cost of evaluating WHERE clause for found records
          as record_count * rnd_records / TIME_FOR_COMPARE. This cost plus
          tmp give us total cost of using TABLE SCAN
        */
	if (best == DBL_MAX ||
	    (tmp  + record_count/(double) TIME_FOR_COMPARE*rnd_records <
	     best + record_count/(double) TIME_FOR_COMPARE*records))
	{
	  /*
	    If the table has a range (s->quick is set) make_join_select()
	    will ensure that this will be used
	  */
	  best=tmp;
	  records= rows2double(rnd_records);
	  best_key=0;
	}
      }
      join->positions[idx].records_read= records;
      join->positions[idx].key=best_key;
      join->positions[idx].table= s;
      if (!best_key && idx == join->const_tables &&
	  s->table == join->sort_by_table &&
	  join->unit->select_limit_cnt >= records)
	join->sort_by_table= (TABLE*) 1;	// Must use temporary table

     /*
	Go to the next level only if there hasn't been a better key on
	this level! This will cut down the search for a lot simple cases!
       */
      double current_record_count=record_count*records;
      double current_read_time=read_time+best;
      if (best_record_count > current_record_count ||
	  best_read_time > current_read_time ||
	  idx == join->const_tables && s->table == join->sort_by_table)
      {
	if (best_record_count >= current_record_count &&
	    best_read_time >= current_read_time &&
	    (!(s->key_dependent & rest_tables) || records < 2.0))
	{
	  best_record_count=current_record_count;
	  best_read_time=current_read_time;
	}
	swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
	find_best(join,rest_tables & ~real_table_bit,idx+1,
		  current_record_count,current_read_time);
        if (thd->killed)
          return;
	swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
      }
      if (join->select_options & SELECT_STRAIGHT_JOIN)
	break;				// Don't test all combinations
    }
  }
}


/*
  Find how much space the prevous read not const tables takes in cache
*/

static void calc_used_field_length(THD *thd, JOIN_TAB *join_tab)
{
  uint null_fields,blobs,fields,rec_length;
  null_fields=blobs=fields=rec_length=0;

  Field **f_ptr,*field;
  for (f_ptr=join_tab->table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (field->query_id == thd->query_id)
    {
      uint flags=field->flags;
      fields++;
      rec_length+=field->pack_length();
      if (flags & BLOB_FLAG)
	blobs++;
      if (!(flags & NOT_NULL_FLAG))
	null_fields++;
    }
  }
  if (null_fields)
    rec_length+=(join_tab->table->s->null_fields+7)/8;
  if (join_tab->table->maybe_null)
    rec_length+=sizeof(my_bool);
  if (blobs)
  {
    uint blob_length=(uint) (join_tab->table->file->mean_rec_length-
			     (join_tab->table->s->reclength- rec_length));
    rec_length+=(uint) max(4,blob_length);
  }
  join_tab->used_fields=fields;
  join_tab->used_fieldlength=rec_length;
  join_tab->used_blobs=blobs;
}


static uint
cache_record_length(JOIN *join,uint idx)
{
  uint length=0;
  JOIN_TAB **pos,**end;
  THD *thd=join->thd;

  for (pos=join->best_ref+join->const_tables,end=join->best_ref+idx ;
       pos != end ;
       pos++)
  {
    JOIN_TAB *join_tab= *pos;
    if (!join_tab->used_fieldlength)		/* Not calced yet */
      calc_used_field_length(thd, join_tab);
    length+=join_tab->used_fieldlength;
  }
  return length;
}


static double
prev_record_reads(JOIN *join,table_map found_ref)
{
  double found=1.0;
  found_ref&= ~OUTER_REF_TABLE_BIT;
  for (POSITION *pos=join->positions ; found_ref ; pos++)
  {
    if (pos->table->table->map & found_ref)
    {
      found_ref&= ~pos->table->table->map;
      found*=pos->records_read;
    }
  }
  return found;
}


/*****************************************************************************
  Set up join struct according to best position.
*****************************************************************************/

static bool
get_best_combination(JOIN *join)
{
  uint i,tablenr;
  table_map used_tables;
  JOIN_TAB *join_tab,*j;
  KEYUSE *keyuse;
  uint table_count;
  THD *thd=join->thd;
  DBUG_ENTER("get_best_combination");

  table_count=join->tables;
  if (!(join->join_tab=join_tab=
	(JOIN_TAB*) thd->alloc(sizeof(JOIN_TAB)*table_count)))
    DBUG_RETURN(TRUE);

  join->full_join=0;

  used_tables= OUTER_REF_TABLE_BIT;		// Outer row is already read
  for (j=join_tab, tablenr=0 ; tablenr < table_count ; tablenr++,j++)
  {
    TABLE *form;
    *j= *join->best_positions[tablenr].table;
    form=join->table[tablenr]=j->table;
    used_tables|= form->map;
    form->reginfo.join_tab=j;
    if (!*j->on_expr_ref)
      form->reginfo.not_exists_optimize=0;	// Only with LEFT JOIN
    DBUG_PRINT("info",("type: %d", j->type));
    if (j->type == JT_CONST)
      continue;					// Handled in make_join_stat..

    j->ref.key = -1;
    j->ref.key_parts=0;

    if (j->type == JT_SYSTEM)
      continue;
    if (j->keys.is_clear_all() || !(keyuse= join->best_positions[tablenr].key))
    {
      j->type=JT_ALL;
      if (tablenr != join->const_tables)
	join->full_join=1;
    }
    else if (create_ref_for_key(join, j, keyuse, used_tables))
      DBUG_RETURN(TRUE);                        // Something went wrong
  }

  for (i=0 ; i < table_count ; i++)
    join->map2table[join->join_tab[i].table->tablenr]=join->join_tab+i;
  update_depend_map(join);
  DBUG_RETURN(0);
}


static bool create_ref_for_key(JOIN *join, JOIN_TAB *j, KEYUSE *org_keyuse,
			       table_map used_tables)
{
  KEYUSE *keyuse=org_keyuse;
  bool ftkey=(keyuse->keypart == FT_KEYPART);
  THD  *thd= join->thd;
  uint keyparts,length,key;
  TABLE *table;
  KEY *keyinfo;
  DBUG_ENTER("create_ref_for_key");

  /*  Use best key from find_best */
  table=j->table;
  key=keyuse->key;
  keyinfo=table->key_info+key;

  if (ftkey)
  {
    Item_func_match *ifm=(Item_func_match *)keyuse->val;

    length=0;
    keyparts=1;
    ifm->join_key=1;
  }
  else
  {
    keyparts=length=0;
    uint found_part_ref_or_null= 0;
    /*
      Calculate length for the used key
      Stop if there is a missing key part or when we find second key_part
      with KEY_OPTIMIZE_REF_OR_NULL
    */
    do
    {
      if (!(~used_tables & keyuse->used_tables))
      {
	if (keyparts == keyuse->keypart &&
	    !(found_part_ref_or_null & keyuse->optimize))
	{
	  keyparts++;
	  length+= keyinfo->key_part[keyuse->keypart].store_length;
	  found_part_ref_or_null|= keyuse->optimize;
	}
      }
      keyuse++;
    } while (keyuse->table == table && keyuse->key == key);
  } /* not ftkey */

  /* set up fieldref */
  keyinfo=table->key_info+key;
  j->ref.key_parts=keyparts;
  j->ref.key_length=length;
  j->ref.key=(int) key;
  if (!(j->ref.key_buff= (byte*) thd->calloc(ALIGN_SIZE(length)*2)) ||
      !(j->ref.key_copy= (store_key**) thd->alloc((sizeof(store_key*) *
						   (keyparts+1)))) ||
      !(j->ref.items=    (Item**) thd->alloc(sizeof(Item*)*keyparts)))
  {
    DBUG_RETURN(TRUE);
  }
  j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
  j->ref.key_err=1;
  j->ref.null_rejecting= 0;
  keyuse=org_keyuse;

  store_key **ref_key= j->ref.key_copy;
  byte *key_buff=j->ref.key_buff, *null_ref_key= 0;
  bool keyuse_uses_no_tables= TRUE;
  if (ftkey)
  {
    j->ref.items[0]=((Item_func*)(keyuse->val))->key_item();
    if (keyuse->used_tables)
      DBUG_RETURN(TRUE);                        // not supported yet. SerG

    j->type=JT_FT;
  }
  else
  {
    uint i;
    for (i=0 ; i < keyparts ; keyuse++,i++)
    {
      while (keyuse->keypart != i ||
	     ((~used_tables) & keyuse->used_tables))
	keyuse++;				/* Skip other parts */

      uint maybe_null= test(keyinfo->key_part[i].null_bit);
      j->ref.items[i]=keyuse->val;		// Save for cond removal
      if (keyuse->null_rejecting) 
        j->ref.null_rejecting |= 1 << i;
      keyuse_uses_no_tables= keyuse_uses_no_tables && !keyuse->used_tables;
      if (!keyuse->used_tables &&
	  !(join->select_options & SELECT_DESCRIBE))
      {					// Compare against constant
	store_key_item tmp(thd, keyinfo->key_part[i].field,
                           (char*)key_buff + maybe_null,
                           maybe_null ?  (char*) key_buff : 0,
                           keyinfo->key_part[i].length, keyuse->val);
	if (thd->is_fatal_error)
	  DBUG_RETURN(TRUE);
	tmp.copy();
      }
      else
	*ref_key++= get_store_key(thd,
				  keyuse,join->const_table_map,
				  &keyinfo->key_part[i],
				  (char*) key_buff,maybe_null);
      /*
	Remember if we are going to use REF_OR_NULL
	But only if field _really_ can be null i.e. we force JT_REF
	instead of JT_REF_OR_NULL in case if field can't be null
      */
      if ((keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL) && maybe_null)
	null_ref_key= key_buff;
      key_buff+=keyinfo->key_part[i].store_length;
    }
  } /* not ftkey */
  *ref_key=0;				// end_marker
  if (j->type == JT_FT)
    DBUG_RETURN(0);
  if (j->type == JT_CONST)
    j->table->const_table= 1;
  else if (((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY |
			       HA_END_SPACE_KEY)) != HA_NOSAME) ||
	   keyparts != keyinfo->key_parts || null_ref_key)
  {
    /* Must read with repeat */
    j->type= null_ref_key ? JT_REF_OR_NULL : JT_REF;
    j->ref.null_ref_key= null_ref_key;
  }
  else if (keyuse_uses_no_tables)
  {
    /*
      This happen if we are using a constant expression in the ON part
      of an LEFT JOIN.
      SELECT * FROM a LEFT JOIN b ON b.key=30
      Here we should not mark the table as a 'const' as a field may
      have a 'normal' value or a NULL value.
    */
    j->type=JT_CONST;
  }
  else
    j->type=JT_EQ_REF;
  DBUG_RETURN(0);
}



static store_key *
get_store_key(THD *thd, KEYUSE *keyuse, table_map used_tables,
	      KEY_PART_INFO *key_part, char *key_buff, uint maybe_null)
{
  if (!((~used_tables) & keyuse->used_tables))		// if const item
  {
    return new store_key_const_item(thd,
				    key_part->field,
				    key_buff + maybe_null,
				    maybe_null ? key_buff : 0,
				    key_part->length,
				    keyuse->val);
  }
  else if (keyuse->val->type() == Item::FIELD_ITEM)
    return new store_key_field(thd,
			       key_part->field,
			       key_buff + maybe_null,
			       maybe_null ? key_buff : 0,
			       key_part->length,
			       ((Item_field*) keyuse->val)->field,
			       keyuse->val->full_name());
  return new store_key_item(thd,
			    key_part->field,
			    key_buff + maybe_null,
			    maybe_null ? key_buff : 0,
			    key_part->length,
			    keyuse->val);
}

/*
  This function is only called for const items on fields which are keys
  returns 1 if there was some conversion made when the field was stored.
*/

bool
store_val_in_field(Field *field,Item *item)
{
  bool error;
  THD *thd= field->table->in_use;
  ha_rows cuted_fields=thd->cuted_fields;
  /*
    we should restore old value of count_cuted_fields because
    store_val_in_field can be called from mysql_insert 
    with select_insert, which make count_cuted_fields= 1
   */
  enum_check_fields old_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_WARN;
  error= item->save_in_field(field, 1);
  thd->count_cuted_fields= old_count_cuted_fields;
  return error || cuted_fields != thd->cuted_fields;
}


static bool
make_simple_join(JOIN *join,TABLE *tmp_table)
{
  TABLE **tableptr;
  JOIN_TAB *join_tab;
  DBUG_ENTER("make_simple_join");

  if (!(tableptr=(TABLE**) join->thd->alloc(sizeof(TABLE*))) ||
      !(join_tab=(JOIN_TAB*) join->thd->alloc(sizeof(JOIN_TAB))))
    DBUG_RETURN(TRUE);
  join->join_tab=join_tab;
  join->table=tableptr; tableptr[0]=tmp_table;
  join->tables=1;
  join->const_tables=0;
  join->const_table_map=0;
  join->tmp_table_param.field_count= join->tmp_table_param.sum_func_count=
    join->tmp_table_param.func_count=0;
  join->tmp_table_param.copy_field=join->tmp_table_param.copy_field_end=0;
  join->first_record=join->sort_and_group=0;
  join->send_records=(ha_rows) 0;
  join->group=0;
  join->row_limit=join->unit->select_limit_cnt;
  join->do_send_rows = (join->row_limit) ? 1 : 0;

  join_tab->cache.buff=0;			/* No caching */
  join_tab->table=tmp_table;
  join_tab->select=0;
  join_tab->select_cond=0;
  join_tab->quick=0;
  join_tab->type= JT_ALL;			/* Map through all records */
  join_tab->keys.init();
  join_tab->keys.set_all();                     /* test everything in quick */
  join_tab->info=0;
  join_tab->on_expr_ref=0;
  join_tab->last_inner= 0;
  join_tab->first_unmatched= 0;
  join_tab->ref.key = -1;
  join_tab->not_used_in_distinct=0;
  join_tab->read_first_record= join_init_read_record;
  join_tab->join=join;
  join_tab->ref.key_parts= 0;
  bzero((char*) &join_tab->read_record,sizeof(join_tab->read_record));
  tmp_table->status=0;
  tmp_table->null_row=0;
  DBUG_RETURN(FALSE);
}


inline void add_cond_and_fix(Item **e1, Item *e2)
{
  if (*e1)
  {
    Item *res;
    if ((res= new Item_cond_and(*e1, e2)))
    {
      *e1= res;
      res->quick_fix_field();
    }
  }
  else
    *e1= e2;
}


/*
  Add to join_tab->select_cond[i] "table.field IS NOT NULL" conditions we've
  inferred from ref/eq_ref access performed.

  SYNOPSIS
    add_not_null_conds()
      join  Join to process

  NOTES
    This function is a part of "Early NULL-values filtering for ref access"
    optimization.

    Example of this optimization:
      For query SELECT * FROM t1,t2 WHERE t2.key=t1.field
      and plan " any-access(t1), ref(t2.key=t1.field) "
      add "t1.field IS NOT NULL" to t1's table condition.
    Description of the optimization:
    
      We look through equalities choosen to perform ref/eq_ref access,
      pick equalities that have form "tbl.part_of_key = othertbl.field"
      (where othertbl is a non-const table and othertbl.field may be NULL)
      and add them to conditions on correspoding tables (othertbl in this
      example).

      Exception from that is the case when referred_tab->join != join.
      I.e. don't add NOT NULL constraints from any embedded subquery.
      Consider this query:
      SELECT A.f2 FROM t1 LEFT JOIN t2 A ON A.f2 = f1
      WHERE A.f3=(SELECT MIN(f3) FROM  t2 C WHERE A.f4 = C.f4) OR A.f3 IS NULL;
      Here condition A.f3 IS NOT NULL is going to be added to the WHERE
      condition of the embedding query.
      Another example:
      SELECT * FROM t10, t11 WHERE (t10.a < 10 OR t10.a IS NULL)
      AND t11.b <=> t10.b AND (t11.a = (SELECT MAX(a) FROM t12
      WHERE t12.b = t10.a ));
      Here condition t10.a IS NOT NULL is going to be added.
      In both cases addition of NOT NULL condition will erroneously reject
      some rows of the result set.
      referred_tab->join != join constraint would disallow such additions.

      This optimization doesn't affect the choices that ref, range, or join
      optimizer make. This was intentional because this was added after 4.1
      was GA.
      
    Implementation overview
      1. update_ref_and_keys() accumulates info about null-rejecting
         predicates in in KEY_FIELD::null_rejecting
      1.1 add_key_part saves these to KEYUSE.
      2. create_ref_for_key copies them to TABLE_REF.
      3. add_not_null_conds adds "x IS NOT NULL" to join_tab->select_cond of
         appropiate JOIN_TAB members.
*/

static void add_not_null_conds(JOIN *join)
{
  DBUG_ENTER("add_not_null_conds");
  for (uint i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    if ((tab->type == JT_REF || tab->type == JT_REF_OR_NULL) &&
         !tab->table->maybe_null)
    {
      for (uint keypart= 0; keypart < tab->ref.key_parts; keypart++)
      {
        if (tab->ref.null_rejecting & (1 << keypart))
        {
          Item *item= tab->ref.items[keypart];
          Item *notnull;
          DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
          Item_field *not_null_item= (Item_field*)item;
          JOIN_TAB *referred_tab= not_null_item->field->table->reginfo.join_tab;
          /*
            For UPDATE queries such as:
            UPDATE t1 SET t1.f2=(SELECT MAX(t2.f4) FROM t2 WHERE t2.f3=t1.f1);
            not_null_item is the t1.f1, but it's referred_tab is 0.
          */
          if (!referred_tab || referred_tab->join != join)
            continue;
          if (!(notnull= new Item_func_isnotnull(not_null_item)))
            DBUG_VOID_RETURN;
          /*
            We need to do full fix_fields() call here in order to have correct
            notnull->const_item(). This is needed e.g. by test_quick_select 
            when it is called from make_join_select after this function is 
            called.
          */
          if (notnull->fix_fields(join->thd, &notnull))
            DBUG_VOID_RETURN;
          DBUG_EXECUTE("where",print_where(notnull,
                                           referred_tab->table->alias););
          add_cond_and_fix(&referred_tab->select_cond, notnull);
        }
      }
    }
  }
  DBUG_VOID_RETURN;
}

/*
  Build a predicate guarded by match variables for embedding outer joins

  SYNOPSIS
    add_found_match_trig_cond()
    tab       the first inner table for most nested outer join
    cond      the predicate to be guarded
    root_tab  the first inner table to stop

  DESCRIPTION
    The function recursively adds guards for predicate cond
    assending from tab to the first inner table  next embedding
    nested outer join and so on until it reaches root_tab
    (root_tab can be 0).

  RETURN VALUE
    pointer to the guarded predicate, if success
    0, otherwise
*/ 

static COND*
add_found_match_trig_cond(JOIN_TAB *tab, COND *cond, JOIN_TAB *root_tab)
{
  COND *tmp;
  if (tab == root_tab || !cond)
    return cond;
  if ((tmp= add_found_match_trig_cond(tab->first_upper, cond, root_tab)))
  {
    tmp= new Item_func_trig_cond(tmp, &tab->found);
  }
  if (tmp)
  {
    tmp->quick_fix_field();
    tmp->update_used_tables();
  }
  return tmp;
}


/*
   Fill in outer join related info for the execution plan structure

  SYNOPSIS
    make_outerjoin_info()
    join - reference to the info fully describing the query

  DESCRIPTION
    For each outer join operation left after simplification of the
    original query the function set up the following pointers in the linear
    structure join->join_tab representing the selected execution plan.
    The first inner table t0 for the operation is set to refer to the last
    inner table tk through the field t0->last_inner.
    Any inner table ti for the operation are set to refer to the first
    inner table ti->first_inner.
    The first inner table t0 for the operation is set to refer to the
    first inner table of the embedding outer join operation, if there is any,
    through the field t0->first_upper.
    The on expression for the outer join operation is attached to the
    corresponding first inner table through the field t0->on_expr_ref.
    Here ti are structures of the JOIN_TAB type.

  EXAMPLE
    For the query: 
      SELECT * FROM t1
                    LEFT JOIN
                    (t2, t3 LEFT JOIN t4 ON t3.a=t4.a)
                    ON (t1.a=t2.a AND t1.b=t3.b)
        WHERE t1.c > 5,
    given the execution plan with the table order t1,t2,t3,t4
    is selected, the following references will be set;
    t4->last_inner=[t4], t4->first_inner=[t4], t4->first_upper=[t2]
    t2->last_inner=[t4], t2->first_inner=t3->first_inner=[t2],
    on expression (t1.a=t2.a AND t1.b=t3.b) will be attached to 
    *t2->on_expr_ref, while t3.a=t4.a will be attached to *t4->on_expr_ref.
            
  NOTES
    The function assumes that the simplification procedure has been
    already applied to the join query (see simplify_joins).
    This function can be called only after the execution plan
    has been chosen.
*/
 
static void
make_outerjoin_info(JOIN *join)
{
  DBUG_ENTER("make_outerjoin_info");
  for (uint i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;
    TABLE_LIST *tbl= table->pos_in_table_list;
    TABLE_LIST *embedding= tbl->embedding;

    if (tbl->outer_join)
    {
      /* 
        Table tab is the only one inner table for outer join.
        (Like table t4 for the table reference t3 LEFT JOIN t4 ON t3.a=t4.a
        is in the query above.)
      */
      tab->last_inner= tab->first_inner= tab;
      tab->on_expr_ref= &tbl->on_expr;
      tab->cond_equal= tbl->cond_equal;
      if (embedding)
        tab->first_upper= embedding->nested_join->first_nested;
    }    
    for ( ; embedding ; embedding= embedding->embedding)
    {
      NESTED_JOIN *nested_join= embedding->nested_join;
      if (!nested_join->counter)
      {
        /* 
          Table tab is the first inner table for nested_join.
          Save reference to it in the nested join structure.
        */ 
        nested_join->first_nested= tab;
        tab->on_expr_ref= &embedding->on_expr;
        tab->cond_equal= tbl->cond_equal;
        if (embedding->embedding)
          tab->first_upper= embedding->embedding->nested_join->first_nested;
      }
      if (!tab->first_inner)  
        tab->first_inner= nested_join->first_nested;
      if (++nested_join->counter < nested_join->join_list.elements)
        break;
      /* Table tab is the last inner table for nested join. */
      nested_join->first_nested->last_inner= tab;
    }
  }
  DBUG_VOID_RETURN;
}


static bool
make_join_select(JOIN *join,SQL_SELECT *select,COND *cond)
{
  THD *thd= join->thd;
  DBUG_ENTER("make_join_select");
  if (select)
  {
    add_not_null_conds(join);
    table_map used_tables;
    if (cond)                /* Because of QUICK_GROUP_MIN_MAX_SELECT */
    {                        /* there may be a select without a cond. */    
      if (join->tables > 1)
        cond->update_used_tables();		// Tablenr may have changed
      if (join->const_tables == join->tables &&
	  thd->lex->current_select->master_unit() ==
	  &thd->lex->unit)		// not upper level SELECT
        join->const_table_map|=RAND_TABLE_BIT;
      {						// Check const tables
        COND *const_cond=
	  make_cond_for_table(cond,join->const_table_map,(table_map) 0);
        DBUG_EXECUTE("where",print_where(const_cond,"constants"););
        for (JOIN_TAB *tab= join->join_tab+join->const_tables;
             tab < join->join_tab+join->tables ; tab++)
        {
          if (*tab->on_expr_ref)
          {
            JOIN_TAB *cond_tab= tab->first_inner;
            COND *tmp= make_cond_for_table(*tab->on_expr_ref,
                                           join->const_table_map,
                                         (  table_map) 0);
            if (!tmp)
              continue;
            tmp= new Item_func_trig_cond(tmp, &cond_tab->not_null_compl);
            if (!tmp)
              DBUG_RETURN(1);
            tmp->quick_fix_field();
            cond_tab->select_cond= !cond_tab->select_cond ? tmp :
	                            new Item_cond_and(cond_tab->select_cond,tmp);
            if (!cond_tab->select_cond)
	      DBUG_RETURN(1);
            cond_tab->select_cond->quick_fix_field();
          }       
        }
        if (const_cond && !const_cond->val_int())
        {
	  DBUG_PRINT("info",("Found impossible WHERE condition"));
	  DBUG_RETURN(1);	 // Impossible const condition
        }
      }
    }
    used_tables=((select->const_tables=join->const_table_map) |
		 OUTER_REF_TABLE_BIT | RAND_TABLE_BIT);
    for (uint i=join->const_tables ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      JOIN_TAB *first_inner_tab= tab->first_inner; 
      table_map current_map= tab->table->map;
      bool use_quick_range=0;
      COND *tmp;

      /*
	Following force including random expression in last table condition.
	It solve problem with select like SELECT * FROM t1 WHERE rand() > 0.5
      */
      if (i == join->tables-1)
	current_map|= OUTER_REF_TABLE_BIT | RAND_TABLE_BIT;
      used_tables|=current_map;

      if (tab->type == JT_REF && tab->quick &&
	  (uint) tab->ref.key == tab->quick->index &&
	  tab->ref.key_length < tab->quick->max_used_key_length)
      {
	/* Range uses longer key;  Use this instead of ref on key */
	tab->type=JT_ALL;
	use_quick_range=1;
	tab->use_quick=1;
        tab->ref.key= -1;
	tab->ref.key_parts=0;		// Don't use ref key.
	join->best_positions[i].records_read= rows2double(tab->quick->records);
      }

      tmp= NULL;
      if (cond)
        tmp= make_cond_for_table(cond,used_tables,current_map);
      if (cond && !tmp && tab->quick)
      {						// Outer join
        if (tab->type != JT_ALL)
        {
          /*
            Don't use the quick method
            We come here in the case where we have 'key=constant' and
            the test is removed by make_cond_for_table()
          */
          delete tab->quick;
          tab->quick= 0;
        }
        else
        {
          /*
            Hack to handle the case where we only refer to a table
            in the ON part of an OUTER JOIN. In this case we want the code
            below to check if we should use 'quick' instead.
          */
          DBUG_PRINT("info", ("Item_int"));
          tmp= new Item_int((longlong) 1,1);	// Always true
          DBUG_PRINT("info", ("Item_int 0x%lx", (ulong)tmp));
        }

      }
      if (tmp || !cond)
      {
	DBUG_EXECUTE("where",print_where(tmp,tab->table->alias););
	SQL_SELECT *sel=tab->select=(SQL_SELECT*)
	  thd->memdup((gptr) select, sizeof(SQL_SELECT));
	if (!sel)
	  DBUG_RETURN(1);			// End of memory
        /*
          If tab is an inner table of an outer join operation,
          add a match guard to the pushed down predicate.
          The guard will turn the predicate on only after
          the first match for outer tables is encountered.
	*/        
        if (cond)
        {
          /*
            Because of QUICK_GROUP_MIN_MAX_SELECT there may be a select without
            a cond, so neutralize the hack above.
          */
          if (!(tmp= add_found_match_trig_cond(first_inner_tab, tmp, 0)))
            DBUG_RETURN(1);
          tab->select_cond=sel->cond=tmp;
          /* Push condition to storage engine if this is enabled
             and the condition is not guarded */
          tab->table->file->pushed_cond= NULL;
	  if (thd->variables.engine_condition_pushdown)
          {
            COND *push_cond= 
              make_cond_for_table(tmp, current_map, current_map);
            if (push_cond)
            {
              /* Push condition to handler */
              if (!tab->table->file->cond_push(push_cond))
                tab->table->file->pushed_cond= push_cond;
            }
          }
        }
        else
          tab->select_cond= sel->cond= NULL;

	sel->head=tab->table;
	DBUG_EXECUTE("where",print_where(tmp,tab->table->alias););
	if (tab->quick)
	{
	  /* Use quick key read if it's a constant and it's not used
	     with key reading */
	  if (tab->needed_reg.is_clear_all() && tab->type != JT_EQ_REF
	      && tab->type != JT_FT && (tab->type != JT_REF ||
               (uint) tab->ref.key == tab->quick->index))
	  {
	    sel->quick=tab->quick;		// Use value from get_quick_...
	    sel->quick_keys.clear_all();
	    sel->needed_reg.clear_all();
	  }
	  else
	  {
	    delete tab->quick;
	  }
	  tab->quick=0;
	}
	uint ref_key=(uint) sel->head->reginfo.join_tab->ref.key+1;
	if (i == join->const_tables && ref_key)
	{
	  if (!tab->const_keys.is_clear_all() &&
              tab->table->reginfo.impossible_range)
	    DBUG_RETURN(1);
	}
	else if (tab->type == JT_ALL && ! use_quick_range)
	{
	  if (!tab->const_keys.is_clear_all() &&
	      tab->table->reginfo.impossible_range)
	    DBUG_RETURN(1);				// Impossible range
	  /*
	    We plan to scan all rows.
	    Check again if we should use an index.
	    We could have used an column from a previous table in
	    the index if we are using limit and this is the first table
	  */

	  if (cond &&
              (!tab->keys.is_subset(tab->const_keys) && i > 0) ||
	      (!tab->const_keys.is_clear_all() && i == join->const_tables &&
	       join->unit->select_limit_cnt <
	       join->best_positions[i].records_read &&
	       !(join->select_options & OPTION_FOUND_ROWS)))
	  {
	    /* Join with outer join condition */
	    COND *orig_cond=sel->cond;
	    sel->cond= and_conds(sel->cond, *tab->on_expr_ref);

	    /*
              We can't call sel->cond->fix_fields,
              as it will break tab->on_expr if it's AND condition
              (fix_fields currently removes extra AND/OR levels).
              Yet attributes of the just built condition are not needed.
              Thus we call sel->cond->quick_fix_field for safety.
	    */
	    if (sel->cond && !sel->cond->fixed)
	      sel->cond->quick_fix_field();

	    if (sel->test_quick_select(thd, tab->keys,
				       used_tables & ~ current_map,
				       (join->select_options &
					OPTION_FOUND_ROWS ?
					HA_POS_ERROR :
					join->unit->select_limit_cnt), 0) < 0)
            {
	      /*
		Before reporting "Impossible WHERE" for the whole query
		we have to check isn't it only "impossible ON" instead
	      */
              sel->cond=orig_cond;
              if (!*tab->on_expr_ref ||
                  sel->test_quick_select(thd, tab->keys,
                                         used_tables & ~ current_map,
                                         (join->select_options &
                                          OPTION_FOUND_ROWS ?
                                          HA_POS_ERROR :
                                          join->unit->select_limit_cnt),0) < 0)
		DBUG_RETURN(1);			// Impossible WHERE
            }
            else
	      sel->cond=orig_cond;

	    /* Fix for EXPLAIN */
	    if (sel->quick)
	      join->best_positions[i].records_read= sel->quick->records;
	  }
	  else
	  {
	    sel->needed_reg=tab->needed_reg;
	    sel->quick_keys.clear_all();
	  }
	  if (!sel->quick_keys.is_subset(tab->checked_keys) ||
              !sel->needed_reg.is_subset(tab->checked_keys))
	  {
	    tab->keys=sel->quick_keys;
            tab->keys.merge(sel->needed_reg);
	    tab->use_quick= (!sel->needed_reg.is_clear_all() &&
			     (select->quick_keys.is_clear_all() ||
			      (select->quick &&
			       (select->quick->records >= 100L)))) ?
	      2 : 1;
	    sel->read_tables= used_tables & ~current_map;
	  }
	  if (i != join->const_tables && tab->use_quick != 2)
	  {					/* Read with cache */
	    if (cond &&
                (tmp=make_cond_for_table(cond,
					 join->const_table_map |
					 current_map,
					 current_map)))
	    {
	      DBUG_EXECUTE("where",print_where(tmp,"cache"););
	      tab->cache.select=(SQL_SELECT*)
		thd->memdup((gptr) sel, sizeof(SQL_SELECT));
	      tab->cache.select->cond=tmp;
	      tab->cache.select->read_tables=join->const_table_map;
	    }
	  }
	}
      }
      
      /* 
        Push down all predicates from on expressions.
        Each of these predicated are guarded by a variable
        that turns if off just before null complemented row for
        outer joins is formed. Thus, the predicates from an
        'on expression' are guaranteed not to be checked for
        the null complemented row.
      */ 
      JOIN_TAB *last_tab= tab;
      while (first_inner_tab && first_inner_tab->last_inner == last_tab)
      {  
        /* 
          Table tab is the last inner table of an outer join.
          An on expression is always attached to it.
	*/     
        COND *on_expr= *first_inner_tab->on_expr_ref;

        table_map used_tables= join->const_table_map |
		               OUTER_REF_TABLE_BIT | RAND_TABLE_BIT;
	for (tab= join->join_tab+join->const_tables; tab <= last_tab ; tab++)
        {
          current_map= tab->table->map;
          used_tables|= current_map;
          COND *tmp= make_cond_for_table(on_expr, used_tables, current_map);
          if (tmp)
          {
            JOIN_TAB *cond_tab= tab < first_inner_tab ? first_inner_tab : tab;
            /*
              First add the guards for match variables of
              all embedding outer join operations.
	    */
            if (!(tmp= add_found_match_trig_cond(cond_tab->first_inner,
                                                 tmp, first_inner_tab)))
              DBUG_RETURN(1);
            /* 
              Now add the guard turning the predicate off for 
              the null complemented row.
	    */ 
            DBUG_PRINT("info", ("Item_func_trig_cond"));
            tmp= new Item_func_trig_cond(tmp, 
                                         &first_inner_tab->not_null_compl);
            DBUG_PRINT("info", ("Item_func_trig_cond 0x%lx", (ulong) tmp));
            if (tmp)
              tmp->quick_fix_field();
	    /* Add the predicate to other pushed down predicates */
            DBUG_PRINT("info", ("Item_cond_and"));
            cond_tab->select_cond= !cond_tab->select_cond ? tmp :
	                          new Item_cond_and(cond_tab->select_cond,tmp);
            DBUG_PRINT("info", ("Item_cond_and 0x%lx",
                                (ulong)cond_tab->select_cond));
            if (!cond_tab->select_cond)
	      DBUG_RETURN(1);
            cond_tab->select_cond->quick_fix_field();
          }              
        }
        first_inner_tab= first_inner_tab->first_upper;       
      }
    }
  }
  DBUG_RETURN(0);
}

static void
make_join_readinfo(JOIN *join, uint options)
{
  uint i;

  bool statistics= test(!(join->select_options & SELECT_DESCRIBE));
  DBUG_ENTER("make_join_readinfo");

  for (i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;
    tab->read_record.table= table;
    tab->read_record.file=table->file;
    tab->next_select=sub_select;		/* normal select */
    switch (tab->type) {
    case JT_SYSTEM:				// Only happens with left join
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_system;
      tab->read_record.read_record= join_no_more_records;
      break;
    case JT_CONST:				// Only happens with left join
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_const;
      tab->read_record.read_record= join_no_more_records;
      if (table->used_keys.is_set(tab->ref.key) &&
          !table->no_keyread)
      {
        table->key_read=1;
        table->file->extra(HA_EXTRA_KEYREAD);
      }
      break;
    case JT_EQ_REF:
      table->status=STATUS_NO_RECORD;
      if (tab->select)
      {
	delete tab->select->quick;
	tab->select->quick=0;
      }
      delete tab->quick;
      tab->quick=0;
      tab->read_first_record= join_read_key;
      tab->read_record.read_record= join_no_more_records;
      if (table->used_keys.is_set(tab->ref.key) &&
	  !table->no_keyread)
      {
	table->key_read=1;
	table->file->extra(HA_EXTRA_KEYREAD);
      }
      break;
    case JT_REF_OR_NULL:
    case JT_REF:
      table->status=STATUS_NO_RECORD;
      if (tab->select)
      {
	delete tab->select->quick;
	tab->select->quick=0;
      }
      delete tab->quick;
      tab->quick=0;
      if (table->used_keys.is_set(tab->ref.key) &&
	  !table->no_keyread)
      {
	table->key_read=1;
	table->file->extra(HA_EXTRA_KEYREAD);
      }
      if (tab->type == JT_REF)
      {
	tab->read_first_record= join_read_always_key;
	tab->read_record.read_record= join_read_next_same;
      }
      else
      {
	tab->read_first_record= join_read_always_key_or_null;
	tab->read_record.read_record= join_read_next_same_or_null;
      }
      break;
    case JT_FT:
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_ft_read_first;
      tab->read_record.read_record= join_ft_read_next;
      break;
    case JT_ALL:
      /*
	If previous table use cache
      */
      table->status=STATUS_NO_RECORD;
      if (i != join->const_tables && !(options & SELECT_NO_JOIN_CACHE) &&
          tab->use_quick != 2 && !tab->first_inner)
      {
	if ((options & SELECT_DESCRIBE) ||
	    !join_init_cache(join->thd,join->join_tab+join->const_tables,
			     i-join->const_tables))
	{
	  tab[-1].next_select=sub_select_cache; /* Patch previous */
	}
      }
      /* These init changes read_record */
      if (tab->use_quick == 2)
      {
	join->thd->server_status|=SERVER_QUERY_NO_GOOD_INDEX_USED;
	tab->read_first_record= join_init_quick_read_record;
	if (statistics)
	  statistic_increment(join->thd->status_var.select_range_check_count,
			      &LOCK_status);
      }
      else
      {
	tab->read_first_record= join_init_read_record;
	if (i == join->const_tables)
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      statistic_increment(join->thd->status_var.select_range_count,
				  &LOCK_status);
	  }
	  else
	  {
	    join->thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
	    if (statistics)
	      statistic_increment(join->thd->status_var.select_scan_count,
				  &LOCK_status);
	  }
	}
	else
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      statistic_increment(join->thd->status_var.select_full_range_join_count,
				  &LOCK_status);
	  }
	  else
	  {
	    join->thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
	    if (statistics)
	      statistic_increment(join->thd->status_var.select_full_join_count,
				  &LOCK_status);
	  }
	}
	if (!table->no_keyread)
	{
	  if (tab->select && tab->select->quick &&
              tab->select->quick->index != MAX_KEY && //not index_merge
	      table->used_keys.is_set(tab->select->quick->index))
	  {
	    table->key_read=1;
	    table->file->extra(HA_EXTRA_KEYREAD);
	  }
	  else if (!table->used_keys.is_clear_all() &&
		   !(tab->select && tab->select->quick))
	  {					// Only read index tree
	    tab->index=find_shortest_key(table, & table->used_keys);
	    tab->read_first_record= join_read_first;
	    tab->type=JT_NEXT;		// Read with index_first / index_next
	  }
	}
      }
      break;
    default:
      DBUG_PRINT("error",("Table type %d found",tab->type)); /* purecov: deadcode */
      break;					/* purecov: deadcode */
    case JT_UNKNOWN:
    case JT_MAYBE_REF:
      abort();					/* purecov: deadcode */
    }
  }
  join->join_tab[join->tables-1].next_select=0; /* Set by do_select */
  DBUG_VOID_RETURN;
}


/*
  Give error if we some tables are done with a full join

  SYNOPSIS
    error_if_full_join()
    join		Join condition

  USAGE
   This is used by multi_table_update and multi_table_delete when running
   in safe mode

 RETURN VALUES
   0	ok
   1	Error (full join used)
*/

bool error_if_full_join(JOIN *join)
{
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->type == JT_ALL && (!tab->select || !tab->select->quick))
    {
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      return(1);
    }
  }
  return(0);
}


/*
  cleanup JOIN_TAB

  SYNOPSIS
    JOIN_TAB::cleanup()
*/

void JOIN_TAB::cleanup()
{
  delete select;
  select= 0;
  delete quick;
  quick= 0;
  x_free(cache.buff);
  cache.buff= 0;
  if (table)
  {
    if (table->key_read)
    {
      table->key_read= 0;
      table->file->extra(HA_EXTRA_NO_KEYREAD);
    }
    table->file->ha_index_or_rnd_end();
    /*
      We need to reset this for next select
      (Tested in part_of_refkey)
    */
    table->reginfo.join_tab= 0;
  }
  end_read_record(&read_record);
}


void JOIN::join_free(bool full)
{
  SELECT_LEX_UNIT *unit;
  SELECT_LEX *sl;
  DBUG_ENTER("JOIN::join_free");

  /*
    Optimization: if not EXPLAIN and we are done with the JOIN,
    free all tables.
  */
  full= full || (!select_lex->uncacheable && !thd->lex->describe);

  cleanup(full);

  for (unit= select_lex->first_inner_unit(); unit; unit= unit->next_unit())
    for (sl= unit->first_select_in_union(); sl; sl= sl->next_select())
    {
      JOIN *join= sl->join;
      if (join)
        join->join_free(full);
    }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (full && lock && thd->lock && !(select_options & SELECT_NO_UNLOCK) &&
      !select_lex->subquery_in_having &&
      (select_lex == (thd->lex->unit.fake_select_lex ?
                      thd->lex->unit.fake_select_lex : &thd->lex->select_lex)))
  {
    /*
      TODO: unlock tables even if the join isn't top level select in the
      tree.
    */
    mysql_unlock_read_tables(thd, lock);           // Don't free join->lock
    lock= 0;
  }

  DBUG_VOID_RETURN;
}


/*
  Free resources of given join

  SYNOPSIS
    JOIN::cleanup()
    fill - true if we should free all resources, call with full==1 should be
           last, before it this function can be called with full==0

  NOTE: with subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/

void JOIN::cleanup(bool full)
{
  DBUG_ENTER("JOIN::cleanup");

  if (table)
  {
    JOIN_TAB *tab,*end;
    /*
      Only a sorted table may be cached.  This sorted table is always the
      first non const table in join->table
    */
    if (tables > const_tables) // Test for not-const tables
    {
      free_io_cache(table[const_tables]);
      filesort_free_buffers(table[const_tables]);
    }

    if (full)
    {
      for (tab= join_tab, end= tab+tables; tab != end; tab++)
	tab->cleanup();
      table= 0;
      tables= 0;
    }
    else
    {
      for (tab= join_tab, end= tab+tables; tab != end; tab++)
      {
	if (tab->table)
	    tab->table->file->ha_index_or_rnd_end();
      }
    }
  }
  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (full)
  {
    if (tmp_join)
      tmp_table_param.copy_field= 0;
    group_fields.delete_elements();
    /*
      We can't call delete_elements() on copy_funcs as this will cause
      problems in free_elements() as some of the elements are then deleted.
    */
    tmp_table_param.copy_funcs.empty();
    tmp_table_param.cleanup();
  }
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  Remove the following expressions from ORDER BY and GROUP BY:
  Constant expressions
  Expression that only uses tables that are of type EQ_REF and the reference
  is in the ORDER list or if all refereed tables are of the above type.

  In the following, the X field can be removed:
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t1.a,t2.X
  SELECT * FROM t1,t2,t3 WHERE t1.a=t2.a AND t2.b=t3.b ORDER BY t1.a,t3.X

  These can't be optimized:
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.X,t1.a
  SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b=t2.b ORDER BY t1.a,t2.c
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.b,t1.a
*****************************************************************************/

static bool
eq_ref_table(JOIN *join, ORDER *start_order, JOIN_TAB *tab)
{
  if (tab->cached_eq_ref_table)			// If cached
    return tab->eq_ref_table;
  tab->cached_eq_ref_table=1;
  if (tab->type == JT_CONST)			// We can skip const tables
    return (tab->eq_ref_table=1);		/* purecov: inspected */
  if (tab->type != JT_EQ_REF)
    return (tab->eq_ref_table=0);		// We must use this
  Item **ref_item=tab->ref.items;
  Item **end=ref_item+tab->ref.key_parts;
  uint found=0;
  table_map map=tab->table->map;

  for (; ref_item != end ; ref_item++)
  {
    if (! (*ref_item)->const_item())
    {						// Not a const ref
      ORDER *order;
      for (order=start_order ; order ; order=order->next)
      {
	if ((*ref_item)->eq(order->item[0],0))
	  break;
      }
      if (order)
      {
	found++;
	DBUG_ASSERT(!(order->used & map));
	order->used|=map;
	continue;				// Used in ORDER BY
      }
      if (!only_eq_ref_tables(join,start_order, (*ref_item)->used_tables()))
	return (tab->eq_ref_table=0);
    }
  }
  /* Check that there was no reference to table before sort order */
  for (; found && start_order ; start_order=start_order->next)
  {
    if (start_order->used & map)
    {
      found--;
      continue;
    }
    if (start_order->depend_map & map)
      return (tab->eq_ref_table=0);
  }
  return tab->eq_ref_table=1;
}


static bool
only_eq_ref_tables(JOIN *join,ORDER *order,table_map tables)
{
  if (specialflag &  SPECIAL_SAFE_MODE)
    return 0;			// skip this optimize /* purecov: inspected */
  for (JOIN_TAB **tab=join->map2table ; tables ; tab++, tables>>=1)
  {
    if (tables & 1 && !eq_ref_table(join, order, *tab))
      return 0;
  }
  return 1;
}


/* Update the dependency map for the tables */

static void update_depend_map(JOIN *join)
{
  JOIN_TAB *join_tab=join->join_tab, *end=join_tab+join->tables;

  for (; join_tab != end ; join_tab++)
  {
    TABLE_REF *ref= &join_tab->ref;
    table_map depend_map=0;
    Item **item=ref->items;
    uint i;
    for (i=0 ; i < ref->key_parts ; i++,item++)
      depend_map|=(*item)->used_tables();
    ref->depend_map=depend_map & ~OUTER_REF_TABLE_BIT;
    depend_map&= ~OUTER_REF_TABLE_BIT;
    for (JOIN_TAB **tab=join->map2table;
	 depend_map ;
	 tab++,depend_map>>=1 )
    {
      if (depend_map & 1)
	ref->depend_map|=(*tab)->ref.depend_map;
    }
  }
}


/* Update the dependency map for the sort order */

static void update_depend_map(JOIN *join, ORDER *order)
{
  for (; order ; order=order->next)
  {
    table_map depend_map;
    order->item[0]->update_used_tables();
    order->depend_map=depend_map=order->item[0]->used_tables();
    // Not item_sum(), RAND() and no reference to table outside of sub select
    if (!(order->depend_map & (OUTER_REF_TABLE_BIT | RAND_TABLE_BIT)))
    {
      for (JOIN_TAB **tab=join->map2table;
	   depend_map ;
	   tab++, depend_map>>=1)
      {
	if (depend_map & 1)
	  order->depend_map|=(*tab)->ref.depend_map;
      }
    }
  }
}


/*
  Remove all constants and check if ORDER only contains simple expressions

  SYNOPSIS
   remove_const()
   join			Join handler
   first_order		List of SORT or GROUP order
   cond			WHERE statement
   change_list		Set to 1 if we should remove things from list
			If this is not set, then only simple_order is
                        calculated   
   simple_order		Set to 1 if we are only using simple expressions

  RETURN
    Returns new sort order

    simple_order is set to 1 if sort_order only uses fields from head table
    and the head table is not a LEFT JOIN table

*/

static ORDER *
remove_const(JOIN *join,ORDER *first_order, COND *cond,
             bool change_list, bool *simple_order)
{
  if (join->tables == join->const_tables)
    return change_list ? 0 : first_order;		// No need to sort

  ORDER *order,**prev_ptr;
  table_map first_table= join->join_tab[join->const_tables].table->map;
  table_map not_const_tables= ~join->const_table_map;
  table_map ref;
  DBUG_ENTER("remove_const");

  prev_ptr= &first_order;
  *simple_order= *join->join_tab[join->const_tables].on_expr_ref ? 0 : 1;

  /* NOTE: A variable of not_const_tables ^ first_table; breaks gcc 2.7 */

  update_depend_map(join, first_order);
  for (order=first_order; order ; order=order->next)
  {
    table_map order_tables=order->item[0]->used_tables();
    if (order->item[0]->with_sum_func)
      *simple_order=0;				// Must do a temp table to sort
    else if (!(order_tables & not_const_tables))
    {
      DBUG_PRINT("info",("removing: %s", order->item[0]->full_name()));
      continue;					// skip const item
    }
    else
    {
      if (order_tables & (RAND_TABLE_BIT | OUTER_REF_TABLE_BIT))
	*simple_order=0;
      else
      {
	Item *comp_item=0;
	if (cond && const_expression_in_where(cond,order->item[0], &comp_item))
	{
	  DBUG_PRINT("info",("removing: %s", order->item[0]->full_name()));
	  continue;
	}
	if ((ref=order_tables & (not_const_tables ^ first_table)))
	{
	  if (!(order_tables & first_table) &&
              only_eq_ref_tables(join,first_order, ref))
	  {
	    DBUG_PRINT("info",("removing: %s", order->item[0]->full_name()));
	    continue;
	  }
	  *simple_order=0;			// Must do a temp table to sort
	}
      }
    }
    if (change_list)
      *prev_ptr= order;				// use this entry
    prev_ptr= &order->next;
  }
  if (change_list)
    *prev_ptr=0;
  if (prev_ptr == &first_order)			// Nothing to sort/group
    *simple_order=1;
  DBUG_PRINT("exit",("simple_order: %d",(int) *simple_order));
  DBUG_RETURN(first_order);
}


static int
return_zero_rows(JOIN *join, select_result *result,TABLE_LIST *tables,
		 List<Item> &fields, bool send_row, uint select_options,
		 const char *info, Item *having)
{
  DBUG_ENTER("return_zero_rows");

  if (select_options & SELECT_DESCRIBE)
  {
    select_describe(join, FALSE, FALSE, FALSE, info);
    DBUG_RETURN(0);
  }

  join->join_free(0);

  if (send_row)
  {
    for (TABLE_LIST *table= tables; table; table= table->next_leaf)
      mark_as_null_row(table->table);		// All fields are NULL
    if (having && having->val_int() == 0)
      send_row=0;
  }
  if (!(result->send_fields(fields,
                              Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)))
  {
    if (send_row)
    {
      List_iterator_fast<Item> it(fields);
      Item *item;
      while ((item= it++))
	item->no_rows_in_result();
      result->send_data(fields);
    }
    result->send_eof();				// Should be safe
  }
  /* Update results for FOUND_ROWS */
  join->thd->limit_found_rows= join->thd->examined_row_count= 0;
  DBUG_RETURN(0);
}


static void clear_tables(JOIN *join)
{
  for (uint i=0 ; i < join->tables ; i++)
    mark_as_null_row(join->table[i]);		// All fields are NULL
}

/*****************************************************************************
  Make som simple condition optimization:
  If there is a test 'field = const' change all refs to 'field' to 'const'
  Remove all dummy tests 'item = item', 'const op const'.
  Remove all 'item is NULL', when item can never be null!
  item->marker should be 0 for all items on entry
  Return in cond_value FALSE if condition is impossible (1 = 2)
*****************************************************************************/

class COND_CMP :public ilink {
public:
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused)))
  { TRASH(ptr, size); }

  Item *and_level;
  Item_func *cmp_func;
  COND_CMP(Item *a,Item_func *b) :and_level(a),cmp_func(b) {}
};

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List<COND_CMP>;
template class I_List_iterator<COND_CMP>;
template class List<Item_func_match>;
template class List_iterator<Item_func_match>;
#endif


/* 
  Find the multiple equality predicate containing a field
 
  SYNOPSIS
    find_item_equal()
    cond_equal          multiple equalities to search in
    field               field to look for
    inherited_fl  :out  set up to TRUE if multiple equality is found
                        on upper levels (not on current level of cond_equal) 

  DESCRIPTION
    The function retrieves the multiple equalities accessed through
    the con_equal structure from current level and up looking for
    an equality containing field. It stops retrieval as soon as the equality
    is found and set up inherited_fl to TRUE if it's found on upper levels.

  RETURN
    Item_equal for the found multiple equality predicate if a success;
    NULL - otherwise.
*/

Item_equal *find_item_equal(COND_EQUAL *cond_equal, Field *field,
                            bool *inherited_fl)
{
  Item_equal *item= 0;
  bool in_upper_level= FALSE;
  while (cond_equal)
  {
    List_iterator_fast<Item_equal> li(cond_equal->current_level);
    while ((item= li++))
    {
      if (item->contains(field))
        goto finish;
    }
    in_upper_level= TRUE;
    cond_equal= cond_equal->upper_levels;
  }
  in_upper_level= FALSE;
finish:
  *inherited_fl= in_upper_level;
  return item;
}

  
/* 
  Check whether an item is a simple equality predicate and if so
  create/find a multiple equality for this predicate

  SYNOPSIS
    check_equality()
    item       item to check
    cond_equal multiple equalities that must hold together with the predicate

  DESCRIPTION
    This function first checks whether an item is a simple equality i.e.
    the one that equates a field with another field or a constant
    (item=constant_item or item=field_item).
    If this is the case the function looks a for a multiple equality
    in the lists referenced directly or indirectly by cond_equal inferring
    the given simple equality. If it doesn't find any, it builds a multiple
    equality that covers the predicate, i.e. the predicate can be inferred
    from it.
    The built multiple equality could be obtained in such a way:
    create a binary  multiple equality equivalent to the predicate, then
    merge it, if possible, with one of old multiple equalities.
    This guarantees that the set of multiple equalities covering equality
    predicates will
    be minimal.

  EXAMPLE
    For the where condition
    WHERE a=b AND b=c AND
          (b=2 OR f=e)
    the check_equality will be called for the following equality
    predicates a=b, b=c, b=2 and f=e.
    For a=b it will be called with *cond_equal=(0,[]) and will transform
    *cond_equal into (0,[Item_equal(a,b)]). 
    For b=c it will be called with *cond_equal=(0,[Item_equal(a,b)])
    and will transform *cond_equal into CE=(0,[Item_equal(a,b,c)]).
    For b=2 it will be called with *cond_equal=(ptr(CE),[])
    and will transform *cond_equal into (ptr(CE),[Item_equal(2,a,b,c)]).
    For f=e it will be called with *cond_equal=(ptr(CE), [])
    and will transform *cond_equal into (ptr(CE),[Item_equal(f,e)]).

  NOTES
    Now only fields that have the same type defintions (verified by
    the Field::eq_def method) are placed to the same multiple equalities.
    Because of this some equality predicates are not eliminated and
    can be used in the constant propagation procedure.
    We could weeken the equlity test as soon as at least one of the 
    equal fields is to be equal to a constant. It would require a 
    more complicated implementation: we would have to store, in
    general case, its own constant for each fields from the multiple
    equality. But at the same time it would allow us to get rid
    of constant propagation completely: it would be done by the call
    to build_equal_items_for_cond.
    
  IMPLEMENTATION
    The implementation does not follow exactly the above rules to
    build a new multiple equality for the equality predicate.
    If it processes the equality of the form field1=field2, it
    looks for multiple equalities me1 containig field1 and me2 containing
    field2. If only one of them is found the fuction expands it with
    the lacking field. If multiple equalities for both fields are
    found they are merged. If both searches fail a new multiple equality
    containing just field1 and field2 is added to the existing
    multiple equalities.
    If the function processes the predicate of the form field1=const,
    it looks for a multiple equality containing field1. If found, the 
    function checks the constant of the multiple equality. If the value
    is unknown, it is setup to const. Otherwise the value is compared with
    const and the evaluation of the equality predicate is performed.
    When expanding/merging equality predicates from the upper levels
    the function first copies them for the current level. It looks
    acceptable, as this happens rarely. The implementation without
    copying would be much more complicated.

  RETURN
    TRUE  - if the predicate is a simple equality predicate
    FALSE - otherwise
*/

static bool check_equality(Item *item, COND_EQUAL *cond_equal)
{
  if (item->type() == Item::FUNC_ITEM &&
         ((Item_func*) item)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item= ((Item_func*) item)->arguments()[0];
    Item *right_item= ((Item_func*) item)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM &&
        right_item->type() == Item::FIELD_ITEM &&
        !((Item_field*)left_item)->depended_from &&
        !((Item_field*)right_item)->depended_from)
    {
      /* The predicate the form field1=field2 is processed */

      Field *left_field= ((Item_field*) left_item)->field;
      Field *right_field= ((Item_field*) right_item)->field;

      if (!left_field->eq_def(right_field))
        return FALSE;

      if (left_field->eq(right_field))  /* f = f */
        return TRUE;
      
      /* Search for multiple equalities containing field1 and/or field2 */
      bool left_copyfl, right_copyfl;
      Item_equal *left_item_equal=
                 find_item_equal(cond_equal, left_field, &left_copyfl);
      Item_equal *right_item_equal= 
                 find_item_equal(cond_equal, right_field, &right_copyfl);

      if (left_item_equal && left_item_equal == right_item_equal)
      {
        /* 
           The equality predicate is inference of one of the existing
           multiple equalities, i.e the condition is already covered
           by upper level equalities
        */
          return TRUE;
      }
      
      /* Copy the found multiple equalities at the current level if needed */
      if (left_copyfl)
      {
        /* left_item_equal of an upper level contains left_item */
        left_item_equal= new Item_equal(left_item_equal);
        cond_equal->current_level.push_back(left_item_equal);
      }
      if (right_copyfl)
      {
        /* right_item_equal of an upper level contains right_item */
        right_item_equal= new Item_equal(right_item_equal);
        cond_equal->current_level.push_back(right_item_equal);
      }

      if (left_item_equal)
      { 
        /* left item was found in the current or one of the upper levels */
        if (! right_item_equal)
          left_item_equal->add((Item_field *) right_item);
        else
        {
          /* Merge two multiple equalities forming a new one */
          left_item_equal->merge(right_item_equal);
          /* Remove the merged multiple equality from the list */
          List_iterator<Item_equal> li(cond_equal->current_level);
          while ((li++) != right_item_equal);
          li.remove();
        }
      }
      else
      { 
        /* left item was not found neither the current nor in upper levels  */
         if (right_item_equal)
           right_item_equal->add((Item_field *) left_item);
         else 
         {
           /* None of the fields was found in multiple equalities */
           Item_equal *item= new Item_equal((Item_field *) left_item,
                                            (Item_field *) right_item);
           cond_equal->current_level.push_back(item);
         }
      }
      return TRUE;
    }

    {
      /* The predicate of the form field=const/const=field is processed */
      Item *const_item= 0;
      Item_field *field_item= 0;
      if (left_item->type() == Item::FIELD_ITEM &&
          !((Item_field*)left_item)->depended_from &&
          right_item->const_item())
      {
        field_item= (Item_field*) left_item;
        const_item= right_item;
      }
      else if (right_item->type() == Item::FIELD_ITEM &&
               !((Item_field*)right_item)->depended_from &&
               left_item->const_item())
      {
        field_item= (Item_field*) right_item;
        const_item= left_item;
      }
      if (const_item &&
          field_item->result_type() == const_item->result_type())
      {
        bool copyfl;

        if (field_item->result_type() == STRING_RESULT)
        {
          CHARSET_INFO *cs= ((Field_str*) field_item->field)->charset();
          if ((cs != ((Item_cond *) item)->compare_collation()) ||
              !cs->coll->propagate(cs, 0, 0))
            return FALSE;
        }

        Item_equal *item_equal = find_item_equal(cond_equal,
                                                 field_item->field, &copyfl);
        if (copyfl)
        {
          item_equal= new Item_equal(item_equal);
          cond_equal->current_level.push_back(item_equal);
        }
        if (item_equal)
        {
          /* 
            The flag cond_false will be set to 1 after this, if item_equal
            already contains a constant and its value is  not equal to
            the value of const_item.
          */
          item_equal->add(const_item);
        }
        else
        {
          item_equal= new Item_equal(const_item, field_item);
          cond_equal->current_level.push_back(item_equal);
        }
        return TRUE;
      }
    }
  }
  return FALSE;
}

/* 
  Replace all equality predicates in a condition by multiple equality items

  SYNOPSIS
    build_equal_items_for_cond()
    cond       condition(expression) where to make replacement
    inherited  path to all inherited multiple equality items

  DESCRIPTION
    At each 'and' level the function detects items for equality predicates
    and replaced them by a set of multiple equality items of class Item_equal,
    taking into account inherited equalities from upper levels. 
    If an equality predicate is used not in a conjunction it's just
    replaced by a multiple equality predicate.
    For each 'and' level the function set a pointer to the inherited
    multiple equalities in the cond_equal field of the associated
    object of the type Item_cond_and.   
    The function also traverses the cond tree and and for each field reference
    sets a pointer to the multiple equality item containing the field, if there
    is any. If this multiple equality equates fields to a constant the
    function replace the field reference by the constant.
    The function also determines the maximum number of members in 
    equality lists of each Item_cond_and object assigning it to
    cond_equal->max_members of this object and updating accordingly
    the upper levels COND_EQUAL structures.  

  NOTES
    Multiple equality predicate =(f1,..fn) is equivalent to the conjuction of
    f1=f2, .., fn-1=fn. It substitutes any inference from these
    equality predicates that is equivalent to the conjunction.
    Thus, =(a1,a2,a3) can substitute for ((a1=a3) AND (a2=a3) AND (a2=a1)) as
    it is equivalent to ((a1=a2) AND (a2=a3)).
    The function always makes a substitution of all equality predicates occured
    in a conjuction for a minimal set of multiple equality predicates.
    This set can be considered as a canonical representation of the
    sub-conjunction of the equality predicates.
    E.g. (t1.a=t2.b AND t2.b>5 AND t1.a=t3.c) is replaced by 
    (=(t1.a,t2.b,t3.c) AND t2.b>5), not by
    (=(t1.a,t2.b) AND =(t1.a,t3.c) AND t2.b>5);
    while (t1.a=t2.b AND t2.b>5 AND t3.c=t4.d) is replaced by
    (=(t1.a,t2.b) AND =(t3.c=t4.d) AND t2.b>5),
    but if additionally =(t4.d,t2.b) is inherited, it
    will be replaced by (=(t1.a,t2.b,t3.c,t4.d) AND t2.b>5)

  IMPLEMENTATION
    The function performs the substitution in a recursive descent by
    the condtion tree, passing to the next AND level a chain of multiple
    equality predicates which have been built at the upper levels.
    The Item_equal items built at the level are attached to other 
    non-equality conjucts as a sublist. The pointer to the inherited
    multiple equalities is saved in the and condition object (Item_cond_and).
    This chain allows us for any field reference occurence easyly to find a 
    multiple equality that must be held for this occurence.
    For each AND level we do the following:
    - scan it for all equality predicate (=) items
    - join them into disjoint Item_equal() groups
    - process the included OR conditions recursively to do the same for 
      lower AND levels. 
    We need to do things in this order as lower AND levels need to know about
    all possible Item_equal objects in upper levels.

  RETURN
    pointer to the transformed condition
*/

static COND *build_equal_items_for_cond(COND *cond,
                                        COND_EQUAL *inherited)
{
  Item_equal *item_equal;
  uint members;
  COND_EQUAL cond_equal;
  cond_equal.upper_levels= inherited;

  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List<Item> *args= ((Item_cond*) cond)->argument_list();
    
    List_iterator<Item> li(*args);
    Item *item;

    if (and_level)
    {
      /*
         Retrieve all conjucts of this level detecting the equality
         that are subject to substitution by multiple equality items and
         removing each such predicate from the conjunction after having 
         found/created a multiple equality whose inference the predicate is.
     */      
      while ((item= li++))
      {
        /*
          PS/SP note: we can safely remove a node from AND-OR
          structure here because it's restored before each
          re-execution of any prepared statement/stored procedure.
        */
        if (check_equality(item, &cond_equal))
          li.remove();
      }

      List_iterator_fast<Item_equal> it(cond_equal.current_level);
      while ((item_equal= it++))
      {
        item_equal->fix_length_and_dec();
        item_equal->update_used_tables();
        members= item_equal->members();
        if (cond_equal.max_members < members)
          cond_equal.max_members= members; 
      }
      members= cond_equal.max_members;
      if (inherited && inherited->max_members < members)
      {
        do
        {
	  inherited->max_members= members;
          inherited= inherited->upper_levels;
        }
        while (inherited);
      }

      ((Item_cond_and*)cond)->cond_equal= cond_equal;
      inherited= &(((Item_cond_and*)cond)->cond_equal);
    }
    /*
       Make replacement of equality predicates for lower levels
       of the condition expression.
    */
    li.rewind();
    while ((item= li++))
    { 
      Item *new_item;
      if ((new_item = build_equal_items_for_cond(item, inherited))!= item)
      {
        /* This replacement happens only for standalone equalities */
        /*
          This is ok with PS/SP as the replacement is done for
          arguments of an AND/OR item, which are restored for each
          execution of PS/SP.
        */
        li.replace(new_item);
      }
    }
    if (and_level)
      args->concat((List<Item> *)&cond_equal.current_level);
  }
  else if (cond->type() == Item::FUNC_ITEM)
  {
    /*
      If an equality predicate forms the whole and level,
      we call it standalone equality and it's processed here.
      E.g. in the following where condition
      WHERE a=5 AND (b=5 or a=c)
      (b=5) and (a=c) are standalone equalities.
      In general we can't leave alone standalone eqalities:
      for WHERE a=b AND c=d AND (b=c OR d=5)
      b=c is replaced by =(a,b,c,d).  
     */
    if (check_equality(cond, &cond_equal) &&
        (item_equal= cond_equal.current_level.pop()))
    {
      item_equal->fix_length_and_dec();
      item_equal->update_used_tables();
      return item_equal;
    }
    /* 
      For each field reference in cond, not from equalitym predicates,
      set a pointer to the multiple equality if belongs to (if there is any)
    */ 
    cond= cond->transform(&Item::equal_fields_propagator,
                            (byte *) inherited);
    cond->update_used_tables();
  }
  return cond;
}


/* 
  Build multiple equalities for a condition and all on expressions that
  inherit these multiple equalities

  SYNOPSIS
    build_equal_items()
    thd			Thread handler
    cond                condition to build the multiple equalities for
    inherited           path to all inherited multiple equality items
    join_list           list of join tables to which the condition refers to
    cond_equal_ref :out pointer to the structure to place built equalities in

  DESCRIPTION
    The function first applies the build_equal_items_for_cond function
    to build all multiple equalities for condition cond utilizing equalities
    referred through the parameter inherited. The extended set of
    equalities is returned in the structure referred by the cond_equal_ref
    parameter. After this the function calls itself recursively for
    all on expressions whose direct references can be found in join_list
    and who inherit directly the multiple equalities just having built.

  NOTES
    The on expression used in an outer join operation inherits all equalities
    from the on expression of the embedding join, if there is any, or 
    otherwise - from the where condition.
    This fact is not obvious, but presumably can be proved.
    Consider the following query:
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t2.a=t4.a
        WHERE t1.a=t2.a;
    If the on expression in the query inherits =(t1.a,t2.a), then we
    can build the multiple equality =(t1.a,t2.a,t3.a,t4.a) that infers
    the equality t3.a=t4.a. Although the on expression
    t1.a=t3.a AND t2.a=t4.a AND t3.a=t4.a is not equivalent to the one
    in the query the latter can be replaced by the former: the new query
    will return the same result set as the original one.

    Interesting that multiple equality =(t1.a,t2.a,t3.a,t4.a) allows us
    to use t1.a=t3.a AND t3.a=t4.a under the on condition:
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    This query equivalent to:
      SELECT * FROM (t1 LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a),t2
        WHERE t1.a=t2.a
    Similarly the original query can be rewritten to the query:
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t2.a=t4.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    that is equivalent to:   
      SELECT * FROM (t2 LEFT JOIN (t3,t4)ON t2.a=t4.a AND t3.a=t4.a), t1
        WHERE t1.a=t2.a
    Thus, applying equalities from the where condition we basically
    can get more freedom in performing join operations.
    Althogh we don't use this property now, it probably makes sense to use 
    it in the future.    
         
  RETURN
    pointer to the transformed condition containing multiple equalities
*/
   
static COND *build_equal_items(THD *thd, COND *cond,
                               COND_EQUAL *inherited,
                               List<TABLE_LIST> *join_list,
                               COND_EQUAL **cond_equal_ref)
{
  COND_EQUAL *cond_equal= 0;

  if (cond) 
  {
    cond= build_equal_items_for_cond(cond, inherited);
    cond->update_used_tables();
    if (cond->type() == Item::COND_ITEM &&
        ((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
      cond_equal= &((Item_cond_and*) cond)->cond_equal;
    else if (cond->type() == Item::FUNC_ITEM &&
             ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
    {
      cond_equal= new COND_EQUAL;
      cond_equal->current_level.push_back((Item_equal *) cond);
    }
  }
  if (cond_equal)
  {
    cond_equal->upper_levels= inherited;
    inherited= cond_equal;
  }
  *cond_equal_ref= cond_equal;

  if (join_list)
  {
    TABLE_LIST *table;
    List_iterator<TABLE_LIST> li(*join_list);

    while ((table= li++))
    {
      if (table->on_expr)
      {
        List<TABLE_LIST> *join_list= table->nested_join ?
	                             &table->nested_join->join_list : NULL;
        /*
          We can modify table->on_expr because its old value will
          be restored before re-execution of PS/SP.
        */
        table->on_expr= build_equal_items(thd, table->on_expr, inherited,
                                          join_list, &table->cond_equal);
      }
    }
  }

  return cond;
}    


/* 
  Compare field items by table order in the execution plan
 
  SYNOPSIS
    compare_fields_by_table_order()
    field1          first field item to compare
    field2          second field item to compare
    table_join_idx  index to tables determining table order    

  DESCRIPTION
    field1 considered as better than field2 if the table containing
    field1 is accessed earlier than the table containing field2.   
    The function finds out what of two fields is better according
    this criteria.

  RETURN
     1, if field1 is better than field2 
    -1, if field2 is better than field1
     0, otherwise
*/

static int compare_fields_by_table_order(Item_field *field1,
                                  Item_field *field2,
                                  void *table_join_idx)
{
  int cmp= 0;
  bool outer_ref= 0;
  if (field2->used_tables() & OUTER_REF_TABLE_BIT)
  {  
    outer_ref= 1;
    cmp= -1;
  }
  if (field2->used_tables() & OUTER_REF_TABLE_BIT)
  {
    outer_ref= 1;
    cmp++;
  }
  if (outer_ref)
    return cmp;
  JOIN_TAB **idx= (JOIN_TAB **) table_join_idx;
  cmp= idx[field2->field->table->tablenr]-idx[field1->field->table->tablenr];
  return cmp < 0 ? -1 : (cmp ? 1 : 0);
}


/* 
  Generate minimal set of simple equalities equivalent to a multiple equality
 
  SYNOPSIS
    eliminate_item_equal()
    cond            condition to add the generated equality to
    upper_levels    structure to access multiple equality of upper levels
    item_equal      multiple equality to generate simple equality from     

  DESCRIPTION
    The function retrieves the fields of the multiple equality item
    item_equal and  for each field f:
    - if item_equal contains const it generates the equality f=const_item;
    - otherwise, if f is not the first field, generates the equality
      f=item_equal->get_first().
    All generated equality are added to the cond conjunction.

  NOTES
    Before generating an equality function checks that it has not
    been generated for multiple equalies of the upper levels.
    E.g. for the following where condition
    WHERE a=5 AND ((a=b AND b=c) OR  c>4)
    the upper level AND condition will contain =(5,a),
    while the lower level AND condition will contain =(5,a,b,c).
    When splitting =(5,a,b,c) into a separate equality predicates
    we should omit 5=a, as we have it already in the upper level.
    The following where condition gives us a more complicated case:
    WHERE t1.a=t2.b AND t3.c=t4.d AND (t2.b=t3.c OR t4.e>5 ...) AND ...
    Given the tables are accessed in the order t1->t2->t3->t4 for
    the selected query execution plan the lower level multiple
    equality =(t1.a,t2.b,t3.c,t4.d) formally  should be converted to
    t1.a=t2.b AND t1.a=t3.c AND t1.a=t4.d. But t1.a=t2.a will be
    generated for the upper level. Also t3.c=t4.d will be generated there.
    So only t1.a=t3.c should be left in the lower level.
    If cond is equal to 0, then not more then one equality is generated
    and a pointer to it is returned as the result of the function.

  RETURN
    The condition with generated simple equalities or
    a pointer to the simple generated equality, if success.
    0, otherwise.
*/

static Item *eliminate_item_equal(COND *cond, COND_EQUAL *upper_levels,
                                  Item_equal *item_equal)
{
  List<Item> eq_list;
  Item_func_eq *eq_item= 0;
  if (((Item *) item_equal)->const_item() && !item_equal->val_int())
    return new Item_int((longlong) 0,1); 
  Item *item_const= item_equal->get_const();
  Item_equal_iterator it(*item_equal);
  Item *head;
  if (item_const)
    head= item_const;
  else
  {
    head= item_equal->get_first();
    it++;
  }
  Item_field *item_field;
  while ((item_field= it++))
  {
    Item_equal *upper= item_field->find_item_equal(upper_levels);
    Item_field *item= item_field;
    if (upper)
    { 
      if (item_const && upper->get_const())
        item= 0;
      else
      {
        Item_equal_iterator li(*item_equal);
        while ((item= li++) != item_field)
        {
          if (item->find_item_equal(upper_levels) == upper)
            break;
        }
      }
    }
    if (item == item_field)
    {
      if (eq_item)
        eq_list.push_back(eq_item);
      eq_item= new Item_func_eq(item_field, head);
      if (!eq_item)
        return 0;
      eq_item->set_cmp_func();
      eq_item->quick_fix_field();
   }
  }

  if (!cond && !eq_list.head())
  {
    if (!eq_item)
      return new Item_int((longlong) 1,1);
    return eq_item;
  }

  if (eq_item)
    eq_list.push_back(eq_item);
  if (!cond)
    cond= new Item_cond_and(eq_list);
  else
    ((Item_cond *) cond)->add_at_head(&eq_list);

  cond->quick_fix_field();
  cond->update_used_tables();
   
  return cond;
}


/* 
  Substitute every field reference in a condition by the best equal field 
  and eliminate all multiplle equality predicates
 
  SYNOPSIS
    substitute_for_best_equal_field()
    cond            condition to process
    cond_equal      multiple equalities to take into consideration
    table_join_idx  index to tables determining field preference

  DESCRIPTION
    The function retrieves the cond condition and for each encountered
    multiple equality predicate it sorts the field references in it
    according to the order of tables specified by the table_join_idx
    parameter. Then it eliminates the multiple equality predicate it
    replacing it by the conjunction of simple equality predicates 
    equating every field from the multiple equality to the first
    field in it, or to the constant, if there is any.
    After this the function retrieves all other conjuncted
    predicates substitute every field reference by the field reference
    to the first equal field or equal constant if there are any.
 
  NOTES
    At the first glance full sort of fields in multiple equality
    seems to be an overkill. Yet it's not the case due to possible
    new fields in multiple equality item of lower levels. We want
    the order in them to comply with the order of upper levels.

  RETURN
    The transformed condition
*/

static COND* substitute_for_best_equal_field(COND *cond,
                                             COND_EQUAL *cond_equal,
                                             void *table_join_idx)
{
  Item_equal *item_equal;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> *cond_list= ((Item_cond*) cond)->argument_list();

    bool and_level= ((Item_cond*) cond)->functype() ==
                      Item_func::COND_AND_FUNC;
    if (and_level)
    {
      cond_equal= &((Item_cond_and *) cond)->cond_equal;
      cond_list->disjoin((List<Item> *) &cond_equal->current_level);

      List_iterator_fast<Item_equal> it(cond_equal->current_level);      
      while ((item_equal= it++))
      {
        item_equal->sort(&compare_fields_by_table_order, table_join_idx);
      }
    }
    
    List_iterator<Item> li(*cond_list);
    Item *item;
    while ((item= li++))
    {
      Item *new_item =substitute_for_best_equal_field(item, cond_equal,
                                                      table_join_idx);
      /*
        This works OK with PS/SP re-execution as changes are made to
        the arguments of AND/OR items only
      */
      if (new_item != item)
        li.replace(new_item);
    }

    if (and_level)
    {
      List_iterator_fast<Item_equal> it(cond_equal->current_level);
      while ((item_equal= it++))
      {
        cond= eliminate_item_equal(cond, cond_equal->upper_levels, item_equal);
      }
    }
  }
  else if (cond->type() == Item::FUNC_ITEM && 
           ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
  {
    item_equal= (Item_equal *) cond;
    item_equal->sort(&compare_fields_by_table_order, table_join_idx);
    if (cond_equal && cond_equal->current_level.head() == item_equal)
      cond_equal= 0;
    return eliminate_item_equal(0, cond_equal, item_equal);
  }
  else
    cond->transform(&Item::replace_equal_field, 0);
  return cond;
}


/*
  change field = field to field = const for each found field = const in the
  and_level
*/

static void
change_cond_ref_to_const(THD *thd, I_List<COND_CMP> *save_list,
                         Item *and_father, Item *cond,
                         Item *field, Item *value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
      change_cond_ref_to_const(thd, save_list,and_level ? cond : item, item,
			       field, value);
    return;
  }
  if (cond->eq_cmp_result() == Item::COND_OK)
    return;					// Not a boolean function

  Item_bool_func2 *func=  (Item_bool_func2*) cond;
  Item **args= func->arguments();
  Item *left_item=  args[0];
  Item *right_item= args[1];
  Item_func::Functype functype=  func->functype();

  if (right_item->eq(field,0) && left_item != value &&
      (left_item->result_type() != STRING_RESULT ||
       value->result_type() != STRING_RESULT ||
       left_item->collation.collation == value->collation.collation))
  {
    Item *tmp=value->new_item();
    if (tmp)
    {
      thd->change_item_tree(args + 1, tmp);
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC)
	  && and_father != cond && !left_item->const_item())
      {
	cond->marker=1;
	COND_CMP *tmp2;
	if ((tmp2=new COND_CMP(and_father,func)))
	  save_list->push_back(tmp2);
      }
      func->set_cmp_func();
    }
  }
  else if (left_item->eq(field,0) && right_item != value &&
           (right_item->result_type() != STRING_RESULT ||
            value->result_type() != STRING_RESULT ||
            right_item->collation.collation == value->collation.collation))
  {
    Item *tmp=value->new_item();
    if (tmp)
    {
      thd->change_item_tree(args, tmp);
      value= tmp;
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC)
	  && and_father != cond && !right_item->const_item())
      {
        args[0]= args[1];                       // For easy check
        thd->change_item_tree(args + 1, value);
	cond->marker=1;
	COND_CMP *tmp2;
	if ((tmp2=new COND_CMP(and_father,func)))
	  save_list->push_back(tmp2);
      }
      func->set_cmp_func();
    }
  }
}

/*
  Remove additional condition inserted by IN/ALL/ANY transformation

  SYNOPSIS
    remove_additional_cond()
    conds - condition for processing

  RETURN VALUES
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

static void
propagate_cond_constants(THD *thd, I_List<COND_CMP> *save_list,
                         COND *and_father, COND *cond)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    I_List<COND_CMP> save;
    while ((item=li++))
    {
      propagate_cond_constants(thd, &save,and_level ? cond : item, item);
    }
    if (and_level)
    {						// Handle other found items
      I_List_iterator<COND_CMP> cond_itr(save);
      COND_CMP *cond_cmp;
      while ((cond_cmp=cond_itr++))
      {
        Item **args= cond_cmp->cmp_func->arguments();
        if (!args[0]->const_item())
          change_cond_ref_to_const(thd, &save,cond_cmp->and_level,
                                   cond_cmp->and_level, args[0], args[1]);
      }
    }
  }
  else if (and_father != cond && !cond->marker)		// In a AND group
  {
    if (cond->type() == Item::FUNC_ITEM &&
	(((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
	 ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC))
    {
      Item_func_eq *func=(Item_func_eq*) cond;
      Item **args= func->arguments();
      bool left_const= args[0]->const_item();
      bool right_const= args[1]->const_item();
      if (!(left_const && right_const) &&
          args[0]->result_type() == args[1]->result_type())
      {
	if (right_const)
	{
          resolve_const_item(thd, &args[1], args[0]);
	  func->update_used_tables();
          change_cond_ref_to_const(thd, save_list, and_father, and_father,
                                   args[0], args[1]);
	}
	else if (left_const)
	{
          resolve_const_item(thd, &args[0], args[1]);
	  func->update_used_tables();
          change_cond_ref_to_const(thd, save_list, and_father, and_father,
                                   args[1], args[0]);
	}
      }
    }
  }
}


/*
  Simplify joins replacing outer joins by inner joins whenever it's possible

  SYNOPSIS
    simplify_joins()
    join        reference to the query info
    join_list   list representation of the join to be converted
    conds       conditions to add on expressions for converted joins
    top         true <=> conds is the where condition  

  DESCRIPTION
    The function, during a retrieval of join_list,  eliminates those
    outer joins that can be converted into inner join, possibly nested.
    It also moves the on expressions for the converted outer joins
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

  NOTES
    An outer join can be replaced by an inner join if the where condition
    or the on expression for an embedding nested join contains a conjunctive
    predicate rejecting null values for some attribute of the inner tables.

    E.g. in the query:    
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    the predicate t2.b < 5 rejects nulls.
    The query is converted first to:
      SELECT * FROM t1 INNER JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    then to the equivalent form:
      SELECT * FROM t1, t2 ON t2.a=t1.a WHERE t2.b < 5 AND t2.a=t1.a.

    Similarly the following query:
      SELECT * from t1 LEFT JOIN (t2, t3) ON t2.a=t1.a t3.b=t1.b
        WHERE t2.c < 5  
    is converted to:
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a t3.b=t1.b 

    One conversion might trigger another:
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a
                       LEFT JOIN t3 ON t3.b=t2.b
        WHERE t3 IS NOT NULL =>
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b => 
      SELECT * FROM t1, t2, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b AND t2.a=t1.a
   
    The function removes all unnecessary braces from the expression
    produced by the conversions.
    E.g. SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b
    finally is converted to: 
      SELECT * FROM t1, t2, t3 WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b

    It also will remove braces from the following queries:
      SELECT * from (t1 LEFT JOIN t2 ON t2.a=t1.a) LEFT JOIN t3 ON t3.b=t2.b
      SELECT * from (t1, (t2,t3)) WHERE t1.a=t2.a AND t2.b=t3.b.

    The benefit of this simplification procedure is that it might return 
    a query for which the optimizer can evaluate execution plan with more
    join orders. With a left join operation the optimizer does not
    consider any plan where one of the inner tables is before some of outer
    tables.

  IMPLEMENTATION.
    The function is implemented by a recursive procedure.  On the recursive
    ascent all attributes are calculated, all outer joins that can be
    converted are replaced and then all unnecessary braces are removed.
    As join list contains join tables in the reverse order sequential
    elimination of outer joins does not requite extra recursive calls.

  EXAMPLES
    Here is an example of a join query with invalid cross references:
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t3.a LEFT JOIN ON  t3.b=t1.b 
     
  RETURN VALUE
    The new condition, if success
    0, otherwise  
*/

static COND *
simplify_joins(JOIN *join, List<TABLE_LIST> *join_list, COND *conds, bool top)
{
  TABLE_LIST *table;
  NESTED_JOIN *nested_join;
  TABLE_LIST *prev_table= 0;
  List_iterator<TABLE_LIST> li(*join_list);
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
      if (table->on_expr)
      {
        Item *expr= table->prep_on_expr ? table->prep_on_expr : table->on_expr;
        /* 
           If an on expression E is attached to the table, 
           check all null rejected predicates in this expression.
           If such a predicate over an attribute belonging to
           an inner table of an embedded outer join is found,
           the outer join is converted to an inner join and
           the corresponding on expression is added to E. 
	*/ 
        expr= simplify_joins(join, &nested_join->join_list,
                             expr, FALSE);
        table->prep_on_expr= table->on_expr= expr;
      }
      nested_join->used_tables= (table_map) 0;
      nested_join->not_null_tables=(table_map) 0;
      conds= simplify_joins(join, &nested_join->join_list, conds, top);
      used_tables= nested_join->used_tables;
      not_null_tables= nested_join->not_null_tables;  
    }
    else
    {
      if (!(table->prep_on_expr))
        table->prep_on_expr= table->on_expr;
      used_tables= table->table->map;
      if (conds)
        not_null_tables= conds->not_null_tables();
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
      table->outer_join= 0;
      if (table->on_expr)
      {
        /* Add on expression to the where condition. */
        if (conds)
        {
          conds= and_conds(conds, table->on_expr);
          conds->top_level_item();
          /* conds is always a new item as both cond and on_expr existed */
          DBUG_ASSERT(!conds->fixed);
          conds->fix_fields(join->thd, &conds);
        }
        else
          conds= table->on_expr; 
        table->prep_on_expr= table->on_expr= 0;
      }
    }
    
    if (!top)
      continue;

    /* 
      Only inner tables of non-convertible outer joins
      remain with on_expr.
    */ 
    if (table->on_expr)
    {
      table->dep_tables|= table->on_expr->used_tables(); 
      if (table->embedding)
      {
        table->dep_tables&= ~table->embedding->nested_join->used_tables;   
        /*
           Embedding table depends on tables used
           in embedded on expressions. 
        */
        table->embedding->on_expr_dep_tables|= table->on_expr->used_tables();
      }
      else
        table->dep_tables&= ~table->table->map;
    }

    if (prev_table)
    {
      /* The order of tables is reverse: prev_table follows table */
      if (prev_table->straight)
        prev_table->dep_tables|= used_tables;
      if (prev_table->on_expr)
      {
        prev_table->dep_tables|= table->on_expr_dep_tables;
        table_map prev_used_tables= prev_table->nested_join ?
	                            prev_table->nested_join->used_tables :
	                            prev_table->table->map;
        /* 
          If on expression contains only references to inner tables
          we still make the inner tables dependent on the outer tables.
          It would be enough to set dependency only on one outer table
          for them. Yet this is really a rare case.
	*/  
        if (!(prev_table->on_expr->used_tables() & ~prev_used_tables))
          prev_table->dep_tables|= used_tables;
      }
    }
    prev_table= table;
  }
    
  /* Flatten nested joins that can be flattened. */
  li.rewind();
  while ((table= li++))
  {
    nested_join= table->nested_join;
    if (nested_join && !table->on_expr)
    {
      TABLE_LIST *tbl;
      List_iterator<TABLE_LIST> it(nested_join->join_list);
      while ((tbl= it++))
      {
        tbl->embedding= table->embedding;
        tbl->join_list= table->join_list;
      }      
      li.replace(nested_join->join_list);
    }
  }
  DBUG_RETURN(conds); 
}
        

static COND *
optimize_cond(JOIN *join, COND *conds, List<TABLE_LIST> *join_list,
              Item::cond_result *cond_value)
{
  THD *thd= join->thd;
  SELECT_LEX *select= thd->lex->current_select;    
  DBUG_ENTER("optimize_cond");

  if (!conds)
    *cond_value= Item::COND_TRUE;
  else
  {
    /* 
      Build all multiple equality predicates and eliminate equality
      predicates that can be inferred from these multiple equalities.
      For each reference of a field included into a multiple equality
      that occurs in a function set a pointer to the multiple equality
      predicate. Substitute a constant instead of this field if the
      multiple equality contains a constant.
    */ 
    DBUG_EXECUTE("where", print_where(conds, "original"););
    conds= build_equal_items(join->thd, conds, NULL, join_list,
                             &join->cond_equal);
    DBUG_EXECUTE("where",print_where(conds,"after equal_items"););

    /* change field = field to field = const for each found field = const */
    propagate_cond_constants(thd, (I_List<COND_CMP> *) 0, conds, conds);
    /*
      Remove all instances of item == item
      Remove all and-levels where CONST item != CONST item
    */
    DBUG_EXECUTE("where",print_where(conds,"after const change"););
    conds= remove_eq_conds(thd, conds, cond_value) ;
    DBUG_EXECUTE("info",print_where(conds,"after remove"););
  }
  DBUG_RETURN(conds);
}


/*
  Remove const and eq items. Return new item, or NULL if no condition
  cond_value is set to according:
  COND_OK    query is possible (field = constant)
  COND_TRUE  always true	( 1 = 1 )
  COND_FALSE always false	( 1 = 2 )
*/

static COND *
remove_eq_conds(THD *thd, COND *cond, Item::cond_result *cond_value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype()
      == Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item::cond_result tmp_cond_value;
    bool should_fix_fields=0;

    *cond_value=Item::COND_UNDEF;
    Item *item;
    while ((item=li++))
    {
      Item *new_item=remove_eq_conds(thd, item, &tmp_cond_value);
      if (!new_item)
	li.remove();
      else if (item != new_item)
      {
	VOID(li.replace(new_item));
	should_fix_fields=1;
      }
      if (*cond_value == Item::COND_UNDEF)
	*cond_value=tmp_cond_value;
      switch (tmp_cond_value) {
      case Item::COND_OK:			// Not TRUE or FALSE
	if (and_level || *cond_value == Item::COND_FALSE)
	  *cond_value=tmp_cond_value;
	break;
      case Item::COND_FALSE:
	if (and_level)
	{
	  *cond_value=tmp_cond_value;
	  return (COND*) 0;			// Always false
	}
	break;
      case Item::COND_TRUE:
	if (!and_level)
	{
	  *cond_value= tmp_cond_value;
	  return (COND*) 0;			// Always true
	}
	break;
      case Item::COND_UNDEF:			// Impossible
	break; /* purecov: deadcode */
      }
    }
    if (should_fix_fields)
      cond->update_used_tables();

    if (!((Item_cond*) cond)->argument_list()->elements ||
	*cond_value != Item::COND_OK)
      return (COND*) 0;
    if (((Item_cond*) cond)->argument_list()->elements == 1)
    {						// Remove list
      item= ((Item_cond*) cond)->argument_list()->head();
      ((Item_cond*) cond)->argument_list()->empty();
      return item;
    }
  }
  else if (cond->type() == Item::FUNC_ITEM &&
	   ((Item_func*) cond)->functype() == Item_func::ISNULL_FUNC)
  {
    /*
      Handles this special case for some ODBC applications:
      The are requesting the row that was just updated with a auto_increment
      value with this construct:

      SELECT * from table_name where auto_increment_column IS NULL
      This will be changed to:
      SELECT * from table_name where auto_increment_column = LAST_INSERT_ID
    */

    Item_func_isnull *func=(Item_func_isnull*) cond;
    Item **args= func->arguments();
    if (args[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) args[0])->field;
      if (field->flags & AUTO_INCREMENT_FLAG && !field->table->maybe_null &&
	  (thd->options & OPTION_AUTO_IS_NULL) &&
	  thd->insert_id())
      {
#ifdef HAVE_QUERY_CACHE
	query_cache_abort(&thd->net);
#endif
	COND *new_cond;
	if ((new_cond= new Item_func_eq(args[0],
					new Item_int("last_insert_id()",
						     thd->insert_id(),
						     21))))
	{
	  cond=new_cond;
          /*
            Item_func_eq can't be fixed after creation so we do not check
            cond->fixed, also it do not need tables so we use 0 as second
            argument.
          */
	  cond->fix_fields(thd, &cond);
	}
	thd->insert_id(0);		// Clear for next request
      }
      /* fix to replace 'NULL' dates with '0' (shreeve@uci.edu) */
      else if (((field->type() == FIELD_TYPE_DATE) ||
		(field->type() == FIELD_TYPE_DATETIME)) &&
		(field->flags & NOT_NULL_FLAG) &&
	       !field->table->maybe_null)
      {
	COND *new_cond;
	if ((new_cond= new Item_func_eq(args[0],new Item_int("0", 0, 2))))
	{
	  cond=new_cond;
          /*
            Item_func_eq can't be fixed after creation so we do not check
            cond->fixed, also it do not need tables so we use 0 as second
            argument.
          */
	  cond->fix_fields(thd, &cond);
	}
      }
    }
    if (cond->const_item())
    {
      *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
      return (COND*) 0;
    }
  }
  else if (cond->const_item())
  {
    *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
    return (COND*) 0;
  }
  else if ((*cond_value= cond->eq_cmp_result()) != Item::COND_OK)
  {						// boolan compare function
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(right_item,1))
    {
      if (!left_item->maybe_null ||
	  ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC)
	return (COND*) 0;			// Compare of identical items
    }
  }
  *cond_value=Item::COND_OK;
  return cond;					// Point at next and level
}

/*
  Return 1 if the item is a const value in all the WHERE clause
*/

static bool
const_expression_in_where(COND *cond, Item *comp_item, Item **const_item)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype()
		     == Item_func::COND_AND_FUNC);
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      bool res=const_expression_in_where(item, comp_item, const_item);
      if (res)					// Is a const value
      {
	if (and_level)
	  return 1;
      }
      else if (!and_level)
	return 0;
    }
    return and_level ? 0 : 1;
  }
  else if (cond->eq_cmp_result() != Item::COND_OK)
  {						// boolan compare function
    Item_func* func= (Item_func*) cond;
    if (func->functype() != Item_func::EQUAL_FUNC &&
	func->functype() != Item_func::EQ_FUNC)
      return 0;
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(comp_item,1))
    {
      if (right_item->const_item())
      {
	if (*const_item)
	  return right_item->eq(*const_item, 1);
	*const_item=right_item;
	return 1;
      }
    }
    else if (right_item->eq(comp_item,1))
    {
      if (left_item->const_item())
      {
	if (*const_item)
	  return left_item->eq(*const_item, 1);
	*const_item=left_item;
	return 1;
      }
    }
  }
  return 0;
}

/****************************************************************************
  Create internal temporary table
****************************************************************************/

/*
  Create field for temporary table from given field
  
  SYNOPSIS
    create_tmp_field_from_field()
    thd			Thread handler
    org_field           field from which new field will be created
    name                New field name
    table		Temporary table
    item	        !=NULL if item->result_field should point to new field.
			This is relevant for how fill_record() is going to work:
			If item != NULL then fill_record() will update
			the record in the original table.
			If item == NULL then fill_record() will update
			the temporary table
    convert_blob_length If >0 create a varstring(convert_blob_length) field 
                        instead of blob.

  RETURN
    0			on error
    new_created field
*/

Field* create_tmp_field_from_field(THD *thd, Field* org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item, uint convert_blob_length)
{
  Field *new_field;

  if (convert_blob_length && (org_field->flags & BLOB_FLAG))
    new_field= new Field_varstring(convert_blob_length,
                                   org_field->maybe_null(),
                                   org_field->field_name, table,
                                   org_field->charset());
  else
    new_field= org_field->new_field(thd->mem_root, table);
  if (new_field)
  {
    if (item)
      item->result_field= new_field;
    else
      new_field->field_name= name;
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    if (org_field->type() == MYSQL_TYPE_VAR_STRING ||
        org_field->type() == MYSQL_TYPE_VARCHAR)
      table->s->db_create_options|= HA_OPTION_PACK_RECORD;
  }
  return new_field;
}

/*
  Create field for temporary table using type of given item
  
  SYNOPSIS
    create_tmp_field_from_item()
    thd			Thread handler
    item		Item to create a field for
    table		Temporary table
    copy_func		If set and item is a function, store copy of item
			in this array
    modify_item		1 if item->result_field should point to new item.
			This is relevent for how fill_record() is going to
			work:
			If modify_item is 1 then fill_record() will update
			the record in the original table.
			If modify_item is 0 then fill_record() will update
			the temporary table
    convert_blob_length If >0 create a varstring(convert_blob_length) field 
                        instead of blob.

  RETURN
    0			on error
    new_created field
*/

static Field *create_tmp_field_from_item(THD *thd, Item *item, TABLE *table,
                                         Item ***copy_func, bool modify_item,
                                         uint convert_blob_length)
{
  bool maybe_null=item->maybe_null;
  Field *new_field;
  LINT_INIT(new_field);

  switch (item->result_type()) {
  case REAL_RESULT:
    new_field=new Field_double(item->max_length, maybe_null,
			       item->name, table, item->decimals);
    break;
  case INT_RESULT:
    new_field=new Field_longlong(item->max_length, maybe_null,
				   item->name, table, item->unsigned_flag);
    break;
  case STRING_RESULT:
    DBUG_ASSERT(item->collation.collation);
  
    enum enum_field_types type;
    /*
      DATE/TIME fields have STRING_RESULT result type. To preserve
      type they needed to be handled separately.
    */
    if ((type= item->field_type()) == MYSQL_TYPE_DATETIME ||
        type == MYSQL_TYPE_TIME || type == MYSQL_TYPE_DATE)
      new_field= item->tmp_table_field_from_field_type(table);
    else if (item->max_length/item->collation.collation->mbmaxlen > 255 &&
             convert_blob_length)
      new_field= new Field_varstring(convert_blob_length, maybe_null,
                                     item->name, table,
                                     item->collation.collation);
    else
      new_field= item->make_string_field(table);
    break;
  case DECIMAL_RESULT:
    new_field= new Field_new_decimal(item->max_length, maybe_null, item->name,
                                     table, item->decimals, item->unsigned_flag);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    new_field= 0; // to satisfy compiler (uninitialized variable)
    break;
  }
  if (copy_func && item->is_result_field())
    *((*copy_func)++) = item;			// Save for copy_funcs
  if (modify_item)
    item->set_result_field(new_field);
  return new_field;
}


/*
  Create field for information schema table

  SYNOPSIS
    create_tmp_field_for_schema()
    thd			Thread handler
    table		Temporary table
    item		Item to create a field for

  RETURN
    0			on error
    new_created field
*/

Field *create_tmp_field_for_schema(THD *thd, Item *item, TABLE *table)
{
  if (item->field_type() == MYSQL_TYPE_VARCHAR)
  {
    if (item->max_length > MAX_FIELD_VARCHARLENGTH /
        item->collation.collation->mbmaxlen)
      return new Field_blob(item->max_length, item->maybe_null,
                            item->name, table, item->collation.collation);
    return new Field_varstring(item->max_length, item->maybe_null, item->name,
                               table, item->collation.collation);
  }
  return item->tmp_table_field_from_field_type(table);
}


/*
  Create field for temporary table

  SYNOPSIS
    create_tmp_field()
    thd			Thread handler
    table		Temporary table
    item		Item to create a field for
    type		Type of item (normally item->type)
    copy_func		If set and item is a function, store copy of item
			in this array
    from_field          if field will be created using other field as example,
                        pointer example field will be written here
    group		1 if we are going to do a relative group by on result
    modify_item		1 if item->result_field should point to new item.
			This is relevent for how fill_record() is going to
			work:
			If modify_item is 1 then fill_record() will update
			the record in the original table.
			If modify_item is 0 then fill_record() will update
			the temporary table
    convert_blob_length If >0 create a varstring(convert_blob_length) field 
                        instead of blob.

  RETURN
    0			on error
    new_created field
*/

Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
                        Item ***copy_func, Field **from_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields,
                        uint convert_blob_length)
{
  Item::Type orig_type;
  Item *orig_item;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM &&
      (item->type() != Item::REF_ITEM ||
       !((Item_ref *) item)->depended_from))
  {
    orig_item= item;
    item= item->real_item();
    orig_type= type;
    type= Item::FIELD_ITEM;
  }
  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    Field *result= item_sum->create_tmp_field(group, table, convert_blob_length);
    if (!result)
      thd->fatal_error();
    return result;
  }
  case Item::FIELD_ITEM:
  case Item::DEFAULT_VALUE_ITEM:
  {
    Item_field *field= (Item_field*) item;
    bool orig_modify= modify_item;
    Field *result;
    if (orig_type == Item::REF_ITEM)
      modify_item= 0;
    /*
      If item have to be able to store NULLs but underlaid field can't do it,
      create_tmp_field_from_field() can't be used for tmp field creation.
    */
    if (field->maybe_null && !field->field->maybe_null())
    {
      result= create_tmp_field_from_item(thd, item, table, NULL,
                                       modify_item, convert_blob_length);
      *from_field= field->field;
      if (result && modify_item)
        ((Item_field*)item)->result_field= result;
    } 
    else if (table_cant_handle_bit_fields && field->field->type() == FIELD_TYPE_BIT)
      result= create_tmp_field_from_item(thd, item, table, copy_func,
                                        modify_item, convert_blob_length);
    else
      result= create_tmp_field_from_field(thd, (*from_field= field->field),
                                       item->name, table,
                                       modify_item ? (Item_field*) item :
                                       NULL,
                                       convert_blob_length);
    if (orig_type == Item::REF_ITEM && orig_modify)
      ((Item_ref*)orig_item)->set_result_field(result);
    return result;
  }
  /* Fall through */
  case Item::FUNC_ITEM:
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
  case Item::FIELD_STD_ITEM:
  case Item::SUBSELECT_ITEM:
    /* The following can only happen with 'CREATE TABLE ... SELECT' */
  case Item::PROC_ITEM:
  case Item::INT_ITEM:
  case Item::REAL_ITEM:
  case Item::DECIMAL_ITEM:
  case Item::STRING_ITEM:
  case Item::REF_ITEM:
  case Item::NULL_ITEM:
  case Item::VARBIN_ITEM:
    return create_tmp_field_from_item(thd, item, table, copy_func, modify_item,
                                      convert_blob_length);
  case Item::TYPE_HOLDER:
    return ((Item_type_holder *)item)->make_field_by_type(table);
  default:					// Dosen't have to be stored
    return 0;
  }
}


/*
  Create a temp table according to a field list.
  Set distinct if duplicates could be removed
  Given fields field pointers are changed to point at tmp_table
  for send_fields
*/

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2
#define MIN_STRING_LENGTH_TO_PACK_ROWS   10

TABLE *
create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 ulonglong select_options, ha_rows rows_limit,
		 char *table_alias)
{
  TABLE *table;
  uint	i,field_count,null_count,null_pack_length;
  uint  hidden_null_count, hidden_null_pack_length, hidden_field_count;
  uint  blob_count,group_null_items, string_count;
  uint  temp_pool_slot=MY_BIT_NONE;
  ulong reclength, string_total_length;
  bool  using_unique_constraint= 0;
  bool  use_packed_rows= 0;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char	*tmpname,path[FN_REFLEN];
  byte	*pos,*group_buff;
  uchar *null_flags;
  Field **reg_field, **from_field;
  uint *blob_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",("distinct: %d  save_sum_fields: %d  rows_limit: %lu  group: %d",
		      (int) distinct, (int) save_sum_fields,
		      (ulong) rows_limit,test(group)));

  statistic_increment(thd->status_var.created_tmp_tables, &LOCK_status);

  if (use_temp_pool)
    temp_pool_slot = bitmap_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", tmp_file_prefix,
	    current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    sprintf(path,"%s%lx_%lx_%x", tmp_file_prefix,current_pid,
            thd->thread_id, thd->tmp_table++);
  }

  /*
    No need to change table name to lower case as we are only creating
    MyISAM or HEAP tables here
  */
  fn_format(path, path, mysql_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  if (group)
  {
    if (!param->quick_group)
      group=0;					// Can't use group key
    else for (ORDER *tmp=group ; tmp ; tmp=tmp->next)
    {
      (*tmp->item)->marker=4;			// Store null in key
      if ((*tmp->item)->max_length >= CONVERT_IF_BIGGER_TO_BLOB)
	using_unique_constraint=1;
    }
    if (param->group_length >= MAX_BLOB_WIDTH)
      using_unique_constraint=1;
    if (group)
      distinct=0;				// Can't use distinct
  }

  field_count=param->field_count+param->func_count+param->sum_func_count;
  hidden_field_count=param->hidden_field_count;
  if (!my_multi_malloc(MYF(MY_WME),
		       &table,sizeof(*table),
		       &reg_field,  sizeof(Field*)*(field_count+1),
		       &blob_field, sizeof(uint)*(field_count+1),
		       &from_field, sizeof(Field*)*field_count,
		       &copy_func,sizeof(*copy_func)*(param->func_count+1),
		       &param->keyinfo,sizeof(*param->keyinfo),
		       &key_part_info,
		       sizeof(*key_part_info)*(param->group_parts+1),
		       &param->start_recinfo,
		       sizeof(*param->recinfo)*(field_count*2+4),
		       &tmpname,(uint) strlen(path)+1,
		       &group_buff,group && ! using_unique_constraint ?
		       param->group_length : 0,
		       NullS))
  {
    bitmap_clear_bit(&temp_pool, temp_pool_slot);
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  if (!(param->copy_field=copy=new Copy_field[field_count]))
  {
    bitmap_clear_bit(&temp_pool, temp_pool_slot);
    my_free((gptr) table,MYF(0));		/* purecov: inspected */
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  param->items_to_copy= copy_func;
  strmov(tmpname,path);
  /* make table according to fields */

  bzero((char*) table,sizeof(*table));
  bzero((char*) reg_field,sizeof(Field*)*(field_count+1));
  bzero((char*) from_field,sizeof(Field*)*field_count);
  table->field=reg_field;
  table->alias= table_alias;
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= thd;
  table->quick_keys.init();
  table->used_keys.init();
  table->keys_in_use_for_query.init();

  table->s= &table->share_not_to_be_used;
  table->s->blob_field= blob_field;
  table->s->table_name= table->s->path= tmpname;
  table->s->db= "";
  table->s->blob_ptr_size= mi_portable_sizeof_char_ptr;
  table->s->tmp_table= TMP_TABLE;
  table->s->db_low_byte_first=1;                // True for HEAP and MyISAM
  table->s->table_charset= param->table_charset;
  table->s->keys_for_keyread.init();
  table->s->keys_in_use.init();
  /* For easier error reporting */
  table->s->table_cache_key= (char*) (table->s->db= "");


  /* Calculate which type of fields we will store in the temporary table */

  reclength= string_total_length= 0;
  blob_count= string_count= null_count= hidden_null_count= group_null_items= 0;
  param->using_indirect_summary_function=0;

  List_iterator_fast<Item> li(fields);
  Item *item;
  Field **tmp_from_field=from_field;
  while ((item=li++))
  {
    Item::Type type=item->type();
    if (not_all_columns)
    {
      if (item->with_sum_func && type != Item::SUM_FUNC_ITEM)
      {
	/*
	  Mark that the we have ignored an item that refers to a summary
	  function. We need to know this if someone is going to use
	  DISTINCT on the result.
	*/
	param->using_indirect_summary_function=1;
	continue;
      }
      if (item->const_item() && (int) hidden_field_count <= 0)
        continue; // We don't have to store this
    }
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      ((Item_sum*) item)->result_field=0;
      for (i=0 ; i < ((Item_sum*) item)->arg_count ; i++)
      {
	Item **argp= ((Item_sum*) item)->args + i;
	Item *arg= *argp;
	if (!arg->const_item())
	{
	  Field *new_field=
            create_tmp_field(thd, table, arg, arg->type(), &copy_func,
                             tmp_from_field, group != 0,not_all_columns,
                             group || distinct,
                             param->convert_blob_length);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  reclength+=new_field->pack_length();
	  if (new_field->flags & BLOB_FLAG)
	  {
	    *blob_field++= (uint) (reg_field - table->field);
	    blob_count++;
	  }
          new_field->field_index= (uint) (reg_field - table->field);
	  *(reg_field++)= new_field;
          if (new_field->real_type() == MYSQL_TYPE_STRING ||
              new_field->real_type() == MYSQL_TYPE_VARCHAR)
          {
            string_count++;
            string_total_length+= new_field->pack_length();
          }
          thd->change_item_tree(argp, new Item_field(new_field));
	  if (!(new_field->flags & NOT_NULL_FLAG))
          {
	    null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            (*argp)->maybe_null=1;
          }
          new_field->query_id= thd->query_id;
	}
      }
    }
    else
    {
      /*
	The last parameter to create_tmp_field() is a bit tricky:

	We need to set it to 0 in union, to get fill_record() to modify the
	temporary table.
	We need to set it to 1 on multi-table-update and in select to
	write rows to the temporary table.
	We here distinguish between UNION and multi-table-updates by the fact
	that in the later case group is set to the row pointer.
      */
      Field *new_field= (param->schema_table) ?
        create_tmp_field_for_schema(thd, item, table) :
        create_tmp_field(thd, table, item, type, &copy_func,
                         tmp_from_field, group != 0,
                         not_all_columns || group != 0, 0,
                         param->convert_blob_length);

      if (!new_field)
      {
	if (thd->is_fatal_error)
	  goto err;				// Got OOM
	continue;				// Some kindf of const item
      }
      if (type == Item::SUM_FUNC_ITEM)
	((Item_sum *) item)->result_field= new_field;
      tmp_from_field++;
      reclength+=new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG))
	null_count++;
      if (new_field->type() == FIELD_TYPE_BIT)
        total_uneven_bit_length+= new_field->field_length & 7;
      if (new_field->flags & BLOB_FLAG)
      {
        *blob_field++= (uint) (reg_field - table->field);
	blob_count++;
      }
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      new_field->query_id= thd->query_id;
      new_field->field_index= (uint) (reg_field - table->field);
      *(reg_field++) =new_field;
    }
    if (!--hidden_field_count)
      hidden_null_count=null_count;
  }
  DBUG_ASSERT(field_count >= (uint) (reg_field - table->field));
  field_count= (uint) (reg_field - table->field);
  *blob_field= 0;				// End marker

  /* If result table is small; use a heap */
  if (blob_count || using_unique_constraint ||
      (select_options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES || (select_options & TMP_TABLE_FORCE_MYISAM))
  {
    table->file=get_new_handler(table,table->s->db_type= DB_TYPE_MYISAM);
    if (group &&
	(param->group_parts > table->file->max_key_parts() ||
	 param->group_length > table->file->max_key_length()))
      using_unique_constraint=1;
  }
  else
  {
    table->file=get_new_handler(table,table->s->db_type= DB_TYPE_HEAP);
  }

  if (!using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  table->s->blob_fields= blob_count;
  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length=(hidden_null_count+7)/8;
  null_pack_length= hidden_null_count +
                    (null_count + total_uneven_bit_length + 7) / 8;
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count ||
      string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS &&
      (reclength / string_total_length <= RATIO_TO_PACK_ROWS ||
       string_total_length / string_count >= AVG_STRING_LENGTH_TO_PACK_ROWS))
    use_packed_rows= 1;

  table->s->fields= field_count;
  table->s->reclength= reclength;
  {
    uint alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    table->s->rec_buff_length= alloc_length;
    if (!(table->record[0]= (byte *) my_malloc(alloc_length*3, MYF(MY_WME))))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    table->s->default_values= table->record[1]+alloc_length;
  }
  copy_func[0]=0;				// End marker

  recinfo=param->start_recinfo;
  null_flags=(uchar*) table->record[0];
  pos=table->record[0]+ null_pack_length;
  if (null_pack_length)
  {
    bzero((byte*) recinfo,sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    bfill(null_flags,null_pack_length,255);	// Set null fields

    table->null_flags= (uchar*) table->record[0];
    table->s->null_fields= null_count+ hidden_null_count;
    table->s->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  for (i=0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint length;
    bzero((byte*) recinfo,sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
	/*
	  We have to reserve one byte here for NULL bits,
	  as this is updated by 'end_update()'
	*/
	*pos++=0;				// Null is stored here
	recinfo->length=1;
	recinfo->type=FIELD_NORMAL;
	recinfo++;
	bzero((byte*) recinfo,sizeof(*recinfo));
      }
      else
      {
	recinfo->null_bit= 1 << (null_count & 7);
	recinfo->null_pos= null_count/8;
      }
      field->move_field((char*) pos,null_flags+null_count/8,
			1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field((char*) pos,(uchar*) 0,0);
    if (field->type() == FIELD_TYPE_BIT)
    {
      /* We have to reserve place for extra bits among null bits */
      ((Field_bit*) field)->set_bit_ptr(null_flags + null_count / 8,
                                        null_count & 7);
      null_count+= (field->field_length & 7);
    }
    field->reset();
    if (from_field[i])
    {						/* Not a table Item */
      copy->set(field,from_field[i],save_sum_fields);
      copy++;
    }
    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= (int) FIELD_BLOB;
    else if (use_packed_rows &&
             field->real_type() == MYSQL_TYPE_STRING &&
	     length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type=FIELD_SKIP_ENDSPACE;
    else
      recinfo->type=FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count=(null_count+7) & ~7;		// move to next byte

    // fix table name in field entry
    field->table_name= &table->alias;
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  store_record(table,s->default_values);        // Make empty default record

  if (thd->variables.tmp_table_size == ~(ulong) 0)		// No limit
    table->s->max_rows= ~(ha_rows) 0;
  else
    table->s->max_rows= (((table->s->db_type == DB_TYPE_HEAP) ?
                          min(thd->variables.tmp_table_size,
                              thd->variables.max_heap_table_size) :
                          thd->variables.tmp_table_size)/ table->s->reclength);
  set_if_bigger(table->s->max_rows,1);		// For dummy start options
  keyinfo= param->keyinfo;

  if (group)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    table->s->keys=1;
    table->s->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "group_key";
    for (; group ; group=group->next,key_part_info++)
    {
      Field *field=(*group->item)->get_tmp_table_field();
      bool maybe_null=(*group->item)->maybe_null;
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset();
      key_part_info->length= (uint16) field->key_length();
      key_part_info->type=   (uint8) field->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
	group->buff=(char*) group_buff;
	if (!(group->field= field->new_key_field(thd->mem_root,table,
                                                 (char*) group_buff +
                                                 test(maybe_null),
                                                 field->null_ptr,
                                                 field->null_bit)))
	  goto err; /* purecov: inspected */
	if (maybe_null)
	{
	  /*
	    To be able to group on NULL, we reserved place in group_buff
	    for the NULL flag just before the column. (see above).
	    The field data is after this flag.
	    The NULL flag is updated in 'end_update()' and 'end_write()'
	  */
	  keyinfo->flags|= HA_NULL_ARE_EQUAL;	// def. that NULL == NULL
	  key_part_info->null_bit=field->null_bit;
	  key_part_info->null_offset= (uint) (field->null_ptr -
					      (uchar*) table->record[0]);
          group->buff++;                        // Pointer to field data
	  group_buff++;                         // Skipp null flag
	}
        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL;
	group_buff+= group->field->pack_length();
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (distinct)
  {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    DBUG_PRINT("info",("hidden_field_count: %d", param->hidden_field_count));

    null_pack_length-=hidden_null_pack_length;
    keyinfo->key_parts= ((field_count-param->hidden_field_count)+
			 test(null_pack_length));
    set_if_smaller(table->s->max_rows, rows_limit);
    param->end_write_records= rows_limit;
    table->distinct= 1;
    table->s->keys= 1;
    if (blob_count)
    {
      using_unique_constraint=1;
      table->s->uniques= 1;
    }
    if (!(key_part_info= (KEY_PART_INFO*)
	  sql_calloc((keyinfo->key_parts)*sizeof(KEY_PART_INFO))))
      goto err;
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME | HA_NULL_ARE_EQUAL;
    keyinfo->key_length=(uint16) reclength;
    keyinfo->name= (char*) "distinct_key";
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->rec_per_key=0;
    if (null_pack_length)
    {
      key_part_info->null_bit=0;
      key_part_info->offset=hidden_null_pack_length;
      key_part_info->length=null_pack_length;
      key_part_info->field=new Field_string((char*) table->record[0],
					    (uint32) key_part_info->length,
					    (uchar*) 0,
					    (uint) 0,
					    Field::NONE,
					    NullS, table, &my_charset_bin);
      key_part_info->key_type=FIELDFLAG_BINARY;
      key_part_info->type=    HA_KEYTYPE_BINARY;
      key_part_info++;
    }
    /* Create a distinct key over the columns we are going to return */
    for (i=param->hidden_field_count, reg_field=table->field + i ;
	 i < field_count;
	 i++, reg_field++, key_part_info++)
    {
      key_part_info->null_bit=0;
      key_part_info->field=    *reg_field;
      key_part_info->offset=   (*reg_field)->offset();
      key_part_info->length=   (uint16) (*reg_field)->pack_length();
      key_part_info->type=     (uint8) (*reg_field)->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : FIELDFLAG_BINARY;
    }
  }
  if (thd->is_fatal_error)				// If end of memory
    goto err;					 /* purecov: inspected */
  table->s->db_record_offset= 1;
  if (table->s->db_type == DB_TYPE_MYISAM)
  {
    if (create_myisam_tmp_table(table,param,select_options))
      goto err;
  }
  if (!open_tmp_table(table))
    DBUG_RETURN(table);

 err:
  free_tmp_table(thd,table);                    /* purecov: inspected */
  bitmap_clear_bit(&temp_pool, temp_pool_slot);
  DBUG_RETURN(NULL);				/* purecov: inspected */
}


/****************************************************************************/

/*
  Create a reduced TABLE object with properly set up Field list from a
  list of field definitions.

  SYNOPSIS
    create_virtual_tmp_table()
      thd         connection handle
      field_list  list of column definitions

  DESCRIPTION
    The created table doesn't have a table handler assotiated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this TABLE object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in THD mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  RETURN
    0 if out of memory, TABLE object in case of success
*/

TABLE *create_virtual_tmp_table(THD *thd, List<create_field> &field_list)
{
  uint field_count= field_list.elements;
  Field **field;
  create_field *cdef;                           /* column definition */
  uint record_length= 0;
  uint null_count= 0;                 /* number of columns which may be null */
  uint null_pack_length;              /* NULL representation array length */
  TABLE_SHARE *s;
  /* Create the table and list of all fields */
  TABLE *table= (TABLE*) thd->calloc(sizeof(*table));
  field= (Field**) thd->alloc((field_count + 1) * sizeof(Field*));
  if (!table || !field)
    return 0;

  table->field= field;
  table->s= s= &table->share_not_to_be_used;
  s->fields= field_count;

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<create_field> it(field_list);
  while ((cdef= it++))
  {
    *field= make_field(0, cdef->length,
                       (uchar*) (f_maybe_null(cdef->pack_flag) ? "" : 0),
                       f_maybe_null(cdef->pack_flag) ? 1 : 0,
                       cdef->pack_flag, cdef->sql_type, cdef->charset,
                       cdef->geom_type, cdef->unireg_check,
                       cdef->interval, cdef->field_name, table);
    if (!*field)
      goto error;
    record_length+= (**field).pack_length();
    if (! ((**field).flags & NOT_NULL_FLAG))
      ++null_count;
    ++field;
  }
  *field= NULL;                                 /* mark the end of the list */

  null_pack_length= (null_count + 7)/8;
  s->reclength= record_length + null_pack_length;
  s->rec_buff_length= ALIGN_SIZE(s->reclength + 1);
  table->record[0]= (byte*) thd->alloc(s->rec_buff_length);
  if (!table->record[0])
    goto error;

  if (null_pack_length)
  {
    table->null_flags= (uchar*) table->record[0];
    s->null_fields= null_count;
    s->null_bytes= null_pack_length;
  }

  table->in_use= thd;           /* field->reset() may access table->in_use */
  {
    /* Set up field pointers */
    byte *null_pos= table->record[0];
    byte *field_pos= null_pos + s->null_bytes;
    uint null_bit= 1;

    for (field= table->field; *field; ++field)
    {
      Field *cur_field= *field;
      if ((cur_field->flags & NOT_NULL_FLAG))
        cur_field->move_field((char*) field_pos);
      else
      {
        cur_field->move_field((char*) field_pos, (uchar*) null_pos, null_bit);
        null_bit<<= 1;
        if (null_bit == (1 << 8))
        {
          ++null_pos;
          null_bit= 1;
        }
      }
      cur_field->reset();

      field_pos+= cur_field->pack_length();
    }
  }
  return table;
error:
  for (field= table->field; *field; ++field)
    delete *field;                         /* just invokes field destructor */
  return 0;
}


static bool open_tmp_table(TABLE *table)
{
  int error;
  if ((error=table->file->ha_open(table->s->table_name,O_RDWR,
                                  HA_OPEN_TMP_TABLE)))
  {
    table->file->print_error(error,MYF(0)); /* purecov: inspected */
    table->db_stat=0;
    return(1);
  }
  (void) table->file->extra(HA_EXTRA_QUICK);		/* Faster */
  return(0);
}


static bool create_myisam_tmp_table(TABLE *table,TMP_TABLE_PARAM *param,
				    ulong options)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  KEY *keyinfo=param->keyinfo;
  DBUG_ENTER("create_myisam_tmp_table");

  if (table->s->keys)
  {						// Get keys for ni_create
    bool using_unique_constraint=0;
    HA_KEYSEG *seg= (HA_KEYSEG*) sql_calloc(sizeof(*seg) *
					    keyinfo->key_parts);
    if (!seg)
      goto err;

    if (keyinfo->key_length >= table->file->max_key_length() ||
	keyinfo->key_parts > table->file->max_key_parts() ||
	table->s->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      table->s->keys=    0;
      table->s->uniques= 1;
      using_unique_constraint=1;
      bzero((char*) &uniquedef,sizeof(uniquedef));
      uniquedef.keysegs=keyinfo->key_parts;
      uniquedef.seg=seg;
      uniquedef.null_are_equal=1;

      /* Create extra column for hash value */
      bzero((byte*) param->recinfo,sizeof(*param->recinfo));
      param->recinfo->type= FIELD_CHECK;
      param->recinfo->length=MI_UNIQUE_HASH_LENGTH;
      param->recinfo++;
      table->s->reclength+=MI_UNIQUE_HASH_LENGTH;
    }
    else
    {
      /* Create an unique key */
      bzero((char*) &keydef,sizeof(keydef));
      keydef.flag=HA_NOSAME | HA_BINARY_PACK_KEY | HA_PACK_KEY;
      keydef.keysegs=  keyinfo->key_parts;
      keydef.seg= seg;
    }
    for (uint i=0; i < keyinfo->key_parts ; i++,seg++)
    {
      Field *field=keyinfo->key_part[i].field;
      seg->flag=     0;
      seg->language= field->charset()->number;
      seg->length=   keyinfo->key_part[i].length;
      seg->start=    keyinfo->key_part[i].offset;
      if (field->flags & BLOB_FLAG)
      {
	seg->type=
	((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	 HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2);
	seg->bit_start= (uint8)(field->pack_length() - table->s->blob_ptr_size);
	seg->flag= HA_BLOB_PART;
	seg->length=0;			// Whole blob in unique constraint
      }
      else
      {
	seg->type= keyinfo->key_part[i].type;
        /* Tell handler if it can do suffic space compression */
	if (field->real_type() == MYSQL_TYPE_STRING &&
	    keyinfo->key_part[i].length > 4)
	  seg->flag|= HA_SPACE_PACK;
      }
      if (!(field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (uchar*) table->record[0]);
	/*
	  We are using a GROUP BY on something that contains NULL
	  In this case we have to tell MyISAM that two NULL should
	  on INSERT be regarded at the same value
	*/
	if (!using_unique_constraint)
	  keydef.flag|= HA_NULL_ARE_EQUAL;
      }
    }
  }
  MI_CREATE_INFO create_info;
  bzero((char*) &create_info,sizeof(create_info));

  if ((options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES)
    create_info.data_file_length= ~(ulonglong) 0;

  if ((error=mi_create(table->s->table_name,table->s->keys,&keydef,
		       (uint) (param->recinfo-param->start_recinfo),
		       param->start_recinfo,
		       table->s->uniques, &uniquedef,
		       &create_info,
		       HA_CREATE_TMP_TABLE)))
  {
    table->file->print_error(error,MYF(0));	/* purecov: inspected */
    table->db_stat=0;
    goto err;
  }
  statistic_increment(table->in_use->status_var.created_tmp_disk_tables,
		      &LOCK_status);
  table->s->db_record_offset= 1;
  DBUG_RETURN(0);
 err:
  DBUG_RETURN(1);
}


void
free_tmp_table(THD *thd, TABLE *entry)
{
  const char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter",("table: %s",entry->alias));

  save_proc_info=thd->proc_info;
  thd->proc_info="removing tmp table";
  free_blobs(entry);
  if (entry->file)
  {
    if (entry->db_stat)
    {
      (void) entry->file->close();
    }
    /*
      We can't call ha_delete_table here as the table may created in mixed case
      here and we have to ensure that delete_table gets the table name in
      the original case.
    */
    if (!(test_flags & TEST_KEEP_TMP_TABLES) ||
        entry->s->db_type == DB_TYPE_HEAP)
      entry->file->delete_table(entry->s->table_name);
    delete entry->file;
  }

  /* free blobs */
  for (Field **ptr=entry->field ; *ptr ; ptr++)
    (*ptr)->free();
  my_free((gptr) entry->record[0],MYF(0));
  free_io_cache(entry);

  bitmap_clear_bit(&temp_pool, entry->temp_pool_slot);

  my_free((gptr) entry,MYF(0));
  thd->proc_info=save_proc_info;

  DBUG_VOID_RETURN;
}

/*
* If a HEAP table gets full, create a MyISAM table and copy all rows to this
*/

bool create_myisam_from_heap(THD *thd, TABLE *table, TMP_TABLE_PARAM *param,
			     int error, bool ignore_last_dupp_key_error)
{
  TABLE new_table;
  const char *save_proc_info;
  int write_err;
  DBUG_ENTER("create_myisam_from_heap");

  if (table->s->db_type != DB_TYPE_HEAP || error != HA_ERR_RECORD_FILE_FULL)
  {
    table->file->print_error(error,MYF(0));
    DBUG_RETURN(1);
  }
  new_table= *table;
  new_table.s= &new_table.share_not_to_be_used;
  new_table.s->db_type= DB_TYPE_MYISAM;
  if (!(new_table.file= get_new_handler(&new_table,DB_TYPE_MYISAM)))
    DBUG_RETURN(1);				// End of memory

  save_proc_info=thd->proc_info;
  thd->proc_info="converting HEAP to MyISAM";

  if (create_myisam_tmp_table(&new_table,param,
			      thd->lex->select_lex.options | thd->options))
    goto err2;
  if (open_tmp_table(&new_table))
    goto err1;
  if (table->file->indexes_are_disabled())
    new_table.file->disable_indexes(HA_KEY_SWITCH_ALL);
  table->file->ha_index_or_rnd_end();
  table->file->ha_rnd_init(1);
  if (table->no_rows)
  {
    new_table.file->extra(HA_EXTRA_NO_ROWS);
    new_table.no_rows=1;
  }

#ifdef TO_BE_DONE_LATER_IN_4_1
  /*
    To use start_bulk_insert() (which is new in 4.1) we need to find
    all places where a corresponding end_bulk_insert() should be put.
  */
  table->file->info(HA_STATUS_VARIABLE); /* update table->file->records */
  new_table.file->start_bulk_insert(table->file->records);
#else
  /* HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it */
  new_table.file->extra(HA_EXTRA_WRITE_CACHE);
#endif

  /* copy all old rows */
  while (!table->file->rnd_next(new_table.record[1]))
  {
    if ((write_err=new_table.file->write_row(new_table.record[1])))
      goto err;
  }
  /* copy row that filled HEAP table */
  if ((write_err=new_table.file->write_row(table->record[0])))
  {
    if (write_err != HA_ERR_FOUND_DUPP_KEY &&
	write_err != HA_ERR_FOUND_DUPP_UNIQUE || !ignore_last_dupp_key_error)
    goto err;
  }

  /* remove heap table and change to use myisam table */
  (void) table->file->ha_rnd_end();
  (void) table->file->close();
  (void) table->file->delete_table(table->s->table_name);
  delete table->file;
  table->file=0;
  *table= new_table;
  table->s= &table->share_not_to_be_used;
  table->file->change_table_ptr(table);
  if (save_proc_info)
    thd->proc_info= (!strcmp(save_proc_info,"Copying to tmp table") ?
                     "Copying to tmp table on disk" : save_proc_info);
  DBUG_RETURN(0);

 err:
  DBUG_PRINT("error",("Got error: %d",write_err));
  table->file->print_error(error,MYF(0));	// Give table is full error
  (void) table->file->ha_rnd_end();
  (void) new_table.file->close();
 err1:
  new_table.file->delete_table(new_table.s->table_name);
  delete new_table.file;
 err2:
  thd->proc_info=save_proc_info;
  DBUG_RETURN(1);
}


/*
  SYNOPSIS
    setup_end_select_func()
    join   join to setup the function for.

  DESCRIPTION
    Rows produced by a join sweep may end up in a temporary table or be sent
    to a client. Setup the function of the nested loop join algorithm which
    handles final fully constructed and matched records.

  RETURN
    end_select function to use. This function can't fail.
*/

static Next_select_func setup_end_select_func(JOIN *join)
{
  TABLE *table= join->tmp_table;
  Next_select_func end_select;
  /* Set up select_end */
  if (table)
  {
    if (table->group && join->tmp_table_param.sum_func_count)
    {
      if (table->s->keys)
      {
	DBUG_PRINT("info",("Using end_update"));
	end_select=end_update;
      }
      else
      {
	DBUG_PRINT("info",("Using end_unique_update"));
	end_select=end_unique_update;
      }
    }
    else if (join->sort_and_group)
    {
      DBUG_PRINT("info",("Using end_write_group"));
      end_select=end_write_group;
    }
    else
    {
      DBUG_PRINT("info",("Using end_write"));
      end_select=end_write;
    }
  }
  else
  {
    /* Test if data is accessed via QUICK_GROUP_MIN_MAX_SELECT. */
    bool is_using_quick_group_min_max_select=
      (join->join_tab->select && join->join_tab->select->quick &&
       (join->join_tab->select->quick->get_type() ==
        QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX));

    if ((join->sort_and_group ||
         (join->procedure && join->procedure->flags & PROC_GROUP)) &&
        !is_using_quick_group_min_max_select)
      end_select= end_send_group;
    else
      end_select= end_send;
  }
  return end_select;
}


/****************************************************************************
  Make a join of all tables and write it on socket or to table
  Return:  0 if ok
           1 if error is sent
          -1 if error should be sent
****************************************************************************/

static int
do_select(JOIN *join,List<Item> *fields,TABLE *table,Procedure *procedure)
{
  int rc= 0;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  JOIN_TAB *join_tab;
  DBUG_ENTER("do_select");

  join->procedure=procedure;
  join->tmp_table= table;			/* Save for easy recursion */
  join->fields= fields;

  if (table)
  {
    VOID(table->file->extra(HA_EXTRA_WRITE_CACHE));
    empty_record(table);
    if (table->group && join->tmp_table_param.sum_func_count &&
        table->s->keys && !table->file->inited)
      table->file->ha_index_init(0);
  }
  /* Set up select_end */
  join->join_tab[join->tables-1].next_select= setup_end_select_func(join);

  join_tab=join->join_tab+join->const_tables;
  join->send_records=0;
  if (join->tables == join->const_tables)
  {
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)
    */
    if (!join->conds || join->conds->val_int())
    {
      Next_select_func end_select= join->join_tab[join->tables-1].next_select;
      error= (*end_select)(join,join_tab,0);
      if (error == NESTED_LOOP_OK || error == NESTED_LOOP_QUERY_LIMIT)
	error= (*end_select)(join,join_tab,1);
    }
    else if (join->send_row_on_empty_set())
      rc= join->result->send_data(*join->fields);
  }
  else
  {
    error= sub_select(join,join_tab,0);
    if (error == NESTED_LOOP_OK || error == NESTED_LOOP_NO_MORE_ROWS)
      error= sub_select(join,join_tab,1);
    if (error == NESTED_LOOP_QUERY_LIMIT)
      error= NESTED_LOOP_OK;                    /* select_limit used */
  }
  if (error == NESTED_LOOP_NO_MORE_ROWS)
    error= NESTED_LOOP_OK;

  if (error == NESTED_LOOP_OK)
  {
    /*
      Sic: this branch works even if rc != 0, e.g. when
      send_data above returns an error.
    */
    if (!table)					// If sending data to client
    {
      /*
	The following will unlock all cursors if the command wasn't an
	update command
      */
      join->join_free(0);				// Unlock all cursors
      if (join->result->send_eof())
	rc= 1;                                  // Don't send error
    }
    DBUG_PRINT("info",("%ld records output",join->send_records));
  }
  else
    rc= -1;
  if (table)
  {
    int tmp, new_errno= 0;
    if ((tmp=table->file->extra(HA_EXTRA_NO_CACHE)))
    {
      DBUG_PRINT("error",("extra(HA_EXTRA_NO_CACHE) failed"));
      new_errno= tmp;
    }
    if ((tmp=table->file->ha_index_or_rnd_end()))
    {
      DBUG_PRINT("error",("ha_index_or_rnd_end() failed"));
      new_errno= tmp;
    }
    if (new_errno)
      table->file->print_error(new_errno,MYF(0));
  }
#ifndef DBUG_OFF
  if (rc)
  {
    DBUG_PRINT("error",("Error: do_select() failed"));
  }
#endif
  DBUG_RETURN(join->thd->net.report_error ? -1 : rc);
}


static enum_nested_loop_state
sub_select_cache(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  enum_nested_loop_state rc;

  if (end_of_records)
  {
    rc= flush_cached_records(join,join_tab,FALSE);
    if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
      rc= sub_select(join,join_tab,end_of_records);
    return rc;
  }
  if (join->thd->killed)		// If aborted by user
  {
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED;                   /* purecov: inspected */
  }
  if (join_tab->use_quick != 2 || test_if_quick_select(join_tab) <= 0)
  {
    if (!store_record_in_cache(&join_tab->cache))
      return NESTED_LOOP_OK;                     // There is more room in cache
    return flush_cached_records(join,join_tab,FALSE);
  }
  rc= flush_cached_records(join, join_tab, TRUE);
  if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= sub_select(join, join_tab, end_of_records);
  return rc;
}

/*
  Retrieve records ends with a given beginning from the result of a join  

  SYNPOSIS
    sub_select()
    join      pointer to the structure providing all context info for the query
    join_tab  the first next table of the execution plan to be retrieved
    end_records  true when we need to perform final steps of retrival   

  DESCRIPTION
    For a given partial join record consisting of records from the tables 
    preceding the table join_tab in the execution plan, the function
    retrieves all matching full records from the result set and
    send them to the result set stream. 

  NOTES
    The function effectively implements the  final (n-k) nested loops
    of nested loops join algorithm, where k is the ordinal number of
    the join_tab table and n is the total number of tables in the join query.
    It performs nested loops joins with all conjunctive predicates from
    the where condition pushed as low to the tables as possible.
    E.g. for the query
      SELECT * FROM t1,t2,t3 
        WHERE t1.a=t2.a AND t2.b=t3.b AND t1.a BETWEEN 5 AND 9
    the predicate (t1.a BETWEEN 5 AND 9) will be pushed to table t1,
    given the selected plan prescribes to nest retrievals of the
    joined tables in the following order: t1,t2,t3.
    A pushed down predicate are attached to the table which it pushed to,
    at the field select_cond.
    When executing a nested loop of level k the function runs through
    the rows of 'join_tab' and for each row checks the pushed condition
    attached to the table.
    If it is false the function moves to the next row of the
    table. If the condition is true the function recursively executes (n-k-1)
    remaining embedded nested loops.
    The situation becomes more complicated if outer joins are involved in
    the execution plan. In this case the pushed down predicates can be
    checked only at certain conditions.
    Suppose for the query
      SELECT * FROM t1 LEFT JOIN (t2,t3) ON t3.a=t1.a 
        WHERE t1>2 AND (t2.b>5 OR t2.b IS NULL)
    the optimizer has chosen a plan with the table order t1,t2,t3.  
    The predicate P1=t1>2 will be pushed down to the table t1, while the
    predicate P2=(t2.b>5 OR t2.b IS NULL) will be attached to the table
    t2. But the second predicate can not be unconditionally tested right
    after a row from t2 has been read. This can be done only after the
    first row with t3.a=t1.a has been encountered.
    Thus, the second predicate P2 is supplied with a guarded value that are
    stored in the field 'found' of the first inner table for the outer join
    (table t2). When the first row with t3.a=t1.a for the  current row 
    of table t1  appears, the value becomes true. For now on the predicate
    is evaluated immediately after the row of table t2 has been read.
    When the first row with t3.a=t1.a has been encountered all
    conditions attached to the inner tables t2,t3 must be evaluated.
    Only when all of them are true the row is sent to the output stream.
    If not, the function returns to the lowest nest level that has a false
    attached condition.
    The predicates from on expressions are also pushed down. If in the 
    the above example the on expression were (t3.a=t1.a AND t2.a=t1.a),
    then t1.a=t2.a would be pushed down to table t2, and without any
    guard.
    If after the run through all rows of table t2, the first inner table
    for the outer join operation, it turns out that no matches are
    found for the current row of t1, then current row from table t1
    is complemented by nulls  for t2 and t3. Then the pushed down predicates
    are checked for the composed row almost in the same way as it had
    been done for the first row with a match. The only difference is
    the predicates  from on expressions are not checked. 

  IMPLEMENTATION
    The function forms output rows for a current partial join of k
    tables tables recursively.
    For each partial join record ending with a certain row from
    join_tab it calls sub_select that builds all possible matching
    tails from the result set.
    To be able  check predicates conditionally items of the class
    Item_func_trig_cond  are employed.
    An object of  this class is constructed from an item of class COND
    and a pointer to a guarding boolean variable.
    When the value of the guard variable is true the value of the object
    is the same as the value of the predicate, otherwise it's just returns
    true. 
    To carry out a return to a nested loop level of join table t the pointer 
    to t is remembered in the field 'return_tab' of the join structure.
    Consider the following query:
      SELECT * FROM t1,
                    LEFT JOIN
                    (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                    ON t4.a=t2.a
         WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    Suppose the chosen execution plan dictates the order t1,t2,t3,t4,t5
    and suppose for a given joined rows from tables t1,t2,t3 there are
    no rows in the result set yet.
    When first row from t5 that satisfies the on condition
    t5.a=t3.a is found, the pushed down predicate t4.b=2 OR t4.b IS NULL
    becomes 'activated', as well the predicate t4.a=t2.a. But
    the predicate (t2.b=5 OR t2.b IS NULL) can not be checked until
    t4.a=t2.a becomes true. 
    In order not to re-evaluate the predicates that were already evaluated
    as attached pushed down predicates, a pointer to the the first
    most inner unmatched table is maintained in join_tab->first_unmatched.
    Thus, when the first row from t5 with t5.a=t3.a is found
    this pointer for t5 is changed from t4 to t2.             

  STRUCTURE NOTES
    join_tab->first_unmatched points always backwards to the first inner
    table of the embedding nested join, if any.

  RETURN
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

static enum_nested_loop_state
sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  join_tab->table->null_row=0;
  if (end_of_records)
    return (*join_tab->next_select)(join,join_tab+1,end_of_records);

  int error;
  enum_nested_loop_state rc;
  my_bool *report_error= &(join->thd->net.report_error);
  READ_RECORD *info= &join_tab->read_record;

  if (join->resume_nested_loop)
  {
    /* If not the last table, plunge down the nested loop */
    if (join_tab < join->join_tab + join->tables - 1)
      rc= (*join_tab->next_select)(join, join_tab + 1, 0);
    else
    {
      join->resume_nested_loop= FALSE;
      rc= NESTED_LOOP_OK;
    }
  }
  else
  {
    join->return_tab= join_tab;

    if (join_tab->last_inner)
    {
      /* join_tab is the first inner table for an outer join operation. */

      /* Set initial state of guard variables for this table.*/
      join_tab->found=0;
      join_tab->not_null_compl= 1;

      /* Set first_unmatched for the last inner table of this group */
      join_tab->last_inner->first_unmatched= join_tab;
    }
    join->thd->row_count= 0;

    error= (*join_tab->read_first_record)(join_tab);
    rc= evaluate_join_record(join, join_tab, error, report_error);
  }

  while (rc == NESTED_LOOP_OK)
  {
    error= info->read_record(info);
    rc= evaluate_join_record(join, join_tab, error, report_error);
  }

  if (rc == NESTED_LOOP_NO_MORE_ROWS &&
      join_tab->last_inner && !join_tab->found)
    rc= evaluate_null_complemented_join_record(join, join_tab);

  if (rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= NESTED_LOOP_OK;
  return rc;
}


/*
  Process one record of the nested loop join.

  DESCRIPTION
    This function will evaluate parts of WHERE/ON clauses that are
    applicable to the partial record on hand and in case of success
    submit this record to the next level of the nested loop.
*/

static enum_nested_loop_state
evaluate_join_record(JOIN *join, JOIN_TAB *join_tab,
                     int error, my_bool *report_error)
{
  bool not_exists_optimize= join_tab->table->reginfo.not_exists_optimize;
  bool not_used_in_distinct=join_tab->not_used_in_distinct;
  ha_rows found_records=join->found_records;
  COND *select_cond= join_tab->select_cond;

  if (error > 0 || (*report_error))				// Fatal error
    return NESTED_LOOP_ERROR;
  if (error < 0)
    return NESTED_LOOP_NO_MORE_ROWS;
  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED;               /* purecov: inspected */
  }
  DBUG_PRINT("info", ("select cond 0x%lx", (ulong)select_cond));
  if (!select_cond || select_cond->val_int())
  {
    /*
      There is no select condition or the attached pushed down
      condition is true => a match is found.
    */
    bool found= 1;
    while (join_tab->first_unmatched && found)
    {
      /*
        The while condition is always false if join_tab is not
        the last inner join table of an outer join operation.
      */
      JOIN_TAB *first_unmatched= join_tab->first_unmatched;
      /*
        Mark that a match for current outer table is found.
        This activates push down conditional predicates attached
        to the all inner tables of the outer join.
      */
      first_unmatched->found= 1;
      for (JOIN_TAB *tab= first_unmatched; tab <= join_tab; tab++)
      {
        /* Check all predicates that has just been activated. */
        /*
          Actually all predicates non-guarded by first_unmatched->found
          will be re-evaluated again. It could be fixed, but, probably,
          it's not worth doing now.
        */
        if (tab->select_cond && !tab->select_cond->val_int())
        {
          /* The condition attached to table tab is false */
          if (tab == join_tab)
            found= 0;
          else
          {
            /*
              Set a return point if rejected predicate is attached
              not to the last table of the current nest level.
            */
            join->return_tab= tab;
            return NESTED_LOOP_OK;
          }
        }
      }
      /*
        Check whether join_tab is not the last inner table
        for another embedding outer join.
      */
      if ((first_unmatched= first_unmatched->first_upper) &&
          first_unmatched->last_inner != join_tab)
        first_unmatched= 0;
      join_tab->first_unmatched= first_unmatched;
    }

    /*
      It was not just a return to lower loop level when one
      of the newly activated predicates is evaluated as false
      (See above join->return_tab= tab).
    */
    join->examined_rows++;
    join->thd->row_count++;

    if (found)
    {
      enum enum_nested_loop_state rc;
      if (not_exists_optimize)
        return NESTED_LOOP_NO_MORE_ROWS;
      /* A match from join_tab is found for the current partial join. */
      rc= (*join_tab->next_select)(join, join_tab+1, 0);
      if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
        return rc;
      if (join->return_tab < join_tab)
        return NESTED_LOOP_OK;
      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        return NESTED_LOOP_OK;
    }
    else
      join_tab->read_record.file->unlock_row();
  }
  else
  {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    join->examined_rows++;
    join->thd->row_count++;
  }
  return NESTED_LOOP_OK;
}


/*
  DESCRIPTION
    Construct a NULL complimented partial join record and feed it to the next
    level of the nested loop. This function is used in case we have
    an OUTER join and no matching record was found.
*/

static enum_nested_loop_state
evaluate_null_complemented_join_record(JOIN *join, JOIN_TAB *join_tab)
{
  /*
    The table join_tab is the first inner table of a outer join operation
    and no matches has been found for the current outer row.
  */
  JOIN_TAB *last_inner_tab= join_tab->last_inner;
  /* Cache variables for faster loop */
  COND *select_cond;
  for ( ; join_tab <= last_inner_tab ; join_tab++)
  {
    /* Change the the values of guard predicate variables. */
    join_tab->found= 1;
    join_tab->not_null_compl= 0;
    /* The outer row is complemented by nulls for each inner tables */
    restore_record(join_tab->table,s->default_values);  // Make empty record
    mark_as_null_row(join_tab->table);       // For group by without error
    select_cond= join_tab->select_cond;
    /* Check all attached conditions for inner table rows. */
    if (select_cond && !select_cond->val_int())
      return NESTED_LOOP_OK;
  }
  join_tab--;
  /*
    The row complemented by nulls might be the first row
    of embedding outer joins.
    If so, perform the same actions as in the code
    for the first regular outer join row above.
  */
  for ( ; ; )
  {
    JOIN_TAB *first_unmatched= join_tab->first_unmatched;
    if ((first_unmatched= first_unmatched->first_upper) &&
        first_unmatched->last_inner != join_tab)
      first_unmatched= 0;
    join_tab->first_unmatched= first_unmatched;
    if (!first_unmatched)
      break;
    first_unmatched->found= 1;
    for (JOIN_TAB *tab= first_unmatched; tab <= join_tab; tab++)
    {
      if (tab->select_cond && !tab->select_cond->val_int())
      {
        join->return_tab= tab;
        return NESTED_LOOP_OK;
      }
    }
  }
  /*
    The row complemented by nulls satisfies all conditions
    attached to inner tables.
    Send the row complemented by nulls to be joined with the
    remaining tables.
  */
  return (*join_tab->next_select)(join, join_tab+1, 0);
}


static enum_nested_loop_state
flush_cached_records(JOIN *join,JOIN_TAB *join_tab,bool skip_last)
{
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  int error;
  READ_RECORD *info;

  if (!join_tab->cache.records)
    return NESTED_LOOP_OK;                      /* Nothing to do */
  if (skip_last)
    (void) store_record_in_cache(&join_tab->cache); // Must save this for later
  if (join_tab->use_quick == 2)
  {
    if (join_tab->select->quick)
    {					/* Used quick select last. reset it */
      delete join_tab->select->quick;
      join_tab->select->quick=0;
    }
  }
 /* read through all records */
  if ((error=join_init_read_record(join_tab)))
  {
    reset_cache_write(&join_tab->cache);
    return error < 0 ? NESTED_LOOP_NO_MORE_ROWS: NESTED_LOOP_ERROR;
  }

  for (JOIN_TAB *tmp=join->join_tab; tmp != join_tab ; tmp++)
  {
    tmp->status=tmp->table->status;
    tmp->table->status=0;
  }

  info= &join_tab->read_record;
  do
  {
    if (join->thd->killed)
    {
      join->thd->send_kill_message();
      return NESTED_LOOP_KILLED; // Aborted by user /* purecov: inspected */
    }
    SQL_SELECT *select=join_tab->select;
    if (rc == NESTED_LOOP_OK &&
        (!join_tab->cache.select || !join_tab->cache.select->skip_record()))
    {
      uint i;
      reset_cache_read(&join_tab->cache);
      for (i=(join_tab->cache.records- (skip_last ? 1 : 0)) ; i-- > 0 ;)
      {
	read_cached_record(join_tab);
	if (!select || !select->skip_record())
        {
          rc= (join_tab->next_select)(join,join_tab+1,0);
	  if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
          {
            reset_cache_write(&join_tab->cache);
            return rc;
          }
        }
      }
    }
  } while (!(error=info->read_record(info)));

  if (skip_last)
    read_cached_record(join_tab);		// Restore current record
  reset_cache_write(&join_tab->cache);
  if (error > 0)				// Fatal error
    return NESTED_LOOP_ERROR;                   /* purecov: inspected */
  for (JOIN_TAB *tmp2=join->join_tab; tmp2 != join_tab ; tmp2++)
    tmp2->table->status=tmp2->status;
  return NESTED_LOOP_OK;
}


/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/* Help function when we get some an error from the table handler */

int report_error(TABLE *table, int error)
{
  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND)
  {
    table->status= STATUS_GARBAGE;
    return -1;					// key not found; ok
  }
  /*
    Locking reads can legally return also these errors, do not
    print them to the .err log
  */
  if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
    sql_print_error("Got error %d when reading table '%s'",
		    error, table->s->path);
  table->file->print_error(error,MYF(0));
  return 1;
}


int safe_index_read(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;
  if ((error=table->file->index_read(table->record[0],
				     tab->ref.key_buff,
				     tab->ref.key_length, HA_READ_KEY_EXACT)))
    return report_error(table, error);
  return 0;
}


static int
join_read_const_table(JOIN_TAB *tab, POSITION *pos)
{
  int error;
  DBUG_ENTER("join_read_const_table");
  TABLE *table=tab->table;
  table->const_table=1;
  table->null_row=0;
  table->status=STATUS_NO_RECORD;
  
  if (tab->type == JT_SYSTEM)
  {
    if ((error=join_read_system(tab)))
    {						// Info for DESCRIBE
      tab->info="const row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->records_read=0.0;
      if (!table->maybe_null || error > 0)
	DBUG_RETURN(error);
    }
  }
  else
  {
    if (!table->key_read && table->used_keys.is_set(tab->ref.key) &&
	!table->no_keyread &&
        (int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY)
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
      tab->index= tab->ref.key;
    }
    error=join_read_const(tab);
    if (table->key_read)
    {
      table->key_read=0;
      table->file->extra(HA_EXTRA_NO_KEYREAD);
    }
    if (error)
    {
      tab->info="unique row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->records_read=0.0;
      if (!table->maybe_null || error > 0)
	DBUG_RETURN(error);
    }
  }
  if (*tab->on_expr_ref && !table->null_row)
  {
    if ((table->null_row= test((*tab->on_expr_ref)->val_int() == 0)))
      mark_as_null_row(table);  
  }
  if (!table->null_row)
    table->maybe_null=0;
  DBUG_RETURN(0);
}


static int
join_read_system(JOIN_TAB *tab)
{
  TABLE *table= tab->table;
  int error;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    if ((error=table->file->read_first_row(table->record[0],
					   table->s->primary_key)))
    {
      if (error != HA_ERR_END_OF_FILE)
	return report_error(table, error);
      mark_as_null_row(tab->table);
      empty_record(table);			// Make empty record
      return -1;
    }
    store_record(table,record[1]);
  }
  else if (!table->status)			// Only happens with left join
    restore_record(table,record[1]);			// restore old record
  table->null_row=0;
  return table->status ? -1 : 0;
}


/*
  Read a table when there is at most one matching row

  SYNOPSIS
    join_read_const()
    tab			Table to read

  RETURN
    0	Row was found
    -1  Row was not found
   1    Got an error (other than row not found) during read
*/

static int
join_read_const(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    table->status= 0;
    if (cp_buffer_from_ref(tab->join->thd, &tab->ref))
      error=HA_ERR_KEY_NOT_FOUND;
    else
    {
      error=table->file->index_read_idx(table->record[0],tab->ref.key,
					(byte*) tab->ref.key_buff,
					tab->ref.key_length,HA_READ_KEY_EXACT);
    }
    if (error)
    {
      table->status= STATUS_NOT_FOUND;
      mark_as_null_row(tab->table);
      empty_record(table);
      if (error != HA_ERR_KEY_NOT_FOUND)
	return report_error(table, error);
      return -1;
    }
    store_record(table,record[1]);
  }
  else if (!(table->status & ~STATUS_NULL_ROW))	// Only happens with left join
  {
    table->status=0;
    restore_record(table,record[1]);			// restore old record
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}


static int
join_read_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key);
  if (cmp_buffer_with_ref(tab) ||
      (table->status & (STATUS_GARBAGE | STATUS_NO_PARENT | STATUS_NULL_ROW)))
  {
    if (tab->ref.key_err)
    {
      table->status=STATUS_NOT_FOUND;
      return -1;
    }
    error=table->file->index_read(table->record[0],
				  tab->ref.key_buff,
				  tab->ref.key_length,HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND)
      return report_error(table, error);
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}


static int
join_read_always_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  for (uint i= 0 ; i < tab->ref.key_parts ; i++)
  {
    if ((tab->ref.null_rejecting & 1 << i) && tab->ref.items[i]->is_null())
        return -1;
  } 
  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key);
  if (cp_buffer_from_ref(tab->join->thd, &tab->ref))
    return -1;
  if ((error=table->file->index_read(table->record[0],
				     tab->ref.key_buff,
				     tab->ref.key_length,HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND)
      return report_error(table, error);
    return -1; /* purecov: inspected */
  }
  return 0;
}


/*
  This function is used when optimizing away ORDER BY in 
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC
*/
  
static int
join_read_last_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key);
  if (cp_buffer_from_ref(tab->join->thd, &tab->ref))
    return -1;
  if ((error=table->file->index_read_last(table->record[0],
					  tab->ref.key_buff,
					  tab->ref.key_length)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND)
      return report_error(table, error);
    return -1; /* purecov: inspected */
  }
  return 0;
}


	/* ARGSUSED */
static int
join_no_more_records(READ_RECORD *info __attribute__((unused)))
{
  return -1;
}


static int
join_read_next_same(READ_RECORD *info)
{
  int error;
  TABLE *table= info->table;
  JOIN_TAB *tab=table->reginfo.join_tab;

  if ((error=table->file->index_next_same(table->record[0],
					  tab->ref.key_buff,
					  tab->ref.key_length)))
  {
    if (error != HA_ERR_END_OF_FILE)
      return report_error(table, error);
    table->status= STATUS_GARBAGE;
    return -1;
  }
  return 0;
}


static int
join_read_prev_same(READ_RECORD *info)
{
  int error;
  TABLE *table= info->table;
  JOIN_TAB *tab=table->reginfo.join_tab;

  if ((error=table->file->index_prev(table->record[0])))
    return report_error(table, error);
  if (key_cmp_if_same(table, tab->ref.key_buff, tab->ref.key,
                      tab->ref.key_length))
  {
    table->status=STATUS_NOT_FOUND;
    error= -1;
  }
  return error;
}


static int
join_init_quick_read_record(JOIN_TAB *tab)
{
  if (test_if_quick_select(tab) == -1)
    return -1;					/* No possible records */
  return join_init_read_record(tab);
}


static int
test_if_quick_select(JOIN_TAB *tab)
{
  delete tab->select->quick;
  tab->select->quick=0;
  return tab->select->test_quick_select(tab->join->thd, tab->keys,
					(table_map) 0, HA_POS_ERROR, 0);
}


static int
join_init_read_record(JOIN_TAB *tab)
{
  if (tab->select && tab->select->quick && tab->select->quick->reset())
    return 1;
  init_read_record(&tab->read_record, tab->join->thd, tab->table,
		   tab->select,1,1);
  return (*tab->read_record.read_record)(&tab->read_record);
}


static int
join_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table=tab->table;
  if (!table->key_read && table->used_keys.is_set(tab->index) &&
      !table->no_keyread)
  {
    table->key_read=1;
    table->file->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_read_next;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  if (!table->file->inited)
    table->file->ha_index_init(tab->index);
  if ((error=tab->table->file->index_first(tab->table->record[0])))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      report_error(table, error);
    return -1;
  }
  return 0;
}


static int
join_read_next(READ_RECORD *info)
{
  int error;
  if ((error=info->file->index_next(info->record)))
    return report_error(info->table, error);
  return 0;
}


static int
join_read_last(JOIN_TAB *tab)
{
  TABLE *table=tab->table;
  int error;
  if (!table->key_read && table->used_keys.is_set(tab->index) &&
      !table->no_keyread)
  {
    table->key_read=1;
    table->file->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_read_prev;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  if (!table->file->inited)
    table->file->ha_index_init(tab->index);
  if ((error= tab->table->file->index_last(tab->table->record[0])))
    return report_error(table, error);
  return 0;
}


static int
join_read_prev(READ_RECORD *info)
{
  int error;
  if ((error= info->file->index_prev(info->record)))
    return report_error(info->table, error);
  return 0;
}


static int
join_ft_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key);
#if NOT_USED_YET
  if (cp_buffer_from_ref(tab->join->thd, &tab->ref)) // as ft-key doesn't use store_key's
    return -1;                             // see also FT_SELECT::init()
#endif
  table->file->ft_init();

  if ((error= table->file->ft_read(table->record[0])))
    return report_error(table, error);
  return 0;
}

static int
join_ft_read_next(READ_RECORD *info)
{
  int error;
  if ((error= info->file->ft_read(info->table->record[0])))
    return report_error(info->table, error);
  return 0;
}


/*
  Reading of key with key reference and one part that may be NULL
*/

static int
join_read_always_key_or_null(JOIN_TAB *tab)
{
  int res;

  /* First read according to key which is NOT NULL */
  *tab->ref.null_ref_key= 0;			// Clear null byte
  if ((res= join_read_always_key(tab)) >= 0)
    return res;

  /* Then read key with null value */
  *tab->ref.null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);
}


static int
join_read_next_same_or_null(READ_RECORD *info)
{
  int error;
  if ((error= join_read_next_same(info)) >= 0)
    return error;
  JOIN_TAB *tab= info->table->reginfo.join_tab;

  /* Test if we have already done a read after null key */
  if (*tab->ref.null_ref_key)
    return -1;					// All keys read
  *tab->ref.null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);			// then read null keys
}


/*****************************************************************************
  DESCRIPTION
    Functions that end one nested loop iteration. Different functions
    are used to support GROUP BY clause and to redirect records
    to a table (e.g. in case of SELECT into a temporary table) or to the
    network client.

  RETURN VALUES
    NESTED_LOOP_OK           - the record has been successfully handled
    NESTED_LOOP_ERROR        - a fatal error (like table corruption)
                               was detected
    NESTED_LOOP_KILLED       - thread shutdown was requested while processing
                               the record
    NESTED_LOOP_QUERY_LIMIT  - the record has been successfully handled;
                               additionally, the nested loop produced the
                               number of rows specified in the LIMIT clause
                               for the query
    NESTED_LOOP_CURSOR_LIMIT - the record has been successfully handled;
                               additionally, there is a cursor and the nested
                               loop algorithm produced the number of rows
                               that is specified for current cursor fetch
                               operation.
   All return values except NESTED_LOOP_OK abort the nested loop.
*****************************************************************************/

/* ARGSUSED */
static enum_nested_loop_state
end_send(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	 bool end_of_records)
{
  DBUG_ENTER("end_send");
  if (!end_of_records)
  {
    int error;
    if (join->having && join->having->val_int() == 0)
      DBUG_RETURN(NESTED_LOOP_OK);               // Didn't match having
    error=0;
    if (join->procedure)
      error=join->procedure->send_row(*join->fields);
    else if (join->do_send_rows)
      error=join->result->send_data(*join->fields);
    if (error)
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if (++join->send_records >= join->unit->select_limit_cnt &&
	join->do_send_rows)
    {
      if (join->select_options & OPTION_FOUND_ROWS)
      {
	JOIN_TAB *jt=join->join_tab;
	if ((join->tables == 1) && !join->tmp_table && !join->sort_and_group
	    && !join->send_group_parts && !join->having && !jt->select_cond &&
	    !(jt->select && jt->select->quick) &&
	    !(jt->table->file->table_flags() & HA_NOT_EXACT_COUNT) &&
            (jt->ref.key < 0))
	{
	  /* Join over all rows in table;  Return number of found rows */
	  TABLE *table=jt->table;

	  join->select_options ^= OPTION_FOUND_ROWS;
	  if (table->sort.record_pointers ||
	      (table->sort.io_cache && my_b_inited(table->sort.io_cache)))
	  {
	    /* Using filesort */
	    join->send_records= table->sort.found_records;
	  }
	  else
	  {
	    table->file->info(HA_STATUS_VARIABLE);
	    join->send_records = table->file->records;
	  }
	}
	else 
	{
	  join->do_send_rows= 0;
	  if (join->unit->fake_select_lex)
	    join->unit->fake_select_lex->select_limit= 0;
	  DBUG_RETURN(NESTED_LOOP_OK);
	}
      }
      DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);      // Abort nicely
    }
    else if (join->send_records >= join->fetch_limit)
    {
      /*
        There is a server side cursor and all rows for
        this fetch request are sent.
      */
      DBUG_RETURN(NESTED_LOOP_CURSOR_LIMIT);
    }
  }
  else
  {
    if (join->procedure && join->procedure->end_of_records())
      DBUG_RETURN(NESTED_LOOP_ERROR);
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
static enum_nested_loop_state
end_send_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	       bool end_of_records)
{
  int idx= -1;
  enum_nested_loop_state ok_code= NESTED_LOOP_OK;
  DBUG_ENTER("end_send_group");

  if (!join->first_record || end_of_records ||
      (idx=test_if_group_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      if (join->procedure)
	join->procedure->end_group();
      if (idx < (int) join->send_group_parts)
      {
	int error=0;
	if (join->procedure)
	{
	  if (join->having && join->having->val_int() == 0)
	    error= -1;				// Didn't satisfy having
 	  else
	  {
	    if (join->do_send_rows)
	      error=join->procedure->send_row(*join->fields) ? 1 : 0;
	    join->send_records++;
	  }
	  if (end_of_records && join->procedure->end_of_records())
	    error= 1;				// Fatal error
	}
	else
	{
	  if (!join->first_record)
	  {
	    /* No matching rows for group function */
	    join->clear();
	  }
	  if (join->having && join->having->val_int() == 0)
	    error= -1;				// Didn't satisfy having
	  else
	  {
	    if (join->do_send_rows)
	      error=join->result->send_data(*join->fields) ? 1 : 0;
	    join->send_records++;
	  }
	  if (join->rollup.state != ROLLUP::STATE_NONE && error <= 0)
	  {
	    if (join->rollup_send_data((uint) (idx+1)))
	      error= 1;
	  }
	}
	if (error > 0)
          DBUG_RETURN(NESTED_LOOP_ERROR);        /* purecov: inspected */
	if (end_of_records)
	  DBUG_RETURN(NESTED_LOOP_OK);
	if (join->send_records >= join->unit->select_limit_cnt &&
	    join->do_send_rows)
	{
	  if (!(join->select_options & OPTION_FOUND_ROWS))
	    DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT); // Abort nicely
	  join->do_send_rows=0;
	  join->unit->select_limit_cnt = HA_POS_ERROR;
        }
        else if (join->send_records >= join->fetch_limit)
        {
          /*
            There is a server side cursor and all rows
            for this fetch request are sent.
          */
          /*
            Preventing code duplication. When finished with the group reset
            the group functions and copy_fields. We fall through. bug #11904
          */
          ok_code= NESTED_LOOP_CURSOR_LIMIT;
        }
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      /*
        This branch is executed also for cursors which have finished their
        fetch limit - the reason for ok_code.
      */
      copy_fields(&join->tmp_table_param);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
	DBUG_RETURN(NESTED_LOOP_ERROR);
      if (join->procedure)
	join->procedure->add();
      DBUG_RETURN(ok_code);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(NESTED_LOOP_ERROR);
  if (join->procedure)
    join->procedure->add();
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
static enum_nested_loop_state
end_write(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	  bool end_of_records)
{
  TABLE *table=join->tmp_table;
  DBUG_ENTER("end_write");

  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }
  if (!end_of_records)
  {
    copy_fields(&join->tmp_table_param);
    copy_funcs(join->tmp_table_param.items_to_copy);

#ifdef TO_BE_DELETED
    if (!table->uniques)			// If not unique handling
    {
      /* Copy null values from group to row */
      ORDER   *group;
      for (group=table->group ; group ; group=group->next)
      {
	Item *item= *group->item;
	if (item->maybe_null)
	{
	  Field *field=item->get_tmp_table_field();
	  field->ptr[-1]= (byte) (field->is_null() ? 1 : 0);
	}
      }
    }
#endif
    if (!join->having || join->having->val_int())
    {
      int error;
      join->found_records++;
      if ((error=table->file->write_row(table->record[0])))
      {
	if (error == HA_ERR_FOUND_DUPP_KEY ||
	    error == HA_ERR_FOUND_DUPP_UNIQUE)
	  goto end;
	if (create_myisam_from_heap(join->thd, table, &join->tmp_table_param,
				    error,1))
	  DBUG_RETURN(NESTED_LOOP_ERROR);        // Not a table_is_full error
	table->s->uniques=0;			// To ensure rows are the same
      }
      if (++join->send_records >= join->tmp_table_param.end_write_records &&
	  join->do_send_rows)
      {
	if (!(join->select_options & OPTION_FOUND_ROWS))
	  DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);
	join->do_send_rows=0;
	join->unit->select_limit_cnt = HA_POS_ERROR;
	DBUG_RETURN(NESTED_LOOP_OK);
      }
    }
  }
end:
  DBUG_RETURN(NESTED_LOOP_OK);
}

/* Group by searching after group record and updating it if possible */
/* ARGSUSED */

static enum_nested_loop_state
end_update(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	   bool end_of_records)
{
  TABLE *table=join->tmp_table;
  ORDER   *group;
  int	  error;
  DBUG_ENTER("end_update");

  if (end_of_records)
    DBUG_RETURN(NESTED_LOOP_OK);
  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }

  join->found_records++;
  copy_fields(&join->tmp_table_param);		// Groups are copied twice.
  /* Make a key of group index */
  for (group=table->group ; group ; group=group->next)
  {
    Item *item= *group->item;
    item->save_org_in_field(group->field);
    /* Store in the used key if the field was 0 */
    if (item->maybe_null)
      group->buff[-1]=item->null_value ? 1 : 0;
  }
  if (!table->file->index_read(table->record[1],
			       join->tmp_table_param.group_buff,0,
			       HA_READ_KEY_EXACT))
  {						/* Update old record */
    restore_record(table,record[1]);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->update_row(table->record[1],
				       table->record[0])))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    DBUG_RETURN(NESTED_LOOP_OK);
  }

  /*
    Copy null bits from group key to table
    We can't copy all data as the key may have different format
    as the row data (for example as with VARCHAR keys)
  */
  KEY_PART_INFO *key_part;
  for (group=table->group,key_part=table->key_info[0].key_part;
       group ;
       group=group->next,key_part++)
  {
    if (key_part->null_bit)
      memcpy(table->record[0]+key_part->offset, group->buff, 1);
  }
  init_tmptable_sum_functions(join->sum_funcs);
  copy_funcs(join->tmp_table_param.items_to_copy);
  if ((error=table->file->write_row(table->record[0])))
  {
    if (create_myisam_from_heap(join->thd, table, &join->tmp_table_param,
				error, 0))
      DBUG_RETURN(NESTED_LOOP_ERROR);            // Not a table_is_full error
    /* Change method to update rows */
    table->file->ha_index_init(0);
    join->join_tab[join->tables-1].next_select=end_unique_update;
  }
  join->send_records++;
  DBUG_RETURN(NESTED_LOOP_OK);
}


/* Like end_update, but this is done with unique constraints instead of keys */

static enum_nested_loop_state
end_unique_update(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		  bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int	  error;
  DBUG_ENTER("end_unique_update");

  if (end_of_records)
    DBUG_RETURN(NESTED_LOOP_OK);
  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }

  init_tmptable_sum_functions(join->sum_funcs);
  copy_fields(&join->tmp_table_param);		// Groups are copied twice.
  copy_funcs(join->tmp_table_param.items_to_copy);

  if (!(error=table->file->write_row(table->record[0])))
    join->send_records++;			// New group
  else
  {
    if ((int) table->file->get_dup_key(error) < 0)
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    if (table->file->rnd_pos(table->record[1],table->file->dupp_ref))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    restore_record(table,record[1]);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->update_row(table->record[1],
				       table->record[0])))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
static enum_nested_loop_state
end_write_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int	  idx= -1;
  DBUG_ENTER("end_write_group");

  if (join->thd->killed)
  {						// Aborted by user
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }
  if (!join->first_record || end_of_records ||
      (idx=test_if_group_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      if (join->procedure)
	join->procedure->end_group();
      int send_group_parts= join->send_group_parts;
      if (idx < send_group_parts)
      {
	if (!join->first_record)
	{
	  /* No matching rows for group function */
	  join->clear();
	}
        copy_sum_funcs(join->sum_funcs,
                       join->sum_funcs_end[send_group_parts]);
	if (!join->having || join->having->val_int())
	{
          int error= table->file->write_row(table->record[0]);
          if (error && create_myisam_from_heap(join->thd, table,
                                               &join->tmp_table_param,
                                               error, 0))
	    DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        if (join->rollup.state != ROLLUP::STATE_NONE)
	{
	  if (join->rollup_write_data((uint) (idx+1), table))
	    DBUG_RETURN(NESTED_LOOP_ERROR);
	}
	if (end_of_records)
	  DBUG_RETURN(NESTED_LOOP_OK);
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      copy_funcs(join->tmp_table_param.items_to_copy);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
	DBUG_RETURN(NESTED_LOOP_ERROR);
      if (join->procedure)
	join->procedure->add();
      DBUG_RETURN(NESTED_LOOP_OK);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(NESTED_LOOP_ERROR);
  if (join->procedure)
    join->procedure->add();
  DBUG_RETURN(NESTED_LOOP_OK);
}


/*****************************************************************************
  Remove calculation with tables that aren't yet read. Remove also tests
  against fields that are read through key where the table is not a
  outer join table.
  We can't remove tests that are made against columns which are stored
  in sorted order.
*****************************************************************************/

/* Return 1 if right_item is used removable reference key on left_item */

static bool test_if_ref(Item_field *left_item,Item *right_item)
{
  Field *field=left_item->field;
  // No need to change const test. We also have to keep tests on LEFT JOIN
  if (!field->table->const_table && !field->table->maybe_null)
  {
    Item *ref_item=part_of_refkey(field->table,field);
    if (ref_item && ref_item->eq(right_item,1))
    {
      if (right_item->type() == Item::FIELD_ITEM)
	return (field->eq_def(((Item_field *) right_item)->field));
      if (right_item->const_item() && !(right_item->is_null()))
      {
	/*
	  We can remove binary fields and numerical fields except float,
	  as float comparison isn't 100 % secure
	  We have to keep normal strings to be able to check for end spaces
	*/
	if (field->binary() &&
	    field->real_type() != MYSQL_TYPE_STRING &&
	    field->real_type() != MYSQL_TYPE_VARCHAR &&
	    (field->type() != FIELD_TYPE_FLOAT || field->decimals() == 0))
	{
	  return !store_val_in_field(field,right_item);
	}
      }
    }
  }
  return 0;					// keep test
}


static COND *
make_cond_for_table(COND *cond, table_map tables, table_map used_table)
{
  if (used_table && !(cond->used_tables() & used_table))
    return (COND*) 0;				// Already checked
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (COND*) 0;			// OOM /* purecov: inspected */
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_table(item,tables,used_table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (COND*) 0;			// Always true
      case 1:
	return new_cond->argument_list()->head();
      default:
	/*
	  Item_cond_and do not need fix_fields for execution, its parameters
	  are fixed or do not need fix_fields, too
	*/
	new_cond->quick_fix_field();
	new_cond->used_tables_cache=
	  ((Item_cond_and*) cond)->used_tables_cache &
	  tables;
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (COND*) 0;			// OOM /* purecov: inspected */
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_table(item,tables,0L);
	if (!fix)
	  return (COND*) 0;			// Always true
	new_cond->argument_list()->push_back(fix);
      }
      /*
	Item_cond_and do not need fix_fields for execution, its parameters
	are fixed or do not need fix_fields, too
      */
      new_cond->quick_fix_field();
      new_cond->used_tables_cache= ((Item_cond_or*) cond)->used_tables_cache;
      new_cond->top_level_item();
      return new_cond;
    }
  }

  /*
    Because the following test takes a while and it can be done
    table_count times, we mark each item that we have examined with the result
    of the test
  */

  if (cond->marker == 3 || (cond->used_tables() & ~tables))
    return (COND*) 0;				// Can't check this yet
  if (cond->marker == 2 || cond->eq_cmp_result() == Item::COND_OK)
    return cond;				// Not boolean op

  if (((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM &&
	test_if_ref((Item_field*) left_item,right_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
    if (right_item->type() == Item::FIELD_ITEM &&
	test_if_ref((Item_field*) right_item,left_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
  }
  cond->marker=2;
  return cond;
}

static Item *
part_of_refkey(TABLE *table,Field *field)
{
  if (!table->reginfo.join_tab)
    return (Item*) 0;             // field from outer non-select (UPDATE,...)

  uint ref_parts=table->reginfo.join_tab->ref.key_parts;
  if (ref_parts)
  {
    KEY_PART_INFO *key_part=
      table->key_info[table->reginfo.join_tab->ref.key].key_part;

    for (uint part=0 ; part < ref_parts ; part++,key_part++)
      if (field->eq(key_part->field) &&
	  !(key_part->key_part_flag & HA_PART_KEY_SEG))
	return table->reginfo.join_tab->ref.items[part];
  }
  return (Item*) 0;
}


/*****************************************************************************
  Test if one can use the key to resolve ORDER BY

  SYNOPSIS
    test_if_order_by_key()
    order		Sort order
    table		Table to sort
    idx			Index to check
    used_key_parts	Return value for used key parts.


  NOTES
    used_key_parts is set to correct key parts used if return value != 0
    (On other cases, used_key_part may be changed)

  RETURN
    1   key is ok.
    0   Key can't be used
    -1  Reverse key can be used
*****************************************************************************/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint idx,
				uint *used_key_parts)
{
  KEY_PART_INFO *key_part,*key_part_end;
  key_part=table->key_info[idx].key_part;
  key_part_end=key_part+table->key_info[idx].key_parts;
  key_part_map const_key_parts=table->const_key_parts[idx];
  int reverse=0;
  DBUG_ENTER("test_if_order_by_key");

  for (; order ; order=order->next, const_key_parts>>=1)
  {
    Field *field=((Item_field*) (*order->item))->field;
    int flag;

    /*
      Skip key parts that are constants in the WHERE clause.
      These are already skipped in the ORDER BY by const_expression_in_where()
    */
    for (; const_key_parts & 1 ; const_key_parts>>= 1)
      key_part++; 

    if (key_part == key_part_end || key_part->field != field)
      DBUG_RETURN(0);

    /* set flag to 1 if we can use read-next on key, else to -1 */
    flag= ((order->asc == !(key_part->key_part_flag & HA_REVERSE_SORT)) ?
           1 : -1);
    if (reverse && flag != reverse)
      DBUG_RETURN(0);
    reverse=flag;				// Remember if reverse
    key_part++;
  }
  *used_key_parts= (uint) (key_part - table->key_info[idx].key_part);
  if (reverse == -1 && !(table->file->index_flags(idx, *used_key_parts-1, 1) &
                         HA_READ_PREV))
    reverse= 0;                                 // Index can't be used
  DBUG_RETURN(reverse);
}


uint find_shortest_key(TABLE *table, const key_map *usable_keys)
{
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  if (!usable_keys->is_clear_all())
  {
    for (uint nr=0; nr < table->s->keys ; nr++)
    {
      if (usable_keys->is_set(nr))
      {
        if (table->key_info[nr].key_length < min_length)
        {
          min_length=table->key_info[nr].key_length;
          best=nr;
        }
      }
    }
  }
  return best;
}

/*
  Test if a second key is the subkey of the first one.

  SYNOPSIS
    is_subkey()
    key_part		First key parts
    ref_key_part	Second key parts
    ref_key_part_end	Last+1 part of the second key

  NOTE
    Second key MUST be shorter than the first one.

  RETURN
    1	is a subkey
    0	no sub key
*/

inline bool 
is_subkey(KEY_PART_INFO *key_part, KEY_PART_INFO *ref_key_part,
	  KEY_PART_INFO *ref_key_part_end)
{
  for (; ref_key_part < ref_key_part_end; key_part++, ref_key_part++)
    if (!key_part->field->eq(ref_key_part->field))
      return 0;
  return 1;
}

/*
  Test if we can use one of the 'usable_keys' instead of 'ref' key for sorting

  SYNOPSIS
    test_if_subkey()
    ref			Number of key, used for WHERE clause
    usable_keys		Keys for testing

  RETURN
    MAX_KEY			If we can't use other key
    the number of found key	Otherwise
*/

static uint
test_if_subkey(ORDER *order, TABLE *table, uint ref, uint ref_key_parts,
	       const key_map *usable_keys)
{
  uint nr;
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  uint not_used;
  KEY_PART_INFO *ref_key_part= table->key_info[ref].key_part;
  KEY_PART_INFO *ref_key_part_end= ref_key_part + ref_key_parts;

  for (nr= 0 ; nr < table->s->keys ; nr++)
  {
    if (usable_keys->is_set(nr) &&
	table->key_info[nr].key_length < min_length &&
	table->key_info[nr].key_parts >= ref_key_parts &&
	is_subkey(table->key_info[nr].key_part, ref_key_part,
		  ref_key_part_end) &&
	test_if_order_by_key(order, table, nr, &not_used))
    {
      min_length= table->key_info[nr].key_length;
      best= nr;
    }
  }
  return best;
}

/*
  Test if we can skip the ORDER BY by using an index.

  If we can use an index, the JOIN_TAB / tab->select struct
  is changed to use the index.

  Return:
     0 We have to use filesort to do the sorting
     1 We can use an index.
*/

static bool
test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,ha_rows select_limit,
			bool no_changes)
{
  int ref_key;
  uint ref_key_parts;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  key_map usable_keys;
  DBUG_ENTER("test_if_skip_sort_order");
  LINT_INIT(ref_key_parts);

  /* Check which keys can be used to resolve ORDER BY */
  usable_keys.set_all();
  for (ORDER *tmp_order=order; tmp_order ; tmp_order=tmp_order->next)
  {
    Item *item= (*tmp_order->item)->real_item();
    if (item->type() != Item::FIELD_ITEM)
    {
      usable_keys.clear_all();
      DBUG_RETURN(0);
    }
    usable_keys.intersect(((Item_field*) item)->field->part_of_sortkey);
    if (usable_keys.is_clear_all())
      DBUG_RETURN(0);					// No usable keys
  }

  ref_key= -1;
  /* Test if constant range in WHERE */
  if (tab->ref.key >= 0 && tab->ref.key_parts)
  {
    ref_key=	   tab->ref.key;
    ref_key_parts= tab->ref.key_parts;
    if (tab->type == JT_REF_OR_NULL || tab->type == JT_FT)
      DBUG_RETURN(0);
  }
  else if (select && select->quick)		// Range found by opt_range
  {
    int quick_type= select->quick->get_type();
    /* 
      assume results are not ordered when index merge is used 
      TODO: sergeyp: Results of all index merge selects actually are ordered 
      by clustered PK values.
    */
  
    if (quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE || 
        quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION || 
        quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT)
      DBUG_RETURN(0);
    ref_key=	   select->quick->index;
    ref_key_parts= select->quick->used_key_parts;
  }

  if (ref_key >= 0)
  {
    /*
      We come here when there is a REF key.
    */
    int order_direction;
    uint used_key_parts;
    if (!usable_keys.is_set(ref_key))
    {
      /*
	We come here when ref_key is not among usable_keys
      */
      uint new_ref_key;
      /*
	If using index only read, only consider other possible index only
	keys
      */
      if (table->used_keys.is_set(ref_key))
	usable_keys.intersect(table->used_keys);
      if ((new_ref_key= test_if_subkey(order, table, ref_key, ref_key_parts,
				       &usable_keys)) < MAX_KEY)
      {
	/* Found key that can be used to retrieve data in sorted order */
	if (tab->ref.key >= 0)
	{
          /*
            We'll use ref access method on key new_ref_key. In general case 
            the index search tuple for new_ref_key will be different (e.g.
            when one index is defined as (part1, part2, ...) and another as
            (part1, part2(N), ...) and the WHERE clause contains 
            "part1 = const1 AND part2=const2". 
            So we build tab->ref from scratch here.
          */
          KEYUSE *keyuse= tab->keyuse;
          while (keyuse->key != new_ref_key && keyuse->table == tab->table)
            keyuse++;
          if (create_ref_for_key(tab->join, tab, keyuse, 
                                 tab->join->const_table_map))
            DBUG_RETURN(0);
	}
	else
	{
          /*
            The range optimizer constructed QUICK_RANGE for ref_key, and
            we want to use instead new_ref_key as the index. We can't
            just change the index of the quick select, because this may
            result in an incosistent QUICK_SELECT object. Below we
            create a new QUICK_SELECT from scratch so that all its
            parameres are set correctly by the range optimizer.
           */
          key_map new_ref_key_map;
          new_ref_key_map.clear_all();  // Force the creation of quick select
          new_ref_key_map.set_bit(new_ref_key); // only for new_ref_key.

          if (select->test_quick_select(tab->join->thd, new_ref_key_map, 0,
                                        (tab->join->select_options &
                                         OPTION_FOUND_ROWS) ?
                                        HA_POS_ERROR :
                                        tab->join->unit->select_limit_cnt,0) <=
              0)
            DBUG_RETURN(0);
	}
        ref_key= new_ref_key;
      }
    }
    /* Check if we get the rows in requested sorted order by using the key */
    if (usable_keys.is_set(ref_key) &&
	(order_direction = test_if_order_by_key(order,table,ref_key,
						&used_key_parts)))
    {
      if (order_direction == -1)		// If ORDER BY ... DESC
      {
	if (select && select->quick)
	{
	  /*
	    Don't reverse the sort order, if it's already done.
	    (In some cases test_if_order_by_key() can be called multiple times
	  */
	  if (!select->quick->reverse_sorted())
	  {
            int quick_type= select->quick->get_type();
            if (quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE ||
                quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT ||
                quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION ||
                quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
              DBUG_RETURN(0);                   // Use filesort
            
            /* ORDER BY range_key DESC */
	    QUICK_SELECT_DESC *tmp=new QUICK_SELECT_DESC((QUICK_RANGE_SELECT*)(select->quick),
							 used_key_parts);
	    if (!tmp || tmp->error)
	    {
	      delete tmp;
	      DBUG_RETURN(0);		// Reverse sort not supported
	    }
	    select->quick=tmp;
	  }
	  DBUG_RETURN(1);
	}
	if (tab->ref.key_parts < used_key_parts)
	{
	  /*
	    SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC

	    Use a traversal function that starts by reading the last row
	    with key part (A) and then traverse the index backwards.
	  */
	  tab->read_first_record=       join_read_last_key;
	  tab->read_record.read_record= join_read_prev_same;
	  /* fall through */
	}
      }
      else if (select && select->quick)
	  select->quick->sorted= 1;
      DBUG_RETURN(1);			/* No need to sort */
    }
  }
  else
  {
    /* check if we can use a key to resolve the group */
    /* Tables using JT_NEXT are handled here */
    uint nr;
    key_map keys;

    /*
      If not used with LIMIT, only use keys if the whole query can be
      resolved with a key;  This is because filesort() is usually faster than
      retrieving all rows through an index.
    */
    if (select_limit >= table->file->records)
    {
      keys= *table->file->keys_to_use_for_scanning();
      keys.merge(table->used_keys);

      /*
	We are adding here also the index specified in FORCE INDEX clause, 
	if any.
        This is to allow users to use index in ORDER BY.
      */
      if (table->force_index) 
	keys.merge(table->keys_in_use_for_query);
      keys.intersect(usable_keys);
    }
    else
      keys= usable_keys;

    for (nr=0; nr < table->s->keys ; nr++)
    {
      uint not_used;
      if (keys.is_set(nr))
      {
	int flag;
	if ((flag= test_if_order_by_key(order, table, nr, &not_used)))
	{
	  if (!no_changes)
	  {
	    tab->index=nr;
	    tab->read_first_record=  (flag > 0 ? join_read_first:
				      join_read_last);
	    tab->type=JT_NEXT;	// Read with index_first(), index_next()
	    if (table->used_keys.is_set(nr))
	    {
	      table->key_read=1;
	      table->file->extra(HA_EXTRA_KEYREAD);
	    }
	  }
	  DBUG_RETURN(1);
	}
      }
    }
  }
  DBUG_RETURN(0);				// Can't use index.
}


/*
  If not selecting by given key, create an index how records should be read

  SYNOPSIS
   create_sort_index()
     thd		Thread handler
     tab		Table to sort (in join structure)
     order		How table should be sorted
     filesort_limit	Max number of rows that needs to be sorted
     select_limit	Max number of rows in final output
		        Used to decide if we should use index or not


  IMPLEMENTATION
   - If there is an index that can be used, 'tab' is modified to use
     this index.
   - If no index, create with filesort() an index file that can be used to
     retrieve rows in order (should be done with 'read_record').
     The sorted data is stored in tab->table and will be freed when calling
     free_io_cache(tab->table).

  RETURN VALUES
    0		ok
    -1		Some fatal error
    1		No records
*/

static int
create_sort_index(THD *thd, JOIN *join, ORDER *order,
		  ha_rows filesort_limit, ha_rows select_limit)
{
  SORT_FIELD *sortorder;
  uint length;
  ha_rows examined_rows;
  TABLE *table;
  SQL_SELECT *select;
  JOIN_TAB *tab;
  DBUG_ENTER("create_sort_index");

  if (join->tables == join->const_tables)
    DBUG_RETURN(0);				// One row, no need to sort
  tab=    join->join_tab + join->const_tables;
  table=  tab->table;
  select= tab->select;

  if (test_if_skip_sort_order(tab,order,select_limit,0))
    DBUG_RETURN(0);
  if (!(sortorder=make_unireg_sortorder(order,&length)))
    goto err;				/* purecov: inspected */
  /* It's not fatal if the following alloc fails */
  table->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
                                             MYF(MY_WME | MY_ZEROFILL));
  table->status=0;				// May be wrong if quick_select

  // If table has a range, move it to select
  if (select && !select->quick && tab->ref.key >= 0)
  {
    if (tab->quick)
    {
      select->quick=tab->quick;
      tab->quick=0;
      /* 
        We can only use 'Only index' if quick key is same as ref_key
        and in index_merge 'Only index' cannot be used
      */
      if (table->key_read && ((uint) tab->ref.key != select->quick->index))
      {
	table->key_read=0;
	table->file->extra(HA_EXTRA_NO_KEYREAD);
      }
    }
    else
    {
      /*
	We have a ref on a const;  Change this to a range that filesort
	can use.
	For impossible ranges (like when doing a lookup on NULL on a NOT NULL
	field, quick will contain an empty record set.
      */
      if (!(select->quick= (tab->type == JT_FT ?
			    new FT_SELECT(thd, table, tab->ref.key) :
			    get_quick_select_for_ref(thd, table, &tab->ref, 
                                                     tab->found_records))))
	goto err;
    }
  }
  if (table->s->tmp_table)
    table->file->info(HA_STATUS_VARIABLE);	// Get record count
  table->sort.found_records=filesort(thd, table,sortorder, length,
                                     select, filesort_limit, &examined_rows);
  tab->records= table->sort.found_records;	// For SQL_CALC_ROWS
  if (select)
  {
    select->cleanup();				// filesort did select
    tab->select= 0;
  }
  tab->select_cond=0;
  tab->last_inner= 0;
  tab->first_unmatched= 0;
  tab->type=JT_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  tab->join->examined_rows+=examined_rows;
  if (table->key_read)				// Restore if we used indexes
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  DBUG_RETURN(table->sort.found_records == HA_POS_ERROR);
err:
  DBUG_RETURN(-1);
}

/*
  Add the HAVING criteria to table->select
*/

#ifdef NOT_YET
static bool fix_having(JOIN *join, Item **having)
{
  (*having)->update_used_tables();	// Some tables may have been const
  JOIN_TAB *table=&join->join_tab[join->const_tables];
  table_map used_tables= join->const_table_map | table->table->map;

  DBUG_EXECUTE("where",print_where(*having,"having"););
  Item* sort_table_cond=make_cond_for_table(*having,used_tables,used_tables);
  if (sort_table_cond)
  {
    if (!table->select)
      if (!(table->select=new SQL_SELECT))
	return 1;
    if (!table->select->cond)
      table->select->cond=sort_table_cond;
    else					// This should never happen
      if (!(table->select->cond= new Item_cond_and(table->select->cond,
						   sort_table_cond)) ||
	  table->select->cond->fix_fields(join->thd, &table->select->cond))
	return 1;
    table->select_cond=table->select->cond;
    table->select_cond->top_level_item();
    DBUG_EXECUTE("where",print_where(table->select_cond,
				     "select and having"););
    *having=make_cond_for_table(*having,~ (table_map) 0,~used_tables);
    DBUG_EXECUTE("where",print_where(*having,"having after make_cond"););
  }
  return 0;
}
#endif


/*****************************************************************************
  Remove duplicates from tmp table
  This should be recoded to add a unique index to the table and remove
  duplicates
  Table is a locked single thread table
  fields is the number of fields to check (from the end)
*****************************************************************************/

static bool compare_record(TABLE *table, Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_offset(table->s->rec_buff_length))
      return 1;
  }
  return 0;
}

static bool copy_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      if (((Field_blob *) (*ptr))->copy())
	return 1;				// Error
  }
  return 0;
}

static void free_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->free();
  }
}


static int
remove_duplicates(JOIN *join, TABLE *entry,List<Item> &fields, Item *having)
{
  int error;
  ulong reclength,offset;
  uint field_count;
  THD *thd= join->thd;
  DBUG_ENTER("remove_duplicates");

  entry->reginfo.lock_type=TL_WRITE;

  /* Calculate how many saved fields there is in list */
  field_count=0;
  List_iterator<Item> it(fields);
  Item *item;
  while ((item=it++))
  {
    if (item->get_tmp_table_field() && ! item->const_item())
      field_count++;
  }

  if (!field_count && !(join->select_options & OPTION_FOUND_ROWS)) 
  {                    // only const items with no OPTION_FOUND_ROWS
    join->unit->select_limit_cnt= 1;		// Only send first row
    DBUG_RETURN(0);
  }
  Field **first_field=entry->field+entry->s->fields - field_count;
  offset= field_count ? 
          entry->field[entry->s->fields - field_count]->offset() : 0;
  reclength=entry->s->reclength-offset;

  free_io_cache(entry);				// Safety
  entry->file->info(HA_STATUS_VARIABLE);
  if (entry->s->db_type == DB_TYPE_HEAP ||
      (!entry->s->blob_fields &&
       ((ALIGN_SIZE(reclength) + HASH_OVERHEAD) * entry->file->records <
	thd->variables.sortbuff_size)))
    error=remove_dup_with_hash_index(join->thd, entry,
				     field_count, first_field,
				     reclength, having);
  else
    error=remove_dup_with_compare(join->thd, entry, first_field, offset,
				  having);

  free_blobs(first_field);
  DBUG_RETURN(error);
}


static int remove_dup_with_compare(THD *thd, TABLE *table, Field **first_field,
				   ulong offset, Item *having)
{
  handler *file=table->file;
  char *org_record,*new_record;
  byte *record;
  int error;
  ulong reclength= table->s->reclength-offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record=(char*) (record=table->record[0])+offset;
  new_record=(char*) table->record[1]+offset;

  file->ha_rnd_init(1);
  error=file->rnd_next(record);
  for (;;)
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      error=0;
      goto err;
    }
    if (error)
    {
      if (error == HA_ERR_RECORD_DELETED)
	continue;
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->delete_row(record)))
	goto err;
      error=file->rnd_next(record);
      continue;
    }
    if (copy_blobs(first_field))
    {
      my_message(ER_OUTOFMEMORY, ER(ER_OUTOFMEMORY), MYF(0));
      error=0;
      goto err;
    }
    memcpy(new_record,org_record,reclength);

    /* Read through rest of file and mark duplicated rows deleted */
    bool found=0;
    for (;;)
    {
      if ((error=file->rnd_next(record)))
      {
	if (error == HA_ERR_RECORD_DELETED)
	  continue;
	if (error == HA_ERR_END_OF_FILE)
	  break;
	goto err;
      }
      if (compare_record(table, first_field) == 0)
      {
	if ((error=file->delete_row(record)))
	  goto err;
      }
      else if (!found)
      {
	found=1;
	file->position(record);	// Remember position
      }
    }
    if (!found)
      break;					// End of file
    /* Restart search on next row */
    error=file->restart_rnd_next(record,file->ref);
  }

  file->extra(HA_EXTRA_NO_CACHE);
  DBUG_RETURN(0);
err:
  file->extra(HA_EXTRA_NO_CACHE);
  if (error)
    file->print_error(error,MYF(0));
  DBUG_RETURN(1);
}


/*
  Generate a hash index for each row to quickly find duplicate rows
  Note that this will not work on tables with blobs!
*/

static int remove_dup_with_hash_index(THD *thd, TABLE *table,
				      uint field_count,
				      Field **first_field,
				      ulong key_length,
				      Item *having)
{
  byte *key_buffer, *key_pos, *record=table->record[0];
  int error;
  handler *file= table->file;
  ulong extra_length= ALIGN_SIZE(key_length)-key_length;
  uint *field_lengths,*field_length;
  HASH hash;
  DBUG_ENTER("remove_dup_with_hash_index");

  if (!my_multi_malloc(MYF(MY_WME),
		       &key_buffer,
		       (uint) ((key_length + extra_length) *
			       (long) file->records),
		       &field_lengths,
		       (uint) (field_count*sizeof(*field_lengths)),
		       NullS))
    DBUG_RETURN(1);

  {
    Field **ptr;
    ulong total_length= 0;
    for (ptr= first_field, field_length=field_lengths ; *ptr ; ptr++)
    {
      uint length= (*ptr)->pack_length();
      (*field_length++)= length;
      total_length+= length;
    }
    DBUG_PRINT("info",("field_count: %u  key_length: %lu  total_length: %lu",
                       field_count, key_length, total_length));
    DBUG_ASSERT(total_length <= key_length);
    key_length= total_length;
    extra_length= ALIGN_SIZE(key_length)-key_length;
  }

  if (hash_init(&hash, &my_charset_bin, (uint) file->records, 0, 
		key_length, (hash_get_key) 0, 0, 0))
  {
    my_free((char*) key_buffer,MYF(0));
    DBUG_RETURN(1);
  }

  file->ha_rnd_init(1);
  key_pos=key_buffer;
  for (;;)
  {
    byte *org_key_pos;
    if (thd->killed)
    {
      thd->send_kill_message();
      error=0;
      goto err;
    }
    if ((error=file->rnd_next(record)))
    {
      if (error == HA_ERR_RECORD_DELETED)
	continue;
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->delete_row(record)))
	goto err;
      continue;
    }

    /* copy fields to key buffer */
    org_key_pos= key_pos;
    field_length=field_lengths;
    for (Field **ptr= first_field ; *ptr ; ptr++)
    {
      (*ptr)->sort_string((char*) key_pos,*field_length);
      key_pos+= *field_length++;
    }
    /* Check if it exists before */
    if (hash_search(&hash, org_key_pos, key_length))
    {
      /* Duplicated found ; Remove the row */
      if ((error=file->delete_row(record)))
	goto err;
    }
    else
      (void) my_hash_insert(&hash, org_key_pos);
    key_pos+=extra_length;
  }
  my_free((char*) key_buffer,MYF(0));
  hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->ha_rnd_end();
  DBUG_RETURN(0);

err:
  my_free((char*) key_buffer,MYF(0));
  hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->ha_rnd_end();
  if (error)
    file->print_error(error,MYF(0));
  DBUG_RETURN(1);
}


SORT_FIELD *make_unireg_sortorder(ORDER *order, uint *length)
{
  uint count;
  SORT_FIELD *sort,*pos;
  DBUG_ENTER("make_unireg_sortorder");

  count=0;
  for (ORDER *tmp = order; tmp; tmp=tmp->next)
    count++;
  pos=sort=(SORT_FIELD*) sql_alloc(sizeof(SORT_FIELD)*(count+1));
  if (!pos)
    return 0;

  for (;order;order=order->next,pos++)
  {
    pos->field=0; pos->item=0;
    if (order->item[0]->type() == Item::FIELD_ITEM)
      pos->field= ((Item_field*) (*order->item))->field;
    else if (order->item[0]->type() == Item::SUM_FUNC_ITEM &&
	     !order->item[0]->const_item())
      pos->field= ((Item_sum*) order->item[0])->get_tmp_table_field();
    else if (order->item[0]->type() == Item::COPY_STR_ITEM)
    {						// Blob patch
      pos->item= ((Item_copy_string*) (*order->item))->item;
    }
    else
      pos->item= *order->item;
    pos->reverse=! order->asc;
  }
  *length=count;
  DBUG_RETURN(sort);
}


/*****************************************************************************
  Fill join cache with packed records
  Records are stored in tab->cache.buffer and last record in
  last record is stored with pointers to blobs to support very big
  records
******************************************************************************/

static int
join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count)
{
  reg1 uint i;
  uint length,blobs,size;
  CACHE_FIELD *copy,**blob_ptr;
  JOIN_CACHE  *cache;
  JOIN_TAB *join_tab;
  DBUG_ENTER("join_init_cache");

  cache= &tables[table_count].cache;
  cache->fields=blobs=0;

  join_tab=tables;
  for (i=0 ; i < table_count ; i++,join_tab++)
  {
    if (!join_tab->used_fieldlength)		/* Not calced yet */
      calc_used_field_length(thd, join_tab);
    cache->fields+=join_tab->used_fields;
    blobs+=join_tab->used_blobs;
  }
  if (!(cache->field=(CACHE_FIELD*)
	sql_alloc(sizeof(CACHE_FIELD)*(cache->fields+table_count*2)+(blobs+1)*

		  sizeof(CACHE_FIELD*))))
  {
    my_free((gptr) cache->buff,MYF(0));		/* purecov: inspected */
    cache->buff=0;				/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  copy=cache->field;
  blob_ptr=cache->blob_ptr=(CACHE_FIELD**)
    (cache->field+cache->fields+table_count*2);

  length=0;
  for (i=0 ; i < table_count ; i++)
  {
    uint null_fields=0,used_fields;

    Field **f_ptr,*field;
    for (f_ptr=tables[i].table->field,used_fields=tables[i].used_fields ;
	 used_fields ;
	 f_ptr++)
    {
      field= *f_ptr;
      if (field->query_id == thd->query_id)
      {
	used_fields--;
	length+=field->fill_cache_field(copy);
	if (copy->blob_field)
	  (*blob_ptr++)=copy;
	if (field->maybe_null())
	  null_fields++;
	copy++;
      }
    }
    /* Copy null bits from table */
    if (null_fields && tables[i].table->s->null_fields)
    {						/* must copy null bits */
      copy->str=(char*) tables[i].table->null_flags;
      copy->length= tables[i].table->s->null_bytes;
      copy->strip=0;
      copy->blob_field=0;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
    /* If outer join table, copy null_row flag */
    if (tables[i].table->maybe_null)
    {
      copy->str= (char*) &tables[i].table->null_row;
      copy->length=sizeof(tables[i].table->null_row);
      copy->strip=0;
      copy->blob_field=0;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
  }

  cache->length=length+blobs*sizeof(char*);
  cache->blobs=blobs;
  *blob_ptr=0;					/* End sequentel */
  size=max(thd->variables.join_buff_size, cache->length);
  if (!(cache->buff=(uchar*) my_malloc(size,MYF(0))))
    DBUG_RETURN(1);				/* Don't use cache */ /* purecov: inspected */
  cache->end=cache->buff+size;
  reset_cache_write(cache);
  DBUG_RETURN(0);
}


static ulong
used_blob_length(CACHE_FIELD **ptr)
{
  uint length,blob_length;
  for (length=0 ; *ptr ; ptr++)
  {
    (*ptr)->blob_length=blob_length=(*ptr)->blob_field->get_length();
    length+=blob_length;
    (*ptr)->blob_field->get_ptr(&(*ptr)->str);
  }
  return length;
}


static bool
store_record_in_cache(JOIN_CACHE *cache)
{
  uint length;
  uchar *pos;
  CACHE_FIELD *copy,*end_field;
  bool last_record;

  pos=cache->pos;
  end_field=cache->field+cache->fields;

  length=cache->length;
  if (cache->blobs)
    length+=used_blob_length(cache->blob_ptr);
  if ((last_record=(length+cache->length > (uint) (cache->end - pos))))
    cache->ptr_record=cache->records;

  /*
    There is room in cache. Put record there
  */
  cache->records++;
  for (copy=cache->field ; copy < end_field; copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
	copy->blob_field->get_image((char*) pos,copy->length+sizeof(char*), 
				    copy->blob_field->charset());
	pos+=copy->length+sizeof(char*);
      }
      else
      {
	copy->blob_field->get_image((char*) pos,copy->length, // blob length
				    copy->blob_field->charset());
	memcpy(pos+copy->length,copy->str,copy->blob_length);  // Blob data
	pos+=copy->length+copy->blob_length;
      }
    }
    else
    {
      if (copy->strip)
      {
	char *str,*end;
	for (str=copy->str,end= str+copy->length;
	     end > str && end[-1] == ' ' ;
	     end--) ;
	length=(uint) (end-str);
	memcpy(pos+2, str, length);
        int2store(pos, length);
	pos+= length+2;
      }
      else
      {
	memcpy(pos,copy->str,copy->length);
	pos+=copy->length;
      }
    }
  }
  cache->pos=pos;
  return last_record || (uint) (cache->end -pos) < cache->length;
}


static void
reset_cache_read(JOIN_CACHE *cache)
{
  cache->record_nr=0;
  cache->pos=cache->buff;
}


static void reset_cache_write(JOIN_CACHE *cache)
{
  reset_cache_read(cache);
  cache->records= 0;
  cache->ptr_record= (uint) ~0;
}


static void
read_cached_record(JOIN_TAB *tab)
{
  uchar *pos;
  uint length;
  bool last_record;
  CACHE_FIELD *copy,*end_field;

  last_record=tab->cache.record_nr++ == tab->cache.ptr_record;
  pos=tab->cache.pos;

  for (copy=tab->cache.field,end_field=copy+tab->cache.fields ;
       copy < end_field;
       copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
	copy->blob_field->set_image((char*) pos,copy->length+sizeof(char*),
				    copy->blob_field->charset());
	pos+=copy->length+sizeof(char*);
      }
      else
      {
	copy->blob_field->set_ptr((char*) pos,(char*) pos+copy->length);
	pos+=copy->length+copy->blob_field->get_length();
      }
    }
    else
    {
      if (copy->strip)
      {
        length= uint2korr(pos);
	memcpy(copy->str, pos+2, length);
	memset(copy->str+length, ' ', copy->length-length);
	pos+= 2 + length;
      }
      else
      {
	memcpy(copy->str,pos,copy->length);
	pos+=copy->length;
      }
    }
  }
  tab->cache.pos=pos;
  return;
}


static bool
cmp_buffer_with_ref(JOIN_TAB *tab)
{
  bool diff;
  if (!(diff=tab->ref.key_err))
  {
    memcpy(tab->ref.key_buff2, tab->ref.key_buff, tab->ref.key_length);
  }
  if ((tab->ref.key_err= cp_buffer_from_ref(tab->join->thd, &tab->ref)) || 
      diff)
    return 1;
  return memcmp(tab->ref.key_buff2, tab->ref.key_buff, tab->ref.key_length)
    != 0;
}


bool
cp_buffer_from_ref(THD *thd, TABLE_REF *ref)
{
  enum enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  for (store_key **copy=ref->key_copy ; *copy ; copy++)
  {
    if ((*copy)->copy() & 1)
    {
      thd->count_cuted_fields= save_count_cuted_fields;
      return 1;                                 // Something went wrong
    }
  }
  thd->count_cuted_fields= save_count_cuted_fields;
  return 0;
}


/*****************************************************************************
  Group and order functions
*****************************************************************************/

/*
  Resolve an ORDER BY or GROUP BY column reference.

  SYNOPSIS
    find_order_in_list()
    thd		      [in]     Pointer to current thread structure
    ref_pointer_array [in/out] All select, group and order by fields
    tables            [in]     List of tables to search in (usually FROM clause)
    order             [in]     Column reference to be resolved
    fields            [in]     List of fields to search in (usually SELECT list)
    all_fields        [in/out] All select, group and order by fields
    is_group_field    [in]     True if order is a GROUP field, false if
                               ORDER by field

  DESCRIPTION
    Given a column reference (represented by 'order') from a GROUP BY or ORDER
    BY clause, find the actual column it represents. If the column being
    resolved is from the GROUP BY clause, the procedure searches the SELECT
    list 'fields' and the columns in the FROM list 'tables'. If 'order' is from
    the ORDER BY clause, only the SELECT list is being searched.

    If 'order' is resolved to an Item, then order->item is set to the found
    Item. If there is no item for the found column (that is, it was resolved
    into a table field), order->item is 'fixed' and is added to all_fields and
    ref_pointer_array.

  RETURN
    FALSE if OK
    TRUE  if error occurred

    ref_pointer_array and all_fields are updated
*/

static bool
find_order_in_list(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
                   ORDER *order, List<Item> &fields, List<Item> &all_fields,
                   bool is_group_field)
{
  Item *order_item= *order->item; /* The item from the GROUP/ORDER caluse. */
  Item::Type order_item_type;
  Item **select_item; /* The corresponding item from the SELECT clause. */
  Field *from_field;  /* The corresponding field from the FROM clause. */

  if (order_item->type() == Item::INT_ITEM)
  {						/* Order by position */
    uint count= (uint) order_item->val_int();
    if (!count || count > fields.elements)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0),
               order_item->full_name(), thd->where);
      return TRUE;
    }
    order->item= ref_pointer_array + count - 1;
    order->in_field_list= 1;
    order->counter= count;
    order->counter_used= 1;
    return FALSE;
  }
  /* Lookup the current GROUP/ORDER field in the SELECT clause. */
  uint counter;
  bool unaliased;
  select_item= find_item_in_list(order_item, fields, &counter,
                                 REPORT_EXCEPT_NOT_FOUND, &unaliased);
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
    if (unaliased && !order_item->fixed &&
        order_item->fix_fields(thd, order->item))
      return TRUE;

    /* Lookup the current GROUP field in the FROM clause. */
    order_item_type= order_item->type();
    from_field= (Field*) not_found_field;
    if (is_group_field &&
        order_item_type == Item::FIELD_ITEM ||
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
      order->item= ref_pointer_array + counter;
      order->in_field_list=1;
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
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR), from_field->field_name,
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
  */
  if (!order_item->fixed &&
      (order_item->fix_fields(thd, order->item) ||
       (order_item= *order->item)->check_cols(1) ||
       thd->is_fatal_error))
    return TRUE; /* Wrong field. */

  uint el= all_fields.elements;
  all_fields.push_front(order_item); /* Add new field to field list. */
  ref_pointer_array[el]= order_item;
  order->item= ref_pointer_array + el;
  return FALSE;
}


/*
  Change order to point at item in select list. If item isn't a number
  and doesn't exits in the select list, add it the the field list.
*/

int setup_order(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List<Item> &all_fields, ORDER *order)
{
  thd->where="order clause";
  for (; order; order=order->next)
  {
    if (find_order_in_list(thd, ref_pointer_array, tables, order, fields,
			   all_fields, FALSE))
      return 1;
  }
  return 0;
}


/*
  Intitialize the GROUP BY list.

  SYNOPSIS
   setup_group()
   thd			Thread handler
   ref_pointer_array	We store references to all fields that was not in
			'fields' here.
   fields		All fields in the select part. Any item in 'order'
			that is part of these list is replaced by a pointer
			to this fields.
   all_fields		Total list of all unique fields used by the select.
			All items in 'order' that was not part of fields will
			be added first to this list.
  order			The fields we should do GROUP BY on.
  hidden_group_fields	Pointer to flag that is set to 1 if we added any fields
			to all_fields.

  RETURN
   0  ok
   1  error (probably out of memory)
*/

int
setup_group(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
	    List<Item> &fields, List<Item> &all_fields, ORDER *order,
	    bool *hidden_group_fields)
{
  *hidden_group_fields=0;
  if (!order)
    return 0;				/* Everything is ok */

  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
  {
    Item *item;
    List_iterator<Item> li(fields);
    while ((item=li++))
      item->marker=0;			/* Marker that field is not used */
  }
  uint org_fields=all_fields.elements;

  thd->where="group statement";
  for (; order; order=order->next)
  {
    if (find_order_in_list(thd, ref_pointer_array, tables, order, fields,
			   all_fields, TRUE))
      return 1;
    (*order->item)->marker=1;		/* Mark found */
    if ((*order->item)->with_sum_func)
    {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), (*order->item)->full_name());
      return 1;
    }
  }
  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
  {
    /* Don't allow one to use fields that is not used in GROUP BY */
    Item *item;
    List_iterator<Item> li(fields);

    while ((item=li++))
    {
      if (item->type() != Item::SUM_FUNC_ITEM && !item->marker &&
	  !item->const_item())
      {
	my_error(ER_WRONG_FIELD_WITH_GROUP, MYF(0), item->full_name());
	return 1;
      }
    }
  }
  if (org_fields != all_fields.elements)
    *hidden_group_fields=1;			// group fields is not used
  return 0;
}

/*
  Add fields with aren't used at start of field list. Return FALSE if ok
*/

static bool
setup_new_fields(THD *thd, List<Item> &fields,
		 List<Item> &all_fields, ORDER *new_field)
{
  Item	  **item;
  DBUG_ENTER("setup_new_fields");

  thd->set_query_id=1;				// Not really needed, but...
  uint counter;
  bool not_used;
  for (; new_field ; new_field= new_field->next)
  {
    if ((item= find_item_in_list(*new_field->item, fields, &counter,
				 IGNORE_ERRORS, &not_used)))
      new_field->item=item;			/* Change to shared Item */
    else
    {
      thd->where="procedure list";
      if ((*new_field->item)->fix_fields(thd, new_field->item))
	DBUG_RETURN(1); /* purecov: inspected */
      all_fields.push_front(*new_field->item);
      new_field->item=all_fields.head_ref();
    }
  }
  DBUG_RETURN(0);
}

/*
  Create a group by that consist of all non const fields. Try to use
  the fields in the order given by 'order' to allow one to optimize
  away 'order by'.
*/

static ORDER *
create_distinct_group(THD *thd, Item **ref_pointer_array,
                      ORDER *order_list, List<Item> &fields, 
		      bool *all_order_by_fields_used)
{
  List_iterator<Item> li(fields);
  Item *item;
  ORDER *order,*group,**prev;

  *all_order_by_fields_used= 1;
  while ((item=li++))
    item->marker=0;			/* Marker that field is not used */

  prev= &group;  group=0;
  for (order=order_list ; order; order=order->next)
  {
    if (order->in_field_list)
    {
      ORDER *ord=(ORDER*) thd->memdup((char*) order,sizeof(ORDER));
      if (!ord)
	return 0;
      *prev=ord;
      prev= &ord->next;
      (*ord->item)->marker=1;
    }
    else
      *all_order_by_fields_used= 0;
  }

  li.rewind();
  while ((item=li++))
  {
    if (!item->const_item() && !item->with_sum_func && !item->marker)
    {
      ORDER *ord=(ORDER*) thd->calloc(sizeof(ORDER));
      if (!ord)
	return 0;
      /*
        We have here only field_list (not all_field_list), so we can use
        simple indexing of ref_pointer_array (order in the array and in the
        list are same)
      */
      ord->item= ref_pointer_array;
      ord->asc=1;
      *prev=ord;
      prev= &ord->next;
    }
    ref_pointer_array++;
  }
  *prev=0;
  return group;
}


/*****************************************************************************
  Update join with count of the different type of fields
*****************************************************************************/

void
count_field_types(TMP_TABLE_PARAM *param, List<Item> &fields,
		  bool reset_with_sum_func)
{
  List_iterator<Item> li(fields);
  Item *field;

  param->field_count=param->sum_func_count=param->func_count=
    param->hidden_field_count=0;
  param->quick_group=1;
  while ((field=li++))
  {
    Item::Type type=field->real_item()->type();
    if (type == Item::FIELD_ITEM)
      param->field_count++;
    else if (type == Item::SUM_FUNC_ITEM)
    {
      if (! field->const_item())
      {
	Item_sum *sum_item=(Item_sum*) field;
	if (!sum_item->quick_group)
	  param->quick_group=0;			// UDF SUM function
	param->sum_func_count++;

	for (uint i=0 ; i < sum_item->arg_count ; i++)
	{
	  if (sum_item->args[0]->real_item()->type() == Item::FIELD_ITEM)
	    param->field_count++;
	  else
	    param->func_count++;
	}
      }
    }
    else
    {
      param->func_count++;
      if (reset_with_sum_func)
	field->with_sum_func=0;
    }
  }
}


/*
  Return 1 if second is a subpart of first argument
  If first parts has different direction, change it to second part
  (group is sorted like order)
*/

static bool
test_if_subpart(ORDER *a,ORDER *b)
{
  for (; a && b; a=a->next,b=b->next)
  {
    if ((*a->item)->eq(*b->item,1))
      a->asc=b->asc;
    else
      return 0;
  }
  return test(!b);
}

/*
  Return table number if there is only one table in sort order
  and group and order is compatible
  else return 0;
*/

static TABLE *
get_sort_by_table(ORDER *a,ORDER *b,TABLE_LIST *tables)
{
  table_map map= (table_map) 0;
  DBUG_ENTER("get_sort_by_table");

  if (!a)
    a=b;					// Only one need to be given
  else if (!b)
    b=a;

  for (; a && b; a=a->next,b=b->next)
  {
    if (!(*a->item)->eq(*b->item,1))
      DBUG_RETURN(0);
    map|=a->item[0]->used_tables();
  }
  if (!map || (map & (RAND_TABLE_BIT | OUTER_REF_TABLE_BIT)))
    DBUG_RETURN(0);

  for (; !(map & tables->table->map); tables= tables->next_leaf);
  if (map != tables->table->map)
    DBUG_RETURN(0);				// More than one table
  DBUG_PRINT("exit",("sort by table: %d",tables->table->tablenr));
  DBUG_RETURN(tables->table);
}


	/* calc how big buffer we need for comparing group entries */

static void
calc_group_buffer(JOIN *join,ORDER *group)
{
  uint key_length=0, parts=0, null_parts=0;

  if (group)
    join->group= 1;
  for (; group ; group=group->next)
  {
    Item *group_item= *group->item;
    Field *field= group_item->get_tmp_table_field();
    if (field)
    {
      if (field->type() == FIELD_TYPE_BLOB)
	key_length+=MAX_BLOB_WIDTH;		// Can't be used as a key
      else if (field->type() == MYSQL_TYPE_VARCHAR)
        key_length+= field->field_length + HA_KEY_BLOB_LENGTH;
      else
	key_length+= field->pack_length();
    }
    else
    { 
      switch (group_item->result_type()) {
      case REAL_RESULT:
        key_length+= sizeof(double);
        break;
      case INT_RESULT:
        key_length+= sizeof(longlong);
        break;
      case DECIMAL_RESULT:
        key_length+= my_decimal_get_binary_size(group_item->max_length - 
                                                (group_item->decimals ? 1 : 0),
                                                group_item->decimals);
        break;
      case STRING_RESULT:
        /*
          Group strings are taken as varstrings and require an length field.
          A field is not yet created by create_tmp_field()
          and the sizes should match up.
        */
        key_length+= group_item->max_length + HA_KEY_BLOB_LENGTH;
        break;
      default:
        /* This case should never be choosen */
        DBUG_ASSERT(0);
        join->thd->fatal_error();
      }
    }
    parts++;
    if (group_item->maybe_null)
      null_parts++;
  }
  join->tmp_table_param.group_length=key_length+null_parts;
  join->tmp_table_param.group_parts=parts;
  join->tmp_table_param.group_null_parts=null_parts;
}


/*
  allocate group fields or take prepared (cached)

  SYNOPSIS
    make_group_fields()
    main_join - join of current select
    curr_join - current join (join of current select or temporary copy of it)

  RETURN
    0 - ok
    1 - failed
*/

static bool
make_group_fields(JOIN *main_join, JOIN *curr_join)
{
  if (main_join->group_fields_cache.elements)
  {
    curr_join->group_fields= main_join->group_fields_cache;
    curr_join->sort_and_group= 1;
  }
  else
  {
    if (alloc_group_fields(curr_join, curr_join->group_list))
      return (1);
    main_join->group_fields_cache= curr_join->group_fields;
  }
  return (0);
}


/*
  Get a list of buffers for saveing last group
  Groups are saved in reverse order for easyer check loop
*/

static bool
alloc_group_fields(JOIN *join,ORDER *group)
{
  if (group)
  {
    for (; group ; group=group->next)
    {
      Cached_item *tmp=new_Cached_item(join->thd, *group->item);
      if (!tmp || join->group_fields.push_front(tmp))
	return TRUE;
    }
  }
  join->sort_and_group=1;			/* Mark for do_select */
  return FALSE;
}


static int
test_if_group_changed(List<Cached_item> &list)
{
  DBUG_ENTER("test_if_group_changed");
  List_iterator<Cached_item> li(list);
  int idx= -1,i;
  Cached_item *buff;

  for (i=(int) list.elements-1 ; (buff=li++) ; i--)
  {
    if (buff->cmp())
      idx=i;
  }
  DBUG_PRINT("info", ("idx: %d", idx));
  DBUG_RETURN(idx);
}


/*
  Setup copy_fields to save fields at start of new group

  setup_copy_fields()
    thd - THD pointer
    param - temporary table parameters
    ref_pointer_array - array of pointers to top elements of filed list
    res_selected_fields - new list of items of select item list
    res_all_fields - new list of all items
    elements - number of elements in select item list
    all_fields - all fields list

  DESCRIPTION
    Setup copy_fields to save fields at start of new group
    Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
    Change old item_field to use a new field with points at saved fieldvalue
    This function is only called before use of send_fields
  
  RETURN
    0 - ok
    !=0 - error
*/

bool
setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param,
		  Item **ref_pointer_array,
		  List<Item> &res_selected_fields, List<Item> &res_all_fields,
		  uint elements, List<Item> &all_fields)
{
  Item *pos;
  List_iterator_fast<Item> li(all_fields);
  Copy_field *copy= NULL;
  res_selected_fields.empty();
  res_all_fields.empty();
  List_iterator_fast<Item> itr(res_all_fields);
  List<Item> extra_funcs;
  uint i, border= all_fields.elements - elements;
  DBUG_ENTER("setup_copy_fields");

  if (param->field_count && 
      !(copy=param->copy_field= new Copy_field[param->field_count]))
    goto err2;

  param->copy_funcs.empty();
  for (i= 0; (pos= li++); i++)
  {
    if (pos->real_item()->type() == Item::FIELD_ITEM)
    {
      Item_field *item;
      pos= pos->real_item();
      if (!(item= new Item_field(thd, ((Item_field*) pos))))
	goto err;
      pos= item;
      if (item->field->flags & BLOB_FLAG)
      {
	if (!(pos= new Item_copy_string(pos)))
	  goto err;
       /*
         Item_copy_string::copy for function can call 
         Item_copy_string::val_int for blob via Item_ref.
         But if Item_copy_string::copy for blob isn't called before,
         it's value will be wrong
         so let's insert Item_copy_string for blobs in the beginning of 
         copy_funcs
         (to see full test case look at having.test, BUG #4358) 
       */
	if (param->copy_funcs.push_front(pos))
	  goto err;
      }
      else
      {
	/* 
	   set up save buffer and change result_field to point at 
	   saved value
	*/
	Field *field= item->field;
	item->result_field=field->new_field(thd->mem_root,field->table);
	char *tmp=(char*) sql_alloc(field->pack_length()+1);
	if (!tmp)
	  goto err;
      if (copy)
      {
        copy->set(tmp, item->result_field);
        item->result_field->move_field(copy->to_ptr,copy->to_null_ptr,1);
        copy++;
      }				
      }
    }
    else if ((pos->type() == Item::FUNC_ITEM ||
	      pos->type() == Item::SUBSELECT_ITEM ||
	      pos->type() == Item::CACHE_ITEM ||
	      pos->type() == Item::COND_ITEM) &&
	     !pos->with_sum_func)
    {						// Save for send fields
      /* TODO:
	 In most cases this result will be sent to the user.
	 This should be changed to use copy_int or copy_real depending
	 on how the value is to be used: In some cases this may be an
	 argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      if (!(pos=new Item_copy_string(pos)))
	goto err;
      if (i < border)                           // HAVING, ORDER and GROUP BY
      {
        if (extra_funcs.push_back(pos))
          goto err;
      }
      else if (param->copy_funcs.push_back(pos))
	goto err;
    }
    res_all_fields.push_back(pos);
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      pos;
  }
  param->copy_field_end= copy;

  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  /*
    Put elements from HAVING, ORDER BY and GROUP BY last to ensure that any
    reference used in these will resolve to a item that is already calculated
  */
  param->copy_funcs.concat(&extra_funcs);

  DBUG_RETURN(0);

 err:
  if (copy)
    delete [] param->copy_field;			// This is never 0
  param->copy_field=0;
err2:
  DBUG_RETURN(TRUE);
}


/*
  Make a copy of all simple SELECT'ed items

  This is done at the start of a new group so that we can retrieve
  these later when the group changes.
*/

void
copy_fields(TMP_TABLE_PARAM *param)
{
  Copy_field *ptr=param->copy_field;
  Copy_field *end=param->copy_field_end;

  for (; ptr != end; ptr++)
    (*ptr->do_copy)(ptr);

  List_iterator_fast<Item> it(param->copy_funcs);
  Item_copy_string *item;
  while ((item = (Item_copy_string*) it++))
    item->copy();
}


/*
  Make an array of pointers to sum_functions to speed up sum_func calculation

  SYNOPSIS
    alloc_func_list()

  RETURN
    0	ok
    1	Error
*/

bool JOIN::alloc_func_list()
{
  uint func_count, group_parts;
  DBUG_ENTER("alloc_func_list");

  func_count= tmp_table_param.sum_func_count;
  /*
    If we are using rollup, we need a copy of the summary functions for
    each level
  */
  if (rollup.state != ROLLUP::STATE_NONE)
    func_count*= (send_group_parts+1);

  group_parts= send_group_parts;
  /*
    If distinct, reserve memory for possible
    disctinct->group_by optimization
  */
  if (select_distinct)
    group_parts+= fields_list.elements;

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs= (Item_sum**) thd->calloc(sizeof(Item_sum**) * (func_count+1) +
				      sizeof(Item_sum***) * (group_parts+1));
  sum_funcs_end= (Item_sum***) (sum_funcs+func_count+1);
  DBUG_RETURN(sum_funcs == 0);
}


/*
  Initialize 'sum_funcs' array with all Item_sum objects

  SYNOPSIS
    make_sum_func_list()
    field_list		All items
    send_fields		Items in select list
    before_group_by	Set to 1 if this is called before GROUP BY handling
    recompute           Set to TRUE if sum_funcs must be recomputed

  RETURN
    0  ok
    1  error
*/

bool JOIN::make_sum_func_list(List<Item> &field_list, List<Item> &send_fields,
			      bool before_group_by, bool recompute)
{
  List_iterator_fast<Item> it(field_list);
  Item_sum **func;
  Item *item;
  DBUG_ENTER("make_sum_func_list");

  if (*sum_funcs && !recompute)
    DBUG_RETURN(FALSE); /* We have already initialized sum_funcs. */

  func= sum_funcs;
  while ((item=it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item())
      *func++= (Item_sum*) item;
  }
  if (before_group_by && rollup.state == ROLLUP::STATE_INITED)
  {
    rollup.state= ROLLUP::STATE_READY;
    if (rollup_make_fields(field_list, send_fields, &func))
      DBUG_RETURN(TRUE);			// Should never happen
  }
  else if (rollup.state == ROLLUP::STATE_NONE)
  {
    for (uint i=0 ; i <= send_group_parts ;i++)
      sum_funcs_end[i]= func;
  }
  else if (rollup.state == ROLLUP::STATE_READY)
    DBUG_RETURN(FALSE);                         // Don't put end marker
  *func=0;					// End marker
  DBUG_RETURN(FALSE);
}


/*
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  change_to_use_tmp_fields()
    thd - THD pointer
    ref_pointer_array - array of pointers to top elements of filed list
    res_selected_fields - new list of items of select item list
    res_all_fields - new list of all items
    elements - number of elements in select item list
    all_fields - all fields list

   RETURN
    0 - ok
    !=0 - error
*/

static bool
change_to_use_tmp_fields(THD *thd, Item **ref_pointer_array,
			 List<Item> &res_selected_fields,
			 List<Item> &res_all_fields,
			 uint elements, List<Item> &all_fields)
{
  List_iterator_fast<Item> it(all_fields);
  Item *item_field,*item;
  DBUG_ENTER("change_to_use_tmp_fields");

  res_selected_fields.empty();
  res_all_fields.empty();

  uint i, border= all_fields.elements - elements;
  for (i= 0; (item= it++); i++)
  {
    Field *field;
    
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
      item_field= item;
    else
    {
      if (item->type() == Item::FIELD_ITEM)
      {
	item_field= item->get_tmp_table_item(thd);
      }
      else if ((field= item->get_tmp_table_field()))
      {
	if (item->type() == Item::SUM_FUNC_ITEM && field->table->group)
	  item_field= ((Item_sum*) item)->result_item(field);
	else
	  item_field= (Item*) new Item_field(field);
	if (!item_field)
	  DBUG_RETURN(TRUE);                    // Fatal error
	item_field->name= item->name;
#ifndef DBUG_OFF
	if (_db_on_ && !item_field->name)
	{
	  char buff[256];
	  String str(buff,sizeof(buff),&my_charset_bin);
	  str.length(0);
	  item->print(&str);
	  item_field->name= sql_strmake(str.ptr(),str.length());
	}
#endif
      }
      else
	item_field= item;
    }
    res_all_fields.push_back(item_field);
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      item_field;
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  DBUG_RETURN(FALSE);
}


/*
  Change all sum_func refs to fields to point at fields in tmp table
  Change all funcs to be fields in tmp table

  change_refs_to_tmp_fields()
    thd - THD pointer
    ref_pointer_array - array of pointers to top elements of filed list
    res_selected_fields - new list of items of select item list
    res_all_fields - new list of all items
    elements - number of elements in select item list
    all_fields - all fields list

   RETURN
    0	ok
    1	error
*/

static bool
change_refs_to_tmp_fields(THD *thd, Item **ref_pointer_array,
			  List<Item> &res_selected_fields,
			  List<Item> &res_all_fields, uint elements,
			  List<Item> &all_fields)
{
  List_iterator_fast<Item> it(all_fields);
  Item *item, *new_item;
  res_selected_fields.empty();
  res_all_fields.empty();

  uint i, border= all_fields.elements - elements;
  for (i= 0; (item= it++); i++)
  {
    res_all_fields.push_back(new_item= item->get_tmp_table_item(thd));
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      new_item;
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);

  return thd->is_fatal_error;
}



/******************************************************************************
  Code for calculating functions
******************************************************************************/


/*
  Call ::setup for all sum functions

  SYNOPSIS
    setup_sum_funcs()
    thd           thread handler
    func_ptr      sum function list

  RETURN
    FALSE  ok
    TRUE   error
*/

static bool setup_sum_funcs(THD *thd, Item_sum **func_ptr)
{
  Item_sum *func;
  DBUG_ENTER("setup_sum_funcs");
  while ((func= *(func_ptr++)))
  {
    if (func->setup(thd))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


static void
init_tmptable_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->reset_field();
}


	/* Update record 0 in tmp_table from record 1 */

static void
update_tmptable_sum_func(Item_sum **func_ptr,
			 TABLE *tmp_table __attribute__((unused)))
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->update_field();
}


	/* Copy result of sum functions to record in tmp_table */

static void
copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ; func_ptr++)
    (void) (*func_ptr)->save_in_result_field(1);
  return;
}


static bool
init_sum_functions(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ;func_ptr++)
  {
    if ((*func_ptr)->reset())
      return 1;
  }
  /* If rollup, calculate the upper sum levels */
  for ( ; *func_ptr ; func_ptr++)
  {
    if ((*func_ptr)->add())
      return 1;
  }
  return 0;
}


static bool
update_sum_func(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    if (func->add())
      return 1;
  return 0;
}

	/* Copy result of functions to record in tmp_table */

void
copy_funcs(Item **func_ptr)
{
  Item *func;
  for (; (func = *func_ptr) ; func_ptr++)
    func->save_in_result_field(1);
}


/*
  Create a condition for a const reference and add this to the
  currenct select for the table
*/

static bool add_ref_to_table_cond(THD *thd, JOIN_TAB *join_tab)
{
  DBUG_ENTER("add_ref_to_table_cond");
  if (!join_tab->ref.key_parts)
    DBUG_RETURN(FALSE);

  Item_cond_and *cond=new Item_cond_and();
  TABLE *table=join_tab->table;
  int error;
  if (!cond)
    DBUG_RETURN(TRUE);

  for (uint i=0 ; i < join_tab->ref.key_parts ; i++)
  {
    Field *field=table->field[table->key_info[join_tab->ref.key].key_part[i].
			      fieldnr-1];
    Item *value=join_tab->ref.items[i];
    cond->add(new Item_func_equal(new Item_field(field), value));
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(TRUE);

  if (!cond->fixed)
    cond->fix_fields(thd, (Item**)&cond);
  if (join_tab->select)
  {
    error=(int) cond->add(join_tab->select->cond);
    join_tab->select_cond=join_tab->select->cond=cond;
  }
  else if ((join_tab->select= make_select(join_tab->table, 0, 0, cond, 0,
                                          &error)))
    join_tab->select_cond=cond;

  DBUG_RETURN(error ? TRUE : FALSE);
}


/*
  Free joins of subselect of this select.

  free_underlaid_joins()
    thd - THD pointer
    select - pointer to st_select_lex which subselects joins we will free
*/

void free_underlaid_joins(THD *thd, SELECT_LEX *select)
{
  for (SELECT_LEX_UNIT *unit= select->first_inner_unit();
       unit;
       unit= unit->next_unit())
    unit->cleanup();
}

/****************************************************************************
  ROLLUP handling
****************************************************************************/

/*
  Replace occurences of group by fields in an expression by ref items

  SYNOPSIS
    change_group_ref()
    thd                  reference to the context
    expr                 expression to make replacement
    group_list           list of references to group by items
    changed        out:  returns 1 if item contains a replaced field item

  DESCRIPTION
    The function replaces occurrences of group by fields in expr
    by ref objects for these fields unless they are under aggregate
    functions.

  IMPLEMENTATION
    The function recursively traverses the tree of the expr expression,
    looks for occurrences of the group by fields that are not under
    aggregate functions and replaces them for the corresponding ref items.

  NOTES
    This substitution is needed GROUP BY queries with ROLLUP if
    SELECT list contains expressions over group by attributes.

  EXAMPLES
    SELECT a+1 FROM t1 GROUP BY a WITH ROLLUP
    SELECT SUM(a)+a FROM t1 GROUP BY a WITH ROLLUP 
    
  RETURN
    0	if ok
    1   on error
*/

static bool change_group_ref(THD *thd, Item_func *expr, ORDER *group_list,
                             bool *changed)
{
  if (expr->arg_count)
  {
    Name_resolution_context *context= &thd->lex->current_select->context;
    Item **arg,**arg_end;
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
                                        item->name)))
              return 1;                                 // fatal_error is set
            thd->change_item_tree(arg, new_item);
            *changed= TRUE;
          }
        }
      }
      else if (item->type() == Item::FUNC_ITEM)
      {
        if (change_group_ref(thd, (Item_func *) item, group_list, changed))
          return 1;
      }
    }
  }
  return 0;
}


/* Allocate memory needed for other rollup functions */

bool JOIN::rollup_init()
{
  uint i,j;
  Item **ref_array;

  tmp_table_param.quick_group= 0;	// Can't create groups in tmp table
  rollup.state= ROLLUP::STATE_INITED;

  /*
    Create pointers to the different sum function groups
    These are updated by rollup_make_fields()
  */
  tmp_table_param.group_parts= send_group_parts;

  if (!(rollup.null_items= (Item_null_result**) thd->alloc((sizeof(Item*) +
                                                sizeof(Item**) +
                                                sizeof(List<Item>) +
				                ref_pointer_array_size)
				                * send_group_parts )))
    return 1;
  
  rollup.fields= (List<Item>*) (rollup.null_items + send_group_parts);
  rollup.ref_pointer_arrays= (Item***) (rollup.fields + send_group_parts);
  ref_array= (Item**) (rollup.ref_pointer_arrays+send_group_parts);

  /*
    Prepare space for field list for the different levels
    These will be filled up in rollup_make_fields()
  */
  for (i= 0 ; i < send_group_parts ; i++)
  {
    rollup.null_items[i]= new (thd->mem_root) Item_null_result();
    List<Item> *rollup_fields= &rollup.fields[i];
    rollup_fields->empty();
    rollup.ref_pointer_arrays[i]= ref_array;
    ref_array+= all_fields.elements;
  }
  for (i= 0 ; i < send_group_parts; i++)
  {
    for (j=0 ; j < fields_list.elements ; j++)
      rollup.fields[i].push_back(rollup.null_items[i]);
  }
  List_iterator_fast<Item> it(all_fields);
  Item *item;
  while ((item= it++))
  {
    ORDER *group_tmp;
    for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
    {
      if (*group_tmp->item == item)
        item->maybe_null= 1;
    }
    if (item->type() == Item::FUNC_ITEM)
    {
      bool changed= 0;
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
  

/*
  Fill up rollup structures with pointers to fields to use

  SYNOPSIS
    rollup_make_fields()
    fields_arg			List of all fields (hidden and real ones)
    sel_fields			Pointer to selected fields
    func			Store here a pointer to all fields

  IMPLEMENTATION:
    Creates copies of item_sum items for each sum level

  RETURN
    0	if ok
	In this case func is pointing to next not used element.
    1   on error
*/

bool JOIN::rollup_make_fields(List<Item> &fields_arg, List<Item> &sel_fields,
			      Item_sum ***func)
{
  List_iterator_fast<Item> it(fields_arg);
  Item *first_field= sel_fields.head();
  uint level;

  /*
    Create field lists for the different levels

    The idea here is to have a separate field list for each rollup level to
    avoid all runtime checks of which columns should be NULL.

    The list is stored in reverse order to get sum function in such an order
    in func that it makes it easy to reset them with init_sum_functions()

    Assuming:  SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP

    rollup.fields[0] will contain list where a,b,c is NULL
    rollup.fields[1] will contain list where b,c is NULL
    ...
    rollup.ref_pointer_array[#] points to fields for rollup.fields[#]
    ...
    sum_funcs_end[0] points to all sum functions
    sum_funcs_end[1] points to all sum functions, except grand totals
    ...
  */

  for (level=0 ; level < send_group_parts ; level++)
  {
    uint i;
    uint pos= send_group_parts - level -1;
    bool real_fields= 0;
    Item *item;
    List_iterator<Item> new_it(rollup.fields[pos]);
    Item **ref_array_start= rollup.ref_pointer_arrays[pos];
    ORDER *start_group;

    /* Point to first hidden field */
    Item **ref_array= ref_array_start + fields_arg.elements-1;

    /* Remember where the sum functions ends for the previous level */
    sum_funcs_end[pos+1]= *func;

    /* Find the start of the group for this level */
    for (i= 0, start_group= group_list ;
	 i++ < pos ;
	 start_group= start_group->next)
      ;

    it.rewind();
    while ((item= it++))
    {
      if (item == first_field)
      {
	real_fields= 1;				// End of hidden fields
	ref_array= ref_array_start;
      }

      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item())
      {
	/*
	  This is a top level summary function that must be replaced with
	  a sum function that is reset for this level.

	  NOTE: This code creates an object which is not that nice in a
	  sub select.  Fortunately it's not common to have rollup in
	  sub selects.
	*/
	item= item->copy_or_same(thd);
	((Item_sum*) item)->make_unique();
	*(*func)= (Item_sum*) item;
	(*func)++;
      }
      else 
      {
	/* Check if this is something that is part of this group by */
	ORDER *group_tmp;
	for (group_tmp= start_group, i= pos ;
             group_tmp ; group_tmp= group_tmp->next, i++)
	{
          if (*group_tmp->item == item)
	  {
	    /*
	      This is an element that is used by the GROUP BY and should be
	      set to NULL in this level
	    */
            Item_null_result *null_item= new (thd->mem_root) Item_null_result();
            if (!null_item)
              return 1;
	    item->maybe_null= 1;		// Value will be null sometimes
            null_item->result_field= item->get_tmp_table_field();
            item= null_item;
	    break;
	  }
	}
      }
      *ref_array= item;
      if (real_fields)
      {
	(void) new_it++;			// Point to next item
	new_it.replace(item);			// Replace previous
	ref_array++;
      }
      else
	ref_array--;
    }
  }
  sum_funcs_end[0]= *func;			// Point to last function
  return 0;
}

/*
  Send all rollup levels higher than the current one to the client

  SYNOPSIS:
    rollup_send_data()
    idx			Level we are on:
			0 = Total sum level
			1 = First group changed  (a)
			2 = Second group changed (a,b)

  SAMPLE
    SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP

  RETURN
    0	ok
    1   If send_data_failed()
*/

int JOIN::rollup_send_data(uint idx)
{
  uint i;
  for (i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    memcpy((char*) ref_pointer_array,
	   (char*) rollup.ref_pointer_arrays[i],
	   ref_pointer_array_size);
    if ((!having || having->val_int()))
    {
      if (send_records < unit->select_limit_cnt && do_send_rows &&
	  result->send_data(rollup.fields[i]))
	return 1;
      send_records++;
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_pointer_array);
  return 0;
}

/*
  Write all rollup levels higher than the current one to a temp table

  SYNOPSIS:
    rollup_write_data()
    idx                 Level we are on:
                        0 = Total sum level
                        1 = First group changed  (a)
                        2 = Second group changed (a,b)
    table               reference to temp table

  SAMPLE
    SELECT a, b, SUM(c) FROM t1 GROUP BY a,b WITH ROLLUP

  RETURN
    0	ok
    1   if write_data_failed()
*/

int JOIN::rollup_write_data(uint idx, TABLE *table)
{
  uint i;
  for (i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    memcpy((char*) ref_pointer_array,
	   (char*) rollup.ref_pointer_arrays[i],
	   ref_pointer_array_size);
    if ((!having || having->val_int()))
    {
      int error;
      Item *item;
      List_iterator_fast<Item> it(rollup.fields[i]);
      while ((item= it++))
      {
        if (item->type() == Item::NULL_ITEM && item->is_result_field())
          item->save_in_result_field(1);
      }
      copy_sum_funcs(sum_funcs_end[i+1], sum_funcs_end[i]);
      if ((error= table->file->write_row(table->record[0])))
      {
	if (create_myisam_from_heap(thd, table, &tmp_table_param,
				      error, 0))
	  return 1;		     
      }
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_pointer_array);
  return 0;
}

/*
  clear results if there are not rows found for group
  (end_send_group/end_write_group)

  SYNOPSYS
     JOIN::clear()
*/

void JOIN::clear()
{
  clear_tables(this);
  copy_fields(&tmp_table_param);

  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }
}

/****************************************************************************
  EXPLAIN handling

  Send a description about what how the select will be done to stdout
****************************************************************************/

static void select_describe(JOIN *join, bool need_tmp_table, bool need_order,
			    bool distinct,const char *message)
{
  List<Item> field_list;
  List<Item> item_list;
  THD *thd=join->thd;
  select_result *result=join->result;
  Item *item_null= new Item_null();
  CHARSET_INFO *cs= system_charset_info;
  int quick_type;
  DBUG_ENTER("select_describe");
  DBUG_PRINT("info", ("Select 0x%lx, type %s, message %s",
		      (ulong)join->select_lex, join->select_lex->type,
		      message ? message : "NULL"));
  /* Don't log this into the slow query log */
  thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  join->unit->offset_limit_cnt= 0;

  if (message)
  {
    item_list.push_back(new Item_int((int32)
				     join->select_lex->select_number));
    item_list.push_back(new Item_string(join->select_lex->type,
					strlen(join->select_lex->type), cs));
    for (uint i=0 ; i < 7; i++)
      item_list.push_back(item_null);
    item_list.push_back(new Item_string(message,strlen(message),cs));
    if (result->send_data(item_list))
      join->error= 1;
  }
  else if (join->select_lex == join->unit->fake_select_lex)
  {
    /* 
      here we assume that the query will return at least two rows, so we
      show "filesort" in EXPLAIN. Of course, sometimes we'll be wrong
      and no filesort will be actually done, but executing all selects in
      the UNION to provide precise EXPLAIN information will hardly be
      appreciated :)
    */
    char table_name_buffer[NAME_LEN];
    item_list.empty();
    /* id */
    item_list.push_back(new Item_null);
    /* select_type */
    item_list.push_back(new Item_string(join->select_lex->type,
					strlen(join->select_lex->type),
					cs));
    /* table */
    {
      SELECT_LEX *sl= join->unit->first_select();
      uint len= 6, lastop= 0;
      memcpy(table_name_buffer, "<union", 6);
      for (; sl && len + lastop + 5 < NAME_LEN; sl= sl->next_select())
      {
        len+= lastop;
        lastop= my_snprintf(table_name_buffer + len, NAME_LEN - len,
                            "%u,", sl->select_number);
      }
      if (sl || len + lastop >= NAME_LEN)
      {
        memcpy(table_name_buffer + len, "...>", 5);
        len+= 4;
      }
      else
      {
        len+= lastop;
        table_name_buffer[len - 1]= '>';  // change ',' to '>'
      }
      item_list.push_back(new Item_string(table_name_buffer, len, cs));
    }
    /* type */
    item_list.push_back(new Item_string(join_type_str[JT_ALL],
					  strlen(join_type_str[JT_ALL]),
					  cs));
    /* possible_keys */
    item_list.push_back(item_null);
    /* key*/
    item_list.push_back(item_null);
    /* key_len */
    item_list.push_back(item_null);
    /* ref */
    item_list.push_back(item_null);
    /* rows */
    item_list.push_back(item_null);
    /* extra */
    if (join->unit->global_parameters->order_list.first)
      item_list.push_back(new Item_string("Using filesort",
					  14, cs));
    else
      item_list.push_back(new Item_string("", 0, cs));

    if (result->send_data(item_list))
      join->error= 1;
  }
  else
  {
    table_map used_tables=0;
    for (uint i=0 ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      TABLE *table=tab->table;
      char buff[512]; 
      char buff1[512], buff2[512], buff3[512];
      char keylen_str_buf[64];
      String extra(buff, sizeof(buff),cs);
      char table_name_buffer[NAME_LEN];
      String tmp1(buff1,sizeof(buff1),cs);
      String tmp2(buff2,sizeof(buff2),cs);
      String tmp3(buff3,sizeof(buff3),cs);
      extra.length(0);
      tmp1.length(0);
      tmp2.length(0);
      tmp3.length(0);

      quick_type= -1;
      item_list.empty();
      /* id */
      item_list.push_back(new Item_uint((uint32)
				       join->select_lex->select_number));
      /* select_type */
      item_list.push_back(new Item_string(join->select_lex->type,
					  strlen(join->select_lex->type),
					  cs));
      if (tab->type == JT_ALL && tab->select && tab->select->quick)
      {
        quick_type= tab->select->quick->get_type();
        if ((quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE) ||
            (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT) ||
            (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION))
          tab->type = JT_INDEX_MERGE;
        else
	  tab->type = JT_RANGE;
      }
      /* table */
      if (table->derived_select_number)
      {
	/* Derived table name generation */
	int len= my_snprintf(table_name_buffer, sizeof(table_name_buffer)-1,
			     "<derived%u>",
			     table->derived_select_number);
	item_list.push_back(new Item_string(table_name_buffer, len, cs));
      }
      else
	item_list.push_back(new Item_string(table->alias,
					    strlen(table->alias),
					    cs));
      /* type */
      item_list.push_back(new Item_string(join_type_str[tab->type],
					  strlen(join_type_str[tab->type]),
					  cs));
      /* Build "possible_keys" value and add it to item_list */
      if (!tab->keys.is_clear_all())
      {
        uint j;
        for (j=0 ; j < table->s->keys ; j++)
        {
          if (tab->keys.is_set(j))
          {
            if (tmp1.length())
              tmp1.append(',');
            tmp1.append(table->key_info[j].name, 
			strlen(table->key_info[j].name),
			system_charset_info);
          }
        }
      }
      if (tmp1.length())
	item_list.push_back(new Item_string(tmp1.ptr(),tmp1.length(),cs));
      else
	item_list.push_back(item_null);

      /* Build "key", "key_len", and "ref" values and add them to item_list */
      if (tab->ref.key_parts)
      {
	KEY *key_info=table->key_info+ tab->ref.key;
        register uint length;
	item_list.push_back(new Item_string(key_info->name,
					    strlen(key_info->name),
					    system_charset_info));
        length= longlong2str(tab->ref.key_length, keylen_str_buf, 10) - 
                keylen_str_buf;
        item_list.push_back(new Item_string(keylen_str_buf, length,
                                            system_charset_info));
	for (store_key **ref=tab->ref.key_copy ; *ref ; ref++)
	{
	  if (tmp2.length())
	    tmp2.append(',');
	  tmp2.append((*ref)->name(), strlen((*ref)->name()),
		      system_charset_info);
	}
	item_list.push_back(new Item_string(tmp2.ptr(),tmp2.length(),cs));
      }
      else if (tab->type == JT_NEXT)
      {
	KEY *key_info=table->key_info+ tab->index;
        register uint length;
	item_list.push_back(new Item_string(key_info->name,
					    strlen(key_info->name),cs));
        length= longlong2str(key_info->key_length, keylen_str_buf, 10) - 
                keylen_str_buf;
        item_list.push_back(new Item_string(keylen_str_buf, 
                                            length,
                                            system_charset_info));
	item_list.push_back(item_null);
      }
      else if (tab->select && tab->select->quick)
      {
        tab->select->quick->add_keys_and_lengths(&tmp2, &tmp3);
	item_list.push_back(new Item_string(tmp2.ptr(),tmp2.length(),cs));
	item_list.push_back(new Item_string(tmp3.ptr(),tmp3.length(),cs));
	item_list.push_back(item_null);
      }
      else
      {
	item_list.push_back(item_null);
	item_list.push_back(item_null);
	item_list.push_back(item_null);
      }
      /* Add "rows" field to item_list. */
      item_list.push_back(new Item_int((longlong) (ulonglong)
				       join->best_positions[i]. records_read,
				       21));
      /* Build "Extra" field and add it to item_list. */
      my_bool key_read=table->key_read;
      if ((tab->type == JT_NEXT || tab->type == JT_CONST) &&
          table->used_keys.is_set(tab->index))
	key_read=1;
      if (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT &&
          !((QUICK_ROR_INTERSECT_SELECT*)tab->select->quick)->need_to_fetch_row)
        key_read=1;
        
      if (tab->info)
	item_list.push_back(new Item_string(tab->info,strlen(tab->info),cs));
      else
      {
        if (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION || 
            quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT ||
            quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE)
        {
          extra.append("; Using ");
          tab->select->quick->add_info_string(&extra);
        }
	if (tab->select)
	{
	  if (tab->use_quick == 2)
	  {
            char buf[MAX_KEY/8+1];
            extra.append("; Range checked for each record (index map: 0x");
            extra.append(tab->keys.print(buf));
            extra.append(')');
	  }
	  else if (tab->select->cond)
          {
            const COND *pushed_cond= tab->table->file->pushed_cond;

            if (thd->variables.engine_condition_pushdown && pushed_cond)
            {
              extra.append("; Using where with pushed condition");
              if (thd->lex->describe & DESCRIBE_EXTENDED)
              {
                extra.append(": ");
                ((COND *)pushed_cond)->print(&extra);
              }
            }
            else
              extra.append("; Using where");
          }
	}
	if (key_read)
        {
          if (quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
            extra.append("; Using index for group-by");
          else
            extra.append("; Using index");
        }
	if (table->reginfo.not_exists_optimize)
	  extra.append("; Not exists");
	if (need_tmp_table)
	{
	  need_tmp_table=0;
	  extra.append("; Using temporary");
	}
	if (need_order)
	{
	  need_order=0;
	  extra.append("; Using filesort");
	}
	if (distinct & test_all_bits(used_tables,thd->used_tables))
	  extra.append("; Distinct");
        
        /* Skip initial "; "*/
        const char *str= extra.ptr();
        uint32 len= extra.length();
        if (len)
        {
          str += 2;
          len -= 2;
        }
	item_list.push_back(new Item_string(str, len, cs));
      }
      // For next iteration
      used_tables|=table->map;
      if (result->send_data(item_list))
	join->error= 1;
    }
  }
  for (SELECT_LEX_UNIT *unit= join->select_lex->first_inner_unit();
       unit;
       unit= unit->next_unit())
  {
    if (mysql_explain_union(thd, unit, result))
      DBUG_VOID_RETURN;
  }
  DBUG_VOID_RETURN;
}


bool mysql_explain_union(THD *thd, SELECT_LEX_UNIT *unit, select_result *result)
{
  DBUG_ENTER("mysql_explain_union");
  bool res= 0;
  SELECT_LEX *first= unit->first_select();

  for (SELECT_LEX *sl= first;
       sl;
       sl= sl->next_select())
  {
    // drop UNCACHEABLE_EXPLAIN, because it is for internal usage only
    uint8 uncacheable= (sl->uncacheable & ~UNCACHEABLE_EXPLAIN);
    sl->type= (((&thd->lex->select_lex)==sl)?
	       ((thd->lex->all_selects_list != sl) ? 
		primary_key_name : "SIMPLE"):
	       ((sl == first)?
		((sl->linkage == DERIVED_TABLE_TYPE) ?
		 "DERIVED":
		 ((uncacheable & UNCACHEABLE_DEPENDENT) ?
		  "DEPENDENT SUBQUERY":
		  (uncacheable?"UNCACHEABLE SUBQUERY":
		   "SUBQUERY"))):
		((uncacheable & UNCACHEABLE_DEPENDENT) ?
		 "DEPENDENT UNION":
		 uncacheable?"UNCACHEABLE UNION":
		 "UNION")));
    sl->options|= SELECT_DESCRIBE;
  }
  if (first->next_select())
  {
    unit->fake_select_lex->select_number= UINT_MAX; // jost for initialization
    unit->fake_select_lex->type= "UNION RESULT";
    unit->fake_select_lex->options|= SELECT_DESCRIBE;
    if (!(res= unit->prepare(thd, result, SELECT_NO_UNLOCK | SELECT_DESCRIBE,
                             "")))
      res= unit->exec();
    res|= unit->cleanup();
  }
  else
  {
    thd->lex->current_select= first;
    unit->set_limit(unit->global_parameters);
    res= mysql_select(thd, &first->ref_pointer_array,
			(TABLE_LIST*) first->table_list.first,
			first->with_wild, first->item_list,
			first->where,
			first->order_list.elements +
			first->group_list.elements,
			(ORDER*) first->order_list.first,
			(ORDER*) first->group_list.first,
			first->having,
			(ORDER*) thd->lex->proc_list.first,
			first->options | thd->options | SELECT_DESCRIBE,
			result, unit, first);
  }
  DBUG_RETURN(res || thd->net.report_error);
}


/*
  Print joins from the FROM clause

  SYNOPSIS
    print_join()
    thd     thread handler
    str     string where table should be printed
    tables  list of tables in join
*/

static void print_join(THD *thd, String *str, List<TABLE_LIST> *tables)
{
  /* List is reversed => we should reverse it before using */
  List_iterator_fast<TABLE_LIST> ti(*tables);
  TABLE_LIST **table= (TABLE_LIST **)thd->alloc(sizeof(TABLE_LIST*) *
                                                tables->elements);
  if (table == 0)
    return;  // out of memory

  for (TABLE_LIST **t= table + (tables->elements - 1); t >= table; t--)
    *t= ti++;

  DBUG_ASSERT(tables->elements >= 1);
  (*table)->print(thd, str);

  TABLE_LIST **end= table + tables->elements;
  for (TABLE_LIST **tbl= table + 1; tbl < end; tbl++)
  {
    TABLE_LIST *curr= *tbl;
    if (curr->outer_join)
      str->append(" left join ", 11); // MySQL converts right to left joins
    else if (curr->straight)
      str->append(" straight_join ", 15);
    else
      str->append(" join ", 6);
    curr->print(thd, str);
    if (curr->on_expr)
    {
      str->append(" on(", 4);
      curr->on_expr->print(str);
      str->append(')');
    }
  }
}


/*
  Print table as it should be in join list

  SYNOPSIS
    st_table_list::print();
    str   string where table should bbe printed
*/

void st_table_list::print(THD *thd, String *str)
{
  if (nested_join)
  {
    str->append('(');
    print_join(thd, str, &nested_join->join_list);
    str->append(')');
  }
  else
  {
    const char *cmp_name;                         // Name to compare with alias
    if (view_name.str)
    {
      append_identifier(thd, str, view_db.str, view_db.length);
      str->append('.');
      append_identifier(thd, str, view_name.str, view_name.length);
      cmp_name= view_name.str;
    }
    else if (derived)
    {
      str->append('(');
      derived->print(str);
      str->append(')');
      cmp_name= "";                               // Force printing of alias
    }
    else
    {
      append_identifier(thd, str, db, db_length);
      str->append('.');
      if (schema_table)
      {
        append_identifier(thd, str, schema_table_name,
                          strlen(schema_table_name));
        cmp_name= schema_table_name;
      }
      else
      {
        append_identifier(thd, str, table_name, table_name_length);
        cmp_name= table_name;
      }
    }
    if (my_strcasecmp(table_alias_charset, cmp_name, alias))
    {
      str->append(' ');
      append_identifier(thd, str, alias, strlen(alias));
    }
  }
}


void st_select_lex::print(THD *thd, String *str)
{
  /* QQ: thd may not be set for sub queries, but this should be fixed */
  if (!thd)
    thd= current_thd;

  str->append("select ", 7);

  /* First add options */
  if (options & SELECT_STRAIGHT_JOIN)
    str->append("straight_join ", 14);
  if ((thd->lex->lock_option == TL_READ_HIGH_PRIORITY) &&
      (this == &thd->lex->select_lex))
    str->append("high_priority ", 14);
  if (options & SELECT_DISTINCT)
    str->append("distinct ", 9);
  if (options & SELECT_SMALL_RESULT)
    str->append("sql_small_result ", 17);
  if (options & SELECT_BIG_RESULT)
    str->append("sql_big_result ", 15);
  if (options & OPTION_BUFFER_RESULT)
    str->append("sql_buffer_result ", 18);
  if (options & OPTION_FOUND_ROWS)
    str->append("sql_calc_found_rows ", 20);
  if (!thd->lex->safe_to_cache_query)
    str->append("sql_no_cache ", 13);
  if (options & OPTION_TO_QUERY_CACHE)
    str->append("sql_cache ", 10);

  //Item List
  bool first= 1;
  List_iterator_fast<Item> it(item_list);
  Item *item;
  while ((item= it++))
  {
    if (first)
      first= 0;
    else
      str->append(',');
    item->print_item_w_name(str);
  }

  /*
    from clause
    TODO: support USING/FORCE/IGNORE index
  */
  if (table_list.elements)
  {
    str->append(" from ", 6);
    /* go through join tree */
    print_join(thd, str, &top_join_list);
  }

  // Where
  Item *cur_where= where;
  if (join)
    cur_where= join->conds;
  if (cur_where)
  {
    str->append(" where ", 7);
    cur_where->print(str);
  }

  // group by & olap
  if (group_list.elements)
  {
    str->append(" group by ", 10);
    print_order(str, (ORDER *) group_list.first);
    switch (olap)
    {
      case CUBE_TYPE:
	str->append(" with cube", 10);
	break;
      case ROLLUP_TYPE:
	str->append(" with rollup", 12);
	break;
      default:
	;  //satisfy compiler
    }
  }

  // having
  Item *cur_having= having;
  if (join)
    cur_having= join->having;

  if (cur_having)
  {
    str->append(" having ", 8);
    cur_having->print(str);
  }

  if (order_list.elements)
  {
    str->append(" order by ", 10);
    print_order(str, (ORDER *) order_list.first);
  }

  // limit
  print_limit(thd, str);

  // PROCEDURE unsupported here
}


/*
  change select_result object of JOIN

  SYNOPSIS
    JOIN::change_result()
    res		new select_result object

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool JOIN::change_result(select_result *res)
{
  DBUG_ENTER("JOIN::change_result");
  result= res;
  if (!procedure && (result->prepare(fields_list, select_lex->master_unit()) ||
                     result->prepare2()))
  {
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}
