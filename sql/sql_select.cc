/* Copyright (C) 2000-2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

#include "opt_ft.h"

#include <m_ctype.h>
#include <hash.h>
#include <ft_global.h>
#include <assert.h>

const char *join_type_str[]={ "UNKNOWN","system","const","eq_ref","ref",
			      "MAYBE_REF","ALL","range","index","fulltext" };

static bool make_join_statistics(JOIN *join,TABLE_LIST *tables,COND *conds,
				 DYNAMIC_ARRAY *keyuse);
static bool update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,
				JOIN_TAB *join_tab,
                                uint tables,COND *conds,table_map table_map);
static int sort_keyuse(KEYUSE *a,KEYUSE *b);
static void set_position(JOIN *join,uint index,JOIN_TAB *table,KEYUSE *key);
static bool create_ref_for_key(JOIN *join, JOIN_TAB *j, KEYUSE *org_keyuse,
			       table_map used_tables);
static void find_best_combination(JOIN *join,table_map rest_tables);
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
static bool make_join_select(JOIN *join,SQL_SELECT *select,COND *item);
static void make_join_readinfo(JOIN *join,uint options);
static void join_free(JOIN *join);
static bool only_eq_ref_tables(JOIN *join,ORDER *order,table_map tables);
static void update_depend_map(JOIN *join);
static void update_depend_map(JOIN *join, ORDER *order);
static ORDER *remove_const(JOIN *join,ORDER *first_order,COND *cond,
			   bool *simple_order);
static int return_zero_rows(JOIN *join, select_result *res,TABLE_LIST *tables,
			    List<Item> &fields, bool send_row,
			    uint select_options, const char *info,
			    Item *having, Procedure *proc);
static COND *optimize_cond(COND *conds,Item::cond_result *cond_value);
static COND *remove_eq_conds(COND *cond,Item::cond_result *cond_value);
static bool const_expression_in_where(COND *conds,Item *item, Item **comp_item);
static bool open_tmp_table(TABLE *table);
static bool create_myisam_tmp_table(TABLE *table,TMP_TABLE_PARAM *param,
				    ulong options);
static int do_select(JOIN *join,List<Item> *fields,TABLE *tmp_table,
		     Procedure *proc);
static int sub_select_cache(JOIN *join,JOIN_TAB *join_tab,bool end_of_records);
static int sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records);
static int flush_cached_records(JOIN *join,JOIN_TAB *join_tab,bool skipp_last);
static int end_send(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_send_group(JOIN *join, JOIN_TAB *join_tab,bool end_of_records);
static int end_write(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_unique_update(JOIN *join,JOIN_TAB *join_tab,
			     bool end_of_records);
static int end_write_group(JOIN *join, JOIN_TAB *join_tab,
			   bool end_of_records);
static int test_if_group_changed(List<Item_buff> &list);
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
static COND *make_cond_for_table(COND *cond,table_map table,
				 table_map used_table);
static Item* part_of_refkey(TABLE *form,Field *field);
static uint find_shortest_key(TABLE *table, key_map usable_keys);
static bool test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,
				    ha_rows select_limit, bool no_changes);
static int create_sort_index(JOIN_TAB *tab,ORDER *order,ha_rows filesort_limit,
			     ha_rows select_limit);
static int remove_duplicates(JOIN *join,TABLE *entry,List<Item> &fields,
			     Item *having);
static int remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
				   ulong offset,Item *having);
static int remove_dup_with_hash_index(THD *thd, TABLE *table,
				      uint field_count, Field **first_field,
				      ulong key_length,Item *having);
static int join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count);
static ulong used_blob_length(CACHE_FIELD **ptr);
static bool store_record_in_cache(JOIN_CACHE *cache);
static void reset_cache(JOIN_CACHE *cache);
static void read_cached_record(JOIN_TAB *tab);
static bool cmp_buffer_with_ref(JOIN_TAB *tab);
static int setup_group(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		       List<Item> &all_fields, ORDER *order, bool *hidden);
static bool setup_new_fields(THD *thd,TABLE_LIST *tables,List<Item> &fields,
			     List<Item> &all_fields,ORDER *new_order);
static ORDER *create_distinct_group(THD *thd, ORDER *order,
				    List<Item> &fields,
				    bool *all_order_by_fields_used);
static bool test_if_subpart(ORDER *a,ORDER *b);
static TABLE *get_sort_by_table(ORDER *a,ORDER *b,TABLE_LIST *tables);
static void calc_group_buffer(JOIN *join,ORDER *group);
static bool alloc_group_fields(JOIN *join,ORDER *group);
static bool make_sum_func_list(JOIN *join,List<Item> &fields);
static bool change_to_use_tmp_fields(List<Item> &func);
static bool change_refs_to_tmp_fields(THD *thd, List<Item> &func);
static void init_tmptable_sum_functions(Item_sum **func);
static void update_tmptable_sum_func(Item_sum **func,TABLE *tmp_table);
static void copy_sum_funcs(Item_sum **func_ptr);
static bool add_ref_to_table_cond(THD *thd, JOIN_TAB *join_tab);
static void init_sum_functions(Item_sum **func);
static bool update_sum_func(Item_sum **func);
static void select_describe(JOIN *join, bool need_tmp_table,bool need_order,
			    bool distinct, const char *message=NullS);
static void describe_info(JOIN *join, const char *info);

/*
  This handles SELECT with and without UNION
*/

int handle_select(THD *thd, LEX *lex, select_result *result)
{
  int res;
  register SELECT_LEX *select_lex = &lex->select_lex;

#ifdef DISABLED_UNTIL_REWRITTEN_IN_4_1
  if (lex->olap)
  {
    SELECT_LEX *sl, *sl_next;
    int error;
    for (sl= &select_lex; sl; sl=sl_next)
    {
      sl_next=sl->next;				// Save if sl->next changes
      if (sl->olap != UNSPECIFIED_OLAP_TYPE)
      {
	if ((error=handle_olaps(lex,sl)))
	  return error;
	lex->last_selects->next=sl_next;
      }
    }
    lex->select = select_lex;
  }
#endif /* DISABLED_UNTIL_REWRITTEN_IN_4_1 */
  if (select_lex->next)
    res=mysql_union(thd,lex,result);
  else
    res=mysql_select(thd,(TABLE_LIST*) select_lex->table_list.first,
		     select_lex->item_list,
		     select_lex->where,
		     (ORDER*) select_lex->order_list.first,
		     (ORDER*) select_lex->group_list.first,
		     select_lex->having,
		     (ORDER*) lex->proc_list.first,
		     select_lex->options | thd->options,
		     result);
  if (res && result)
    result->abort();
  delete result;
  return res;
}


/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  mysql_select assumes that all tables are already opened
*****************************************************************************/

int
mysql_select(THD *thd,TABLE_LIST *tables,List<Item> &fields,COND *conds,
	     ORDER *order, ORDER *group,Item *having,ORDER *proc_param,
	     ulong select_options,select_result *result)
{
  TABLE		*tmp_table;
  int		error, tmp_error;
  bool         need_tmp;
  bool          hidden_group_fields;
  /*
    simple_xxxxx is set if ORDER/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if ORDER/GROUP BY is empty.
  */
  bool         simple_order, simple_group;
  /*
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool          no_order;
  /* Is set if we have a GROUP BY and we have ORDER BY on a constant. */
  bool          skip_sort_order;
  /* We cannot always prepare the result before selecting. */
  bool          is_result_prepared;
  ha_rows	select_limit;
  Item::cond_result cond_value;
  SQL_SELECT	*select;
  DYNAMIC_ARRAY keyuse;
  JOIN		join;
  Procedure	*procedure;
  List<Item>	all_fields(fields);
  bool		select_distinct;
  SELECT_LEX *cur_sel = thd->lex.select;
  DBUG_ENTER("mysql_select");

  /* Check that all tables, fields, conds and order are ok */

  select_distinct=test(select_options & SELECT_DISTINCT);
  tmp_table=0;
  select=0;
  no_order=skip_sort_order=0;
  bzero((char*) &keyuse,sizeof(keyuse));
  thd->proc_info="init";
  thd->used_tables=0;				// Updated by setup_fields
  /* select_limit is used to decide if we are likely to scan the whole table */
  select_limit= thd->select_limit;
  if (having || (select_options & OPTION_FOUND_ROWS))
    select_limit= HA_POS_ERROR;

  if (setup_tables(tables) ||
      setup_fields(thd,tables,fields,1,&all_fields,1) ||
      setup_conds(thd,tables,&conds) ||
      setup_order(thd,tables,fields,all_fields,order) ||
      setup_group(thd,tables,fields,all_fields,group,&hidden_group_fields))
    DBUG_RETURN(-1);				/* purecov: inspected */

  if (having)
  {
    thd->where="having clause";
    thd->allow_sum_func=1;
    if (having->fix_fields(thd,tables) || thd->fatal_error)
      DBUG_RETURN(-1);				/* purecov: inspected */
    if (having->with_sum_func)
      having->split_sum_func(all_fields);
  }
  if (setup_ftfuncs(thd)) /* should be after having->fix_fields */
    DBUG_RETURN(-1);
  /*
    Check if one one uses a not constant column with group functions
    and no GROUP BY.
    TODO:  Add check of calculation of GROUP functions and fields:
	   SELECT COUNT(*)+table.col1 from table1;
  */
  join.table=0;
  join.tables=0;
  {
    if (!group)
    {
      uint flag=0;
      List_iterator_fast<Item> it(fields);
      Item *item;
      while ((item= it++))
      {
	if (item->with_sum_func)
	  flag|=1;
	else if (!(flag & 2) && !item->const_item())
	  flag|=2;
      }
      if (flag == 3)
      {
	my_error(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,MYF(0));
	DBUG_RETURN(-1);
      }
    }
    TABLE_LIST *table;
    for (table=tables ; table ; table=table->next)
      join.tables++;
  }
  procedure=setup_procedure(thd,proc_param,result,fields,&error);
  if (error)
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (procedure)
  {
    if (setup_new_fields(thd,tables,fields,all_fields,procedure->param_fields))
    {						/* purecov: inspected */
      delete procedure;				/* purecov: inspected */
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
    if (procedure->group)
    {
      if (!test_if_subpart(procedure->group,group))
      {						/* purecov: inspected */
	my_message(0,"Can't handle procedures with differents groups yet",
		   MYF(0));			/* purecov: inspected */
	delete procedure;			/* purecov: inspected */
	DBUG_RETURN(-1);			/* purecov: inspected */
      }
    }
#ifdef NOT_NEEDED
    else if (!group && procedure->flags & PROC_GROUP)
    {
      my_message(0,"Select must have a group with this procedure",MYF(0));
      delete procedure;
      DBUG_RETURN(-1);
    }
#endif
    if (order && (procedure->flags & PROC_NO_SORT))
    { /* purecov: inspected */
      my_message(0,"Can't use order with this procedure",MYF(0)); /* purecov: inspected */
      delete procedure; /* purecov: inspected */
      DBUG_RETURN(-1); /* purecov: inspected */
    }
  }

  /* Init join struct */
  join.thd=thd;
  join.lock=thd->lock;
  join.join_tab=0;
  join.tmp_table_param.copy_field=0;
  join.sum_funcs=0;
  join.send_records=join.found_records=join.examined_rows=0;
  join.tmp_table_param.end_write_records= HA_POS_ERROR;
  join.first_record=join.sort_and_group=0;
  join.select_options=select_options;
  join.result=result;
  count_field_types(&join.tmp_table_param,all_fields,0);
  join.const_tables=0;
  join.having=0;
  join.do_send_rows = 1;
  join.group= group != 0;
  join.row_limit= ((select_distinct || order || group) ? HA_POS_ERROR :
		   thd->select_limit);

#ifdef RESTRICTED_GROUP
  if (join.sum_func_count && !group && (join.func_count || join.field_count))
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT),MYF(0));
    delete procedure;
    DBUG_RETURN(-1);
  }
#endif
  /*
    We must not yet prepare the result table if it is the same as one of the 
    source tables (INSERT SELECT). This is checked in mysql_execute_command()
    and OPTION_BUFFER_RESULT is added to the select_options. A temporary 
    table is then used to hold the result. The preparation may disable 
    indexes on the result table, which may be used during the select, if it
    is the same table (Bug #6034). Do the preparation after the select phase.
  */
  if ((is_result_prepared= (! procedure && 
                            ! test(select_options & OPTION_BUFFER_RESULT))) &&
      result->prepare(fields))
  {						/* purecov: inspected */
    DBUG_RETURN(-1);				/* purecov: inspected */
  }

#ifdef HAVE_REF_TO_FIELDS			// Not done yet
  /* Add HAVING to WHERE if possible */
  if (having && !group && ! join.sum_func_count)
  {
    if (!conds)
    {
      conds=having;
      having=0;
    }
    else if ((conds=new Item_cond_and(conds,having)))
    {
      conds->fix_fields(thd,tables);
      conds->change_ref_to_fields(thd,tables);
      conds->top_level_item();
      having=0;
    }
  }
#endif

  conds=optimize_cond(conds,&cond_value);
  if (thd->fatal_error)				// Out of memory
  {
    delete procedure;
    DBUG_RETURN(0);
  }
  if (cond_value == Item::COND_FALSE || !thd->select_limit)
  {					/* Impossible cond */
    if (! is_result_prepared && ! procedure && result->prepare(fields))
      goto err;
    error=return_zero_rows(&join, result, tables, fields,
			   join.tmp_table_param.sum_func_count != 0 && !group,
			   select_options,"Impossible WHERE",having,
			   procedure);
    delete procedure;
    DBUG_RETURN(error);
  }

  /* Optimize count(*), min() and max() */
  if (tables && join.tmp_table_param.sum_func_count && ! group)
  {
    int res;
    /*
    opt_sum_query returns -1 if no rows match to the WHERE conditions,
    or 1 if all items were resolved, or 0, or an error number HA_ERR_...
    */
    if ((res=opt_sum_query(tables, all_fields, conds)))
    {
      if (res > 1)
      {
	delete procedure;
	DBUG_RETURN(-1);
      }
      if (res < 0)
      {
        if (! is_result_prepared && ! procedure && result->prepare(fields))
          goto err;
	error=return_zero_rows(&join, result, tables, fields, !group,
			       select_options,"No matching min/max row",
			       having,procedure);
	delete procedure;
	DBUG_RETURN(error);
      }
      if (select_options & SELECT_DESCRIBE)
      {
	describe_info(&join, "Select tables optimized away");
	delete procedure;
	DBUG_RETURN(error);
      }
      tables=0;					// All tables resolved
    }
  }
  if (!tables)
  {						// Only test of functions
    error=0;
    if (select_options & SELECT_DESCRIBE)
      describe_info(&join, "No tables used");
    else
    {
      if (! is_result_prepared && ! procedure && result->prepare(fields))
        goto err;
      result->send_fields(fields,1);
      if (!having || having->val_int())
      {
	if (join.do_send_rows && result->send_data(fields))
	{
	  result->send_error(0,NullS);		/* purecov: inspected */
	  error=1;
	}
	else
	  error=(int) result->send_eof();
      }
      else
	error=(int) result->send_eof();
    }
    delete procedure;
    DBUG_RETURN(error);
  }

  error = -1;
  join.sort_by_table=get_sort_by_table(order,group,tables);

  /* Calculate how to do the join */
  thd->proc_info="statistics";
  if (make_join_statistics(&join,tables,conds,&keyuse) || thd->fatal_error)
    goto err;

  thd->proc_info="preparing";

  select_distinct= select_distinct && (join.const_tables != join.tables);

  if (result->initialize_tables(&join))
    goto err;
  if (join.const_table_map != join.found_const_table_map &&
      !(select_options & SELECT_DESCRIBE))
  {
    if (! is_result_prepared && ! procedure && result->prepare(fields))
      goto err;
    error=return_zero_rows(&join,result,tables,fields,
			   join.tmp_table_param.sum_func_count != 0 &&
			   !group,0,"no matching row in const table",having,
			   procedure);
    goto err;
  }
  if (!(thd->options & OPTION_BIG_SELECTS) &&
      join.best_read > (double) thd->variables.max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  {						/* purecov: inspected */
    result->send_error(ER_TOO_BIG_SELECT,ER(ER_TOO_BIG_SELECT)); /* purecov: inspected */
    error= 1;					/* purecov: inspected */
    goto err;					/* purecov: inspected */
  }
  if (join.const_tables && !thd->locked_tables &&
      !(select_options & SELECT_NO_UNLOCK))
  {
    TABLE **table, **end;
    for (table=join.table, end=table + join.const_tables ;
	 table != end;
	 table++)
    {
      /* BDB tables require that we call index_end() before doing an unlock */
      if ((*table)->key_read)
      {
	(*table)->key_read=0;
	(*table)->file->extra(HA_EXTRA_NO_KEYREAD);
      }
      (*table)->file->index_end();
    }
    mysql_unlock_some_tables(thd, join.table,join.const_tables);
  }
  if (!conds && join.outer_join)
  {
    /* Handle the case where we have an OUTER JOIN without a WHERE */
    conds=new Item_int((longlong) 1,1);	// Always true
  }
  select=make_select(*join.table, join.const_table_map,
		     join.const_table_map,conds,&error);
  if (error)
  { /* purecov: inspected */
    error= -1; /* purecov: inspected */
    goto err; /* purecov: inspected */
  }
  if (make_join_select(&join,select,conds))
  {
    if (! is_result_prepared && ! procedure && result->prepare(fields))
      goto err;
    error=return_zero_rows(&join, result, tables, fields,
			   join.tmp_table_param.sum_func_count != 0 && !group,
			   select_options,
			   "Impossible WHERE noticed after reading const tables",
			   having,procedure);
    goto err;
  }

  error= -1;					/* if goto err */

  /* Optimize distinct away if possible */
  {
    ORDER *org_order= order;
    order=remove_const(&join,order,conds,&simple_order);
    /*
      If we are using ORDER BY NULL or ORDER BY const_expression,
      return result in any order (even if we are using a GROUP BY)
    */
    if (!order && org_order)
      skip_sort_order= 1;
  }
  if (group || join.tmp_table_param.sum_func_count)
  {
    if (! hidden_group_fields)
      select_distinct=0;
  }
  else if (select_distinct && join.tables - join.const_tables == 1)
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
    JOIN_TAB *tab= &join.join_tab[join.const_tables];
    bool all_order_fields_used;
    if (order)
      skip_sort_order= test_if_skip_sort_order(tab, order, select_limit, 1);
    if ((group=create_distinct_group(thd, order, fields,
				     &all_order_fields_used)))
    {
      bool skip_group= (skip_sort_order &&
			test_if_skip_sort_order(tab, group, select_limit,
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
	    join.tmp_table_param.quick_group=0;
	  }
	  order=0;
	}
	join.group=1;				// For end_write_group
      }
      else
	group= 0;
    } else if (thd->fatal_error)			// End of memory
      goto err;
  }
  group=remove_const(&join,group,conds,&simple_group);
  if (!group && join.group)
  {
    order=0;					// The output has only one row
    simple_order=1;
  }

  calc_group_buffer(&join,group);
  join.send_group_parts=join.tmp_table_param.group_parts; /* Save org parts */
  if (procedure && procedure->group)
  {
    group=procedure->group=remove_const(&join,procedure->group,conds,
					&simple_group);
    calc_group_buffer(&join,group);
  }

  if (test_if_subpart(group,order) ||
      (!group && join.tmp_table_param.sum_func_count))
    order=0;

  // Can't use sort on head table if using row cache
  if (join.full_join)
  {
    if (group)
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
  need_tmp= (join.const_tables != join.tables &&
	     ((select_distinct || !simple_order || !simple_group) ||
	      (group && order) || 
	      test(select_options & OPTION_BUFFER_RESULT)));

  // No cache for MATCH
  make_join_readinfo(&join,
		     (select_options & (SELECT_DESCRIBE |
					SELECT_NO_JOIN_CACHE)) |
		     (cur_sel->ftfunc_list.elements ? SELECT_NO_JOIN_CACHE :
		      0));
  /*
    Need to tell Innobase that to play it safe, it should fetch all
    columns of the tables: this is because MySQL may build row
    pointers for the rows, and for all columns of the primary key the
    field->query_id has not necessarily been set to thd->query_id by
    MySQL.
  */

#ifdef HAVE_INNOBASE_DB
  if (need_tmp || select_distinct || group || order)
  {
    for (uint i_h = join.const_tables; i_h < join.tables; i_h++)
    {
      TABLE*	  table_h = join.join_tab[i_h].table;
      if (table_h->db_type == DB_TYPE_INNODB)
	table_h->file->extra(HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE);
    }
  }
#endif

  DBUG_EXECUTE("info",TEST_join(&join););
  /*
    Because filesort always does a full table scan or a quick range scan
    we must add the removed reference to the select for the table.
    We only need to do this when we have a simple_order or simple_group
    as in other cases the join is done before the sort.
    */
  if ((order || group) && join.join_tab[join.const_tables].type != JT_ALL &&
      join.join_tab[join.const_tables].type != JT_FT &&
      (order && simple_order || group && simple_group))
  {
    if (add_ref_to_table_cond(thd,&join.join_tab[join.const_tables]))
      goto err;
  }

  if (!(select_options & SELECT_BIG_RESULT) &&
      ((group && join.const_tables != join.tables &&
	(!simple_group ||
	 !test_if_skip_sort_order(&join.join_tab[join.const_tables], group,
				  thd->select_limit,0))) ||
       select_distinct) &&
      join.tmp_table_param.quick_group && !procedure)
  {
    need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
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
      /* Reset 'order' to 'group' and reinit variables describing 'order' */
      order= group;
      simple_order= simple_group;
      skip_sort_order= 0;
    }
    if (order &&
	(join.const_tables == join.tables ||
	 ((simple_order || skip_sort_order) &&
	  test_if_skip_sort_order(&join.join_tab[join.const_tables], order,
				  select_limit, 0))))
      order=0;
    select_describe(&join,need_tmp,
		    order != 0 && !skip_sort_order,
		    select_distinct);
    error=0;
    goto err;
  }

  /* Perform FULLTEXT search before all regular searches */
  init_ftfuncs(thd,test(order));

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    DBUG_PRINT("info",("Creating tmp table"));
    thd->proc_info="Creating tmp table";

    join.tmp_table_param.hidden_field_count= (all_fields.elements -
					      fields.elements);
    if (!(tmp_table =
	  create_tmp_table(thd,&join.tmp_table_param,all_fields,
			   ((!simple_group && !procedure &&
			     !(test_flags & TEST_NO_KEY_GROUP)) ?
			    group : (ORDER*) 0),
			   group ? 0 : select_distinct,
			   group && simple_group,
			   (order == 0 || skip_sort_order) &&
			   select_limit != HA_POS_ERROR,
			   join.select_options)))
      goto err;					/* purecov: inspected */

    if (having && (join.sort_and_group || (tmp_table->distinct && !group)))
      join.having=having;

    /* if group or order on first table, sort first */
    if (group && simple_group)
    {
      DBUG_PRINT("info",("Sorting for group"));
      thd->proc_info="Sorting for group";
      if (create_sort_index(&join.join_tab[join.const_tables],group,
			    HA_POS_ERROR, HA_POS_ERROR) ||
	  make_sum_func_list(&join,all_fields) ||
	  alloc_group_fields(&join,group))
	goto err;
      group=0;
    }
    else
    {
      if (make_sum_func_list(&join,all_fields))
	goto err;
      if (!group && ! tmp_table->distinct && order && simple_order)
      {
	DBUG_PRINT("info",("Sorting for order"));
	thd->proc_info="Sorting for order";
	if (create_sort_index(&join.join_tab[join.const_tables],order,
			      HA_POS_ERROR, HA_POS_ERROR))
	  goto err;				/* purecov: inspected */
	order=0;
      }
    }

    /*
      Optimize distinct when used on some of the tables
      SELECT DISTINCT t1.a FROM t1,t2 WHERE t1.b=t2.b
      In this case we can stop scanning t2 when we have found one t1.a
    */

    if (tmp_table->distinct)
    {
      table_map used_tables= thd->used_tables;
      JOIN_TAB *join_tab=join.join_tab+join.tables-1;
      do
      {
	if (used_tables & join_tab->table->map)
	  break;
	join_tab->not_used_in_distinct=1;
      } while (join_tab-- != join.join_tab);
      /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
      if (order && skip_sort_order)
      {
	/* Should always succeed */
	if (test_if_skip_sort_order(&join.join_tab[join.const_tables],
				    order, thd->select_limit,0))
	  order=0;
      }
    }

    /* Copy data to the temporary table */
    thd->proc_info="Copying to tmp table";
    if ((tmp_error=do_select(&join,(List<Item> *) 0,tmp_table,0)))
    {
      error=tmp_error;
      goto err;					/* purecov: inspected */
    }
    if (join.having)
      join.having=having=0;			// Allready done

    /* Change sum_fields reference to calculated fields in tmp_table */
    if (join.sort_and_group || tmp_table->group)
    {
      if (change_to_use_tmp_fields(all_fields))
	goto err;
      join.tmp_table_param.field_count+=join.tmp_table_param.sum_func_count+
	join.tmp_table_param.func_count;
      join.tmp_table_param.sum_func_count=join.tmp_table_param.func_count=0;
    }
    else
    {
      if (change_refs_to_tmp_fields(thd,all_fields))
	goto err;
      join.tmp_table_param.field_count+=join.tmp_table_param.func_count;
      join.tmp_table_param.func_count=0;
    }
    if (procedure)
      procedure->update_refs();
    if (tmp_table->group)
    {						// Already grouped
      if (!order && !no_order && !skip_sort_order)
	order=group;				/* order by group */
      group=0;
    }

    /*
      If we have different sort & group then we must sort the data by group
      and copy it to another tmp table
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)).
    */

    if (group && (!test_if_subpart(group,order) || select_distinct) ||
	(select_distinct &&
	 join.tmp_table_param.using_indirect_summary_function))
    {					/* Must copy to another table */
      TABLE *tmp_table2;
      DBUG_PRINT("info",("Creating group table"));

      /* Free first data from old join */
      join_free(&join);
      if (make_simple_join(&join,tmp_table))
	goto err;
      calc_group_buffer(&join,group);
      count_field_types(&join.tmp_table_param,all_fields,
			select_distinct && !group);
      join.tmp_table_param.hidden_field_count=(all_fields.elements-
					       fields.elements);

      /* group data to new table */
      if (!(tmp_table2 = create_tmp_table(thd,&join.tmp_table_param,all_fields,
					  (ORDER*) 0,
					  select_distinct && !group,
					  1, 0,
					  join.select_options)))
	goto err;				/* purecov: inspected */
      if (group)
      {
	thd->proc_info="Creating sort index";
	if (create_sort_index(join.join_tab,group,HA_POS_ERROR, HA_POS_ERROR) ||
	    alloc_group_fields(&join,group))
	{
	  free_tmp_table(thd,tmp_table2);	/* purecov: inspected */
	  goto err;				/* purecov: inspected */
	}
	group=0;
      }
      thd->proc_info="Copying to group table";
      tmp_error= -1;
      if (make_sum_func_list(&join,all_fields) ||
	  (tmp_error=do_select(&join,(List<Item> *) 0,tmp_table2,0)))
      {
	error=tmp_error;
	free_tmp_table(thd,tmp_table2);
	goto err;				/* purecov: inspected */
      }
      end_read_record(&join.join_tab->read_record);
      free_tmp_table(thd,tmp_table);
      join.const_tables=join.tables;		// Mark free for join_free()
      tmp_table=tmp_table2;
      join.join_tab[0].table=0;			// Table is freed

      if (change_to_use_tmp_fields(all_fields)) // No sum funcs anymore
	goto err;
      join.tmp_table_param.field_count+=join.tmp_table_param.sum_func_count;
      join.tmp_table_param.sum_func_count=0;
    }

    if (tmp_table->distinct)
      select_distinct=0;			/* Each row is unique */

    join_free(&join);				/* Free quick selects */
    if (select_distinct && ! group)
    {
      thd->proc_info="Removing duplicates";
      if (having)
	having->update_used_tables();
      if (remove_duplicates(&join,tmp_table,fields, having))
	goto err;				/* purecov: inspected */
      having=0;
      select_distinct=0;
    }
    tmp_table->reginfo.lock_type=TL_UNLOCK;
    if (make_simple_join(&join,tmp_table))
      goto err;
    calc_group_buffer(&join,group);
    count_field_types(&join.tmp_table_param,all_fields,0);
  }
  if (procedure)
  {
    if (procedure->change_columns(fields) ||
	result->prepare(fields))
      goto err;
    count_field_types(&join.tmp_table_param,all_fields,0);
  }
  else if (! is_result_prepared && result->prepare(fields))
    goto err;
  if (join.group || join.tmp_table_param.sum_func_count ||
      (procedure && (procedure->flags & PROC_GROUP)))
  {
    if (alloc_group_fields(&join,group) ||
        setup_copy_fields(thd, &join.tmp_table_param,all_fields) ||
        make_sum_func_list(&join,all_fields) || thd->fatal_error)
      goto err; /* purecov: inspected */
  }
  if (group || order)
  {
    DBUG_PRINT("info",("Sorting for send_fields"));
    thd->proc_info="Sorting result";
    /* If we have already done the group, add HAVING to sorted table */
    if (having && ! group && ! join.sort_and_group)
    {
      having->update_used_tables();	// Some tables may have been const
      JOIN_TAB *table=&join.join_tab[join.const_tables];
      table_map used_tables= join.const_table_map | table->table->map;

      Item* sort_table_cond=make_cond_for_table(having,used_tables,used_tables);
      if (sort_table_cond)
      {
	if (!table->select)
	  if (!(table->select=new SQL_SELECT))
	    goto err;
	if (!table->select->cond)
	  table->select->cond=sort_table_cond;
	else					// This should never happen
	  if (!(table->select->cond=new Item_cond_and(table->select->cond,
						      sort_table_cond)))
	    goto err;
	table->select_cond=table->select->cond;
	table->select_cond->top_level_item();
	DBUG_EXECUTE("where",print_where(table->select->cond,
					 "select and having"););
	having=make_cond_for_table(having,~ (table_map) 0,~used_tables);
	DBUG_EXECUTE("where",print_where(conds,"having after sort"););
      }
    }
    if (group)
      select_limit= HA_POS_ERROR;
    else
    {
      /*
	We can abort sorting after thd->select_limit rows if we there is no
	WHERE clause for any tables after the sorted one.
      */
      JOIN_TAB *table= &join.join_tab[join.const_tables+1];
      JOIN_TAB *end_table= &join.join_tab[join.tables];
      for (; table < end_table ; table++)
      {
	/*
	  table->keyuse is set in the case there was an original WHERE clause
	  on the table that was optimized away.
	  table->on_expr tells us that it was a LEFT JOIN and there will be
	  at least one row generated from the table.
	*/
	if (table->select_cond || (table->keyuse && !table->on_expr))
	{
	  /* We have to sort all rows */
	  select_limit= HA_POS_ERROR;
	  break;
	}
      }
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
    if (create_sort_index(&join.join_tab[join.const_tables],
			  group ? group : order,
			  select_limit, 
                          join.select_options & OPTION_FOUND_ROWS ?
                          HA_POS_ERROR : thd->select_limit))
      goto err;
  }
  join.having=having;				// Actually a parameter
  thd->proc_info="Sending data";
  error=do_select(&join,&fields,NULL,procedure);

err:
  thd->limit_found_rows = join.send_records;
  thd->examined_row_count = join.examined_rows;
  thd->proc_info="end";
  join.lock=0;					// It's faster to unlock later
  join_free(&join);
  thd->proc_info="end2";			// QQ
  if (tmp_table)
    free_tmp_table(thd,tmp_table);
  thd->proc_info="end3";			// QQ
  delete select;
  delete_dynamic(&keyuse);
  delete procedure;
  thd->proc_info="end4";			// QQ
  DBUG_RETURN(error);
}

/*****************************************************************************
  Create JOIN_TABS, make a guess about the table types,
  Approximate how many records will be used in each table
*****************************************************************************/

static ha_rows get_quick_record_count(THD *thd, SQL_SELECT *select,
				      TABLE *table,
				      key_map keys,ha_rows limit)
{
  int error;
  DBUG_ENTER("get_quick_record_count");
  if (select)
  {
    select->head=table;
    table->reginfo.impossible_range=0;
    if ((error=select->test_quick_select(thd, keys,(table_map) 0,limit))
	== 1)
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
make_join_statistics(JOIN *join,TABLE_LIST *tables,COND *conds,
		     DYNAMIC_ARRAY *keyuse_array)
{
  int error;
  uint i,table_count,const_count,found_ref,refs,key,const_ref,eq_part;
  table_map found_const_table_map,all_table_map;
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
  found_const_table_map=all_table_map=0;
  const_count=0;

  for (s=stat,i=0 ; tables ; s++,tables=tables->next,i++)
  {
    TABLE *table;
    stat_vector[i]=s;
    table_vector[i]=s->table=table=tables->table;
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);// record count
    table->quick_keys=0;
    table->reginfo.join_tab=s;
    table->reginfo.not_exists_optimize=0;
    bzero((char*) table->const_key_parts, sizeof(key_part_map)*table->keys);
    all_table_map|= table->map;
    s->join=join;
    s->info=0;					// For describe
    if ((s->on_expr=tables->on_expr))
    {
      /* Left join */
      if (!table->file->records)
      {						// Empty table
	s->key_dependent=s->dependent=0;	// Ignore LEFT JOIN depend.
	set_position(join,const_count++,s,(KEYUSE*) 0);
	continue;
      }
      s->key_dependent=s->dependent=
	s->on_expr->used_tables() & ~(table->map);
      if (table->outer_join & JOIN_TYPE_LEFT)
	s->dependent|=stat_vector[i-1]->dependent | table_vector[i-1]->map;
      if (tables->outer_join & JOIN_TYPE_RIGHT)
	s->dependent|=tables->next->table->map;
      outer_join|=table->map;
      continue;
    }
    if (tables->straight)			// We don't have to move this
      s->dependent= table_vector[i-1]->map | stat_vector[i-1]->dependent;
    else
      s->dependent=(table_map) 0;
    s->key_dependent=(table_map) 0;
    if ((table->system || table->file->records <= 1) && ! s->dependent &&
	!(table->file->table_flags() & HA_NOT_EXACT_COUNT) &&
        !table->fulltext_searched)
    {
      set_position(join,const_count++,s,(KEYUSE*) 0);
    }
  }
  stat_vector[i]=0;
  join->outer_join=outer_join;

  /*
    If outer join: Re-arrange tables in stat_vector so that outer join
    tables are after all tables it is dependent of.
    For example: SELECT * from A LEFT JOIN B ON B.c=C.c, C WHERE A.C=C.C
    Will shift table B after table C.
  */
  if (outer_join)
  {
    table_map used_tables=0L;
    for (i=0 ; i < join->tables-1 ; i++)
    {
      if (stat_vector[i]->dependent & ~used_tables)
      {
	JOIN_TAB *save= stat_vector[i];
	uint j;
	for (j=i+1;
	     j < join->tables && stat_vector[j]->dependent & ~used_tables;
	     j++)
	{
	  JOIN_TAB *tmp=stat_vector[j];		// Move element up
	  stat_vector[j]=save;
	  save=tmp;
	}
	if (j == join->tables)
	{
	  join->tables=0;			// Don't use join->table
	  my_error(ER_WRONG_OUTER_JOIN,MYF(0));
	  DBUG_RETURN(1);
	}
	stat_vector[i]=stat_vector[j];
	stat_vector[j]=save;
      }
      used_tables|= stat_vector[i]->table->map;
    }
  }

  if (conds || outer_join)
    if (update_ref_and_keys(join->thd,keyuse_array,stat,join->tables,
                            conds,~outer_join))
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
      TABLE *table=s->table;
      if (s->dependent)				// If dependent on some table
      {
	// All dep. must be constants
	if (s->dependent & ~(found_const_table_map))
	  continue;
	if (table->file->records <= 1L &&
	    !(table->file->table_flags() & HA_NOT_EXACT_COUNT))
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
	  s->keys|= (key_map) 1 << key;		// QQ: remove this ?

	  refs=const_ref=eq_part=0;
	  do
	  {
	    if (keyuse->val->type() != Item::NULL_ITEM)
	    {
	      if (!((~found_const_table_map) & keyuse->used_tables))
		const_ref|= (key_map) 1 << keyuse->keypart;
	      else
		refs|=keyuse->used_tables;
	      eq_part|= (uint) 1 << keyuse->keypart;
	    }
	    keyuse++;
	  } while (keyuse->table == table && keyuse->key == key);

	  if (eq_part == PREV_BITS(uint,table->key_info[key].key_parts) &&
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

    if (s->const_keys)
    {
      ha_rows records;
      SQL_SELECT *select;
      select= make_select(s->table, found_const_table_map,
			  found_const_table_map,
			  s->on_expr ? s->on_expr : conds,
			  &error);
      records= get_quick_record_count(join->thd, select, s->table,
				      s->const_keys, join->row_limit);
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
	if (s->on_expr)
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

  /* Find best combination and return it */
  join->join_tab=stat;
  join->map2table=stat_ref;
  join->table= join->all_tables=table_vector;
  join->const_tables=const_count;
  join->found_const_table_map=found_const_table_map;

  if (join->const_tables != join->tables)
    find_best_combination(join,all_table_map & ~join->const_table_map);
  else
  {
    memcpy((gptr) join->best_positions,(gptr) join->positions,
	   sizeof(POSITION)*join->const_tables);
    join->best_read=1.0;
  }
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
  uint		level,const_level;	// QQ: Remove const_level
  bool		eq_func;
  bool		exists_optimize;
} KEY_FIELD;


/* merge new key definitions to old ones, remove those not used in both */

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
	  if (old->val->eq(new_fields->val, old->field->binary()))
	  {
	    old->level=old->const_level=and_level;
	    old->exists_optimize&=new_fields->exists_optimize;
	  }
	}
	else if (old->val->eq(new_fields->val, old->field->binary()) &&
		 old->eq_func && new_fields->eq_func)
	{
	  old->level=old->const_level=and_level;
	  old->exists_optimize&=new_fields->exists_optimize;
	}
	else					// Impossible; remove it
	{
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
    if (old->level != and_level && old->const_level != and_level)
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


static void
add_key_field(KEY_FIELD **key_fields,uint and_level,
	      Field *field,bool eq_func,Item **value, uint num_values,
	      table_map usable_tables)
{
  bool exists_optimize=0;
  if (!(field->flags & PART_KEY_FLAG))
  {
    // Don't remove column IS NULL on a LEFT JOIN table
    if (!eq_func || (*value)->type() != Item::NULL_ITEM ||
        !field->table->maybe_null || field->null_ptr)
      return;					// Not a key. Skip it
    exists_optimize=1;
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
      exists_optimize=1;
    }
    else
    {
      JOIN_TAB *stat=field->table->reginfo.join_tab;
      key_map possible_keys= (field->key_start &
			      field->table->keys_in_use_for_query);
      stat[0].keys|= possible_keys;		// Add possible keys

      /* Save the following cases:
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
        is_const&= (*value)->const_item();
      if (is_const)
        stat[0].const_keys |= possible_keys;

      /* We can't always use indexes when comparing a string index to a
	 number. cmp_type() is checked to allow compare of dates to numbers
         also eq_func is NEVER true when num_values > 1
       */
      if (!eq_func ||
	  field->result_type() == STRING_RESULT &&
	  (*value)->result_type() != STRING_RESULT &&
	  field->cmp_type() != (*value)->result_type())
	return;
    }
  }
  DBUG_ASSERT(num_values == 1);
  /*
    For the moment eq_func is always true. This slot is reserved for future
    extensions where we want to remembers other things than just eq comparisons
  */
  DBUG_ASSERT(eq_func);
  /* Store possible eq field */
  (*key_fields)->field=field;
  (*key_fields)->eq_func=eq_func;
  (*key_fields)->val= *value;
  (*key_fields)->level=(*key_fields)->const_level=and_level;
  (*key_fields)->exists_optimize=exists_optimize;
  (*key_fields)++;
}


static void
add_key_fields(JOIN_TAB *stat,KEY_FIELD **key_fields,uint *and_level,
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
	add_key_fields(stat,key_fields,and_level,item,usable_tables);
      for (; org_key_fields != *key_fields ; org_key_fields++)
      {
	if (org_key_fields->const_level == org_key_fields->level)
	  org_key_fields->const_level=org_key_fields->level= *and_level;
	else
	  org_key_fields->const_level= *and_level;
      }
    }
    else
    {
      (*and_level)++;
      add_key_fields(stat,key_fields,and_level,li++,usable_tables);
      Item *item;
      while ((item=li++))
      {
	KEY_FIELD *start_key_fields= *key_fields;
	(*and_level)++;
	add_key_fields(stat,key_fields,and_level,item,usable_tables);
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
  switch (cond_func->select_optimize()) {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
    // BETWEEN or IN
    if (cond_func->key_item()->type() == Item::FIELD_ITEM)
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->key_item()))->field,
#ifndef TO_BE_REMOVED_IN_4_1
                    /* special treatment for IN. Not necessary in 4.1 */
                    cond_func->argument_count() == 1,
                    cond_func->arguments()      + (cond_func->functype() != Item_func::IN_FUNC),
                    cond_func->argument_count() - (cond_func->functype() != Item_func::IN_FUNC),
#else
                    cond_func->argument_count() == 2,
                    cond_func->arguments()+1, cond_func->argument_count()-1,
#endif
                    usable_tables);
    break;
  case Item_func::OPTIMIZE_OP:
  {
    bool equal_func=(cond_func->functype() == Item_func::EQ_FUNC ||
		     cond_func->functype() == Item_func::EQUAL_FUNC);

    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[0]))->field,
		    equal_func,
                    cond_func->arguments()+1, 1, usable_tables);
    }
    if (cond_func->arguments()[1]->type() == Item::FIELD_ITEM &&
	cond_func->functype() != Item_func::LIKE_FUNC)
    {
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[1]))->field,
		    equal_func,
		    cond_func->arguments(),1,usable_tables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      Item *tmp=new Item_null;
      if (!tmp)					// Should never be true
	return;
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[0]))->field,
		    cond_func->functype() == Item_func::ISNULL_FUNC,
		    &tmp, 1, usable_tables);
    }
    break;
  }
  return;
}

/*
  Add all keys with uses 'field' for some keypart
  If field->and_level != and_level then only mark key_part as const_part
*/

static uint
max_part_bit(key_map bits)
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

  if (key_field->eq_func && !key_field->exists_optimize)
  {
    for (uint key=0 ; key < form->keys ; key++)
    {
      if (!(form->keys_in_use_for_query & (((key_map) 1) << key)))
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
	  keyuse.used_tables=key_field->val->used_tables();
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
          ((functype == Item_func::GE_FUNC && arg1->val()> 0) ||
           (functype == Item_func::GT_FUNC && arg1->val()>=0))  &&
           arg0->type() == Item::FUNC_ITEM            &&
           arg0->functype() == Item_func::FT_FUNC)
        cond_func=(Item_func_match *) arg0;
      else if (arg0->const_item() &&
               ((functype == Item_func::LE_FUNC && arg0->val()> 0) ||
                (functype == Item_func::LT_FUNC && arg0->val()>=0)) &&
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
  VOID(insert_dynamic(keyuse_array,(gptr) &keyuse));
}

static int
sort_keyuse(KEYUSE *a,KEYUSE *b)
{
  if (a->table->tablenr != b->table->tablenr)
    return (int) (a->table->tablenr - b->table->tablenr);
  if (a->key != b->key)
    return (int) (a->key - b->key);
  if (a->keypart != b->keypart)
    return (int) (a->keypart - b->keypart);
  return test(a->used_tables) - test(b->used_tables);	// Place const first
}


/*
  Update keyuse array with all possible keys we can use to fetch rows
  join_tab is a array in tablenr_order
  stat is a reference array in 'prefered' order.
*/

static bool
update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,
		    uint tables, COND *cond, table_map normal_tables)
{
  uint	and_level,i,found_eq_constant;
  KEY_FIELD *key_fields, *end, *field;

  if (!(key_fields=(KEY_FIELD*)
	thd->alloc(sizeof(key_fields[0])*(thd->cond_count+1)*2)))
    return TRUE; /* purecov: inspected */
  and_level=0; field=end=key_fields;
  if (my_init_dynamic_array(keyuse,sizeof(KEYUSE),20,64))
    return TRUE;
  if (cond)
  {
    add_key_fields(join_tab,&end,&and_level,cond,normal_tables);
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
    if (join_tab[i].on_expr)
    {
      add_key_fields(join_tab,&end,&and_level,join_tab[i].on_expr,
		     join_tab[i].table->map);
    }
  }
  /* fill keyuse with found key parts */
  for (; field != end ; field++)
    add_key_part(keyuse,field);

  if (thd->lex.select->ftfunc_list.elements)
  {
    add_ft_keys(keyuse,join_tab,cond,normal_tables);
  }

  /*
    Remove ref if there is a keypart which is a ref and a const.
    Remove keyparts without previous keyparts.
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
	use->table->const_key_parts[use->key] |=
	  (key_part_map) 1 << use->keypart;
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
      use->table->reginfo.join_tab->checked_keys|= (key_map) 1 << use->key;
      save_pos++;
    }
    i=(uint) (save_pos-(KEYUSE*) keyuse->buffer);
    VOID(set_dynamic(keyuse,(gptr) &end,i));
    keyuse->elements=i;
  }
  return FALSE;
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


static void
find_best_combination(JOIN *join, table_map rest_tables)
{
  DBUG_ENTER("find_best_combination");
  join->best_read=DBL_MAX;
  find_best(join,rest_tables, join->const_tables,1.0,0.0);
  DBUG_VOID_RETURN;
}


static void
find_best(JOIN *join,table_map rest_tables,uint idx,double record_count,
	  double read_time)
{
  ha_rows rec;
  double tmp;
  THD *thd= join->thd;
  if (thd->killed)				// Abort
    return;

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
      join->best_read=read_time;
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
	  key_map found_part=0;
	  table_map found_ref=0;
	  uint key=keyuse->key;
	  KEY *keyinfo=table->key_info+key;
          bool ft_key=(keyuse->keypart == FT_KEYPART);

	  start_key=keyuse;
	  do
	  {
            uint keypart=keyuse->keypart;
	    do
	    {
              if (!ft_key)
              {
		table_map map;
		if (!(rest_tables & keyuse->used_tables))
		{
		  found_part|= (key_part_map) 1 << keypart;
		  found_ref|= keyuse->used_tables;
		}
		/*
		  If we find a ref, assume this table matches a proportional
		  part of this table.
		  For example 100 records matching a table with 5000 records
		  gives 5000/100 = 50 records per key
		  Constant tables are ignored and to avoid bad matches,
		  we don't make rec less than 100.
		*/
		if (keyuse->used_tables &
		    (map=(keyuse->used_tables & ~join->const_table_map)))
		{
		  uint tablenr;
		  for (tablenr=0 ; ! (map & 1) ; map>>=1, tablenr++) ;
		  if (map == 1)			// Only one table
		  {
		    TABLE *tmp_table=join->all_tables[tablenr];
		    if (rec > tmp_table->file->records && rec > 100)
		      rec=max(tmp_table->file->records,100);
		  }
		}
              }
	      keyuse++;
	    } while (keyuse->table == table && keyuse->key == key &&
		     keyuse->keypart == keypart);
	  } while (keyuse->table == table && keyuse->key == key);

	  /*
	    Assume that that each key matches a proportional part of table.
	  */
          if (!found_part && !ft_key)
	    continue;				// Nothing usable found
	  if (rec == 0)
	    rec=1L;				// Fix for small tables

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
	  if (found_part == PREV_BITS(uint,keyinfo->key_parts))
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
		if (table->quick_keys & ((key_map) 1 << key))
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
		      ((double) (table->max_key_length-keyinfo->key_length) /
		       (double) table->max_key_length)));
		  if (records < 2.0)
		    records=2.0;		// Can't be as good as a unique
		}
	      }
	      /* Limit the number of matched rows */
	      tmp= records;
	      set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
	      if (table->used_keys & ((key_map) 1 << key))
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
		!(table->file->index_flags(key) & HA_ONLY_WHOLE_INDEX))
	    {
	      max_key_part=max_part_bit(found_part);
	      /*
		Check if quick_range could determinate how many rows we
		will match
	      */
	      if (table->quick_keys & ((key_map) 1 << key) &&
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
		    and that the hole key matches 10 (duplicates) or 1
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
		if (table->quick_keys & ((key_map) 1 << key) &&
		    table->quick_key_parts[key] <= max_key_part &&
		    records > (double) table->quick_rows[key])
		  tmp= records= (double) table->quick_rows[key];
	      }
	      /* Limit the number of matched rows */
	      set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
	      if (table->used_keys & ((key_map) 1 << key))
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
	    s->table->used_keys && best_key) &&
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
          if (s->on_expr)                         // Can't use join cache
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
	  join->thd->select_limit >= records)
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
	swap(JOIN_TAB*,join->best_ref[idx],*pos);
	find_best(join,rest_tables & ~real_table_bit,idx+1,
		  current_record_count,current_read_time);
	swap(JOIN_TAB*,join->best_ref[idx],*pos);
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
    rec_length+=(join_tab->table->null_fields+7)/8;
  if (join_tab->table->maybe_null)
    rec_length+=sizeof(my_bool);
  if (blobs)
  {
    uint blob_length=(uint) (join_tab->table->file->mean_rec_length-
			     (join_tab->table->reclength- rec_length));
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

  table_count=join->tables;
  if (!(join->join_tab=join_tab=
	(JOIN_TAB*) thd->alloc(sizeof(JOIN_TAB)*table_count)))
    return TRUE;

  join->full_join=0;

  used_tables=0;
  for (j=join_tab, tablenr=0 ; tablenr < table_count ; tablenr++,j++)
  {
    TABLE *form;
    *j= *join->best_positions[tablenr].table;
    form=join->table[tablenr]=j->table;
    used_tables|= form->map;
    form->reginfo.join_tab=j;
    if (!j->on_expr)
      form->reginfo.not_exists_optimize=0;	// Only with LEFT JOIN
    if (j->type == JT_CONST)
      continue;					// Handled in make_join_stat..

    j->ref.key = -1;
    j->ref.key_parts=0;

    if (j->type == JT_SYSTEM)
      continue;
    if (!j->keys || !(keyuse= join->best_positions[tablenr].key))
    {
      j->type=JT_ALL;
      if (tablenr != join->const_tables)
	join->full_join=1;
    }
    else if (create_ref_for_key(join, j, keyuse, used_tables))
      return TRUE;				// Something went wrong
  }

  for (i=0 ; i < table_count ; i++)
    join->map2table[join->join_tab[i].table->tablenr]=join->join_tab+i;
  update_depend_map(join);
  return 0;
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

  /*
    Use best key from find_best
  */
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
    do
    {
      if (!((~used_tables) & keyuse->used_tables))
      {
	if (keyparts == keyuse->keypart)
	{
	  keyparts++;
	  length+=keyinfo->key_part[keyuse->keypart].store_length;
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
    return TRUE;
  }
  j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
  j->ref.key_err=1;
  keyuse=org_keyuse;

  store_key **ref_key=j->ref.key_copy;
  byte *key_buff=j->ref.key_buff;
  if (ftkey)
  {
    j->ref.items[0]=((Item_func*)(keyuse->val))->key_item();
    if (keyuse->used_tables)
      return TRUE; // not supported yet. SerG

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
      if (!keyuse->used_tables &&
	  !(join->select_options & SELECT_DESCRIBE))
      {					// Compare against constant
	store_key_item *tmp=new store_key_item(thd,
					       keyinfo->key_part[i].field,
					       (char*)key_buff +
					       maybe_null,
					       maybe_null ?
					       (char*) key_buff : 0,
					       keyinfo->key_part[i].length,
					       keyuse->val);
	if (thd->fatal_error)
	{
	  return TRUE;
	}
	tmp->copy();
      }
      else
	*ref_key++= get_store_key(thd,
				  keyuse,join->const_table_map,
				  &keyinfo->key_part[i],
				  (char*) key_buff,maybe_null);
      key_buff+=keyinfo->key_part[i].store_length;
    }
  } /* not ftkey */
  *ref_key=0;				// end_marker
  if (j->type == JT_FT)  /* no-op */;
  else if (j->type == JT_CONST)
    j->table->const_table=1;
  else if (((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY |
			       HA_END_SPACE_KEY)) != HA_NOSAME) ||
	   keyparts != keyinfo->key_parts)
    j->type=JT_REF;				/* Must read with repeat */
  else if (ref_key == j->ref.key_copy)
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
  return 0;
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
  THD *thd=current_thd;
  ha_rows cuted_fields=thd->cuted_fields;
  /*
    we should restore old value of count_cuted_fields because
    store_val_in_field can be called from mysql_insert 
    with select_insert, which make count_cuted_fields= 1
   */
  bool old_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields=1;
  error= item->save_in_field(field, 1);
  thd->count_cuted_fields= old_count_cuted_fields;
  return error || cuted_fields != thd->cuted_fields;
}


static bool
make_simple_join(JOIN *join,TABLE *tmp_table)
{
  TABLE **tableptr;
  JOIN_TAB *join_tab;

  if (!(tableptr=(TABLE**) join->thd->alloc(sizeof(TABLE*))) ||
      !(join_tab=(JOIN_TAB*) join->thd->alloc(sizeof(JOIN_TAB))))
    return TRUE;
  join->join_tab=join_tab;
  join->table=tableptr; tableptr[0]=tmp_table;
  join->tables=1;
  join->const_tables=0;
  join->const_table_map=0;
  join->tmp_table_param.field_count= join->tmp_table_param.sum_func_count=
    join->tmp_table_param.func_count=0;
  join->tmp_table_param.copy_field=join->tmp_table_param.copy_field_end=0;
  join->first_record=join->sort_and_group=0;
  join->sum_funcs=0;
  join->send_records=(ha_rows) 0;
  join->group=0;
  join->do_send_rows = 1;
  join->row_limit=join->thd->select_limit;

  join_tab->cache.buff=0;			/* No cacheing */
  join_tab->table=tmp_table;
  join_tab->select=0;
  join_tab->select_cond=0;
  join_tab->quick=0;
  join_tab->type= JT_ALL;			/* Map through all records */
  join_tab->keys= (uint) ~0;			/* test everything in quick */
  join_tab->info=0;
  join_tab->on_expr=0;
  join_tab->ref.key = -1;
  join_tab->not_used_in_distinct=0;
  join_tab->read_first_record= join_init_read_record;
  join_tab->join=join;
  bzero((char*) &join_tab->read_record,sizeof(join_tab->read_record));
  tmp_table->status=0;
  tmp_table->null_row=0;
  return FALSE;
}


static bool
make_join_select(JOIN *join,SQL_SELECT *select,COND *cond)
{
  DBUG_ENTER("make_join_select");
  if (select)
  {
    table_map used_tables;
    if (join->tables > 1)
      cond->update_used_tables();		// Tablenr may have changed
    if (join->const_tables == join->tables)
      join->const_table_map|=RAND_TABLE_BIT;
    {						// Check const tables
      COND *const_cond=
	make_cond_for_table(cond,join->const_table_map,(table_map) 0);
      DBUG_EXECUTE("where",print_where(const_cond,"constants"););
      if (const_cond && !const_cond->val_int())
      {
	DBUG_PRINT("info",("Found impossible WHERE condition"));
	DBUG_RETURN(1);				// Impossible const condition
      }
    }
    used_tables=(select->const_tables=join->const_table_map) | RAND_TABLE_BIT;
    for (uint i=join->const_tables ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      table_map current_map= tab->table->map;
      /*
	Following force including random expression in last table condition.
	It solve problem with select like SELECT * FROM t1 WHERE rand() > 0.5
      */
      if (i == join->tables-1)
	current_map|= RAND_TABLE_BIT;
      bool use_quick_range=0;
      used_tables|=current_map;

      if (tab->type == JT_REF && tab->quick &&
	  (uint) tab->ref.key == tab->quick->index &&
	  tab->ref.key_length < tab->quick->max_used_key_length)
      {
	/* Range uses longer key;  Use this instead of ref on key */
	tab->type=JT_ALL;
	use_quick_range=1;
	tab->use_quick=1;
	tab->ref.key_parts=0;		// Don't use ref key.
	join->best_positions[i].records_read= rows2double(tab->quick->records);
      }

      COND *tmp=make_cond_for_table(cond,used_tables,current_map);
      if (!tmp && tab->quick)
      {						// Outer join
	/*
	  Hack to handle the case where we only refer to a table
	  in the ON part of an OUTER JOIN.
	*/
	tmp=new Item_int((longlong) 1,1);	// Always true
      }
      if (tmp)
      {
	DBUG_EXECUTE("where",print_where(tmp,tab->table->table_name););
	SQL_SELECT *sel=tab->select=(SQL_SELECT*)
	  join->thd->memdup((gptr) select, sizeof(SQL_SELECT));
	if (!sel)
	  DBUG_RETURN(1);			// End of memory
	tab->select_cond=sel->cond=tmp;
	sel->head=tab->table;
	if (tab->quick)
	{
	  /* Use quick key read if it's a constant and it's not used
	     with key reading */
	  if (tab->needed_reg == 0 && tab->type != JT_EQ_REF
	      && tab->type != JT_FT && (tab->type != JT_REF ||
	       (uint) tab->ref.key == tab->quick->index))
	  {
	    sel->quick=tab->quick;		// Use value from get_quick_...
	    sel->quick_keys=0;
	    sel->needed_reg=0;
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
	  if (tab->const_keys && tab->table->reginfo.impossible_range)
	    DBUG_RETURN(1);
	}
	else if (tab->type == JT_ALL && ! use_quick_range)
	{
	  if (tab->const_keys &&
	      tab->table->reginfo.impossible_range)
	    DBUG_RETURN(1);				// Impossible range
	  /*
	    We plan to scan all rows.
	    Check again if we should use an index.
	    We could have used an column from a previous table in
	    the index if we are using limit and this is the first table
	  */

	  if ((tab->keys & ~ tab->const_keys && i > 0) ||
	      (tab->const_keys && i == join->const_tables &&
	       join->thd->select_limit < join->best_positions[i].records_read &&
	       !(join->select_options & OPTION_FOUND_ROWS)))
	  {
	    /* Join with outer join condition */
	    COND *orig_cond=sel->cond;
	    sel->cond=and_conds(sel->cond,tab->on_expr);
	    if (sel->test_quick_select(join->thd, tab->keys,
				       used_tables & ~ current_map,
				       (join->select_options &
					OPTION_FOUND_ROWS ?
					HA_POS_ERROR :
					join->thd->select_limit)) < 0)
            { /* before reporting "Impossible WHERE" for the whole query
                 we have to check isn't it only "impossible ON" instead */
              sel->cond=orig_cond;
              if (!tab->on_expr ||
                  sel->test_quick_select(join->thd, tab->keys,
                                         used_tables & ~ current_map,
                                         (join->select_options &
                                          OPTION_FOUND_ROWS ?
                                          HA_POS_ERROR :
                                          join->thd->select_limit)) < 0)
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
	    sel->quick_keys=0;
	  }
	  if ((sel->quick_keys | sel->needed_reg) & ~tab->checked_keys)
	  {
	    tab->keys=sel->quick_keys | sel->needed_reg;
	    tab->use_quick= (sel->needed_reg &&
			     (!select->quick_keys ||
			      (select->quick &&
			       (select->quick->records >= 100L)))) ?
	      2 : 1;
	    sel->read_tables= used_tables & ~current_map;
	  }
	  if (i != join->const_tables && tab->use_quick != 2)
	  {					/* Read with cache */
	    if ((tmp=make_cond_for_table(cond,
					 join->const_table_map |
					 current_map,
					 current_map)))
	    {
	      DBUG_EXECUTE("where",print_where(tmp,"cache"););
	      tab->cache.select=(SQL_SELECT*)
		join->thd->memdup((gptr) sel, sizeof(SQL_SELECT));
	      tab->cache.select->cond=tmp;
	      tab->cache.select->read_tables=join->const_table_map;
	    }
	  }
	}
      }
    }
  }
  DBUG_RETURN(0);
}


static void
make_join_readinfo(JOIN *join,uint options)
{
  uint i;
  SELECT_LEX *select_lex = &(join->thd->lex.select_lex);
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
      table->file->index_init(tab->ref.key);
      tab->read_first_record= join_read_key;
      tab->read_record.read_record= join_no_more_records;
      if (table->used_keys & ((key_map) 1 << tab->ref.key) &&
	  !table->no_keyread)
      {
	table->key_read=1;
	table->file->extra(HA_EXTRA_KEYREAD);
      }
      break;
    case JT_REF:
      table->status=STATUS_NO_RECORD;
      if (tab->select)
      {
	delete tab->select->quick;
	tab->select->quick=0;
      }
      delete tab->quick;
      tab->quick=0;
      table->file->index_init(tab->ref.key);
      tab->read_first_record= join_read_always_key;
      tab->read_record.read_record= join_read_next_same;
      if (table->used_keys & ((key_map) 1 << tab->ref.key) &&
	  !table->no_keyread)
      {
	table->key_read=1;
	table->file->extra(HA_EXTRA_KEYREAD);
      }
      break;
    case JT_FT:
      table->status=STATUS_NO_RECORD;
      table->file->index_init(tab->ref.key);
      tab->read_first_record= join_ft_read_first;
      tab->read_record.read_record= join_ft_read_next;
      break;
    case JT_ALL:
      /*
	If previous table use cache
      */
      table->status=STATUS_NO_RECORD;
      if (i != join->const_tables && !(options & SELECT_NO_JOIN_CACHE) &&
	  tab->use_quick != 2 && !tab->on_expr)
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
	select_lex->options|=QUERY_NO_GOOD_INDEX_USED;
	tab->read_first_record= join_init_quick_read_record;
	if (statistics)
	  statistic_increment(select_range_check_count, &LOCK_status);
      }
      else
      {
	tab->read_first_record= join_init_read_record;
	if (i == join->const_tables)
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      statistic_increment(select_range_count, &LOCK_status);
	  }
	  else
	  {
	    select_lex->options|=QUERY_NO_INDEX_USED;
	    if (statistics)
	      statistic_increment(select_scan_count, &LOCK_status);
	  }
	}
	else
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      statistic_increment(select_full_range_join_count, &LOCK_status);
	  }
	  else
	  {
	    select_lex->options|=QUERY_NO_INDEX_USED;
	    if (statistics)
	      statistic_increment(select_full_join_count, &LOCK_status);
	  }
	}
	if (!table->no_keyread)
	{
	  if (tab->select && tab->select->quick &&
	      table->used_keys & ((key_map) 1 << tab->select->quick->index))
	  {
	    table->key_read=1;
	    table->file->extra(HA_EXTRA_KEYREAD);
	  }
	  else if (table->used_keys && ! (tab->select && tab->select->quick))
	  {					// Only read index tree
	    tab->index=find_shortest_key(table, table->used_keys);
	    tab->table->file->index_init(tab->index);
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
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,MYF(0));
      return(1);
    }
  }
  return(0);
}


static void
join_free(JOIN *join)
{
  JOIN_TAB *tab,*end;
  DBUG_ENTER("join_free");

  if (join->table)
  {
    /*
      Only a sorted table may be cached.  This sorted table is always the
      first non const table in join->table
    */
    if (join->tables > join->const_tables) // Test for not-const tables
      free_io_cache(join->table[join->const_tables]);
    for (tab=join->join_tab,end=tab+join->tables ; tab != end ; tab++)
    {
      delete tab->select;
      delete tab->quick;
      x_free(tab->cache.buff);
      if (tab->table)
      {
	if (tab->table->key_read)
	{
	  tab->table->key_read=0;
	  tab->table->file->extra(HA_EXTRA_NO_KEYREAD);
	}
	/* Don't free index if we are using read_record */
	if (!tab->read_record.table)
	  tab->table->file->index_end();
      }
      end_read_record(&tab->read_record);
    }
    join->table=0;
  }
  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (join->lock && join->thd->lock &&
      !(join->select_options & SELECT_NO_UNLOCK))
  {
    mysql_unlock_read_tables(join->thd, join->lock);// Don't free join->lock
    join->lock=0;
  }
  join->group_fields.delete_elements();
  join->tmp_table_param.copy_funcs.delete_elements();
  join->tmp_table_param.cleanup();
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
  if (tab->type != JT_EQ_REF || tab->table->maybe_null)
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
    ref->depend_map=depend_map;
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
    if (!(order->depend_map & RAND_TABLE_BIT))	// Not item_sum() or RAND()
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
  simple_order is set to 1 if sort_order only uses fields from head table
  and the head table is not a LEFT JOIN table
*/

static ORDER *
remove_const(JOIN *join,ORDER *first_order, COND *cond, bool *simple_order)
{
  if (join->tables == join->const_tables)
    return 0;					// No need to sort
  DBUG_ENTER("remove_const");
  ORDER *order,**prev_ptr;
  table_map first_table= join->join_tab[join->const_tables].table->map;
  table_map not_const_tables= ~join->const_table_map;
  table_map ref;
  prev_ptr= &first_order;
  *simple_order= join->join_tab[join->const_tables].on_expr ? 0 : 1;

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
      if (order_tables & RAND_TABLE_BIT)
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
	  if (only_eq_ref_tables(join,first_order,ref))
	  {
	    DBUG_PRINT("info",("removing: %s", order->item[0]->full_name()));
	    continue;
	  }
	  *simple_order=0;			// Must do a temp table to sort
	}
      }
    }
    *prev_ptr= order;				// use this entry
    prev_ptr= &order->next;
  }
  *prev_ptr=0;
  if (!first_order)				// Nothing to sort/group
    *simple_order=1;
  DBUG_PRINT("exit",("simple_order: %d",(int) *simple_order));
  DBUG_RETURN(first_order);
}


static int
return_zero_rows(JOIN *join, select_result *result,TABLE_LIST *tables,
		 List<Item> &fields, bool send_row, uint select_options,
		 const char *info, Item *having, Procedure *procedure)
{
  DBUG_ENTER("return_zero_rows");

  if (select_options & SELECT_DESCRIBE)
  {
    describe_info(join, info);
    DBUG_RETURN(0);
  }
  if (procedure)
  {
    if (result->prepare(fields))		// This hasn't been done yet
      DBUG_RETURN(-1);
  }
  if (send_row)
  {
    for (TABLE_LIST *table=tables; table ; table=table->next)
      mark_as_null_row(table->table);		// All fields are NULL
    if (having && having->val_int() == 0)
      send_row=0;
  }
  if (!(result->send_fields(fields,1)))
  {
    if (send_row)
    {
      List_iterator_fast<Item> it(fields);
      Item *item;
      while ((item= it++))
	item->no_rows_in_result();
      result->send_data(fields);
    }
    if (tables)				// Not from do_select()
    {
      /* Close open cursors */
      for (TABLE_LIST *table=tables; table ; table=table->next)
	table->table->file->index_end();
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
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr __attribute__((unused)),
			      size_t size __attribute__((unused))) {} /*lint -e715 */

  Item *and_level;
  Item_func *cmp_func;
  COND_CMP(Item *a,Item_func *b) :and_level(a),cmp_func(b) {}
};

#ifdef __GNUC__
template class I_List<COND_CMP>;
template class I_List_iterator<COND_CMP>;
template class List<Item_func_match>;
template class List_iterator<Item_func_match>;
#endif

/*
  change field = field to field = const for each found field = const in the
  and_level
*/

static void
change_cond_ref_to_const(I_List<COND_CMP> *save_list,Item *and_father,
			 Item *cond, Item *field, Item *value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
      change_cond_ref_to_const(save_list,and_level ? cond : item, item,
			       field, value);
    return;
  }
  if (cond->eq_cmp_result() == Item::COND_OK)
    return;					// Not a boolean function

  Item_bool_func2 *func=  (Item_bool_func2*) cond;
  Item *left_item=  func->arguments()[0];
  Item *right_item= func->arguments()[1];
  Item_func::Functype functype=  func->functype();

  if (right_item->eq(field,0) && left_item != value &&
      (left_item->result_type() != STRING_RESULT ||
       value->result_type() != STRING_RESULT ||
       left_item->binary == value->binary))
  {
    Item *tmp=value->new_item();
    if (tmp)
    {
      func->arguments()[1] = tmp;
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC)
	  && and_father != cond && !left_item->const_item())
      {
	cond->marker=1;
	COND_CMP *tmp2;
	if ((tmp2=new COND_CMP(and_father,func)))
	  save_list->push_back(tmp2);
      }
      func->set_cmp_func(item_cmp_type(func->arguments()[0]->result_type(),
				       func->arguments()[1]->result_type()));
    }
  }
  else if (left_item->eq(field,0) && right_item != value &&
           (right_item->result_type() != STRING_RESULT ||
            value->result_type() != STRING_RESULT ||
            right_item->binary == value->binary))
  {
    Item *tmp=value->new_item();
    if (tmp)
    {
      func->arguments()[0] = value = tmp;
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC)
	  && and_father != cond && !right_item->const_item())
      {
	func->arguments()[0] = func->arguments()[1]; // For easy check
	func->arguments()[1] = value;
	cond->marker=1;
	COND_CMP *tmp2;
	if ((tmp2=new COND_CMP(and_father,func)))
	  save_list->push_back(tmp2);
      }
      func->set_cmp_func(item_cmp_type(func->arguments()[0]->result_type(),
				       func->arguments()[1]->result_type()));
    }
  }
}


static void
propagate_cond_constants(I_List<COND_CMP> *save_list,COND *and_father,
			 COND *cond)
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
      propagate_cond_constants(&save,and_level ? cond : item, item);
    }
    if (and_level)
    {						// Handle other found items
      I_List_iterator<COND_CMP> cond_itr(save);
      COND_CMP *cond_cmp;
      while ((cond_cmp=cond_itr++))
	if (!cond_cmp->cmp_func->arguments()[0]->const_item())
	  change_cond_ref_to_const(&save,cond_cmp->and_level,
				   cond_cmp->and_level,
				   cond_cmp->cmp_func->arguments()[0],
				   cond_cmp->cmp_func->arguments()[1]);
    }
  }
  else if (and_father != cond && !cond->marker)		// In a AND group
  {
    if (cond->type() == Item::FUNC_ITEM &&
	(((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
	 ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC))
    {
      Item_func_eq *func=(Item_func_eq*) cond;
      bool left_const= func->arguments()[0]->const_item();
      bool right_const=func->arguments()[1]->const_item();
      if (!(left_const && right_const) &&
	  (func->arguments()[0]->result_type() ==
	   (func->arguments()[1]->result_type())))
      {
	if (right_const)
	{
	  func->arguments()[1]=resolve_const_item(func->arguments()[1],
						  func->arguments()[0]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_father,and_father,
				   func->arguments()[0],
				   func->arguments()[1]);
	}
	else if (left_const)
	{
	  func->arguments()[0]=resolve_const_item(func->arguments()[0],
						  func->arguments()[1]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_father,and_father,
				   func->arguments()[1],
				   func->arguments()[0]);
	}
      }
    }
  }
}


static COND *
optimize_cond(COND *conds,Item::cond_result *cond_value)
{
  if (!conds)
  {
    *cond_value= Item::COND_TRUE;
    return conds;
  }
  /* change field = field to field = const for each found field = const */
  DBUG_EXECUTE("where",print_where(conds,"original"););
  propagate_cond_constants((I_List<COND_CMP> *) 0,conds,conds);
  /*
    Remove all instances of item == item
    Remove all and-levels where CONST item != CONST item
  */
  DBUG_EXECUTE("where",print_where(conds,"after const change"););
  conds=remove_eq_conds(conds,cond_value) ;
  DBUG_EXECUTE("info",print_where(conds,"after remove"););
  return conds;
}


/*
  Remove const and eq items. Return new item, or NULL if no condition
  cond_value is set to according:
  COND_OK    query is possible (field = constant)
  COND_TRUE  always true	( 1 = 1 )
  COND_FALSE always false	( 1 = 2 )
*/

static COND *
remove_eq_conds(COND *cond,Item::cond_result *cond_value)
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
      Item *new_item=remove_eq_conds(item,&tmp_cond_value);
      if (!new_item)
      {
#ifdef DELETE_ITEMS
	delete item;				// This may be shared
#endif
	li.remove();
      }
      else if (item != new_item)
      {
#ifdef DELETE_ITEMS
	delete item;				// This may be shared
#endif
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
      cond->fix_fields(current_thd,0);

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
    THD *thd=current_thd;
    if (args[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) args[0])->field;
      if (field->flags & AUTO_INCREMENT_FLAG && !field->table->maybe_null &&
	  (thd->options & OPTION_AUTO_IS_NULL) &&
	  thd->insert_id())
      {
	query_cache_abort(&thd->net);
	COND *new_cond;
	if ((new_cond= new Item_func_eq(args[0],
					new Item_int("last_insert_id()",
						     thd->insert_id(),
						     21))))
	{
	  cond=new_cond;
	  cond->fix_fields(thd,0);
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
	  cond->fix_fields(thd,0);
	}
      }
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
  Create field for temporary table

  SYNOPSIS
    create_tmp_field()
    thd			Thread handler
    table		Temporary table
    item		Item to create a field for
    type		Type of item (normally item->type)
    copy_func		If set and item is a function, store copy of item
			in this array
    group		1 if we are going to do a relative group by on result
    modify_item		1 if item->result_field should point to new item.
			This is relevent for how fill_record() is going to
			work:
			If modify_item is 1 then fill_record() will update
			the record in the original table.
			If modify_item is 0 then fill_record() will update
			the temporary table
		       
  RETURN
    0			on error
    new_created field
*/

Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
			Item ***copy_func, Field **from_field,
			bool group, bool modify_item)
{
  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    bool maybe_null=item_sum->maybe_null;
    switch (item_sum->sum_func()) {
    case Item_sum::AVG_FUNC:			/* Place for sum & count */
      if (group)
	return new Field_string(sizeof(double)+sizeof(longlong),
				maybe_null, item->name,table,1);
      else
	return new Field_double(item_sum->max_length,maybe_null,
				item->name, table, item_sum->decimals);
    case Item_sum::STD_FUNC:			/* Place for sum & count */
      if (group)
	return	new Field_string(sizeof(double)*2+sizeof(longlong),
				 maybe_null, item->name,table,1);
      else
	return new Field_double(item_sum->max_length, maybe_null,
				item->name,table,item_sum->decimals);
    case Item_sum::UNIQUE_USERS_FUNC:
      return new Field_long(9,maybe_null,item->name,table,1);
    default:
      switch (item_sum->result_type()) {
      case REAL_RESULT:
	return new Field_double(item_sum->max_length,maybe_null,
				item->name,table,item_sum->decimals);
      case INT_RESULT:
	return new Field_longlong(item_sum->max_length,maybe_null,
				  item->name,table,item->unsigned_flag);
      case STRING_RESULT:
	if (item_sum->max_length > 255)
	  return  new Field_blob(item_sum->max_length,maybe_null,
				 item->name,table,item->binary);
	return	new Field_string(item_sum->max_length,maybe_null,
				 item->name,table,item->binary);
      }
    }
    thd->fatal_error=1;
    return 0;					// Error
  }
  case Item::FIELD_ITEM:
  {
    Field *org_field=((Item_field*) item)->field,*new_field;

    *from_field=org_field;
    // The following should always be true
    if ((new_field= org_field->new_field(&thd->mem_root,table)))
    {
      if (modify_item)
	((Item_field*) item)->result_field= new_field;
      else
	new_field->field_name=item->name;
      if (org_field->maybe_null())
	new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
      if (org_field->type()==FIELD_TYPE_VAR_STRING)
	table->db_create_options|= HA_OPTION_PACK_RECORD;
    }
    return new_field;
  }
  case Item::FUNC_ITEM:
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
  case Item::FIELD_STD_ITEM:
    /* The following can only happen with 'CREATE TABLE ... SELECT' */
  case Item::PROC_ITEM:
  case Item::INT_ITEM:
  case Item::REAL_ITEM:
  case Item::STRING_ITEM:
  case Item::REF_ITEM:
  case Item::NULL_ITEM:
  case Item::VARBIN_ITEM:
  {
    bool maybe_null=item->maybe_null;
    Field *new_field;
    LINT_INIT(new_field);

    switch (item->result_type()) {
    case REAL_RESULT:
      new_field=new Field_double(item->max_length,maybe_null,
				 item->name,table,item->decimals);
      break;
    case INT_RESULT:
      new_field=new Field_longlong(item->max_length,maybe_null,
				   item->name,table, item->unsigned_flag);
      break;
    case STRING_RESULT:
      if (item->max_length > 255)
	new_field=  new Field_blob(item->max_length,maybe_null,
				   item->name,table,item->binary);
      else
	new_field= new Field_string(item->max_length,maybe_null,
				    item->name,table,item->binary);
      break;
    }
    if (copy_func && item->is_result_field())
      *((*copy_func)++) = item;			// Save for copy_funcs
    if (modify_item)
      item->set_result_field(new_field);
    return new_field;
  }
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

TABLE *
create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 bool allow_distinct_limit, ulong select_options)
{
  TABLE *table;
  uint	i,field_count,reclength,null_count,null_pack_length,
        hidden_null_count, hidden_null_pack_length, hidden_field_count,
	blob_count,group_null_items;
  bool	using_unique_constraint=0;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char	*tmpname,path[FN_REFLEN];
  byte	*pos,*group_buff;
  uchar *null_flags;
  Field **reg_field, **from_field, **blob_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  uint temp_pool_slot=MY_BIT_NONE;

  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",("distinct: %d  save_sum_fields: %d  allow_distinct_limit: %d  group: %d",
		      (int) distinct, (int) save_sum_fields,
		      (int) allow_distinct_limit,test(group)));

  statistic_increment(created_tmp_tables, &LOCK_status);

  if (use_temp_pool)
    temp_pool_slot = bitmap_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s%s_%lx_%i", mysql_tmpdir, tmp_file_prefix,
	    current_pid, temp_pool_slot);
  else // if we run out of slots or we are not using tempool
    sprintf(path,"%s%s%lx_%lx_%x",mysql_tmpdir,tmp_file_prefix,current_pid,
            thd->thread_id, thd->tmp_table++);

  if (group)
  {
    if (!param->quick_group)
      group=0;					// Can't use group key
    else for (ORDER *tmp=group ; tmp ; tmp=tmp->next)
    {
      (*tmp->item)->marker=4;			// Store null in key
      if ((*tmp->item)->max_length >= MAX_CHAR_WIDTH)
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
		       &blob_field, sizeof(Field*)*(field_count+1),
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
  table->blob_field= (Field_blob**) blob_field;
  table->real_name=table->path=tmpname;
  /*
    This must be "" as field may refer to it after tempory table is dropped
  */
  table->table_name= (char*) "";
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->blob_ptr_size=mi_portable_sizeof_char_ptr;
  table->map=1;
  table->tmp_table= TMP_TABLE;
  table->db_low_byte_first=1;			// True for HEAP and MyISAM
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;

  /* Calculate which type of fields we will store in the temporary table */

  reclength=blob_count=null_count=hidden_null_count=group_null_items=0;
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
	Item *arg= ((Item_sum*) item)->args[i];
	if (!arg->const_item())
	{
	  Field *new_field=
	    create_tmp_field(thd, table,arg,arg->type(),&copy_func,
			     tmp_from_field, group != 0,not_all_columns);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  *(reg_field++)= new_field;
	  reclength+=new_field->pack_length();
	  if (!(new_field->flags & NOT_NULL_FLAG))
	    null_count++;
	  if (new_field->flags & BLOB_FLAG)
	  {
	    *blob_field++= new_field;
	    blob_count++;
	  }
	  ((Item_sum*) item)->args[i]= new Item_field(new_field);
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
      Field *new_field=create_tmp_field(thd, table, item,type, &copy_func,
					tmp_from_field, group != 0,
					not_all_columns || group !=0);
      if (!new_field)
      {
	if (thd->fatal_error)
	  goto err;				// Got OOM
	continue;				// Some kindf of const item
      }
      if (type == Item::SUM_FUNC_ITEM)
	((Item_sum *) item)->result_field= new_field;
      tmp_from_field++;
      reclength+=new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG))
	null_count++;
      if (new_field->flags & BLOB_FLAG)
      {
	*blob_field++= new_field;
	blob_count++;
      }
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
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
      OPTION_BIG_TABLES)
  {
    table->file=get_new_handler(table,table->db_type=DB_TYPE_MYISAM);
    if (group &&
	(param->group_parts > table->file->max_key_parts() ||
	 param->group_length > table->file->max_key_length()))
      using_unique_constraint=1;
  }
  else
  {
    table->file=get_new_handler(table,table->db_type=DB_TYPE_HEAP);
  }

  if (!using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  table->blob_fields=blob_count;
  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length=(hidden_null_count+7)/8;
  null_pack_length=hidden_null_count+(null_count+7)/8;
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select

  table->fields=field_count;
  table->reclength=reclength;
  {
    uint alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    table->rec_buff_length=alloc_length;
    if (!(table->record[0]= (byte *) my_malloc(alloc_length*3, MYF(MY_WME))))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    table->record[2]= table->record[1]+alloc_length;
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
    else if (!field->zero_pack() &&
	     (field->type() == FIELD_TYPE_STRING ||
	      field->type() == FIELD_TYPE_VAR_STRING) &&
	     length >= 10 && blob_count)
      recinfo->type=FIELD_SKIP_ENDSPACE;
    else
      recinfo->type=FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count=(null_count+7) & ~7;		// move to next byte
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  store_record(table,2);			// Make empty default record

  if (thd->variables.tmp_table_size == ~(ulong) 0)		// No limit
    table->max_rows= ~(ha_rows) 0;
  else
    table->max_rows=(((table->db_type == DB_TYPE_HEAP) ?
		      min(thd->variables.tmp_table_size,
			  thd->variables.max_heap_table_size) :
		      thd->variables.tmp_table_size)/ table->reclength);
  set_if_bigger(table->max_rows,1);		// For dummy start options
  keyinfo=param->keyinfo;

  if (group)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    table->keys=1;
    table->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    for (; group ; group=group->next,key_part_info++)
    {
      Field *field=(*group->item)->tmp_table_field();
      bool maybe_null=(*group->item)->maybe_null;
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset();
      key_part_info->length= (uint16) field->pack_length();
      key_part_info->type=   (uint8) field->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT) ?
	0 : FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
	group->buff=(char*) group_buff;
	if (!(group->field=field->new_field(&thd->mem_root,table)))
	  goto err; /* purecov: inspected */
	if (maybe_null)
	{
	  /*
	    To be able to group on NULL, we reserve place in group_buff
	    for the NULL flag just before the column.
	    The field data is after this flag.
	    The NULL flag is updated by 'end_update()' and 'end_write()'
	  */
	  keyinfo->flags|= HA_NULL_ARE_EQUAL;	// def. that NULL == NULL
	  key_part_info->null_bit=field->null_bit;
	  key_part_info->null_offset= (uint) (field->null_ptr -
					      (uchar*) table->record[0]);
	  group->field->move_field((char*) ++group->buff);
	  group_buff++;
	}
	else
	  group->field->move_field((char*) group_buff);
	group_buff+= key_part_info->length;
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
    if (allow_distinct_limit)
    {
      set_if_smaller(table->max_rows,thd->select_limit);
      param->end_write_records=thd->select_limit;
    }
    else
      param->end_write_records= HA_POS_ERROR;
    table->distinct=1;
    table->keys=1;
    if (blob_count)
    {
      using_unique_constraint=1;
      table->uniques=1;
    }
    if (!(key_part_info= (KEY_PART_INFO*)
	  sql_calloc((keyinfo->key_parts)*sizeof(KEY_PART_INFO))))
      goto err;
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME | HA_NULL_ARE_EQUAL;
    keyinfo->key_length=(uint16) reclength;
    keyinfo->name=(char*) "tmp";
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
					    NullS, table, (bool) 1);
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
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT) ?
	0 : FIELDFLAG_BINARY;
    }
  }
  if (thd->fatal_error)				// If end of memory
    goto err;					 /* purecov: inspected */
  table->db_record_offset=1;
  if (table->db_type == DB_TYPE_MYISAM)
  {
    if (create_myisam_tmp_table(table,param,select_options))
      goto err;
  }
  /* Set table_name for easier debugging */
  table->table_name= base_name(tmpname);
  if (!open_tmp_table(table))
    DBUG_RETURN(table);

 err:
  /*
    Hack to ensure that free_blobs() doesn't fail if blob_field is not yet
    complete
  */
  *table->blob_field= 0;
  free_tmp_table(thd,table);                    /* purecov: inspected */
  bitmap_clear_bit(&temp_pool, temp_pool_slot);
  DBUG_RETURN(NULL);				/* purecov: inspected */
}


static bool open_tmp_table(TABLE *table)
{
  int error;
  if ((error=table->file->ha_open(table->real_name,O_RDWR,HA_OPEN_TMP_TABLE)))
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
  if (table->keys)
  {						// Get keys for ni_create
    bool using_unique_constraint=0;
    MI_KEYSEG *seg= (MI_KEYSEG*) sql_calloc(sizeof(*seg) *
					    keyinfo->key_parts);
    if (!seg)
      goto err;

    if (keyinfo->key_length >= table->file->max_key_length() ||
	keyinfo->key_parts > table->file->max_key_parts() ||
	table->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      table->keys=0;
      table->uniques=1;
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
      table->reclength+=MI_UNIQUE_HASH_LENGTH;
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
      seg->language= MY_CHARSET_CURRENT;
      seg->length=   keyinfo->key_part[i].length;
      seg->start=    keyinfo->key_part[i].offset;
      if (field->flags & BLOB_FLAG)
      {
	seg->type=
	((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	 HA_KEYTYPE_VARBINARY : HA_KEYTYPE_VARTEXT);
	seg->bit_start=seg->length - table->blob_ptr_size;
	seg->flag= HA_BLOB_PART;
	seg->length=0;			// Whole blob in unique constraint
      }
      else
      {
	seg->type=
	  ((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	   HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT);
	if (!(field->flags & ZEROFILL_FLAG) &&
	    (field->type() == FIELD_TYPE_STRING ||
	     field->type() == FIELD_TYPE_VAR_STRING) &&
	    keyinfo->key_part[i].length > 4)
	  seg->flag|=HA_SPACE_PACK;
      }
      if (!(field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (uchar*) table->record[0]);
	/*
	  We are using a GROUP BY on something that contains NULL
	  In this case we have to tell MyISAM that two NULL should
	  on INSERT be compared as equal
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

  if ((error=mi_create(table->real_name,table->keys,&keydef,
		       (uint) (param->recinfo-param->start_recinfo),
		       param->start_recinfo,
		       table->uniques, &uniquedef,
		       &create_info,
		       HA_CREATE_TMP_TABLE)))
  {
    table->file->print_error(error,MYF(0));	/* purecov: inspected */
    table->db_stat=0;
    goto err;
  }
  statistic_increment(created_tmp_disk_tables, &LOCK_status);
  table->db_record_offset=1;
  DBUG_RETURN(0);
 err:
  DBUG_RETURN(1);
}


void
free_tmp_table(THD *thd, TABLE *entry)
{
  const char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter",("table: %s",entry->table_name));

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
    if (!(test_flags & TEST_KEEP_TMP_TABLES) || entry->db_type == DB_TYPE_HEAP)
      entry->file->delete_table(entry->real_name);
    delete entry->file;
  }

  /* free blobs */
  for (Field **ptr=entry->field ; *ptr ; ptr++)
    delete *ptr;
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

  if (table->db_type != DB_TYPE_HEAP || error != HA_ERR_RECORD_FILE_FULL)
  {
    table->file->print_error(error,MYF(0));
    DBUG_RETURN(1);
  }
  new_table= *table;
  new_table.db_type=DB_TYPE_MYISAM;
  if (!(new_table.file=get_new_handler(&new_table,DB_TYPE_MYISAM)))
    DBUG_RETURN(1);				// End of memory

  save_proc_info=thd->proc_info;
  thd->proc_info="converting HEAP to MyISAM";

  if (create_myisam_tmp_table(&new_table,param,
			      thd->lex.select_lex.options | thd->options))
    goto err2;
  if (open_tmp_table(&new_table))
    goto err1;
  table->file->index_end();
  table->file->rnd_init();
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
  /*
    HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it explicitly.
  */
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
  (void) table->file->rnd_end();
  (void) table->file->close();
  (void) table->file->delete_table(table->real_name);
  delete table->file;
  table->file=0;
  *table =new_table;
  table->file->change_table_ptr(table);
  thd->proc_info= (!strcmp(save_proc_info,"Copying to tmp table") ?
		   "Copying to tmp table on disk" : save_proc_info);
  DBUG_RETURN(0);

 err:
  DBUG_PRINT("error",("Got error: %d",write_err));
  table->file->print_error(error,MYF(0));	// Give table is full error
  (void) table->file->rnd_end();
  (void) new_table.file->close();
 err1:
  new_table.file->delete_table(new_table.real_name);
  delete new_table.file;
 err2:
  thd->proc_info=save_proc_info;
  DBUG_RETURN(1);
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
  int error;
  JOIN_TAB *join_tab;
  int (*end_select)(JOIN *, struct st_join_table *,bool);
  DBUG_ENTER("do_select");

  join->procedure=procedure;
  /*
    Tell the client how many fields there are in a row
  */
  if (!table)
    join->result->send_fields(*fields,1);
  else
  {
    VOID(table->file->extra(HA_EXTRA_WRITE_CACHE));
    empty_record(table);
  }
  join->tmp_table=table;			/* Save for easy recursion */
  join->fields= fields;

  /* Set up select_end */
  if (table)
  {
    if (table->group && join->tmp_table_param.sum_func_count)
    {
      if (table->keys)
      {
	DBUG_PRINT("info",("Using end_update"));
	end_select=end_update;
	table->file->index_init(0);
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
    if (join->sort_and_group || (join->procedure &&
				 join->procedure->flags & PROC_GROUP))
      end_select=end_send_group;
    else
      end_select=end_send;
  }
  join->join_tab[join->tables-1].next_select=end_select;

  join_tab=join->join_tab+join->const_tables;
  join->send_records=0;
  if (join->tables == join->const_tables)
  {
    if (!(error=(*end_select)(join,join_tab,0)) || error == -3)
      error=(*end_select)(join,join_tab,1);
  }
  else
  {
    error=sub_select(join,join_tab,0);
    if (error >= 0)
      error=sub_select(join,join_tab,1);
    if (error == -3)
      error=0;					/* select_limit used */
  }

  /* Return 1 if error is sent;  -1 if error should be sent */
  if (error < 0)
  {
    join->result->send_error(0,NullS);		/* purecov: inspected */
    error=1;					// Error sent
  }
  else
  {
    error=0;
    if (!table)					// If sending data to client
    {
      /*
	The following will unlock all cursors if the command wasn't an
	update command
      */
      join_free(join);
      if (join->result->send_eof())
	error= 1;				// Don't send error
    }
    DBUG_PRINT("info",("%ld records output",join->send_records));
  }
  if (table)
  {
    int tmp;
    if ((tmp=table->file->extra(HA_EXTRA_NO_CACHE)))
    {
      my_errno=tmp;
      error= -1;
    }
    if ((tmp=table->file->index_end()))
    {
      my_errno=tmp;
      error= -1;
    }
    if (error == -1)
      table->file->print_error(my_errno,MYF(0));
  }
  DBUG_RETURN(error);
}


static int
sub_select_cache(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  int error;

  if (end_of_records)
  {
    if ((error=flush_cached_records(join,join_tab,FALSE)) < 0)
      return error; /* purecov: inspected */
    return sub_select(join,join_tab,end_of_records);
  }
  if (join->thd->killed)		// If aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0)); /* purecov: inspected */
    return -2;				 /* purecov: inspected */
  }
  if (join_tab->use_quick != 2 || test_if_quick_select(join_tab) <= 0)
  {
    if (!store_record_in_cache(&join_tab->cache))
      return 0;					// There is more room in cache
    return flush_cached_records(join,join_tab,FALSE);
  }
  if ((error=flush_cached_records(join,join_tab,TRUE)) < 0)
    return error; /* purecov: inspected */
  return sub_select(join,join_tab,end_of_records); /* Use ordinary select */
}


static int
sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{

  join_tab->table->null_row=0;
  if (end_of_records)
    return (*join_tab->next_select)(join,join_tab+1,end_of_records);

  /* Cache variables for faster loop */
  int error;
  bool found=0;
  COND *on_expr=join_tab->on_expr, *select_cond=join_tab->select_cond;

  if (!(error=(*join_tab->read_first_record)(join_tab)))
  {
    bool not_exists_optimize= join_tab->table->reginfo.not_exists_optimize;
    bool not_used_in_distinct=join_tab->not_used_in_distinct;
    ha_rows found_records=join->found_records;
    READ_RECORD *info= &join_tab->read_record;

    do
    {
      if (join->thd->killed)			// Aborted by user
      {
	my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
	return -2;				/* purecov: inspected */
      }
      join->examined_rows++;
      if (!on_expr || on_expr->val_int())
      {
	found=1;
	if (not_exists_optimize)
	  break;			// Searching after not null columns
	if (!select_cond || select_cond->val_int())
	{
	  if ((error=(*join_tab->next_select)(join,join_tab+1,0)) < 0)
	    return error;
	  /*
	    Test if this was a SELECT DISTINCT query on a table that
	    was not in the field list;  In this case we can abort if
	    we found a row, as no new rows can be added to the result.
	  */
	  if (not_used_in_distinct && found_records != join->found_records)
	    return 0;
	}
	else
	  info->file->unlock_row();
      }
    } while (!(error=info->read_record(info)));
  }
  if (error > 0)				// Fatal error
    return -1;

  if (!found && on_expr)
  {						// OUTER JOIN
    restore_record(join_tab->table,2);		// Make empty record
    mark_as_null_row(join_tab->table);		// For group by without error
    if (!select_cond || select_cond->val_int())
    {
      if ((error=(*join_tab->next_select)(join,join_tab+1,0)) < 0)
	return error;				/* purecov: inspected */
    }
  }
  return 0;
}


static int
flush_cached_records(JOIN *join,JOIN_TAB *join_tab,bool skipp_last)
{
  int error;
  READ_RECORD *info;

  if (!join_tab->cache.records)
    return 0;				/* Nothing to do */
  if (skipp_last)
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
    reset_cache(&join_tab->cache);
    join_tab->cache.records=0; join_tab->cache.ptr_record= (uint) ~0;
    return -error;			/* No records or error */
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
      my_error(ER_SERVER_SHUTDOWN,MYF(0)); /* purecov: inspected */
      return -2;				// Aborted by user /* purecov: inspected */
    }
    SQL_SELECT *select=join_tab->select;
    if (!error && (!join_tab->cache.select ||
		   !join_tab->cache.select->skipp_record()))
    {
      uint i;
      reset_cache(&join_tab->cache);
      for (i=(join_tab->cache.records- (skipp_last ? 1 : 0)) ; i-- > 0 ;)
      {
	read_cached_record(join_tab);
	if (!select || !select->skipp_record())
	  if ((error=(join_tab->next_select)(join,join_tab+1,0)) < 0)
	    return error; /* purecov: inspected */
      }
    }
  } while (!(error=info->read_record(info)));

  if (skipp_last)
    read_cached_record(join_tab);		// Restore current record
  reset_cache(&join_tab->cache);
  join_tab->cache.records=0; join_tab->cache.ptr_record= (uint) ~0;
  if (error > 0)				// Fatal error
    return -1;					/* purecov: inspected */
  for (JOIN_TAB *tmp2=join->join_tab; tmp2 != join_tab ; tmp2++)
    tmp2->table->status=tmp2->status;
  return 0;
}


/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/
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
      if (!table->outer_join || error > 0)
	DBUG_RETURN(error);
    }
  }
  else
  {
    if (!table->key_read &&
        (table->used_keys & ((key_map) 1 << tab->ref.key)) &&
    	!table->no_keyread &&
        (int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY)
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
      tab->index= tab->ref.key;
    }
    if ((error=join_read_const(tab)))
    {
      tab->info="unique row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->records_read=0.0;
      if (!table->outer_join || error > 0)
	DBUG_RETURN(error);
    }
  }
  if (tab->on_expr && !table->null_row)
  {
    if ((table->null_row= test(tab->on_expr->val_int() == 0)))
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
					   table->primary_key)))
    {
      if (error != HA_ERR_END_OF_FILE)
      {
	table->file->print_error(error,MYF(0));
	return 1;
      }
      mark_as_null_row(tab->table);
      empty_record(table);			// Make empty record
      return -1;
    }
    store_record(table,1);
  }
  else if (!table->status)			// Only happens with left join
    restore_record(table,1);			// restore old record
  table->null_row=0;
  return table->status ? -1 : 0;
}


static int
join_read_const(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    if (cp_buffer_from_ref(&tab->ref))
      error=HA_ERR_KEY_NOT_FOUND;
    else
    {
      error=table->file->index_read_idx(table->record[0],tab->ref.key,
					(byte*) tab->ref.key_buff,
					tab->ref.key_length,HA_READ_KEY_EXACT);
    }
    if (error)
    {
      mark_as_null_row(tab->table);
      empty_record(table);
      if (error != HA_ERR_KEY_NOT_FOUND)
      {
	/* Locking reads can legally return also these errors, do not
	   print them to the .err log */
	if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	  sql_print_error("read_const: Got error %d when reading table %s",
			error, table->path);
	table->file->print_error(error,MYF(0));
	return 1;
      }
      return -1;
    }
    store_record(table,1);
  }
  else if (!(table->status & ~STATUS_NULL_ROW))	// Only happens with left join
  {
    table->status=0;
    restore_record(table,1);			// restore old record
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}


static int
join_read_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

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
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
        sql_print_error("read_key: Got error %d when reading table '%s'",error,
		      table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}


static int
join_read_always_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (cp_buffer_from_ref(&tab->ref))
    return -1;
  if ((error=table->file->index_read(table->record[0],
				     tab->ref.key_buff,
				     tab->ref.key_length,HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_const: Got error %d when reading table %s",error,
		      table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
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

  if (cp_buffer_from_ref(&tab->ref))
    return -1;
  if ((error=table->file->index_read_last(table->record[0],
					  tab->ref.key_buff,
					  tab->ref.key_length)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_const: Got error %d when reading table %s",error,
		      table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
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
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_next: Got error %d when reading table %s",error,
		      table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
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
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_next: Got error %d when reading table %s",error,
		      table->path);
      table->file->print_error(error,MYF(0));
      error= 1;
    }
    else
    {
      table->status= STATUS_GARBAGE;
      error= -1;
    }
  }
  else if (key_cmp(table, tab->ref.key_buff, tab->ref.key,
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
					(table_map) 0, HA_POS_ERROR);
}


static int
join_init_read_record(JOIN_TAB *tab)
{
  if (tab->select && tab->select->quick)
    tab->select->quick->reset();
  init_read_record(&tab->read_record, tab->join->thd, tab->table,
		   tab->select,1,1);
  return (*tab->read_record.read_record)(&tab->read_record);
}

static int
join_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table=tab->table;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)) &&
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
  error=tab->table->file->index_first(tab->table->record[0]);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_first_with_key: Got error %d when reading table",
		      error);
      table->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}


static int
join_read_next(READ_RECORD *info)
{
  int error=info->file->index_next(info->record);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error(
		    "read_next_with_key: Got error %d when reading table %s",
		      error, info->table->path);
      info->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}

static int
join_read_last(JOIN_TAB *tab)
{
  TABLE *table=tab->table;
  int error;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)) &&
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
  error=tab->table->file->index_last(tab->table->record[0]);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("read_last_with_key: Got error %d when reading table",
		      error, table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}


static int
join_read_prev(READ_RECORD *info)
{
  int error=info->file->index_prev(info->record);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error(
		  "read_prev_with_key: Got error %d when reading table: %s",
		      error,info->table->path);
      info->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}


static int
join_ft_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

#if NOT_USED_YET
  if (cp_buffer_from_ref(&tab->ref))       // as ft-key doesn't use store_key's
    return -1;                             // see also FT_SELECT::init()
#endif
  table->file->ft_init();

  error=table->file->ft_read(table->record[0]);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("ft_read_first: Got error %d when reading table %s",
                      error, table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}

static int
join_ft_read_next(READ_RECORD *info)
{
  int error=info->file->ft_read(info->table->record[0]);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
	sql_print_error("ft_read_next: Got error %d when reading table %s",
                      error, info->table->path);
      info->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}


/*****************************************************************************
  The different end of select functions
  These functions returns < 0 when end is reached, 0 on ok and > 0 if a
  fatal error (like table corruption) was detected
*****************************************************************************/

/* ARGSUSED */
static int
end_send(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	 bool end_of_records)
{
  DBUG_ENTER("end_send");
  if (!end_of_records)
  {
    int error;
    if (join->having && join->having->val_int() == 0)
      DBUG_RETURN(0);				// Didn't match having
    error=0;
    if (join->procedure)
      error=join->procedure->send_row(*join->fields);
    else if (join->do_send_rows)
      error=join->result->send_data(*join->fields);
    if (error)
      DBUG_RETURN(-1); /* purecov: inspected */
    if (++join->send_records >= join->thd->select_limit &&
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
	  if (table->record_pointers ||
	      (table->io_cache && my_b_inited(table->io_cache)))
	  {
	    /* Using filesort */
	    join->send_records= table->found_records;
	  }
	  else
	  {
	    table->file->info(HA_STATUS_VARIABLE);
	    join->send_records = table->file->records;
	  }
	}
	else 
	{
	  join->do_send_rows=0;
	  join->thd->select_limit = HA_POS_ERROR;
	  DBUG_RETURN(0);
	}
      }
      DBUG_RETURN(-3);				// Abort nicely
    }
  }
  else
  {
    if (join->procedure && join->procedure->end_of_records())
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


	/* ARGSUSED */
static int
end_send_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	       bool end_of_records)
{
  int idx= -1;
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
 	  else if (join->do_send_rows)
	    error=join->procedure->send_row(*join->fields) ? 1 : 0;
	  if (end_of_records && join->procedure->end_of_records())
	    error= 1;				// Fatal error
	}
	else
	{
	  if (!join->first_record)
	  {
	    /* No matching rows for group function */
	    clear_tables(join);
	    copy_fields(&join->tmp_table_param);
	  }
	  if (join->having && join->having->val_int() == 0)
	    error= -1;				// Didn't satisfy having
	  else if (join->do_send_rows)
	    error=join->result->send_data(*join->fields) ? 1 : 0;
	}
	if (error > 0)
	  DBUG_RETURN(-1);			/* purecov: inspected */
	if (end_of_records)
	{
	  if (!error)
            join->send_records++;
	  DBUG_RETURN(0);
	}
	if (!error &&
	    ++join->send_records >= join->thd->select_limit &&
	    join->do_send_rows)
	{
	  if (!(join->select_options & OPTION_FOUND_ROWS))
	    DBUG_RETURN(-3);				// Abort nicely
	  join->do_send_rows=0;
	  join->thd->select_limit = HA_POS_ERROR;
        }
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(0);
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      init_sum_functions(join->sum_funcs);
      if (join->procedure)
	join->procedure->add();
      DBUG_RETURN(0);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(-1);
  if (join->procedure)
    join->procedure->add();
  DBUG_RETURN(0);
}


	/* ARGSUSED */
static int
end_write(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	  bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int error;
  DBUG_ENTER("end_write");

  if (join->thd->killed)			// Aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    DBUG_RETURN(-2);				/* purecov: inspected */
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
	  Field *field=item->tmp_table_field();
	  field->ptr[-1]= (byte) (field->is_null() ? 1 : 0);
	}
      }
    }
#endif
    if (!join->having || join->having->val_int())
    {
      join->found_records++;
      if ((error=table->file->write_row(table->record[0])))
      {
	if (error == HA_ERR_FOUND_DUPP_KEY ||
	    error == HA_ERR_FOUND_DUPP_UNIQUE)
	  goto end;
	if (create_myisam_from_heap(join->thd, table, &join->tmp_table_param,
				    error,1))
	  DBUG_RETURN(-1);			// Not a table_is_full error
	table->uniques=0;			// To ensure rows are the same
      }
      if (++join->send_records >= join->tmp_table_param.end_write_records &&
	  join->do_send_rows)
      {
	if (!(join->select_options & OPTION_FOUND_ROWS))
	  DBUG_RETURN(-3);
	join->do_send_rows=0;
	join->thd->select_limit = HA_POS_ERROR;
	DBUG_RETURN(0);
      }
    }
  }
end:
  DBUG_RETURN(0);
}

/* Group by searching after group record and updating it if possible */
/* ARGSUSED */

static int
end_update(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	   bool end_of_records)
{
  TABLE *table=join->tmp_table;
  ORDER   *group;
  int	  error;
  DBUG_ENTER("end_update");

  if (end_of_records)
    DBUG_RETURN(0);
  if (join->thd->killed)			// Aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    DBUG_RETURN(-2);				/* purecov: inspected */
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
  // table->file->index_init(0);
  if (!table->file->index_read(table->record[1],
			       join->tmp_table_param.group_buff,0,
			       HA_READ_KEY_EXACT))
  {						/* Update old record */
    restore_record(table,1);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->update_row(table->record[1],
				       table->record[0])))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
    DBUG_RETURN(0);
  }

  /* The null bits are already set */
  KEY_PART_INFO *key_part;
  for (group=table->group,key_part=table->key_info[0].key_part;
       group ;
       group=group->next,key_part++)
    memcpy(table->record[0]+key_part->offset, group->buff, key_part->length);

  init_tmptable_sum_functions(join->sum_funcs);
  copy_funcs(join->tmp_table_param.items_to_copy);
  if ((error=table->file->write_row(table->record[0])))
  {
    if (create_myisam_from_heap(join->thd, table, &join->tmp_table_param,
				error, 0))
      DBUG_RETURN(-1);				// Not a table_is_full error
    /* Change method to update rows */
    table->file->index_init(0);
    join->join_tab[join->tables-1].next_select=end_unique_update;
  }
  join->send_records++;
  DBUG_RETURN(0);
}

/* Like end_update, but this is done with unique constraints instead of keys */

static int
end_unique_update(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		  bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int	  error;
  DBUG_ENTER("end_unique_update");

  if (end_of_records)
    DBUG_RETURN(0);
  if (join->thd->killed)			// Aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    DBUG_RETURN(-2);				/* purecov: inspected */
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
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
    if (table->file->rnd_pos(table->record[1],table->file->dupp_ref))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
    restore_record(table,1);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->update_row(table->record[1],
				       table->record[0])))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
  }
  DBUG_RETURN(0);
}


	/* ARGSUSED */
static int
end_write_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int	  error;
  int	  idx= -1;
  DBUG_ENTER("end_write_group");

  if (join->thd->killed)
  {						// Aborted by user
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    DBUG_RETURN(-2);				/* purecov: inspected */
  }
  if (!join->first_record || end_of_records ||
      (idx=test_if_group_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      if (join->procedure)
	join->procedure->end_group();
      if (idx < (int) join->send_group_parts)
      {
	if (!join->first_record)
	{
	  /* No matching rows for group function */
	  clear_tables(join);
	  copy_fields(&join->tmp_table_param);
	}
	copy_sum_funcs(join->sum_funcs);
	if (!join->having || join->having->val_int())
	{
	  if ((error=table->file->write_row(table->record[0])))
	  {
	    if (create_myisam_from_heap(join->thd, table,
					&join->tmp_table_param, error, 0))
	      DBUG_RETURN(-1);			// Not a table_is_full error
	  }
	  else
	    join->send_records++;
	}
	if (end_of_records)
	  DBUG_RETURN(0);
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(0);
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      copy_funcs(join->tmp_table_param.items_to_copy);
      init_sum_functions(join->sum_funcs);
      if (join->procedure)
	join->procedure->add();
      DBUG_RETURN(0);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(-1);
  if (join->procedure)
    join->procedure->add();
  DBUG_RETURN(0);
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
	*/
	if (field->binary() &&
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
make_cond_for_table(COND *cond,table_map tables,table_map used_table)
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
	new_cond->used_tables_cache=((Item_cond*) cond)->used_tables_cache &
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
      new_cond->used_tables_cache=((Item_cond_or*) cond)->used_tables_cache;
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
  Returns: 1 if key is ok.
	   0 if key can't be used
	  -1 if reverse key can be used
          used_key_parts is set to key parts used if length != 0
*****************************************************************************/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint idx,
				uint *used_key_parts)
{
  KEY_PART_INFO *key_part,*key_part_end;
  key_part=table->key_info[idx].key_part;
  key_part_end=key_part+table->key_info[idx].key_parts;
  key_part_map const_key_parts=table->const_key_parts[idx];
  int reverse=0;

  for (; order ; order=order->next, const_key_parts>>=1)
  {
    Field *field=((Item_field*) (*order->item))->field;
    int flag;

    /*
      Skip key parts that are constants in the WHERE clause.
      These are already skipped in the ORDER BY by const_expression_in_where()
    */
    while (const_key_parts & 1)
    {
      key_part++; const_key_parts>>=1;
    }
    if (key_part == key_part_end || key_part->field != field)
      return 0;

    /* set flag to 1 if we can use read-next on key, else to -1 */
    flag=(order->asc == !(key_part->key_part_flag & HA_REVERSE_SORT))
      ? 1 : -1;
    if (reverse && flag != reverse)
      return 0;
    reverse=flag;				// Remember if reverse
    key_part++;
  }
  *used_key_parts= (uint) (key_part - table->key_info[idx].key_part);
  return reverse;
}

static uint find_shortest_key(TABLE *table, key_map usable_keys)
{
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  for (uint nr=0; usable_keys ; usable_keys>>=1, nr++)
  {
    if (usable_keys & 1)
    {
      if (table->key_info[nr].key_length < min_length)
      {
	min_length=table->key_info[nr].key_length;
	best=nr;
      }
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
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  key_map usable_keys;
  DBUG_ENTER("test_if_skip_sort_order");

  /* Check which keys can be used to resolve ORDER BY */
  usable_keys= ~(key_map) 0;
  for (ORDER *tmp_order=order; tmp_order ; tmp_order=tmp_order->next)
  {
    if ((*tmp_order->item)->type() != Item::FIELD_ITEM)
    {
      usable_keys=0;
      break;
    }
    usable_keys&=((Item_field*) (*tmp_order->item))->field->part_of_sortkey;
  }

  ref_key= -1;
  if (tab->ref.key >= 0)			// Constant range in WHERE
    ref_key=tab->ref.key;
  else if (select && select->quick)		// Range found by opt_range
    ref_key=select->quick->index;

  if (ref_key >= 0)
  {
    /*
      We come here when there is a REF key.
    */
    int order_direction;
    uint used_key_parts;
    /* Check if we get the rows in requested sorted order by using the key */
    if ((usable_keys & ((key_map) 1 << ref_key)) &&
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
            if (table->file->index_flags(ref_key) & HA_NOT_READ_PREFIX_LAST)
              DBUG_RETURN(0);			// Use filesort
	    // ORDER BY range_key DESC
	    QUICK_SELECT_DESC *tmp=new QUICK_SELECT_DESC(select->quick,
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
	  if (table->file->index_flags(ref_key) & HA_NOT_READ_PREFIX_LAST)
	    DBUG_RETURN(0);			// Use filesort
	  tab->read_first_record=       join_read_last_key;
	  tab->read_record.read_record= join_read_prev_same;
	  /* fall through */
	}
      }
      DBUG_RETURN(1);			/* No need to sort */
    }
  }
  else
  {
    /* check if we can use a key to resolve the group */
    /* Tables using JT_NEXT are handled here */
    uint nr;
    key_map keys_to_use=~0,keys=usable_keys;

    /*
      If not used with LIMIT, only use keys if the whole query can be
      resolved with a key;  This is because filesort() is usually faster than
      retrieving all rows through an index.
    */
    if (select_limit >= table->file->records)
      keys_to_use= (table->used_keys |table->file->keys_to_use_for_scanning());

    /*
      We are adding here also the index speified in FORCE INDEX clause, 
      if any.
      This is to allow users to use index in ORDER BY.
    */
 
    if (table->force_index) 
      keys_to_use|= table->keys_in_use_for_query;
    keys&= keys_to_use;

    for (nr=0; keys ; keys>>=1, nr++)
    {
      uint not_used;
      if (keys & 1)
      {
	int flag;
	if ((flag=test_if_order_by_key(order, table, nr, &not_used)))
	{
	  if (!no_changes)
	  {
	    tab->index=nr;
	    tab->read_first_record=  (flag > 0 ? join_read_first:
				      join_read_last);
	    table->file->index_init(nr);
	    tab->type=JT_NEXT;	// Read with index_first(), index_next()
	    if (table->used_keys & ((key_map) 1 << nr))
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


/*****************************************************************************
  If not selecting by given key, create an index how records should be read
  return: 0  ok
	  -1 some fatal error
	   1  no records
*****************************************************************************/

static int
create_sort_index(JOIN_TAB *tab, ORDER *order, ha_rows filesort_limit,
		  ha_rows select_limit)
{
  SORT_FIELD *sortorder;
  uint length;
  ha_rows examined_rows;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  DBUG_ENTER("create_sort_index");

  if (test_if_skip_sort_order(tab,order,select_limit,0))
    DBUG_RETURN(0);
  if (!(sortorder=make_unireg_sortorder(order,&length)))
    goto err;				/* purecov: inspected */
  /* It's not fatal if the following alloc fails */
  table->io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
					MYF(MY_WME | MY_ZEROFILL));
  table->status=0;				// May be wrong if quick_select

  // If table has a range, move it to select
  if (select && !select->quick && tab->ref.key >= 0)
  {
    if (tab->quick)
    {
      select->quick=tab->quick;
      tab->quick=0;
      /* We can only use 'Only index' if quick key is same as ref_key */
      if (table->key_read && (uint) tab->ref.key != select->quick->index)
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
      if (!(select->quick=get_ft_or_quick_select_for_ref(tab->join->thd,
							 table, tab)))
	goto err;
    }
  }
  if (table->tmp_table)
    table->file->info(HA_STATUS_VARIABLE);	// Get record count
  table->found_records=filesort(table,sortorder,length,
				select, 0L, filesort_limit, &examined_rows);
  tab->records=table->found_records;		// For SQL_CALC_ROWS
  delete select;				// filesort did select
  tab->select=0;
  tab->select_cond=0;
  tab->type=JT_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  tab->join->examined_rows+=examined_rows;
  if (table->key_read)				// Restore if we used indexes
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  DBUG_RETURN(table->found_records == HA_POS_ERROR);
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
      if (!(table->select->cond=new Item_cond_and(table->select->cond,
						  sort_table_cond)))
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
    if ((*ptr)->cmp_offset(table->rec_buff_length))
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
    if (item->tmp_table_field() && ! item->const_item())
      field_count++;
  }

  if (!field_count && !(join->select_options & OPTION_FOUND_ROWS)) 
  {                    // only const items with no OPTION_FOUND_ROWS
    join->thd->select_limit=1;	// Only send first row
    DBUG_RETURN(0);
  }
  Field **first_field=entry->field+entry->fields - field_count;
  offset= field_count ? 
          entry->field[entry->fields - field_count]->offset() : 0;
  reclength=entry->reclength-offset;

  free_io_cache(entry);				// Safety
  entry->file->info(HA_STATUS_VARIABLE);
  if (entry->db_type == DB_TYPE_HEAP ||
      (!entry->blob_fields &&
       ((ALIGN_SIZE(reclength) +sizeof(HASH_LINK)) * entry->file->records <
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
  ulong reclength=table->reclength-offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record=(char*) (record=table->record[0])+offset;
  new_record=(char*) table->record[1]+offset;

  file->rnd_init();
  error=file->rnd_next(record);
  for (;;)
  {
    if (thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
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
      my_error(ER_OUT_OF_SORTMEMORY,MYF(0));
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
  handler *file=table->file;
  ulong extra_length=ALIGN_SIZE(key_length)-key_length;
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
  if (hash_init(&hash, (uint) file->records, 0, key_length,
		(hash_get_key) 0, 0, 0))
  {
    my_free((char*) key_buffer,MYF(0));
    DBUG_RETURN(1);
  }
  {
    Field **ptr;
    for (ptr= first_field, field_length=field_lengths ; *ptr ; ptr++)
      (*field_length++)= (*ptr)->pack_length();
  }

  file->rnd_init();
  key_pos=key_buffer;
  for (;;)
  {
    if (thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
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
    field_length=field_lengths;
    for (Field **ptr= first_field ; *ptr ; ptr++)
    {
      (*ptr)->sort_string((char*) key_pos,*field_length);
      key_pos+= *field_length++;
    }
    /* Check if it exists before */
    if (hash_search(&hash,key_pos-key_length,key_length))
    {
      /* Duplicated found ; Remove the row */
      if ((error=file->delete_row(record)))
	goto err;
    }
    else
      (void) hash_insert(&hash, key_pos-key_length);
    key_pos+=extra_length;
  }
  my_free((char*) key_buffer,MYF(0));
  hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->rnd_end();
  DBUG_RETURN(0);

err:
  my_free((char*) key_buffer,MYF(0));
  hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->rnd_end();
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
      pos->field= ((Item_sum*) order->item[0])->tmp_table_field();
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
    if (null_fields && tables[i].table->null_fields)
    {						/* must copy null bits */
      copy->str=(char*) tables[i].table->null_flags;
      copy->length=tables[i].table->null_bytes;
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

  cache->records=0; cache->ptr_record= (uint) ~0;
  cache->length=length+blobs*sizeof(char*);
  cache->blobs=blobs;
  *blob_ptr=0;					/* End sequentel */
  size=max(thd->variables.join_buff_size, cache->length);
  if (!(cache->buff=(uchar*) my_malloc(size,MYF(0))))
    DBUG_RETURN(1);				/* Don't use cache */ /* purecov: inspected */
  cache->end=cache->buff+size;
  reset_cache(cache);
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
  ulong length;
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
	copy->blob_field->get_image((char*) pos,copy->length+sizeof(char*));
	pos+=copy->length+sizeof(char*);
      }
      else
      {
	copy->blob_field->get_image((char*) pos,copy->length); // blob length
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
	memcpy(pos+1,str,length);
	*pos=(uchar) length;
	pos+=length+1;
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
reset_cache(JOIN_CACHE *cache)
{
  cache->record_nr=0;
  cache->pos=cache->buff;
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
	copy->blob_field->set_image((char*) pos,copy->length+sizeof(char*));
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
	memcpy(copy->str,pos+1,length=(uint) *pos);
	memset(copy->str+length,' ',copy->length-length);
	pos+=1+length;
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
  if ((tab->ref.key_err=cp_buffer_from_ref(&tab->ref)) || diff)
    return 1;
  return memcmp(tab->ref.key_buff2, tab->ref.key_buff, tab->ref.key_length)
    != 0;
}


bool
cp_buffer_from_ref(TABLE_REF *ref)
{
  for (store_key **copy=ref->key_copy ; *copy ; copy++)
    if ((*copy)->copy())
      return 1;					// Something went wrong
  return 0;
}


/*****************************************************************************
  Group and order functions
*****************************************************************************/

/*
  Find order/group item in requested columns and change the item to point at
  it. If item doesn't exists, add it first in the field list
  Return 0 if ok.
*/

static int
find_order_in_list(THD *thd,TABLE_LIST *tables,ORDER *order,List<Item> &fields,
		   List<Item> &all_fields)
{
  if ((*order->item)->type() == Item::INT_ITEM)
  {						/* Order by position */
    Item *item=0;
    List_iterator<Item> li(fields);

    for (uint count= (uint) ((Item_int*) (*order->item))->value ;
	 count-- && (item=li++) ;) ;
    if (!item)
    {
      my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),
		      MYF(0),(*order->item)->full_name(),
	       thd->where);
      return 1;
    }
    order->item=li.ref();
    order->in_field_list=1;
    return 0;
  }
  const char *save_where=thd->where;
  thd->where=0;					// No error if not found
  Item **item=find_item_in_list(*order->item,fields);
  thd->where=save_where;
  if (item)
  {
    order->item=item;				// use it
    order->in_field_list=1;
    return 0;
  }
  order->in_field_list=0;
  /* Allow lookup in select's item_list to find aliased fields */
  thd->lex.select_lex.is_item_list_lookup= 1;
  if ((*order->item)->fix_fields(thd,tables) || thd->fatal_error)
  {
    thd->lex.select_lex.is_item_list_lookup= 0;
    return 1;					// Wrong field
  }
  thd->lex.select_lex.is_item_list_lookup= 0;
  all_fields.push_front(*order->item);		// Add new field to field list
  order->item=(Item**) all_fields.head_ref();
  return 0;
}


/*
  Change order to point at item in select list. If item isn't a number
  and doesn't exits in the select list, add it the the field list.
*/

int setup_order(THD *thd,TABLE_LIST *tables,List<Item> &fields,
	     List<Item> &all_fields, ORDER *order)
{
  thd->where="order clause";
  for (; order; order=order->next)
  {
    if (find_order_in_list(thd,tables,order,fields,all_fields))
      return 1;
  }
  return 0;
}


static int
setup_group(THD *thd,TABLE_LIST *tables,List<Item> &fields,
	    List<Item> &all_fields, ORDER *order, bool *hidden_group_fields)
{
  *hidden_group_fields=0;
  if (!order)
    return 0;				/* Everything is ok */

  if (thd->sql_mode & MODE_ONLY_FULL_GROUP_BY)
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
    if (find_order_in_list(thd,tables,order,fields,all_fields))
      return 1;
    (*order->item)->marker=1;		/* Mark found */
    if ((*order->item)->with_sum_func)
    {
      my_printf_error(ER_WRONG_GROUP_FIELD, ER(ER_WRONG_GROUP_FIELD),MYF(0),
		      (*order->item)->full_name());
      return 1;
    }
  }
  if (thd->sql_mode & MODE_ONLY_FULL_GROUP_BY)
  {
    /* Don't allow one to use fields that is not used in GROUP BY */
    Item *item;
    List_iterator<Item> li(fields);

    while ((item=li++))
    {
      if (item->type() != Item::SUM_FUNC_ITEM && !item->marker &&
	  !item->const_item())
      {
	my_printf_error(ER_WRONG_FIELD_WITH_GROUP,
			ER(ER_WRONG_FIELD_WITH_GROUP),
			MYF(0),item->full_name());
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
setup_new_fields(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &all_fields, ORDER *new_field)
{
  Item	  **item;
  DBUG_ENTER("setup_new_fields");

  thd->set_query_id=1;				// Not really needed, but...
  thd->where=0;					// Don't give error
  for (; new_field ; new_field=new_field->next)
  {
    if ((item=find_item_in_list(*new_field->item,fields)))
      new_field->item=item;			/* Change to shared Item */
    else
    {
      thd->where="procedure list";
      if ((*new_field->item)->fix_fields(thd,tables))
	DBUG_RETURN(1); /* purecov: inspected */
      thd->where=0;
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
create_distinct_group(THD *thd, ORDER *order_list, List<Item> &fields, 
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
    if (item->const_item() || item->with_sum_func)
      continue;
    if (!item->marker)
    {
      ORDER *ord=(ORDER*) thd->calloc(sizeof(ORDER));
      if (!ord)
	return 0;
      ord->item=li.ref();
      ord->asc=1;
      *prev=ord;
      prev= &ord->next;
    }
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
    Item::Type type=field->type();
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
	  if (sum_item->args[0]->type() == Item::FIELD_ITEM)
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
  if (!map || (map & RAND_TABLE_BIT))
    DBUG_RETURN(0);

  for (; !(map & tables->table->map) ; tables=tables->next) ;
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
    Field *field=(*group->item)->tmp_table_field();
    if (field)
    {
      if (field->type() == FIELD_TYPE_BLOB)
	key_length+=MAX_BLOB_WIDTH;		// Can't be used as a key
      else
	key_length+=field->pack_length();
    }
    else if ((*group->item)->result_type() == REAL_RESULT)
      key_length+=sizeof(double);
    else if ((*group->item)->result_type() == INT_RESULT)
      key_length+=sizeof(longlong);
    else
      key_length+=(*group->item)->max_length;
    parts++;
    if ((*group->item)->maybe_null)
      null_parts++;
  }
  join->tmp_table_param.group_length=key_length+null_parts;
  join->tmp_table_param.group_parts=parts;
  join->tmp_table_param.group_null_parts=null_parts;
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
      Item_buff *tmp=new_Item_buff(*group->item);
      if (!tmp || join->group_fields.push_front(tmp))
	return TRUE;
    }
  }
  join->sort_and_group=1;			/* Mark for do_select */
  return FALSE;
}


static int
test_if_group_changed(List<Item_buff> &list)
{
  List_iterator<Item_buff> li(list);
  int idx= -1,i;
  Item_buff *buff;

  for (i=(int) list.elements-1 ; (buff=li++) ; i--)
  {
    if (buff->cmp())
      idx=i;
  }
  return idx;
}



/*
  Setup copy_fields to save fields at start of new group
  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_fields
*/

bool
setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param, List<Item> &fields)
{
  Item *pos;
  List_iterator<Item> li(fields);
  Copy_field *copy;
  DBUG_ENTER("setup_copy_fields");

  if (!(copy=param->copy_field= new Copy_field[param->field_count]))
    goto err2;

  param->copy_funcs.empty();
  while ((pos=li++))
  {
    if (pos->type() == Item::FIELD_ITEM)
    {
      Item_field *item=(Item_field*) pos;
      if (item->field->flags & BLOB_FLAG)
      {
	if (!(pos=new Item_copy_string(pos)))
	  goto err;
	VOID(li.replace(pos));
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
	continue;
      }

      /* set up save buffer and change result_field to point at saved value */
      Field *field= item->field;
      item->result_field=field->new_field(&thd->mem_root,field->table);
      char *tmp=(char*) sql_alloc(field->pack_length()+1);
      if (!tmp)
	goto err;
      copy->set(tmp, item->result_field);
      item->result_field->move_field(copy->to_ptr,copy->to_null_ptr,1);
      copy++;
    }
    else if ((pos->type() == Item::FUNC_ITEM ||
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
      VOID(li.replace(pos));
      if (param->copy_funcs.push_back(pos))
	goto err;
    }
  }
  param->copy_field_end= copy;
  DBUG_RETURN(0);

 err:
  delete [] param->copy_field;			// This is never 0
  param->copy_field=0;
err2:
  DBUG_RETURN(TRUE);
}


/*
  Copy fields and null values between two tables
*/

void
copy_fields(TMP_TABLE_PARAM *param)
{
  Copy_field *ptr=param->copy_field;
  Copy_field *end=param->copy_field_end;

  for (; ptr != end; ptr++)
    (*ptr->do_copy)(ptr);

  List_iterator_fast<Item> &it=param->copy_funcs_it;
  it.rewind();
  Item_copy_string *item;
  while ((item = (Item_copy_string*) it++))
    item->copy();
}


/*****************************************************************************
  Make an array of pointer to sum_functions to speed up sum_func calculation
*****************************************************************************/

static bool
make_sum_func_list(JOIN *join,List<Item> &fields)
{
  DBUG_ENTER("make_sum_func_list");
  Item_sum **func =
    (Item_sum**) sql_alloc(sizeof(Item_sum*)*
			   (join->tmp_table_param.sum_func_count+1));
  if (!func)
    DBUG_RETURN(TRUE);
  List_iterator<Item> it(fields);
  join->sum_funcs=func;

  Item *field;
  while ((field=it++))
  {
    if (field->type() == Item::SUM_FUNC_ITEM && !field->const_item())
    {
      *func++=(Item_sum*) field;
      /* let COUNT(DISTINCT) create the temporary table */
      if (((Item_sum*) field)->setup(join->thd))
	DBUG_RETURN(TRUE);
    }
  }
  *func=0;					// End marker
  DBUG_RETURN(FALSE);
}


/*
  Change all funcs and sum_funcs to fields in tmp table
*/

static bool
change_to_use_tmp_fields(List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item_field,*item;

  while ((item=it++))
  {
    Field *field;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
      continue;
    if (item->type() == Item::FIELD_ITEM)
    {
      ((Item_field*) item)->field=
	((Item_field*) item)->result_field;
    }
    else if ((field=item->tmp_table_field()))
    {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group)
	item_field=((Item_sum*) item)->result_item(field);
      else
	item_field=(Item*) new Item_field(field);
      if (!item_field)
	return TRUE;				// Fatal error
      item_field->name=item->name;		/*lint -e613 */
#ifndef DBUG_OFF
      if (_db_on_ && !item_field->name)
      {
	char buff[256];
	String str(buff,sizeof(buff));
	str.length(0);
	item->print(&str);
	item_field->name=sql_strmake(str.ptr(),str.length());
      }
#endif
#ifdef DELETE_ITEMS
      delete it.replace(item_field);		/*lint -e613 */
#else
      (void) it.replace(item_field);		/*lint -e613 */
#endif
    }
  }
  return FALSE;
}


/*
  Change all sum_func refs to fields to point at fields in tmp table
  Change all funcs to be fields in tmp table
*/

static bool
change_refs_to_tmp_fields(THD *thd,List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item;

  while ((item= it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM)
    {
      if (!item->const_item())
      {
	Item_sum *sum_item= (Item_sum*) item;
	if (sum_item->result_field)		// If not a const sum func
	{
	  Field *result_field=sum_item->result_field;
	  for (uint i=0 ; i < sum_item->arg_count ; i++)
	  {
	    Item *arg= sum_item->args[i];
	    if (!arg->const_item())
	    {
	      if (arg->type() == Item::FIELD_ITEM)
		((Item_field*) arg)->field= result_field++;
	      else
		sum_item->args[i]= new Item_field(result_field++);
	    }
	  }
	}
      }
    }
    else if (item->with_sum_func)
      continue;
    else if ((item->type() == Item::FUNC_ITEM ||
	      item->type() == Item::COND_ITEM) &&
	     !item->const_item())
    {						/* All funcs are stored */
#ifdef DELETE_ITEMS
      delete it.replace(new Item_field(((Item_func*) item)->result_field));
#else
      (void) it.replace(new Item_field(((Item_func*) item)->result_field));
#endif
    }
    else if (item->type() == Item::FIELD_ITEM)	/* Change refs */
    {
      ((Item_field*)item)->field=((Item_field*) item)->result_field;
    }
  }
  return thd->fatal_error;
}



/******************************************************************************
  Code for calculating functions
******************************************************************************/

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
copy_sum_funcs(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func = *func_ptr) ; func_ptr++)
    (void) func->save_in_result_field(1);
  return;
}


static void
init_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    func->reset();
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


/*****************************************************************************
  Create a condition for a const reference and add this to the
  currenct select for the table
*****************************************************************************/

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
    Field *field=table->field[table->key_info[join_tab->ref.key].key_part[i].fieldnr-1];
    Item *value=join_tab->ref.items[i];
    cond->add(new Item_func_equal(new Item_field(field),value));
  }
  if (thd->fatal_error)
    DBUG_RETURN(TRUE);

  /*
    Here we pass 0 as the first argument to fix_fields that don't need
    to do any stack checking (This is already done in the initial fix_fields).
  */
  cond->fix_fields((THD *) 0,(TABLE_LIST *) 0);
  if (join_tab->select)
  {
    error=(int) cond->add(join_tab->select->cond);
    join_tab->select_cond=join_tab->select->cond=cond;
  }
  else if ((join_tab->select=make_select(join_tab->table, 0, 0, cond,&error)))
    join_tab->select_cond=cond;

  DBUG_RETURN(error ? TRUE : FALSE);
}

/****************************************************************************
  Send a description about what how the select will be done to stdout
****************************************************************************/

static void select_describe(JOIN *join, bool need_tmp_table, bool need_order,
			    bool distinct,const char *message)
{
  List<Item> field_list;
  Item *item;
  List<Item> item_list;
  THD *thd=join->thd;
  MYSQL_LOCK *save_lock;
  SELECT_LEX *select_lex = &(join->thd->lex.select_lex);
  select_result *result=join->result;
  Item *item_null= new Item_null();
  DBUG_ENTER("select_describe");

  /* Don't log this into the slow query log */
  select_lex->options&= ~(QUERY_NO_INDEX_USED | QUERY_NO_GOOD_INDEX_USED);
  thd->offset_limit=0;
  if (thd->lex.select == select_lex)
  {
    field_list.push_back(new Item_empty_string("table",NAME_LEN));
    field_list.push_back(new Item_empty_string("type",10));
    field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_LEN*MAX_KEY));
    item->maybe_null=1;
    field_list.push_back(item=new Item_empty_string("key",NAME_LEN));
    item->maybe_null=1;
    field_list.push_back(item=new Item_int("key_len",0,3));
    item->maybe_null=1;
    field_list.push_back(item=new Item_empty_string("ref",
						    NAME_LEN*MAX_REF_PARTS));
    item->maybe_null=1;
    field_list.push_back(new Item_real("rows",0.0,0,10));
    field_list.push_back(new Item_empty_string("Extra",255));
    if (result->send_fields(field_list,1))
      return;
  }

  if (message)
  {
    Item *empty= new Item_empty_string("",0);
    for (uint i=0 ; i < 7; i++)
      item_list.push_back(empty);
    item_list.push_back(new Item_string(message,strlen(message)));
    if (result->send_data(item_list))
      result->send_error(0,NullS);
  }
  else
  {
    table_map used_tables=0;
    for (uint i=0 ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      TABLE *table=tab->table;
      char buff[512],*buff_ptr=buff;
      char buff1[512], buff2[512], buff3[512];
      String tmp1(buff1,sizeof(buff1));
      String tmp2(buff2,sizeof(buff2));
      tmp1.length(0);
      tmp2.length(0);
      item_list.empty();

      if (tab->type == JT_ALL && tab->select && tab->select->quick)
	tab->type= JT_RANGE;
      item_list.push_back(new Item_string(table->table_name,
					  strlen(table->table_name)));
      item_list.push_back(new Item_string(join_type_str[tab->type],
					  strlen(join_type_str[tab->type])));
      key_map bits;
      uint j;
      for (j=0,bits=tab->keys ; bits ; j++,bits>>=1)
      {
	if (bits & 1)
	{
	  if (tmp1.length())
	    tmp1.append(',');
	  tmp1.append(table->key_info[j].name);
	}
      }
      if (tmp1.length())
	item_list.push_back(new Item_string(tmp1.ptr(),tmp1.length()));
      else
	item_list.push_back(item_null);
      if (tab->ref.key_parts)
      {
	KEY *key_info=table->key_info+ tab->ref.key;
	item_list.push_back(new Item_string(key_info->name,
					    strlen(key_info->name)));
	item_list.push_back(new Item_int((int32) tab->ref.key_length));
	for (store_key **ref=tab->ref.key_copy ; *ref ; ref++)
	{
	  if (tmp2.length())
	    tmp2.append(',');
	  tmp2.append((*ref)->name());
	}
	item_list.push_back(new Item_string(tmp2.ptr(),tmp2.length()));
      }
      else if (tab->type == JT_NEXT)
      {
	KEY *key_info=table->key_info+ tab->index;
	item_list.push_back(new Item_string(key_info->name,
					    strlen(key_info->name)));
	item_list.push_back(new Item_int((int32) key_info->key_length));
	item_list.push_back(item_null);
      }
      else if (tab->select && tab->select->quick)
      {
	KEY *key_info=table->key_info+ tab->select->quick->index;
	item_list.push_back(new Item_string(key_info->name,
					    strlen(key_info->name)));
	item_list.push_back(new Item_int((int32) tab->select->quick->max_used_key_length));
	item_list.push_back(item_null);
      }
      else
      {
	item_list.push_back(item_null);
	item_list.push_back(item_null);
	item_list.push_back(item_null);
      }
      sprintf(buff3,"%.0f",join->best_positions[i].records_read);
      item_list.push_back(new Item_string(buff3,strlen(buff3)));
      my_bool key_read=table->key_read;
      if ((tab->type == JT_NEXT || tab->type == JT_CONST) &&
	  ((table->used_keys & ((key_map) 1 << tab->index))))
	key_read=1;
      
      if (tab->info)
	item_list.push_back(new Item_string(tab->info,strlen(tab->info)));
      else
      {
	if (tab->select)
	{
	  if (tab->use_quick == 2)
	  {
	    sprintf(buff_ptr,"; Range checked for each record (index map: %u)",
		    tab->keys);
	    buff_ptr=strend(buff_ptr);
	  }
	  else
	    buff_ptr=strmov(buff_ptr,"; Using where");
	}
	if (key_read)
	  buff_ptr= strmov(buff_ptr,"; Using index");
	if (table->reginfo.not_exists_optimize)
	  buff_ptr= strmov(buff_ptr,"; Not exists");
	if (need_tmp_table)
	{
	  need_tmp_table=0;
	  buff_ptr= strmov(buff_ptr,"; Using temporary");
	}
	if (need_order)
	{
	  need_order=0;
	  buff_ptr= strmov(buff_ptr,"; Using filesort");
	}
	if (distinct && test_all_bits(used_tables,thd->used_tables))
	  buff_ptr= strmov(buff_ptr,"; Distinct");
	if (buff_ptr == buff)
	  buff_ptr+= 2;
	item_list.push_back(new Item_string(buff+2,(uint) (buff_ptr - buff)-2));
      }
      // For next iteration
      used_tables|=table->map;
      if (result->send_data(item_list))
	result->send_error(0,NullS);
    }
  }
  if (!thd->lex.select->next)			// Not union
  {
    save_lock=thd->lock;
    thd->lock=(MYSQL_LOCK *)0;
    result->send_eof();
    thd->lock=save_lock;
  }
  DBUG_VOID_RETURN;
}


static void describe_info(JOIN *join, const char *info)
{
  THD *thd= join->thd;

  if (thd->lex.select_lex.next)			/* If in UNION */
  {
    select_describe(join,FALSE,FALSE,FALSE,info);
    return;
  }
  List<Item> field_list;
  String *packet= &thd->packet;

  /* Don't log this into the slow query log */
  thd->lex.select_lex.options&= ~(QUERY_NO_INDEX_USED |
				  QUERY_NO_GOOD_INDEX_USED);
  field_list.push_back(new Item_empty_string("Comment",80));
  if (send_fields(thd,field_list,1))
    return; /* purecov: inspected */
  packet->length(0);
  net_store_data(packet,info);
  if (!my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
    send_eof(&thd->net);
}
