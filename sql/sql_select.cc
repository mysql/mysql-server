/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
                        DYNAMIC_ARRAY *keyuse,List<Item_func_match> &ftfuncs);
static bool update_ref_and_keys(DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,
                                uint tables,COND *conds,table_map table_map,
                                List<Item_func_match> &ftfuncs);
static int sort_keyuse(KEYUSE *a,KEYUSE *b);
static void set_position(JOIN *join,uint index,JOIN_TAB *table,KEYUSE *key);
static void find_best_combination(JOIN *join,table_map rest_tables);
static void find_best(JOIN *join,table_map rest_tables,uint index,
		      double record_count,double read_time);
static uint cache_record_length(JOIN *join,uint index);
static double prev_record_reads(JOIN *join,table_map found_ref);
static bool get_best_combination(JOIN *join);
static store_key *get_store_key(KEYUSE *keyuse, table_map used_tables,
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
static int return_zero_rows(select_result *res,TABLE_LIST *tables,
			    List<Item> &fields, bool send_row,
			    uint select_options, const char *info,
			    Item *having, Procedure *proc);
static COND *optimize_cond(COND *conds,Item::cond_result *cond_value);
static COND *remove_eq_conds(COND *cond,Item::cond_result *cond_value);
static bool const_expression_in_where(COND *conds,Item *item, Item **comp_item);
static bool open_tmp_table(TABLE *table);
static bool create_myisam_tmp_table(TABLE *table,TMP_TABLE_PARAM *param,
				    uint options);
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
static int join_read_const_tables(JOIN *join);
static int join_read_system(JOIN_TAB *tab);
static int join_read_const(JOIN_TAB *tab);
static int join_read_key(JOIN_TAB *tab);
static int join_read_always_key(JOIN_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_init_quick_read_record(JOIN_TAB *tab);
static int test_if_quick_select(JOIN_TAB *tab);
static int join_init_read_record(JOIN_TAB *tab);
static int join_init_read_first_with_key(JOIN_TAB *tab);
static int join_init_read_next_with_key(READ_RECORD *info);
static int join_init_read_last_with_key(JOIN_TAB *tab);
static int join_init_read_prev_with_key(READ_RECORD *info);
static int join_ft_read_first(JOIN_TAB *tab);
static int join_ft_read_next(READ_RECORD *info);
static COND *make_cond_for_table(COND *cond,table_map table,
				 table_map used_table);
static Item* part_of_refkey(TABLE *form,Field *field);
static uint find_shortest_key(TABLE *table, key_map usable_keys);
static bool test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,
				    ha_rows select_limit);
static int create_sort_index(JOIN_TAB *tab,ORDER *order,ha_rows select_limit);
static int remove_duplicates(JOIN *join,TABLE *entry,List<Item> &fields);
static int remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
				   ulong offset);
static int remove_dup_with_hash_index(THD *thd, TABLE *table,
				      uint field_count, Field **first_field,
				      ulong key_length);
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
static ORDER *create_distinct_group(ORDER *order, List<Item> &fields);
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
static void select_describe(JOIN *join, bool need_tmp_table, bool need_order,
			    bool distinct);
static void describe_info(THD *thd, const char *info);

/*****************************************************************************
** check fields, find best join, do the select and output fields.
** mysql_select assumes that all tables are allready opened
*****************************************************************************/

int
mysql_select(THD *thd,TABLE_LIST *tables,List<Item> &fields,COND *conds,
             List<Item_func_match> &ftfuncs,
	     ORDER *order, ORDER *group,Item *having,ORDER *proc_param,
	     uint select_options,select_result *result)
{
  TABLE		*tmp_table;
  int		error,tmp;
  bool		need_tmp,hidden_group_fields;
  bool		simple_order,simple_group,no_order;
  Item::cond_result cond_value;
  SQL_SELECT	*select;
  DYNAMIC_ARRAY keyuse;
  JOIN		join;
  Procedure	*procedure;
  List<Item>	all_fields(fields);
  bool		select_distinct;
  DBUG_ENTER("mysql_select");

  /* Check that all tables, fields, conds and order are ok */

  select_distinct=test(select_options & SELECT_DISTINCT);
  tmp_table=0;
  select=0;
  no_order=0;
  bzero((char*) &keyuse,sizeof(keyuse));
  thd->proc_info="init";
  thd->used_tables=0;				// Updated by setup_fields

  if (setup_fields(thd,tables,fields,1,&all_fields) ||
      setup_conds(thd,tables,&conds) ||
      setup_order(thd,tables,fields,all_fields,order) ||
      setup_group(thd,tables,fields,all_fields,group,&hidden_group_fields) ||
      setup_ftfuncs(thd,tables,ftfuncs))
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
      List_iterator<Item> it(fields);
      Item *item;
      while ((item= it++))
      {
	if (item->with_sum_func)
	  flag|=1;
	else if (!item->const_item())
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
  join.send_records=join.found_records=0;
  join.tmp_table_param.end_write_records= HA_POS_ERROR;
  join.first_record=join.sort_and_group=0;
  join.select_options=select_options;
  join.result=result;
  count_field_types(&join.tmp_table_param,all_fields);
  join.const_tables=0;
  join.having=0;
  join.group= group != 0;

#ifdef RESTRICTED_GROUP
  if (join.sum_func_count && !group && (join.func_count || join.field_count))
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT));
    delete procedure;
    DBUG_RETURN(-1);
  }
#endif
  if (!procedure && result->prepare(fields))
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
    error=return_zero_rows(result, tables, fields,
			   join.tmp_table_param.sum_func_count != 0 && !group,
			   select_options,"Impossible WHERE",join.having,
			   procedure);
    delete procedure;
    DBUG_RETURN(error);
  }

  /* Optimize count(*), min() and max() */
  if (tables && join.tmp_table_param.sum_func_count && ! group)
  {
    int res;
    if ((res=opt_sum_query(tables, all_fields, conds)))
    {
      if (res < 0)
      {
	error=return_zero_rows(result, tables, fields, !group,
			       select_options,"No matching min/max row",
			       join.having,procedure);
	delete procedure;
	DBUG_RETURN(error);
      }
      if (select_options & SELECT_DESCRIBE)
      {
	describe_info(thd,"Select tables optimized away");
	delete procedure;
	DBUG_RETURN(0);
      }
      tables=0;					// All tables resolved
    }
  }
  if (!tables)
  {						// Only test of functions
    error=0;
    if (select_options & SELECT_DESCRIBE)
      describe_info(thd,"No tables used");
    else
    {
      result->send_fields(fields,1);
      if (!having || having->val_int())
      {
	if (result->send_data(fields))
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
    DBUG_RETURN(0);
  }

  error = -1;
  join.sort_by_table=get_sort_by_table(order,group,tables);

  /* Calculate how to do the join */
  thd->proc_info="statistics";
  if (make_join_statistics(&join,tables,conds,&keyuse,ftfuncs) ||
      thd->fatal_error)
    goto err;
  thd->proc_info="preparing";
  if ((tmp=join_read_const_tables(&join)) > 0)
    goto err;
  if (tmp && !(select_options & SELECT_DESCRIBE))
  {
    error=return_zero_rows(result,tables,fields,
			   join.tmp_table_param.sum_func_count != 0 &&
			   !group,0,"",join.having,procedure);
    goto err;
  }
  if (!(thd->options & OPTION_BIG_SELECTS) &&
      join.best_read > (double) thd->max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  {						/* purecov: inspected */
    result->send_error(ER_TOO_BIG_SELECT,ER(ER_TOO_BIG_SELECT)); /* purecov: inspected */
    error= 1;					/* purecov: inspected */
    goto err;					/* purecov: inspected */
  }
  if (join.const_tables && !thd->locked_tables)
    mysql_unlock_some_tables(thd, join.table,join.const_tables);
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
    error=return_zero_rows(result,tables,fields,
			   join.tmp_table_param.sum_func_count != 0 && !group,
			   select_options,
			   "Impossible WHERE noticed after reading const tables",
			   join.having,procedure);
    goto err;
  }

  error= -1;					/* if goto err */

  /* Optimize distinct away if possible */
  order=remove_const(&join,order,conds,&simple_order);
  if (group || join.tmp_table_param.sum_func_count)
  {
    if (! hidden_group_fields)
      select_distinct=0;
  }
  else if (select_distinct && join.tables - join.const_tables == 1 &&
	   (order || thd->select_limit == HA_POS_ERROR))
  {
    if ((group=create_distinct_group(order,fields)))
    {
      select_distinct=0;
      no_order= !order;
      join.group=1;				// For end_write_group
    }
    else if (thd->fatal_error)			// End of memory
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

  // Can't use sort on head table if using cache
  if (join.full_join)
  {
    if (group)
      simple_group=0;
    if (order)
      simple_order=0;
  }

  need_tmp= (join.const_tables != join.tables &&
	     ((select_distinct || !simple_order || !simple_group) ||
	      (group && order) ||
	      test(select_options & OPTION_BUFFER_RESULT)));

  make_join_readinfo(&join,
		     (select_options & SELECT_DESCRIBE) | SELECT_USE_CACHE);
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
	!test_if_skip_sort_order(&join.join_tab[join.const_tables], group,
				 HA_POS_ERROR)) ||
       select_distinct) &&
      join.tmp_table_param.quick_group && !procedure)
  {
    need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
  }

  if (select_options & SELECT_DESCRIBE)
  {
    if (!order && !no_order)
      order=group;
    if (order &&
	(join.const_tables == join.tables ||
	 test_if_skip_sort_order(&join.join_tab[join.const_tables], order,
				 (having || group ||
				  join.const_tables != join.tables - 1) ?
				 HA_POS_ERROR : thd->select_limit)))
      order=0;
    select_describe(&join,need_tmp,
		    (order != 0 &&
		     (!need_tmp || order != group || simple_group)),
		    select_distinct);
    error=0;
    goto err;
  }

  /* Perform FULLTEXT search before all regular searches */
  if (ftfuncs.elements)
  {
    List_iterator<Item_func_match> li(ftfuncs);
    Item_func_match *ifm;
    DBUG_PRINT("info",("Performing FULLTEXT search"));
    thd->proc_info="FULLTEXT searching";

    while ((ifm=li++))
    {
      ifm->init_search(test(order));
    }
  }
  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    DBUG_PRINT("info",("Creating tmp table"));
    thd->proc_info="Creating tmp table";

    if (!(tmp_table =
	  create_tmp_table(thd,&join.tmp_table_param,all_fields,
			   ((!simple_group && !procedure &&
			     !(test_flags & TEST_NO_KEY_GROUP)) ?
			    group : (ORDER*) 0),
			   group ? 0 : select_distinct,
			   group && simple_group,
			   order == 0,
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
			    HA_POS_ERROR) ||
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
			      HA_POS_ERROR))
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
    }

    /* Copy data to the temporary table */
    thd->proc_info="Copying to tmp table";
    if (do_select(&join,(List<Item> *) 0,tmp_table,0))
      goto err;					/* purecov: inspected */
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
      if (!order && !no_order)
	order=group;				/* order by group */
      group=0;
    }

    /*
    ** If we have different sort & group then we must sort the data by group
    ** and copy it to another tmp table
    */

    if (group && (!test_if_subpart(group,order) || select_distinct))
    {					/* Must copy to another table */
      TABLE *tmp_table2;
      DBUG_PRINT("info",("Creating group table"));

      /* Free first data from old join */
      join_free(&join);
      if (make_simple_join(&join,tmp_table))
	goto err;
      calc_group_buffer(&join,group);
      count_field_types(&join.tmp_table_param,all_fields);

      /* group data to new table */
      if (!(tmp_table2 = create_tmp_table(thd,&join.tmp_table_param,all_fields,
					  (ORDER*) 0, 0 , 1, 0,
					  join.select_options)))
	goto err;				/* purecov: inspected */
      if (group)
      {
	thd->proc_info="Creating sort index";
	if (create_sort_index(join.join_tab,group,HA_POS_ERROR) ||
	    alloc_group_fields(&join,group))
	{
	  free_tmp_table(thd,tmp_table2); /* purecov: inspected */
	  goto err;				/* purecov: inspected */
	}
	group=0;
      }
      thd->proc_info="Copying to group table";
      if (make_sum_func_list(&join,all_fields) ||
	  do_select(&join,(List<Item> *) 0,tmp_table2,0))
      {
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
      select_distinct=0;			/* Each row is uniq */

    join_free(&join);				/* Free quick selects */
    if (select_distinct && ! group)
    {
      thd->proc_info="Removing duplicates";
      if (remove_duplicates(&join,tmp_table,fields))
	goto err; /* purecov: inspected */
      select_distinct=0;
    }
    tmp_table->reginfo.lock_type=TL_UNLOCK;
    if (make_simple_join(&join,tmp_table))
      goto err;
    calc_group_buffer(&join,group);
    count_field_types(&join.tmp_table_param,all_fields);
  }
  if (procedure)
  {
    if (procedure->change_columns(fields) ||
	result->prepare(fields))
      goto err;
    count_field_types(&join.tmp_table_param,all_fields);
  }
  if (join.group || join.tmp_table_param.sum_func_count ||
      (procedure && (procedure->flags & PROC_GROUP)))
  {
    alloc_group_fields(&join,group);
    setup_copy_fields(&join.tmp_table_param,all_fields);
    if (make_sum_func_list(&join,all_fields) || thd->fatal_error)
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
	DBUG_EXECUTE("where",print_where(table->select->cond,
					 "select and having"););
	having=make_cond_for_table(having,~ (table_map) 0,~used_tables);
	DBUG_EXECUTE("where",print_where(conds,"having after sort"););
      }
    }
    if (create_sort_index(&join.join_tab[join.const_tables],
			  group ? group : order,
			  (having || group ||
			   join.const_tables != join.tables - 1) ?
			  HA_POS_ERROR : thd->select_limit))
      goto err; /* purecov: inspected */
  }
  join.having=having;				// Actually a parameter
  thd->proc_info="Sending data";
  error=do_select(&join,&fields,NULL,procedure);

err:
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
**	Create JOIN_TABS, make a guess about the table types,
**	Approximate how many records will be used in each table
*****************************************************************************/

static ha_rows get_quick_record_count(SQL_SELECT *select,TABLE *table,
				      key_map keys)
{
  int error;
  DBUG_ENTER("get_quick_record_count");
  if (select)
  {
    select->head=table;
    table->reginfo.impossible_range=0;
    if ((error=select->test_quick_select(keys,(table_map) 0,HA_POS_ERROR))
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


static bool
make_join_statistics(JOIN *join,TABLE_LIST *tables,COND *conds,
		     DYNAMIC_ARRAY *keyuse_array,
		     List<Item_func_match> &ftfuncs)
{
  int error;
  uint i,table_count,const_count,found_ref,refs,key,const_ref,eq_part;
  table_map const_table_map,all_table_map;
  TABLE **table_vector;
  JOIN_TAB *stat,*stat_end,*s,**stat_ref;
  SQL_SELECT *select;
  KEYUSE *keyuse,*start_keyuse;
  table_map outer_join=0;
  JOIN_TAB *stat_vector[MAX_TABLES+1];
  DBUG_ENTER("make_join_statistics");

  table_count=join->tables;
  stat=(JOIN_TAB*) join->thd->calloc(sizeof(JOIN_TAB)*table_count);
  stat_ref=(JOIN_TAB**) join->thd->alloc(sizeof(JOIN_TAB*)*MAX_TABLES);
  table_vector=(TABLE**) join->thd->alloc(sizeof(TABLE**)*(table_count*2));
  if (!stat || !stat_ref || !table_vector)
    DBUG_RETURN(1);				// Eom /* purecov: inspected */
  select=0;

  join->best_ref=stat_vector;

  stat_end=stat+table_count;
  const_table_map=all_table_map=0;
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
    if ((s->on_expr=tables->on_expr))
    {
      // table->maybe_null=table->outer_join=1;	// Mark for send fields
      if (!table->file->records)
      {						// Empty table
	s->key_dependent=s->dependent=0;
	s->type=JT_SYSTEM;
	const_table_map|=table->map;
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
    if ((table->system || table->file->records <= 1L) && ! s->dependent)
    {
      s->type=JT_SYSTEM;
      const_table_map|=table->map;
      set_position(join,const_count++,s,(KEYUSE*) 0);
    }
  }
  stat_vector[i]=0;
  join->outer_join=outer_join;

  /*
  ** If outer join: Re-arrange tables in stat_vector so that outer join
  ** tables are after all tables it is dependent of.
  ** For example: SELECT * from A LEFT JOIN B ON B.c=C.c, C WHERE A.C=C.C
  ** Will shift table B after table C.
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
    if (update_ref_and_keys(keyuse_array,stat,join->tables,
                            conds,~outer_join,ftfuncs))
      DBUG_RETURN(1);

  /* loop until no more const tables are found */
  do
  {
    found_ref=0;
    for (JOIN_TAB **pos=stat_vector+const_count; (s= *pos) ; pos++)
    {
      if (s->dependent)				// If dependent on some table
      {
	if (s->dependent & ~(const_table_map)) // All dep. must be constants
	  continue;
	if (s->table->file->records <= 1L)
	{					// system table
	  s->type=JT_SYSTEM;
	  const_table_map|=s->table->map;
	  set_position(join,const_count++,s,(KEYUSE*) 0);
	  continue;
	}
      }
      /* check if table can be read by key or table only uses const refs */
      if ((keyuse=s->keyuse))
      {
	TABLE *table=s->table;
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
	      if (!((~const_table_map) & keyuse->used_tables))
		const_ref|= (key_map) 1 << keyuse->keypart;
	      else
		refs|=keyuse->used_tables;
	      eq_part|= (uint) 1 << keyuse->keypart;
	    }
	    keyuse++;
	  } while (keyuse->table == table && keyuse->key == key);

	  if (eq_part == PREV_BITS(uint,table->key_info[key].key_parts) &&
	      (table->key_info[key].flags & HA_NOSAME))
	  {
	    if (const_ref == eq_part)
	    {					// Found everything for ref.
	      s->type=JT_CONST;
	      const_table_map|=table->map;
	      set_position(join,const_count++,s,start_keyuse);
	      break;
	    }
	    else
	      found_ref|= refs;		// Table is const if all refs are const
	  }
	}
      }
    }
  } while (const_table_map & found_ref);

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

    /* Set a max range of how many seeks we can expect when using keys */
    s->worst_seeks= (double) (s->read_time*2);
    if (s->worst_seeks < 2.0)			// Fix for small tables
      s->worst_seeks=2.0;

    /* if (s->type == JT_EQ_REF)
      continue; */
    if (s->const_keys)
    {
      ha_rows records;
      if (!select)
	select=make_select(s->table,const_table_map,
			   0,
			   and_conds(conds,s->on_expr),&error);
      records=get_quick_record_count(select,s->table, s->const_keys);
      s->quick=select->quick;
      s->needed_reg=select->needed_reg;
      select->quick=0;
      select->read_tables=const_table_map;
      if (records != HA_POS_ERROR)
      {
	s->found_records=records;
	s->read_time= (ha_rows) (s->quick ? s->quick->read_time : 0.0);
      }
    }
  }
  delete select;

  /* Find best combination and return it */
  join->join_tab=stat;
  join->map2table=stat_ref;
  join->table= join->all_tables=table_vector;
  join->const_tables=const_count;
  join->const_table_map=const_table_map;

  if (join->const_tables != join->tables)
    find_best_combination(join,all_table_map & ~const_table_map);
  else
  {
    memcpy((gptr) join->best_positions,(gptr) join->positions,
	   sizeof(POSITION)*join->const_tables);
    join->best_read=1.0;
  }
  DBUG_RETURN(get_best_combination(join));
}


/*****************************************************************************
**	check with keys are used and with tables references with tables
**	updates in stat:
**	  keys	     Bitmap of all used keys
**	  const_keys Bitmap of all keys with may be used with quick_select
**	  keyuse     Pointer to possible keys
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
    return start;				// No new fields, skipp all

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
	  if (old->val->eq(new_fields->val))
	  {
	    old->level=old->const_level=and_level;
	    old->exists_optimize&=new_fields->exists_optimize;
	  }
	}
	else if (old->val->eq(new_fields->val) && old->eq_func &&
		 new_fields->eq_func)
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
	      Field *field,bool eq_func,Item *value,
	      table_map usable_tables)
{
  bool exists_optimize=0;
  if (!(field->flags & PART_KEY_FLAG))
  {
    // Don't remove column IS NULL on a LEFT JOIN table
    if (!eq_func || !value || value->type() != Item::NULL_ITEM ||
	!field->table->maybe_null || field->null_ptr)
      return;					// Not a key. Skipp it
    exists_optimize=1;
  }
  else
  {
    table_map used_tables=0;
    if (value && (used_tables=value->used_tables()) &
	(field->table->map | RAND_TABLE_BIT))
      return;
    if (!(usable_tables & field->table->map))
    {
      if (!eq_func || !value || value->type() != Item::NULL_ITEM ||
	  !field->table->maybe_null || field->null_ptr)
	return;					// Can't use left join optimize
      exists_optimize=1;
    }
    else
    {
      JOIN_TAB *stat=field->table->reginfo.join_tab;
      stat[0].keys|=field->key_start;		// Add possible keys

      if (!value)
      {						// Probably BETWEEN or IN
	stat[0].const_keys |= field->key_start;
	return;					// Can't be used as eq key
      }

      /* Save the following cases:
	 Field op constant
	 Field LIKE constant where constant doesn't start with a wildcard
	 Field = field2 where field2 is in a different table
	 Field op formula
	 Field IS NULL
	 Field IS NOT NULL
      */
      stat[0].key_dependent|=used_tables;
      if (value->const_item())
	stat[0].const_keys |= field->key_start;

      /* We can't always use indexes when comparing a string index to a
	 number. cmp_type() is checked to allow compare of dates to numbers */
      if (!eq_func ||
	  field->result_type() == STRING_RESULT &&
	  value->result_type() != STRING_RESULT &&
	  field->cmp_type() != value->result_type())
	return;
    }
  }
  /* Store possible eq field */
  (*key_fields)->field=field;
  (*key_fields)->eq_func=eq_func;
  (*key_fields)->val=value;
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
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
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
    if (cond_func->key_item()->type() == Item::FIELD_ITEM)
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->key_item()))->field,
		    0,(Item*) 0,usable_tables);
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
		    (cond_func->arguments()[1]),usable_tables);
    }
    if (cond_func->arguments()[1]->type() == Item::FIELD_ITEM &&
	cond_func->functype() != Item_func::LIKE_FUNC)
    {
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[1]))->field,
		    equal_func,
		    (cond_func->arguments()[0]),usable_tables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      add_key_field(key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[0]))->field,
		    cond_func->functype() == Item_func::ISNULL_FUNC,
		    new Item_null, usable_tables);
    }
    break;
  }
  return;
}

/*
** Add all keys with uses 'field' for some keypart
** If field->and_level != and_level then only mark key_part as const_part
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
  /* Mark that we can optimize LEFT JOIN */
  if (key_field->val->type() == Item::NULL_ITEM &&
      !key_field->field->real_maybe_null())
    key_field->field->table->reginfo.not_exists_optimize=1;
}

static void
add_ft_keys(DYNAMIC_ARRAY *keyuse_array,
            JOIN_TAB *stat,COND *cond,table_map usable_tables)
{
  Item_func_match *cond_func=NULL;

  if (cond->type() == Item::FUNC_ITEM)
  {
    Item_func *func=(Item_func *)cond,
              *arg0=(Item_func *)(func->arguments()[0]),
              *arg1=(Item_func *)(func->arguments()[1]);

    if (func->functype() == Item_func::FT_FUNC)
      cond_func=(Item_func_match *)cond;
    else if ((func->functype() == Item_func::GE_FUNC ||
              func->functype() == Item_func::GT_FUNC)  &&
              arg0->type() == Item::FUNC_ITEM          &&
              arg0->functype() == Item_func::FT_FUNC   &&
              arg1->const_item() && arg1->val()>=0)
      cond_func=(Item_func_match *)arg0;
    else if ((func->functype() == Item_func::LE_FUNC ||
              func->functype() == Item_func::LT_FUNC)  &&
              arg1->type() == Item::FUNC_ITEM          &&
              arg1->functype() == Item_func::FT_FUNC   &&
              arg0->const_item() && arg0->val()>=0)
      cond_func=(Item_func_match *)arg1;
  }
  else if (cond->type() == Item::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      /* I'm too lazy to implement proper recursive descent here,
         and anyway, nobody will use such a stupid queries
         that will require it :-)
         May be later...
       */
      while ((item=li++))
        if (item->type() == Item::FUNC_ITEM &&
            ((Item_func *)item)->functype() == Item_func::FT_FUNC)
        {
          cond_func=(Item_func_match *)item;
          break;
        }
    }
  }

  if(!cond_func)
    return;

  KEYUSE keyuse;

  keyuse.table= cond_func->table;
  keyuse.val =  cond_func;
  keyuse.key =  cond_func->key;
#define FT_KEYPART   (MAX_REF_PARTS+10)
  keyuse.keypart=FT_KEYPART;
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
** Update keyuse array with all possible keys we can use to fetch rows
** join_tab is a array in tablenr_order
** stat is a reference array in 'prefered' order.
*/

static bool
update_ref_and_keys(DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,uint tables,
        COND *cond, table_map normal_tables,List<Item_func_match> &ftfuncs)
{
  uint	and_level,i,found_eq_constant;

  {
    KEY_FIELD *key_fields,*end;

    if (!(key_fields=(KEY_FIELD*)
	  my_malloc(sizeof(key_fields[0])*
		    (current_thd->cond_count+1)*2,MYF(0))))
      return TRUE; /* purecov: inspected */
    and_level=0; end=key_fields;
    if (cond)
      add_key_fields(join_tab,&end,&and_level,cond,normal_tables);
    for (i=0 ; i < tables ; i++)
    {
      if (join_tab[i].on_expr)
      {
	add_key_fields(join_tab,&end,&and_level,join_tab[i].on_expr,
		       join_tab[i].table->map);
      }
    }
    if (init_dynamic_array(keyuse,sizeof(KEYUSE),20,64))
    {
      my_free((gptr) key_fields,MYF(0));
      return TRUE;
    }
    /* fill keyuse with found key parts */
    for (KEY_FIELD *field=key_fields ; field != end ; field++)
      add_key_part(keyuse,field);
    my_free((gptr) key_fields,MYF(0));
  }

  if (ftfuncs.elements)
  {
    add_ft_keys(keyuse,join_tab,cond,normal_tables);
  }

  /*
  ** remove ref if there is a keypart which is a ref and a const.
  ** remove keyparts without previous keyparts.
  ** Special treatment for ft-keys. SerG.
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
**	Go through all combinations of not marked tables and find the one
**	which uses least records
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
  for ( ;next != table ; pos++)
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
  ulong rec;
  double tmp;

  if (!rest_tables)
  {
    DBUG_PRINT("best",("read_time: %g  record_count: %g",read_time,
		       record_count));

    read_time+=record_count/(double) TIME_FOR_COMPARE;
    if (join->sort_by_table &&
	join->sort_by_table != join->positions[join->const_tables].table->table)
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

      if (s->keyuse)
      {						/* Use key if possible */
	TABLE *table=s->table;
	KEYUSE *keyuse,*start_key=0;
	double best_records=DBL_MAX;
	uint max_key_part=0;

	/* Test how we can use keys */
	rec= s->records/MATCHING_ROWS_IN_OTHER_TABLE;  /* Assumed records/key */
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
		** If we find a ref, assume this table matches a proportional
		** part of this table.
		** For example 100 records matching a table with 5000 records
		** gives 5000/100 = 50 records per key
		** Constant tables are ignored and to avoid bad matches,
		** we don't make rec less than 100.
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
	  ** Assume that that each key matches a proportional part of table.
	  */
          if (!found_part && !ft_key)
	    continue;				// Nothing usable found
	  if (rec == 0)
	    rec=1L;				// Fix for small tables

          /*
          ** ft-keys require special treatment
          */
          if (ft_key)
          {
            /*
            ** Really, there should be records=0.0 (yes!)
            ** but 1.0 would be probably safer
            */
            tmp=prev_record_reads(join,found_ref);
            records=1.0;
          }
          else
          {
	  /*
	  ** Check if we found full key
	  */
	  if (found_part == PREV_BITS(uint,keyinfo->key_parts))
	  {				/* use eq key */
	    max_key_part= (uint) ~0;
	    if ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
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
		  records= (double) s->records/rec; // quick_range couldn't use key!
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
	      if (table->used_keys & ((key_map) 1 << key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->file->block_size/2/
		  keyinfo->key_length+1;
		tmp=(record_count*(records+keys_per_block-1)/
		     keys_per_block);
	      }
	      else
		tmp=record_count*min(records,s->worst_seeks);
	    }
	  }
	  else
	  {
	    /*
	    ** Use as much key-parts as possible and a uniq key is better
	    ** than a not unique key
	    ** Set tmp to (previous record count) * (records / combination)
	    */
	    if ((found_part & 1) &&
		!(table->file->option_flag() & HA_ONLY_WHOLE_INDEX))
	    {
	      max_key_part=max_part_bit(found_part);
	      /* Check if quick_range could determinate how many rows we
		 will match */

	      if (table->quick_keys & ((key_map) 1 << key) &&
		  table->quick_key_parts[key] <= max_key_part)
		tmp=records= (double) table->quick_rows[key];
	      else
	      {
		/* Check if we have statistic about the distribution */
		if ((records=keyinfo->rec_per_key[max_key_part-1]))
		  tmp=records;
		else
		{
		  /*
		  ** Assume that the first key part matches 1% of the file
		  ** and that the hole key matches 10 (dupplicates) or 1
		  ** (unique) records.
		  ** Assume also that more key matches proportionally more
		  ** records
		  ** This gives the formula:
		  ** records= (x * (b-a) + a*c-b)/(c-1)
		  **
		  ** b = records matched by whole key
		  ** a = records matched by first key part (10% of all records?)
		  ** c = number of key parts in key
		  ** x = used key parts (1 <= x <= c)
		  */
		  double rec_per_key;
		  if (!(rec_per_key=(double)
			keyinfo->rec_per_key[keyinfo->key_parts-1]))
		    rec_per_key=(double) s->records/rec+1;

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
	      }
	      if (table->used_keys & ((key_map) 1 << key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->file->block_size/2/
		  keyinfo->key_length+1;
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
      */
      if ((records >= s->found_records || best > s->read_time) &&
	  !(s->quick && best_key && s->quick->index == best_key->key &&
	    best_max_key_part >= s->table->quick_key_parts[best_key->key]))
      {						// Check full join
	if (s->on_expr)
	{
	  tmp=s->found_records;			// Can't use read cache
	}
	else
	{
	  tmp=(double) s->read_time;
	  /* Calculate time to read through cache */
	  tmp*=(1.0+floor((double) cache_record_length(join,idx)*
			  record_count/(double) join_buff_size));
	}
	if (best == DBL_MAX ||
	    (tmp  + record_count/(double) TIME_FOR_COMPARE*s->found_records <
	     best + record_count/(double) TIME_FOR_COMPARE*records))
	{
	  /*
	    If the table has a range (s->quick is set) make_join_select()
	    will ensure that this will be used
	  */
	  best=tmp;
	  records=s->found_records;
	  best_key=0;
	}
      }
      join->positions[idx].records_read=(double) records;
      join->positions[idx].key=best_key;
      join->positions[idx].table= s;
      if (!best_key && idx == join->const_tables &&
	  s->table == join->sort_by_table)
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
** Find how much space the prevous read not const tables takes in cache
*/

static uint
cache_record_length(JOIN *join,uint idx)
{
  uint length;
  JOIN_TAB **pos,**end;
  THD *thd=current_thd;

  length=0;
  for (pos=join->best_ref+join->const_tables,end=join->best_ref+idx ;
       pos != end ;
       pos++)
  {
    JOIN_TAB *join_tab= *pos;
    if (!join_tab->used_fieldlength)
    {					/* Not calced yet */
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
**	Set up join struct according to best position.
*****************************************************************************/

static bool
get_best_combination(JOIN *join)
{
  uint i,key,tablenr;
  table_map used_tables;
  TABLE *table;
  JOIN_TAB *join_tab,*j;
  KEYUSE *keyuse;
  KEY *keyinfo;
  uint table_count;
  String *ft_tmp=0;
  char tmp1[FT_QUERY_MAXLEN];
  String tmp2(tmp1,sizeof(tmp1));

  table_count=join->tables;
  if (!(join->join_tab=join_tab=
	(JOIN_TAB*) join->thd->alloc(sizeof(JOIN_TAB)*table_count)))
    return TRUE;

  join->const_tables=0;				/* for checking */
  join->const_table_map=0;
  join->full_join=0;

  used_tables=0;
  for (j=join_tab, tablenr=0 ; tablenr < table_count ; tablenr++,j++)
  {
    TABLE *form;
    *j= *join->best_positions[tablenr].table;
    form=join->table[tablenr]=j->table;
    j->ref.key = -1;
    j->ref.key_parts=0;
    j->info=0;					// For describe
    used_tables|= form->map;
    form->reginfo.join_tab=j;
    if (!j->on_expr)
      form->reginfo.not_exists_optimize=0;	// Only with LEFT JOIN

    if (j->type == JT_SYSTEM)
    {
      j->table->const_table=1;
      if (join->const_tables == tablenr)
      {
	join->const_tables++;
	join->const_table_map|=form->map;
      }
      continue;
    }
    if (!j->keys || !(keyuse= join->best_positions[tablenr].key))
    {
      j->type=JT_ALL;
      if (tablenr != join->const_tables)
	join->full_join=1;
    }
    else
    {
      uint keyparts,length;
      bool ftkey=(keyuse->keypart == FT_KEYPART);
      /*
      ** Use best key from find_best
      */
      table=j->table;
      key=keyuse->key;

      keyinfo=table->key_info+key;
      if (ftkey)
      {
        Item_func_match *ifm=(Item_func_match *)keyuse->val;

        ft_tmp=ifm->key_item()->val_str(&tmp2);
        length=ft_tmp->length();
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
              length+=keyinfo->key_part[keyuse->keypart].length +
                test(keyinfo->key_part[keyuse->keypart].null_bit);
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
      if (!(j->ref.key_buff= (byte*) sql_calloc(ALIGN_SIZE(length)*2)) ||
	  !(j->ref.key_copy= (store_key**) sql_alloc((sizeof(store_key*) *
						      (keyparts+1)))) ||
	  !(j->ref.items=    (Item**) sql_alloc(sizeof(Item*)*keyparts)))
      {
	return TRUE;
      }
      j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
      j->ref.key_err=1;
      keyuse=join->best_positions[tablenr].key;

      store_key **ref_key=j->ref.key_copy;
      byte *key_buff=j->ref.key_buff;
      if (ftkey)
      {
        j->ref.items[0]=((Item_func*)(keyuse->val))->key_item();
        if (!keyuse->used_tables)
        {
          // AFAIK key_buff is zeroed...
	  // We don't need to free ft_tmp as the buffer will be freed atom.
          memcpy((gptr)key_buff, (gptr) ft_tmp->ptr(), ft_tmp->length());
        }
        else
	{
          return TRUE; // not supported yet. SerG
	}
        j->type=JT_FT;
      }
      else
      {
	THD *thd=current_thd;
	for (i=0 ; i < keyparts ; keyuse++,i++)
	{
	  while (keyuse->keypart != i ||
		 ((~used_tables) & keyuse->used_tables))
	    keyuse++;				/* Skipp other parts */

	  uint maybe_null= test(keyinfo->key_part[i].null_bit);
	  j->ref.items[i]=keyuse->val;		// Save for cond removal
	  if (!keyuse->used_tables &&
	      !(join->select_options & SELECT_DESCRIBE))
	  {					// Compare against constant
	    store_key_item *tmp=new store_key_item(keyinfo->key_part[i].field,
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
	    *ref_key++= get_store_key(keyuse,join->const_table_map,
				      &keyinfo->key_part[i],
				      (char*) key_buff,maybe_null);
	  key_buff+=keyinfo->key_part[i].store_length;
	}
      } /* not ftkey */
      *ref_key=0;				// end_marker
      if (j->type == JT_FT)  /* no-op */;
      else if (j->type == JT_CONST)
      {
	j->table->const_table=1;
	if (join->const_tables == tablenr)
	{
	  join->const_tables++;
	  join->const_table_map|=form->map;
	}
      }
      else if (((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) != HA_NOSAME) ||
	       keyparts != keyinfo->key_parts)
	j->type=JT_REF;				/* Must read with repeat */
      else if (ref_key == j->ref.key_copy)
      {						/* Should never be reached */
	j->type=JT_CONST;			/* purecov: deadcode */
	if (join->const_tables == tablenr)
	{
	  join->const_tables++;			/* purecov: deadcode */
	  join->const_table_map|=form->map;
	}
      }
      else
	j->type=JT_EQ_REF;
    }
  }

  for (i=0 ; i < table_count ; i++)
    join->map2table[join->join_tab[i].table->tablenr]=join->join_tab+i;
  update_depend_map(join);
  return 0;
}


static store_key *
get_store_key(KEYUSE *keyuse, table_map used_tables, KEY_PART_INFO *key_part,
	      char *key_buff, uint maybe_null)
{
  if (!((~used_tables) & keyuse->used_tables))		// if const item
  {
    return new store_key_const_item(key_part->field,
				    key_buff + maybe_null,
				    maybe_null ? key_buff : 0,
				    key_part->length,
				    keyuse->val);
  }
  else if (keyuse->val->type() == Item::FIELD_ITEM)
    return new store_key_field(key_part->field,
			       key_buff + maybe_null,
			       maybe_null ? key_buff : 0,
			       key_part->length,
			       ((Item_field*) keyuse->val)->field,
			       keyuse->val->full_name());
  return new store_key_item(key_part->field,
			    key_buff + maybe_null,
			    maybe_null ? key_buff : 0,
			    key_part->length,
			    keyuse->val);
}

/*
** This function is only called for const items on fields which are keys
** returns 1 if there was some conversion made when the field was stored.
*/

bool
store_val_in_field(Field *field,Item *item)
{
  THD *thd=current_thd;
  ulong cuted_fields=thd->cuted_fields;
  thd->count_cuted_fields=1;
  item->save_in_field(field);
  thd->count_cuted_fields=0;
  return cuted_fields != thd->cuted_fields;
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
  join->tmp_table_param.copy_field_count=join->tmp_table_param.field_count=
    join->tmp_table_param.sum_func_count= join->tmp_table_param.func_count=0;
  join->tmp_table_param.copy_field=0;
  join->first_record=join->sort_and_group=0;
  join->sum_funcs=0;
  join->send_records=(ha_rows) 0;
  join->group=0;

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
    {						// Check const tables
      COND *const_cond=
	make_cond_for_table(cond,join->const_table_map,(table_map) 0);
      DBUG_EXECUTE("where",print_where(const_cond,"constants"););
      if (const_cond && !const_cond->val_int())
	DBUG_RETURN(1);				// Impossible const condition
    }
    used_tables=(select->const_tables=join->const_table_map) | RAND_TABLE_BIT;
    for (uint i=join->const_tables ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      table_map current_map= tab->table->map;
      used_tables|=current_map;
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
	  sql_memdup((gptr) select, sizeof(SQL_SELECT));
	if (!sel)
	  DBUG_RETURN(1);			// End of memory
	tab->select_cond=sel->cond=tmp;
	sel->head=tab->table;
	if (tab->quick)
	{
	  /* Use quick key read if it's a constant and it's not used
	     with key reading */
	  if (tab->needed_reg == 0 && tab->type != JT_EQ_REF &&
	      (tab->type != JT_REF ||
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
	else if (tab->type == JT_ALL)
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
	      tab->const_keys && i == join->const_tables &&
	      join->thd->select_limit < join->best_positions[i].records_read)
	  {
	    /* Join with outer join condition */
	    COND *orig_cond=sel->cond;
	    sel->cond=and_conds(sel->cond,tab->on_expr);
	    if (sel->test_quick_select(tab->keys,
				       used_tables & ~ current_map,
				       join->thd->select_limit) < 0)
	      DBUG_RETURN(1);				// Impossible range
	    sel->cond=orig_cond;
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
	    sel->read_tables= used_tables;
	  }
	  if (i != join->const_tables && tab->use_quick != 2)
	  {					/* Read with cache */
	    if ((tmp=make_cond_for_table(cond,
					 join->const_table_map |
					 current_map,
					 current_map)))
	    {
	      DBUG_EXECUTE("where",print_where(tmp,"cache"););
	      tab->cache.select=(SQL_SELECT*) sql_memdup((gptr) sel,
						    sizeof(SQL_SELECT));
	      tab->cache.select->cond=tmp;
	      tab->cache.select->read_tables=join->const_table_map;
	    }
	  }
	}
	if (tab->type == JT_REF && sel->quick &&
	    tab->ref.key_length < sel->quick->max_used_key_length)
	{
	  /* Range uses longer key;  Use this instead of ref on key */
	  tab->type=JT_ALL;
	  tab->use_quick=1;
	  tab->ref.key_parts=0;		// Don't use ref key.
	  join->best_positions[i].records_read=sel->quick->records;
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
      if (table->used_keys & ((key_map) 1 << tab->ref.key))
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
      tab->read_record.read_record= join_read_next;
      if (table->used_keys & ((key_map) 1 << tab->ref.key))
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
      ** if previous table use cache
      */
      table->status=STATUS_NO_RECORD;
      if (i != join->const_tables && (options & SELECT_USE_CACHE) &&
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
	join->thd->lex.options|=QUERY_NO_GOOD_INDEX_USED;
	tab->read_first_record= join_init_quick_read_record;
	statistic_increment(select_range_check_count, &LOCK_status);
      }
      else
      {
	tab->read_first_record= join_init_read_record;
	if (i == join->const_tables)
	{
	  if (tab->select && tab->select->quick)
	  {
	    statistic_increment(select_range_count, &LOCK_status);
	  }
	  else
	  {
	    join->thd->lex.options|=QUERY_NO_INDEX_USED;
	    statistic_increment(select_scan_count, &LOCK_status);
	  }
	}
	else
	{
	  if (tab->select && tab->select->quick)
	  {
	    statistic_increment(select_full_range_join_count, &LOCK_status);
	  }
	  else
	  {
	    join->thd->lex.options|=QUERY_NO_INDEX_USED;
	    statistic_increment(select_full_join_count, &LOCK_status);
	  }
	}
	if (tab->select && tab->select->quick &&
	    table->used_keys & ((key_map) 1 << tab->select->quick->index))
	{
	  table->key_read=1;
	  table->file->extra(HA_EXTRA_KEYREAD);
	}
	else if (table->used_keys && ! (tab->select && tab->select->quick))
	{					// Only read index tree
	  tab->index=find_shortest_key(table, table->used_keys);
	  tab->read_first_record= join_init_read_first_with_key;
	  tab->type=JT_NEXT;		// Read with index_first / index_next
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


static void
join_free(JOIN *join)
{
  JOIN_TAB *tab,*end;

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
      end_read_record(&tab->read_record);
      if (tab->table)
      {
	if (tab->table->key_read)
	{
	  tab->table->key_read=0;
	  tab->table->file->extra(HA_EXTRA_NO_KEYREAD);
	}
	tab->table->file->index_end();
      }
    }
    join->table=0;
  }
  // We are not using tables anymore
  // Unlock all tables. We may be in an INSERT .... SELECT statement.
  if (join->lock && join->thd->lock)
  {
    mysql_unlock_read_tables(join->thd, join->lock);// Don't free join->lock
    join->lock=0;
  }
  join->group_fields.delete_elements();
  join->tmp_table_param.copy_funcs.delete_elements();
  delete [] join->tmp_table_param.copy_field;
  join->tmp_table_param.copy_field=0;
}


/*****************************************************************************
** Remove the following expressions from ORDER BY and GROUP BY:
** Constant expressions
** Expression that only uses tables that are of type EQ_REF and the reference
** is in the ORDER list or if all refereed tables are of the above type.
**
** In the following, the X field can be removed:
** SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t1.a,t2.X
** SELECT * FROM t1,t2,t3 WHERE t1.a=t2.a AND t2.b=t3.b ORDER BY t1.a,t3.X
**
** These can't be optimized:
** SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.X,t1.a
** SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b=t2.b ORDER BY t1.a,t2.c
** SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.b,t1.a
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
	if ((*ref_item)->eq(order->item[0]))
	  break;
      }
      if (order)
      {
	found++;
	dbug_assert(!(order->used & map));
	order->used|=map;
	continue;				// Used in ORDER BY
      }
      if (!only_eq_ref_tables(join,start_order, (*ref_item)->used_tables()))
	return (tab->eq_ref_table=0);
    }
  }
  /* Check that there was no reference to table before sort order */
  for ( ; found && start_order ; start_order=start_order->next)
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
    return 0;					// skip this optimize /* purecov: inspected */
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

  for ( ; join_tab != end ; join_tab++)
  {
    TABLE_REF *ref= &join_tab->ref;
    table_map depend_map=0;
    Item **item=ref->items;
    uint i;
    for (i=0 ; i < ref->key_parts ; i++,item++)
      depend_map|=(*item)->used_tables();
    ref->depend_map=depend_map;
    for (JOIN_TAB *join_tab2=join->join_tab;
	 depend_map ;
	 join_tab2++,depend_map>>=1 )
    {
      if (depend_map & 1)
	ref->depend_map|=join_tab2->ref.depend_map;
    }
  }
}


/* Update the dependency map for the sort order */

static void update_depend_map(JOIN *join, ORDER *order)
{
  for ( ; order ; order=order->next)
  {
    table_map depend_map;
    order->item[0]->update_used_tables();
    order->depend_map=depend_map=order->item[0]->used_tables();
    if (!(order->depend_map & RAND_TABLE_BIT))	// Not item_sum() or RAND()
    {
      for (JOIN_TAB *join_tab=join->join_tab;
	   depend_map ;
	   join_tab++, depend_map>>=1)
      {
	if (depend_map & 1)
	  order->depend_map|=join_tab->ref.depend_map;
      }
    }
  }
}


/*
**  simple_order is set to 1 if sort_order only uses fields from head table
**  and the head table is not a LEFT JOIN table
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
      continue;					// skipp const item
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
return_zero_rows(select_result *result,TABLE_LIST *tables,List<Item> &fields,
		 bool send_row, uint select_options,const char *info,
		 Item *having, Procedure *procedure)
{
  DBUG_ENTER("return_zero_rows");

  if (select_options & SELECT_DESCRIBE)
  {
    describe_info(current_thd, info);
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
  if (!tables || !(result->send_fields(fields,1)))
  {
    if (send_row)
      result->send_data(fields);
    if (tables)					// Not from do_select()
      result->send_eof();			// Should be safe
  }
  DBUG_RETURN(0);
}


static void clear_tables(JOIN *join)
{
  for (uint i=0 ; i < join->tables ; i++)
    mark_as_null_row(join->table[i]);		// All fields are NULL
}

/*****************************************************************************
** Make som simple condition optimization:
** If there is a test 'field = const' change all refs to 'field' to 'const'
** Remove all dummy tests 'item = item', 'const op const'.
** Remove all 'item is NULL', when item can never be null!
** item->marker should be 0 for all items on entry
** Return in cond_value FALSE if condition is impossible (1 = 2)
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
** change field = field to field = const for each found field = const in the
** and_level
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

  if (right_item->eq(field) && left_item != value)
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
  else if (left_item->eq(field) && right_item != value)
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
propagate_cond_constants(I_List<COND_CMP> *save_list,COND *and_level,
			 COND *cond)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
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
  else if (and_level != cond && !cond->marker)		// In a AND group
  {
    if (cond->type() == Item::FUNC_ITEM &&
	(((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
	 ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC))
    {
      Item_func_eq *func=(Item_func_eq*) cond;
      bool left_const= func->arguments()[0]->const_item();
      bool right_const=func->arguments()[1]->const_item();
      if (!(left_const && right_const))
      {
	if (right_const)
	{
	  func->arguments()[1]=resolve_const_item(func->arguments()[1],
						  func->arguments()[0]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_level,and_level,
				   func->arguments()[0],
				   func->arguments()[1]);
	}
	else if (left_const)
	{
	  func->arguments()[0]=resolve_const_item(func->arguments()[0],
						  func->arguments()[1]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_level,and_level,
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
  ** Remove all instances of item == item
  ** Remove all and-levels where CONST item != CONST item
  */
  DBUG_EXECUTE("where",print_where(conds,"after const change"););
  conds=remove_eq_conds(conds,cond_value) ;
  DBUG_EXECUTE("info",print_where(conds,"after remove"););
  return conds;
}


/*
** remove const and eq items. Return new item, or NULL if no condition
** cond_value is set to according:
** COND_OK    query is possible (field = constant)
** COND_TRUE  always true	( 1 = 1 )
** COND_FALSE always false	( 1 = 2 )
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
    ** Handles this special case for some ODBC applications:
    ** The are requesting the row that was just updated with a auto_increment
    ** value with this construct:
    **
    ** SELECT * from table_name where auto_increment_column IS NULL
    ** This will be changed to:
    ** SELECT * from table_name where auto_increment_column = LAST_INSERT_ID
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
		(field->flags & NOT_NULL_FLAG))
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
    if (left_item->eq(right_item))
    {
      if (!left_item->maybe_null ||
	  ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC)
	return (COND*) 0;			// Compare of identical items
    }
  }
  *cond_value=Item::COND_OK;
  return cond;				/* Point at next and level */
}

/*
** Return 1 if the item is a const value in all the WHERE clause
*/

static bool
const_expression_in_where(COND *cond, Item *comp_item, Item **const_item)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype()
		     == Item_func::COND_AND_FUNC);
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
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
    if (left_item->eq(comp_item))
    {
      if (right_item->const_item())
      {
	if (*const_item)
	  return right_item->eq(*const_item);
	*const_item=right_item;
	return 1;
      }
    }
    else if (right_item->eq(comp_item))
    {
      if (left_item->const_item())
      {
	if (*const_item)
	  return left_item->eq(*const_item);
	*const_item=left_item;
	return 1;
      }
    }
  }
  return 0;
}


/****************************************************************************
**	Create a temp table according to a field list.
**	Set distinct if duplicates could be removed
**	Given fields field pointers are changed to point at tmp_table
**	for send_fields
****************************************************************************/

Field *create_tmp_field(TABLE *table,Item *item, Item::Type type,
			Item_result_field ***copy_func, Field **from_field,
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
				  item->name,table);
      case STRING_RESULT:
	if (item_sum->max_length > 255)
	  return  new Field_blob(item_sum->max_length,maybe_null,
				 item->name,table,item->binary);
	return	new Field_string(item_sum->max_length,maybe_null,
				 item->name,table,item->binary);
      }
    }
    current_thd->fatal_error=1;
    return 0;					// Error
  }
  case Item::FIELD_ITEM:
  {
    Field *org_field=((Item_field*) item)->field,*new_field;

    *from_field=org_field;
    if ((new_field= org_field->new_field(table))) // Should always be true
    {
      if (modify_item)
	((Item_field*) item)->result_field= new_field;
      else
	new_field->field_name=item->name;
      if (org_field->maybe_null())
	new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    }
    return new_field;
  }
  case Item::PROC_ITEM:
  case Item::FUNC_ITEM:
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
  case Item::FIELD_STD_ITEM:
    /* The following can only happen with 'CREATE TABLE ... SELECT' */
  case Item::INT_ITEM:
  case Item::REAL_ITEM:
  case Item::STRING_ITEM:
  case Item::REF_ITEM:
  case Item::NULL_ITEM:
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
				   item->name,table);
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
    if (copy_func)
      *((*copy_func)++) = (Item_result_field*) item; // Save for copy_funcs
    if (modify_item)
      ((Item_result_field*) item)->result_field=new_field;
    return new_field;
  }
  default:					// Dosen't have to be stored
    return 0;
  }
}


TABLE *
create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 bool allow_distinct_limit, uint select_options)
{
  TABLE *table;
  uint	i,field_count,reclength,null_count,null_pack_length,
	blob_count,group_null_items;
  bool	using_unique_constraint=0;
  char	*tmpname,path[FN_REFLEN];
  byte	*pos,*group_buff;
  uchar *null_flags;
  Field **reg_field,**from_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item_result_field **copy_func;
  MI_COLUMNDEF *recinfo;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",("distinct: %d  save_sum_fields: %d  allow_distinct_limit: %d  group: %d",
		      (int) distinct, (int) save_sum_fields,
		      (int) allow_distinct_limit,test(group)));

  statistic_increment(created_tmp_tables, &LOCK_status);
  sprintf(path,"%s%s%lx_%lx_%x",mysql_tmpdir,tmp_file_prefix,current_pid,
	  thd->thread_id, thd->tmp_table++);
  if (group)
  {
    if (!param->quick_group)
      group=0;					// Can't use group key
    else for (ORDER *tmp=group ; tmp ; tmp=tmp->next)
      (*tmp->item)->marker=4;			// Store null in key
    if (param->group_length >= MAX_BLOB_WIDTH)
      using_unique_constraint=1;
  }

  field_count=param->field_count+param->func_count+param->sum_func_count;
  if (!my_multi_malloc(MYF(MY_WME),
		       &table,sizeof(*table),
		       &reg_field,sizeof(Field*)*(field_count+1),
		       &from_field,sizeof(Field*)*field_count,
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
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  if (!(param->copy_field=copy=new Copy_field[field_count]))
  {
    my_free((gptr) table,MYF(0)); /* purecov: inspected */
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  param->funcs=copy_func;
  strmov(tmpname,path);
  /* make table according to fields */

  bzero((char*) table,sizeof(*table));
  bzero((char*) reg_field,sizeof(Field*)*(field_count+1));
  bzero((char*) from_field,sizeof(Field*)*field_count);
  table->field=reg_field;
  table->real_name=table->path=tmpname;
  table->table_name=base_name(tmpname);
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->blob_ptr_size=mi_portable_sizeof_char_ptr;
  table->map=1;
  table->tmp_table=1;
  table->db_low_byte_first=1;			// True for HEAP and MyISAM

  /* Calculate with type of fields we will need in heap table */

  reclength=blob_count=null_count=group_null_items=0;

  List_iterator<Item> li(fields);
  Item *item;
  Field **tmp_from_field=from_field;
  while ((item=li++))
  {
    Item::Type type=item->type();
    if (item->with_sum_func && type != Item::SUM_FUNC_ITEM ||
	item->const_item())
      continue;
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      ((Item_sum*) item)->result_field=0;
      for (i=0 ; i < ((Item_sum*) item)->arg_count ; i++)
      {
	Item *arg= ((Item_sum*) item)->args[i];
	if (!arg->const_item())
	{
	  Field *new_field=
	    create_tmp_field(table,arg,arg->type(),&copy_func,tmp_from_field,
			     group != 0,1);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  *(reg_field++)= new_field;
	  reclength+=new_field->pack_length();
	  if (!(new_field->flags & NOT_NULL_FLAG))
	    null_count++;
	  if (new_field->flags & BLOB_FLAG)
	    blob_count++;
	  ((Item_sum*) item)->args[i]= new Item_field(new_field);
	}
      }
    }
    else
    {
      Field *new_field=create_tmp_field(table,item,type,&copy_func,
					tmp_from_field, group != 0,1);
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
	blob_count++;
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      *(reg_field++) =new_field;
    }
  }
  field_count= (uint) (reg_field - table->field);

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
    null_count++;				// For delete link
  reclength+=(null_pack_length=(null_count+7)/8);
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
  for (i=0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint length;
    bzero((byte*) recinfo,sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
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
      recinfo->type=FIELD_SKIPP_ENDSPACE;
    else
      recinfo->type=FIELD_NORMAL;
  }

  param->copy_field_count=(uint) (copy - param->copy_field);
  param->recinfo=recinfo;
  store_record(table,2);			// Make empty default record

  table->max_rows=(((table->db_type == DB_TYPE_HEAP) ?
		    min(tmp_table_size, max_heap_table_size) : tmp_table_size)/
		   table->reclength);
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
	if (!(group->field=field->new_field(table)))
	  goto err; /* purecov: inspected */
	if (maybe_null)
	{
	  /*
	    To be able to group on NULL, we move the null bit to be
	     just before the column and extend the key to cover the null bit
	  */
	  *group_buff= 0;			// Init null byte
	  key_part_info->offset--;
	  key_part_info->length++;
	  group->field->move_field((char*) group_buff+1, (uchar*) group_buff,
				   1);
	}
	else
	  group->field->move_field((char*) group_buff);
	group_buff+= key_part_info->length;
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (distinct && !group)
  {
    /* Create an unique key or an unique constraint over all columns */
    keyinfo->key_parts=field_count+ test(null_count);
    if (distinct && allow_distinct_limit)
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
    keyinfo->flags=HA_NOSAME;
    keyinfo->key_length=(uint16) reclength;
    keyinfo->name=(char*) "tmp";
    if (null_count)
    {
      key_part_info->null_bit=0;
      key_part_info->offset=0;
      key_part_info->length=(null_count+7)/8;
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
    for (i=0,reg_field=table->field; i < field_count;
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
  if (!open_tmp_table(table))
    DBUG_RETURN(table);

 err:
  free_tmp_table(thd,table);			/* purecov: inspected */
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
  /* VOID(ha_lock(table,F_WRLCK)); */		/* Single thread table */
  (void) table->file->extra(HA_EXTRA_NO_READCHECK);	/* Not needed */
  (void) table->file->extra(HA_EXTRA_QUICK);		/* Faster */
  return(0);
}


static bool create_myisam_tmp_table(TABLE *table,TMP_TABLE_PARAM *param,
				    uint options)
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
      seg->flag=0;
      seg->language=MY_CHARSET_CURRENT;
      seg->length=keyinfo->key_part[i].length;
      seg->start=keyinfo->key_part[i].offset;
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
      if (using_unique_constraint &&
	  !(field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (uchar*) table->record[0]);
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
  if (entry->db_stat && entry->file)
  {
    (void) entry->file->close();
    delete entry->file;
  }
  if (!(test_flags & TEST_KEEP_TMP_TABLES) || entry->db_type == DB_TYPE_HEAP)
    (void) ha_delete_table(entry->db_type,entry->real_name);
  /* free blobs */
  for (Field **ptr=entry->field ; *ptr ; ptr++)
    delete *ptr;
  my_free((gptr) entry->record[0],MYF(0));
  free_io_cache(entry);
  my_free((gptr) entry,MYF(0));
  thd->proc_info=save_proc_info;

  DBUG_VOID_RETURN;
}

/*
* If a HEAP table gets full, create a MyISAM table and copy all rows to this
*/

bool create_myisam_from_heap(TABLE *table, TMP_TABLE_PARAM *param, int error,
			     bool ignore_last_dupp_key_error)
{
  TABLE new_table;
  const char *save_proc_info;
  THD *thd=current_thd;
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
			      thd->lex.options | thd->options))
    goto err2;
  if (open_tmp_table(&new_table))
    goto err1;
  table->file->index_end();
  table->file->rnd_init();
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

  thd->proc_info=save_proc_info;
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


/*****************************************************************************
**	Make a join of all tables and write it on socket or to table
*****************************************************************************/

static int
do_select(JOIN *join,List<Item> *fields,TABLE *table,Procedure *procedure)
{
  int error;
  JOIN_TAB *join_tab;
  int (*end_select)(JOIN *, struct st_join_table *,bool);
  DBUG_ENTER("do_select");

  join->procedure=procedure;
  /*
  ** Tell the client how many fields there are in a row
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
      DBUG_PRINT("info",("Using end_update"));
      if (table->keys)
      {
	end_select=end_update;
	table->file->index_init(0);
      }
      else
	end_select=end_unique_update;
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
  if (!table)					/* If sending data to client */
  {
    if (error < 0)
      join->result->send_error(0,NullS);	/* purecov: inspected */
    else
    {
      join_free(join);				// Unlock all cursors
      if (join->result->send_eof())
	error= -1;
    }
  }
  else if (error < 0)
    join->result->send_error(0,NullS); /* purecov: inspected */

  if (error >= 0)
  {
    DBUG_PRINT("info",("%ld records output",join->send_records));
  }
  if (table)
  {
    int old_error=error,tmp;
    if ((tmp=table->file->extra(HA_EXTRA_NO_CACHE)))
    {
      my_errno=tmp;
      error= -1;
    }
    if (table->file->index_end())
    {
      my_errno=tmp;
      error= -1;
    }
    if (error != old_error)
      table->file->print_error(my_errno,MYF(0));
  }
  DBUG_RETURN(error < 0);
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
  int  (*next_select)(JOIN *,struct st_join_table *,bool)=
    join_tab->next_select;

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
      if (!on_expr || on_expr->val_int())
      {
	found=1;
	if (not_exists_optimize)
	  break;			// Searching after not null columns
	if (!select_cond || select_cond->val_int())
	{
	  if ((error=(*next_select)(join,join_tab+1,0)) < 0)
	    return error;
	  if (not_used_in_distinct && found_records != join->found_records)
	    return 0;
	}
      }
    } while (!(error=info->read_record(info)));
    if (error > 0)				// Fatal error
      return -1;
  }
  else if (error > 0)
    return -1;

  if (!found && on_expr)
  {						// OUTER JOIN
    restore_record(join_tab->table,2);		// Make empty record
    mark_as_null_row(join_tab->table);		// For group by without error
    if (!select_cond || select_cond->val_int())
    {
      if ((error=(*next_select)(join,join_tab+1,0)) < 0)
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
** The different ways to read a record
** Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

static int
join_read_const_tables(JOIN *join)
{
  uint i;
  int error;
  DBUG_ENTER("join_read_const_tables");
  for (i=0 ; i < join->const_tables ; i++)
  {
    TABLE *form=join->table[i];
    form->null_row=0;
    form->status=STATUS_NO_RECORD;

    if (join->join_tab[i].type == JT_SYSTEM)
    {
      if ((error=join_read_system(join->join_tab+i)))
      {						// Info for DESCRIBE
	join->join_tab[i].info="const row not found";
	join->best_positions[i].records_read=0.0;
	if (!form->outer_join || error > 0)
	  DBUG_RETURN(error);
      }
    }
    else
    {
      if ((error=join_read_const(join->join_tab+i)))
      {
	join->join_tab[i].info="unique row not found";
	join->best_positions[i].records_read=0.0;
	if (!form->outer_join || error > 0)
	  DBUG_RETURN(error);
      }
    }
    if (join->join_tab[i].on_expr && !form->null_row)
    {
      if ((form->null_row= test(join->join_tab[i].on_expr->val_int() == 0)))
	empty_record(form);
    }
    if (!form->null_row)
      form->maybe_null=0;
  }
  DBUG_RETURN(0);
}


static int
join_read_system(JOIN_TAB *tab)
{
  TABLE *table= tab->table;
  int error;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    if ((error=table->file->rnd_first(table->record[0])))
    {
      if (error != HA_ERR_END_OF_FILE)
      {
	table->file->print_error(error,MYF(0));
	return 1;
      }
      table->null_row=1;			// This is ok.
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
      table->null_row=1;
      empty_record(table);
      if (error != HA_ERR_KEY_NOT_FOUND)
      {
	sql_print_error("read_const: Got error %d when reading table %s",
			error, table->path);
	table->file->print_error(error,MYF(0));
	return 1;
      }
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
join_read_next(READ_RECORD *info)
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
  return tab->select->test_quick_select(tab->keys,(table_map) 0,HA_POS_ERROR);
}


static int
join_init_read_record(JOIN_TAB *tab)
{
  if (tab->select && tab->select->quick)
    tab->select->quick->reset();
  init_read_record(&tab->read_record,current_thd, tab->table, tab->select,1,1);
  return (*tab->read_record.read_record)(&tab->read_record);
}

static int
join_init_read_first_with_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table=tab->table;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)))
  {
    table->key_read=1;
    table->file->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_init_read_next_with_key;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  tab->table->file->index_init(tab->index);
  error=tab->table->file->index_first(tab->table->record[0]);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      sql_print_error("read_first_with_key: Got error %d when reading table",error);
      table->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}

static int
join_init_read_next_with_key(READ_RECORD *info)
{
  int error=info->file->index_next(info->record);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      sql_print_error("read_next_with_key: Got error %d when reading table %s",
		      error, info->table->path);
      info->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}

static int
join_init_read_last_with_key(JOIN_TAB *tab)
{
  TABLE *table=tab->table;
  int error;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)))
  {
    table->key_read=1;
    table->file->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_init_read_prev_with_key;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  tab->table->file->index_init(tab->index);
  error=tab->table->file->index_last(tab->table->record[0]);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      sql_print_error("read_first_with_key: Got error %d when reading table",
		      error, table->path);
      table->file->print_error(error,MYF(0));
      return 1;
    }
    return -1;
  }
  return 0;
}

static int
join_init_read_prev_with_key(READ_RECORD *info)
{
  int error=info->file->index_prev(info->record);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
      sql_print_error("read_prev_with_key: Got error %d when reading table: %s",
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

#if 0
  if (cp_buffer_from_ref(&tab->ref))       // as ft-key doesn't use store_key's
    return -1;                             // see also FT_SELECT::init()
#endif
  table->file->ft_init();

  error=table->file->ft_read(table->record[0]);
  if (error)
  {
    if (error != HA_ERR_END_OF_FILE)
    {
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
** The different end of select functions
** These functions returns < 0 when end is reached, 0 on ok and > 0 if a
** fatal error (like table corruption) was detected
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
    if (join->procedure)
      error=join->procedure->send_row(*join->fields);
    else
      error=join->result->send_data(*join->fields);
    if (error)
      DBUG_RETURN(-1); /* purecov: inspected */
    if (++join->send_records >= join->thd->select_limit)
      DBUG_RETURN(-3);				// Abort nicely
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
	int error;
	if (join->procedure)
	{
	  if (join->having && join->having->val_int() == 0)
	    error= -1;				// Didn't satisfy having
	  else
	    error=join->procedure->send_row(*join->fields) ? 1 : 0;
	  if (end_of_records && join->procedure->end_of_records())
	    error= 1;				// Fatal error
	}
	else
	{
	  if (!join->first_record)
	    clear_tables(join);
	  if (join->having && join->having->val_int() == 0)
	    error= -1;				// Didn't satisfy having
	  else
	    error=join->result->send_data(*join->fields) ? 1 : 0;
	}
	if (error > 0)
	  DBUG_RETURN(-1);			/* purecov: inspected */
	if (end_of_records)
	  DBUG_RETURN(0);
	if (!error && ++join->send_records >= join->thd->select_limit)
	  DBUG_RETURN(-3);			/* Abort nicely */
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
    copy_funcs(join->tmp_table_param.funcs);

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
	  field->ptr[-1]= (byte) (field->is_null() ? 0 : 1);
	}
      }
    }
    if (!join->having || join->having->val_int())
    {
      join->found_records++;
      if ((error=table->file->write_row(table->record[0])))
      {
	if (error == HA_ERR_FOUND_DUPP_KEY ||
	    error == HA_ERR_FOUND_DUPP_UNIQUE)
	  goto end;
	if (create_myisam_from_heap(table, &join->tmp_table_param, error,1))
	  DBUG_RETURN(1);			// Not a table_is_full error
	table->uniques=0;			// To ensure rows are the same
	if (++join->send_records >= join->tmp_table_param.end_write_records)
	  DBUG_RETURN(-3);
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
    if (item->maybe_null)
      group->buff[0]=item->null_value ? 0: 1;	// Save reversed value
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
  copy_funcs(join->tmp_table_param.funcs);
  if ((error=table->file->write_row(table->record[0])))
  {
    if (create_myisam_from_heap(table, &join->tmp_table_param, error, 0))
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
  copy_funcs(join->tmp_table_param.funcs);

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
	  clear_tables(join);
	copy_sum_funcs(join->sum_funcs);
	if (!join->having || join->having->val_int())
	{
	  if ((error=table->file->write_row(table->record[0])))
	  {
	    if (create_myisam_from_heap(table, &join->tmp_table_param,
					error, 0))
	      DBUG_RETURN(1);			// Not a table_is_full error
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
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      copy_funcs(join->tmp_table_param.funcs);
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
** Remove calculation with tables that aren't yet read. Remove also tests
** against fields that are read through key.
** We can't remove tests that are made against columns which are stored
** in sorted order.
*****************************************************************************/

/* Return 1 if right_item is used removable reference key on left_item */

static bool test_if_ref(Item_field *left_item,Item *right_item)
{
  Field *field=left_item->field;
  if (!field->table->const_table)		// No need to change const test
  {
    Item *ref_item=part_of_refkey(field->table,field);
    if (ref_item && ref_item->eq(right_item))
    {
      if (right_item->type() == Item::FIELD_ITEM)
	return field->eq_def(((Item_field *) right_item)->field);
      if (right_item->const_item())
      {
	// We can remove binary fields and numerical fields except float,
	// as float comparison isn't 100 % secure
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
      return new_cond;
    }
  }

  /*
  ** Because the following test takes a while and it can be done
  ** table_count times, we mark each item that we have examined with the result
  ** of the test
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
	  !(key_part->key_part_flag & HA_PART_KEY))
	return table->reginfo.join_tab->ref.items[part];
  }
  return (Item*) 0;
}


/*****************************************************************************
** Test if one can use the key to resolve ORDER BY
** Returns: 1 if key is ok.
**	    0 if key can't be used
**	    -1 if reverse key can be used
*****************************************************************************/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint idx)
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


/*****************************************************************************
** If not selecting by given key, create a index how records should be read
** return: 0  ok
**	  -1 some fatal error
**	   1  no records
*****************************************************************************/

/* Return 1 if we don't have to do file sorting */

static bool
test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,ha_rows select_limit)
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
    usable_keys&=((Item_field*) (*tmp_order->item))->field->part_of_key;
  }

  ref_key= -1;
  if (tab->ref.key >= 0)			// Constant range in WHERE
    ref_key=tab->ref.key;
  else if (select && select->quick)		// Range found by opt_range
    ref_key=select->quick->index;

  if (ref_key >= 0)
  {
    /* Check if we get the rows in requested sorted order by using the key */
    if ((usable_keys & ((key_map) 1 << ref_key)) &&
	test_if_order_by_key(order,table,ref_key) == 1)
      DBUG_RETURN(1);			/* No need to sort */
  }
  else
  {
    /* check if we can use a key to resolve the group */
    /* Tables using JT_NEXT are handled here */
    uint nr;
    key_map keys=usable_keys;

    /*
      If not used with LIMIT, only use keys if the whole query can be
      resolved with a key;  This is because filesort() is usually faster than
      retrieving all rows through an index.
    */
    if (select_limit >= table->file->records)
      keys&= table->used_keys;

    for (nr=0; keys ; keys>>=1, nr++)
    {
      if (keys & 1)
      {
	int flag;
	if ((flag=test_if_order_by_key(order,table,nr)))
	{
	  tab->index=nr;
	  tab->read_first_record=  (flag > 0 ? join_init_read_first_with_key:
				    join_init_read_last_with_key);
	  tab->type=JT_NEXT;	// Read with index_first(), index_next()
	  DBUG_RETURN(1);
	}
      }
    }
  }
  DBUG_RETURN(0);				// Can't use index.
}

static int
create_sort_index(JOIN_TAB *tab,ORDER *order,ha_rows select_limit)
{
  SORT_FIELD *sortorder;
  uint length;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  DBUG_ENTER("create_sort_index");

  if (test_if_skip_sort_order(tab,order,select_limit))
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
      */
      if (!(select->quick=get_ft_or_quick_select_for_ref(table, tab)))
	goto err;
    }
  }
  table->found_records=filesort(&table,sortorder,length,
				select, 0L, select_limit);
  delete select;				// filesort did select
  tab->select=0;
  tab->select_cond=0;
  tab->type=JT_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  if (table->key_read)				// Restore if we used indexes
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  DBUG_RETURN(table->found_records == HA_POS_ERROR);
err:
  DBUG_RETURN(-1);
}


/*****************************************************************************
** Remove duplicates from tmp table
** This should be recoded to add a uniuqe index to the table and remove
** dupplicates
** Table is a locked single thread table
** fields is the number of fields to check (from the end)
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
remove_duplicates(JOIN *join, TABLE *entry,List<Item> &fields)
{
  int error;
  ulong reclength,offset;
  uint field_count;
  DBUG_ENTER("remove_duplicates");

  entry->reginfo.lock_type=TL_WRITE;
  entry->file->extra(HA_EXTRA_NO_READCHECK);

  /* Calculate how many saved fields there is in list */
  field_count=0;
  List_iterator<Item> it(fields);
  Item *item;
  while ((item=it++))
    if (item->tmp_table_field())
      field_count++;

  if (!field_count)
  {						// only const items
    join->thd->select_limit=1;			// Only send first row
    DBUG_RETURN(0);
  }
  Field **first_field=entry->field+entry->fields - field_count;
  offset=entry->field[entry->fields - field_count]->offset();
  reclength=entry->reclength-offset;

  free_io_cache(entry);				// Safety
  entry->file->info(HA_STATUS_VARIABLE);
  if (entry->db_type == DB_TYPE_HEAP ||
      (!entry->blob_fields &&
       ((ALIGN_SIZE(reclength) +sizeof(HASH_LINK)) * entry->file->records <
	sortbuff_size)))
    error=remove_dup_with_hash_index(join->thd, entry,
				     field_count, first_field,
				     reclength);
  else
    error=remove_dup_with_compare(join->thd, entry, first_field, offset);

  free_blobs(first_field);
  DBUG_RETURN(error);
}


static int remove_dup_with_compare(THD *thd, TABLE *table, Field **first_field,
				   ulong offset)
{
  handler *file=table->file;
  char *org_record,*new_record;
  int error;
  ulong reclength=table->reclength-offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record=(char*) table->record[0]+offset;
  new_record=(char*) table->record[1]+offset;

  file->rnd_init();
  error=file->rnd_next(table->record[0]);
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
      if ((error=file->rnd_next(table->record[0])))
      {
	if (error == HA_ERR_RECORD_DELETED)
	  continue;
	if (error == HA_ERR_END_OF_FILE)
	  break;
	goto err;
      }
      if (compare_record(table, first_field) == 0)
      {
	if ((error=file->delete_row(table->record[0])))
	  goto err;
      }
      else if (!found)
      {
	found=1;
	file->position(table->record[0]);	// Remember position
      }
    }
    if (!found)
      break;					// End of file
    /* Restart search on next row */
    error=file->restart_rnd_next(table->record[0],file->ref);
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
				      ulong key_length)
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
**	Fill join cache with packed records
**	Records are stored in tab->cache.buffer and last record in
**	last record is stored with pointers to blobs to support very big
**	records
******************************************************************************/

static int
join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count)
{
  reg1 uint i;
  uint length,blobs,size;
  CACHE_FIELD *copy,**blob_ptr;
  JOIN_CACHE  *cache;
  DBUG_ENTER("join_init_cache");

  cache= &tables[table_count].cache;
  cache->fields=blobs=0;

  for (i=0 ; i < table_count ; i++)
  {
    cache->fields+=tables[i].used_fields;
    blobs+=tables[i].used_blobs;
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
      copy->length=(tables[i].table->null_fields+7)/8;
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
  size=max(join_buff_size,cache->length);
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
  ** There is room in cache. Put record there
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
** Group and order functions
*****************************************************************************/

/*
** Find order/group item in requested columns and change the item to point at
** it. If item doesn't exists, add it first in the field list
** Return 0 if ok.
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
  if ((*order->item)->fix_fields(thd,tables) || thd->fatal_error)
    return 1;					// Wrong field
  all_fields.push_front(*order->item);		// Add new field to field list
  order->item=(Item**) all_fields.head_ref();
  return 0;
}


/*
** Change order to point at item in select list. If item isn't a number
** and doesn't exits in the select list, add it the the field list.
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

  if (thd->options & OPTION_ANSI_MODE)
  {
    Item *item;
    List_iterator<Item> li(fields);
    while ((item=li++))
      item->marker=0;			/* Marker that field is not used */
  }
  uint org_fields=all_fields.elements;

  thd->where="group statement";
  for ( ; order; order=order->next)
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
  if (thd->options & OPTION_ANSI_MODE)
  {
    /* Don't allow one to use fields that is not used in GROUP BY */
    Item *item;
    List_iterator<Item> li(fields);

    while ((item=li++))
    {
      if (item->type() != Item::SUM_FUNC_ITEM && !item->marker)
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
** Add fields with aren't used at start of field list. Return FALSE if ok
*/

static bool
setup_new_fields(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &all_fields, ORDER *new_field)
{
  Item	  **item;
  DBUG_ENTER("setup_new_fields");

  thd->set_query_id=1;				// Not really needed, but...
  thd->where=0;					// Don't give error
  for ( ; new_field ; new_field=new_field->next)
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
** Create a group by that consist of all non const fields. Try to use
** the fields in the order given by 'order' to allow one to optimize
** away 'order by'.
*/

static ORDER *
create_distinct_group(ORDER *order_list,List<Item> &fields)
{
  List_iterator<Item> li(fields);
  Item *item;
  ORDER *order,*group,**prev;

  while ((item=li++))
    item->marker=0;			/* Marker that field is not used */

  prev= &group;  group=0;
  for (order=order_list ; order; order=order->next)
  {
    if (order->in_field_list)
    {
      ORDER *ord=(ORDER*) sql_memdup(order,sizeof(ORDER));
      if (!ord)
	return 0;
      *prev=ord;
      prev= &ord->next;
      (*ord->item)->marker=1;
    }
  }

  li.rewind();
  while ((item=li++))
  {
    if (item->const_item() || item->with_sum_func)
      continue;
    if (!item->marker)
    {
      ORDER *ord=(ORDER*) sql_calloc(sizeof(ORDER));
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
** Update join with count of the different type of fields
*****************************************************************************/

void
count_field_types(TMP_TABLE_PARAM *param, List<Item> &fields)
{
  List_iterator<Item> li(fields);
  Item *field;

  param->field_count=param->sum_func_count=
    param->func_count=0;
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
      param->func_count++;
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
    if ((*a->item)->eq(*b->item))
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
    if (!(*a->item)->eq(*b->item))
      DBUG_RETURN(0);
    map|=a->item[0]->used_tables();
  }
  if (!map || (map & RAND_TABLE_BIT))
    DBUG_RETURN(0);

  for ( ; !(map & tables->table->map) ; tables=tables->next) ;
  if (map != tables->table->map)
    DBUG_RETURN(0);				// More than one table
  DBUG_PRINT("exit",("sort by table: %d",tables->table->tablenr));
  DBUG_RETURN(tables->table);
}


	/* calc how big buffer we need for comparing group entries */

static void
calc_group_buffer(JOIN *join,ORDER *group)
{
  uint key_length=0,parts=0;
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
      key_length++;
  }
  join->tmp_table_param.group_length=key_length;
  join->tmp_table_param.group_parts=parts;
}


/*
** Get a list of buffers for saveing last group
** Groups are saved in reverse order for easyer check loop
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
** Setup copy_fields to save fields at start of new group
** Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
** Change old item_field to use a new field with points at saved fieldvalue
** This function is only called before use of send_fields
*/

bool
setup_copy_fields(TMP_TABLE_PARAM *param,List<Item> &fields)
{
  Item *pos;
  List_iterator<Item> li(fields);
  Copy_field *copy;
  DBUG_ENTER("setup_copy_fields");

  if (!(copy=param->copy_field= new Copy_field[param->field_count]))
    goto err;

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
	if (param->copy_funcs.push_back(pos))
	  goto err;
	continue;
      }

      /* set up save buffer and change result_field to point at saved value */
      Field *field= item->field;
      item->result_field=field->new_field(field->table);
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
  param->copy_field_count= (uint) (copy - param->copy_field);
  DBUG_RETURN(0);

 err:
  delete [] param->copy_field;
  param->copy_field=0;
  DBUG_RETURN(TRUE);
}


/*
** Copy fields and null values between two tables
*/

void
copy_fields(TMP_TABLE_PARAM *param)
{
  Copy_field *ptr=param->copy_field;
  Copy_field *end=ptr+param->copy_field_count;

  for ( ; ptr != end; ptr++)
    (*ptr->do_copy)(ptr);

  List_iterator<Item> it(param->copy_funcs);
  Item_copy_string *item;
  while ((item = (Item_copy_string*) it++))
  {
    item->copy();
  }
}


/*****************************************************************************
** Make an array of pointer to sum_functions to speed up sum_func calculation
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
** Change all funcs and sum_funcs to fields in tmp table
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
** Change all sum_func refs to fields to point at fields in tmp table
** Change all funcs to be fields in tmp table
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
** code for calculating functions
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
    func->update_field(0);
}


	/* Copy result of sum functions to record in tmp_table */

static void
copy_sum_funcs(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func = *func_ptr) ; func_ptr++)
    (void) func->save_in_field(func->result_field);
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
copy_funcs(Item_result_field **func_ptr)
{
  Item_result_field *func;
  for (; (func = *func_ptr) ; func_ptr++)
    (void) func->save_in_field(func->result_field);
  return;
}


/*****************************************************************************
** Create a condition for a const reference and add this to the
** currenct select for the table
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
** Send a description about what how the select will be done to stdout
****************************************************************************/

static void select_describe(JOIN *join, bool need_tmp_table, bool need_order,
			    bool distinct)
{
  List<Item> field_list;
  Item *item;
  THD *thd=join->thd;
  DBUG_ENTER("select_describe");

  /* Don't log this into the slow query log */
  join->thd->lex.options&= ~(QUERY_NO_INDEX_USED | QUERY_NO_GOOD_INDEX_USED);
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
  if (send_fields(thd,field_list,1))
    return; /* purecov: inspected */

  char buff[512],*buff_ptr;
  String tmp(buff,sizeof(buff)),*packet= &thd->packet;
  table_map used_tables=0;
  for (uint i=0 ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;

    if (tab->type == JT_ALL && tab->select && tab->select->quick)
      tab->type= JT_RANGE;
    packet->length(0);
    net_store_data(packet,table->table_name);
    net_store_data(packet,join_type_str[tab->type]);
    tmp.length(0);
    key_map bits;
    uint j;
    for (j=0,bits=tab->keys ; bits ; j++,bits>>=1)
    {
      if (bits & 1)
      {
	if (tmp.length())
	  tmp.append(',');
	tmp.append(table->key_info[j].name);
      }
    }
    if (tmp.length())
      net_store_data(packet,tmp.ptr(),tmp.length());
    else
      net_store_null(packet);
    if (tab->ref.key_parts)
    {
      net_store_data(packet,table->key_info[tab->ref.key].name);
      net_store_data(packet,(uint32) tab->ref.key_length);
      tmp.length(0);
      for (store_key **ref=tab->ref.key_copy ; *ref ; ref++)
      {
	if (tmp.length())
	  tmp.append(',');
	tmp.append((*ref)->name());
      }
      net_store_data(packet,tmp.ptr(),tmp.length());
    }
    else if (tab->type == JT_NEXT)
    {
      net_store_data(packet,table->key_info[tab->index].name);
      net_store_data(packet,(uint32) table->key_info[tab->index].key_length);
      net_store_null(packet);
    }
    else if (tab->select && tab->select->quick)
    {
      net_store_data(packet,table->key_info[tab->select->quick->index].name);;
      net_store_data(packet,(uint32) tab->select->quick->max_used_key_length);
      net_store_null(packet);
    }
    else
    {
      net_store_null(packet);
      net_store_null(packet);
      net_store_null(packet);
    }
    sprintf(buff,"%.0f",join->best_positions[i].records_read);
    net_store_data(packet,buff);
    my_bool key_read=table->key_read;
    if (tab->type == JT_NEXT &&
	((table->used_keys & ((key_map) 1 << tab->index))))
      key_read=1;

    buff_ptr=buff;
    if (tab->info)
      net_store_data(packet,tab->info);
    else if (tab->select)
    {
      if (tab->use_quick == 2)
      {
	sprintf(buff_ptr,"range checked for each record (index map: %u)",
		tab->keys);
	buff_ptr=strend(buff_ptr);
      }
      else
	buff_ptr=strmov(buff_ptr,"where used");
    }
    if (key_read)
    {
      if (buff != buff_ptr)
      {
	buff_ptr[0]=';' ; buff_ptr[1]=' '; buff_ptr+=2;
      }
      buff_ptr=strmov(buff_ptr,"Using index");
    }
    if (table->reginfo.not_exists_optimize)
    {
      if (buff != buff_ptr)
      {
	buff_ptr[0]=';' ; buff_ptr[1]=' '; buff_ptr+=2;
      }
      buff_ptr=strmov(buff_ptr,"Not exists");
    }
    if (need_tmp_table)
    {
      need_tmp_table=0;
      if (buff != buff_ptr)
      {
	buff_ptr[0]=';' ; buff_ptr[1]=' '; buff_ptr+=2;
      }
      buff_ptr=strmov(buff_ptr,"Using temporary");
    }
    if (need_order)
    {
      need_order=0;
      if (buff != buff_ptr)
      {
	buff_ptr[0]=';' ; buff_ptr[1]=' '; buff_ptr+=2;
      }
      buff_ptr=strmov(buff_ptr,"Using filesort");
    }
    if (distinct & test_all_bits(used_tables,thd->used_tables))
    {
      if (buff != buff_ptr)
      {
	buff_ptr[0]=';' ; buff_ptr[1]=' '; buff_ptr+=2;
      }
      buff_ptr=strmov(buff_ptr,"Distinct");
    }
    net_store_data(packet,buff,(uint) (buff_ptr - buff));
    if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
      DBUG_VOID_RETURN;				/* purecov: inspected */

    // For next iteration
    used_tables|=table->map;
  }
  send_eof(&thd->net);
  DBUG_VOID_RETURN;
}


static void describe_info(THD *thd, const char *info)
{
  List<Item> field_list;
  String *packet= &thd->packet;

  /* Don't log this into the slow query log */
  thd->lex.options&= ~(QUERY_NO_INDEX_USED | QUERY_NO_GOOD_INDEX_USED);
  field_list.push_back(new Item_empty_string("Comment",80));
  if (send_fields(thd,field_list,1))
    return; /* purecov: inspected */
  packet->length(0);
  net_store_data(packet,info);
  if (!my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
    send_eof(&thd->net);
}
