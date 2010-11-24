/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  mysql_select and join optimization


  @defgroup Query_Optimizer  Query Optimizer
  @{
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
#include "unireg.h"
#include "sql_select.h"
#include "sql_cache.h"                          // query_cache_*
#include "sql_table.h"                          // primary_key_name
#include "probes_mysql.h"
#include "key.h"                 // key_copy, key_cmp, key_cmp_if_same
#include "lock.h"                // mysql_unlock_some_tables,
                                 // mysql_unlock_read_tables
#include "sql_show.h"            // append_identifier
#include "sql_base.h"            // setup_wild, setup_fields, fill_record
#include "sql_parse.h"                          // check_stack_overrun
#include "sql_partition.h"       // make_used_partitions_str
#include "sql_acl.h"             // *_ACL
#include "sql_test.h"            // print_where, print_keyuse_array,
                                 // print_sjm, print_plan, TEST_join
#include "records.h"             // init_read_record, end_read_record
#include "filesort.h"            // filesort_free_buffers
#include "sql_union.h"           // mysql_union
#include <m_ctype.h>
#include <my_bit.h>
#include <hash.h>
#include <ft_global.h>

#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))

const char *join_type_str[]={ "UNKNOWN","system","const","eq_ref","ref",
			      "MAYBE_REF","ALL","range","index","fulltext",
			      "ref_or_null","unique_subquery","index_subquery",
                              "index_merge"
};

struct st_sargable_param;

static void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array);
static bool make_join_statistics(JOIN *join, TABLE_LIST *leaves, Item *conds,
				 DYNAMIC_ARRAY *keyuse);
static bool optimize_semijoin_nests(JOIN *join, table_map all_table_map);
static bool update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,
                                JOIN_TAB *join_tab,
                                uint tables, Item *conds,
                                COND_EQUAL *cond_equal,
                                table_map table_map, SELECT_LEX *select_lex,
                                st_sargable_param **sargables);
static int sort_keyuse(Key_use *a, Key_use *b);
static void set_position(JOIN *join, uint index, JOIN_TAB *table, Key_use *key);
static bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
			       table_map used_tables);
static bool choose_plan(JOIN *join, table_map join_tables);
static void best_access_path(JOIN  *join, JOIN_TAB *s, 
                             table_map remaining_tables, uint idx, 
                             bool disable_jbuf, double record_count,
                             POSITION *pos, POSITION *loose_scan_pos);
static void optimize_straight_join(JOIN *join, table_map join_tables);
static bool greedy_search(JOIN *join, table_map remaining_tables,
                             uint depth, uint prune_level);
static bool best_extension_by_limited_search(JOIN *join,
                                             table_map remaining_tables,
                                             uint idx, double record_count,
                                             double read_time, uint depth,
                                             uint prune_level);
static uint determine_search_depth(JOIN* join);
C_MODE_START
static int join_tab_cmp(const void *dummy, const void* ptr1, const void* ptr2);
static int join_tab_cmp_straight(const void *dummy, const void* ptr1, const void* ptr2);
static int join_tab_cmp_embedded_first(const void *emb, const void* ptr1, const void *ptr2);
C_MODE_END
static uint cache_record_length(JOIN *join,uint index);
static double prev_record_reads(JOIN *join, uint idx, table_map found_ref);
static bool get_best_combination(JOIN *join);
static store_key *get_store_key(THD *thd,
				Key_use *keyuse, table_map used_tables,
				KEY_PART_INFO *key_part, uchar *key_buff,
				uint maybe_null);
static void make_outerjoin_info(JOIN *join);
static Item*
make_cond_after_sjm(Item * root_cond,Item *cond, table_map tables, table_map sjm_tables);
static bool make_join_select(JOIN *join, Item *item);
static bool make_join_readinfo(JOIN *join, ulonglong options, uint no_jbuf_after);
static bool only_eq_ref_tables(JOIN *join, ORDER *order, table_map tables);
static void update_depend_map(JOIN *join);
static void update_depend_map(JOIN *join, ORDER *order);
static ORDER *remove_const(JOIN *join,ORDER *first_order,Item *cond,
			   bool change_list, bool *simple_order);
static int return_zero_rows(JOIN *join, select_result *res,TABLE_LIST *tables,
                            List<Item> &fields, bool send_row,
                            ulonglong select_options, const char *info,
                            Item *having);
static Item *build_equal_items(THD *thd, Item *cond,
                               COND_EQUAL *inherited,
                               List<TABLE_LIST> *join_list,
                               COND_EQUAL **cond_equal_ref);
static Item* substitute_for_best_equal_field(Item *cond,
                                             COND_EQUAL *cond_equal,
                                             void *table_join_idx);
static Item *simplify_joins(JOIN *join, List<TABLE_LIST> *join_list,
                            Item *conds, bool top, bool in_sj);
static bool check_interleaving_with_nj(JOIN_TAB *next);
static void reset_nj_counters(List<TABLE_LIST> *join_list);
static uint build_bitmap_for_nested_joins(List<TABLE_LIST> *join_list,
                                          uint first_unused);

static 
void advance_sj_state(JOIN *join, const table_map remaining_tables, 
                      const JOIN_TAB *new_join_tab, uint idx, 
                      double *current_record_count, double *current_read_time,
                      POSITION *loose_scan_pos);
static void backout_nj_sj_state(const table_map remaining_tables,
                                const JOIN_TAB *tab);

static Item *optimize_cond(JOIN *join, Item *conds,
                           List<TABLE_LIST> *join_list,
			   bool build_equalities,
                           Item::cond_result *cond_value);
bool const_expression_in_where(Item *conds,Item *item, Item **comp_item);
static bool open_tmp_table(TABLE *table);
static bool create_myisam_tmp_table(TABLE *table, KEY *keyinfo,
                                    MI_COLUMNDEF *start_recinfo,
                                    MI_COLUMNDEF **recinfo,
				    ulonglong options,
                                    my_bool big_tables);
static int do_select(JOIN *join,List<Item> *fields,TABLE *tmp_table,
		     Procedure *proc);

static enum_nested_loop_state
evaluate_join_record(JOIN *join, JOIN_TAB *join_tab,
                     int error);
static enum_nested_loop_state
evaluate_null_complemented_join_record(JOIN *join, JOIN_TAB *join_tab);
static enum_nested_loop_state
end_send(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_write(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static enum_nested_loop_state
end_unique_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);

static int join_read_const_table(JOIN_TAB *tab, POSITION *pos);
static int join_read_system(JOIN_TAB *tab);
static int join_read_const(JOIN_TAB *tab);
static int join_read_key(JOIN_TAB *tab);
static int join_read_key2(JOIN_TAB *tab, TABLE *table, TABLE_REF *table_ref);
static void join_read_key_unlock_row(st_join_table *tab);
static int join_read_always_key(JOIN_TAB *tab);
static int join_read_last_key(JOIN_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_init_quick_read_record(JOIN_TAB *tab);
static int test_if_quick_select(JOIN_TAB *tab);
static bool test_if_use_dynamic_range_scan(JOIN_TAB *join_tab);
static int join_read_first(JOIN_TAB *tab);
static int join_read_next_same(READ_RECORD *info);
static int join_read_last(JOIN_TAB *tab);
static int join_read_prev_same(READ_RECORD *info);
static int join_read_prev(READ_RECORD *info);
static int join_ft_read_first(JOIN_TAB *tab);
static int join_ft_read_next(READ_RECORD *info);
int join_read_always_key_or_null(JOIN_TAB *tab);
int join_read_next_same_or_null(READ_RECORD *info);
static Item *make_cond_for_table(Item *cond,table_map table,
				 table_map used_table,
                                 bool exclude_expensive_cond);
static Item *make_cond_for_table_from_pred(Item *root_cond, Item *cond,
                                           table_map tables,
                                           table_map used_table,
                                           bool exclude_expensive_cond);
static Item* part_of_refkey(TABLE *form,Field *field);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);
static bool test_if_cheaper_ordering(const JOIN_TAB *tab,
                                     ORDER *order, TABLE *table,
                                     key_map usable_keys, int key,
                                     ha_rows select_limit,
                                     int *new_key, int *new_key_direction,
                                     ha_rows *new_select_limit,
                                     uint *new_used_key_parts= NULL,
                                     uint *saved_best_key_parts= NULL);
static bool test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,
				    ha_rows select_limit, bool no_changes,
                                    const key_map *map);
static bool list_contains_unique_index(TABLE *table,
                          bool (*find_func) (Field *, void *), void *data);
static bool find_field_in_item_list (Field *field, void *data);
static bool find_field_in_order_list (Field *field, void *data);
static int create_sort_index(THD *thd, JOIN *join, ORDER *order,
			     ha_rows filesort_limit, ha_rows select_limit,
                             bool is_order_by);
static int remove_duplicates(JOIN *join,TABLE *entry,List<Item> &fields,
			     Item *having);
static int remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
				   ulong offset,Item *having);
static int remove_dup_with_hash_index(THD *thd,TABLE *table,
				      uint field_count, Field **first_field,

				      ulong key_length,Item *having);
static bool cmp_buffer_with_ref(THD *thd, TABLE *table, TABLE_REF *tab_ref);
static bool setup_new_fields(THD *thd, List<Item> &fields,
			     List<Item> &all_fields, ORDER *new_order);
static ORDER *create_distinct_group(THD *thd, Item **ref_pointer_array,
                                    ORDER *order, List<Item> &fields,
                                    List<Item> &all_fields,
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
static bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct);
static bool init_sum_functions(Item_sum **func, Item_sum **end);
static bool update_sum_func(Item_sum **func);
void select_describe(JOIN *join, bool need_tmp_table,bool need_order,
			    bool distinct, const char *message=NullS);
static Item *remove_additional_cond(Item* conds);
static void add_group_and_distinct_keys(JOIN *join, JOIN_TAB *join_tab);
static bool replace_subcondition(JOIN *join, Item **tree, 
                                 Item *old_cond, Item *new_cond,
                                 bool do_fix_fields);
static bool test_if_ref(Item *root_cond, 
                        Item_field *left_item,Item *right_item);

void get_partial_join_cost(JOIN *join, uint idx, double *read_time_arg,
                           double *record_count_arg);
static uint make_join_orderinfo(JOIN *join);
static int
join_read_record_no_init(JOIN_TAB *tab);
static
bool subquery_types_allow_materialization(Item_in_subselect *predicate);
static 
bool semijoin_types_allow_materialization(TABLE_LIST *sj_nest);
static
bool types_allow_materialization(Item *outer, Item *inner);
static
bool resolve_subquery(THD *thd, JOIN *join);
int do_sj_reset(SJ_TMP_TABLE *sj_tbl);
TABLE *create_duplicate_weedout_tmp_table(THD *thd, uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl);
Item_equal *find_item_equal(COND_EQUAL *cond_equal, Field *field,
                            bool *inherited_fl);


/**
  This handles SELECT with and without UNION
*/

bool handle_select(THD *thd, LEX *lex, select_result *result,
                   ulong setup_tables_done_option)
{
  bool res;
  register SELECT_LEX *select_lex = &lex->select_lex;
  DBUG_ENTER("handle_select");
  MYSQL_SELECT_START(thd->query());

  if (select_lex->master_unit()->is_union() || 
      select_lex->master_unit()->fake_select_lex)
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
		      select_lex->table_list.first,
		      select_lex->with_wild, select_lex->item_list,
		      select_lex->where,
		      select_lex->order_list.elements +
		      select_lex->group_list.elements,
		      select_lex->order_list.first,
		      select_lex->group_list.first,
		      select_lex->having,
		      lex->proc_list.first,
		      select_lex->options | thd->variables.option_bits |
                      setup_tables_done_option,
		      result, unit, select_lex);
  }
  DBUG_PRINT("info",("res: %d  report_error: %d", res,
		     thd->is_error()));
  res|= thd->is_error();
  if (unlikely(res))
    result->abort_result_set();

  MYSQL_SELECT_DONE((int) res, (ulong) thd->limit_found_rows);
  DBUG_RETURN(res);
}


/*
  Fix fields referenced from inner selects.

  SYNOPSIS
    fix_inner_refs()
    thd               Thread handle
    all_fields        List of all fields used in select
    select            Current select
    ref_pointer_array Array of references to Items used in current select
    group_list        GROUP BY list (is NULL by default)

  DESCRIPTION
    The function serves 3 purposes - adds fields referenced from inner
    selects to the current select list, resolves which class to use
    to access referenced item (Item_ref of Item_direct_ref) and fixes
    references (Item_ref objects) to these fields.

    If a field isn't already in the select list and the ref_pointer_array
    is provided then it is added to the all_fields list and the pointer to
    it is saved in the ref_pointer_array.

    The class to access the outer field is determined by the following rules:
    1. If the outer field isn't used under an aggregate function
      then the Item_ref class should be used.
    2. If the outer field is used under an aggregate function and this
      function is aggregated in the select where the outer field was
      resolved or in some more inner select then the Item_direct_ref
      class should be used.
      Also it should be used if we are grouping by a subquery containing
      the outer field.
    The resolution is done here and not at the fix_fields() stage as
    it can be done only after sum functions are fixed and pulled up to
    selects where they are have to be aggregated.
    When the class is chosen it substitutes the original field in the
    Item_outer_ref object.

    After this we proceed with fixing references (Item_outer_ref objects) to
    this field from inner subqueries.

  RETURN
    TRUE  an error occured
    FALSE ok
*/

bool
fix_inner_refs(THD *thd, List<Item> &all_fields, SELECT_LEX *select,
                 Item **ref_pointer_array, ORDER *group_list)
{
  Item_outer_ref *ref;
  bool res= FALSE;
  bool direct_ref= FALSE;

  List_iterator<Item_outer_ref> ref_it(select->inner_refs_list);
  while ((ref= ref_it++))
  {
    Item *item= ref->outer_ref;
    Item **item_ref= ref->ref;
    Item_ref *new_ref;
    /*
      TODO: this field item already might be present in the select list.
      In this case instead of adding new field item we could use an
      existing one. The change will lead to less operations for copying fields,
      smaller temporary tables and less data passed through filesort.
    */
    if (ref_pointer_array && !ref->found_in_select_list)
    {
      int el= all_fields.elements;
      ref_pointer_array[el]= item;
      /* Add the field item to the select list of the current select. */
      all_fields.push_front(item);
      /*
        If it's needed reset each Item_ref item that refers this field with
        a new reference taken from ref_pointer_array.
      */
      item_ref= ref_pointer_array + el;
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
    thd->used_tables|= item->used_tables();
  }
  return res;
}

#define MAGIC_IN_WHERE_TOP_LEVEL 10
/**
  Function to setup clauses without sum functions.
*/
inline int setup_without_group(THD *thd, Item **ref_pointer_array,
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
    Need to save the value, so we can turn off only the new NON_AGG_FIELD
    additions coming from the WHERE
  */
  uint8 saved_flag= thd->lex->current_select->full_group_by_flag;
  DBUG_ENTER("setup_without_group");

  thd->lex->allow_sum_func&= ~(1 << thd->lex->current_select->nest_level);
  res= setup_conds(thd, tables, leaves, conds);

  /* it's not wrong to have non-aggregated columns in a WHERE */
  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
    thd->lex->current_select->full_group_by_flag= saved_flag |
      (thd->lex->current_select->full_group_by_flag & ~NON_AGG_FIELD_USED);

  thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;
  res= res || setup_order(thd, ref_pointer_array, tables, fields, all_fields,
                          order);
  thd->lex->allow_sum_func&= ~(1 << thd->lex->current_select->nest_level);
  res= res || setup_group(thd, ref_pointer_array, tables, fields, all_fields,
                          group, hidden_group_fields);
  thd->lex->allow_sum_func= save_allow_sum_func;
  DBUG_RETURN(res);
}

/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  mysql_select assumes that all tables are already opened
*****************************************************************************/

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
JOIN::prepare(Item ***rref_pointer_array,
	      TABLE_LIST *tables_init,
	      uint wild_num, Item *conds_init, uint og_num,
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
  union_part= unit_arg->is_union();

  thd->lex->current_select->is_item_list_lookup= 1;
  /*
    If we have already executed SELECT, then it have not sense to prevent
    its table from update (see unique_table())
  */
  if (thd->derived_tables_processing)
    select_lex->exclude_from_table_unique_test= TRUE;

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
    tables++;

  if (setup_wild(thd, tables_list, fields_list, &all_fields, wild_num) ||
      select_lex->setup_ref_array(thd, og_num) ||
      setup_fields(thd, (*rref_pointer_array), fields_list, MARK_COLUMNS_READ,
		   &all_fields, 1) ||
      setup_without_group(thd, (*rref_pointer_array), tables_list,
			  select_lex->leaf_tables, fields_list,
			  all_fields, &conds, order, group_list,
			  &hidden_group_fields))
    DBUG_RETURN(-1);				/* purecov: inspected */

  ref_pointer_array= *rref_pointer_array;
  
  if (having)
  {
    nesting_map save_allow_sum_func= thd->lex->allow_sum_func;
    thd->where="having clause";
    thd->lex->allow_sum_func|= 1 << select_lex_arg->nest_level;
    select_lex->having_fix_field= 1;
    bool having_fix_rc= (!having->fixed &&
			 (having->fix_fields(thd, &having) ||
			  having->check_cols(1)));
    select_lex->having_fix_field= 0;
    if (having_fix_rc || thd->is_error())
      DBUG_RETURN(-1);				/* purecov: inspected */
    thd->lex->allow_sum_func= save_allow_sum_func;
  }
  
  if (select_lex->master_unit()->item &&    // This is a subquery
      !thd->lex->view_prepare_mode &&       // Not normalizing a view
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
        item->split_sum_func(thd, ref_pointer_array, all_fields);
    }
    if (!real_order)
      order= NULL;
  }

  if (having && having->with_sum_func)
    having->split_sum_func2(thd, ref_pointer_array, all_fields,
                            &having, TRUE);
  if (select_lex->inner_sum_func_list)
  {
    Item_sum *end=select_lex->inner_sum_func_list;
    Item_sum *item_sum= end;  
    do
    { 
      item_sum= item_sum->next;
      item_sum->split_sum_func2(thd, ref_pointer_array,
                                all_fields, item_sum->ref_by, FALSE);
    } while (item_sum != end);
  }

  if (select_lex->inner_refs_list.elements &&
      fix_inner_refs(thd, all_fields, select_lex, ref_pointer_array,
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
        ref_pointer_array[el]= field;
        all_fields.push_front(field);
        ord->item= ref_pointer_array + el;
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
      select_lex->full_group_by_flag == (NON_AGG_FIELD_USED | SUM_FUNC_USED))
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
    if (order && (procedure->flags & PROC_NO_SORT))
    {						/* purecov: inspected */
      my_message(ER_ORDER_WITH_PROC, ER(ER_ORDER_WITH_PROC),
                 MYF(0));                       /* purecov: inspected */
      goto err;					/* purecov: inspected */
    }
    if (thd->lex->derived_tables)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", 
               thd->lex->derived_tables & DERIVED_VIEW ?
               "view" : "subquery"); 
      goto err;
    }
    if (thd->lex->sql_command != SQLCOM_SELECT)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", "non-SELECT");
      goto err;
    }
  }

  if (!procedure && result && result->prepare(fields_list, unit_arg))
    goto err;					/* purecov: inspected */

  /* Init join struct */
  count_field_types(select_lex, &tmp_table_param, all_fields, 0);
  ref_pointer_array_size= all_fields.elements*sizeof(Item*);
  this->group= group_list != 0;
  unit= unit_arg;

  if (tmp_table_param.sum_func_count && !group_list)
    implicit_grouping= TRUE;

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

  DBUG_RETURN(0); // All OK

err:
  delete procedure;				/* purecov: inspected */
  procedure= 0;
  DBUG_RETURN(-1);				/* purecov: inspected */
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

static
bool resolve_subquery(THD *thd, JOIN *join)

{
  DBUG_ENTER("resolve_subquery");

  SELECT_LEX *select_lex= join->select_lex;

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

  /* in_exists_predicate is non-NULL for IN, =ANY and EXISTS predicates */
  Item_exists_subselect *in_exists_predicate= 
    (subq_predicate->substype() == Item_subselect::IN_SUBS ||
     subq_predicate->substype() == Item_subselect::EXISTS_SUBS) ?
        (Item_exists_subselect*)subq_predicate :
        NULL;

  if (subq_predicate->substype() == Item_subselect::IN_SUBS)
  {
    Item_in_subselect *in_predicate= (Item_in_subselect *)subq_predicate;
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

    SELECT_LEX *current= thd->lex->current_select;
    thd->lex->current_select= current->outer_select();
    char const *save_where= thd->where;
    thd->where= "IN/ALL/ANY subquery";
        
    bool result= !in_predicate->left_expr->fixed &&
                  in_predicate->left_expr->fix_fields(thd,
                                                     &in_predicate->left_expr);
    thd->lex->current_select= current;
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
      3. Subquery does not have GROUP BY or ORDER BY
      4. Subquery does not use aggregate functions or HAVING
      5. Subquery predicate is at the AND-top-level of ON/WHERE clause
      6. We are not in a subquery of a single table UPDATE/DELETE that 
           doesn't have a JOIN (TODO: We should handle this at some
           point by switching to multi-table UPDATE/DELETE)
      7. We're not in a confluent table-less subquery, like "SELECT 1".
      8. No execution method was already chosen (by a prepared statement)
      9. Parent select is not a confluent table-less select
      10. Neither parent nor child select have STRAIGHT_JOIN option.

      Please note that subq_predicate and in_exists_predicate points to the
      same object here, but in_exists_predicate is NULL if the predicate
      is not IN or EXISTS. Therefore both pointers are needed in the same
      statement.
  */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SEMIJOIN) &&
      subq_predicate->substype() == Item_subselect::IN_SUBS &&          // 1
      !select_lex->is_part_of_union() &&                                // 2
      !select_lex->group_list.elements && !join->order &&               // 3
      !join->having && !select_lex->with_sum_func &&                    // 4
      thd->thd_marker.emb_on_expr_nest &&                               // 5
      select_lex->outer_select()->join &&                               // 6
      select_lex->master_unit()->first_select()->leaf_tables &&         // 7
      in_exists_predicate->exec_method == 
                           Item_exists_subselect::EXEC_UNSPECIFIED &&   // 8
      select_lex->outer_select()->leaf_tables &&                        // 9
      !((join->select_options |
         select_lex->outer_select()->join->select_options)
        & SELECT_STRAIGHT_JOIN))                                        // 10
  {
    DBUG_PRINT("info", ("Subquery is semi-join conversion candidate"));

    /* Notify in the subquery predicate where it belongs in the query graph */
    in_exists_predicate->embedding_join_nest= thd->thd_marker.emb_on_expr_nest;

    /* Register the subquery for further processing in flatten_subqueries() */
    select_lex->outer_select()->join->
      sj_subselects.append(thd->mem_root, in_exists_predicate);
  }
  else
  {
    DBUG_PRINT("info", ("Subquery can't be converted to semi-join"));
    /*
      Check if the subquery predicate can be executed via materialization.
      The required conditions are:
      1. Subquery predicate is an IN/=ANY subquery predicate
      2. Subquery is a single SELECT (not a UNION)
      3. Subquery is not a table-less query. In this case there is no
         point in materializing.
        3A The upper query is not a confluent SELECT ... FROM DUAL. We
           can't do materialization for SELECT .. FROM DUAL because it
           does not call setup_subquery_materialization(). We could make 
           SELECT ... FROM DUAL call that function but that doesn't seem
           to be the case that is worth handling.
      4. Subquery predicate is a top-level predicate
         (this implies it is not negated)
         TODO: this is a limitation that should be lifted once we
         implement correct NULL semantics (WL#3830)
      5. Subquery is non-correlated
         TODO:
         This is an overly restrictive condition. It can be extended to:
         (Subquery is non-correlated ||
          Subquery is correlated to any query outer to IN predicate ||
          (Subquery is correlated to the immediate outer query &&
           Subquery !contains {GROUP BY, ORDER BY [LIMIT],
           aggregate functions}) && subquery predicate is not under "NOT IN"))
      6. No execution method was already chosen (by a prepared statement).
      7. Involved expression types allow materialization (temporary only)

      (*) The subquery must be part of a SELECT statement. The current
           condition also excludes multi-table update statements.

      We have to determine whether we will perform subquery materialization
      before calling the IN=>EXISTS transformation, so that we know whether to
      perform the whole transformation or only that part of it which wraps
      Item_in_subselect in an Item_in_optimizer.
    */
    Item_in_subselect *in_predicate= (Item_in_subselect *)subq_predicate;

    if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MATERIALIZATION)  && 
        subq_predicate->substype() == Item_subselect::IN_SUBS &&        // 1
        !select_lex->is_part_of_union() &&                              // 2
        select_lex->master_unit()->first_select()->leaf_tables &&       // 3
        thd->lex->sql_command == SQLCOM_SELECT &&                       // *
        select_lex->outer_select()->leaf_tables &&                      // 3A
        in_predicate->is_top_level_item() &&                            // 4
        !in_predicate->is_correlated &&                                 // 5
        in_predicate->exec_method ==
                      Item_exists_subselect::EXEC_UNSPECIFIED &&        // 6
        subquery_types_allow_materialization(in_predicate))             // 7
      in_predicate->exec_method= Item_exists_subselect::EXEC_MATERIALIZATION;

    if (subq_predicate->select_transformer(join) == Item_subselect::RES_ERROR)
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}

/**
  @brief Check if subquery predicate's compared types allow materialization.

  @param predicate subquery predicate

  @return TRUE if subquery types allow materialization, FALSE otherwise.

  @details
    This is a temporary fix for BUG#36752.
    See bug report for description of restrictions we need to put on the
    compared expressions.
*/

static 
bool subquery_types_allow_materialization(Item_in_subselect *predicate)
{
  DBUG_ENTER("subquery_types_allow_materialization");

  DBUG_ASSERT(predicate->left_expr->fixed);
  DBUG_ASSERT(predicate->left_expr->cols() ==
              predicate->unit->first_select()->item_list.elements);

  List_iterator<Item> it(predicate->unit->first_select()->item_list);
  uint elements= predicate->unit->first_select()->item_list.elements;
  
  for (uint i= 0; i < elements; i++)
  {
    Item *inner= it++;
    if (!types_allow_materialization(predicate->left_expr->element_index(i),
                                     inner))
      DBUG_RETURN(FALSE);
    if (inner->is_blob_field())
      DBUG_RETURN(FALSE);
  }

  DBUG_PRINT("info",("subquery_types_allow_materialization: ok, allowed"));
  DBUG_RETURN(TRUE);
}


/**
  @brief Check if semijoin's compared types allow materialization.

  @param[inout] sj_nest Semi-join nest containing information about correlated
         expressions. Set nested_join->sjm.scan_allowed to TRUE if
         MaterializeScan strategy allowed.

  @return TRUE if materialization is allowed, FALSE otherwise

  @details
    This is a temporary fix for BUG#36752.
    
    There are two subquery materialization strategies for semijoin:

    1. Materialize and do index lookups in the materialized table. See 
       BUG#36752 for description of restrictions we need to put on the
       compared expressions.

       In addition, since indexes are not supported for BLOB columns,
       this strategy can not be used if any of the columns in the
       materialized table will be BLOB/GEOMETRY columns.  (Note that
       also columns for non-BLOB values that may be greater in size
       than CONVERT_IF_BIGGER_TO_BLOB, will be represented as BLOB
       columns.)

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
bool semijoin_types_allow_materialization(TABLE_LIST *sj_nest)
{
  DBUG_ENTER("semijoin_types_allow_materialization");

  DBUG_ASSERT(sj_nest->nested_join->sj_outer_exprs.elements ==
              sj_nest->nested_join->sj_inner_exprs.elements);

  List_iterator<Item> it1(sj_nest->nested_join->sj_outer_exprs);
  List_iterator<Item> it2(sj_nest->nested_join->sj_inner_exprs);

  sj_nest->nested_join->sjm.scan_allowed= FALSE;
  sj_nest->nested_join->sjm.lookup_allowed= FALSE;

  bool all_are_fields= TRUE;
  bool blobs_involved= FALSE;
  Item *outer, *inner;
  while (outer= it1++, inner= it2++)
  {
    all_are_fields &= (outer->real_item()->type() == Item::FIELD_ITEM && 
                       inner->real_item()->type() == Item::FIELD_ITEM);
    if (!types_allow_materialization(outer, inner))
      DBUG_RETURN(FALSE);
    blobs_involved|= inner->is_blob_field();
  }
  sj_nest->nested_join->sjm.scan_allowed= all_are_fields;
  sj_nest->nested_join->sjm.lookup_allowed= !blobs_involved;
  DBUG_PRINT("info",("semijoin_types_allow_materialization: ok, allowed"));
  DBUG_RETURN(sj_nest->nested_join->sjm.scan_allowed || 
              sj_nest->nested_join->sjm.lookup_allowed);
}


/**
  @brief Check if two items are compatible wrt. materialization.

  @param outer Expression from outer query
  @param inner Expression from inner query

  @retval TRUE   If subquery types allow materialization.
  @retval FALSE  Otherwise.
*/

static bool types_allow_materialization(Item *outer, Item *inner)

{
  if (outer->result_type() != inner->result_type())
    return FALSE;
  switch (outer->result_type()) {
  case STRING_RESULT:
    if (outer->is_datetime() != inner->is_datetime())
      return FALSE;
    if (!(outer->collation.collation == inner->collation.collation
        /*&& outer->max_length <= inner->max_length */))
      return FALSE;
  /*case INT_RESULT:
    if (!(outer->unsigned_flag ^ inner->unsigned_flag))
      return FALSE; */
  default:
    ;                 /* suitable for materialization */
  }
  return TRUE;
}


/*
  Remove the predicates pushed down into the subquery

  SYNOPSIS
    JOIN::remove_subq_pushed_predicates()
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

void JOIN::remove_subq_pushed_predicates(Item **where)
{
  if (conds->type() == Item::FUNC_ITEM &&
      ((Item_func *)this->conds)->functype() == Item_func::EQ_FUNC &&
      ((Item_func *)conds)->arguments()[0]->type() == Item::REF_ITEM &&
      ((Item_func *)conds)->arguments()[1]->type() == Item::FIELD_ITEM &&
      test_if_ref (this->conds, 
                   (Item_field *)((Item_func *)conds)->arguments()[1],
                   ((Item_func *)conds)->arguments()[0]))
  {
    *where= 0;
    return;
  }
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

/**
   Check if the optimizer might choose to use join buffering for this
   join. If that is the case, and if duplicate weedout semijoin
   strategy is used, the duplicate generating range must be extended
   to the first non-const table. 

   This function is called from setup_semijoin_dups_elimination()
   before the final decision is made on whether or not buffering is
   used. It is therefore only a rough test that covers all cases where
   join buffering might be used, but potentially also some cases where
   join buffering will not be used.

   @param join_cache_level     The join cache level
   @param sj_tab               Table that might be joined by BNL/BKA

   @return                     
      true if join buffering might be used, false otherwise

 */
bool might_do_join_buffering(uint join_cache_level, 
                             const JOIN_TAB *sj_tab) 
{
  /* 
     (1) sj_tab is not a const table
  */
  int sj_tabno= sj_tab - sj_tab->join->join_tab;
  return (sj_tabno >= (int)sj_tab->join->const_tables && // (1)
          sj_tab->use_quick != QS_DYNAMIC_RANGE && 
          ((join_cache_level != 0 && sj_tab->type == JT_ALL) ||
           (join_cache_level > 4 && 
            (sj_tab->type == JT_REF || 
             sj_tab->type == JT_EQ_REF ||
             sj_tab->type == JT_CONST))));
}

/**
  Setup the strategies to eliminate semi-join duplicates.
  
  @param join           Join to process
  @param options        Join options (needed to see if join buffering will be 
                        used or not)
  @param no_jbuf_after  Do not use join buffering after the table with this 
                        number

  @retval FALSE  OK 
  @retval TRUE   Out of memory error

  @details
    Setup the strategies to eliminate semi-join duplicates.
    At the moment there are 5 strategies:

    1. DuplicateWeedout (use of temptable to remove duplicates based on rowids
                         of row combinations)
    2. FirstMatch (pick only the 1st matching row combination of inner tables)
    3. LooseScan (scanning the sj-inner table in a way that groups duplicates
                  together and picking the 1st one)
    4. MaterializeLookup (Materialize inner tables, then setup a scan over
                          outer correlated tables, lookup in materialized table)
    5. MaterializeScan (Materialize inner tables, then setup a scan over
                        materialized tables, perform lookup in outer tables)
    
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

      (1) - Prefix of outer correlated and non-correlated tables
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

      (4) - The suffix of outer correlated and non-correlated tables.

    MaterializeLookup strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~~

     (ot|nt)*  [ it (it)* ]  (nt)*
     +------+  +==========+  +---+
        (1)         (2)        (3)

      (1) - Prefix of outer correlated and non-correlated tables.

      (2) - The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table that is
            later used as a lookup structure for the outer correlated tables.

      (3) - The suffix of outer non-correlated tables.

    MaterializeScan strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~~

     (ot|nt)*  [ it (it)* ]  (ot|nt)*
     +------+  +==========+  +-----+
        (1)         (2)         (3)

      (1) - Prefix of outer correlated and non-correlated tables.

      (2) - The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table which is
            later used to setup a scan.

      (3) - The suffix of outer correlated and non-correlated tables.

  Note that MaterializeLookup and MaterializeScan has overlap in their patterns.
  It may be possible to consolidate the materialization strategies into one.
  
  The choice between the strategies is made by the join optimizer (see
  advance_sj_state() and fix_semijoin_strategies_for_picked_join_order()).
  This function sets up all fields/structures/etc needed for execution except
  for setup/initialization of semi-join materialization which is done in 
  setup_sj_materialization() (todo: can't we move that to here also?)
*/

bool setup_semijoin_dups_elimination(JOIN *join, ulonglong options,
                                     uint no_jbuf_after)
{
  uint tableno;
  THD *thd= join->thd;
  DBUG_ENTER("setup_semijoin_dups_elimination");

  if (join->select_lex->sj_nests.is_empty())
    DBUG_RETURN(FALSE);

  for (tableno= join->const_tables ; tableno < join->tables; )
  {
    JOIN_TAB *tab=join->join_tab + tableno;
    POSITION *pos= join->best_positions + tableno;
    uint keylen, keyno;
    if (pos->sj_strategy == SJ_OPT_NONE)
    {
      tableno++;  // nothing to do
      continue;
    }
    JOIN_TAB *last_sj_tab= tab + pos->n_sj_tables - 1;
    switch (pos->sj_strategy) {
      case SJ_OPT_MATERIALIZE_LOOKUP:
      case SJ_OPT_MATERIALIZE_SCAN:
        /* Do nothing */
        tableno+= pos->n_sj_tables;
        break;
      case SJ_OPT_LOOSE_SCAN:
      {
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        /* We jump from the last table to the first one */
        tab->loosescan_match_tab= last_sj_tab;

        /* For LooseScan, duplicate elimination is based on rows being sorted 
           on key. We need to make sure that range select keep the sorted index
           order. (When using MRR it may not.)  

           Note: need_sorted_output() implementations for range select classes 
           that do not support sorted output, will trigger an assert. This 
           should happen since LooseScan strategy will not be picked if sorted 
           output is not supported.
        */
        if (tab->select && tab->select->quick)
          tab->select->quick->need_sorted_output();

        /* Calculate key length */
        keylen= 0;
        keyno= pos->loosescan_key;
        for (uint kp=0; kp < pos->loosescan_parts; kp++)
          keylen += tab->table->key_info[keyno].key_part[kp].store_length;

        tab->loosescan_key_len= keylen;
        if (pos->n_sj_tables > 1) 
          last_sj_tab->do_firstmatch= tab;
        tableno+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_DUPS_WEEDOUT:
      {
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        /*
          Consider a semijoin of one outer and one inner table, both
          with two rows. The inner table is assumed to be confluent
          (See sj_opt_materialize_lookup)

          If normal nested loop execution is used, we do not need to
          include semi-join outer table rowids in the duplicate
          weedout temp table since NL guarantees that outer table rows
          are encountered only consecutively and because all rows in
          the temp table are deleted for every new outer table
          combination (example is with a confluent inner table):

            ot1.row1|it1.row1 
                 '-> temp table's have_confluent_row == FALSE 
                   |-> output ot1.row1
                   '-> set have_confluent_row= TRUE
            ot1.row1|it1.row2
                 |-> temp table's have_confluent_row == TRUE
                 | '-> do not output ot1.row1
                 '-> no more join matches - set have_confluent_row= FALSE
            ot1.row2|it1.row1 
                 '-> temp table's have_confluent_row == FALSE 
                   |-> output ot1.row2
                   '-> set have_confluent_row= TRUE
              ...                 

          Note: not having outer table rowids in the temp table and
          then emptying the temp table when a new outer table row
          combinition is encountered is an optimization. Including
          outer table rowids in the temp table is not harmful but
          wastes memory.

          Now consider the join buffering algorithms (BNL/BKA). These
          algorithms join each inner row with outer rows in "reverse"
          order compared to NL. Effectively, this means that outer
          table rows may be encountered multiple times in a
          non-consecutive manner:

            NL:                 BNL/BKA:
            ot1.row1|it1.row1   ot1.row1|it1.row1
            ot1.row1|it1.row2   ot1.row2|it1.row1
            ot1.row2|it1.row1   ot1.row1|it1.row2
            ot1.row2|it1.row2   ot1.row2|it1.row2

          It is clear from the above that there is no place we can
          empty the temp table like we do in NL to avoid storing outer
          table rowids. 

          Below we check if join buffering might be used. If so, set
          first_table to the first non-constant table so that outer
          table rowids are included in the temp table. Do not destroy
          other duplicate elimination methods. 
        */
        uint first_table= tableno;
        uint join_cache_level= join->thd->variables.optimizer_join_cache_level;
        for (uint sj_tableno= tableno; 
             sj_tableno < tableno + pos->n_sj_tables; 
             sj_tableno++)
        {
          /*
            The final decision on whether or not join buffering will
            be used is taken in check_join_cache_usage(), which is
            called from make_join_readinfo()'s main loop.
            check_join_cache_usage() needs to know if duplicate
            weedout is used, so moving
            setup_semijoin_dups_elimination() from before the main
            loop to after it is not possible. I.e.,
            join->best_positions[sj_tableno].use_join_buffer is not
            trustworthy at this point.
          */
          /**
            @todo: merge make_join_readinfo() and
            setup_semijoin_dups_elimination() loops and change the
            following 'if' to

            "if (join->best_positions[sj_tableno].use_join_buffer && 
                 sj_tableno <= no_jbuf_after)".

            For now, use a rough criteria:
          */

          if (sj_tableno <= no_jbuf_after &&
              might_do_join_buffering(join_cache_level, 
                                      join->join_tab + sj_tableno))

          {
            /* Join buffering will probably be used */
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
        for (JOIN_TAB *tab_in_range= join->join_tab + first_table; 
             tab_in_range <= last_sj_tab; 
             tab_in_range++)
        {
          if (sj_table_is_included(join, tab_in_range))
          {
            last_tab->join_tab= tab_in_range;
            last_tab->rowid_offset= jt_rowid_offset;
            jt_rowid_offset += tab_in_range->table->file->ref_length;
            if (tab_in_range->table->maybe_null)
            {
              last_tab->null_byte= jt_null_bits / 8;
              last_tab->null_bit= jt_null_bits++;
            }
            last_tab++;
            tab_in_range->table->prepare_for_position();
            tab_in_range->keep_current_rowid= TRUE;
          }
        }

        SJ_TMP_TABLE *sjtbl;
        if (jt_rowid_offset) /* Temptable has at least one rowid */
        {
          uint tabs_size= (last_tab - sjtabs) * sizeof(SJ_TMP_TABLE::TAB);
          if (!(sjtbl= new (thd->mem_root) SJ_TMP_TABLE) ||
              !(sjtbl->tabs= (SJ_TMP_TABLE::TAB*) thd->alloc(tabs_size)))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          memcpy(sjtbl->tabs, sjtabs, tabs_size);
          sjtbl->is_confluent= FALSE;
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
            This is confluent case where the entire subquery predicate does 
            not depend on anything at all, ie this is 
              WHERE const IN (uncorrelated select)
          */
          if (!(sjtbl= new (thd->mem_root) SJ_TMP_TABLE))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          sjtbl->tmp_table= NULL;
          sjtbl->is_confluent= TRUE;
          sjtbl->have_confluent_row= FALSE;
        }
        join->join_tab[first_table].flush_weedout_table= sjtbl;
        last_sj_tab->check_weed_out_table= sjtbl;

        tableno+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_FIRST_MATCH:
      {
        JOIN_TAB *jump_to= tab - 1;
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        for (JOIN_TAB *tab_in_range= tab; 
             tab_in_range <= last_sj_tab; 
             tab_in_range++)
        {
          if (!tab_in_range->emb_sj_nest)
          {
            /*
              Let last non-correlated table be jump target for
              subsequent inner tables.
            */
            jump_to= tab_in_range;
          }
          else
          {
            /*
              Assign jump target for last table in a consecutive range of 
              inner tables.
            */
            if (tab_in_range == last_sj_tab || !(tab_in_range+1)->emb_sj_nest)
              tab_in_range->do_firstmatch= jump_to;
          }
        }
        tableno+= pos->n_sj_tables;
        break;
      }
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Destroy all temporary tables created by NL-semijoin runtime
*/

static void destroy_sj_tmp_tables(JOIN *join)
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
  join->sjm_exec_list.empty();
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

static int clear_sj_tmp_tables(JOIN *join)
{
  int res;
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table= it++))
  {
    if ((res= table->file->ha_delete_all_rows()))
      return res; /* purecov: inspected */
  }

  Semijoin_mat_exec *sjm;
  List_iterator<Semijoin_mat_exec> it2(join->sjm_exec_list);
  while ((sjm= it2++))
  {
    sjm->materialized= FALSE;
  }
  return 0;
}



/**
  global select optimisation.

  @note
    error code saved in field 'error'

  @retval
    0   success
  @retval
    1   error
*/

int
JOIN::optimize()
{
  bool need_distinct;
  ulonglong select_opts_for_readinfo;
  uint no_jbuf_after;

  DBUG_ENTER("JOIN::optimize");
  // to prevent double initialization on EXPLAIN
  if (optimized)
    DBUG_RETURN(0);
  optimized= 1;

  thd_proc_info(thd, "optimizing");

  /* dump_TABLE_LIST_graph(select_lex, select_lex->leaf_tables); */
  if (flatten_subqueries())
    DBUG_RETURN(1); /* purecov: inspected */
  /* dump_TABLE_LIST_graph(select_lex, select_lex->leaf_tables); */

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
    conds= simplify_joins(this, join_list, conds, TRUE, FALSE);
    build_bitmap_for_nested_joins(join_list, 0);

    sel->prep_where= conds ? conds->copy_andor_structure(thd) : 0;

    if (arena)
      thd->restore_active_arena(arena, &backup);
  }

  conds= optimize_cond(this, conds, join_list, TRUE, &select_lex->cond_value);
  if (thd->is_error())
  {
    error= 1;
    DBUG_PRINT("error",("Error from optimize_cond"));
    DBUG_RETURN(1);
  }

  {
    having= optimize_cond(this, having, join_list, FALSE,
                          &select_lex->having_value);
    if (thd->is_error())
    {
      error= 1;
      DBUG_PRINT("error",("Error from optimize_cond"));
      DBUG_RETURN(1);
    }
    if (select_lex->cond_value == Item::COND_FALSE || 
        select_lex->having_value == Item::COND_FALSE || 
        (!unit->select_limit_cnt && !(select_options & OPTION_FOUND_ROWS)))
    {						/* Impossible cond */
      DBUG_PRINT("info", (select_lex->having_value == Item::COND_FALSE ? 
                            "Impossible HAVING" : "Impossible WHERE"));
      zero_result_cause=  select_lex->having_value == Item::COND_FALSE ?
                           "Impossible HAVING" : "Impossible WHERE";
      tables= 0;
      goto setup_subq_exit;
    }
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  {
    TABLE_LIST *tbl;
    for (tbl= select_lex->leaf_tables; tbl; tbl= tbl->next_leaf)
    {
      /* 
        If tbl->embedding!=NULL that means that this table is in the inner
        part of the nested outer join, and we can't do partition pruning
        (TODO: check if this limitation can be lifted. 
               This also excludes semi-joins.  Is that intentional?)
      */
      if (!tbl->embedding)
      {
        Item *prune_cond= tbl->on_expr? tbl->on_expr : conds;
        tbl->table->no_partitions_used= prune_partitions(thd, tbl->table,
	                                                 prune_cond);
      }
    }
  }
#endif

  /* 
     Try to optimize count(*), min() and max() to const fields if
     there is implicit grouping (aggregate functions but no
     group_list). In this case, the result set shall only contain one
     row. 
  */
  if (tables_list && implicit_grouping)
  {
    int res;
    /*
      opt_sum_query() returns HA_ERR_KEY_NOT_FOUND if no rows match
      to the WHERE conditions,
      or 1 if all items were resolved (optimized away),
      or 0, or an error number HA_ERR_...

      If all items were resolved by opt_sum_query, there is no need to
      open any tables.
    */
    if ((res=opt_sum_query(select_lex->leaf_tables, all_fields, conds)))
    {
      if (res == HA_ERR_KEY_NOT_FOUND)
      {
        DBUG_PRINT("info",("No matching min/max row"));
	zero_result_cause= "No matching min/max row";
        tables= 0;
        goto setup_subq_exit;
      }
      if (res > 1)
      {
        error= res;
        DBUG_PRINT("error",("Error from opt_sum_query"));
        DBUG_RETURN(1);
      }
      if (res < 0)
      {
        DBUG_PRINT("info",("No matching min/max row"));
        zero_result_cause= "No matching min/max row";
        tables= 0;
        goto setup_subq_exit;
      }
      DBUG_PRINT("info",("Select tables optimized away"));
      zero_result_cause= "Select tables optimized away";
      tables_list= 0;				// All tables resolved
      const_tables= tables;
      /*
        Extract all table-independent conditions and replace the WHERE
        clause with them. All other conditions were computed by opt_sum_query
        and the MIN/MAX/COUNT function(s) have been replaced by constants,
        so there is no need to compute the whole WHERE clause again.
        Notice that make_cond_for_table() will always succeed to remove all
        computed conditions, because opt_sum_query() is applicable only to
        conjunctions.
        Preserve conditions for EXPLAIN.
      */
      if (conds && !(thd->lex->describe & DESCRIBE_EXTENDED))
      {
        Item *table_independent_conds=
          make_cond_for_table(conds, PSEUDO_TABLE_BITS, 0, 0);
        DBUG_EXECUTE("where",
                     print_where(table_independent_conds,
                                 "where after opt_sum_query()",
                                 QT_ORDINARY););
        conds= table_independent_conds;
      }
      goto setup_subq_exit;
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
  thd_proc_info(thd, "statistics");
  if (make_join_statistics(this, select_lex->leaf_tables, conds, &keyuse) ||
      thd->is_fatal_error)
  {
    DBUG_PRINT("error",("Error: make_join_statistics() failed"));
    DBUG_RETURN(1);
  }

  if (rollup.state != ROLLUP::STATE_NONE)
  {
    if (rollup_process_const_fields())
    {
      DBUG_PRINT("error", ("Error: rollup_process_fields() failed"));
      DBUG_RETURN(1);
    }
  }
  else
  {
    /* Remove distinct if only const tables */
    select_distinct= select_distinct && (const_tables != tables);
  }

  thd_proc_info(thd, "preparing");
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
    goto setup_subq_exit;
  }
  if (!(thd->variables.option_bits & OPTION_BIG_SELECTS) &&
      best_read > (double) thd->variables.max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  {						/* purecov: inspected */
    my_message(ER_TOO_BIG_SELECT, ER(ER_TOO_BIG_SELECT), MYF(0));
    error= -1;
    DBUG_RETURN(1);
  }
  if (const_tables && !thd->locked_tables_mode &&
      !(select_options & SELECT_NO_UNLOCK))
    mysql_unlock_some_tables(thd, all_tables, const_tables);
  if (!conds && outer_join)
  {
    /* Handle the case where we have an OUTER JOIN without a WHERE */
    conds=new Item_int((longlong) 1,1);	// Always true
  }

  error= 0;
  reset_nj_counters(join_list);
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
    DBUG_EXECUTE("where",
                 print_where(conds,
                             "after substitute_best_equal",
                             QT_ORDINARY););
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

  if (conds && const_table_map != found_const_table_map &&
      (select_options & SELECT_DESCRIBE))
  {
    conds=new Item_int((longlong) 0,1);	// Always false
  }

  if (make_join_select(this, conds))
  {
    zero_result_cause=
      "Impossible WHERE noticed after reading const tables";
    goto setup_subq_exit;
  }

  error= -1;					/* if goto err */

  /* Optimize distinct away if possible */
  {
    ORDER *org_order= order;
    order=remove_const(this, order,conds,1, &simple_order);
    if (thd->is_error())
    {
      error= 1;
      DBUG_PRINT("error",("Error from remove_const"));
      DBUG_RETURN(1);
    }

    /*
      If we are using ORDER BY NULL or ORDER BY const_expression,
      return result in any order (even if we are using a GROUP BY)
    */
    if (!order && org_order)
      skip_sort_order= 1;
  }
  /*
     Check if we can optimize away GROUP BY/DISTINCT.
     We can do that if there are no aggregate functions, the
     fields in DISTINCT clause (if present) and/or columns in GROUP BY
     (if present) contain direct references to all key parts of
     an unique index (in whatever order) and if the key parts of the
     unique index cannot contain NULLs.
     Note that the unique keys for DISTINCT and GROUP BY should not
     be the same (as long as they are unique).

     The FROM clause must contain a single non-constant table.
  */
  if (tables - const_tables == 1 && (group_list || select_distinct) &&
      !tmp_table_param.sum_func_count &&
      (!join_tab[const_tables].select ||
       !join_tab[const_tables].select->quick ||
       join_tab[const_tables].select->quick->get_type() != 
       QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX))
  {
    if (group_list && rollup.state == ROLLUP::STATE_NONE &&
       list_contains_unique_index(join_tab[const_tables].table,
                                 find_field_in_order_list,
                                 (void *) group_list))
    {
      /*
        We have found that grouping can be removed since groups correspond to
        only one row anyway, but we still have to guarantee correct result
        order. The line below effectively rewrites the query from GROUP BY
        <fields> to ORDER BY <fields>. There are two exceptions:
        - if skip_sort_order is set (see above), then we can simply skip
          GROUP BY;
        - we can only rewrite ORDER BY if the ORDER BY fields are 'compatible'
          with the GROUP BY ones, i.e. either one is a prefix of another.
          We only check if the ORDER BY is a prefix of GROUP BY. In this case
          test_if_subpart() copies the ASC/DESC attributes from the original
          ORDER BY fields.
          If GROUP BY is a prefix of ORDER BY, then it is safe to leave
          'order' as is.
       */
      if (!order || test_if_subpart(group_list, order))
          order= skip_sort_order ? 0 : group_list;
      /*
        If we have an IGNORE INDEX FOR GROUP BY(fields) clause, this must be 
        rewritten to IGNORE INDEX FOR ORDER BY(fields).
      */
      join_tab->table->keys_in_use_for_order_by=
        join_tab->table->keys_in_use_for_group_by;
      group_list= 0;
      group= 0;
    }
    if (select_distinct &&
       list_contains_unique_index(join_tab[const_tables].table,
                                 find_field_in_item_list,
                                 (void *) &fields_list))
    {
      select_distinct= 0;
    }
  }
  if (group_list || tmp_table_param.sum_func_count)
  {
    if (! hidden_group_fields && rollup.state == ROLLUP::STATE_NONE)
      select_distinct=0;
  }
  else if (select_distinct && tables - const_tables == 1 &&
           rollup.state == ROLLUP::STATE_NONE)
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
      skip_sort_order= test_if_skip_sort_order(tab, order, select_limit, 1, 
        &tab->table->keys_in_use_for_order_by);
    if ((group_list=create_distinct_group(thd, select_lex->ref_pointer_array,
                                          order, fields_list, all_fields,
				          &all_order_fields_used)))
    {
      bool skip_group= (skip_sort_order &&
        test_if_skip_sort_order(tab, group_list, select_limit, 1, 
                                &tab->table->keys_in_use_for_group_by) != 0);
      count_field_types(select_lex, &tmp_table_param, all_fields, 0);
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
    if (thd->is_error())
    {
      error= 1;
      DBUG_PRINT("error",("Error from remove_const"));
      DBUG_RETURN(1);
    }
    if (old_group_list && !group_list)
      select_distinct= 0;
  }
  if (!group_list && group)
  {
    order=0;					// The output has only one row
    simple_order=1;
    select_distinct= 0;                       // No need in distinct for 1 row
    group_optimized_away= 1;
  }

  calc_group_buffer(this, group_list);
  send_group_parts= tmp_table_param.group_parts; /* Save org parts */
  if (procedure && procedure->group)
  {
    group_list= procedure->group= remove_const(this, procedure->group, conds,
					       1, &simple_group);
    if (thd->is_error())
    {
      error= 1;
      DBUG_PRINT("error",("Error from remove_const"));
      DBUG_RETURN(1);
    }   
    calc_group_buffer(this, group_list);
  }

  if (test_if_subpart(group_list, order) ||
      (!group_list && tmp_table_param.sum_func_count))
  {
    order=0;
    if (is_indexed_agg_distinct(this, NULL))
      sort_and_group= 0;
  }

  // Can't use sort on head table if using join buffering
  if (full_join)
  {
    TABLE *stable= (sort_by_table == (TABLE *) 1 ? 
      join_tab[const_tables].table : sort_by_table);
    /* 
      FORCE INDEX FOR ORDER BY can be used to prevent join buffering when
      sorting on the first table.
    */
    if (!stable || !stable->force_index_order)
    {
      if (group_list)
        simple_group= 0;
      if (order)
        simple_order= 0;
    }
  }

  /*
    Check if we need to create a temporary table.
    This has to be done if all tables are not already read (const tables)
    and one of the following conditions holds:
    - We are using DISTINCT (simple distinct's are already optimized away)
    - We are using an ORDER BY or GROUP BY on fields not in the first table
    - We are using different ORDER BY and GROUP BY orders
    - The user wants us to buffer the result.
    When the WITH ROLLUP modifier is present, we cannot skip temporary table
    creation for the DISTINCT clause just because there are only const tables.
  */
  need_tmp= ((const_tables != tables &&
	     ((select_distinct || !simple_order || !simple_group) ||
	      (group_list && order) ||
	      test(select_options & OPTION_BUFFER_RESULT))) ||
             (rollup.state != ROLLUP::STATE_NONE && select_distinct));

  /*
    If the hint FORCE INDEX FOR ORDER BY/GROUP BY is used for the table
    whose columns are required to be returned in a sorted order, then
    the proper value for no_jbuf_after should be yielded by a call to
    the make_join_orderinfo function. 
    Yet the current implementation of FORCE INDEX hints does not
    allow us to do it in a clean manner.
  */   
  no_jbuf_after= 1 ? tables : make_join_orderinfo(this);
  select_opts_for_readinfo= 
    (select_options & (SELECT_DESCRIBE | SELECT_NO_JOIN_CACHE)) |
    (select_lex->ftfunc_list->elements ?  SELECT_NO_JOIN_CACHE : 0);

  // No cache for MATCH == 'Don't use join buffering when we use MATCH'.
  if (make_join_readinfo(this, select_opts_for_readinfo, no_jbuf_after))
    DBUG_RETURN(1);

  /* Perform FULLTEXT search before all regular searches */
  if (!(select_options & SELECT_DESCRIBE))
    init_ftfuncs(thd, select_lex, test(order));

  /* Create all structures needed for materialized subquery execution. */
  if (setup_subquery_materialization())
    DBUG_RETURN(1);

  /*
    It's necessary to check const part of HAVING cond as
    there is a chance that some cond parts may become
    const items after make_join_statisctics(for example
    when Item is a reference to cost table field from
    outer join).
    This check is performed only for those conditions
    which do not use aggregate functions. In such case
    temporary table may not be used and const condition
    elements may be lost during further having
    condition transformation in JOIN::exec.
  */
  if (having && const_table_map && !having->with_sum_func)
  {
    having->update_used_tables();
    having= remove_eq_conds(thd, having, &select_lex->having_value);
    if (select_lex->having_value == Item::COND_FALSE)
    {
      having= new Item_int((longlong) 0,1);
      zero_result_cause= "Impossible HAVING noticed after reading const tables";
      error= 0;
      DBUG_RETURN(0);
    }
  }

  /* Cache constant expressions in WHERE, HAVING, ON clauses. */
  cache_const_exprs();

  /*
    is this simple IN subquery?
  */
  if (!group_list && !order &&
      unit->item && unit->item->substype() == Item_subselect::IN_SUBS &&
      tables == 1 && conds &&
      !unit->is_union())
  {
    if (!having)
    {
      Item *where= conds;
      if (join_tab[0].type == JT_EQ_REF &&
	  join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
        remove_subq_pushed_predicates(&where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= JT_UNIQUE_SUBQUERY;
        error= 0;
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
	remove_subq_pushed_predicates(&where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= JT_INDEX_SUBQUERY;
        error= 0;
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
               having->name == in_having_cond)
    {
      join_tab[0].type= JT_INDEX_SUBQUERY;
      error= 0;
      conds= remove_additional_cond(conds);
      save_index_subquery_explain_info(join_tab, conds);
      DBUG_RETURN(unit->item->
		  change_engine(new subselect_indexsubquery_engine(thd,
								   join_tab,
								   unit->item,
								   conds,
                                                                   having,
								   1)));
    }

  }
  /*
    Need to tell handlers that to play it safe, it should fetch all
    columns of the primary key of the tables: this is because MySQL may
    build row pointers for the rows, and for all columns of the primary key
    the read set has not necessarily been set by the server code.
  */
  if (need_tmp || select_distinct || group_list || order)
  {
    for (uint i = const_tables; i < tables; i++)
      join_tab[i].table->prepare_for_position();
  }

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
        ((order && simple_order) || (group_list && simple_group)))
    {
      if (add_ref_to_table_cond(thd,&join_tab[const_tables])) {
        DBUG_RETURN(1);
      }
    }
    
    if (!(select_options & SELECT_BIG_RESULT) &&
        ((group_list &&
          (!simple_group ||
           !test_if_skip_sort_order(&join_tab[const_tables], group_list,
                                    unit->select_limit_cnt, 0, 
                                    &join_tab[const_tables].table->
                                    keys_in_use_for_group_by))) ||
         select_distinct) &&
        tmp_table_param.quick_group && !procedure)
    {
      need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
    }
    if (order)
    {
      /*
        Do we need a temporary table due to the ORDER BY not being equal to
        the GROUP BY? The call to test_if_skip_sort_order above tests for the
        GROUP BY clause only and hence is not valid in this case. So the
        estimated number of rows to be read from the first table is not valid.
        We clear it here so that it doesn't show up in EXPLAIN.
       */
      if (need_tmp && (select_options & SELECT_DESCRIBE) != 0)
        join_tab[const_tables].limit= 0;
      /*
        Force using of tmp table if sorting by a SP or UDF function due to
        their expensive and probably non-deterministic nature.
      */
      for (ORDER *tmp_order= order; tmp_order ; tmp_order=tmp_order->next)
      {
        Item *item= *tmp_order->item;
        if (item->is_expensive())
        {
          /* Force tmp table without sort */
          need_tmp=1; simple_order=simple_group=0;
          break;
        }
      }
    }
  }

  tmp_having= having;
  if (select_options & SELECT_DESCRIBE)
  {
    error= 0;
    DBUG_RETURN(0);
  }
  having= 0;

  /*
    The loose index scan access method guarantees that all grouping or
    duplicate row elimination (for distinct) is already performed
    during data retrieval, and that all MIN/MAX functions are already
    computed for each group. Thus all MIN/MAX functions should be
    treated as regular functions, and there is no need to perform
    grouping in the main execution loop.
    Notice that currently loose index scan is applicable only for
    single table queries, thus it is sufficient to test only the first
    join_tab element of the plan for its access method.
  */
  need_distinct= TRUE;
  if (join_tab->is_using_loose_index_scan())
  {
    tmp_table_param.precomputed_group_by= TRUE;
    if (join_tab->is_using_agg_loose_index_scan())
    {
      need_distinct= FALSE;
      tmp_table_param.precomputed_group_by= FALSE;
    }
  }

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    DBUG_PRINT("info",("Creating tmp table"));
    thd_proc_info(thd, "Creating tmp table");

    init_items_ref_array();

    tmp_table_param.hidden_field_count= (all_fields.elements -
					 fields_list.elements);
    ORDER *tmp_group= ((!simple_group && !procedure &&
                        !(test_flags & TEST_NO_KEY_GROUP)) ? group_list :
                                                             (ORDER*) 0);
    /*
      Pushing LIMIT to the temporary table creation is not applicable
      when there is ORDER BY or GROUP BY or there is no GROUP BY, but
      there are aggregate functions, because in all these cases we need
      all result rows.
    */
    ha_rows tmp_rows_limit= ((order == 0 || skip_sort_order) &&
                             !tmp_group &&
                             !thd->lex->current_select->with_sum_func) ?
                            select_limit : HA_POS_ERROR;

    if (!(exec_tmp_table1=
	  create_tmp_table(thd, &tmp_table_param, all_fields,
                           tmp_group, group_list ? 0 : select_distinct,
			   group_list && simple_group,
			   select_options, tmp_rows_limit, "")))
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
      thd_proc_info(thd, "Sorting for group");
      if (create_sort_index(thd, this, group_list,
			    HA_POS_ERROR, HA_POS_ERROR, FALSE) ||
	  alloc_group_fields(this, group_list) ||
          make_sum_func_list(all_fields, fields_list, 1) ||
          prepare_sum_aggregators(sum_funcs, need_distinct) ||
          setup_sum_funcs(thd, sum_funcs))
      {
        DBUG_RETURN(1);
      }
      group_list=0;
    }
    else
    {
      if (make_sum_func_list(all_fields, fields_list, 0) ||
          prepare_sum_aggregators(sum_funcs, need_distinct) ||
          setup_sum_funcs(thd, sum_funcs))
      {
        DBUG_RETURN(1);
      }

      if (!group_list && ! exec_tmp_table1->distinct && order && simple_order)
      {
        thd_proc_info(thd, "Sorting for order");
        if (create_sort_index(thd, this, order,
                              HA_POS_ERROR, HA_POS_ERROR, TRUE))
        {
          DBUG_RETURN(1);
        }
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
				    order, unit->select_limit_cnt, 0, 
                                    &join_tab[const_tables].table->
                                      keys_in_use_for_order_by))
	  order=0;
      }
    }

    /* If this join belongs to an uncacheable query save the original join */
    if (select_lex->uncacheable && init_save_join_tab())
      DBUG_RETURN(-1);                         /* purecov: inspected */
  }

  error= 0;
  DBUG_RETURN(0);

setup_subq_exit:
  /*
    Even with zero matching rows, subqueries in the HAVING clause may
    need to be evaluated if there are aggregate functions in the
    query. If we have planned to materialize the subquery, we need to
    set it up properly before prematurely leaving optimize().
  */
  if (setup_subquery_materialization())
    DBUG_RETURN(1);
  error= 0;
  DBUG_RETURN(0);
}


/**
  Restore values in temporary join.
*/
void JOIN::restore_tmp()
{
  memcpy(tmp_join, this, (size_t) sizeof(JOIN));
}


int
JOIN::reinit()
{
  DBUG_ENTER("JOIN::reinit");

  unit->offset_limit_cnt= (ha_rows)(select_lex->offset_limit ?
                                    select_lex->offset_limit->val_uint() :
                                    ULL(0));

  first_record= 0;

  if (exec_tmp_table1)
  {
    exec_tmp_table1->file->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table1->file->ha_delete_all_rows();
    free_io_cache(exec_tmp_table1);
    filesort_free_buffers(exec_tmp_table1,0);
  }
  if (exec_tmp_table2)
  {
    exec_tmp_table2->file->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table2->file->ha_delete_all_rows();
    free_io_cache(exec_tmp_table2);
    filesort_free_buffers(exec_tmp_table2,0);
  }
  clear_sj_tmp_tables(this);
  if (items0)
    set_items_ref_array(items0);

  if (join_tab_save)
  {
    for (uint ix=0; ix < tables; ++ix)
      join_tab[ix]= join_tab_save[ix];
  }

  /* need to reset ref access state (see join_read_key) */
  if (join_tab)
    for (uint i= 0; i < tables; i++)
      join_tab[i].ref.key_err= TRUE;

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

/**
   @brief Save the original join layout
      
   @details Saves the original join layout so it can be reused in 
   re-execution and for EXPLAIN.
             
   @return Operation status
   @retval 0      success.
   @retval 1      error occurred.
*/

bool
JOIN::init_save_join_tab()
{
  if (!(tmp_join= (JOIN*)thd->alloc(sizeof(JOIN))))
    return 1;                                  /* purecov: inspected */
  error= 0;				       // Ensure that tmp_join.error= 0
  restore_tmp();
  return 0;
}


bool
JOIN::save_join_tab()
{
  if (!join_tab_save && select_lex->master_unit()->uncacheable)
  {
    if (!(join_tab_save= new (thd->mem_root) JOIN_TAB[tables]))
      return TRUE;
    for (uint ix= 0; ix < tables; ++ix)
      join_tab_save[ix]= join_tab[ix];
  }
  return FALSE;
}


/**
  Exec select.

  @todo
    Note, that create_sort_index calls test_if_skip_sort_order and may
    finally replace sorting with index scan if there is a LIMIT clause in
    the query.  It's never shown in EXPLAIN!

  @todo
    When can we have here thd->net.report_error not zero?
*/
void
JOIN::exec()
{
  List<Item> *columns_list= &fields_list;
  int      tmp_error;
  DBUG_ENTER("JOIN::exec");

  thd_proc_info(thd, "executing");
  error= 0;
  if (procedure)
  {
    procedure_fields_list= fields_list;
    if (procedure->change_columns(procedure_fields_list) ||
	result->prepare(procedure_fields_list, unit))
    {
      thd->limit_found_rows= thd->examined_row_count= 0;
      DBUG_VOID_RETURN;
    }
    columns_list= &procedure_fields_list;
  }
  (void) result->prepare2(); // Currently, this cannot fail.

  if (!tables_list && (tables || !select_lex->with_sum_func))
  {                                           // Only test of functions
    if (select_options & SELECT_DESCRIBE)
      select_describe(this, FALSE, FALSE, FALSE,
		      (zero_result_cause?zero_result_cause:"No tables used"));
    else
    {
      if (result->send_result_set_metadata(*columns_list,
                                           Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
      {
        DBUG_VOID_RETURN;
      }
      /*
        We have to test for 'conds' here as the WHERE may not be constant
        even if we don't have any tables for prepared statements or if
        conds uses something like 'rand()'.
        If the HAVING clause is either impossible or always true, then
        JOIN::having is set to NULL by optimize_cond.
        In this case JOIN::exec must check for JOIN::having_value, in the
        same way it checks for JOIN::cond_value.
      */
      if (select_lex->cond_value != Item::COND_FALSE &&
          select_lex->having_value != Item::COND_FALSE &&
          (!conds || conds->val_int()) &&
          (!having || having->val_int()))
      {
	if (do_send_rows &&
            (procedure ? (procedure->send_row(procedure_fields_list) ||
             procedure->end_of_records()) : result->send_data(fields_list)))
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
  /*
    Don't reset the found rows count if there're no tables as
    FOUND_ROWS() may be called. Never reset the examined row count here.
    It must be accumulated from all join iterations of all join parts.
  */
  if (tables)
    thd->limit_found_rows= 0;

  if (zero_result_cause)
  {
    (void) return_zero_rows(this, result, select_lex->leaf_tables,
                            *columns_list,
			    send_row_on_empty_set(),
			    select_options,
			    zero_result_cause,
			    having);
    DBUG_VOID_RETURN;
  }

  if ((this->select_lex->options & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(this, PROCESSED_BY_JOIN_EXEC))
    DBUG_VOID_RETURN;

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
        (order != group_list || !(select_options & SELECT_BIG_RESULT)) &&
	(const_tables == tables ||
 	 ((simple_order || skip_sort_order) &&
	  test_if_skip_sort_order(&join_tab[const_tables], order,
				  select_limit, 0, 
                                  &join_tab[const_tables].table->
                                    keys_in_use_for_query))))
      order=0;
    having= tmp_having;
    select_describe(this, need_tmp,
		    order != 0 && !skip_sort_order,
		    select_distinct,
                    !tables ? "No tables used" : NullS);
    DBUG_VOID_RETURN;
  }

  JOIN *curr_join= this;
  List<Item> *curr_all_fields= &all_fields;
  List<Item> *curr_fields_list= &fields_list;
  TABLE *curr_tmp_table= 0;
  /*
    Initialize examined rows here because the values from all join parts
    must be accumulated in examined_row_count. Hence every join
    iteration must count from zero.
  */
  curr_join->examined_rows= 0;

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
    thd_proc_info(thd, "Copying to tmp table");
    DBUG_PRINT("info", ("%s", thd->proc_info));
    /*
      If there is no sorting or grouping, one may turn off
      requirement that access method should deliver rows in sorted
      order.  Exception: LooseScan strategy for semijoin requires
      sorted access even if final result is not to be sorted.
    */
    if (!curr_join->sort_and_group &&
        curr_join->const_tables != curr_join->tables && 
        curr_join->best_positions[curr_join->const_tables].sj_strategy 
          != SJ_OPT_LOOSE_SCAN)
      curr_join->join_tab[curr_join->const_tables].sorted= 0;
    if ((tmp_error= do_select(curr_join, (List<Item> *) 0, curr_tmp_table, 0)))
    {
      error= tmp_error;
      DBUG_VOID_RETURN;
    }
    curr_tmp_table->file->info(HA_STATUS_VARIABLE);
    
    if (curr_join->having)
      curr_join->having= curr_join->tmp_having= 0; // Allready done
    
    /* Change sum_fields reference to calculated fields in tmp_table */
    if (curr_join != this)
      curr_join->all_fields= *curr_all_fields;
    if (!items1)
    {
      items1= items0 + all_fields.elements;
      if (sort_and_group || curr_tmp_table->group ||
          tmp_table_param.precomputed_group_by)
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
      if (curr_join != this)
      {
        curr_join->tmp_all_fields1= tmp_all_fields1;
        curr_join->tmp_fields_list1= tmp_fields_list1;
      }
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

    if ((curr_join->group_list && (!test_if_subpart(curr_join->group_list,
						   curr_join->order) || 
				  curr_join->select_distinct)) ||
	(curr_join->select_distinct &&
	 curr_join->tmp_table_param.using_indirect_summary_function))
    {					/* Must copy to another table */
      DBUG_PRINT("info",("Creating group table"));
      
      /* Free first data from old join */
      curr_join->join_free();
      if (curr_join->make_simple_join(this, curr_tmp_table))
	DBUG_VOID_RETURN;
      calc_group_buffer(curr_join, group_list);
      count_field_types(select_lex, &curr_join->tmp_table_param,
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

        /*
          If the access method is loose index scan then all MIN/MAX
          functions are precomputed, and should be treated as regular
          functions. See extended comment in JOIN::exec.
        */
        if (curr_join->join_tab->is_using_loose_index_scan())
          curr_join->tmp_table_param.precomputed_group_by= TRUE;

	if (!(curr_tmp_table=
	      exec_tmp_table2= create_tmp_table(thd,
						&curr_join->tmp_table_param,
						*curr_all_fields,
						(ORDER*) 0,
						curr_join->select_distinct && 
						!curr_join->group_list,
						1, curr_join->select_options,
						HA_POS_ERROR, "")))
	  DBUG_VOID_RETURN;
	curr_join->exec_tmp_table2= exec_tmp_table2;
      }
      if (curr_join->group_list)
      {
	thd_proc_info(thd, "Creating sort index");
	if (curr_join->join_tab == join_tab && save_join_tab())
	{
	  DBUG_VOID_RETURN;
	}
	if (create_sort_index(thd, curr_join, curr_join->group_list,
			      HA_POS_ERROR, HA_POS_ERROR, FALSE) ||
	    make_group_fields(this, curr_join))
	{
	  DBUG_VOID_RETURN;
	}
        sortorder= curr_join->sortorder;
      }
      
      thd_proc_info(thd, "Copying to group table");
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
					1, TRUE) ||
        prepare_sum_aggregators(curr_join->sum_funcs,
          !curr_join->join_tab->is_using_agg_loose_index_scan()))
        DBUG_VOID_RETURN;
      curr_join->group_list= 0;
      if (!curr_join->sort_and_group &&
          curr_join->const_tables != curr_join->tables)
        curr_join->join_tab[curr_join->const_tables].sorted= 0;
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
        if (curr_join != this)
        {
          curr_join->tmp_fields_list2= tmp_fields_list2;
          curr_join->tmp_all_fields2= tmp_all_fields2;
        }
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
    
    curr_join->join_free();			/* Free quick selects */
    if (curr_join->select_distinct && ! curr_join->group_list)
    {
      thd_proc_info(thd, "Removing duplicates");
      if (curr_join->tmp_having)
	curr_join->tmp_having->update_used_tables();
      if (remove_duplicates(curr_join, curr_tmp_table,
			    *curr_fields_list, curr_join->tmp_having))
	DBUG_VOID_RETURN;
      curr_join->tmp_having=0;
      curr_join->select_distinct=0;
    }
    curr_tmp_table->reginfo.lock_type= TL_UNLOCK;
    if (curr_join->make_simple_join(this, curr_tmp_table))
      DBUG_VOID_RETURN;
    calc_group_buffer(curr_join, curr_join->group_list);
    count_field_types(select_lex, &curr_join->tmp_table_param, 
                      *curr_all_fields, 0);
    
  }
  if (procedure)
    count_field_types(select_lex, &curr_join->tmp_table_param, 
                      *curr_all_fields, 0);
  
  if (curr_join->group || curr_join->implicit_grouping ||
      curr_join->tmp_table_param.sum_func_count ||
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
      if (curr_join != this)
      {
        curr_join->tmp_all_fields3= tmp_all_fields3;
        curr_join->tmp_fields_list3= tmp_fields_list3;
      }
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
        prepare_sum_aggregators(curr_join->sum_funcs,
                                !curr_join->join_tab ||
                                !curr_join->join_tab->
                                  is_using_agg_loose_index_scan()) ||
        setup_sum_funcs(curr_join->thd, curr_join->sum_funcs) ||
        thd->is_fatal_error)
      DBUG_VOID_RETURN;
  }
  if (curr_join->group_list || curr_join->order)
  {
    DBUG_PRINT("info",("Sorting for send_result_set_metadata"));
    thd_proc_info(thd, "Sorting result");
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
						 used_tables, 0);
      if (sort_table_cond)
      {
	if (!curr_table->select)
	  if (!(curr_table->select= new SQL_SELECT))
	    DBUG_VOID_RETURN;
	if (!curr_table->select->cond)
	  curr_table->select->cond= sort_table_cond;
	else
	{
	  if (!(curr_table->select->cond=
		new Item_cond_and(curr_table->select->cond,
				  sort_table_cond)))
	    DBUG_VOID_RETURN;
	  curr_table->select->cond->fix_fields(thd, 0);
	}
        curr_table->set_select_cond(curr_table->select->cond, __LINE__);
	curr_table->select_cond->top_level_item();
	DBUG_EXECUTE("where",print_where(curr_table->select->cond,
					 "select and having",
                                         QT_ORDINARY););
	curr_join->tmp_having= make_cond_for_table(curr_join->tmp_having,
						   ~ (table_map) 0,
						   ~used_tables, 0);
	DBUG_EXECUTE("where",print_where(curr_join->tmp_having,
                                         "having after sort",
                                         QT_ORDINARY););
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
			     HA_POS_ERROR : unit->select_limit_cnt),
                            curr_join->group_list ? FALSE : TRUE))
	DBUG_VOID_RETURN;
      sortorder= curr_join->sortorder;
      if (curr_join->const_tables != curr_join->tables &&
          !curr_join->join_tab[curr_join->const_tables].table->sort.io_cache)
      {
        /*
          If no IO cache exists for the first table then we are using an
          INDEX SCAN and no filesort. Thus we should not remove the sorted
          attribute on the INDEX SCAN.
        */
        skip_sort_order= 1;
      }
    }
  }
  /* XXX: When can we have here thd->is_error() not zero? */
  if (thd->is_error())
  {
    error= thd->is_error();
    DBUG_VOID_RETURN;
  }
  curr_join->having= curr_join->tmp_having;
  curr_join->fields= curr_fields_list;
  curr_join->procedure= procedure;

  thd_proc_info(thd, "Sending data");
  DBUG_PRINT("info", ("%s", thd->proc_info));
  result->send_result_set_metadata((procedure ? curr_join->procedure_fields_list :
                                    *curr_fields_list),
                                   Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
  error= do_select(curr_join, curr_fields_list, NULL, procedure);
  thd->limit_found_rows= curr_join->send_records;

  /* Accumulate the counts from all join iterations of all join parts. */
  thd->examined_row_count+= curr_join->examined_rows;
  DBUG_PRINT("counts", ("thd->examined_row_count: %lu",
                        (ulong) thd->examined_row_count));

  /* 
    With EXPLAIN EXTENDED we have to restore original ref_array
    for a derived table which is always materialized.
    We also need to do this when we have temp table(s).
    Otherwise we would not be able to print the query correctly.
  */ 
  if (items0 && (thd->lex->describe & DESCRIBE_EXTENDED) &&
      (select_lex->linkage == DERIVED_TABLE_TYPE ||
       exec_tmp_table1 || exec_tmp_table2))
    set_items_ref_array(items0);

  DBUG_VOID_RETURN;
}


/**
  Clean up join.

  @return
    Return error that hold JOIN.
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
	tab->cleanup();
    }
    tmp_join->tmp_join= 0;
    /*
      We need to clean up tmp_table_param for reusable JOINs (having non-zero
      and different from self tmp_join) because it's not being cleaned up
      anywhere else (as we need to keep the join is reusable).
    */
    tmp_table_param.cleanup();
    tmp_table_param.copy_field= tmp_join->tmp_table_param.copy_field= 0;
    DBUG_RETURN(tmp_join->destroy());
  }
  cond_equal= 0;

  cleanup(1);
 /* Cleanup items referencing temporary table columns */
  cleanup_item_list(tmp_all_fields1);
  cleanup_item_list(tmp_all_fields3);
  if (exec_tmp_table1)
    free_tmp_table(thd, exec_tmp_table1);
  if (exec_tmp_table2)
    free_tmp_table(thd, exec_tmp_table2);
  destroy_sj_tmp_tables(this);

  List_iterator<TABLE_LIST> sj_list_it(select_lex->sj_nests);
  TABLE_LIST *sj_nest;
  while ((sj_nest= sj_list_it++))
    sj_nest->sj_mat_exec= NULL;

  delete_dynamic(&keyuse);
  delete procedure;
  DBUG_RETURN(error);
}


void JOIN::cleanup_item_list(List<Item> &items) const
{
  if (!items.is_empty())
  {
    List_iterator_fast<Item> it(items);
    Item *item;
    while ((item= it++))
      item->cleanup();
  }
}


/**
  An entry point to single-unit select (a select without UNION).

  @param thd                  thread handler
  @param rref_pointer_array   a reference to ref_pointer_array of
                              the top-level select_lex for this query
  @param tables               list of all tables used in this query.
                              The tables have been pre-opened.
  @param wild_num             number of wildcards used in the top level 
                              select of this query.
                              For example statement
                              SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
                              has 3 wildcards.
  @param fields               list of items in SELECT list of the top-level
                              select
                              e.g. SELECT a, b, c FROM t1 will have Item_field
                              for a, b and c in this list.
  @param conds                top level item of an expression representing
                              WHERE clause of the top level select
  @param og_num               total number of ORDER BY and GROUP BY clauses
                              arguments
  @param order                linked list of ORDER BY agruments
  @param group                linked list of GROUP BY arguments
  @param having               top level item of HAVING expression
  @param proc_param           list of PROCEDUREs
  @param select_options       select options (BIG_RESULT, etc)
  @param result               an instance of result set handling class.
                              This object is responsible for send result
                              set rows to the client or inserting them
                              into a table.
  @param select_lex           the only SELECT_LEX of this query
  @param unit                 top-level UNIT of this query
                              UNIT is an artificial object created by the
                              parser for every SELECT clause.
                              e.g.
                              SELECT * FROM t1 WHERE a1 IN (SELECT * FROM t2)
                              has 2 unions.

  @retval
    FALSE  success
  @retval
    TRUE   an error
*/

bool
mysql_select(THD *thd, Item ***rref_pointer_array,
	     TABLE_LIST *tables, uint wild_num, List<Item> &fields,
	     Item *conds, uint og_num,  ORDER *order, ORDER *group,
	     Item *having, ORDER *proc_param, ulonglong select_options,
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
        err= join->prepare(rref_pointer_array, tables, wild_num,
                           conds, og_num, order, group, having, proc_param,
                           select_lex, unit);
        if (err)
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
	DBUG_RETURN(TRUE); /* purecov: inspected */
    thd_proc_info(thd, "init");
    thd->used_tables=0;                         // Updated by setup_fields
    err= join->prepare(rref_pointer_array, tables, wild_num,
                       conds, og_num, order, group, having, proc_param,
                       select_lex, unit);
    if (err)
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

  if (thd->is_error())
    goto err;

  join->exec();

  if (thd->lex->describe & DESCRIBE_EXTENDED)
  {
    select_lex->where= join->conds_history;
    select_lex->having= join->having_history;
  }

err:
  if (free_join)
  {
    thd_proc_info(thd, "end");
    err|= select_lex->cleanup();
    DBUG_RETURN(err || thd->is_error());
  }
  DBUG_RETURN(join->error);
}


int subq_sj_candidate_cmp(Item_exists_subselect* const *el1, 
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


inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
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


void fix_list_after_tbl_changes(st_select_lex *parent_select,
                                st_select_lex *removed_select,
                                List<TABLE_LIST> *tlist)
{
  List_iterator<TABLE_LIST> it(*tlist);
  TABLE_LIST *table;
  while ((table= it++))
  {
    if (table->on_expr)
      table->on_expr->fix_after_pullout(parent_select, removed_select,
                                        &table->on_expr);
    if (table->nested_join)
      fix_list_after_tbl_changes(parent_select, removed_select,
                                 &table->nested_join->join_list);
  }
}


/**
  Convert a subquery predicate into a TABLE_LIST semi-join nest

  @param parent_join Parent join, which has subq_pred in its WHERE/ON clause.
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

bool convert_subquery_to_semijoin(JOIN *parent_join,
                                  Item_exists_subselect *subq_pred)
{
  SELECT_LEX *parent_lex= parent_join->select_lex;
  TABLE_LIST *emb_tbl_nest= NULL;
  List<TABLE_LIST> *emb_join_list= &parent_lex->top_join_list;
  THD *thd= parent_join->thd;
  DBUG_ENTER("convert_subquery_to_semijoin");

  DBUG_ASSERT(subq_pred->substype() == Item_subselect::IN_SUBS);

  /*
    1. Find out where to put the predicate into.
     Note: for "t1 LEFT JOIN t2" this will be t2, a leaf.
  */
  if ((void*)subq_pred->embedding_join_nest != (void*)1)
  {
    if (subq_pred->embedding_join_nest->nested_join)
    {
      /*
        We're dealing with

          ... [LEFT] JOIN  ( ... ) ON (subquery AND whatever) ...

        The sj-nest will be inserted into the brackets nest.
      */
      emb_tbl_nest=  subq_pred->embedding_join_nest;
      emb_join_list= &emb_tbl_nest->nested_join->join_list;
    }
    else if (!subq_pred->embedding_join_nest->outer_join)
    {
      /*
        We're dealing with

          ... INNER JOIN tblX ON (subquery AND whatever) ...

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
      TABLE_LIST *wrap_nest;
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
      if (!(wrap_nest= alloc_join_nest(thd)))
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
  if (!(sj_nest= alloc_join_nest(thd)))
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
  for (tl= parent_lex->leaf_tables; tl->next_leaf; tl= tl->next_leaf)
  {}
  tl->next_leaf= subq_lex->leaf_tables;
  last_leaf= tl;

  /*
    Same as above for next_local chain
    (a theory: a next_local chain always starts with ::leaf_tables
     because view's tables are inserted after the view)
  */
  for (tl= parent_lex->leaf_tables; tl->next_local; tl= tl->next_local)
  {}
  tl->next_local= subq_lex->leaf_tables;

  /* A theory: no need to re-connect the next_global chain */

  /* 3. Remove the original subquery predicate from the WHERE/ON */

  // The subqueries were replaced for Item_int(1) earlier
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
  parent_join->tables+= subq_lex->join->tables;

  nested_join->sj_outer_exprs.empty();
  nested_join->sj_inner_exprs.empty();

  /*
    @todo: Add similar conversion for subqueries other than IN.
  */
  if (subq_pred->substype() == Item_subselect::IN_SUBS)
  {
    Item_in_subselect *in_subq_pred= (Item_in_subselect *)subq_pred;

    /* Left side of IN predicate is already resolved */
    DBUG_ASSERT(in_subq_pred->left_expr->fixed);

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
    sj_nest->sj_on_expr= subq_lex->where;

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

    for (uint i= 0; i < in_subq_pred->left_expr->cols(); i++)
    {
      nested_join->sj_outer_exprs.push_back(in_subq_pred->left_expr->
                                            element_index(i));
      nested_join->sj_inner_exprs.push_back(subq_lex->ref_pointer_array[i]);

      Item_func_eq *item_eq= 
        new Item_func_eq(in_subq_pred->left_expr->element_index(i), 
                         subq_lex->ref_pointer_array[i]);
      if (item_eq == NULL)
        DBUG_RETURN(TRUE);

      /*
        Mark this item as a subquery equality predicate. Currently, we
        only check that this field is different from UINT_MAX, so a boolean
        would do better than an integer.
        @todo: Convert to bool, or start using it as an integer.
      */
      item_eq->in_equality_no= i;
      sj_nest->sj_on_expr= and_items(sj_nest->sj_on_expr, item_eq);
      if (sj_nest->sj_on_expr == NULL)
        DBUG_RETURN(TRUE);
    }
    /* Fix the created equality and AND */
    sj_nest->sj_on_expr->fix_fields(thd, &sj_nest->sj_on_expr);
  }

  /* Unlink the child select_lex: */
  subq_lex->master_unit()->exclude_level();
  /*
    Walk through sj nest's WHERE and ON expressions and call
    item->fix_table_changes() for all items.
  */
  sj_nest->sj_on_expr->fix_after_pullout(parent_lex, subq_lex,
                                         &sj_nest->sj_on_expr);
  fix_list_after_tbl_changes(parent_lex, subq_lex,
                             &sj_nest->nested_join->join_list);

  //TODO fix QT_
  DBUG_EXECUTE("where",
               print_where(sj_nest->sj_on_expr,"SJ-EXPR", QT_ORDINARY););

  if (emb_tbl_nest)
  {
    /* Inject sj_on_expr into the parent's ON condition */
    emb_tbl_nest->on_expr= and_items(emb_tbl_nest->on_expr, 
                                     sj_nest->sj_on_expr);
    if (emb_tbl_nest->on_expr == NULL)
      DBUG_RETURN(TRUE);
    emb_tbl_nest->on_expr->fix_fields(parent_join->thd, &emb_tbl_nest->on_expr);
  }
  else
  {
    /* Inject sj_on_expr into the parent's WHERE condition */
    parent_join->conds= and_items(parent_join->conds, sj_nest->sj_on_expr);
    if (parent_join->conds == NULL)
      DBUG_RETURN(TRUE);
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


/*
  Convert semi-join subquery predicates into semi-join join nests

  SYNOPSIS
    JOIN::flatten_subqueries()
 
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

bool JOIN::flatten_subqueries()
{
  Query_arena *arena, backup;
  Item_exists_subselect **subq;
  Item_exists_subselect **subq_end;
  DBUG_ENTER("JOIN::flatten_subqueries");

  if (sj_subselects.elements() == 0)
    DBUG_RETURN(FALSE);

  /* First, convert child join's subqueries. We proceed bottom-up here */
  for (subq= sj_subselects.front(), subq_end= sj_subselects.back(); 
       subq != subq_end;
       subq++)
  {
    /*
      Currently, we only support transformation of IN subqueries.
    */
    DBUG_ASSERT((*subq)->substype() == Item_subselect::IN_SUBS);

    st_select_lex *child_select= (*subq)->get_select_lex();
    JOIN *child_join= child_select->join;
    child_join->outer_tables = child_join->tables;

    /*
      child_select->where contains only the WHERE predicate of the
      subquery itself here. We may be selecting from a VIEW, which has its
      own predicate. The combined predicates are available in child_join->conds,
      which was built by setup_conds() doing prepare_where() for all views.
    */
    child_select->where= child_join->conds;

    if (child_join->flatten_subqueries())
      DBUG_RETURN(TRUE);
    (*subq)->sj_convert_priority= 
      (*subq)->is_correlated * MAX_TABLES + child_join->outer_tables;
  }
  
  // Temporary measure: disable semi-joins when they are together with outer
  // joins.
  for (TABLE_LIST *tbl= select_lex->leaf_tables; tbl; tbl=tbl->next_leaf)
  {
    if (tbl->on_expr || tbl->in_outer_join_nest())
    {
      subq= sj_subselects.front();
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
  sj_subselects.sort(subq_sj_candidate_cmp);
  // #tables-in-parent-query + #tables-in-subquery < MAX_TABLES
  /* Replace all subqueries to be flattened with Item_int(1) */
  arena= thd->activate_stmt_arena_if_needed(&backup);
  for (subq= sj_subselects.front(); 
       subq != subq_end && 
       tables + (*subq)->unit->first_select()->join->tables < MAX_TABLES;
       subq++)
  {
    Item **tree= ((*subq)->embedding_join_nest == (TABLE_LIST*)1)?
                   &conds : &((*subq)->embedding_join_nest->on_expr);
    if (replace_subcondition(this, tree, *subq, new Item_int(1), FALSE))
      DBUG_RETURN(TRUE); /* purecov: inspected */
  }
 
  for (subq= sj_subselects.front(); 
       subq != subq_end && 
       tables + (*subq)->unit->first_select()->join->tables < MAX_TABLES;
       subq++)
  {
    if (convert_subquery_to_semijoin(this, *subq))
      DBUG_RETURN(TRUE);
  }
skip_conversion:
  bool converted= FALSE;
  /* 
    3. Finalize the subqueries that we did not convert,
       ie. perform IN->EXISTS rewrite.
  */
  for (; subq!= subq_end; subq++)
  {
    JOIN *child_join= (*subq)->unit->first_select()->join;
    Item_subselect::trans_res res;
    (*subq)->changed= 0;
    (*subq)->fixed= 0;

    SELECT_LEX *save_select_lex= thd->lex->current_select;
    thd->lex->current_select= (*subq)->unit->first_select();
    converted= TRUE;

    res= (*subq)->select_transformer(child_join);

    thd->lex->current_select= save_select_lex;

    if (res == Item_subselect::RES_ERROR)
      DBUG_RETURN(TRUE);

    (*subq)->changed= 1;
    (*subq)->fixed= 1;

    Item *substitute= (*subq)->substitution;
    bool do_fix_fields= !(*subq)->substitution->fixed;
    Item **tree= ((*subq)->embedding_join_nest == (TABLE_LIST*)1)?
                   &conds : &((*subq)->embedding_join_nest->on_expr);
    if (replace_subcondition(this, tree, *subq, substitute, do_fix_fields))
      DBUG_RETURN(TRUE);
    (*subq)->substitution= NULL;
     
    if (!thd->stmt_arena->is_conventional())
    {
      tree= ((*subq)->embedding_join_nest == (TABLE_LIST*)1)?
                     &select_lex->prep_where :
                     &((*subq)->embedding_join_nest->prep_on_expr);

      if (replace_subcondition(this, tree, *subq, substitute, 
                                     FALSE))
        DBUG_RETURN(TRUE);
    }
  }

  if (arena)
    thd->restore_active_arena(arena, &backup);
  sj_subselects.clear();
  DBUG_RETURN(FALSE);
}


/**
  Setup for execution all subqueries of a query, for which the optimizer
  chose hash semi-join.

  @details Iterate over all subqueries of the query, and if they are under an
  IN predicate, and the optimizer chose to compute it via hash semi-join:
  - try to initialize all data structures needed for the materialized execution
    of the IN predicate,
  - if this fails, then perform the IN=>EXISTS transformation which was
    previously blocked during JOIN::prepare.

  This method is part of the "code generation" query processing phase.

  This phase must be called after substitute_for_best_equal_field() because
  that function may replace items with other items from a multiple equality,
  and we need to reference the correct items in the index access method of the
  IN predicate.

  @return Operation status
  @retval FALSE     success.
  @retval TRUE      error occurred.
*/

bool JOIN::setup_subquery_materialization()
{
  for (SELECT_LEX_UNIT *un= select_lex->first_inner_unit(); un;
       un= un->next_unit())
  {
    for (SELECT_LEX *sl= un->first_select(); sl; sl= sl->next_select())
    {
      Item_subselect *subquery_predicate= sl->master_unit()->item;
      if (subquery_predicate &&
          subquery_predicate->substype() == Item_subselect::IN_SUBS)
      {
        Item_in_subselect *in_subs= (Item_in_subselect*) subquery_predicate;
        if (in_subs->exec_method ==
              Item_exists_subselect::EXEC_MATERIALIZATION &&
            in_subs->setup_engine())
          return TRUE;
      }
    }
  }
  return FALSE;
}


/*
  Check if table's Key_use elements have an eq_ref(outer_tables) candidate

  SYNOPSIS
    find_eq_ref_candidate()
      table             Table to be checked
      sj_inner_tables   Bitmap of inner tables. eq_ref(inner_table) doesn't
                        count.

  DESCRIPTION
    Check if table's Key_use elements have an eq_ref(outer_tables) candidate

  TODO
    Check again if it is feasible to factor common parts with constant table
    search

  RETURN
    TRUE  - There exists an eq_ref(outer-tables) candidate
    FALSE - Otherwise
*/

bool find_eq_ref_candidate(TABLE *table, table_map sj_inner_tables)
{
  Key_use *keyuse= table->reginfo.join_tab->keyuse;
  uint key;

  if (keyuse)
  {
    while (1) /* For each key */
    {
      key= keyuse->key;
      KEY *keyinfo= table->key_info + key;
      key_part_map bound_parts= 0;
      if ((keyinfo->flags & (HA_NOSAME)) == HA_NOSAME)
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
    FALSE - OK
    TRUE  - Out of memory error
*/

bool pull_out_semijoin_tables(JOIN *join)
{
  TABLE_LIST *sj_nest;
  DBUG_ENTER("pull_out_semijoin_tables");

  if (join->select_lex->sj_nests.is_empty())
    DBUG_RETURN(FALSE);

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
        if (tbl->table->map & join->const_table_map)
        {
          pulled_tables |= tbl->table->map;
          DBUG_PRINT("info", ("Table %s pulled out (reason: constant)",
                              tbl->table->alias));
        }
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
                                tbl->table->alias));
            /*
              Pulling a table out of uncorrelated subquery in general makes
              makes it correlated. See the NOTE to this function. 
            */
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
        if (tbl->table &&
            !(inner_tables & tbl->table->map))
        {
          /*
            Pull the table up in the same way as simplify_joins() does:
            update join_list and embedding pointers but keep next[_local]
            pointers.
          */
          child_li.remove();
          if (upper_join_list->push_back(tbl))
          {
            if (arena)
              join->thd->restore_active_arena(arena, &backup);
            DBUG_RETURN(TRUE);
          }
          tbl->join_list= upper_join_list;
          tbl->embedding= sj_nest->embedding;
        }
      }

      /* Remove the sj-nest itself if we've removed everything from it */
      if (!inner_tables)
      {
        List_iterator<TABLE_LIST> li(*upper_join_list);
        /* Find the sj_nest in the list. */
        while (sj_nest != li++)
        {}
        li.remove();
        /* Also remove it from the list of SJ-nests: */
        sj_list_it.remove();
      }

      if (arena)
        join->thd->restore_active_arena(arena, &backup);
    }
  }
  DBUG_RETURN(FALSE);
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
  uchar buff[STACK_BUFF_ALLOC];
  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    DBUG_RETURN(0);                           // Fatal error flag is set
  if (select)
  {
    select->head=table;
    if ((error= select->test_quick_select(thd, *(key_map *)keys,(table_map) 0,
                                          limit, 0, FALSE)) == 1)
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
   This structure is used to collect info on potentially sargable
   predicates in order to check whether they become sargable after
   reading const tables.
   We form a bitmap of indexes that can be used for sargable predicates.
   Only such indexes are involved in range analysis.
*/
typedef struct st_sargable_param
{
  Field *field;              /* field against which to check sargability */
  Item **arg_value;          /* values of potential keys for lookups     */
  uint num_values;           /* number of values in the above array      */
} SARGABLE_PARAM;  



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


/**
  Calculate the best possible join and initialize the join structure.

  @retval
    0	ok
  @retval
    1	Fatal error
*/

static bool
make_join_statistics(JOIN *join, TABLE_LIST *tables_arg, Item *conds,
		     DYNAMIC_ARRAY *keyuse_array)
{
  int error;
  TABLE *table;
  TABLE_LIST *tables= tables_arg;
  uint i,table_count,const_count,key;
  table_map found_const_table_map, all_table_map, found_ref, refs;
  TABLE **table_vector;
  JOIN_TAB *stat,*stat_end,*s,**stat_ref;
  Key_use *keyuse, *start_keyuse;
  table_map outer_join=0;
  SARGABLE_PARAM *sargables= 0;
  JOIN_TAB *stat_vector[MAX_TABLES+1];
  DBUG_ENTER("make_join_statistics");

  table_count=join->tables;
  stat= new (join->thd->mem_root) JOIN_TAB[table_count];
  stat_ref=(JOIN_TAB**) join->thd->alloc(sizeof(JOIN_TAB*)*MAX_TABLES);
  table_vector=(TABLE**) join->thd->alloc(sizeof(TABLE*)*(table_count*2));
  if (!stat || !stat_ref || !table_vector)
    DBUG_RETURN(1);				// Eom /* purecov: inspected */

  if (!(join->positions=
        new (join->thd->mem_root) POSITION[table_count+1]))
    DBUG_RETURN(TRUE);

  if (!(join->best_positions=
        new (join->thd->mem_root) POSITION[table_count+1]))
    DBUG_RETURN(TRUE);

  join->best_ref=stat_vector;

  stat_end=stat+table_count;
  found_const_table_map= all_table_map=0;
  const_count=0;

  for (s= stat, i= 0;
       tables;
       s++, tables= tables->next_leaf, i++)
  {
    stat_vector[i]=s;
    table_vector[i]=s->table=table=tables->table;
    table->pos_in_table_list= tables;
    error= table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    if (error)
    {
      table->file->print_error(error, MYF(0));
      goto error;
    }
    table->quick_keys.clear_all();
    table->reginfo.join_tab=s;
    table->reginfo.not_exists_optimize=0;
    bzero((char*) table->const_key_parts, sizeof(key_part_map)*table->s->keys);
    all_table_map|= table->map;
    s->join=join;

    s->dependent= tables->dep_tables;
    if (tables->schema_table)
      table->file->stats.records= 2;
    table->quick_condition_rows= table->file->stats.records;

    s->on_expr_ref= &tables->on_expr;
    if (*s->on_expr_ref)
    {
      /* s is the only inner table of an outer join */
#ifdef WITH_PARTITION_STORAGE_ENGINE
      if ((!table->file->stats.records || table->no_partitions_used) && 
          !tables->in_outer_join_nest())
#else
      if (!table->file->stats.records && !tables->in_outer_join_nest())
#endif
      {						// Empty table
        s->dependent= 0;                        // Ignore LEFT JOIN depend.
	set_position(join, const_count++, s, NULL);
	continue;
      }
      outer_join|= table->map;
      s->embedding_map= 0;
      for (TABLE_LIST *embedding= tables->embedding;
           embedding;
           embedding= embedding->embedding)
        s->embedding_map|= embedding->nested_join->nj_map;
      continue;
    }
    if (tables->in_outer_join_nest())
    {
      /* s belongs to a nested join, maybe to several embedded joins */
      s->embedding_map= 0;
      for (TABLE_LIST *embedding= tables->embedding;
           embedding;
           embedding= embedding->embedding)
      {
        NESTED_JOIN *nested_join= embedding->nested_join;
        s->embedding_map|=nested_join->nj_map;
        s->dependent|= embedding->dep_tables;
        outer_join|= nested_join->used_tables;
      }
      continue;
    }
#ifdef WITH_PARTITION_STORAGE_ENGINE
    const bool no_partitions_used= table->no_partitions_used;
#else
    const bool no_partitions_used= FALSE;
#endif
    if ((table->s->system || table->file->stats.records <= 1 ||
         no_partitions_used) &&
	!s->dependent &&
	(table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT) &&
        !table->fulltext_searched && !join->no_const_tables)
    {
      set_position(join, const_count++, s, NULL);
    }
  }
  stat_vector[i]=0;
  join->outer_join=outer_join;

  if (join->outer_join)
  {
    /* 
       Build transitive closure for relation 'to be dependent on'.
       This will speed up the plan search for many cases with outer joins,
       as well as allow us to catch illegal cross references.
       Warshall's algorithm is used to build the transitive closure.
       As we may restart the outer loop upto 'table_count' times, the
       complexity of the algorithm is O((number of tables)^3).
       However, most of the iterations will be shortcircuited when
       there are no pedendencies to propogate.
    */
    for (i= 0 ; i < table_count ; i++)
    {
      uint j;
      table= stat[i].table;

      if (!table->reginfo.join_tab->dependent)
        continue;

      /* Add my dependencies to other tables depending on me */
      for (j= 0, s= stat ; j < table_count ; j++, s++)
      {
        if (s->dependent & table->map)
        {
          table_map was_dependent= s->dependent;
          s->dependent |= table->reginfo.join_tab->dependent;
          /*
            If we change dependencies for a table we already have
            processed: Redo dependency propagation from this table.
          */
          if (i > j && s->dependent != was_dependent)
          {
            i = j-1;
            break;
          }
        }
      }
    }

    for (i= 0, s= stat ; i < table_count ; i++, s++)
    {
      /* Catch illegal cross references for outer joins */
      if (s->dependent & s->table->map)
      {
        join->tables=0;			// Don't use join->table
        my_message(ER_WRONG_OUTER_JOIN, ER(ER_WRONG_OUTER_JOIN), MYF(0));
        goto error;
      }

      if (outer_join & s->table->map)
        s->table->maybe_null= 1;
      s->key_dependent= s->dependent;
    }
  }

  if (conds || outer_join)
    if (update_ref_and_keys(join->thd, keyuse_array, stat, join->tables,
                            conds, join->cond_equal,
                            ~outer_join, join->select_lex, &sargables))
      goto error;

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
	goto error;		// Fatal error
    }
    else
    {
      found_const_table_map|= s->table->map;
      s->table->pos_in_table_list->optimized_away= TRUE;
    }
  }

  /* loop until no more const tables are found */
  int ref_changed;
  do
  {
  more_const_tables_found:
    ref_changed = 0;
    found_ref=0;

    /*
      We only have to loop from stat_vector + const_count as
      set_position() will move all const_tables first in stat_vector
    */

    for (JOIN_TAB **pos=stat_vector+const_count ; (s= *pos) ; pos++)
    {
      table=s->table;

      /* 
        If equi-join condition by a key is null rejecting and after a
        substitution of a const table the key value happens to be null
        then we can state that there are no matches for this equi-join.
      */  
      if ((keyuse= s->keyuse) && *s->on_expr_ref && !s->embedding_map)
      {
        /* 
          When performing an outer join operation if there are no matching rows
          for the single row of the outer table all the inner tables are to be
          null complemented and thus considered as constant tables.
          Here we apply this consideration to the case of outer join operations 
          with a single inner table only because the case with nested tables
          would require a more thorough analysis.
          TODO. Apply single row substitution to null complemented inner tables
          for nested outer join operations. 
	*/              
        while (keyuse->table == table)
        {
          if (!(keyuse->val->used_tables() & ~join->const_table_map) &&
              keyuse->val->is_null() && keyuse->null_rejecting)
          {
            s->type= JT_CONST;
            mark_as_null_row(table);
            found_const_table_map|= table->map;
	    join->const_table_map|= table->map;
	    set_position(join, const_count++, s, NULL);
            goto more_const_tables_found;
           }
	  keyuse++;
        }
      }

      if (s->dependent)				// If dependent on some table
      {
	// All dep. must be constants
        if (s->dependent & ~(join->const_table_map))
	  continue;
	if (table->file->stats.records <= 1L &&
	    (table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT) &&
            !table->pos_in_table_list->in_outer_join_nest())
	{					// system table
	  int tmp= 0;
	  s->type=JT_SYSTEM;
	  join->const_table_map|=table->map;
	  set_position(join, const_count++, s, NULL);
	  if ((tmp= join_read_const_table(s, join->positions+const_count-1)))
	  {
	    if (tmp > 0)
	      goto error;			// Fatal error
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
          key_map const_ref, eq_part;
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

          /*
            TODO (low priority): currently we ignore the const tables that
            are within a semi-join nest which is within an outer join nest.
            The effect of this is that we don't do const substitution for
            such tables.
          */
	  if (eq_part.is_prefix(table->key_info[key].key_parts) &&
              !table->fulltext_searched && 
              !table->pos_in_table_list->in_outer_join_nest())
	  {
            if (table->key_info[key].flags & HA_NOSAME)
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
                  goto error;
	        if ((tmp=join_read_const_table(s,
                                               join->positions+const_count-1)))
	        {
		  if (tmp > 0)
		    goto error;			// Fatal error
	        }
	        else
		  found_const_table_map|= table->map;
	        break;
	      }
	      else
	        found_ref|= refs;      // Table is const if all refs are const
	    }
            else if (const_ref == eq_part)
              s->const_keys.set_bit(key);
          }
	}
      }
    }
  } while (join->const_table_map & found_ref && ref_changed);
 
  /* 
    Update info on indexes that can be used for search lookups as
    reading const tables may has added new sargable predicates. 
  */
  if (const_count && sargables)
  {
    for( ; sargables->field ; sargables++)
    {
      Field *field= sargables->field;
      JOIN_TAB *join_tab= field->table->reginfo.join_tab;
      key_map possible_keys= field->key_start;
      possible_keys.intersect(field->table->keys_in_use_for_query);
      bool is_const= 1;
      for (uint j=0; j < sargables->num_values; j++)
        is_const&= sargables->arg_value[j]->const_item();
      if (is_const)
        join_tab[0].const_keys.merge(possible_keys);
    }
  }

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
    s->found_records=s->records=s->table->file->stats.records;
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
    
    /*
      Perform range analysis if there are keys it could use (1). 
      Don't do range analysis if we're on the inner side of an outer join (2).
      Do range analysis if we're on the inner side of a semi-join (3).
    */
    if (!s->const_keys.is_clear_all() &&                        // (1)
        (!s->table->pos_in_table_list->embedding ||             // (2)
         (s->table->pos_in_table_list->embedding &&             // (3)
          s->table->pos_in_table_list->embedding->sj_on_expr))) // (3)
    {
      ha_rows records;
      SQL_SELECT *select;
      select= make_select(s->table, found_const_table_map,
			  found_const_table_map,
			  *s->on_expr_ref ? *s->on_expr_ref : conds,
			  1, &error);
      if (!select)
        goto error;
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
	set_position(join, const_count++, s, NULL);
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

  if (pull_out_semijoin_tables(join))
    DBUG_RETURN(TRUE);

  /*
    Set pointer to embedding semijoin nest for all semijoined tables.
    Note that this must be done for every table inside all semijoin nests,
    even for tables within outer join nests embedded in semijoin nests.
    A table can never be part of multiple semijoin nests, hence no
    ambiguities can ever occur.
    Note also that the pointer is not set for TABLE_LIST objects that
    are outer join nests within semijoin nests.
  */
  for (s= stat; s < stat_end; s++)
  {
    for (TABLE_LIST *tables= s->table->pos_in_table_list;
         tables->embedding;
         tables= tables->embedding)
    {
      if (tables->embedding->sj_on_expr)
      {
        s->emb_sj_nest= tables->embedding;
        break;
      }
    }
  }

  join->join_tab=stat;
  join->map2table=stat_ref;
  join->all_tables= table_vector;
  join->const_tables=const_count;
  join->found_const_table_map=found_const_table_map;

  if (join->const_tables != join->tables)
    optimize_keyuse(join, keyuse_array);
   
  if (optimize_semijoin_nests(join, all_table_map))
    DBUG_RETURN(TRUE); /* purecov: inspected */

  /* Find an optimal join order of the non-constant tables. */
  if (join->const_tables != join->tables)
  {
    if (choose_plan(join, all_table_map & ~join->const_table_map))
      goto error;
  }
  else
  {
    memcpy((uchar*) join->best_positions,(uchar*) join->positions,
	   sizeof(POSITION)*join->const_tables);
    join->best_read=1.0;
  }
  /* Generate an execution plan from the found optimal join order. */
  error= join->thd->killed || get_best_combination(join);
  DBUG_RETURN(error);

error:
  /*
    Need to clean up join_tab from TABLEs in case of error.
    They won't get cleaned up by JOIN::cleanup() because JOIN::join_tab
    may not be assigned yet by this function (which is building join_tab).
    Dangling TABLE::reginfo.join_tab may cause part_of_refkey to choke. 
  */
  for (tables= tables_arg; tables; tables= tables->next_leaf)
    tables->table->reginfo.join_tab= NULL;
  DBUG_RETURN (1);
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

static bool optimize_semijoin_nests(JOIN *join, table_map all_table_map)
{
  DBUG_ENTER("optimize_semijoin_nests");
  List_iterator<TABLE_LIST> sj_list_it(join->select_lex->sj_nests);
  TABLE_LIST *sj_nest;

  while ((sj_nest= sj_list_it++))
  {
    /* As a precaution, reset pointers that were used in prior execution */
    sj_nest->sj_mat_exec= NULL;
    sj_nest->nested_join->sjm.positions= NULL;

    /* Calculate the cost of materialization if materialization is allowed. */
    if (join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SEMIJOIN) &&
        join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MATERIALIZATION))
    {
      /* semi-join nests with only constant tables are not valid */
      DBUG_ASSERT(sj_nest->sj_inner_tables & ~join->const_table_map);
      /*
        Try semijoin materialization if the semijoin is classified as
        non-trivially-correlated.
      */ 
      if (sj_nest->nested_join->sj_corr_tables)
        continue;
      /*
        Check whether data types allow execution with materialization.
      */
      if (semijoin_types_allow_materialization(sj_nest))
      {
        join->emb_sjm_nest= sj_nest;
        if (choose_plan(join, all_table_map & ~join->const_table_map))
          DBUG_RETURN(TRUE); /* purecov: inspected */
        /*
          The best plan to run the subquery is now in join->best_positions,
          save it.
        */
        const uint n_tables= my_count_bits(sj_nest->sj_inner_tables);
        double subjoin_out_rows, subjoin_read_time;
        get_partial_join_cost(join, n_tables,
                              &subjoin_read_time, &subjoin_out_rows);

        sj_nest->nested_join->sjm.materialization_cost
          .convert_from_cost(subjoin_read_time);
        sj_nest->nested_join->sjm.expected_rowcount= subjoin_out_rows;

        List<Item> &inner_expr_list= sj_nest->nested_join->sj_inner_exprs;
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
          for (uint i=0 ; i < join->const_tables + n_tables ; i++)
          {
            JOIN_TAB *tab= join->best_positions[i].table;
            join->map2table[tab->table->tablenr]= tab;
          }
          List_iterator<Item> it(inner_expr_list);
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
          sj_nest->nested_join->sjm.expected_rowcount=
            min(sj_nest->nested_join->sjm.expected_rowcount, rows);
        }
        if (!(sj_nest->nested_join->sjm.positions=
              (st_position*)join->thd->alloc(sizeof(st_position)*n_tables)))
          DBUG_RETURN(TRUE);

        memcpy(sj_nest->nested_join->sjm.positions,
               join->best_positions + join->const_tables, 
               sizeof(st_position) * n_tables);

        /*
          Calculate temporary table parameters and usage costs
        */
        uint rowlen= get_tmp_table_rec_length(inner_expr_list);
        double lookup_cost;
        if (rowlen * subjoin_out_rows< join->thd->variables.max_heap_table_size)
          lookup_cost= HEAP_TEMPTABLE_LOOKUP_COST;
        else
          lookup_cost= DISK_TEMPTABLE_LOOKUP_COST;

        /*
          Let materialization cost include the cost to write the data into the
          temporary table:
        */ 
        sj_nest->nested_join->sjm.materialization_cost
          .add_io(subjoin_out_rows, lookup_cost);
        
        /*
          Set the cost to do a full scan of the temptable (will need this to 
          consider doing sjm-scan):
        */ 
        sj_nest->nested_join->sjm.scan_cost.zero();
        if (sj_nest->nested_join->sjm.expected_rowcount > 0.0)
          sj_nest->nested_join->sjm.scan_cost
           .add_io(sj_nest->nested_join->sjm.expected_rowcount, lookup_cost);

        sj_nest->nested_join->sjm.lookup_cost.convert_from_cost(lookup_cost);
      }
    }
  }
  join->emb_sjm_nest= NULL;
  DBUG_RETURN(FALSE);
}


/*****************************************************************************
  Check with keys are used and with tables references with tables
  Updates in stat:
	  keys	     Bitmap of all used keys
	  const_keys Bitmap of all keys with may be used with quick_select
	  keyuse     Pointer to possible keys
*****************************************************************************/

/// Used when finding key fields
typedef struct key_field_t {
  Field		*field;
  Item		*val;			///< May be empty if diff constant
  uint		level;
  uint		optimize; // KEY_OPTIMIZE_*
  bool		eq_func;
  /**
    If true, the condition this struct represents will not be satisfied
    when val IS NULL.
    @sa Key_use::null_rejecting .
  */
  bool          null_rejecting;
  bool          *cond_guard;                    ///< @sa Key_use::cond_guard
  uint          sj_pred_no;                     ///< @sa Key_use::sj_pred_no
} KEY_FIELD;

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

/**
  Merge new key definitions to old ones, remove those not used in both.

  This is called for OR between different levels.

  To be able to do 'ref_or_null' we merge a comparison of a column
  and 'column IS NULL' to one test.  This is useful for sub select queries
  that are internally transformed to something like:.

  @code
  SELECT * FROM t1 WHERE t1.key=outer_ref_field or t1.key IS NULL 
  @endcode

  KEY_FIELD::null_rejecting is processed as follows: @n
  result has null_rejecting=true if it is set for both ORed references.
  for example:
  -   (t2.key = t1.field OR t2.key  =  t1.field) -> null_rejecting=true
  -   (t2.key = t1.field OR t2.key <=> t1.field) -> null_rejecting=false

  @todo
    The result of this is that we're missing some 'ref' accesses.
    OptimizerTeam: Fix this
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
        /*
          NOTE: below const_item() call really works as "!used_tables()", i.e.
          it can return FALSE where it is feasible to make it return TRUE.
          
          The cause is as follows: Some of the tables are already known to be
          const tables (the detection code is in make_join_statistics(),
          above the update_ref_and_keys() call), but we didn't propagate 
          information about this: TABLE::const_table is not set to TRUE, and
          Item::update_used_tables() hasn't been called for each item.
          The result of this is that we're missing some 'ref' accesses.
          TODO: OptimizerTeam: Fix this
        */
	if (!new_fields->val->const_item())
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
                 old->val->eq_by_collation(new_fields->val, 
                                           old->field->binary(),
                                           old->field->charset()))

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
		 ((old->val->const_item() && old->val->is_null()) || 
                  new_fields->val->is_null()))
	{
	  /* field = expression OR field IS NULL */
	  old->level= and_level;
	  old->optimize= KEY_OPTIMIZE_REF_OR_NULL;
	  /*
            Remember the NOT NULL value unless the value does not depend
            on other tables.
          */
	  if (!old->val->used_tables() && old->val->is_null())
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


/**
  Given a field, return its index in semi-join's select list, or UINT_MAX

  @param field Field that we are looking up table for

  @retval =UINT_MAX Field is not from a semijoin-transformed subquery
  @retval <UINT_MAX Index in select list of subquery

  @details
  Given a field, find its table; then see if the table is within a
  semi-join nest and if the field was in select list of the subquery
  (if subquery was part of a quantified comparison predicate), or
  the field was a result of subquery decorrelation.
  If it was, then return the field's index in the select list.
  The value is used by LooseScan strategy.
*/

static uint get_semi_join_select_list_index(Field *field)
{
  TABLE_LIST *emb_sj_nest= field->table->pos_in_table_list->embedding;
  if (emb_sj_nest && emb_sj_nest->sj_on_expr)
  {
    List<Item> &items= emb_sj_nest->nested_join->sj_inner_exprs;
    List_iterator<Item> it(items);
    for (uint i= 0; i < items.elements; i++)
    {
      Item *sel_item= it++;
      if (sel_item->type() == Item::FIELD_ITEM &&
          ((Item_field*)sel_item)->field->eq(field))
        return i;
    }
  }
  return UINT_MAX;
}

/**
   @brief 
   If EXPLAIN EXTENDED, add warning that an index cannot be used for
   ref access

   @details
   If EXPLAIN EXTENDED, add a warning for each index that cannot be
   used for ref access due to either type conversion or different
   collations on the field used for comparison

   Example type conversion (char compared to int):

   CREATE TABLE t1 (url char(1) PRIMARY KEY);
   SELECT * FROM t1 WHERE url=1;

   Example different collations (danish vs german2):

   CREATE TABLE t1 (url char(1) PRIMARY KEY) collate latin1_danish_ci;
   SELECT * FROM t1 WHERE url='1' collate latin1_german2_ci;

   @param thd                Thread for the connection that submitted the query
   @param field              Field used in comparision
   @param cant_use_indexes   Indexes that cannot be used for lookup
 */
static void 
warn_index_not_applicable(THD *thd, const Field *field, 
                          const key_map cant_use_index) 
{
  if (thd->lex->describe & DESCRIBE_EXTENDED)
    for (uint j=0 ; j < field->table->s->keys ; j++)
      if (cant_use_index.is_set(j))
        push_warning_printf(thd,
                            MYSQL_ERROR::WARN_LEVEL_WARN, 
                            ER_WARN_INDEX_NOT_APPLICABLE,
                            ER(ER_WARN_INDEX_NOT_APPLICABLE),
                            "ref",
                            field->table->key_info[j].name,
                            field->field_name);
}

/**
  Add a possible key to array of possible keys if it's usable as a key

    @param key_fields      Pointer to add key, if usable
    @param and_level       And level, to be stored in KEY_FIELD
    @param cond            Condition predicate
    @param field           Field used in comparision
    @param eq_func         True if we used =, <=> or IS NULL
    @param value           Value used for comparison with field
    @param usable_tables   Tables which can be used for key optimization
    @param sargables       IN/OUT Array of found sargable candidates

  @note
    If we are doing a NOT NULL comparison on a NOT NULL field in a outer join
    table, we store this to be able to do not exists optimization later.

  @returns
    *key_fields is incremented if we stored a key in the array
*/

static void
add_key_field(KEY_FIELD **key_fields,uint and_level, Item_func *cond,
              Field *field, bool eq_func, Item **value, uint num_values,
              table_map usable_tables, SARGABLE_PARAM **sargables)
{
  DBUG_PRINT("info",("add_key_field for field %s",field->field_name));
  uint exists_optimize= 0;
  if (!(field->flags & PART_KEY_FLAG))
  {
    // Don't remove column IS NULL on a LEFT JOIN table
    if (!eq_func || (*value)->type() != Item::NULL_ITEM ||
        !field->table->maybe_null || field->null_ptr)
      return;					// Not a key. Skip it
    exists_optimize= KEY_OPTIMIZE_EXISTS;
    DBUG_ASSERT(num_values == 1);
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
      {
        if (!(is_const&= value[i]->const_item()))
          break;
      }
      if (is_const)
        stat[0].const_keys.merge(possible_keys);
      else if (!eq_func)
      {
        /* 
          Save info to be able check whether this predicate can be 
          considered as sargable for range analisis after reading const tables.
          We do not save info about equalities as update_const_equal_items
          will take care of updating info on keys from sargable equalities. 
        */
        (*sargables)--;
        (*sargables)->field= field;
        (*sargables)->arg_value= value;
        (*sargables)->num_values= num_values;
      }
      /*
	We can't always use indexes when comparing a string index to a
	number. cmp_type() is checked to allow compare of dates to numbers.
        eq_func is NEVER true when num_values > 1
       */
      if (!eq_func)
      {
        /* 
          Additional optimization: if we're processing
          "t.key BETWEEN c1 AND c1" then proceed as if we were processing
          "t.key = c1".
          TODO: This is a very limited fix. A more generic fix is possible. 
          There are 2 options:
          A) Make equality propagation code be able to handle BETWEEN
             (including cases like t1.key BETWEEN t2.key AND t3.key)
          B) Make range optimizer to infer additional "t.key = c" equalities
             and use them in equality propagation process (see details in
             OptimizerKBAndTodo)
        */
        if ((cond->functype() != Item_func::BETWEEN) ||
            ((Item_func_between*) cond)->negated ||
            !value[0]->eq(value[1], field->binary()))
          return;
        eq_func= TRUE;
      }

      if (field->result_type() == STRING_RESULT)
      {
        if ((*value)->result_type() != STRING_RESULT)
        {
          if (field->cmp_type() != (*value)->result_type())
          {
            warn_index_not_applicable(stat->join->thd, field, possible_keys);
            return;
          }
        }
        else
        {
          /*
            We can't use indexes if the effective collation
            of the operation differ from the field collation.
          */
          if (field->cmp_type() == STRING_RESULT &&
              ((Field_str*)field)->charset() != cond->compare_collation())
          {
            warn_index_not_applicable(stat->join->thd, field, possible_keys);
            return;
          }
        }
      }
    }
  }
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
    'othertbl.field IS NOT NULL' to tab->select_cond, if this is not an outer
    join. We also use it to shortcut reading "tbl" when othertbl.field is
    found to be a NULL value (in join_read_always_key() and BKA).
  */
  {
    Item *real= (*value)->real_item();
    if (((cond->functype() == Item_func::EQ_FUNC) ||
         (cond->functype() == Item_func::MULT_EQUAL_FUNC)) &&
        (real->type() == Item::FIELD_ITEM) &&
        ((Item_field*)real)->field->maybe_null())
      (*key_fields)->null_rejecting= true;
    else
      (*key_fields)->null_rejecting= false;
  }
  (*key_fields)->cond_guard= NULL;
  
  (*key_fields)->sj_pred_no= get_semi_join_select_list_index(field);
  (*key_fields)++;
}

/**
  Add possible keys to array of possible keys originated from a simple
  predicate.

    @param  key_fields     Pointer to add key, if usable
    @param  and_level      And level, to be stored in KEY_FIELD
    @param  cond           Condition predicate
    @param  field          Field used in comparision
    @param  eq_func        True if we used =, <=> or IS NULL
    @param  value          Value used for comparison with field
                           Is NULL for BETWEEN and IN    
    @param  usable_tables  Tables which can be used for key optimization
    @param  sargables      IN/OUT Array of found sargable candidates

  @note
    If field items f1 and f2 belong to the same multiple equality and
    a key is added for f1, the the same key is added for f2.

  @returns
    *key_fields is incremented if we stored a key in the array
*/

static void
add_key_equal_fields(KEY_FIELD **key_fields, uint and_level,
                     Item_func *cond, Item_field *field_item,
                     bool eq_func, Item **val,
                     uint num_values, table_map usable_tables,
                     SARGABLE_PARAM **sargables)
{
  Field *field= field_item->field;
  add_key_field(key_fields, and_level, cond, field,
                eq_func, val, num_values, usable_tables, sargables);
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
                      eq_func, val, num_values, usable_tables,
                      sargables);
      }
    }
  }
}


/**
  Check if an expression is a non-outer field.

  Checks if an expression is a field and belongs to the current select.

  @param   field  Item expression to check

  @return boolean
     @retval TRUE   the expression is a local field
     @retval FALSE  it's something else
*/

static bool
is_local_field (Item *field)
{
  return field->real_item()->type() == Item::FIELD_ITEM
    && !(field->used_tables() & OUTER_REF_TABLE_BIT)
    && !((Item_field *)field->real_item())->depended_from;
}


static void
add_key_fields(JOIN *join, KEY_FIELD **key_fields, uint *and_level,
               Item *cond, table_map usable_tables,
               SARGABLE_PARAM **sargables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    KEY_FIELD *org_key_fields= *key_fields;

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
        add_key_fields(join, key_fields, and_level, item, usable_tables,
                       sargables);
      for (; org_key_fields != *key_fields ; org_key_fields++)
	org_key_fields->level= *and_level;
    }
    else
    {
      (*and_level)++;
      add_key_fields(join, key_fields, and_level, li++, usable_tables,
                     sargables);
      Item *item;
      while ((item=li++))
      {
	KEY_FIELD *start_key_fields= *key_fields;
	(*and_level)++;
        add_key_fields(join, key_fields, and_level, item, usable_tables,
                       sargables);
	*key_fields=merge_key_fields(org_key_fields,start_key_fields,
				     *key_fields,++(*and_level));
      }
    }
    return;
  }

  /* 
    Subquery optimization: Conditions that are pushed down into subqueries
    are wrapped into Item_func_trig_cond. We process the wrapped condition
    but need to set cond_guard for Key_use elements generated from it.
  */
  {
    if (cond->type() == Item::FUNC_ITEM &&
        ((Item_func*)cond)->functype() == Item_func::TRIG_COND_FUNC)
    {
      Item *cond_arg= ((Item_func*)cond)->arguments()[0];
      if (!join->group_list && !join->order &&
          join->unit->item && 
          join->unit->item->substype() == Item_subselect::IN_SUBS &&
          !join->unit->is_union())
      {
        KEY_FIELD *save= *key_fields;
        add_key_fields(join, key_fields, and_level, cond_arg, usable_tables,
                       sargables);
        // Indicate that this ref access candidate is for subquery lookup:
        for (; save != *key_fields; save++)
          save->cond_guard= ((Item_func_trig_cond*)cond)->get_trig_var();
      }
      return;
    }
  }

  /* If item is of type 'field op field/constant' add it to key_fields */
  if (cond->type() != Item::FUNC_ITEM)
    return;
  Item_func *cond_func= (Item_func*) cond;
  switch (cond_func->select_optimize()) {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
  {
    Item **values;
    // BETWEEN, IN, NE
    if (is_local_field (cond_func->key_item()) &&
	!(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      values= cond_func->arguments()+1;
      if (cond_func->functype() == Item_func::NE_FUNC &&
        is_local_field (cond_func->arguments()[1]))
        values--;
      DBUG_ASSERT(cond_func->functype() != Item_func::IN_FUNC ||
                  cond_func->argument_count() != 2);
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->key_item()->real_item()),
                           0, values, 
                           cond_func->argument_count()-1,
                           usable_tables, sargables);
    }
    if (cond_func->functype() == Item_func::BETWEEN)
    {
      values= cond_func->arguments();
      for (uint i= 1 ; i < cond_func->argument_count() ; i++)
      {
        Item_field *field_item;
        if (is_local_field (cond_func->arguments()[i]))
        {
          field_item= (Item_field *) (cond_func->arguments()[i]->real_item());
          add_key_equal_fields(key_fields, *and_level, cond_func,
                               field_item, 0, values, 1, usable_tables, 
                               sargables);
        }
      }  
    }
    break;
  }
  case Item_func::OPTIMIZE_OP:
  {
    bool equal_func=(cond_func->functype() == Item_func::EQ_FUNC ||
		     cond_func->functype() == Item_func::EQUAL_FUNC);

    if (is_local_field (cond_func->arguments()[0]))
    {
      add_key_equal_fields(key_fields, *and_level, cond_func,
	                (Item_field*) (cond_func->arguments()[0])->real_item(),
		           equal_func,
                           cond_func->arguments()+1, 1, usable_tables,
                           sargables);
    }
    if (is_local_field (cond_func->arguments()[1]) &&
	cond_func->functype() != Item_func::LIKE_FUNC)
    {
      add_key_equal_fields(key_fields, *and_level, cond_func, 
                       (Item_field*) (cond_func->arguments()[1])->real_item(),
		           equal_func,
                           cond_func->arguments(),1,usable_tables,
                           sargables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (is_local_field (cond_func->arguments()[0]) &&
	!(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      Item *tmp=new Item_null;
      if (unlikely(!tmp))                       // Should never be true
	return;
      add_key_equal_fields(key_fields, *and_level, cond_func,
		    (Item_field*) (cond_func->arguments()[0])->real_item(),
		    cond_func->functype() == Item_func::ISNULL_FUNC,
			   &tmp, 1, usable_tables, sargables);
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
                      TRUE, &const_item, 1, usable_tables, sargables);
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
                          TRUE, (Item **) &item, 1, usable_tables,
                          sargables);
          }
        }
        it.rewind();
      }
    }
    break;
  }
}


static uint
max_part_bit(key_part_map bits)
{
  uint found;
  for (found=0; bits & 1 ; found++,bits>>=1) ;
  return found;
}

/*
  Add all keys with uses 'field' for some keypart
  If field->and_level != and_level then only mark key_part as const_part

  RETURN 
   0 - OK
   1 - Out of memory.
*/

static bool
add_key_part(DYNAMIC_ARRAY *keyuse_array,KEY_FIELD *key_field)
{
  Field *field=key_field->field;
  TABLE *form= field->table;

  if (key_field->eq_func && !(key_field->optimize & KEY_OPTIMIZE_EXISTS))
  {
    for (uint key=0 ; key < form->s->keys ; key++)
    {
      if (!(form->keys_in_use_for_query.is_set(key)))
	continue;
      if (form->key_info[key].flags & (HA_FULLTEXT | HA_SPATIAL))
	continue;    // ToDo: ft-keys in non-ft queries.   SerG

      uint key_parts= (uint) form->key_info[key].key_parts;
      for (uint part=0 ; part <  key_parts ; part++)
      {
	if (field->eq(form->key_info[key].key_part[part].field))
	{
          const Key_use keyuse(field->table,
                               key_field->val,
                               key_field->val->used_tables(),
                               key,
                               part,
                               key_field->optimize & KEY_OPTIMIZE_REF_OR_NULL,
                               (key_part_map) 1 << part,
                               ~(ha_rows) 0, // will be set in optimize_keyuse
                               key_field->null_rejecting,
                               key_field->cond_guard,
                               key_field->sj_pred_no);
          if (insert_dynamic(keyuse_array, &keyuse))
            return TRUE;
	}
      }
    }
  }
  return FALSE;
}


#define FT_KEYPART   (MAX_REF_PARTS+10)

static bool
add_ft_keys(DYNAMIC_ARRAY *keyuse_array,
            JOIN_TAB *stat,Item *cond,table_map usable_tables)
{
  Item_func_match *cond_func=NULL;

  if (!cond)
    return FALSE;

  if (cond->type() == Item::FUNC_ITEM)
  {
    Item_func *func=(Item_func *)cond;
    Item_func::Functype functype=  func->functype();
    if (functype == Item_func::FT_FUNC)
      cond_func=(Item_func_match *)cond;
    else if (func->arg_count == 2)
    {
      Item *arg0=(Item *)(func->arguments()[0]),
           *arg1=(Item *)(func->arguments()[1]);
      if (arg1->const_item() && arg1->cols() == 1 &&
           arg0->type() == Item::FUNC_ITEM &&
           ((Item_func *) arg0)->functype() == Item_func::FT_FUNC &&
          ((functype == Item_func::GE_FUNC && arg1->val_real() > 0) ||
           (functype == Item_func::GT_FUNC && arg1->val_real() >=0)))
        cond_func= (Item_func_match *) arg0;
      else if (arg0->const_item() &&
                arg1->type() == Item::FUNC_ITEM &&
                ((Item_func *) arg1)->functype() == Item_func::FT_FUNC &&
               ((functype == Item_func::LE_FUNC && arg0->val_real() > 0) ||
                (functype == Item_func::LT_FUNC && arg0->val_real() >=0)))
        cond_func= (Item_func_match *) arg1;
    }
  }
  else if (cond->type() == Item::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
      {
        if (add_ft_keys(keyuse_array,stat,item,usable_tables))
          return TRUE;
      }
    }
  }

  if (!cond_func || cond_func->key == NO_SUCH_KEY ||
      !(usable_tables & cond_func->table->map))
    return FALSE;

  const Key_use keyuse(cond_func->table,
                       cond_func,
                       cond_func->key_item()->used_tables(),
                       cond_func->key,
                       FT_KEYPART,
                       0,             // optimize
                       0,             // keypart_map
                       ~(ha_rows)0,   // ref_table_rows
                       false,         // null_rejecting
                       NULL,          // cond_guard
                       UINT_MAX);     // sj_pred_no
  return insert_dynamic(keyuse_array, &keyuse);
}


static int sort_keyuse(Key_use *a, Key_use *b)
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
  Add to KEY_FIELD array all 'ref' access candidates within nested join.

    This function populates KEY_FIELD array with entries generated from the 
    ON condition of the given nested join, and does the same for nested joins 
    contained within this nested join.

  @param[in]      nested_join_table   Nested join pseudo-table to process
  @param[in,out]  end                 End of the key field array
  @param[in,out]  and_level           And-level
  @param[in,out]  sargables           Array of found sargable candidates


  @note
    We can add accesses to the tables that are direct children of this nested 
    join (1), and are not inner tables w.r.t their neighbours (2).
    
    Example for #1 (outer brackets pair denotes nested join this function is 
    invoked for):
    @code
     ... LEFT JOIN (t1 LEFT JOIN (t2 ... ) ) ON cond
    @endcode
    Example for #2:
    @code
     ... LEFT JOIN (t1 LEFT JOIN t2 ) ON cond
    @endcode
    In examples 1-2 for condition cond, we can add 'ref' access candidates to 
    t1 only.
    Example #3:
    @code
     ... LEFT JOIN (t1, t2 LEFT JOIN t3 ON inner_cond) ON cond
    @endcode
    Here we can add 'ref' access candidates for t1 and t2, but not for t3.
*/

static void add_key_fields_for_nj(JOIN *join, TABLE_LIST *nested_join_table,
                                  KEY_FIELD **end, uint *and_level,
                                  SARGABLE_PARAM **sargables)
{
  List_iterator<TABLE_LIST> li(nested_join_table->nested_join->join_list);
  List_iterator<TABLE_LIST> li2(nested_join_table->nested_join->join_list);
  bool have_another = FALSE;
  table_map tables= 0;
  TABLE_LIST *table;
  DBUG_ASSERT(nested_join_table->nested_join);

  while ((table= li++) || (have_another && (li=li2, have_another=FALSE,
                                            (table= li++))))
  {
    if (table->nested_join)
    {
      if (!table->on_expr)
      {
        /* It's a semi-join nest. Walk into it as if it wasn't a nest */
        have_another= TRUE;
        li2= li;
        li= List_iterator<TABLE_LIST>(table->nested_join->join_list); 
      }
      else
        add_key_fields_for_nj(join, table, end, and_level, sargables);
    }
    else
      if (!table->on_expr)
        tables |= table->table->map;
  }
  if (nested_join_table->on_expr)
    add_key_fields(join, end, and_level, nested_join_table->on_expr, tables,
                   sargables);
}


/**
  Update keyuse array with all possible keys we can use to fetch rows.
  
  @param       thd 
  @param[out]  keyuse         Put here ordered array of Key_use structures
  @param       join_tab       Array in tablenr_order
  @param       tables         Number of tables in join
  @param       cond           WHERE condition (note that the function analyzes
                              join_tab[i]->on_expr too)
  @param       normal_tables  Tables not inner w.r.t some outer join (ones
                              for which we can make ref access based the WHERE
                              clause)
  @param       select_lex     current SELECT
  @param[out]  sargables      Array of found sargable candidates
      
   @retval
     0  OK
   @retval
     1  Out of memory.
*/

static bool
update_ref_and_keys(THD *thd, DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,
                    uint tables, Item *cond, COND_EQUAL *cond_equal,
                    table_map normal_tables, SELECT_LEX *select_lex,
                    SARGABLE_PARAM **sargables)
{
  uint	and_level,i,found_eq_constant;
  KEY_FIELD *key_fields, *end, *field;
  uint sz;
  uint m= max(select_lex->max_equal_elems,1);
  
  /* 
    We use the same piece of memory to store both  KEY_FIELD 
    and SARGABLE_PARAM structure.
    KEY_FIELD values are placed at the beginning this memory
    while  SARGABLE_PARAM values are put at the end.
    All predicates that are used to fill arrays of KEY_FIELD
    and SARGABLE_PARAM structures have at most 2 arguments
    except BETWEEN predicates that have 3 arguments and 
    IN predicates.
    This any predicate if it's not BETWEEN/IN can be used 
    directly to fill at most 2 array elements, either of KEY_FIELD
    or SARGABLE_PARAM type. For a BETWEEN predicate 3 elements
    can be filled as this predicate is considered as
    saragable with respect to each of its argument.
    An IN predicate can require at most 1 element as currently
    it is considered as sargable only for its first argument.
    Multiple equality can add  elements that are filled after
    substitution of field arguments by equal fields. There
    can be not more than select_lex->max_equal_elems such 
    substitutions.
  */ 
  sz= max(sizeof(KEY_FIELD),sizeof(SARGABLE_PARAM))*
      (((thd->lex->current_select->cond_count+1)*2 +
	thd->lex->current_select->between_count)*m+1);
  if (!(key_fields=(KEY_FIELD*)	thd->alloc(sz)))
    return TRUE; /* purecov: inspected */
  and_level= 0;
  field= end= key_fields;
  *sargables= (SARGABLE_PARAM *) key_fields + 
                (sz - sizeof((*sargables)[0].field))/sizeof(SARGABLE_PARAM);
  /* set a barrier for the array of SARGABLE_PARAM */
  (*sargables)[0].field= 0; 

  if (my_init_dynamic_array(keyuse, sizeof(Key_use), 20, 64))
    return TRUE;
  if (cond)
  {
    add_key_fields(join_tab->join, &end, &and_level, cond, normal_tables,
                   sargables);
    for (; field != end ; field++)
    {
      if (add_key_part(keyuse,field))
        return TRUE;
      /* Mark that we can optimize LEFT JOIN */
      if (field->val->type() == Item::NULL_ITEM &&
	  !field->field->real_maybe_null())
      {
        /*
          Example:
          SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a WHERE t2.a IS NULL;
          this just wants rows of t1 where t1.a does not exist in t2.
        */
        field->field->table->reginfo.not_exists_optimize=1;
      }
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
      add_key_fields(join_tab->join, &end, &and_level, 
                     *join_tab[i].on_expr_ref,
                     join_tab[i].table->map, sargables);
  }

  /* Process ON conditions for the nested joins */
  {
    List_iterator<TABLE_LIST> li(*join_tab->join->join_list);
    TABLE_LIST *table;
    while ((table= li++))
    {
      if (table->nested_join)
        add_key_fields_for_nj(join_tab->join, table, &end, &and_level, 
                              sargables);
    }
  }

  /* fill keyuse with found key parts */
  for ( ; field != end ; field++)
  {
    if (add_key_part(keyuse,field))
      return TRUE;
  }

  if (select_lex->ftfunc_list->elements)
  {
    if (add_ft_keys(keyuse,join_tab,cond,normal_tables))
      return TRUE;
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
    Key_use *save_pos, *use;

    my_qsort(keyuse->buffer, keyuse->elements, sizeof(Key_use),
             reinterpret_cast<qsort_cmp>(sort_keyuse));

    const Key_use key_end(NULL, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0);
    if (insert_dynamic(keyuse, &key_end)) // added for easy testing
      return TRUE;

    use= save_pos= dynamic_element(keyuse, 0, Key_use *);
    const Key_use *prev= &key_end;
    found_eq_constant=0;
    for (i=0 ; i < keyuse->elements-1 ; i++,use++)
    {
      if (!use->used_tables && use->optimize != KEY_OPTIMIZE_REF_OR_NULL)
	use->table->const_key_parts[use->key]|= use->keypart_map;
      if (use->keypart != FT_KEYPART)
      {
	if (use->key == prev->key && use->table == prev->table)
	{
	  if (prev->keypart+1 < use->keypart ||
	      (prev->keypart == use->keypart && found_eq_constant))
	    continue;				/* remove */
	}
	else if (use->keypart != 0)		// First found must be 0
	  continue;
      }

#ifdef HAVE_purify
      /* Valgrind complains about overlapped memcpy when save_pos==use. */
      if (save_pos != use)
#endif
        *save_pos= *use;
      prev=use;
      found_eq_constant= !use->used_tables;
      /* Save ptr to first use */
      if (!use->table->reginfo.join_tab->keyuse)
	use->table->reginfo.join_tab->keyuse=save_pos;
      use->table->reginfo.join_tab->checked_keys.set_bit(use->key);
      save_pos++;
    }
    i= (uint) (save_pos - (Key_use *)keyuse->buffer);
    (void) set_dynamic(keyuse, &key_end, i);
    keyuse->elements=i;
  }
  DBUG_EXECUTE("opt", print_keyuse_array(keyuse););
  return FALSE;
}

/**
  Update some values in keyuse for faster choose_plan() loop.
*/

static void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array)
{
  Key_use *end, *keyuse= dynamic_element(keyuse_array, 0, Key_use *);

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
	keyuse->ref_table_rows= max(tmp_table->file->stats.records, 100);
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


/**
  Check for the presence of AGGFN(DISTINCT a) queries that may be subject
  to loose index scan.


  Check if the query is a subject to AGGFN(DISTINCT) using loose index scan 
  (QUICK_GROUP_MIN_MAX_SELECT).
  Optionally (if out_args is supplied) will push the arguments of 
  AGGFN(DISTINCT) to the list

  @param      join       the join to check
  @param[out] out_args   list of aggregate function arguments
  @return                does the query qualify for indexed AGGFN(DISTINCT)
    @retval   true       it does
    @retval   false      AGGFN(DISTINCT) must apply distinct in it.
*/

bool
is_indexed_agg_distinct(JOIN *join, List<Item_field> *out_args)
{
  Item_sum **sum_item_ptr;
  bool result= false;

  if (join->tables != 1 ||                    /* reference more than 1 table */
      join->select_distinct ||                /* or a DISTINCT */
      join->select_lex->olap == ROLLUP_TYPE)  /* Check (B3) for ROLLUP */
    return false;

  if (join->make_sum_func_list(join->all_fields, join->fields_list, true))
    return false;

  for (sum_item_ptr= join->sum_funcs; *sum_item_ptr; sum_item_ptr++)
  {
    Item_sum *sum_item= *sum_item_ptr;
    Item *expr;
    /* aggregate is not AGGFN(DISTINCT) or more than 1 argument to it */
    switch (sum_item->sum_func())
    {
      case Item_sum::MIN_FUNC:
      case Item_sum::MAX_FUNC:
        continue;
      case Item_sum::COUNT_DISTINCT_FUNC: 
        break;
      case Item_sum::AVG_DISTINCT_FUNC:
      case Item_sum::SUM_DISTINCT_FUNC:
        if (sum_item->get_arg_count() == 1) 
          break;
        /* fall through */
      default: return false;
    }
    /*
      We arrive here for every COUNT(DISTINCT),AVG(DISTINCT) or SUM(DISTINCT).
      Collect the arguments of the aggregate functions to a list.
      We don't worry about duplicates as these will be sorted out later in 
      get_best_group_min_max 
    */
    for (uint i= 0; i < sum_item->get_arg_count(); i++)
    {
      expr= sum_item->get_arg(i);
      /* The AGGFN(DISTINCT) arg is not an attribute? */
      if (expr->real_item()->type() != Item::FIELD_ITEM)
        return false;

      /* 
        If we came to this point the AGGFN(DISTINCT) loose index scan
        optimization is applicable 
      */
      if (out_args)
        out_args->push_back((Item_field *) expr->real_item());
      result= true;
    }
  }
  return result;
}


/**
  Discover the indexes that can be used for GROUP BY or DISTINCT queries.

  If the query has a GROUP BY clause, find all indexes that contain all
  GROUP BY fields, and add those indexes to join->const_keys.

  If the query has a DISTINCT clause, find all indexes that contain all
  SELECT fields, and add those indexes to join->const_keys.
  This allows later on such queries to be processed by a
  QUICK_GROUP_MIN_MAX_SELECT.

  @param join
  @param join_tab

  @return
    None
*/

static void
add_group_and_distinct_keys(JOIN *join, JOIN_TAB *join_tab)
{
  List<Item_field> indexed_fields;
  List_iterator<Item_field> indexed_fields_it(indexed_fields);
  ORDER      *cur_group;
  Item_field *cur_item;
  key_map possible_keys;

  if (join->group_list)
  { /* Collect all query fields referenced in the GROUP clause. */
    for (cur_group= join->group_list; cur_group; cur_group= cur_group->next)
      (*cur_group->item)->walk(&Item::collect_item_field_processor, 0,
                               (uchar*) &indexed_fields);
  }
  else if (join->select_distinct)
  { /* Collect all query fields referenced in the SELECT clause. */
    List<Item> &select_items= join->fields_list;
    List_iterator<Item> select_items_it(select_items);
    Item *item;
    while ((item= select_items_it++))
      item->walk(&Item::collect_item_field_processor, 0,
                 (uchar*) &indexed_fields);
  }
  else if (is_indexed_agg_distinct(join, &indexed_fields))
  {
    join->sort_and_group= 1;
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

/** Save const tables first as used tables. */

static void
set_position(JOIN *join, uint idx, JOIN_TAB *table, Key_use *key)
{
  join->positions[idx].table= table;
  join->positions[idx].key=key;
  join->positions[idx].records_read=1.0;	/* This is a const table */
  join->positions[idx].ref_depend_map= 0;

  join->positions[idx].loosescan_key= MAX_KEY; /* Not a LooseScan */
  join->positions[idx].sj_strategy= SJ_OPT_NONE;
  join->positions[idx].use_join_buffer= FALSE;

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
  List_iterator<Item> li(sj_nest->nested_join->sj_outer_exprs);
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
  This is a class for considering possible loose index scan optimizations.
  It's usage pattern is as follows:
    best_access_path()
    {
       Loose_scan_opt opt;

       opt.init()
       for each index we can do ref access with
       {
         opt.next_ref_key();
         for each keyuse 
           opt.add_keyuse();
         opt.check_ref_access();
       }

       if (some criteria for range scans)
         opt.check_range_access();
       
       opt.get_best_option();
    }
*/

class Loose_scan_opt
{
private:
  /* All methods must check this before doing anything else */
  bool try_loosescan;

  /*
    If we consider (oe1, .. oeN) IN (SELECT ie1, .. ieN) then ieK=oeK is
    called sj-equality. If oeK depends only on preceding tables then such
    equality is called 'bound'.
  */
  ulonglong bound_sj_equalities;
 
  /* Accumulated properties of ref access we're now considering: */
  ulonglong handled_sj_equalities;
  key_part_map loose_scan_keyparts;
  uint max_loose_keypart;
  bool part1_conds_met;

  /*
    Use of quick select is a special case. Some of its properties:
  */
  uint quick_uses_applicable_index;
  uint quick_max_loose_keypart;
  
  /* Best loose scan method so far */
  uint   best_loose_scan_key;
  double best_loose_scan_cost;
  double best_loose_scan_records;
  Key_use *best_loose_scan_start_key;

  uint best_max_loose_keypart;

public:
  Loose_scan_opt() :
    try_loosescan(FALSE),
    quick_uses_applicable_index(FALSE)
  {
    /*
      We needn't initialize:
      bound_sj_equalities - protected by try_loosescan
      quick_max_loose_keypart - protected by quick_uses_applicable_index
      best_loose_scan_key - protected by best_loose_scan_cost != DBL_MAX
      best_loose_scan_records - same
      best_max_loose_keypart - same
      best_loose_scan_start_key - same
      Not initializing them causes compiler warnings, but using UNINIT_VAR()
      would cause a 2% CPU time loss in a 20-table plan search.
      So, until UNINIT_VAR(x) doesn't do x=0 for any C++ code, it's not used
      here.
    */
  }

  void init(JOIN *join, JOIN_TAB *s, table_map remaining_tables)
  {
    /*
      Discover the bound equalities. We need to do this if
        1. The next table is an SJ-inner table, and
        2. It is the first table from that semijoin, and
        3. We're not within a semi-join range (i.e. all semi-joins either have
           all or none of their tables in join_table_map), except
           s->emb_sj_nest (which we've just entered, see #2).
        4. All non-IN-equality correlation references from this sj-nest are 
           bound
        5. But some of the IN-equalities aren't (so this can't be handled by 
           FirstMatch strategy)
    */
    best_loose_scan_cost= DBL_MAX;
    if (s->emb_sj_nest && !join->emb_sjm_nest &&                        // (1)
        s->emb_sj_nest->nested_join->sj_inner_exprs.elements < 64 && 
        ((remaining_tables & s->emb_sj_nest->sj_inner_tables) ==        // (2)
         s->emb_sj_nest->sj_inner_tables) &&                            // (2)
        join->cur_sj_inner_tables == 0 &&                               // (3)
        !(remaining_tables & 
          s->emb_sj_nest->nested_join->sj_corr_tables) &&               // (4)
        (remaining_tables & s->emb_sj_nest->nested_join->sj_depends_on) &&// (5)
        join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_LOOSE_SCAN))
    {
      /* This table is an LooseScan scan candidate */
      bound_sj_equalities= get_bound_sj_equalities(s->emb_sj_nest, 
                                                   remaining_tables);
      try_loosescan= TRUE;
      DBUG_PRINT("info", ("Will try LooseScan scan, bound_map=%llx",
                          (longlong)bound_sj_equalities));
    }
  }

  void next_ref_key()
  {
    handled_sj_equalities=0;
    loose_scan_keyparts= 0;
    max_loose_keypart= 0;
    part1_conds_met= FALSE;
  }
  
  void add_keyuse(table_map remaining_tables, Key_use *keyuse)
  {
    if (try_loosescan && keyuse->sj_pred_no != UINT_MAX)
    {
      if (!(remaining_tables & keyuse->used_tables))
      {
        /* 
          This allows to use equality propagation to infer that some 
          sj-equalities are bound.
        */
        bound_sj_equalities |= 1ULL << keyuse->sj_pred_no;
      }
      else
      {
        handled_sj_equalities |= 1ULL << keyuse->sj_pred_no;
        loose_scan_keyparts |= ((key_part_map)1) << keyuse->keypart;
        set_if_bigger(max_loose_keypart, keyuse->keypart);
      }
    }
  }

  bool have_a_case() { return test(handled_sj_equalities); }

  void check_ref_access_part1(JOIN_TAB *s, uint key, Key_use *start_key,
                              table_map found_part)
  {
    /*
      Check if we can use LooseScan semi-join strategy. We can if
      1. This is the right table at right location
      2. All IN-equalities are either
         - "bound", ie. the outer_expr part refers to the preceding tables
         - "handled", ie. covered by the index we're considering
      3. Index order allows to enumerate subquery's duplicate groups in
         order. This happens when the index definition matches this
         pattern:

           (handled_col|bound_col)* (other_col|bound_col)

    */
    if (try_loosescan &&                                                // (1)
        (handled_sj_equalities | bound_sj_equalities) ==                // (2)
        PREV_BITS(ulonglong,
                s->emb_sj_nest->nested_join->sj_inner_exprs.elements)&& // (2)
        (PREV_BITS(key_part_map, max_loose_keypart+1) &                 // (3)
         (found_part | loose_scan_keyparts)) ==                         // (3)
         (found_part | loose_scan_keyparts) &&                          // (3)
        !key_uses_partial_cols(s->table, key))
    {
      /* Ok, can use the strategy */
      part1_conds_met= TRUE;
      if (s->quick && s->quick->index == key && 
          s->quick->get_type() == QUICK_SELECT_I::QS_TYPE_RANGE)
      {
        quick_uses_applicable_index= TRUE;
        quick_max_loose_keypart= max_loose_keypart;
      }
      DBUG_PRINT("info", ("Can use LooseScan scan"));

      /* 
        Check if this is a confluent where there are no usable bound
        IN-equalities, e.g. we have

          outer_expr IN (SELECT innertbl.key FROM ...) 
        
        and outer_expr cannot be evaluated yet, so it's actually full
        index scan and not a ref access
      */
      if (!(found_part & 1 ) && /* no usable ref access for 1st key part */
          s->table->covering_keys.is_set(key))
      {
        DBUG_PRINT("info", ("Can use full index scan for LooseScan"));
        
        /* Calculate the cost of complete loose index scan.  */
        double records= rows2double(s->table->file->stats.records);

        /* The cost is entire index scan cost (divided by 2) */
        double read_time= s->table->file->index_only_read_time(key, records);

        /*
          Now find out how many different keys we will get (for now we
          ignore the fact that we have "keypart_i=const" restriction for
          some key components, that may make us think think that loose
          scan will produce more distinct records than it actually will)
        */
        ulong rpc;
        if ((rpc= s->table->key_info[key].rec_per_key[max_loose_keypart]))
          records= records / rpc;

        // TODO: previous version also did /2
        if (read_time < best_loose_scan_cost)
        {
          best_loose_scan_key= key;
          best_loose_scan_cost= read_time;
          best_loose_scan_records= records;
          best_max_loose_keypart= max_loose_keypart;
          best_loose_scan_start_key= start_key;
        }
      }
    }
  }
  
  void check_ref_access_part2(uint key, Key_use *start_key, double records,
                              double read_time)
  {
    if (part1_conds_met && read_time < best_loose_scan_cost)
    {
      /* TODO use rec-per-key-based fanout calculations */
      best_loose_scan_key= key;
      best_loose_scan_cost= read_time;
      best_loose_scan_records= records;
      best_max_loose_keypart= max_loose_keypart;
      best_loose_scan_start_key= start_key;
    }
  }

  void check_range_access(JOIN *join, uint idx, QUICK_SELECT_I *quick)
  {
    /* TODO: this the right part restriction: */
    if (quick_uses_applicable_index && idx == join->const_tables && 
        quick->read_time < best_loose_scan_cost)
    {
      best_loose_scan_key= quick->index;
      best_loose_scan_cost= quick->read_time;
      /* this is ok because idx == join->const_tables */
      best_loose_scan_records= rows2double(quick->records);
      best_max_loose_keypart= quick_max_loose_keypart;
      best_loose_scan_start_key= NULL;
    }
  }

  void save_to_position(JOIN_TAB *tab, POSITION *pos)
  {
    pos->read_time=       best_loose_scan_cost;
    if (best_loose_scan_cost != DBL_MAX)
    {
      pos->records_read=    best_loose_scan_records;
      pos->key=             best_loose_scan_start_key;
      pos->loosescan_key=   best_loose_scan_key;
      pos->loosescan_parts= best_max_loose_keypart + 1;
      pos->use_join_buffer= FALSE;
      pos->table=           tab;
      // todo need ref_depend_map ?
      DBUG_PRINT("info", ("Produced a LooseScan plan, key %s, %s",
                          tab->table->key_info[best_loose_scan_key].name,
                          best_loose_scan_start_key? "(ref access)":
                                                     "(range/index access)"));
    }
  }
};


/**
  Find the best access path for an extension of a partial execution
  plan and add this path to the plan.

  The function finds the best access path to table 's' from the passed
  partial plan where an access path is the general term for any means to
  access the data in 's'. An access path may use either an index or a scan,
  whichever is cheaper. The input partial plan is passed via the array
  'join->positions' of length 'idx'. The chosen access method for 's' and its
  cost are stored in 'join->positions[idx]'.

  @param join             pointer to the structure providing all context info
                          for the query
  @param s                the table to be joined by the function
  @param thd              thread for the connection that submitted the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param idx              the length of the partial plan
  @param disable_jbuf     TRUE<=> Don't use join buffering
  @param record_count     estimate for the number of records returned by the
                          partial plan
  @param pos              OUT Table access plan
  @param loose_scan_pos   OUT Table plan that uses loosescan, or set cost to 
                              DBL_MAX if not possible.

  @return
    None
*/

static void
best_access_path(JOIN      *join,
                 JOIN_TAB  *s,
                 table_map remaining_tables,
                 uint      idx,
                 bool      disable_jbuf,
                 double    record_count,
                 POSITION *pos,
                 POSITION *loose_scan_pos)
{
  THD *thd= join->thd;
  Key_use *best_key=        NULL;
  uint best_max_key_part=   0;
  my_bool found_constraint= 0;
  double best=              DBL_MAX;
  double best_time=         DBL_MAX;
  double records=           DBL_MAX;
  /* Holds the current number of records from the range optimizer, if any */
  double quick_records=     DBL_MAX;
  /* 
     When a key from the range optimizer matches more parts than a ref key, the
     estimates from these sources are not comparable.
  */
  double best_quick_records= DBL_MAX;
  table_map best_ref_depends_map= 0;
  double tmp;
  bool best_uses_jbuf= FALSE;

  Loose_scan_opt loose_scan_opt;
  DBUG_ENTER("best_access_path");
  
  loose_scan_opt.init(join, s, remaining_tables);
  
  /*
    This isn't unlikely at all, but unlikely() cuts 6% CPU time on a 20-table
    search when s->keyuse==0, and has no cost when s->keyuse!=0.
  */
  if (unlikely(s->keyuse != NULL))
  {                                            /* Use key if possible */
    TABLE *table= s->table;
    Key_use *keyuse;
    double best_records= DBL_MAX;
    uint max_key_part=0;

    /* Test how we can use keys */
    ha_rows rec=
      s->records/MATCHING_ROWS_IN_OTHER_TABLE;  // Assumed records/key
    for (keyuse=s->keyuse ; keyuse->table == table ;)
    {
      key_part_map found_part= 0;
      table_map found_ref= 0;
      uint key= keyuse->key;
      KEY *keyinfo= table->key_info+key;
      bool ft_key=  (keyuse->keypart == FT_KEYPART);
      /* Bitmap of keyparts where the ref access is over 'keypart=const': */
      key_part_map const_part= 0;
      /* The or-null keypart in ref-or-null access: */
      key_part_map ref_or_null_part= 0;

      /* Calculate how many key segments of the current key we can use */
      Key_use *start_key= keyuse;

      loose_scan_opt.next_ref_key();
      DBUG_PRINT("info", ("Considering ref access on key %s",
                          keyuse->table->key_info[keyuse->key].name));

      /* 
         True if we find some keys from the range optimizer that match more
         key parts than the best ref key. We then choose the best range key.
      */
      bool quick_matches_more_parts= false;
      
      do /* For each keypart */
      {
        uint keypart= keyuse->keypart;
        table_map best_part_found_ref= 0;
        double best_prev_record_reads= DBL_MAX;
        
        do /* For each way to access the keypart */
        {
          /*
            if 1. expression doesn't refer to forward tables
               2. we won't get two ref-or-null's
          */
          if (!(remaining_tables & keyuse->used_tables) &&
              !(ref_or_null_part && (keyuse->optimize &
                                     KEY_OPTIMIZE_REF_OR_NULL)))
          {
            found_part|= keyuse->keypart_map;
            if (!(keyuse->used_tables & ~join->const_table_map))
              const_part|= keyuse->keypart_map;

            double tmp2= prev_record_reads(join, idx, (found_ref |
                                                      keyuse->used_tables));
            if (tmp2 < best_prev_record_reads)
            {
              best_part_found_ref= keyuse->used_tables & ~join->const_table_map;
              best_prev_record_reads= tmp2;
            }
            if (rec > keyuse->ref_table_rows)
              rec= keyuse->ref_table_rows;
	    /*
	      If there is one 'key_column IS NULL' expression, we can
	      use this ref_or_null optimisation of this field
	    */
            if (keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL)
              ref_or_null_part |= keyuse->keypart_map;
          }
          loose_scan_opt.add_keyuse(remaining_tables, keyuse);
          keyuse++;
        } while (keyuse->table == table && keyuse->key == key &&
                 keyuse->keypart == keypart);
	found_ref|= best_part_found_ref;
      } while (keyuse->table == table && keyuse->key == key);

      /*
        Assume that that each key matches a proportional part of table.
      */
      if (!found_part && !ft_key && !loose_scan_opt.have_a_case())
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
        tmp= prev_record_reads(join, idx, found_ref);
        records= 1.0;
      }
      else
      {
        found_constraint= test(found_part);
        loose_scan_opt.check_ref_access_part1(s, key, start_key, found_part);

        /* Check if we found full key */
        if (found_part == PREV_BITS(uint,keyinfo->key_parts) &&
            !ref_or_null_part)
        {                                         /* use eq key */
          max_key_part= (uint) ~0;
          if ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
          {
            tmp = prev_record_reads(join, idx, found_ref);
            records=1.0;
          }
          else
          {
            if (!found_ref)
            {                                     /* We found a const key */
              /*
                ReuseRangeEstimateForRef-1:
                We get here if we've found a ref(const) (c_i are constants):
                  "(keypart1=c1) AND ... AND (keypartN=cN)"   [ref_const_cond]
                
                If range optimizer was able to construct a "range" 
                access on this index, then its condition "quick_cond" was
                eqivalent to ref_const_cond (*), and we can re-use E(#rows)
                from the range optimizer.
                
                Proof of (*): By properties of range and ref optimizers 
                quick_cond will be equal or tighther than ref_const_cond. 
                ref_const_cond already covers "smallest" possible interval - 
                a singlepoint interval over all keyparts. Therefore, 
                quick_cond is equivalent to ref_const_cond (if it was an 
                empty interval we wouldn't have got here).
              */
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
              /*
                ReuseRangeEstimateForRef-2:  We get here if we could not reuse
                E(#rows) from range optimizer. Make another try:
                
                If range optimizer produced E(#rows) for a prefix of the ref
                access we're considering, and that E(#rows) is lower then our
                current estimate, make an adjustment. The criteria of when we
                can make an adjustment is a special case of the criteria used
                in ReuseRangeEstimateForRef-3.
              */
              if (table->quick_keys.is_set(key) &&
                  (const_part & ((1 << table->quick_key_parts[key])-1)) ==
                  (((key_part_map)1 << table->quick_key_parts[key])-1) &&
                  table->quick_n_ranges[key] == 1 &&
                  records > (double) table->quick_rows[key])
              {
                records= (double) table->quick_rows[key];
              }
            }
            /* Limit the number of matched rows */
            tmp= records;
            set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
            if (table->covering_keys.is_set(key))
            {
              /* we can use only index tree */
              tmp= record_count * table->file->index_only_read_time(key, tmp);
            }
            else
              tmp= record_count*min(tmp,s->worst_seeks);
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
            max_key_part= max_part_bit(found_part);
            /*
              ReuseRangeEstimateForRef-3:
              We're now considering a ref[or_null] access via
              (t.keypart1=e1 AND ... AND t.keypartK=eK) [ OR  
              (same-as-above but with one cond replaced 
               with "t.keypart_i IS NULL")]  (**)
              
              Try re-using E(#rows) from "range" optimizer:
              We can do so if "range" optimizer used the same intervals as
              in (**). The intervals used by range optimizer may be not 
              available at this point (as "range" access might have choosen to
              create quick select over another index), so we can't compare
              them to (**). We'll make indirect judgements instead.
              The sufficient conditions for re-use are:
              (C1) All e_i in (**) are constants, i.e. found_ref==FALSE. (if
                   this is not satisfied we have no way to know which ranges
                   will be actually scanned by 'ref' until we execute the 
                   join)
              (C2) max #key parts in 'range' access == K == max_key_part (this
                   is apparently a necessary requirement)

              We also have a property that "range optimizer produces equal or 
              tighter set of scan intervals than ref(const) optimizer". Each
              of the intervals in (**) are "tightest possible" intervals when 
              one limits itself to using keyparts 1..K (which we do in #2).              
              From here it follows that range access used either one, or
              both of the (I1) and (I2) intervals:
              
               (t.keypart1=c1 AND ... AND t.keypartK=eK)  (I1) 
               (same-as-above but with one cond replaced  
                with "t.keypart_i IS NULL")               (I2)

              The remaining part is to exclude the situation where range
              optimizer used one interval while we're considering
              ref-or-null and looking for estimate for two intervals. This
              is done by last limitation:

              (C3) "range optimizer used (have ref_or_null?2:1) intervals"
            */
            if (table->quick_keys.is_set(key) && !found_ref &&          //(C1)
                table->quick_key_parts[key] == max_key_part &&          //(C2)
                table->quick_n_ranges[key] == 1+test(ref_or_null_part)) //(C3)
            {
              tmp= records= (double) table->quick_rows[key];
            }
            else
            {
              /* Check if we have statistic about the distribution */
              if ((records= keyinfo->rec_per_key[max_key_part-1]))
              {
                /* 
                  Fix for the case where the index statistics is too
                  optimistic: If 
                  (1) We're considering ref(const) and there is quick select
                      on the same index, 
                  (2) and that quick select uses more keyparts (i.e. it will
                      scan equal/smaller interval then this ref(const))
                  (3) and E(#rows) for quick select is higher then our
                      estimate,
                  Then 
                    We'll use E(#rows) from quick select.

                  Q: Why do we choose to use 'ref'? Won't quick select be
                  cheaper in some cases ?
                  TODO: figure this out and adjust the plan choice if needed.
                */
                if (!found_ref && table->quick_keys.is_set(key) &&    // (1)
                    table->quick_key_parts[key] > max_key_part &&     // (2)
                    records < (double)table->quick_rows[key])         // (3)
                {
                  records= quick_records= (double)table->quick_rows[key];
                  quick_matches_more_parts= true;                  
                }

                tmp= records;
              }
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

              if (ref_or_null_part)
              {
                /* We need to do two key searches to find key */
                tmp *= 2.0;
                records *= 2.0;
              }

              /*
                ReuseRangeEstimateForRef-4:  We get here if we could not reuse
                E(#rows) from range optimizer. Make another try:
                
                If range optimizer produced E(#rows) for a prefix of the ref 
                access we're considering, and that E(#rows) is lower then our
                current estimate, make the adjustment.

                The decision whether we can re-use the estimate from the range
                optimizer is the same as in ReuseRangeEstimateForRef-3,
                applied to first table->quick_key_parts[key] key parts.
              */
              if (table->quick_keys.is_set(key) &&
                  table->quick_key_parts[key] <= max_key_part &&
                  const_part & (1 << table->quick_key_parts[key]) &&
                  table->quick_n_ranges[key] == 1 + test(ref_or_null_part &
                                                         const_part) &&
                  records > (double) table->quick_rows[key])
              {
                tmp= records= (double) table->quick_rows[key];
              }
            }

            /* Limit the number of matched rows */
            set_if_smaller(tmp, (double) thd->variables.max_seeks_for_key);
            if (table->covering_keys.is_set(key))
            {
              /* we can use only index tree */
              tmp= record_count * table->file->index_only_read_time(key, tmp);
            }
            else
              tmp= record_count * min(tmp,s->worst_seeks);
          }
          else
            tmp= best_time;                    // Do nothing
        }
        loose_scan_opt.check_ref_access_part2(key, start_key, records, tmp);

      } /* not ft_key */
      if (tmp < best_time - records/(double) TIME_FOR_COMPARE ||
          (quick_matches_more_parts && 
           quick_records < best_quick_records))
      {
        best_quick_records = quick_records;
        best_time= tmp + records/(double) TIME_FOR_COMPARE;
        best= tmp;
        best_records= records;
        best_key= start_key;
        best_max_key_part= max_key_part;
        best_ref_depends_map= found_ref;
      }
    } /* for each key */
    records= best_records;
  }

  /*
    Don't test table scan if it can't be better.
    Prefer key lookup if we would use the same key for scanning.

    Don't do a table scan on InnoDB tables, if we can read the used
    parts of the row from any of the used index.
    This is because table scans uses index and we would not win
    anything by using a table scan.

    A word for word translation of the below if-statement in sergefp's
    understanding: we check if we should use table scan if:
    (1) The found 'ref' access produces more records than a table scan
        (or index scan, or quick select), or 'ref' is more expensive than
        any of them.
    (2) This doesn't hold: the best way to perform table scan is to to perform
        'range' access using index IDX, and the best way to perform 'ref' 
        access is to use the same index IDX, with the same or more key parts.
        (note: it is not clear how this rule is/should be extended to 
        index_merge quick selects)
    (3) See above note about InnoDB.
    (4) NOT ("FORCE INDEX(...)" is used for table and there is 'ref' access
             path, but there is no quick select)
        If the condition in the above brackets holds, then the only possible
        "table scan" access method is ALL/index (there is no quick select).
        Since we have a 'ref' access path, and FORCE INDEX instructs us to
        choose it over ALL/index, there is no need to consider a full table
        scan.
  */
  if ((records >= s->found_records || best > s->read_time) &&            // (1)
      !(s->quick && best_key && s->quick->index == best_key->key &&      // (2)
        best_max_key_part >= s->table->quick_key_parts[best_key->key]) &&// (2)
      !((s->table->file->ha_table_flags() & HA_TABLE_SCAN_ON_INDEX) &&   // (3)
        ! s->table->covering_keys.is_clear_all() && best_key && !s->quick) &&// (3)
      !(s->table->force_index && best_key && !s->quick))                 // (4)
  {                                             // Check full join
    ha_rows rnd_records= s->found_records;
    /*
      If there is a filtering condition on the table (i.e. ref analyzer found
      at least one "table.keyXpartY= exprZ", where exprZ refers only to tables
      preceding this table in the join order we're now considering), then 
      assume that 25% of the rows will be filtered out by this condition.

      This heuristic is supposed to force tables used in exprZ to be before
      this table in join order.
    */
    if (found_constraint)
      rnd_records-= rnd_records/4;

    /*
      If applicable, get a more accurate estimate. Don't use the two
      heuristics at once.
    */
    if (s->table->quick_condition_rows != s->found_records)
      rnd_records= s->table->quick_condition_rows;

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
        TODO: 
        We take into account possible use of join cache for ALL/index
        access (see first else-branch below), but we don't take it into 
        account here for range/index_merge access. Find out why this is so.
      */
      tmp= record_count *
        (s->quick->read_time +
         (s->found_records - rnd_records)/(double) TIME_FOR_COMPARE);

      loose_scan_opt.check_range_access(join, idx, s->quick);
    }
    else
    {
      /* Estimate cost of reading table. */
      if (s->table->force_index && !best_key)
        tmp= s->table->file->read_time(s->ref.key, 1, s->records);
      else
        tmp= s->table->file->scan_time();

      if ((s->table->map & join->outer_join) || disable_jbuf)     // Can't use join cache
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
        /*
          We read the table as many times as join buffer becomes full.
          It would be more exact to round the result of the division with
          floor(), but that takes 5% of time in a 20-table query plan search.
        */
        tmp*= (1.0 + ((double) cache_record_length(join,idx) *
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
      /* range/index_merge/ALL/index access method are "independent", so: */
      best_ref_depends_map= 0;
      best_uses_jbuf= test(!disable_jbuf && !((s->table->map & 
                                               join->outer_join)));
    }
  }
  
  /* Update the cost information for the current partial plan */
  pos->records_read= records;
  pos->read_time=    best;
  pos->key=          best_key;
  pos->table=        s;
  pos->ref_depend_map= best_ref_depends_map;
  pos->loosescan_key= MAX_KEY;
  pos->use_join_buffer= best_uses_jbuf;
   
  loose_scan_opt.save_to_position(s, loose_scan_pos);

  if (!best_key &&
      idx == join->const_tables &&
      s->table == join->sort_by_table &&
      join->unit->select_limit_cnt >= records)
    join->sort_by_table= (TABLE*) 1;  // Must use temporary table

  DBUG_VOID_RETURN;
}


/**
  Selects and invokes a search strategy for an optimal query plan.

  The function checks user-configurable parameters that control the search
  strategy for an optimal plan, selects the search method and then invokes
  it. Each specific optimization procedure stores the final optimal plan in
  the array 'join->best_positions', and the cost of the plan in
  'join->best_read'.

  @param join         pointer to the structure providing all context info for
                      the query
  @param join_tables  set of the tables in the query

  @retval
    FALSE       ok
  @retval
    TRUE        Fatal error
*/

static bool
choose_plan(JOIN *join, table_map join_tables)
{
  uint search_depth= join->thd->variables.optimizer_search_depth;
  uint prune_level=  join->thd->variables.optimizer_prune_level;
  bool straight_join= test(join->select_options & SELECT_STRAIGHT_JOIN);
  DBUG_ENTER("choose_plan");

  join->cur_embedding_map= 0;
  reset_nj_counters(join->join_list);
  qsort2_cmp jtab_sort_func;

  if (join->emb_sjm_nest)
  {
    /* We're optimizing semi-join materialization nest, so put the 
       tables from this semi-join as first
    */
    jtab_sort_func= join_tab_cmp_embedded_first;
  }
  else
  {
    /*
      if (SELECT_STRAIGHT_JOIN option is set)
        reorder tables so dependent tables come after tables they depend 
        on, otherwise keep tables in the order they were specified in the query 
      else
        Apply heuristic: pre-sort all access plans with respect to the number of
        records accessed.
    */
    jtab_sort_func= straight_join ? join_tab_cmp_straight : join_tab_cmp;
  }
  my_qsort2(join->best_ref + join->const_tables,
            join->tables - join->const_tables, sizeof(JOIN_TAB*),
            jtab_sort_func, (void*)join->emb_sjm_nest);
  join->cur_sj_inner_tables= 0;

  if (straight_join)
  {
    optimize_straight_join(join, join_tables);
  }
  else
  {
    if (search_depth == 0)
      /* Automatically determine a reasonable value for 'search_depth' */
      search_depth= determine_search_depth(join);
    if (greedy_search(join, join_tables, search_depth, prune_level))
      DBUG_RETURN(TRUE);
  }

  /* 
    Store the cost of this query into a user variable
    Don't update last_query_cost for statements that are not "flat joins" :
    i.e. they have subqueries, unions or call stored procedures.
    TODO: calculate a correct cost for a query with subqueries and UNIONs.
  */
  if (join->thd->lex->is_single_level_stmt())
    join->thd->status_var.last_query_cost= join->best_read;
  DBUG_RETURN(FALSE);
}


/**
  Compare two JOIN_TAB objects based on the number of accessed records.

  @param ptr1 pointer to first JOIN_TAB object
  @param ptr2 pointer to second JOIN_TAB object

  NOTES
    The order relation implemented by join_tab_cmp() is not transitive,
    i.e. it is possible to choose such a, b and c that (a < b) && (b < c)
    but (c < a). This implies that result of a sort using the relation
    implemented by join_tab_cmp() depends on the order in which
    elements are compared, i.e. the result is implementation-specific.
    Example:
      a: dependent = 0x0 table->map = 0x1 found_records = 3 ptr = 0x907e6b0
      b: dependent = 0x0 table->map = 0x2 found_records = 3 ptr = 0x907e838
      c: dependent = 0x6 table->map = 0x10 found_records = 2 ptr = 0x907ecd0
     
  @retval
    1  if first is bigger
  @retval
    -1  if second is bigger
  @retval
    0  if equal
*/

static int
join_tab_cmp(const void *dummy, const void* ptr1, const void* ptr2)
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


/**
  Same as join_tab_cmp, but for use with SELECT_STRAIGHT_JOIN.
*/

static int
join_tab_cmp_straight(const void *dummy, const void* ptr1, const void* ptr2)
{
  JOIN_TAB *jt1= *(JOIN_TAB**) ptr1;
  JOIN_TAB *jt2= *(JOIN_TAB**) ptr2;
 
  /*
    We don't do subquery flattening if the parent or child select has
    STRAIGHT_JOIN modifier. It is complicated to implement and the semantics
    is hardly useful.
  */
  DBUG_ASSERT(!jt1->emb_sj_nest);
  DBUG_ASSERT(!jt2->emb_sj_nest);

  if (jt1->dependent & jt2->table->map)
    return 1;
  if (jt2->dependent & jt1->table->map)
    return -1;
  return jt1 > jt2 ? 1 : (jt1 < jt2 ? -1 : 0);
}


/*
  Same as join_tab_cmp but tables from within the given semi-join nest go 
  first. Used when the optimizing semi-join materialization nests.
*/

static int
join_tab_cmp_embedded_first(const void *emb,  const void* ptr1, const void* ptr2)
{
  const TABLE_LIST *emb_nest= (TABLE_LIST*) emb;
  JOIN_TAB *jt1= *(JOIN_TAB**) ptr1;
  JOIN_TAB *jt2= *(JOIN_TAB**) ptr2;

  if (jt1->emb_sj_nest == emb_nest && jt2->emb_sj_nest != emb_nest)
    return -1;
  if (jt1->emb_sj_nest != emb_nest && jt2->emb_sj_nest == emb_nest)
    return 1;

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


/**
  Heuristic procedure to automatically guess a reasonable degree of
  exhaustiveness for the greedy search procedure.

  The procedure estimates the optimization time and selects a search depth
  big enough to result in a near-optimal QEP, that doesn't take too long to
  find. If the number of tables in the query exceeds some constant, then
  search_depth is set to this constant.

  @param join   pointer to the structure providing all context info for
                the query

  @note
    This is an extremely simplistic implementation that serves as a stub for a
    more advanced analysis of the join. Ideally the search depth should be
    determined by learning from previous query optimizations, because it will
    depend on the CPU power (and other factors).

  @todo
    this value should be determined dynamically, based on statistics:
    uint max_tables_for_exhaustive_opt= 7;

  @todo
    this value could be determined by some mapping of the form:
    depth : table_count -> [max_tables_for_exhaustive_opt..MAX_EXHAUSTIVE]

  @return
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


/**
  Select the best ways to access the tables in a query without reordering them.

    Find the best access paths for each query table and compute their costs
    according to their order in the array 'join->best_ref' (thus without
    reordering the join tables). The function calls sequentially
    'best_access_path' for each table in the query to select the best table
    access method. The final optimal plan is stored in the array
    'join->best_positions', and the corresponding cost in 'join->best_read'.

  @param join          pointer to the structure providing all context info for
                       the query
  @param join_tables   set of the tables in the query

  @note
    This function can be applied to:
    - queries with STRAIGHT_JOIN
    - internally to compute the cost of an arbitrary QEP
  @par
    Thus 'optimize_straight_join' can be used at any stage of the query
    optimization process to finalize a QEP as it is.
*/

static void
optimize_straight_join(JOIN *join, table_map join_tables)
{
  JOIN_TAB *s;
  uint idx= join->const_tables;
  double    record_count= 1.0;
  double    read_time=    0.0;
  POSITION  loose_scan_pos;
 
  for (JOIN_TAB **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    /*
      Dependency computation (make_join_statistics()) and proper ordering
      based on them (join_tab_cmp*) guarantee that this order is compatible
      with execution, check it:
    */
    DBUG_ASSERT(!check_interleaving_with_nj(s));
    /* Find the best access method from 's' to the current partial plan */
    best_access_path(join, s, join_tables, idx, FALSE, record_count,
                     join->positions + idx, &loose_scan_pos);

    /* compute the cost of the new plan extended with 's' */
    record_count*= join->positions[idx].records_read;
    read_time+=    join->positions[idx].read_time;
    advance_sj_state(join, join_tables, s, idx, &record_count, &read_time,
                     &loose_scan_pos);

    join_tables&= ~(s->table->map);
    ++idx;
  }

  read_time+= record_count / (double) TIME_FOR_COMPARE;
  if (join->sort_by_table &&
      join->sort_by_table != join->positions[join->const_tables].table->table)
    read_time+= record_count;  // We have to make a temp table
  memcpy((uchar*) join->best_positions, (uchar*) join->positions,
         sizeof(POSITION)*idx);
  join->best_read= read_time;
}


/**
  Check whether a semijoin materialization strategy is allowed for
  the current (semi)join table order.

  @param join              Join object
  @param remaining_tables  Tables that have not yet been added to the join plan
  @param tab               Join tab of the table being considered
  @param idx               Index of table with join tab "tab"

  @retval SJ_OPT_NONE               - Materialization not applicable
  @retval SJ_OPT_MATERIALIZE_LOOKUP - Materialization with lookup applicable
  @retval SJ_OPT_MATERIALIZE_SCAN   - Materialization with scan applicable

  @details
  The function checks applicability of both MaterializeLookup and
  MaterializeScan strategies.
  No checking is made until "tab" is pointing to the last inner table
  of a semijoin nest that can be executed using materialization -
  for all other cases SJ_OPT_NONE is returned.

  MaterializeLookup and MaterializeScan are both applicable in the following
  two cases:

   1. There are no correlated outer tables, or
   2. There are correlated outer tables within the prefix only.

  In this case, MaterializeLookup is returned based on a heuristic decision.
*/

static int
semijoin_order_allows_materialization(const JOIN *join,
                                      table_map remaining_tables,
                                      const JOIN_TAB *tab, uint idx)
{
  DBUG_ASSERT(!(remaining_tables & tab->table->map));
  /*
   Check if 
    1. We're in a semi-join nest that can be run with SJ-materialization
    2. All the tables from the subquery are in the prefix
  */
  const TABLE_LIST *emb_sj_nest= tab->emb_sj_nest;
  if (!emb_sj_nest ||
      !emb_sj_nest->nested_join->sjm.positions ||
      (remaining_tables & emb_sj_nest->sj_inner_tables))
    return SJ_OPT_NONE;

  /* 
    Walk back and check if all immediately preceding tables are from
    this semi-join.
  */
  const uint n_tables= my_count_bits(emb_sj_nest->sj_inner_tables);
  for (uint i= 1; i < n_tables ; i++)
  {
    if (join->positions[idx - i].table->emb_sj_nest != emb_sj_nest)
      return SJ_OPT_NONE;
  }

  /*
    Must use MaterializeScan strategy if there are outer correlated tables
    among the remaining tables, otherwise, if possible, use MaterializeLookup.
  */
  if ((remaining_tables & emb_sj_nest->nested_join->sj_depends_on) ||
      !emb_sj_nest->nested_join->sjm.lookup_allowed)
  {
    if (emb_sj_nest->nested_join->sjm.scan_allowed)
      return SJ_OPT_MATERIALIZE_SCAN;
    return SJ_OPT_NONE;
  }
  return SJ_OPT_MATERIALIZE_LOOKUP;
}


/**
  Find a good, possibly optimal, query execution plan (QEP) by a greedy search.

    The search procedure uses a hybrid greedy/exhaustive search with controlled
    exhaustiveness. The search is performed in N = card(remaining_tables)
    steps. Each step evaluates how promising is each of the unoptimized tables,
    selects the most promising table, and extends the current partial QEP with
    that table.  Currenly the most 'promising' table is the one with least
    expensive extension.\

    There are two extreme cases:
    -# When (card(remaining_tables) < search_depth), the estimate finds the
    best complete continuation of the partial QEP. This continuation can be
    used directly as a result of the search.
    -# When (search_depth == 1) the 'best_extension_by_limited_search'
    consideres the extension of the current QEP with each of the remaining
    unoptimized tables.

    All other cases are in-between these two extremes. Thus the parameter
    'search_depth' controlls the exhaustiveness of the search. The higher the
    value, the longer the optimizaton time and possibly the better the
    resulting plan. The lower the value, the fewer alternative plans are
    estimated, but the more likely to get a bad QEP.

    All intermediate and final results of the procedure are stored in 'join':
    - join->positions     : modified for every partial QEP that is explored
    - join->best_positions: modified for the current best complete QEP
    - join->best_read     : modified for the current best complete QEP
    - join->best_ref      : might be partially reordered

    The final optimal plan is stored in 'join->best_positions', and its
    corresponding cost in 'join->best_read'.

  @note
    The following pseudocode describes the algorithm of 'greedy_search':

    @code
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

  @endcode
    where 'best_extension' is a placeholder for a procedure that selects the
    most "promising" of all tables in 'remaining_tables'.
    Currently this estimate is performed by calling
    'best_extension_by_limited_search' to evaluate all extensions of the
    current QEP of size 'search_depth', thus the complexity of 'greedy_search'
    mainly depends on that of 'best_extension_by_limited_search'.

  @par
    If 'best_extension()' == 'best_extension_by_limited_search()', then the
    worst-case complexity of this algorithm is <=
    O(N*N^search_depth/search_depth). When serch_depth >= N, then the
    complexity of greedy_search is O(N!).

  @par
    In the future, 'greedy_search' might be extended to support other
    implementations of 'best_extension', e.g. some simpler quadratic procedure.

  @param join             pointer to the structure providing all context info
                          for the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param search_depth     controlls the exhaustiveness of the search
  @param prune_level      the pruning heuristics that should be applied during
                          search

  @retval
    FALSE       ok
  @retval
    TRUE        Fatal error
*/

static bool
greedy_search(JOIN      *join,
              table_map remaining_tables,
              uint      search_depth,
              uint      prune_level)
{
  double    record_count= 1.0;
  double    read_time=    0.0;
  uint      idx= join->const_tables; // index into 'join->best_ref'
  uint      best_idx;
  uint      size_remain;    // cardinality of remaining_tables
  POSITION  best_pos;
  JOIN_TAB  *best_table; // the next plan node to be added to the curr QEP
  uint      n_tables; // ==join->tables or # tables in the sj-mat nest we're optimizing

  DBUG_ENTER("greedy_search");

  /* number of tables that remain to be optimized */
  n_tables= size_remain= my_count_bits(remaining_tables &
                                       (join->emb_sjm_nest? 
                                         join->emb_sjm_nest->sj_inner_tables :
                                         ~(table_map)0));

  do {
    /* Find the extension of the current QEP with the lowest cost */
    join->best_read= DBL_MAX;
    if (best_extension_by_limited_search(join, remaining_tables, idx, record_count,
                                         read_time, search_depth, prune_level))
      DBUG_RETURN(TRUE);
    /*
      'best_read < DBL_MAX' means that optimizer managed to find
      some plan and updated 'best_positions' array accordingly.
    */
    DBUG_ASSERT(join->best_read < DBL_MAX); 

    if (size_remain <= search_depth)
    {
      /*
        'join->best_positions' contains a complete optimal extension of the
        current partial QEP.
      */
      DBUG_EXECUTE("opt", print_plan(join, n_tables, record_count, read_time,
                                     read_time, "optimal"););
      DBUG_RETURN(FALSE);
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

    /*
      Update the interleaving state after extending the current partial plan
      with a new table.
      We are doing this here because best_extension_by_limited_search reverts
      the interleaving state to the one of the non-extended partial plan 
      on exit.
    */
    bool is_interleave_error __attribute__((unused))= 
      check_interleaving_with_nj (best_table);
    /* This has been already checked by best_extension_by_limited_search */
    DBUG_ASSERT(!is_interleave_error);

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
    --size_remain;
    ++idx;

    DBUG_EXECUTE("opt", print_plan(join, n_tables, record_count, read_time, 
                                   read_time, "extended"););
  } while (TRUE);
}


/*
  Calculate a cost of given partial join order
 
  SYNOPSIS
    get_partial_join_cost()
      join               IN    Join to use. join->positions holds the
                               partial join order
      idx                IN    # tables in the partial join order
      read_time_arg      OUT   Store read time here 
      record_count_arg   OUT   Store record count here

  DESCRIPTION

    This is needed for semi-join materialization code. The idea is that 
    we detect sj-materialization after we've put all sj-inner tables into
    the join prefix

      prefix-tables semi-join-inner-tables  tN
                                             ^--we're here

    and we'll need to get the cost of prefix-tables prefix again.
*/

void get_partial_join_cost(JOIN *join, uint n_tables, double *read_time_arg,
                           double *record_count_arg)
{
  double record_count= 1;
  double read_time= 0.0;
  for (uint i= join->const_tables; i < n_tables + join->const_tables ; i++)
  {
    if (join->best_positions[i].records_read)
    {
      record_count *= join->best_positions[i].records_read;
      read_time += join->best_positions[i].read_time;
    }
  }
  *read_time_arg= read_time;// + record_count / TIME_FOR_COMPARE;
  *record_count_arg= record_count;
}


/**
  Find a good, possibly optimal, query execution plan (QEP) by a possibly
  exhaustive search.

    The procedure searches for the optimal ordering of the query tables in set
    'remaining_tables' of size N, and the corresponding optimal access paths to
    each table. The choice of a table order and an access path for each table
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

  @note
    The procedure uses a recursive depth-first search where the depth of the
    recursion (and thus the exhaustiveness of the search) is controlled by the
    parameter 'search_depth'.

  @note
    The pseudocode below describes the algorithm of
    'best_extension_by_limited_search'. The worst-case complexity of this
    algorithm is O(N*N^search_depth/search_depth). When serch_depth >= N, then
    the complexity of greedy_search is O(N!).

    @code
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
    @endcode

  @note
    When 'best_extension_by_limited_search' is called for the first time,
    'join->best_read' must be set to the largest possible value (e.g. DBL_MAX).
    The actual implementation provides a way to optionally use pruning
    heuristic (controlled by the parameter 'prune_level') to reduce the search
    space by skipping some partial plans.

  @note
    The parameter 'search_depth' provides control over the recursion
    depth, and thus the size of the resulting optimal plan.

  @param join             pointer to the structure providing all context info
                          for the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param idx              length of the partial QEP in 'join->positions';
                          since a depth-first search is used, also corresponds
                          to the current depth of the search tree;
                          also an index in the array 'join->best_ref';
  @param record_count     estimate for the number of records returned by the
                          best partial plan
  @param read_time        the cost of the best partial plan
  @param search_depth     maximum depth of the recursion and thus size of the
                          found optimal plan
                          (0 < search_depth <= join->tables+1).
  @param prune_level      pruning heuristics that should be applied during
                          optimization
                          (values: 0 = EXHAUSTIVE, 1 = PRUNE_BY_TIME_OR_ROWS)

  @retval
    FALSE       ok
  @retval
    TRUE        Fatal error
*/

static bool
best_extension_by_limited_search(JOIN      *join,
                                 table_map remaining_tables,
                                 uint      idx,
                                 double    record_count,
                                 double    read_time,
                                 uint      search_depth,
                                 uint      prune_level)
{
  DBUG_ENTER("best_extension_by_limited_search");

  THD *thd= join->thd;
  if (thd->killed)  // Abort
    DBUG_RETURN(TRUE);

  /* 
     'join' is a partial plan with lower cost than the best plan so far,
     so continue expanding it further with the tables in 'remaining_tables'.
  */
  JOIN_TAB *s;
  double best_record_count= DBL_MAX;
  double best_read_time=    DBL_MAX;

  DBUG_EXECUTE("opt", print_plan(join, idx, record_count, read_time, read_time,
                                "part_plan"););

  table_map allowed_tables= ~(table_map)0;
  if (join->emb_sjm_nest)
    allowed_tables= join->emb_sjm_nest->sj_inner_tables;

  bool has_sj= !join->select_lex->sj_nests.is_empty();

  for (JOIN_TAB **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    table_map real_table_bit= s->table->map;
    if ((remaining_tables & real_table_bit) && 
        (allowed_tables & real_table_bit) &&
        !(remaining_tables & s->dependent) && 
        (!idx || !check_interleaving_with_nj(s)))
    {
      double current_record_count, current_read_time;
      POSITION *position= join->positions + idx;

      /* Find the best access method from 's' to the current partial plan */
      POSITION loose_scan_pos;
      best_access_path(join, s, remaining_tables, idx, FALSE, record_count, 
                       join->positions + idx, &loose_scan_pos);

      /* Compute the cost of extending the plan with 's' */

      current_record_count= record_count * position->records_read;
      current_read_time=    read_time + position->read_time;

      if (has_sj)
      {
        /*
          Even if there are no semijoins, advance_sj_state() has a significant
          cost (takes 9% of time in a 20-table plan search), hence the if()
          above, which is also more efficient than the same if() inside
          advance_sj_state() would be.
        */
        advance_sj_state(join, remaining_tables, s, idx,
                         &current_record_count, &current_read_time,
                         &loose_scan_pos);
      }
      else
        join->positions[idx].sj_strategy= SJ_OPT_NONE;

      /* Expand only partial plans with lower cost than the best QEP so far */
      if ((current_read_time +
           current_record_count / (double) TIME_FOR_COMPARE) >= join->best_read)
      {
        DBUG_EXECUTE("opt", print_plan(join, idx+1,
                                       current_record_count,
                                       read_time,
                                       (current_read_time +
                                        current_record_count / 
                                        (double) TIME_FOR_COMPARE),
                                       "prune_by_cost"););
        backout_nj_sj_state(remaining_tables, s);
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
            (idx == join->const_tables &&  // 's' is the first table in the QEP
            s->table == join->sort_by_table))
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
          DBUG_EXECUTE("opt", print_plan(join, idx+1,
                                         current_record_count,
                                         read_time,
                                         current_read_time,
                                         "pruned_by_heuristic"););
          backout_nj_sj_state(remaining_tables, s);
          continue;
        }
      }

      if ( (search_depth > 1) && (remaining_tables & ~real_table_bit) & allowed_tables )
      { /* Recursively expand the current partial plan */
        swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
        if (best_extension_by_limited_search(join,
                                             remaining_tables & ~real_table_bit,
                                             idx + 1,
                                             current_record_count,
                                             current_read_time,
                                             search_depth - 1,
                                             prune_level))
          DBUG_RETURN(TRUE);
        swap_variables(JOIN_TAB*, join->best_ref[idx], *pos);
      }
      else
      { /*
          'join' is either the best partial QEP with 'search_depth' relations,
          or the best complete QEP so far, whichever is smaller.
        */
        current_read_time+= current_record_count / (double) TIME_FOR_COMPARE;
        if (join->sort_by_table &&
            join->sort_by_table !=
            join->positions[join->const_tables].table->table)
          /* 
             We may have to make a temp table, note that this is only a 
             heuristic since we cannot know for sure at this point. 
             Hence it may be wrong.
          */
          current_read_time+= current_record_count;
        if ((search_depth == 1) || (current_read_time < join->best_read))
        {
          memcpy((uchar*) join->best_positions, (uchar*) join->positions,
                 sizeof(POSITION) * (idx + 1));
          join->best_read= current_read_time - 0.001;
        }
        DBUG_EXECUTE("opt", print_plan(join, idx+1,
                                       current_record_count,
                                       read_time,
                                       current_read_time,
                                       "full_plan"););
      }
      backout_nj_sj_state(remaining_tables, s);
    }
  }
  DBUG_RETURN(FALSE);
}


/**
  Find how much space the prevous read not const tables takes in cache.
*/

void calc_used_field_length(THD *thd, JOIN_TAB *join_tab)
{
  uint null_fields,blobs,fields,rec_length;
  Field **f_ptr,*field;
  uint uneven_bit_fields;
  MY_BITMAP *read_set= join_tab->table->read_set;

  uneven_bit_fields= null_fields= blobs= fields= rec_length=0;
  for (f_ptr=join_tab->table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (bitmap_is_set(read_set, field->field_index))
    {
      uint flags=field->flags;
      fields++;
      rec_length+=field->pack_length();
      if (flags & BLOB_FLAG)
	blobs++;
      if (!(flags & NOT_NULL_FLAG))
        null_fields++;
      if (field->type() == MYSQL_TYPE_BIT &&
          ((Field_bit*)field)->bit_len)
        uneven_bit_fields++;
    }
  }
  if (null_fields || uneven_bit_fields)
    rec_length+=(join_tab->table->s->null_fields+7)/8;
  if (join_tab->table->maybe_null)
    rec_length+=sizeof(my_bool);
  if (blobs)
  {
    uint blob_length=(uint) (join_tab->table->file->stats.mean_rec_length-
			     (join_tab->table->s->reclength- rec_length));
    rec_length+=(uint) max(4,blob_length);
  }
  /**
    @todo why don't we count the rowids that we might need to store
    when using DuplicateElimination?
  */
  join_tab->used_fields=fields;
  join_tab->used_fieldlength=rec_length;
  join_tab->used_blobs=blobs;
  join_tab->used_null_fields= null_fields;
  join_tab->used_uneven_bit_fields= uneven_bit_fields;
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


/*
  Get the number of different row combinations for subset of partial join

  SYNOPSIS
    prev_record_reads()
      join       The join structure
      idx        Number of tables in the partial join order (i.e. the
                 partial join order is in join->positions[0..idx-1])
      found_ref  Bitmap of tables for which we need to find # of distinct
                 row combinations.

  DESCRIPTION
    Given a partial join order (in join->positions[0..idx-1]) and a subset of
    tables within that join order (specified in found_ref), find out how many
    distinct row combinations of subset tables will be in the result of the
    partial join order.
     
    This is used as follows: Suppose we have a table accessed with a ref-based
    method. The ref access depends on current rows of tables in found_ref.
    We want to count # of different ref accesses. We assume two ref accesses
    will be different if at least one of access parameters is different.
    Example: consider a query

    SELECT * FROM t1, t2, t3 WHERE t1.key=c1 AND t2.key=c2 AND t3.key=t1.field

    and a join order:
      t1,  ref access on t1.key=c1
      t2,  ref access on t2.key=c2       
      t3,  ref access on t3.key=t1.field 
    
    For t1: n_ref_scans = 1, n_distinct_ref_scans = 1
    For t2: n_ref_scans = records_read(t1), n_distinct_ref_scans=1
    For t3: n_ref_scans = records_read(t1)*records_read(t2)
            n_distinct_ref_scans = #records_read(t1)
    
    The reason for having this function (at least the latest version of it)
    is that we need to account for buffering in join execution. 
    
    An edge-case example: if we have a non-first table in join accessed via
    ref(const) or ref(param) where there is a small number of different
    values of param, then the access will likely hit the disk cache and will
    not require any disk seeks.
    
    The proper solution would be to assume an LRU disk cache of some size,
    calculate probability of cache hits, etc. For now we just count
    identical ref accesses as one.

  RETURN 
    Expected number of row combinations
*/

static double
prev_record_reads(JOIN *join, uint idx, table_map found_ref)
{
  double found=1.0;
  POSITION *pos_end= join->positions - 1;
  for (POSITION *pos= join->positions + idx - 1; pos != pos_end; pos--)
  {
    if (pos->table->table->map & found_ref)
    {
      found_ref|= pos->ref_depend_map;
      /* 
        For the case of "t1 LEFT JOIN t2 ON ..." where t2 is a const table 
        with no matching row we will get position[t2].records_read==0. 
        Actually the size of output is one null-complemented row, therefore 
        we will use value of 1 whenever we get records_read==0.

        Note
        - the above case can't occur if inner part of outer join has more 
          than one table: table with no matches will not be marked as const.

        - Ideally we should add 1 to records_read for every possible null-
          complemented row. We're not doing it because: 1. it will require
          non-trivial code and add overhead. 2. The value of records_read
          is an inprecise estimate and adding 1 (or, in the worst case,
          #max_nested_outer_joins=64-1) will not make it any more precise.
      */
      if (pos->records_read > DBL_EPSILON)
        found*= pos->records_read;
    }
  }
  return found;
}


/**
  @brief Fix semi-join strategies for the picked join order

  @param join Pointer to JOIN object with picked join order

  @return FALSE if success, TRUE if error

  @details
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

static bool fix_semijoin_strategies_for_picked_join_order(JOIN *join)
{
  table_map remaining_tables= 0;
  table_map handled_tabs= 0;

  DBUG_ENTER("fix_semijoin_strategies_for_picked_join_order");

  if (join->select_lex->sj_nests.is_empty())
    DBUG_RETURN(FALSE);

  for (uint tableno= join->tables - 1;
       tableno != join->const_tables - 1;
       tableno--)
  {
    POSITION *pos= join->best_positions + tableno;
    JOIN_TAB *s= pos->table;
    TABLE_LIST *emb_sj_nest= s->emb_sj_nest;
    uint first;
    LINT_INIT(first); // Set by every branch except SJ_OPT_NONE which doesn't use it

    if ((handled_tabs & s->table->map) || pos->sj_strategy == SJ_OPT_NONE)
    {
      remaining_tables |= s->table->map;
      continue;
    }

    if (pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP)
    {
      const uint table_count= my_count_bits(emb_sj_nest->sj_inner_tables);
      Semijoin_mat_exec* sjm_exec;
      if (!(sjm_exec= new (join->thd->mem_root)
                          Semijoin_mat_exec(table_count, FALSE)))
        DBUG_RETURN(TRUE);
      emb_sj_nest->sj_mat_exec= sjm_exec;
      /*
        This memcpy() copies a partial QEP produced by
        optimize_semijoin_nests() (source) into the final top-level QEP
        (target), in order to re-use the source plan for to-be-materialized
        inner tables. It is however possible that the source QEP had picked
        some semijoin strategy (noted SJY), different from
        materialization. The target QEP rules (it has seen more tables), but
        this memcpy() is going to copy the source stale strategy SJY,
        wrongly. Which is why sj_strategy of each table of the
        duplicate-generating range then becomes temporarily unreliable. It is
        fixed for the first table of that range right after the memcpy(), and
        fixed for the rest of that range at the end of this iteration by
        setting it to SJ_OPT_NONE). But until then, pos->sj_strategy should
        not be read.
      */
      memcpy(pos - table_count + 1, emb_sj_nest->nested_join->sjm.positions, 
             sizeof(POSITION) * table_count);
      first= tableno - table_count + 1;
      join->best_positions[first].n_sj_tables= table_count;
      join->best_positions[first].sj_strategy= SJ_OPT_MATERIALIZE_LOOKUP;

      DBUG_EXECUTE("opt", print_sjm(emb_sj_nest););
    }
    else if (pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN)
    {
      POSITION *first_inner= join->best_positions + pos->sjm_scan_last_inner;
      TABLE_LIST *mat_sj_nest= first_inner->table->emb_sj_nest;
      const uint table_count= my_count_bits(mat_sj_nest->sj_inner_tables);
      Semijoin_mat_exec* sjm_exec;
      if (!(sjm_exec= new (join->thd->mem_root)
                          Semijoin_mat_exec(table_count, TRUE)))
        DBUG_RETURN(TRUE);
      mat_sj_nest->sj_mat_exec= sjm_exec;
      first= pos->sjm_scan_last_inner - table_count + 1;
      memcpy(join->best_positions + first, // stale semijoin strategy here too
             mat_sj_nest->nested_join->sjm.positions,
             sizeof(POSITION) * table_count);
      join->best_positions[first].sj_strategy= SJ_OPT_MATERIALIZE_SCAN;
      join->best_positions[first].n_sj_tables= table_count;
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
      prefix_rec_count *= mat_sj_nest->nested_join->sjm.expected_rowcount;
      
      table_map rem_tables= remaining_tables;
      for (uint i= tableno; i != (first + table_count - 1); i--)
        rem_tables |= join->best_positions[i].table->table->map;

      POSITION dummy;
      join->cur_sj_inner_tables= 0;
      for (uint i= first + table_count; i <= tableno; i++)
      {
        best_access_path(join, join->best_positions[i].table, rem_tables, i, FALSE,
                         prefix_rec_count, join->best_positions + i, &dummy);
        prefix_rec_count *= join->best_positions[i].records_read;
        rem_tables &= ~join->best_positions[i].table->table->map;
      }

      DBUG_EXECUTE("opt", print_sjm(mat_sj_nest););
    }
    else if (pos->sj_strategy == SJ_OPT_FIRST_MATCH)
    {
      first= pos->first_firstmatch_table;
      join->best_positions[first].sj_strategy= SJ_OPT_FIRST_MATCH;
      join->best_positions[first].n_sj_tables= tableno - first + 1;
      POSITION dummy; // For loose scan paths
      double record_count= (first== join->const_tables)? 1.0: 
                           join->best_positions[tableno - 1].prefix_record_count;
      
      table_map rem_tables= remaining_tables;

      for (uint idx= first; idx <= tableno; idx++)
      {
        rem_tables |= join->best_positions[idx].table->table->map;
      }
      /*
        Re-run best_access_path to produce best access methods that do not use
        join buffering
      */ 
      join->cur_sj_inner_tables= 0;
      for (uint idx= first; idx <= tableno; idx++)
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
    else if (pos->sj_strategy == SJ_OPT_LOOSE_SCAN)
    {
      first= pos->first_loosescan_table;
      POSITION *first_pos= join->best_positions + first;
      POSITION loose_scan_pos; // For loose scan paths
      double record_count= (first== join->const_tables)? 1.0: 
                           join->best_positions[tableno - 1].prefix_record_count;
      
      table_map rem_tables= remaining_tables;

      for (uint idx= first; idx <= tableno; idx++)
        rem_tables |= join->best_positions[idx].table->table->map;
      /*
        Re-run best_access_path to produce best access methods that do not use
        join buffering
      */ 
      join->cur_sj_inner_tables= 0;
      for (uint idx= first; idx <= tableno; idx++)
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
    else if (pos->sj_strategy == SJ_OPT_DUPS_WEEDOUT)
    {
      /* 
        Duplicate Weedout starting at pos->first_dupsweedout_table, ending at
        this table.
      */
      first= pos->first_dupsweedout_table;
      join->best_positions[first].sj_strategy= SJ_OPT_DUPS_WEEDOUT;
      join->best_positions[first].n_sj_tables= tableno - first + 1;
    }
    
    uint i_end= first + join->best_positions[first].n_sj_tables;
    for (uint i= first; i < i_end; i++)
    {
      /*
        Eliminate stale strategies. See comment in the
        SJ_OPT_MATERIALIZE_LOOKUP case above.
      */
      if (i != first)
        join->best_positions[i].sj_strategy= SJ_OPT_NONE;
      handled_tabs |= join->best_positions[i].table->table->map;
    }

    if (tableno != first)
      pos->sj_strategy= SJ_OPT_NONE;
    remaining_tables |= s->table->map;
  }

  /* sjm.positions is no longer needed, reset the reference to it */
  List_iterator<TABLE_LIST> sj_list_it(join->select_lex->sj_nests);
  TABLE_LIST *sj_nest;
  while ((sj_nest= sj_list_it++))
    sj_nest->nested_join->sjm.positions= NULL;

  DBUG_RETURN(FALSE);
}


/*
  Set up join struct according to the picked join order in
  
  SYNOPSIS
    get_best_combination()
      join  The join to process (the picked join order is mainly in
            join->best_positions)

  DESCRIPTION
    Setup join structures according the picked join order
    - finalize semi-join strategy choices
      (see fix_semijoin_strategies_for_picked_join_order)
    - create join->join_tab array and put there the JOIN_TABs in the join order
    - create data structures describing ref access methods.

  RETURN 
    FALSE  OK
    TRUE   Out of memory
*/

static bool get_best_combination(JOIN *join)
{
  table_map used_tables;
  Key_use *keyuse;
  const uint table_count= join->tables;
  THD *thd=join->thd;
  DBUG_ENTER("get_best_combination");

  if (!(join->join_tab= new (thd->mem_root) JOIN_TAB[table_count]))
    DBUG_RETURN(TRUE);

  join->full_join=0;

  used_tables= OUTER_REF_TABLE_BIT;		// Outer row is already read

  if (fix_semijoin_strategies_for_picked_join_order(join))
    DBUG_RETURN(TRUE);

  for (uint tableno= 0; tableno < table_count; tableno++)
  {
    JOIN_TAB *j= join->join_tab + tableno;
    TABLE *form;
    *j= *join->best_positions[tableno].table;
    form=join->all_tables[tableno]= j->table;
    used_tables|= form->map;
    form->reginfo.join_tab=j;
    if (!*j->on_expr_ref)
      form->reginfo.not_exists_optimize=0;	// Only with LEFT JOIN
    DBUG_PRINT("info",("type: %d", j->type));

    if (j->type == JT_CONST)
      continue;					// Handled in make_join_stat..

    j->loosescan_match_tab= NULL;  //non-nulls will be set later
    j->ref.key = -1;
    j->ref.key_parts=0;

    if (j->type == JT_SYSTEM)
      continue;
    
    if (j->keys.is_clear_all() ||
        !(keyuse= join->best_positions[tableno].key) || 
        (join->best_positions[tableno].sj_strategy == SJ_OPT_LOOSE_SCAN))
    {
      j->type=JT_ALL;
      j->index= join->best_positions[tableno].loosescan_key;
      if (tableno != join->const_tables)
	join->full_join=1;
    }
    else if (create_ref_for_key(join, j, keyuse, used_tables))
      DBUG_RETURN(TRUE);                        // Something went wrong
  }

  for (uint tableno= 0; tableno < table_count; tableno++)
    join->map2table[join->join_tab[tableno].table->tablenr]=
      join->join_tab + tableno;

  update_depend_map(join);

  /*
    Set the first_sj_inner_tab and last_sj_inner_tab fields for all tables
    inside the semijoin nests of the query.
  */
  for (uint tableno= join->const_tables; tableno < table_count; )
  {
    JOIN_TAB *tab= join->join_tab + tableno;
    const POSITION *pos= join->best_positions + tableno;

    switch (pos->sj_strategy)
    {
    case SJ_OPT_NONE:
      tableno++;
      break;
    case SJ_OPT_MATERIALIZE_LOOKUP:
    case SJ_OPT_MATERIALIZE_SCAN:
    case SJ_OPT_LOOSE_SCAN:
    case SJ_OPT_DUPS_WEEDOUT:
    case SJ_OPT_FIRST_MATCH:
      /*
        Remember the first and last semijoin inner tables; this serves to tell
        a JOIN_TAB's semijoin strategy (like in check_join_cache_usage()).
      */
      JOIN_TAB *last_sj_tab= tab + pos->n_sj_tables - 1;
      JOIN_TAB *last_sj_inner=
        (pos->sj_strategy == SJ_OPT_DUPS_WEEDOUT) ?
        /* Range may end with non-inner table so cannot set last_sj_inner_tab */
        NULL : last_sj_tab;
      for (JOIN_TAB *tab_in_range= tab;
           tab_in_range <= last_sj_tab;
           tab_in_range++)
      {
        tab_in_range->first_sj_inner_tab= tab;
        tab_in_range->last_sj_inner_tab=  last_sj_inner;
      }
      tableno+= pos->n_sj_tables;
      break;
    }
  }

  DBUG_RETURN(FALSE);
}


static bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
			       table_map used_tables)
{
  Key_use *keyuse= org_keyuse;
  bool ftkey=(keyuse->keypart == FT_KEYPART);
  THD  *thd= join->thd;
  uint keyparts,length,key;
  TABLE *table;
  KEY *keyinfo;
  DBUG_ENTER("create_ref_for_key");

  /*  Use best key found during dependency analysis */
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
  if (!(j->ref.key_buff= (uchar*) thd->calloc(ALIGN_SIZE(length)*2)) ||
      !(j->ref.key_copy= (store_key**) thd->alloc((sizeof(store_key*) *
						   (keyparts+1)))) ||
      !(j->ref.items=    (Item**) thd->alloc(sizeof(Item*)*keyparts)) ||
      !(j->ref.cond_guards= (bool**) thd->alloc(sizeof(uint*)*keyparts)))
  {
    DBUG_RETURN(TRUE);
  }
  j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
  j->ref.key_err=1;
  j->ref.has_record= FALSE;
  j->ref.null_rejecting= 0;
  j->ref.use_count= 0;
  j->ref.disable_cache= FALSE;
  keyuse=org_keyuse;

  store_key **ref_key= j->ref.key_copy;
  uchar *key_buff=j->ref.key_buff, *null_ref_key= 0;
  bool keyuse_uses_no_tables= TRUE;
  if (ftkey)
  {
    j->ref.items[0]=((Item_func*)(keyuse->val))->key_item();
    /* Predicates pushed down into subquery can't be used FT access */
    j->ref.cond_guards[0]= NULL;
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
      j->ref.cond_guards[i]= keyuse->cond_guard;
      if (keyuse->null_rejecting) 
        j->ref.null_rejecting |= 1 << i;
      keyuse_uses_no_tables= keyuse_uses_no_tables && !keyuse->used_tables;

      if (keyuse->used_tables || thd->lex->describe)
        /* 
          Comparing against a non-constant or executing an EXPLAIN
          query (which refers to this info when printing the 'ref'
          column of the query plan)
        */
        *ref_key++= get_store_key(thd,
                                  keyuse,join->const_table_map,
                                  &keyinfo->key_part[i],
                                  key_buff, maybe_null);
      else
      { // Compare against constant
        store_key_item tmp(thd, keyinfo->key_part[i].field,
                           key_buff + maybe_null,
                           maybe_null ?  key_buff : 0,
                           keyinfo->key_part[i].length, keyuse->val);
        if (thd->is_fatal_error)
          DBUG_RETURN(TRUE);
        /* 
          The constant is the value to look for with this key. Copy
          the value to ref->key_buff
        */
        tmp.copy(); 
      }
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
  else if (((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) != HA_NOSAME) ||
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
get_store_key(THD *thd, Key_use *keyuse, table_map used_tables,
	      KEY_PART_INFO *key_part, uchar *key_buff, uint maybe_null)
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
  else if (keyuse->val->type() == Item::FIELD_ITEM ||
           (keyuse->val->type() == Item::REF_ITEM &&
            ((Item_ref*)keyuse->val)->ref_type() == Item_ref::OUTER_REF &&
            (*(Item_ref**)((Item_ref*)keyuse->val)->ref)->ref_type() ==
             Item_ref::DIRECT_REF && 
            keyuse->val->real_item()->type() == Item::FIELD_ITEM))
    return new store_key_field(thd,
			       key_part->field,
			       key_buff + maybe_null,
			       maybe_null ? key_buff : 0,
			       key_part->length,
			       ((Item_field*) keyuse->val->real_item())->field,
			       keyuse->val->full_name());
  return new store_key_item(thd,
			    key_part->field,
			    key_buff + maybe_null,
			    maybe_null ? key_buff : 0,
			    key_part->length,
			    keyuse->val);
}

/**
  This function is only called for const items on fields which are keys.

  @return
    returns 1 if there was some conversion made when the field was stored.
*/

bool
store_val_in_field(Field *field, Item *item, enum_check_fields check_flag)
{
  bool error;
  TABLE *table= field->table;
  THD *thd= table->in_use;
  ha_rows cuted_fields=thd->cuted_fields;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                   table->write_set);

  /*
    we should restore old value of count_cuted_fields because
    store_val_in_field can be called from mysql_insert 
    with select_insert, which make count_cuted_fields= 1
   */
  enum_check_fields old_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= check_flag;
  error= item->save_in_field(field, 1);
  thd->count_cuted_fields= old_count_cuted_fields;
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return error || cuted_fields != thd->cuted_fields;
}


/**
  @details Initialize a JOIN as a query execution plan
  that accesses a single table via a table scan.

  @param  parent      contains JOIN_TAB and TABLE object buffers for this join
  @param  tmp_table   temporary table

  @retval FALSE       success
  @retval TRUE        error occurred
*/
bool
JOIN::make_simple_join(JOIN *parent, TABLE *temp_table)
{
  DBUG_ENTER("JOIN::make_simple_join");

  /*
    Reuse TABLE * and JOIN_TAB if already allocated by a previous call
    to this function through JOIN::exec (may happen for sub-queries).
  */
  if (!parent->join_tab_reexec &&
      !(parent->join_tab_reexec= new (thd->mem_root) JOIN_TAB))
    DBUG_RETURN(TRUE);                      /* purecov: inspected */

  join_tab= parent->join_tab_reexec;
  parent->table_reexec[0]= temp_table;
  tables= 1;
  const_tables= 0;
  const_table_map= 0;
  tmp_table_param.field_count= tmp_table_param.sum_func_count=
    tmp_table_param.func_count= 0;
  /*
    We need to destruct the copy_field (allocated in create_tmp_table())
    before setting it to 0 if the join is not "reusable".
  */
  if (!tmp_join || tmp_join != this) 
    tmp_table_param.cleanup(); 
  tmp_table_param.copy_field= tmp_table_param.copy_field_end=0;
  first_record= sort_and_group=0;
  send_records= (ha_rows) 0;
  group= 0;
  row_limit= unit->select_limit_cnt;
  do_send_rows= row_limit ? 1 : 0;

  join_tab->use_join_cache= JOIN_CACHE::ALG_NONE;
  join_tab->table=tmp_table;
  join_tab->type= JT_ALL;			/* Map through all records */
  join_tab->keys.set_all();                     /* test everything in quick */
  join_tab->ref.key = -1;
  join_tab->read_first_record= join_init_read_record;
  join_tab->join= this;
  join_tab->ref.key_parts= 0;
  temp_table->status=0;
  temp_table->null_row=0;
  DBUG_RETURN(FALSE);
}


inline void add_cond_and_fix(Item **e1, Item *e2)
{
  if (*e1)
  {
    if (!e2)
      return;
    Item *res;
    if ((res= new Item_cond_and(*e1, e2)))
    {
      *e1= res;
      res->quick_fix_field();
      res->update_used_tables();
    }
  }
  else
    *e1= e2;
}


/**
  Add to join_tab->select_cond[i] "table.field IS NOT NULL" conditions
  we've inferred from ref/eq_ref access performed.

    This function is a part of "Early NULL-values filtering for ref access"
    optimization.

    Example of this optimization:
    For query SELECT * FROM t1,t2 WHERE t2.key=t1.field @n
    and plan " any-access(t1), ref(t2.key=t1.field) " @n
    add "t1.field IS NOT NULL" to t1's table condition. @n

    Description of the optimization:
    
      We look through equalities choosen to perform ref/eq_ref access,
      pick equalities that have form "tbl.part_of_key = othertbl.field"
      (where othertbl is a non-const table and othertbl.field may be NULL)
      and add them to conditions on correspoding tables (othertbl in this
      example).

      Exception from that is the case when referred_tab->join != join.
      I.e. don't add NOT NULL constraints from any embedded subquery.
      Consider this query:
      @code
      SELECT A.f2 FROM t1 LEFT JOIN t2 A ON A.f2 = f1
      WHERE A.f3=(SELECT MIN(f3) FROM  t2 C WHERE A.f4 = C.f4) OR A.f3 IS NULL;
      @endocde
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
      1.1 add_key_part saves these to Key_use.
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
    if ((tab->type == JT_REF || tab->type == JT_EQ_REF || 
         tab->type == JT_REF_OR_NULL) &&
        !tab->table->maybe_null)
    {
      for (uint keypart= 0; keypart < tab->ref.key_parts; keypart++)
      {
        if (tab->ref.null_rejecting & (1 << keypart))
        {
          Item *item= tab->ref.items[keypart];
          Item *notnull;
          Item *real= item->real_item();
          DBUG_ASSERT(real->type() == Item::FIELD_ITEM);
          Item_field *not_null_item= (Item_field*)real;
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
                                           referred_tab->table->alias,
                                           QT_ORDINARY););
          Item *new_cond= referred_tab->select_cond;
          add_cond_and_fix(&new_cond, notnull);
          referred_tab->set_select_cond(new_cond, __LINE__);
        }
      }
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Build a predicate guarded by match variables for embedding outer joins.
  The function recursively adds guards for predicate cond
  assending from tab to the first inner table  next embedding
  nested outer join and so on until it reaches root_tab
  (root_tab can be 0).

  @param tab       the first inner table for most nested outer join
  @param cond      the predicate to be guarded (must be set)
  @param root_tab  the first inner table to stop

  @return
    -  pointer to the guarded predicate, if success
    -  0, otherwise
*/

static Item*
add_found_match_trig_cond(JOIN_TAB *tab, Item *cond, JOIN_TAB *root_tab)
{
  Item *tmp;
  DBUG_ASSERT(cond != 0);
  if (tab == root_tab)
    return cond;
  if ((tmp= add_found_match_trig_cond(tab->first_upper, cond, root_tab)))
    tmp= new Item_func_trig_cond(tmp, &tab->found);
  if (tmp)
  {
    tmp->quick_fix_field();
    tmp->update_used_tables();
  }
  return tmp;
}


/**
  Fill in outer join related info for the execution plan structure.

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

  EXAMPLE. For the query: 
  @code
        SELECT * FROM t1
                      LEFT JOIN
                      (t2, t3 LEFT JOIN t4 ON t3.a=t4.a)
                      ON (t1.a=t2.a AND t1.b=t3.b)
          WHERE t1.c > 5,
  @endcode

    given the execution plan with the table order t1,t2,t3,t4
    is selected, the following references will be set;
    t4->last_inner=[t4], t4->first_inner=[t4], t4->first_upper=[t2]
    t2->last_inner=[t4], t2->first_inner=t3->first_inner=[t2],
    on expression (t1.a=t2.a AND t1.b=t3.b) will be attached to 
    *t2->on_expr_ref, while t3.a=t4.a will be attached to *t4->on_expr_ref.

  @param join   reference to the info fully describing the query

  @note
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
      /* Ignore sj-nests: */
      if (!embedding->on_expr)
        continue;
      NESTED_JOIN *nested_join= embedding->nested_join;
      if (!nested_join->counter_)
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
      if (++nested_join->counter_ < nested_join->join_list.elements)
        break;
      /* Table tab is the last inner table for nested join. */
      nested_join->first_nested->last_inner= tab;
    }
  }
  DBUG_VOID_RETURN;
}

static bool extend_select_cond(JOIN_TAB *cond_tab, Item *tmp_cond)
{
  DBUG_ENTER("extend_select_cond");

  Item *new_cond= !cond_tab->select_cond ? tmp_cond :
    new Item_cond_and(cond_tab->select_cond, tmp_cond);
  cond_tab->set_select_cond(new_cond, __LINE__);
  if (!cond_tab->select_cond)
    DBUG_RETURN(1);
  cond_tab->select_cond->update_used_tables();
  cond_tab->select_cond->quick_fix_field();
  if (cond_tab->select)
    cond_tab->select->cond= cond_tab->select_cond; 

  DBUG_RETURN(0);
}


/**
   Local helper function for make_join_select().

   Push down conditions from all on expressions.
   Each of these conditions are guarded by a variable
   that turns if off just before null complemented row for
   outer joins is formed. Thus, the condition from an
   'on expression' are guaranteed not to be checked for
   the null complemented row.
*/ 
static bool pushdown_on_conditions(JOIN* join, JOIN_TAB *last_tab)
{
  DBUG_ENTER("pushdown_on_conditions");

  /* First push down constant conditions from on expressions */
  for (JOIN_TAB *join_tab= join->join_tab+join->const_tables;
       join_tab < join->join_tab+join->tables ; join_tab++)
  {
    if (*join_tab->on_expr_ref)
    {
      JOIN_TAB *cond_tab= join_tab->first_inner;
      Item *tmp_cond= make_cond_for_table(*join_tab->on_expr_ref,
                                          join->const_table_map,
                                          (table_map) 0, 0);
      if (!tmp_cond)
        continue;
      tmp_cond= new Item_func_trig_cond(tmp_cond, &cond_tab->not_null_compl);
      if (!tmp_cond)
        DBUG_RETURN(1);
      tmp_cond->quick_fix_field();

      if (extend_select_cond(cond_tab, tmp_cond))
        DBUG_RETURN(1);
    }       
  }

  JOIN_TAB *first_inner_tab= last_tab->first_inner; 

  /* Push down non-constant conditions from on expressions */
  while (first_inner_tab && first_inner_tab->last_inner == last_tab)
  {  
    /* 
       Table last_tab is the last inner table of an outer join.
       An on expression is always attached to it.
    */     
    Item *on_expr= *first_inner_tab->on_expr_ref;

    table_map used_tables= (join->const_table_map |
                            OUTER_REF_TABLE_BIT | RAND_TABLE_BIT);
    for (JOIN_TAB *join_tab= join->join_tab+join->const_tables;
         join_tab <= last_tab ; join_tab++)
    {
      table_map current_map= join_tab->table->map;
      used_tables|= current_map;
      Item *tmp_cond= make_cond_for_table(on_expr, used_tables, current_map, 0);
      if (!tmp_cond)
        continue;

      JOIN_TAB *cond_tab=
        join_tab < first_inner_tab ? first_inner_tab : join_tab;
      /*
        First add the guards for match variables of
        all embedding outer join operations.
      */
      if (!(tmp_cond= add_found_match_trig_cond(cond_tab->first_inner,
                                                tmp_cond,
                                                first_inner_tab)))
        DBUG_RETURN(1);
      /* 
         Now add the guard turning the predicate off for 
         the null complemented row.
      */ 
      tmp_cond= new Item_func_trig_cond(tmp_cond,
                                        &first_inner_tab->not_null_compl);
      if (tmp_cond)
        tmp_cond->quick_fix_field();

      /* Add the predicate to other pushed down predicates */
      if (extend_select_cond(cond_tab, tmp_cond))
        DBUG_RETURN(1);
    }
    first_inner_tab= first_inner_tab->first_upper;       
  }
  DBUG_RETURN(0);
}


/**
  Separates the predicates in a join condition and pushes them to the 
  join step where all involved tables are available in the join prefix.
  ON clauses from JOIN expressions are also pushed to the most appropriate step.

  @param join Join object where predicates are pushed.

  @param cond Pointer to condition which may contain an arbitrary number of
              predicates, combined using AND, OR and XOR items.
              If NULL, equivalent to a predicate that returns TRUE for all
              row combinations.

  @retval TRUE if condition is always false OR an error occurred.
  @retval FALSE otherwise.
*/

static bool make_join_select(JOIN *join, Item *cond)
{
  THD *thd= join->thd;
  DBUG_ENTER("make_join_select");
  {
    add_not_null_conds(join);
    table_map used_tables;
    /*
      Step #1: Extract constant condition
       - Extract and check the constant part of the WHERE 
       - Extract constant parts of ON expressions from outer 
         joins and attach them appropriately.
    */
    if (cond)                /* Because of QUICK_GROUP_MIN_MAX_SELECT */
    {                        /* there may be a select without a cond. */    
      if (join->tables > 1)
        cond->update_used_tables();		// Tablenr may have changed
      if (join->const_tables == join->tables &&
	  thd->lex->current_select->master_unit() ==
	  &thd->lex->unit)		// not upper level SELECT
        join->const_table_map|=RAND_TABLE_BIT;

      /*
        Extract expressions that depend on constant tables
        1. Const part of the join's WHERE clause can be checked immediately
           and if it is not satisfied then the join has empty result
        2. Constant parts of outer joins' ON expressions must be attached 
           there inside the triggers.
      */
      {
        Item *const_cond=
	  make_cond_for_table(cond,
                              join->const_table_map,
                              (table_map) 0, 1);
        /* Add conditions added by add_not_null_conds(). */
        for (uint i= 0 ; i < join->const_tables ; i++)
          add_cond_and_fix(&const_cond, join->join_tab[i].select_cond);

        DBUG_EXECUTE("where",print_where(const_cond,"constants", QT_ORDINARY););
        for (JOIN_TAB *tab= join->join_tab+join->const_tables;
             tab < join->join_tab+join->tables ; tab++)
        {
          if (*tab->on_expr_ref)
          {
            JOIN_TAB *cond_tab= tab->first_inner;
            Item *tmp= make_cond_for_table(*tab->on_expr_ref,
                                           join->const_table_map,
                                           (  table_map) 0, 0);
            if (!tmp)
              continue;
            tmp= new Item_func_trig_cond(tmp, &cond_tab->not_null_compl);
            if (!tmp)
              DBUG_RETURN(1);
            tmp->quick_fix_field();
            Item *new_cond= !cond_tab->select_cond ? tmp :
              new Item_cond_and(cond_tab->select_cond, tmp);
            cond_tab->set_select_cond(new_cond, __LINE__);
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

    /*
      Step #2: Extract WHERE/ON parts
    */
    table_map save_used_tables= 0;
    used_tables= join->const_table_map | OUTER_REF_TABLE_BIT | RAND_TABLE_BIT;
    JOIN_TAB *tab;
    table_map current_map;
    for (uint i=join->const_tables ; i < join->tables ; i++)
    {
      tab= join->join_tab+i;
      /*
        first_inner is the X in queries like:
        SELECT * FROM t1 LEFT OUTER JOIN (t2 JOIN t3) ON X
      */
      JOIN_TAB *first_inner_tab= tab->first_inner; 
      current_map= tab->table->map;
      bool use_quick_range=0;
      Item *tmp;

      /* 
        Tables that are within SJ-Materialization nests cannot have their
        conditions referring to preceding non-const tables.
         - If we're looking at the first SJM table, reset used_tables
           to refer to only allowed tables
      */
      if (sj_is_materialize_strategy(tab->get_sj_strategy()) &&
          !(used_tables & tab->emb_sj_nest->sj_inner_tables))
      {
        save_used_tables= used_tables;
        used_tables= join->const_table_map | OUTER_REF_TABLE_BIT | 
                     RAND_TABLE_BIT;
      }

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
	tab->use_quick=QS_RANGE;
        tab->ref.key= -1;
	tab->ref.key_parts=0;		// Don't use ref key.
	join->best_positions[i].records_read= rows2double(tab->quick->records);
        /* 
          We will use join cache here : prevent sorting of the first
          table only and sort at the end.
        */
        if (i != join->const_tables && join->tables > join->const_tables + 1)
          join->full_join= 1;
      }

      tmp= NULL;
      if (cond)
        tmp= make_cond_for_table(cond,used_tables,current_map, 0);
      /* Add conditions added by add_not_null_conds(). */
      if (tab->select_cond)
        add_cond_and_fix(&tmp, tab->select_cond);

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
        }

      }
      if (tmp || !cond || tab->type == JT_REF || tab->type == JT_REF_OR_NULL ||
          tab->type == JT_EQ_REF || first_inner_tab)
      {
        DBUG_EXECUTE("where",print_where(tmp,tab->table->alias, QT_ORDINARY););
	SQL_SELECT *sel= tab->select= new (thd->mem_root) SQL_SELECT;
	if (!sel)
	  DBUG_RETURN(1);			// End of memory
        sel->read_tables= sel->const_tables= join->const_table_map;
        /*
          If tab is an inner table of an outer join operation,
          add a match guard to the pushed down predicate.
          The guard will turn the predicate on only after
          the first match for outer tables is encountered.
	*/        
        if (cond && tmp)
        {
          /*
            Because of QUICK_GROUP_MIN_MAX_SELECT there may be a select without
            a cond, so neutralize the hack above.
          */
          if (!(tmp= add_found_match_trig_cond(first_inner_tab, tmp, 0)))
            DBUG_RETURN(1);
          sel->cond= tmp;
          tab->set_select_cond(tmp, __LINE__);
          /* Push condition to storage engine if this is enabled
             and the condition is not guarded */
          tab->table->file->pushed_cond= NULL;
	  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
              !first_inner_tab)
          {
            Item *push_cond= 
              make_cond_for_table(tmp, current_map, current_map, 0);
            if (push_cond)
            {
              /* Push condition to handler */
              if (!tab->table->file->cond_push(push_cond))
                tab->table->file->pushed_cond= push_cond;
            }
          }
        }
        else
        {
          sel->cond= NULL;
          tab->set_select_cond(NULL, __LINE__);
        }

	sel->head=tab->table;
        DBUG_EXECUTE("where",print_where(tmp,tab->table->alias, QT_ORDINARY););
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

	  if ((cond &&
              !tab->keys.is_subset(tab->const_keys) && i > 0) ||
	      (!tab->const_keys.is_clear_all() && i == join->const_tables &&
	       join->unit->select_limit_cnt <
	       join->best_positions[i].records_read &&
	       !(join->select_options & OPTION_FOUND_ROWS)))
	  {
	    /* Join with outer join condition */
	    Item *orig_cond=sel->cond;
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
					join->unit->select_limit_cnt), 0,
                                        FALSE) < 0)
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
                                          join->unit->select_limit_cnt),0,
                                          FALSE) < 0)
		DBUG_RETURN(1);			// Impossible WHERE
            }
            else
	      sel->cond=orig_cond;

	    /* Fix for EXPLAIN */
	    if (sel->quick)
	      join->best_positions[i].records_read= (double)sel->quick->records;
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
			     (sel->quick_keys.is_clear_all() ||
			      (sel->quick &&
			       (sel->quick->records >= 100L)))) ?
	      QS_DYNAMIC_RANGE : QS_RANGE;
	    sel->read_tables= used_tables & ~current_map;
	  }
	  if (i != join->const_tables && tab->use_quick != QS_DYNAMIC_RANGE &&
              !tab->first_inner)
	  {					/* Read with cache */
	    if (cond &&
                (tmp=make_cond_for_table(cond,
					 join->const_table_map |
					 current_map,
					 current_map, 0)))
	    {
              DBUG_EXECUTE("where",print_where(tmp,"cache", QT_ORDINARY););
	      tab->cache_select=(SQL_SELECT*)
		thd->memdup((uchar*) sel, sizeof(SQL_SELECT));
	      tab->cache_select->cond=tmp;
	      tab->cache_select->read_tables=join->const_table_map;
	    }
	  }
	}
      }
      
      if (pushdown_on_conditions(join, tab))
        DBUG_RETURN(1);

      DBUG_ASSERT(save_used_tables ? tab->emb_sj_nest != NULL : TRUE);

      /*
         1. We are inside a materialized semijoin nest, and
         2. All inner tables of the nest are covered.
      */ 
      if (save_used_tables &&                                        // 1
         !(tab->emb_sj_nest->sj_inner_tables & ~used_tables))        // 2
      {
        /*
          The join order now looks like this:

           ot1 ... otI SJM(it1 ... itN) otI+1 ... otM
                                       ^
                                        \-we're here
          At this point, we have generated a condition that can be checked
          when we have all of the sj-inner tables (it1 ... itN).
          This will be used while doing materialization.

          In addition, we need a condition that can be checked when we have
          all of the tables in the prefix (both inner and outer).
          This condition is only generated (and used) when we have an SJM-scan
          operation. For SJM-lookup, the condition is completely fulfilled
          through the lookup into the materialized table.
          This constraint will last as long as we do not allow correlated
          subqueries with materialized semijoin execution.
        */
        if (cond && tab->emb_sj_nest->sj_mat_exec->is_scan)
          tab->emb_sj_nest->sj_mat_exec->join_cond= 
            make_cond_after_sjm(cond, cond, save_used_tables, used_tables);

        used_tables= save_used_tables | used_tables;
        save_used_tables= 0;
      }
    }
  }
  DBUG_RETURN(0);
}


/* 
  Check if given expression uses only table fields covered by the given index

  SYNOPSIS
    uses_index_fields_only()
      item           Expression to check
      tbl            The table having the index
      keyno          The index number
      other_tbls_ok  TRUE <=> Fields of other non-const tables are allowed

  DESCRIPTION
    Check if given expression only uses fields covered by index #keyno in the
    table tbl. The expression can use any fields in any other tables.
    
    The expression is guaranteed not to be AND or OR - those constructs are 
    handled outside of this function.

  RETURN
    TRUE   Yes
    FALSE  No
*/

bool uses_index_fields_only(Item *item, TABLE *tbl, uint keyno, 
                            bool other_tbls_ok)
{
  if (item->const_item())
    return TRUE;

  const Item::Type item_type= item->type();

  /* 
    Don't push down the triggered conditions. Nested outer joins execution 
    code may need to evaluate a condition several times (both triggered and
    untriggered), and there is no way to put thi
    TODO: Consider cloning the triggered condition and using the copies for:
      1. push the first copy down, to have most restrictive index condition
         possible
      2. Put the second copy into tab->select_cond. 
  */
  if (item_type == Item::FUNC_ITEM && 
      ((Item_func*)item)->functype() == Item_func::TRIG_COND_FUNC)
    return FALSE;

  /*
    Do not push down subselects for execution by the handler. This
    case would also be handled by the default label of the second
    switch statement in this function. But since a subselect might
    only refer to other tables the check below (if this item only
    contains "other" tables) can return true and thus we need to do
    this check here.
  */
  if (item_type == Item::SUBSELECT_ITEM)
    return false;

  /*
    If this item will be evaluated using only "other tables" we let
    the value of the other_tbls_ok determine if this item can be
    pushed down or not.
   */
  if (!(item->used_tables() & tbl->map))
    return other_tbls_ok;

  switch (item_type) {
  case Item::FUNC_ITEM:
    {
      /* This is a function, apply condition recursively to arguments */
      Item_func *item_func= (Item_func*)item;
      Item **child;
      Item **item_end= (item_func->arguments()) + item_func->argument_count();
      for (child= item_func->arguments(); child != item_end; child++)
      {
        if (!uses_index_fields_only(*child, tbl, keyno, other_tbls_ok))
          return FALSE;
      }
      return TRUE;
    }
  case Item::COND_ITEM:
    {
      /*
        This is a AND/OR condition. Regular AND/OR clauses are handled by
        make_cond_for_index() which will chop off the part that can be
        checked with index. This code is for handling non-top-level AND/ORs,
        e.g. func(x AND y).
      */
      List_iterator<Item> li(*((Item_cond*)item)->argument_list());
      Item *item;
      while ((item=li++))
      {
        if (!uses_index_fields_only(item, tbl, keyno, other_tbls_ok))
          return FALSE;
      }
      return TRUE;
    }
  case Item::FIELD_ITEM:
    {
      Item_field *item_field= (Item_field*)item;
      if (item_field->field->table != tbl) 
        return TRUE;
      /*
        The below is probably a repetition - the first part checks the
        other two, but let's play it safe:
      */
      return item_field->field->part_of_key.is_set(keyno) &&
             item_field->field->type() != MYSQL_TYPE_GEOMETRY &&
             item_field->field->type() != MYSQL_TYPE_BLOB;
    }
  case Item::REF_ITEM:
    return uses_index_fields_only(item->real_item(), tbl, keyno,
                                  other_tbls_ok);
  default:
    return FALSE; /* Play it safe, don't push unknown non-const items */
  }
}


#define ICP_COND_USES_INDEX_ONLY 10

/*
  Get a part of the condition that can be checked using only index fields

  SYNOPSIS
    make_cond_for_index()
      cond           The source condition
      table          The table that is partially available
      keyno          The index in the above table. Only fields covered by the index
                     are available
      other_tbls_ok  TRUE <=> Fields of other non-const tables are allowed

  DESCRIPTION
    Get a part of the condition that can be checked when for the given table 
    we have values only of fields covered by some index. The condition may
    refer to other tables, it is assumed that we have values of all of their 
    fields.

    Example:
      make_cond_for_index(
         "cond(t1.field) AND cond(t2.key1) AND cond(t2.non_key) AND cond(t2.key2)",
          t2, keyno(t2.key1)) 
      will return
        "cond(t1.field) AND cond(t2.key2)"

  RETURN
    Index condition, or NULL if no condition could be inferred.
*/

Item *make_cond_for_index(Item *cond, TABLE *table, uint keyno,
                          bool other_tbls_ok)
{
  if (!cond)
    return NULL;
  if (cond->type() == Item::COND_ITEM)
  {
    uint n_marked= 0;
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      table_map used_tables= 0;
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_index(item, table, keyno, other_tbls_ok);
	if (fix)
        {
	  new_cond->argument_list()->push_back(fix);
          used_tables|= fix->used_tables();
        }
        n_marked += test(item->marker == ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked ==((Item_cond*)cond)->argument_list()->elements)
        cond->marker= ICP_COND_USES_INDEX_ONLY;
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (Item*) 0;
      case 1:
        new_cond->used_tables_cache= used_tables;
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
        new_cond->used_tables_cache= used_tables;
	return new_cond;
      }
    }
    else /* It's OR */
    {
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_index(item, table, keyno, other_tbls_ok);
	if (!fix)
	  return (Item*) 0;
	new_cond->argument_list()->push_back(fix);
        n_marked += test(item->marker == ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked ==((Item_cond*)cond)->argument_list()->elements)
        cond->marker= ICP_COND_USES_INDEX_ONLY;
      new_cond->quick_fix_field();
      new_cond->used_tables_cache= ((Item_cond_or*) cond)->used_tables_cache;
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_index_fields_only(cond, table, keyno, other_tbls_ok))
    return (Item*) 0;
  cond->marker= ICP_COND_USES_INDEX_ONLY;
  return cond;
}


Item *make_cond_remainder(Item *cond, bool exclude_index)
{
  if (exclude_index && cond->marker == ICP_COND_USES_INDEX_ONLY)
    return 0; /* Already checked */

  if (cond->type() == Item::COND_ITEM)
  {
    table_map tbl_map= 0;
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_remainder(item, exclude_index);
	if (fix)
        {
	  new_cond->argument_list()->push_back(fix);
          tbl_map |= fix->used_tables();
        }
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (Item*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
        ((Item_cond*)new_cond)->used_tables_cache= tbl_map;
	return new_cond;
      }
    }
    else /* It's OR */
    {
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_remainder(item, FALSE);
	if (!fix)
	  return (Item*) 0;
	new_cond->argument_list()->push_back(fix);
        tbl_map |= fix->used_tables();
      }
      new_cond->quick_fix_field();
      ((Item_cond*)new_cond)->used_tables_cache= tbl_map;
      new_cond->top_level_item();
      return new_cond;
    }
  }
  return cond;
}


/*
  Try to extract and push the index condition

  SYNOPSIS
    push_index_cond()
      tab            A join tab that has tab->table->file and its condition
                     in tab->select_cond
      keyno          Index for which extract and push the condition
      other_tbls_ok  TRUE <=> Fields of other non-const tables are allowed

  DESCRIPTION
    Try to extract and push the index condition down to table handler
*/

static void push_index_cond(JOIN_TAB *tab, uint keyno, bool other_tbls_ok)
{
  DBUG_ENTER("push_index_cond");
  Item *idx_cond;

  /*
    We will only attempt to push down an index condition when the
    following criteria are true:
    1. The storage engine supports ICP.
    2. The system variable for enabling ICP is ON.
    3. The query is not a multi-table update or delete statement. The reason
       for this requirement is that the same handler will be used 
       both for doing the select/join and the update. The pushed index
       condition might then also be applied by the storage engine
       when doing the update part and result in either not finding
       the record to update or updating the wrong record.
  */
  if (tab->table->file->index_flags(keyno, 0, 1) &
      HA_DO_INDEX_COND_PUSHDOWN &&
      tab->join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN) &&
      tab->join->thd->lex->sql_command != SQLCOM_UPDATE_MULTI &&
      tab->join->thd->lex->sql_command != SQLCOM_DELETE_MULTI)
  {
    DBUG_EXECUTE("where", print_where(tab->select_cond, "full cond",
                 QT_ORDINARY););
    idx_cond= make_cond_for_index(tab->select_cond, tab->table, keyno,
                                  other_tbls_ok);
    DBUG_EXECUTE("where", print_where(idx_cond, "idx cond", QT_ORDINARY););
    if (idx_cond)
    {
      Item *idx_remainder_cond= 0;
      tab->pre_idx_push_select_cond= tab->select_cond;

      /*
        For BKA cache we store condition to special BKA cache field
        because evaluation of the condition requires additional operations
        before the evaluation. This condition is used in 
        JOIN_CACHE_BKA[_UNIQUE]::skip_index_tuple() functions.
      */
      if (tab->use_join_cache &&
          /*
            if cache is used then the value is TRUE only 
            for BKA[_UNIQUE] cache (see check_join_cache_usage func).
            In this case other_tbls_ok is an equivalent of
            cache->is_key_access().
          */
          other_tbls_ok &&
          (idx_cond->used_tables() &
           ~(tab->table->map | tab->join->const_table_map)))
        tab->cache_idx_cond= idx_cond;
      else
      {
        idx_remainder_cond= tab->table->file->idx_cond_push(keyno, idx_cond);
        tab->select->icp_cond= idx_cond;
        DBUG_EXECUTE("where",
                     print_where(tab->select->icp_cond, "icp cond", 
                                 QT_ORDINARY););
      }
      /*
        Disable eq_ref's "lookup cache" if we've pushed down an index
        condition. 
        TODO: This check happens to work on current ICP implementations, but
        there may exist a compliant implementation that will not work 
        correctly with it. Sort this out when we stabilize the condition
        pushdown APIs.
      */
      if (idx_remainder_cond != idx_cond)
        tab->ref.disable_cache= TRUE;

      Item *row_cond= make_cond_remainder(tab->select_cond, TRUE);
      DBUG_EXECUTE("where", print_where(row_cond, "remainder cond",
                   QT_ORDINARY););
      
      if (row_cond)
      {
        if (!idx_remainder_cond)
          tab->set_select_cond(row_cond, __LINE__);
        else
        {
          Item *new_cond= new Item_cond_and(row_cond, idx_remainder_cond);
          tab->set_select_cond(new_cond, __LINE__);
	  tab->select_cond->quick_fix_field();
          ((Item_cond_and*)tab->select_cond)->used_tables_cache= 
            row_cond->used_tables() | idx_remainder_cond->used_tables();
        }
      }
      else
        tab->set_select_cond(idx_remainder_cond, __LINE__);
      if (tab->select)
      {
        DBUG_EXECUTE("where", print_where(tab->select->cond, "select_cond",
                     QT_ORDINARY););
        tab->select->cond= tab->select_cond;
      }
    }
  }
  DBUG_VOID_RETURN;
}

/**
  The default implementation of unlock-row method of READ_RECORD,
  used in all access methods.
*/

void rr_unlock_row(st_join_table *tab)
{
  READ_RECORD *info= &tab->read_record;
  info->file->unlock_row();
}



/**
  Pick the appropriate access method functions

  Sets the functions for the selected table access method

  @param      tab               Table reference to put access method
*/

static void
pick_table_access_method(JOIN_TAB *tab)
{
  switch (tab->type) 
  {
  case JT_REF:
    tab->read_first_record= join_read_always_key;
    tab->read_record.read_record= join_read_next_same;
    break;

  case JT_REF_OR_NULL:
    tab->read_first_record= join_read_always_key_or_null;
    tab->read_record.read_record= join_read_next_same_or_null;
    break;

  case JT_CONST:
    tab->read_first_record= join_read_const;
    tab->read_record.read_record= join_no_more_records;
    break;

  case JT_EQ_REF:
    tab->read_first_record= join_read_key;
    tab->read_record.read_record= join_no_more_records;
    break;

  case JT_FT:
    tab->read_first_record= join_ft_read_first;
    tab->read_record.read_record= join_ft_read_next;
    break;

  case JT_SYSTEM:
    tab->read_first_record= join_read_system;
    tab->read_record.read_record= join_no_more_records;
    break;

  /* keep gcc happy */  
  default:
    break;  
  }
}



/*
  Determine {after which table we'll produce ordered set} 

  SYNOPSIS
    make_join_orderinfo()
     join

   
  DESCRIPTION 
    Determine if the set is already ordered for ORDER BY, so it can 
    disable join cache because it will change the ordering of the results.
    Code handles sort table that is at any location (not only first after 
    the const tables) despite the fact that it's currently prohibited.
    We must disable join cache if the first non-const table alone is
    ordered. If there is a temp table the ordering is done as a last
    operation and doesn't prevent join cache usage.

  RETURN
    Number of table after which the set will be ordered
    join->tables if we don't need an ordered set 
*/

static uint make_join_orderinfo(JOIN *join)
{
  JOIN_TAB *tab;
  tab= join->get_sort_by_join_tab();
  return tab ? tab-join->join_tab : join->tables;
}


/*
  Deny usage of join buffer for the specified table

  SYNOPSIS
    set_join_cache_denial()
      tab    join table for which join buffer usage is to be denied  
     
  DESCRIPTION
    The function denies usage of join buffer when joining the table 'tab'.
    The table is marked as not employing any join buffer. If a join cache
    object has been already allocated for the table this object is destroyed.

  RETURN
    none    
*/

static
void set_join_cache_denial(JOIN_TAB *join_tab)
{
  if (join_tab->cache)
  {
    /* 
      If there is a previous cache linked to this cache through the
      next_cache pointer: remove the link. 
    */
    if (join_tab->cache->prev_cache)
      join_tab->cache->prev_cache->next_cache= 0;
    /*
      No need to do the same for next_cache since cache denial is done
      backwards starting from the latest cache in the linked list (see
      revise_cache_usage()).
    */
    DBUG_ASSERT(!join_tab->cache->next_cache);

    join_tab->cache->free();
    join_tab->cache= 0;
  }
  if (join_tab->use_join_cache)
  {
    join_tab->use_join_cache= JOIN_CACHE::ALG_NONE;
    /*
      It could be only sub_select(). It could not be sub_seject_sjm because we
      don't do join buffering for the first table in sjm nest. 
    */
    join_tab[-1].next_select= sub_select;
  }
}


/* 
  Revise usage of join buffer for the specified table and the whole nest   

  SYNOPSIS
    revise_cache_usage()
      tab    join table for which join buffer usage is to be revised  

  DESCRIPTION
    The function revise the decision to use a join buffer for the table 'tab'.
    If this table happened to be among the inner tables of a nested outer join/
    semi-join the functions denies usage of join buffers for all of them

  RETURN
    none    
*/

static
void revise_cache_usage(JOIN_TAB *join_tab)
{
  JOIN_TAB *tab;
  JOIN_TAB *first_inner;

  if (join_tab->first_inner)
  {
    JOIN_TAB *end_tab= join_tab;
    for (first_inner= join_tab->first_inner; 
         first_inner;
         first_inner= first_inner->first_upper)           
    {
      for (tab= end_tab-1; tab >= first_inner; tab--)
        set_join_cache_denial(tab);
      end_tab= first_inner;
    }
  }
  else if (join_tab->get_sj_strategy() == SJ_OPT_FIRST_MATCH)
  {
    first_inner= join_tab->first_sj_inner_tab;
    for (tab= join_tab-1; tab >= first_inner; tab--)
    {
      if (tab->first_sj_inner_tab == first_inner)
        set_join_cache_denial(tab);
    }
  }
  else set_join_cache_denial(join_tab);
}


/* 
  Check whether a join buffer can be used to join the specified table   

  SYNOPSIS
    check_join_cache_usage()
      tab                 joined table to check join buffer usage for
      join                join for which the check is performed
      options             options of the join
      no_jbuf_after       don't use join buffering after table with this number
      icp_other_tables_ok OUT TRUE if condition pushdown supports
                          other tables presence

  DESCRIPTION
    The function finds out whether the table 'tab' can be joined using a join
    buffer. This check is performed after the best execution plan for 'join'
    has been chosen. If the function decides that a join buffer can be employed
    then it selects the most appropriate join cache object that contains this
    join buffer.
    The result of the check and the type of the the join buffer to be used
    depend on:
      - the access method to access rows of the joined table
      - whether the join table is an inner table of an outer join or semi-join
      - the join cache level set for the query
      - the join 'options'.
    In any case join buffer is not used if the number of the joined table is
    greater than 'no_jbuf_after'. It's also never used if the value of
    join_cache_level is equal to 0.
    The other valid settings of join_cache_level lay in the interval 1..8.
    If join_cache_level==1|2 then join buffer is used only for inner joins
    with 'JT_ALL' access method.  
    If join_cache_level==3|4 then join buffer is used for any join operation
    (inner join, outer join, semi-join) with 'JT_ALL' access method.
    If 'JT_ALL' access method is used to read rows of the joined table then
    always a JOIN_CACHE_BNL object is employed.
    If an index is used to access rows of the joined table and the value of
    join_cache_level==5|6 then a JOIN_CACHE_BKA object is employed. 
    If an index is used to access rows of the joined table and the value of
    join_cache_level==7|8 then a JOIN_CACHE_BKA_UNIQUE object is employed. 
    If the value of join_cache_level is odd then creation of a non-linked 
    join cache is forced.
    If the function decides that a join buffer can be used to join the table
    'tab' then it sets the value of tab->use_join_buffer to TRUE and assigns
    the selected join cache object to the field 'cache' of the previous
    join table. 
    If the function creates a join cache object it tries to initialize it. The
    failure to do this results in an invocation of the function that destructs
    the created object.
 
  NOTES
    An inner table of a nested outer join or a nested semi-join can be currently
    joined only when a linked cache object is employed. In these cases setting
    join cache level to an odd number results in denial of usage of any join
    buffer when joining the table.
    For a nested outer join/semi-join, currently, we either use join buffers for
    all inner tables or for none of them. 
    Some engines (e.g. Falcon) currently allow to use only a join cache
    of the type JOIN_CACHE_BKA_UNIQUE when the joined table is accessed through
    an index. For these engines setting the value of join_cache_level to 5 or 6
    results in that no join buffer is used to join the table. 
   
  TODO
    Support BKA inside SJ-Materialization nests. When doing this, we'll need
    to only store sj-inner tables in the join buffer.
#if 0
        JOIN_TAB *first_tab= join->join_tab+join->const_tables;
        uint n_tables= i-join->const_tables;
        / *
          We normally put all preceding tables into the join buffer, except
          for the constant tables.
          If we're inside a semi-join materialization nest, e.g.

             outer_tbl1  outer_tbl2  ( inner_tbl1, inner_tbl2 ) ...
                                                       ^-- we're here

          then we need to put into the join buffer only the tables from
          within the nest.
        * /
        if (i >= first_sjm_table && i < last_sjm_table)
        {
          n_tables= i - first_sjm_table; // will be >0 if we got here
          first_tab= join->join_tab + first_sjm_table;
        }
#endif

  RETURN
    Bitmap describing the chosen cache's properties:
    1) the algorithm (JOIN_CACHE::ALG_NONE, JOIN_CACHE::ALG_BNL,
    JOIN_CACHE::ALG_BKA, JOIN_CACHE::ALG_BKA_UNIQUE)
    2) the buffer's type (JOIN_CACHE::NON_INCREMENTAL_BUFFER or not).
*/

static
uint check_join_cache_usage(JOIN_TAB *tab,
                            JOIN *join, ulonglong options,
                            uint no_jbuf_after,
                            bool *icp_other_tables_ok)
{
  uint flags;
  COST_VECT cost;
  ha_rows rows;
  uint bufsz= 4096;
  JOIN_CACHE *prev_cache=0;
  uint cache_level= join->thd->variables.optimizer_join_cache_level;
  uint force_unlinked_cache= (cache_level & 1) ?
    JOIN_CACHE::NON_INCREMENTAL_BUFFER : 0;
  uint i= tab-join->join_tab;
  const uint tab_sj_strategy= tab->get_sj_strategy();
  *icp_other_tables_ok= TRUE;
  
  if (cache_level == 0 || i == join->const_tables)
    return JOIN_CACHE::ALG_NONE;

  if (options & SELECT_NO_JOIN_CACHE)
    goto no_join_cache;
  /* 
    psergey-todo: why the below when execution code seems to handle the
    "range checked for each record" case?
  */
  if (tab->use_quick == QS_DYNAMIC_RANGE)
    goto no_join_cache;
  
  /*
    Use join cache with FirstMatch semi-join strategy only when semi-join
    contains only one table.
  */
  if (tab_sj_strategy == SJ_OPT_FIRST_MATCH &&
      !tab->is_single_inner_of_semi_join())
    goto no_join_cache;
  /*
    Non-linked join buffers can't guarantee one match
  */
  if (force_unlinked_cache &&
      tab->is_inner_table_of_outer_join() &&
      !tab->is_single_inner_of_outer_join())
    goto no_join_cache;

  /* No join buffering if prevented by no_jbuf_after */
  if (i > no_jbuf_after)
    goto no_join_cache;

  /* No join buffering if this semijoin nest is handled by loosescan */
  if (tab_sj_strategy == SJ_OPT_LOOSE_SCAN)
    goto no_join_cache;
      
  /* Neither if semijoin Materialization */
  if (sj_is_materialize_strategy(tab_sj_strategy))
    goto no_join_cache;

  for (JOIN_TAB *first_inner= tab->first_inner; first_inner;
       first_inner= first_inner->first_upper)
  {
    if (first_inner != tab && !first_inner->use_join_cache)
      goto no_join_cache;
  }
  if (tab_sj_strategy == SJ_OPT_FIRST_MATCH &&
      tab->first_sj_inner_tab != tab &&
      !tab->first_sj_inner_tab->use_join_cache)
    goto no_join_cache;
  if (!tab[-1].use_join_cache)
  {
    /* 
      Check whether table tab and the previous one belong to the same nest of
      inner tables and if so do not use join buffer when joining table tab. 
    */
    if (tab->first_inner)
    {
      for (JOIN_TAB *first_inner= tab[-1].first_inner;
           first_inner;
           first_inner= first_inner->first_upper)
      {
        if (first_inner == tab->first_inner)
          goto no_join_cache;
      }
    }
    else if (tab_sj_strategy == SJ_OPT_FIRST_MATCH &&
             tab->first_sj_inner_tab == tab[-1].first_sj_inner_tab)
      goto no_join_cache; 
  }       

  if (!force_unlinked_cache)
    prev_cache= tab[-1].cache;

  switch (tab->type) {
  case JT_ALL:
    if (cache_level <= 2 &&
        (tab->first_inner || tab_sj_strategy == SJ_OPT_FIRST_MATCH))
      goto no_join_cache;
    if ((options & SELECT_DESCRIBE) ||
        ((tab->cache= new JOIN_CACHE_BNL(join, tab, prev_cache)) &&
         !tab->cache->init()))
    {
      *icp_other_tables_ok= FALSE;
      DBUG_ASSERT(might_do_join_buffering(cache_level, tab));
      return JOIN_CACHE::ALG_BNL | force_unlinked_cache;
    }
    goto no_join_cache;
  case JT_SYSTEM:
  case JT_CONST:
  case JT_REF:
  case JT_EQ_REF:
    if (cache_level <= 4)
      goto no_join_cache;
    flags= HA_MRR_NO_NULL_ENDPOINTS;
    if (tab->table->covering_keys.is_set(tab->ref.key))
      flags|= HA_MRR_INDEX_ONLY;
    rows= tab->table->file->multi_range_read_info(tab->ref.key, 10, 20,
                                                  &bufsz, &flags, &cost);
    if ((rows != HA_POS_ERROR) && !(flags & HA_MRR_USE_DEFAULT_IMPL) &&
        (!(flags & HA_MRR_NO_ASSOCIATION) || cache_level > 6) &&
        ((options & SELECT_DESCRIBE) ||
         (((cache_level <= 6 && 
            (tab->cache= new JOIN_CACHE_BKA(join, tab, flags, prev_cache))) ||
           (cache_level > 6 &&  
            (tab->cache= new JOIN_CACHE_BKA_UNIQUE(join, tab, flags, prev_cache)))
           ) && !tab->cache->init())))
    {
      DBUG_ASSERT(might_do_join_buffering(cache_level, tab));
      if (cache_level <= 6)
        return JOIN_CACHE::ALG_BKA | force_unlinked_cache;
      return JOIN_CACHE::ALG_BKA_UNIQUE | force_unlinked_cache;
    }
    goto no_join_cache;
  default : ;
  }

no_join_cache:
  if (cache_level>2)
    revise_cache_usage(tab); 
  return JOIN_CACHE::ALG_NONE;
}

/*
  end_select-compatible function that writes the record into a sjm temptable
  
  SYNOPSIS
    end_sj_materialize()
      join            The join 
      join_tab        Last join table
      end_of_records  FALSE <=> This call is made to pass another record 
                                combination
                      TRUE  <=> EOF (no action)

  DESCRIPTION
    This function is used by semi-join materialization to capture suquery's
    resultset and write it into the temptable (that is, materialize it).

  NOTE
    This function is used only for semi-join materialization. Non-semijoin
    materialization uses different mechanism.

  RETURN 
    NESTED_LOOP_OK
    NESTED_LOOP_ERROR
*/

static enum_nested_loop_state 
end_sj_materialize(JOIN *join, JOIN_TAB *join_tab, bool end_of_records)
{
  int error;
  THD *thd= join->thd;
  Semijoin_mat_exec *sjm= join_tab[-1].emb_sj_nest->sj_mat_exec;
  DBUG_ENTER("end_sj_materialize");
  if (!end_of_records)
  {
    TABLE *table= sjm->table;

    List_iterator<Item> it(sjm->table_cols);
    Item *item;
    while ((item= it++))
    {
      if (item->is_null())
        DBUG_RETURN(NESTED_LOOP_OK);
    }
    fill_record(thd, table->field, sjm->table_cols, 1);
    if (thd->is_error())
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if ((error= table->file->ha_write_row(table->record[0])))
    {
      /* create_myisam_from_heap will generate error if needed */
      if (table->file->is_fatal_error(error, HA_CHECK_DUP) &&
          create_myisam_from_heap(thd, table,
                                  sjm->table_param.start_recinfo, 
                                  &sjm->table_param.recinfo, error,
                                  TRUE, NULL))
        DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
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


void remove_sj_conds(Item **tree)
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


/*
  Create subquery equalities assuming use of materialization strategy
  
  @param thd       Thread handle
  @param sj_nest   Semi-join nest

  @retval <>NULL Created condition
  @retval = NULL Error

  @details
  Create subquery equality predicates. That is, for a subquery
    
    (oe1, oe2, ...) IN (SELECT ie1, ie2, ... FROM ...)
    
  create "oe1=ie1 AND oe2=ie2 AND ..." expression, such that ie1, ie2, ..
  refer to the columns of the table that is used to materialize the subquery.
  This function will also generate proper equality predicates for
  trivially-correlated subqueries corresponding to the above IN query.
*/

Item *create_subquery_equalities(THD *thd, TABLE_LIST *sj_nest)
{
  Item *res= NULL;
  Semijoin_mat_exec *sjm= sj_nest->sj_mat_exec;
  List_iterator<Item> outer_expr(sj_nest->nested_join->sj_outer_exprs);

  for (uint i= 0; i < sj_nest->nested_join->sj_outer_exprs.elements; i++)
  {
    Item *conj;
    Item *outer= outer_expr++;
    if (!(conj= new Item_func_eq(outer, new Item_field(sjm->table->field[i]))))
      return NULL; /* purecov: inspected */
    if (!(res= and_items(res, conj)))
      return NULL; /* purecov: inspected */
  }
  if (res->fix_fields(thd, &res))
    return NULL; /* purecov: inspected */
  return res;
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
  Semijoin_mat_exec *sjm= emb_sj_nest->sj_mat_exec;
  THD *thd= tab->join->thd;
  /* First the calls come to the materialization function */
  List<Item> &item_list= emb_sj_nest->nested_join->sj_inner_exprs;
  /* 
    Set up the table to write to, do as select_union::create_result_table does
  */
  sjm->table_param.init();
  sjm->table_param.field_count= item_list.elements;
  sjm->table_param.bit_fields_as_long= TRUE;
  List_iterator<Item> it(item_list);
  Item *right_expr;
  while((right_expr= it++))
    sjm->table_cols.push_back(right_expr);

  if (!(sjm->table= create_tmp_table(thd, &sjm->table_param, 
                                     sjm->table_cols, (ORDER*) 0, 
                                     TRUE /* distinct */, 
                                     1, /*save_sum_fields*/
                                     thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS, 
                                     HA_POS_ERROR /*rows_limit */, 
                                     (char*)"sj-materialize")))
    DBUG_RETURN(TRUE); /* purecov: inspected */
  sjm->table->file->extra(HA_EXTRA_WRITE_CACHE);
  sjm->table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  tab->join->sj_tmp_tables.push_back(sjm->table);
  tab->join->sjm_exec_list.push_back(sjm);
  
  sjm->materialized= FALSE;
  if (!sjm->is_scan)
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
    if (!(tab_ref= new (thd->mem_root) TABLE_REF))
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

    tab_ref->key_buff2= tab_ref->key_buff+ALIGN_SIZE(tmp_key->key_length);
    tab_ref->null_rejecting= 1;

    KEY_PART_INFO *cur_key_part= tmp_key->key_part;
    store_key **ref_key= tab_ref->key_copy;
    uchar *cur_ref_buff= tab_ref->key_buff;
    List_iterator<Item> outer_expr(emb_sj_nest->nested_join->sj_outer_exprs);

    for (i= 0; i < tmp_key_parts; i++, cur_key_part++, ref_key++)
    {
      tab_ref->items[i]= outer_expr++;
      int null_count= test(cur_key_part->field->real_maybe_null());
      *ref_key= new store_key_item(thd, cur_key_part->field,
                                   /* TODO:
                                      the NULL byte is taken into account in
                                      cur_key_part->store_length, so instead of
                                      cur_ref_buff + test(maybe_null), we could
                                      use that information instead.
                                   */
                                   cur_ref_buff + null_count,
                                   null_count ? cur_ref_buff : 0,
                                   cur_key_part->length, tab_ref->items[i]);
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
    for (i= 0; i < sjm->table_count; i++)
    {
      remove_sj_conds(&tab[i].select_cond);
      if (tab[i].select)
        remove_sj_conds(&tab[i].select->cond);
    }
    if (!(sjm->in_equality= create_subquery_equalities(thd, emb_sj_nest)))
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
      
      Suppose it's executed with MaterializeScan. We choose to do scan
      if we can't do the lookup, i.e. the join order is (it, ot). The plan
      would look as follows:

        table    access method      condition
         it      MaterializeScan     -
         ot      (whatever)          ot1.col1=it.col2 (C2)

      The condition C2 refers to current row of table it. The problem is
      that by the time we evaluate C2, we would have finished with scanning
      it itself and will be scanning the temptable. 

      At the moment, our solution is to copy back: when we get the next
      temptable record, we copy its columns to their corresponding columns
      in the record buffers for the source tables. 
    */
    sjm->copy_field= new Copy_field[sjm->table_cols.elements];
    it.rewind();
    for (uint i=0; i < sjm->table_cols.elements; i++)
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
  Plan refinement stage: do various setup things for the executor

  SYNOPSIS
    make_join_readinfo()
      join           Join being processed
      options        Join's options (checking for SELECT_DESCRIBE, 
                     SELECT_NO_JOIN_CACHE)
      no_jbuf_after  Don't use join buffering after table with this number.

  DESCRIPTION
    Plan refinement stage: do various set ups for the executioner
      - setup join buffering use
      - push index conditions
      - increment relevant counters
      - etc

  RETURN 
    FALSE - OK
    TRUE  - Out of memory
*/

static bool
make_join_readinfo(JOIN *join, ulonglong options, uint no_jbuf_after)
{
  uint i, jcl;
  bool statistics= test(!(join->select_options & SELECT_DESCRIBE));
  bool sorted= 1;
  uint first_sjm_table= MAX_TABLES;
  uint last_sjm_table= MAX_TABLES;
  DBUG_ENTER("make_join_readinfo");

  if (setup_semijoin_dups_elimination(join, options, no_jbuf_after))
    DBUG_RETURN(TRUE); /* purecov: inspected */

  for (i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;
    bool icp_other_tables_ok;
    tab->read_record.table= table;
    tab->read_record.file=table->file;
    tab->read_record.unlock_row= rr_unlock_row;
    tab->next_select=sub_select;		/* normal select */
    tab->use_join_cache= JOIN_CACHE::ALG_NONE;
    tab->cache_idx_cond= 0;
    /* 
      TODO: don't always instruct first table's ref/range access method to 
      produce sorted output.
    */
    tab->sorted= sorted;
    sorted= 0;                                  // only first must be sorted
    table->status=STATUS_NO_RECORD;
    pick_table_access_method (tab);

    if (tab->loosescan_match_tab)
    {
      if (!(tab->loosescan_buf= (uchar*)join->thd->alloc(tab->
                                                         loosescan_key_len)))
        DBUG_RETURN(TRUE); /* purecov: inspected */
    }
    if (sj_is_materialize_strategy(join->best_positions[i].sj_strategy))
    {
      /* This is a start of semi-join nest */
      first_sjm_table= i;
      last_sjm_table= i + join->best_positions[i].n_sj_tables;
      if (i == join->const_tables)
        join->first_select= sub_select_sjm;
      else
       tab[-1].next_select= sub_select_sjm;

      if (setup_sj_materialization(tab))
        DBUG_RETURN(TRUE);
    }
    switch (tab->type) {
    case JT_EQ_REF:
      tab->read_record.unlock_row= join_read_key_unlock_row;
      /* fall through */
    case JT_REF_OR_NULL:
    case JT_REF:
      if (tab->select)
      {
	delete tab->select->quick;
	tab->select->quick=0;
      }
      delete tab->quick;
      tab->quick=0;
      /* fall through */
    case JT_SYSTEM: 
    case JT_CONST:
      /* Only happens with outer joins */
      if ((jcl= check_join_cache_usage(tab, join, options, no_jbuf_after,
                                       &icp_other_tables_ok)))
      {
        tab->use_join_cache= jcl;
        tab[-1].next_select= sub_select_cache;
      }
      if (table->covering_keys.is_set(tab->ref.key) &&
	  !table->no_keyread)
        table->set_keyread(TRUE);
      else
        push_index_cond(tab, tab->ref.key, icp_other_tables_ok);
      break;
    case JT_ALL:
      if ((jcl= check_join_cache_usage(tab, join, options, no_jbuf_after,
                                       &icp_other_tables_ok)))
      {
        tab->use_join_cache= jcl;
        tab[-1].next_select=sub_select_cache;
      }
      /* These init changes read_record */
      if (tab->use_quick == QS_DYNAMIC_RANGE)
      {
	join->thd->server_status|=SERVER_QUERY_NO_GOOD_INDEX_USED;
	tab->read_first_record= join_init_quick_read_record;
	if (statistics)
	  status_var_increment(join->thd->status_var.select_range_check_count);
      }
      else
      {
	tab->read_first_record= join_init_read_record;
	if (i == join->const_tables)
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      status_var_increment(join->thd->status_var.select_range_count);
	  }
	  else
	  {
	    join->thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
	    if (statistics)
	      status_var_increment(join->thd->status_var.select_scan_count);
	  }
	}
	else
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      status_var_increment(join->thd->status_var.select_full_range_join_count);
	  }
	  else
	  {
	    join->thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
	    if (statistics)
	      status_var_increment(join->thd->status_var.select_full_join_count);
	  }
	}
	if (!table->no_keyread)
	{
	  if (tab->select && tab->select->quick &&
              tab->select->quick->index != MAX_KEY && //not index_merge
	      table->covering_keys.is_set(tab->select->quick->index))
            table->set_keyread(TRUE);
	  else if (!table->covering_keys.is_clear_all() &&
		   !(tab->select && tab->select->quick))
	  {					// Only read index tree
	    /*
            It has turned out that the below change, while speeding things
            up for disk-bound loads, slows them down for cases when the data
            is in disk cache (see BUG#35850):
	    //  See bug #26447: "Using the clustered index for a table scan
	    //  is always faster than using a secondary index".
            if (table->s->primary_key != MAX_KEY &&
                table->file->primary_key_is_clustered())
              tab->index= table->s->primary_key;
            else
              tab->index=find_shortest_key(table, & table->covering_keys);
	    */
            if (!tab->loosescan_match_tab)
              tab->index=find_shortest_key(table, & table->covering_keys);
	    tab->read_first_record= join_read_first;
	    tab->type=JT_NEXT;		// Read with index_first / index_next
	  }
	}
        if (tab->select && tab->select->quick &&
            tab->select->quick->index != MAX_KEY && ! tab->table->key_read)
          push_index_cond(tab, tab->select->quick->index, icp_other_tables_ok);
      }
      break;
    case JT_FT:
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

  /* 
    If a join buffer is used to join a table the ordering by an index
    for the first non-constant table cannot be employed anymore.
  */
  for (i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    if (tab->use_join_cache)
    {
      JOIN_TAB *sort_by_tab= join->get_sort_by_join_tab();
      if (sort_by_tab)
      {
        join->need_tmp= 1;
        join->simple_order= join->simple_group= 0;
      }
      break;
    }
  } 

  DBUG_RETURN(FALSE);
}


/**
  Give error if we some tables are done with a full join.

  This is used by multi_table_update and multi_table_delete when running
  in safe mode.

  @param join		Join condition

  @retval
    0	ok
  @retval
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
      /* This error should not be ignored. */
      join->select_lex->no_error= FALSE;
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      return(1);
    }
  }
  return(0);
}


/**
  cleanup JOIN_TAB.
*/

void JOIN_TAB::cleanup()
{
  delete select;
  select= 0;
  delete quick;
  quick= 0;
  if (cache)
  {
    cache->free();
    cache= 0;
  }
  limit= 0;
  if (table)
  {
    table->set_keyread(FALSE);
    table->file->ha_index_or_rnd_end();
    /*
      We need to reset this for next select
      (Tested in part_of_refkey)
    */
    table->reginfo.join_tab= 0;
  }
  end_read_record(&read_record);
}


/**
  @returns semijoin strategy for this table.
*/
uint JOIN_TAB::get_sj_strategy() const
{
  if (first_sj_inner_tab == NULL)
    return SJ_OPT_NONE;
  const int j= first_sj_inner_tab - join->join_tab;
  DBUG_ASSERT(j >= 0);
  uint s= join->best_positions[j].sj_strategy;
  DBUG_ASSERT(s != SJ_OPT_NONE);
  return s;
}


/**
  Partially cleanup JOIN after it has executed: close index or rnd read
  (table cursors), free quick selects.

    This function is called in the end of execution of a JOIN, before the used
    tables are unlocked and closed.

    For a join that is resolved using a temporary table, the first sweep is
    performed against actual tables and an intermediate result is inserted
    into the temprorary table.
    The last sweep is performed against the temporary table. Therefore,
    the base tables and associated buffers used to fill the temporary table
    are no longer needed, and this function is called to free them.

    For a join that is performed without a temporary table, this function
    is called after all rows are sent, but before EOF packet is sent.

    For a simple SELECT with no subqueries this function performs a full
    cleanup of the JOIN and calls mysql_unlock_read_tables to free used base
    tables.

    If a JOIN is executed for a subquery or if it has a subquery, we can't
    do the full cleanup and need to do a partial cleanup only.
    - If a JOIN is not the top level join, we must not unlock the tables
    because the outer select may not have been evaluated yet, and we
    can't unlock only selected tables of a query.
    - Additionally, if this JOIN corresponds to a correlated subquery, we
    should not free quick selects and join buffers because they will be
    needed for the next execution of the correlated subquery.
    - However, if this is a JOIN for a [sub]select, which is not
    a correlated subquery itself, but has subqueries, we can free it
    fully and also free JOINs of all its subqueries. The exception
    is a subquery in SELECT list, e.g: @n
    SELECT a, (select max(b) from t1) group by c @n
    This subquery will not be evaluated at first sweep and its value will
    not be inserted into the temporary table. Instead, it's evaluated
    when selecting from the temporary table. Therefore, it can't be freed
    here even though it's not correlated.

  @todo
    Unlock tables even if the join isn't top level select in the tree
*/

void JOIN::join_free()
{
  SELECT_LEX_UNIT *tmp_unit;
  SELECT_LEX *sl;
  /*
    Optimization: if not EXPLAIN and we are done with the JOIN,
    free all tables.
  */
  bool full= (!select_lex->uncacheable && !thd->lex->describe);
  bool can_unlock= full;
  DBUG_ENTER("JOIN::join_free");

  cleanup(full);

  for (tmp_unit= select_lex->first_inner_unit();
       tmp_unit;
       tmp_unit= tmp_unit->next_unit())
    for (sl= tmp_unit->first_select(); sl; sl= sl->next_select())
    {
      Item_subselect *subselect= sl->master_unit()->item;
      bool full_local= full && (!subselect || subselect->is_evaluated());
      /*
        If this join is evaluated, we can fully clean it up and clean up all
        its underlying joins even if they are correlated -- they will not be
        used any more anyway.
        If this join is not yet evaluated, we still must clean it up to
        close its table cursors -- it may never get evaluated, as in case of
        ... HAVING FALSE OR a IN (SELECT ...))
        but all table cursors must be closed before the unlock.
      */
      sl->cleanup_all_joins(full_local);
      /* Can't unlock if at least one JOIN is still needed */
      can_unlock= can_unlock && full_local;
    }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (can_unlock && lock && thd->lock && ! thd->locked_tables_mode &&
      !(select_options & SELECT_NO_UNLOCK) &&
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


/**
  Free resources of given join.

  @param fill   true if we should free all resources, call with full==1
                should be last, before it this function can be called with
                full==0

  @note
    With subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/

void JOIN::cleanup(bool full)
{
  DBUG_ENTER("JOIN::cleanup");

  if (all_tables)
  {
    JOIN_TAB *tab,*end;
    /*
      Only a sorted table may be cached.  This sorted table is always the
      first non const table in join->all_tables
    */
    if (tables > const_tables) // Test for not-const tables
    {
      free_io_cache(all_tables[const_tables]);
      filesort_free_buffers(all_tables[const_tables],full);
    }

    if (full)
    {
      for (tab= join_tab, end= tab+tables; tab != end; tab++)
	tab->cleanup();
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
      Ensure that the above delete_elements() would not be called
      twice for the same list.
    */
    if (tmp_join && tmp_join != this)
      tmp_join->group_fields= group_fields;
    /*
      We can't call delete_elements() on copy_funcs as this will cause
      problems in free_elements() as some of the elements are then deleted.
    */
    tmp_table_param.copy_funcs.empty();
    /*
      If we have tmp_join and 'this' JOIN is not tmp_join and
      tmp_table_param.copy_field's  of them are equal then we have to remove
      pointer to  tmp_table_param.copy_field from tmp_join, because it qill
      be removed in tmp_table_param.cleanup().
    */
    if (tmp_join &&
        tmp_join != this &&
        tmp_join->tmp_table_param.copy_field ==
        tmp_table_param.copy_field)
    {
      tmp_join->tmp_table_param.copy_field=
        tmp_join->tmp_table_param.save_copy_field= 0;
    }
    tmp_table_param.cleanup();
  }
  DBUG_VOID_RETURN;
}


/**
  Remove the following expressions from ORDER BY and GROUP BY:
  Constant expressions @n
  Expression that only uses tables that are of type EQ_REF and the reference
  is in the ORDER list or if all refereed tables are of the above type.

  In the following, the X field can be removed:
  @code
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t1.a,t2.X
  SELECT * FROM t1,t2,t3 WHERE t1.a=t2.a AND t2.b=t3.b ORDER BY t1.a,t3.X
  @endcode

  These can't be optimized:
  @code
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.X,t1.a
  SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b=t2.b ORDER BY t1.a,t2.c
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.b,t1.a
  @endcode
*/

static bool
eq_ref_table(JOIN *join, ORDER *start_order, JOIN_TAB *tab)
{
  if (tab->cached_eq_ref_table)			// If cached
    return tab->eq_ref_table;
  tab->cached_eq_ref_table=1;
  /* We can skip const tables only if not an outer table */
  if (tab->type == JT_CONST && !tab->first_inner)
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
        if (!(order->used & map))
        {
          found++;
          order->used|= map;
        }
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
  tables&= ~PSEUDO_TABLE_BITS;
  for (JOIN_TAB **tab=join->map2table ; tables ; tab++, tables>>=1)
  {
    if (tables & 1 && !eq_ref_table(join, order, *tab))
      return 0;
  }
  return 1;
}


/** Update the dependency map for the tables. */

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


/** Update the dependency map for the sort order. */

static void update_depend_map(JOIN *join, ORDER *order)
{
  for (; order ; order=order->next)
  {
    table_map depend_map;
    order->item[0]->update_used_tables();
    order->depend_map=depend_map=order->item[0]->used_tables();
    order->used= 0;
    // Not item_sum(), RAND() and no reference to table outside of sub select
    if (!(order->depend_map & (OUTER_REF_TABLE_BIT | RAND_TABLE_BIT))
        && !order->item[0]->with_sum_func)
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


/**
  Remove all constants and check if ORDER only contains simple
  expressions.

  simple_order is set to 1 if sort_order only uses fields from head table
  and the head table is not a LEFT JOIN table.

  @param join			Join handler
  @param first_order		List of SORT or GROUP order
  @param cond			WHERE statement
  @param change_list		Set to 1 if we should remove things from list.
                               If this is not set, then only simple_order is
                               calculated.
  @param simple_order		Set to 1 if we are only using simple expressions

  @return
    Returns new sort order
*/

static ORDER *
remove_const(JOIN *join,ORDER *first_order, Item *cond,
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
    if (order->item[0]->with_sum_func ||
        /*
          If the outer table of an outer join is const (either by itself or
          after applying WHERE condition), grouping on a field from such a
          table will be optimized away and filesort without temporary table
          will be used unless we prevent that now. Filesort is not fit to
          handle joins and the join condition is not applied. We can't detect
          the case without an expensive test, however, so we force temporary
          table for all queries containing more than one table, ROLLUP, and an
          outer join.
         */
        (join->tables > 1 && join->rollup.state == ROLLUP::STATE_INITED &&
        join->outer_join))
      *simple_order=0;				// Must do a temp table to sort
    else if (!(order_tables & not_const_tables))
    {
      if (order->item[0]->with_subselect && 
          !(join->select_lex->options & SELECT_DESCRIBE))
        order->item[0]->val_str(&order->item[0]->str_value);
      DBUG_PRINT("info",("removing: %s", order->item[0]->full_name()));
      continue;					// skip const item
    }
    else
    {
      if (order_tables & (RAND_TABLE_BIT | OUTER_REF_TABLE_BIT))
	*simple_order=0;
      else
      {
	if (cond && const_expression_in_where(cond,order->item[0]))
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


/**
  Filter out ORDER items those are equal to constants in WHERE

  This function is a limited version of remove_const() for use
  with non-JOIN statements (i.e. single-table UPDATE and DELETE).


  @param order            Linked list of ORDER BY arguments
  @param cond             WHERE expression

  @return pointer to new filtered ORDER list or NULL if whole list eliminated

  @note
    This function overwrites input order list.
*/

ORDER *simple_remove_const(ORDER *order, Item *where)
{
  if (!order || !where)
    return order;

  ORDER *first= NULL, *prev= NULL;
  for (; order; order= order->next)
  {
    DBUG_ASSERT(!order->item[0]->with_sum_func); // should never happen
    if (!const_expression_in_where(where, order->item[0]))
    {
      if (!first)
        first= order;
      if (prev)
        prev->next= order;
      prev= order;
    }
  }
  if (prev)
    prev->next= NULL;
  return first;
}


static int
return_zero_rows(JOIN *join, select_result *result,TABLE_LIST *tables,
		 List<Item> &fields, bool send_row, ulonglong select_options,
		 const char *info, Item *having)
{
  DBUG_ENTER("return_zero_rows");

  if (select_options & SELECT_DESCRIBE)
  {
    select_describe(join, FALSE, FALSE, FALSE, info);
    DBUG_RETURN(0);
  }

  join->join_free();

  if (send_row)
  {
    for (TABLE_LIST *table= tables; table; table= table->next_leaf)
      mark_as_null_row(table->table);		// All fields are NULL
    if (having && having->val_int() == 0)
      send_row=0;
  }
  if (!(result->send_result_set_metadata(fields,
                              Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)))
  {
    bool send_error= FALSE;
    if (send_row)
    {
      List_iterator_fast<Item> it(fields);
      Item *item;
      while ((item= it++))
	item->no_rows_in_result();
      send_error= result->send_data(fields);
    }
    if (!send_error)
      result->send_eof();				// Should be safe
  }
  /* Update results for FOUND_ROWS */
  join->thd->limit_found_rows= join->thd->examined_row_count= 0;
  DBUG_RETURN(0);
}

/*
  used only in JOIN::clear
*/
static void clear_tables(JOIN *join)
{
  /* 
    must clear only the non-const tables, as const tables
    are not re-calculated.
  */
  for (uint i=join->const_tables ; i < join->tables ; i++)
    mark_as_null_row(join->all_tables[i]);		// All fields are NULL
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


/**
  Find the multiple equality predicate containing a field.

  The function retrieves the multiple equalities accessed through
  the con_equal structure from current level and up looking for
  an equality containing field. It stops retrieval as soon as the equality
  is found and set up inherited_fl to TRUE if it's found on upper levels.

  @param cond_equal          multiple equalities to search in
  @param field               field to look for
  @param[out] inherited_fl   set up to TRUE if multiple equality is found
                             on upper levels (not on current level of
                             cond_equal)

  @return
    - Item_equal for the found multiple equality predicate if a success;
    - NULL otherwise.
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

  
/**
  Check whether an equality can be used to build multiple equalities.

    This function first checks whether the equality (left_item=right_item)
    is a simple equality i.e. the one that equates a field with another field
    or a constant (field=field_item or field=const_item).
    If this is the case the function looks for a multiple equality
    in the lists referenced directly or indirectly by cond_equal inferring
    the given simple equality. If it doesn't find any, it builds a multiple
    equality that covers the predicate, i.e. the predicate can be inferred
    from this multiple equality.
    The built multiple equality could be obtained in such a way:
    create a binary  multiple equality equivalent to the predicate, then
    merge it, if possible, with one of old multiple equalities.
    This guarantees that the set of multiple equalities covering equality
    predicates will be minimal.

  EXAMPLE:
    For the where condition
    @code
      WHERE a=b AND b=c AND
            (b=2 OR f=e)
    @endcode
    the check_equality will be called for the following equality
    predicates a=b, b=c, b=2 and f=e.
    - For a=b it will be called with *cond_equal=(0,[]) and will transform
      *cond_equal into (0,[Item_equal(a,b)]). 
    - For b=c it will be called with *cond_equal=(0,[Item_equal(a,b)])
      and will transform *cond_equal into CE=(0,[Item_equal(a,b,c)]).
    - For b=2 it will be called with *cond_equal=(ptr(CE),[])
      and will transform *cond_equal into (ptr(CE),[Item_equal(2,a,b,c)]).
    - For f=e it will be called with *cond_equal=(ptr(CE), [])
      and will transform *cond_equal into (ptr(CE),[Item_equal(f,e)]).

  @note
    Now only fields that have the same type definitions (verified by
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

  @param left_item   left term of the quality to be checked
  @param right_item  right term of the equality to be checked
  @param item        equality item if the equality originates from a condition
                     predicate, 0 if the equality is the result of row
                     elimination
  @param cond_equal  multiple equalities that must hold together with the
                     equality

  @retval
    TRUE    if the predicate is a simple equality predicate to be used
    for building multiple equalities
  @retval
    FALSE   otherwise
*/

static bool check_simple_equality(Item *left_item, Item *right_item,
                                  Item *item, COND_EQUAL *cond_equal)
{
  if (left_item->type() == Item::REF_ITEM &&
      ((Item_ref*)left_item)->ref_type() == Item_ref::VIEW_REF)
  {
    if (((Item_ref*)left_item)->depended_from)
      return FALSE;
    left_item= left_item->real_item();
  }
  if (right_item->type() == Item::REF_ITEM &&
      ((Item_ref*)right_item)->ref_type() == Item_ref::VIEW_REF)
  {
    if (((Item_ref*)right_item)->depended_from)
      return FALSE;
    right_item= right_item->real_item();
  }
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

    /* Search for multiple equalities containing field1 and/or field2 */
    bool left_copyfl, right_copyfl;
    Item_equal *left_item_equal=
               find_item_equal(cond_equal, left_field, &left_copyfl);
    Item_equal *right_item_equal= 
               find_item_equal(cond_equal, right_field, &right_copyfl);

    /* As (NULL=NULL) != TRUE we can't just remove the predicate f=f */
    if (left_field->eq(right_field)) /* f = f */
      return (!(left_field->maybe_null() && !left_item_equal)); 

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
        while ((li++) != right_item_equal) ;
        li.remove();
      }
    }
    else
    { 
      /* left item was not found neither the current nor in upper levels  */
      if (right_item_equal)
      {
        right_item_equal->add((Item_field *) left_item);
      }
      else 
      {
        /* None of the fields was found in multiple equalities */
        Item_equal *item_equal= new Item_equal((Item_field *) left_item,
                                               (Item_field *) right_item);
        cond_equal->current_level.push_back(item_equal);
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
        if (!item)
        {
          Item_func_eq *eq_item;
          if ((eq_item= new Item_func_eq(left_item, right_item)))
            return FALSE;
          eq_item->set_cmp_func();
          eq_item->quick_fix_field();
          item= eq_item;
        }  
        if ((cs != ((Item_func *) item)->compare_collation()) ||
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
        item_equal->add(const_item, field_item);
      }
      else
      {
        item_equal= new Item_equal(const_item, field_item);
        cond_equal->current_level.push_back(item_equal);
      }
      return TRUE;
    }
  }
  return FALSE;
}


/**
  Convert row equalities into a conjunction of regular equalities.

    The function converts a row equality of the form (E1,...,En)=(E'1,...,E'n)
    into a list of equalities E1=E'1,...,En=E'n. For each of these equalities
    Ei=E'i the function checks whether it is a simple equality or a row
    equality. If it is a simple equality it is used to expand multiple
    equalities of cond_equal. If it is a row equality it converted to a
    sequence of equalities between row elements. If Ei=E'i is neither a
    simple equality nor a row equality the item for this predicate is added
    to eq_list.

  @param thd        thread handle
  @param left_row   left term of the row equality to be processed
  @param right_row  right term of the row equality to be processed
  @param cond_equal multiple equalities that must hold together with the
                    predicate
  @param eq_list    results of conversions of row equalities that are not
                    simple enough to form multiple equalities

  @retval
    TRUE    if conversion has succeeded (no fatal error)
  @retval
    FALSE   otherwise
*/
 
static bool check_row_equality(THD *thd, Item *left_row, Item_row *right_row,
                               COND_EQUAL *cond_equal, List<Item>* eq_list)
{ 
  uint n= left_row->cols();
  for (uint i= 0 ; i < n; i++)
  {
    bool is_converted;
    Item *left_item= left_row->element_index(i);
    Item *right_item= right_row->element_index(i);
    if (left_item->type() == Item::ROW_ITEM &&
        right_item->type() == Item::ROW_ITEM)
    {
      is_converted= check_row_equality(thd, 
                                       (Item_row *) left_item,
                                       (Item_row *) right_item,
			               cond_equal, eq_list);
      if (!is_converted)
        thd->lex->current_select->cond_count++;      
    }
    else
    { 
      is_converted= check_simple_equality(left_item, right_item, 0, cond_equal);
      thd->lex->current_select->cond_count++;
    }  
 
    if (!is_converted)
    {
      Item_func_eq *eq_item;
      if (!(eq_item= new Item_func_eq(left_item, right_item)))
        return FALSE;
      eq_item->set_cmp_func();
      eq_item->quick_fix_field();
      eq_list->push_back(eq_item);
    }
  }
  return TRUE;
}


/**
  Eliminate row equalities and form multiple equalities predicates.

    This function checks whether the item is a simple equality
    i.e. the one that equates a field with another field or a constant
    (field=field_item or field=constant_item), or, a row equality.
    For a simple equality the function looks for a multiple equality
    in the lists referenced directly or indirectly by cond_equal inferring
    the given simple equality. If it doesn't find any, it builds/expands
    multiple equality that covers the predicate.
    Row equalities are eliminated substituted for conjunctive regular
    equalities which are treated in the same way as original equality
    predicates.

  @param thd        thread handle
  @param item       predicate to process
  @param cond_equal multiple equalities that must hold together with the
                    predicate
  @param eq_list    results of conversions of row equalities that are not
                    simple enough to form multiple equalities

  @retval
    TRUE   if re-writing rules have been applied
  @retval
    FALSE  otherwise, i.e.
           if the predicate is not an equality,
           or, if the equality is neither a simple one nor a row equality,
           or, if the procedure fails by a fatal error.
*/

static bool check_equality(THD *thd, Item *item, COND_EQUAL *cond_equal,
                           List<Item> *eq_list)
{
  if (item->type() == Item::FUNC_ITEM &&
         ((Item_func*) item)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item= ((Item_func*) item)->arguments()[0];
    Item *right_item= ((Item_func*) item)->arguments()[1];

    if (left_item->type() == Item::ROW_ITEM &&
        right_item->type() == Item::ROW_ITEM)
    {
      thd->lex->current_select->cond_count--;
      return check_row_equality(thd,
                                (Item_row *) left_item,
                                (Item_row *) right_item,
                                cond_equal, eq_list);
    }
    else 
      return check_simple_equality(left_item, right_item, item, cond_equal);
  } 
  return FALSE;
}

                          
/**
  Replace all equality predicates in a condition by multiple equality items.

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
    function replaces the field reference by the constant in the cases 
    when the field is not of a string type or when the field reference is
    just an argument of a comparison predicate.
    The function also determines the maximum number of members in 
    equality lists of each Item_cond_and object assigning it to
    thd->lex->current_select->max_equal_elems.

  @note
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

  @param thd        thread handle
  @param cond       condition(expression) where to make replacement
  @param inherited  path to all inherited multiple equality items

  @return
    pointer to the transformed condition
*/

static Item *build_equal_items_for_cond(THD *thd, Item *cond,
                                        COND_EQUAL *inherited)
{
  Item_equal *item_equal;
  COND_EQUAL cond_equal;
  cond_equal.upper_levels= inherited;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> eq_list;
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List<Item> *args= ((Item_cond*) cond)->argument_list();
    
    List_iterator<Item> li(*args);
    Item *item;

    if (and_level)
    {
      /*
         Retrieve all conjuncts of this level detecting the equality
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
        if (check_equality(thd, item, &cond_equal, &eq_list))
          li.remove();
      }

      /*
        Check if we eliminated all the predicates of the level, e.g.
        (a=a AND b=b AND a=a).
      */
      if (!args->elements && 
          !cond_equal.current_level.elements && 
          !eq_list.elements)
        return new Item_int((longlong) 1, 1);

      List_iterator_fast<Item_equal> it(cond_equal.current_level);
      while ((item_equal= it++))
      {
        item_equal->fix_length_and_dec();
        item_equal->update_used_tables();
        set_if_bigger(thd->lex->current_select->max_equal_elems,
                      item_equal->members());  
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
      if ((new_item= build_equal_items_for_cond(thd, item, inherited)) != item)
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
    {
      args->concat(&eq_list);
      args->concat((List<Item> *)&cond_equal.current_level);
    }
  }
  else if (cond->type() == Item::FUNC_ITEM)
  {
    List<Item> eq_list;
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
    if (check_equality(thd, cond, &cond_equal, &eq_list))
    {
      int n= cond_equal.current_level.elements + eq_list.elements;
      if (n == 0)
        return new Item_int((longlong) 1,1);
      else if (n == 1)
      {
        if ((item_equal= cond_equal.current_level.pop()))
        {
          item_equal->fix_length_and_dec();
          item_equal->update_used_tables();
          set_if_bigger(thd->lex->current_select->max_equal_elems,
                        item_equal->members());  
          return item_equal;
	}

        return eq_list.pop();
      }
      else
      {
        /* 
          Here a new AND level must be created. It can happen only
          when a row equality is processed as a standalone predicate.
	*/
        Item_cond_and *and_cond= new Item_cond_and(eq_list);
        and_cond->quick_fix_field();
        List<Item> *args= and_cond->argument_list();
        List_iterator_fast<Item_equal> it(cond_equal.current_level);
        while ((item_equal= it++))
        {
          item_equal->fix_length_and_dec();
          item_equal->update_used_tables();
          set_if_bigger(thd->lex->current_select->max_equal_elems,
                        item_equal->members());  
        }
        and_cond->cond_equal= cond_equal;
        args->concat((List<Item> *)&cond_equal.current_level);
        
        return and_cond;
      }
    }
    /* 
      For each field reference in cond, not from equal item predicates,
      set a pointer to the multiple equality it belongs to (if there is any)
      as soon the field is not of a string type or the field reference is
      an argument of a comparison predicate. 
    */ 
    uchar *is_subst_valid= (uchar *) 1;
    cond= cond->compile(&Item::subst_argument_checker,
                        &is_subst_valid, 
                        &Item::equal_fields_propagator,
                        (uchar *) inherited);
    cond->update_used_tables();
  }
  return cond;
}


/**
  Build multiple equalities for a condition and all on expressions that
  inherit these multiple equalities.

    The function first applies the build_equal_items_for_cond function
    to build all multiple equalities for condition cond utilizing equalities
    referred through the parameter inherited. The extended set of
    equalities is returned in the structure referred by the cond_equal_ref
    parameter. After this the function calls itself recursively for
    all on expressions whose direct references can be found in join_list
    and who inherit directly the multiple equalities just having built.

  @note
    The on expression used in an outer join operation inherits all equalities
    from the on expression of the embedding join, if there is any, or
    otherwise - from the where condition.
    This fact is not obvious, but presumably can be proved.
    Consider the following query:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t2.a=t4.a
        WHERE t1.a=t2.a;
    @endcode
    If the on expression in the query inherits =(t1.a,t2.a), then we
    can build the multiple equality =(t1.a,t2.a,t3.a,t4.a) that infers
    the equality t3.a=t4.a. Although the on expression
    t1.a=t3.a AND t2.a=t4.a AND t3.a=t4.a is not equivalent to the one
    in the query the latter can be replaced by the former: the new query
    will return the same result set as the original one.

    Interesting that multiple equality =(t1.a,t2.a,t3.a,t4.a) allows us
    to use t1.a=t3.a AND t3.a=t4.a under the on condition:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    @endcode
    This query equivalent to:
    @code
      SELECT * FROM (t1 LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a),t2
        WHERE t1.a=t2.a
    @endcode
    Similarly the original query can be rewritten to the query:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t2.a=t4.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    @endcode
    that is equivalent to:   
    @code
      SELECT * FROM (t2 LEFT JOIN (t3,t4)ON t2.a=t4.a AND t3.a=t4.a), t1
        WHERE t1.a=t2.a
    @endcode
    Thus, applying equalities from the where condition we basically
    can get more freedom in performing join operations.
    Althogh we don't use this property now, it probably makes sense to use 
    it in the future.    
  @param thd		      Thread handler
  @param cond                condition to build the multiple equalities for
  @param inherited           path to all inherited multiple equality items
  @param join_list           list of join tables to which the condition
                             refers to
  @param[out] cond_equal_ref pointer to the structure to place built
                             equalities in

  @return
    pointer to the transformed condition containing multiple equalities
*/
   
static Item *build_equal_items(THD *thd, Item *cond,
                               COND_EQUAL *inherited,
                               List<TABLE_LIST> *join_list,
                               COND_EQUAL **cond_equal_ref)
{
  COND_EQUAL *cond_equal= 0;

  if (cond) 
  {
    cond= build_equal_items_for_cond(thd, cond, inherited);
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
        List<TABLE_LIST> *nested_join_list= table->nested_join ?
          &table->nested_join->join_list : NULL;
        /*
          We can modify table->on_expr because its old value will
          be restored before re-execution of PS/SP.
        */
        table->on_expr= build_equal_items(thd, table->on_expr, inherited,
                                          nested_join_list,
                                          &table->cond_equal);
      }
    }
  }

  return cond;
}    


/**
  Compare field items by table order in the execution plan.

    field1 considered as better than field2 if the table containing
    field1 is accessed earlier than the table containing field2.   
    The function finds out what of two fields is better according
    this criteria.

  @param field1          first field item to compare
  @param field2          second field item to compare
  @param table_join_idx  index to tables determining table order

  @retval
    1  if field1 is better than field2
  @retval
    -1  if field2 is better than field1
  @retval
    0  otherwise
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


/**
  Generate minimal set of simple equalities equivalent to a multiple equality.

    The function retrieves the fields of the multiple equality item
    item_equal and  for each field f:
    - if item_equal contains const it generates the equality f=const_item;
    - otherwise, if f is not the first field, generates the equality
      f=item_equal->get_first().
    All generated equality are added to the cond conjunction.

  @param cond            condition to add the generated equality to
  @param upper_levels    structure to access multiple equality of upper levels
  @param item_equal      multiple equality to generate simple equality from

  @note
    Before generating an equality function checks that it has not
    been generated for multiple equalities of the upper levels.
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

  @return
    - The condition with generated simple equalities or
    a pointer to the simple generated equality, if success.
    - 0, otherwise.
*/

Item *eliminate_item_equal(Item *cond, COND_EQUAL *upper_levels,
                           Item_equal *item_equal)
{
  List<Item> eq_list;
  Item_func_eq *eq_item= NULL;
  if (((Item *) item_equal)->const_item() && !item_equal->val_int())
    return new Item_int((longlong) 0,1); 
  Item *item_const= item_equal->get_const();
  Item_equal_iterator it(*item_equal);
  Item *head;
  if (!item_const)
  {
    /*
      If there is a const item, match all field items with the const item,
      otherwise match the second and subsequent field items with the first one:
    */
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
        item= NULL;
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

      /*
        item_field may refer to a table that is within a semijoin
        materialization nest. In that case, the join order may look like:

          ot1 ot2 SJM (it3 it4) ot5 

        If we have a multiple equality (ot1.c1, ot2.c2, it3.c3, it4.c4, ot5.c5),
        we should generate the following equalities:
         1. ot1.c1 = ot2.c2
         2. ot1.c1 = it3.c3
         3. it3.c3 = it4.c4
         4. ot1.c1 = ot5.c5

        Equalities 1) and 4) are regular equalities between two outer tables.
        Equality 2) is an equality that matches the outer query with a
        materialized semijoin table. It is either performed as a lookup
        into the materialized table (SJM-lookup), or as a condition on the
        outer table (SJM-scan).
        Equality 3) is evaluated during semijoin materialization.

        If there is a const item, match against this one.
        Otherwise, match against the first field item in the multiple equality,
        unless the item is within a materialized semijoin nest, where we match
        against the first item within the SJM nest (if the item is not the first
        item within the SJM nest), or match against the first item in the
        list (if the item is the first one in the SJM nest).
      */
      head= item_const ? item_const : item_equal->get_subst_item(item_field);
      if (head == item_field)                   // First item in SJM nest
        head= item_equal->get_first();

      eq_item= new Item_func_eq(item_field, head);
      if (!eq_item)
        return NULL;

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
  {
    DBUG_ASSERT(cond->type() == Item::COND_ITEM);
    if (eq_list.elements)
      ((Item_cond *) cond)->add_at_head(&eq_list);
  }

  cond->quick_fix_field();
  cond->update_used_tables();
   
  return cond;
}


/**
  Substitute every field reference in a condition by the best equal field
  and eliminate all multiple equality predicates.

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

  @param cond            condition to process
  @param cond_equal      multiple equalities to take into consideration
  @param table_join_idx  index to tables determining field preference

  @note
    At the first glance full sort of fields in multiple equality
    seems to be an overkill. Yet it's not the case due to possible
    new fields in multiple equality item of lower levels. We want
    the order in them to comply with the order of upper levels.

  @return
    The transformed condition
*/

static Item* substitute_for_best_equal_field(Item *cond,
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
        // This occurs when eliminate_item_equal() founds that cond is
        // always false and substitutes it with Item_int 0.
        // Due to this, value of item_equal will be 0, so just return it.
        if (cond->type() != Item::COND_ITEM)
          break;
      }
    }
    if (cond->type() == Item::COND_ITEM &&
        !((Item_cond*)cond)->argument_list()->elements)
      cond= new Item_int((int32)cond->val_bool());

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


/**
  Check appearance of new constant items in multiple equalities
  of a condition after reading a constant table.

    The function retrieves the cond condition and for each encountered
    multiple equality checks whether new constants have appeared after
    reading the constant (single row) table tab. If so it adjusts
    the multiple equality appropriately.

  @param cond       condition whose multiple equalities are to be checked
  @param table      constant table that has been read
*/

static void update_const_equal_items(Item *cond, JOIN_TAB *tab)
{
  if (!(cond->used_tables() & tab->table->map))
    return;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> *cond_list= ((Item_cond*) cond)->argument_list(); 
    List_iterator_fast<Item> li(*cond_list);
    Item *item;
    while ((item= li++))
      update_const_equal_items(item, tab);
  }
  else if (cond->type() == Item::FUNC_ITEM && 
           ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
  {
    Item_equal *item_equal= (Item_equal *) cond;
    bool contained_const= item_equal->get_const() != NULL;
    item_equal->update_const();
    if (!contained_const && item_equal->get_const())
    {
      /* Update keys for range analysis */
      Item_equal_iterator it(*item_equal);
      Item_field *item_field;
      while ((item_field= it++))
      {
        Field *field= item_field->field;
        JOIN_TAB *stat= field->table->reginfo.join_tab;
        key_map possible_keys= field->key_start;
        possible_keys.intersect(field->table->keys_in_use_for_query);
        stat[0].const_keys.merge(possible_keys);

        /*
          For each field in the multiple equality (for which we know that it 
          is a constant) we have to find its corresponding key part, and set 
          that key part in const_key_parts.
        */  
        if (!possible_keys.is_clear_all())
        {
          TABLE *tab= field->table;
          Key_use *use;
          for (use= stat->keyuse; use && use->table == tab; use++)
            if (possible_keys.is_set(use->key) && 
                tab->key_info[use->key].key_part[use->keypart].field ==
                field)
              tab->const_key_parts[use->key]|= use->keypart_map;
        }
      }
    }
  }
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
      right_item->cmp_context == field->cmp_context &&
      (left_item->result_type() != STRING_RESULT ||
       value->result_type() != STRING_RESULT ||
       left_item->collation.collation == value->collation.collation))
  {
    Item *tmp=value->clone_item();
    if (tmp)
    {
      tmp->collation.set(right_item->collation);
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
           left_item->cmp_context == field->cmp_context &&
           (right_item->result_type() != STRING_RESULT ||
            value->result_type() != STRING_RESULT ||
            right_item->collation.collation == value->collation.collation))
  {
    Item *tmp= value->clone_item();
    if (tmp)
    {
      tmp->collation.set(left_item->collation);
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

static void
propagate_cond_constants(THD *thd, I_List<COND_CMP> *save_list,
                         Item *and_father, Item *cond)
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


/**
  Simplify joins replacing outer joins by inner joins whenever it's
  possible.

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

  @note
    An outer join can be replaced by an inner join if the where condition
    or the on expression for an embedding nested join contains a conjunctive
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

  @param join        reference to the query info
  @param join_list   list representation of the join to be converted
  @param conds       conditions to add on expressions for converted joins
  @param top         true <=> conds is the where condition
  @param in_sj       TRUE <=> processing semi-join nest's children
  @return
    - The new condition, if success
    - 0, otherwise
*/

static Item *
simplify_joins(JOIN *join, List<TABLE_LIST> *join_list, Item *conds, bool top,
               bool in_sj)
{
  TABLE_LIST *table;
  NESTED_JOIN *nested_join;
  TABLE_LIST *prev_table= 0;
  List_iterator<TABLE_LIST> li(*join_list);
  bool straight_join= test(join->select_options & SELECT_STRAIGHT_JOIN);
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
        Item *expr= table->on_expr;
        /* 
           If an on expression E is attached to the table, 
           check all null rejected predicates in this expression.
           If such a predicate over an attribute belonging to
           an inner table of an embedded outer join is found,
           the outer join is converted to an inner join and
           the corresponding on expression is added to E. 
	*/ 
        expr= simplify_joins(join, &nested_join->join_list,
                             expr, FALSE, in_sj || table->sj_on_expr);

        if (!table->prep_on_expr || expr != table->on_expr)
        {
          DBUG_ASSERT(expr);

          table->on_expr= expr;
          table->prep_on_expr= expr->copy_andor_structure(join->thd);
        }
      }
      nested_join->used_tables= (table_map) 0;
      nested_join->not_null_tables=(table_map) 0;
      conds= simplify_joins(join, &nested_join->join_list, conds, top, 
                            in_sj || table->sj_on_expr);
      used_tables= nested_join->used_tables;
      not_null_tables= nested_join->not_null_tables;  
    }
    else
    {
      if (!table->prep_on_expr)
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
        /* Add ON expression to the WHERE or upper-level ON condition. */
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
      if (prev_table->straight || straight_join)
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
          Note:
          RAND_TABLE_BIT mask should not be counted as it
          prevents update of inner table dependences.
          For example it might happen if RAND() function
          is used in JOIN ON clause.
	*/  
        if (!((prev_table->on_expr->used_tables() & ~RAND_TABLE_BIT) &
              ~prev_used_tables))
          prev_table->dep_tables|= used_tables;
      }
    }
    prev_table= table;
  }
    
  TABLE_LIST *right_neighbor= NULL;
  /* 
    Flatten nested joins that can be flattened.
    no ON expression and not a semi-join => can be flattened.
  */
  li.rewind();
  while ((table= li++))
  {
    bool fix_name_res= FALSE;
    nested_join= table->nested_join;
    if (table->sj_on_expr && !in_sj)
    {
       /*
         If this is a semi-join that is not contained within another semi-join, 
         leave it intact (otherwise it is flattened)
       */
      join->select_lex->sj_nests.push_back(table);
    }
    else if (nested_join && !table->on_expr)
    {
      TABLE_LIST *tbl;
      List_iterator<TABLE_LIST> it(nested_join->join_list);
      while ((tbl= it++))
      {
        tbl->embedding= table->embedding;
        tbl->join_list= table->join_list;
      }
      li.replace(nested_join->join_list);
      /* Need to update the name resolution table chain when flattening joins */
      fix_name_res= TRUE;
      table= *li.ref();
    }
    if (fix_name_res)
      table->next_name_resolution_table= right_neighbor ?
        right_neighbor->first_leaf_for_name_resolution() :
        NULL;
    right_neighbor= table;
  }
  DBUG_RETURN(conds); 
}


/**
  Assign each nested join structure a bit in nested_join_map.

    Assign each nested join structure (except "confluent" ones - those that
    embed only one element) a bit in nested_join_map.

  @param join          Join being processed
  @param join_list     List of tables
  @param first_unused  Number of first unused bit in nested_join_map before the
                       call

  @note
    This function is called after simplify_joins(), when there are no
    redundant nested joins, #non_confluent_nested_joins <= #tables_in_join so
    we will not run out of bits in nested_join_map.

  @return
    First unused bit in nested_join_map after the call.
*/

static uint build_bitmap_for_nested_joins(List<TABLE_LIST> *join_list, 
                                          uint first_unused)
{
  List_iterator<TABLE_LIST> li(*join_list);
  TABLE_LIST *table;
  DBUG_ENTER("build_bitmap_for_nested_joins");
  while ((table= li++))
  {
    NESTED_JOIN *nested_join;
    if ((nested_join= table->nested_join))
    {
      /*
        It is guaranteed by simplify_joins() function that a nested join
        that has only one child is either
         - a single-table view (the child is the underlying table), or 
         - a single-table semi-join nest

        We don't assign bits to such sj-nests because 
        1. it is redundant (a "sequence" of one table cannot be interleaved 
            with anything)
        2. we could run out bits in nested_join_map otherwise.
      */
      if (nested_join->join_list.elements != 1)
      {
        /* Don't assign bits to sj-nests */
        if (table->on_expr)
          nested_join->nj_map= (nested_join_map) 1 << first_unused++;
        first_unused= build_bitmap_for_nested_joins(&nested_join->join_list,
                                                    first_unused);
      }
    }
  }
  DBUG_RETURN(first_unused);
}


/**
  Set NESTED_JOIN::counter=0 in all nested joins in passed list.

    Recursively set NESTED_JOIN::counter=0 for all nested joins contained in
    the passed join_list.

  @param join_list  List of nested joins to process. It may also contain base
                    tables which will be ignored.
*/

static void reset_nj_counters(List<TABLE_LIST> *join_list)
{
  List_iterator<TABLE_LIST> li(*join_list);
  TABLE_LIST *table;
  DBUG_ENTER("reset_nj_counters");
  while ((table= li++))
  {
    NESTED_JOIN *nested_join;
    if ((nested_join= table->nested_join))
    {
      nested_join->counter_= 0;
      reset_nj_counters(&nested_join->join_list);
    }
  }
  DBUG_VOID_RETURN;
}


/**
  Check interleaving with an inner tables of an outer join for
  extension table.

    Check if table next_tab can be added to current partial join order, and 
    if yes, record that it has been added. This recording can be rolled back
    with backout_nj_sj_state().

    The function assumes that both current partial join order and its
    extension with next_tab are valid wrt table dependencies.

  @verbatim
     IMPLEMENTATION 
       LIMITATIONS ON JOIN ORDER
         The nested [outer] joins executioner algorithm imposes these limitations
         on join order:
         1. "Outer tables first" -  any "outer" table must be before any 
             corresponding "inner" table.
         2. "No interleaving" - tables inside a nested join must form a continuous
            sequence in join order (i.e. the sequence must not be interrupted by 
            tables that are outside of this nested join).

         #1 is checked elsewhere, this function checks #2 provided that #1 has
         been already checked.

       WHY NEED NON-INTERLEAVING
         Consider an example: 

           select * from t0 join t1 left join (t2 join t3) on cond1

         The join order "t1 t2 t0 t3" is invalid:

         table t0 is outside of the nested join, so WHERE condition for t0 is
         attached directly to t0 (without triggers, and it may be used to access
         t0). Applying WHERE(t0) to (t2,t0,t3) record is invalid as we may miss
         combinations of (t1, t2, t3) that satisfy condition cond1, and produce a
         null-complemented (t1, t2.NULLs, t3.NULLs) row, which should not have
         been produced.

         If table t0 is not between t2 and t3, the problem doesn't exist:
          If t0 is located after (t2,t3), WHERE(t0) is applied after nested join
           processing has finished.
          If t0 is located before (t2,t3), predicates like WHERE_cond(t0, t2) are
           wrapped into condition triggers, which takes care of correct nested
           join processing.

       HOW IT IS IMPLEMENTED
         The limitations on join order can be rephrased as follows: for valid
         join order one must be able to:
           1. write down the used tables in the join order on one line.
           2. for each nested join, put one '(' and one ')' on the said line        
           3. write "LEFT JOIN" and "ON (...)" where appropriate
           4. get a query equivalent to the query we're trying to execute.

         Calls to check_interleaving_with_nj() are equivalent to writing the
         above described line from left to right. 
         A single check_interleaving_with_nj(A,B) call is equivalent to writing 
         table B and appropriate brackets on condition that table A and
         appropriate brackets is the last what was written. Graphically the
         transition is as follows:

                              +---- current position
                              |
             ... last_tab ))) | ( next_tab )  )..) | ...
                                X          Y   Z   |
                                                   +- need to move to this
                                                      position.

         Notes about the position:
           The caller guarantees that there is no more then one X-bracket by 
           checking "!(remaining_tables & s->dependent)" before calling this 
           function. X-bracket may have a pair in Y-bracket.

         When "writing" we store/update this auxilary info about the current
         position:
          1. join->cur_embedding_map - bitmap of pairs of brackets (aka nested
             joins) we've opened but didn't close.
          2. {each NESTED_JOIN structure not simplified away}->counter - number
             of this nested join's children that have already been added to to
             the partial join order.
  @endverbatim

  @param next_tab   Table we're going to extend the current partial join with

  @retval
    FALSE  Join order extended, nested joins info about current join
    order (see NOTE section) updated.
  @retval
    TRUE   Requested join order extension not allowed.
*/

static bool check_interleaving_with_nj(JOIN_TAB *next_tab)
{
  TABLE_LIST *next_emb= next_tab->table->pos_in_table_list->embedding;
  JOIN *join= next_tab->join;

  if (join->cur_embedding_map & ~next_tab->embedding_map)
  {
    /* 
      next_tab is outside of the "pair of brackets" we're currently in.
      Cannot add it.
    */
    return TRUE;
  }
   
  /*
    Do update counters for "pairs of brackets" that we've left (marked as
    X,Y,Z in the above picture)
  */
  for (;next_emb; next_emb= next_emb->embedding)
  {
    next_emb->nested_join->counter_++;
    if (next_emb->nested_join->counter_ == 1)
    {
      /* 
        next_emb is the first table inside a nested join we've "entered". In
        the picture above, we're looking at the 'X' bracket. Don't exit yet as
        X bracket might have Y pair bracket.
      */
      join->cur_embedding_map |= next_emb->nested_join->nj_map;
    }
    
    if (next_emb->nested_join->join_list.elements !=
        next_emb->nested_join->counter_)
      break;

    /*
      We're currently at Y or Z-bracket as depicted in the above picture.
      Mark that we've left it and continue walking up the brackets hierarchy.
    */
    join->cur_embedding_map &= ~next_emb->nested_join->nj_map;
  }
  return FALSE;
}


/*
  Change access methods not to use join buffering and adjust costs accordingly

  SYNOPSIS
    optimize_wo_join_buffering()
      join
      first_tab               The first tab to do re-optimization for
      last_tab                The last tab to do re-optimization for
      last_remaining_tables   Bitmap of tables that are not in the
                              [0...last_tab] join prefix
      first_alt               TRUE <=> Use the LooseScan plan for the first_tab
      no_jbuf_before          Don't allow to use join buffering before this
                              table
      reopt_rec_count     OUT New output record count
                              DBL_MAX if we could find no plan.
      reopt_cost          OUT New join prefix cost
                              DBL_MAX if we could find no plan.

  DESCRIPTION
    Given a join prefix [0; ... first_tab], change the access to the tables
    in the [first_tab; last_tab] not to use join buffering. This is needed
    because some semi-join strategies cannot be used together with the join
    buffering.
    In general case the best table order in [first_tab; last_tab] range with
    join buffering is different from the best order without join buffering but
    we don't try finding a better join order. (TODO ask Igor why did we
    chose not to do this in the end. that's actually the difference from the 
    forking approach)
*/

void optimize_wo_join_buffering(JOIN *join, uint first_tab, uint last_tab, 
                                table_map last_remaining_tables, 
                                bool first_alt, uint no_jbuf_before,
                                double *reopt_rec_count, double *reopt_cost)
{
  double cost, outer_fanout, inner_fanout= 1.0;
  table_map reopt_remaining_tables= last_remaining_tables;
  uint i;

  DBUG_ENTER("optimize_wo_join_buffering");

  if (first_tab > join->const_tables)
  {
    cost=      join->positions[first_tab - 1].prefix_cost.total_cost();
    outer_fanout= join->positions[first_tab - 1].prefix_record_count;
  }
  else
  {
    cost= 0.0;
    outer_fanout= 1.0;
  }

  for (i= first_tab; i <= last_tab; i++)
    reopt_remaining_tables |= join->positions[i].table->table->map;

  for (i= first_tab; i <= last_tab; i++)
  {
    JOIN_TAB *rs= join->positions[i].table;
    POSITION pos, loose_scan_pos;
    
    if ((i == first_tab && first_alt) || join->positions[i].use_join_buffer)
    {
      /* Find the best access method that would not use join buffering */
      best_access_path(join, rs, reopt_remaining_tables, i, 
                       i < no_jbuf_before, inner_fanout * outer_fanout,
                       &pos, &loose_scan_pos);
    }
    else 
      pos= join->positions[i];

    if (i == first_tab && first_alt)
      pos= loose_scan_pos;

    /*
      Terminate search if best_access_path found no possible plan.
      Otherwise we will be getting infinite cost when summing up below.
     */
    if (pos.read_time == DBL_MAX)
    {
      *reopt_rec_count= DBL_MAX;
      *reopt_cost= DBL_MAX;
      DBUG_VOID_RETURN;
    }

    reopt_remaining_tables &= ~rs->table->map;
    cost += pos.read_time;

    if (rs->emb_sj_nest)
      inner_fanout *= pos.records_read;
    else 
      outer_fanout *= pos.records_read;

  }

  *reopt_rec_count= outer_fanout;
  *reopt_cost= cost;
  DBUG_VOID_RETURN;
}


/*
  Do semi-join optimization step after we've added a new tab to join prefix

  SYNOPSIS
    advance_sj_state()
      join                        The join we're optimizing
      remaining_tables            Tables not in the join prefix
      new_join_tab                Join tab that we are adding to the join prefix
      idx                         Index of this join tab (i.e. number of tables
                                  in the prefix)
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
    changes can be rolled back with backout_nj_sj_state().

    See setup_semijoin_dups_elimination() for a description of what kinds of
    join prefixes each strategy can handle.
*/

static 
void advance_sj_state(JOIN *join, table_map remaining_tables, 
                      const JOIN_TAB *new_join_tab, uint idx, 
                      double *current_record_count, double *current_read_time, 
                      POSITION *loose_scan_pos)
{
  TABLE_LIST *emb_sj_nest= new_join_tab->emb_sj_nest;
  POSITION *pos= join->positions + idx;

  /* Add this table to the join prefix */
  remaining_tables &= ~new_join_tab->table->map;

  DBUG_ENTER("advance_sj_state");

  pos->prefix_cost.convert_from_cost(*current_read_time);
  pos->prefix_record_count= *current_record_count;
  pos->sj_strategy= SJ_OPT_NONE;
  
  /* Initialize the state or copy it from prev. tables */
  if (idx == join->const_tables)
  {
    pos->dups_producing_tables= 0;
    pos->first_firstmatch_table= MAX_TABLES;
    pos->first_loosescan_table= MAX_TABLES; 
    pos->dupsweedout_tables= 0;
    pos->sjm_scan_need_tables= 0;
    LINT_INIT(pos->sjm_scan_last_inner);
  }
  else
  {
    pos->dups_producing_tables= pos[-1].dups_producing_tables;

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

    // MaterializeScan
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
  if (emb_sj_nest &&
      join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_FIRSTMATCH))
  {
    const table_map outer_corr_tables= emb_sj_nest->nested_join->sj_depends_on;
    const table_map sj_inner_tables=   emb_sj_nest->sj_inner_tables;
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
        double reopt_cost, reopt_rec_count;
        optimize_wo_join_buffering(join, pos->first_firstmatch_table, idx,
                                   remaining_tables, FALSE, idx,
                                   &reopt_rec_count, &reopt_cost);
        if (reopt_cost < DBL_MAX)
        {
          /*
            We don't yet know what are the other strategies, so pick the
            FirstMatch.

            We ought to save the alternate POSITIONs produced by
            optimize_wo_join_buffering but the problem is that providing save
            space uses too much space. Instead, we will re-calculate the
            alternate POSITIONs after we've picked the best QEP.
          */
          pos->sj_strategy= SJ_OPT_FIRST_MATCH;
          *current_read_time=    reopt_cost;
          *current_record_count= reopt_rec_count;
          handled_by_fm_or_ls=  pos->firstmatch_need_tables;
        }
        else
          DBUG_PRINT("info", ("Cannot use FirstMatch"));
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
        emb_sj_nest != first->table->emb_sj_nest) //(2)
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
      pos->loosescan_need_tables=  emb_sj_nest->sj_inner_tables |
                                   emb_sj_nest->nested_join->sj_depends_on;
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
      double reopt_cost, reopt_rec_count;
      /*
        The same problem as with FirstMatch - we need to save POSITIONs
        somewhere but reserving space for all cases would require too
        much space. We will re-calculate POSITION structures later on. 
      */
      optimize_wo_join_buffering(join, pos->first_loosescan_table, idx,
                                 remaining_tables, 
                                 TRUE,  //first_alt
                                 pos->first_loosescan_table + n_tables,
                                 &reopt_rec_count, 
                                 &reopt_cost);
      if (reopt_cost < DBL_MAX)
      {
        /*
          We don't yet have any other strategies that could handle this
          semi-join nest (the other options are Duplicate Elimination or
          Materialization, which need at least the same set of tables in 
          the join prefix to be considered) so unconditionally pick the 
          LooseScan.
        */
        pos->sj_strategy= SJ_OPT_LOOSE_SCAN;
        *current_read_time=    reopt_cost;
        *current_record_count= reopt_rec_count;
        handled_by_fm_or_ls= first->table->emb_sj_nest->sj_inner_tables;
      }
      else
        DBUG_PRINT("info", ("Cannot use LooseScan"));
    }
  }

  /* 
    Update join->cur_sj_inner_tables (Used by FirstMatch in this function and
    LooseScan detector in best_access_path)
  */
  if (emb_sj_nest)
  {
    join->cur_sj_inner_tables |= emb_sj_nest->sj_inner_tables;
    pos->dups_producing_tables |= emb_sj_nest->sj_inner_tables;

    /* Remove the sj_nest if all of its SJ-inner tables are in cur_table_map */
    if (!(remaining_tables & emb_sj_nest->sj_inner_tables))
      join->cur_sj_inner_tables &= ~emb_sj_nest->sj_inner_tables;
  }
  pos->dups_producing_tables &= ~handled_by_fm_or_ls;

  /* 4. MaterializeLookup and MaterializeScan strategy handler */
  const int sjm_strategy=
    semijoin_order_allows_materialization(join, remaining_tables,
                                          new_join_tab, idx);
  if (sjm_strategy == SJ_OPT_MATERIALIZE_SCAN)
  {
    /*
      We cannot evaluate this option now. This is because we cannot
      account for fanout of sj-inner tables yet:

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
      emb_sj_nest->sj_inner_tables | 
      emb_sj_nest->nested_join->sj_depends_on;
    pos->sjm_scan_last_inner= idx;
  }
  else if (sjm_strategy == SJ_OPT_MATERIALIZE_LOOKUP)
  {
    COST_VECT prefix_cost; 
    int first_tab= (int)idx - my_count_bits(emb_sj_nest->sj_inner_tables);
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
    mat_read_time +=
      emb_sj_nest->nested_join->sjm.materialization_cost.total_cost() +
      prefix_rec_count * emb_sj_nest->nested_join->sjm.lookup_cost.total_cost();

    if (mat_read_time < *current_read_time || pos->dups_producing_tables)
    {
      /*
        NOTE: When we pick to use SJM[-Scan] we don't memcpy its POSITION
        elements to join->positions as that makes it hard to return things
        back when making one step back in join optimization. That's done 
        after the QEP has been chosen.
      */
      pos->sj_strategy= SJ_OPT_MATERIALIZE_LOOKUP;
      *current_read_time=    mat_read_time;
      *current_record_count= prefix_rec_count;
      pos->dups_producing_tables &= ~emb_sj_nest->sj_inner_tables;
    }
  }
  
  /* 4.A SJM-Scan second phase check */
  if (pos->sjm_scan_need_tables && /* Have SJM-Scan prefix */
      !(pos->sjm_scan_need_tables & remaining_tables))
  {
    TABLE_LIST *mat_nest= 
      join->positions[pos->sjm_scan_last_inner].table->emb_sj_nest;
    const uint table_count= my_count_bits(mat_nest->sj_inner_tables);

    double prefix_cost;
    double prefix_rec_count;
    int first_tab= pos->sjm_scan_last_inner + 1 - table_count;
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
    prefix_cost+=
      mat_nest->nested_join->sjm.materialization_cost.total_cost() +
      prefix_rec_count * mat_nest->nested_join->sjm.scan_cost.total_cost();
    prefix_rec_count*= mat_nest->nested_join->sjm.expected_rowcount;
    
    uint i;
    table_map rem_tables= remaining_tables;
    for (i= idx; i != (first_tab + table_count - 1); i--)
      rem_tables |= join->positions[i].table->table->map;

    POSITION curpos, dummy;
    /* Need to re-run best-access-path as we prefix_rec_count has changed */
    for (i= first_tab + table_count; i <= idx; i++)
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
    if (prefix_cost < *current_read_time || pos->dups_producing_tables)
    {
      pos->sj_strategy= SJ_OPT_MATERIALIZE_SCAN;
      *current_read_time=    prefix_cost;
      *current_record_count= prefix_rec_count;
      pos->dups_producing_tables &= ~mat_nest->sj_inner_tables;

    }
  }

  /* 5. Duplicate Weedout strategy handler */
  {
    /* 
       Duplicate weedout can be applied after all ON-correlated and 
       correlated 
    */
    if (emb_sj_nest)
    {
      if (!pos->dupsweedout_tables)
        pos->first_dupsweedout_table= idx;

      pos->dupsweedout_tables|= emb_sj_nest->sj_inner_tables |
                                emb_sj_nest->nested_join->sj_depends_on;
    }

    if (pos->dupsweedout_tables && 
        !(remaining_tables & pos->dupsweedout_tables))
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
      if (dups_cost < *current_read_time || pos->dups_producing_tables)
      {
        pos->sj_strategy= SJ_OPT_DUPS_WEEDOUT;
        *current_read_time= dups_cost;
        *current_record_count= prefix_rec_count * sj_outer_fanout;
        pos->dups_producing_tables &= ~dups_removed_fanout;
      }
    }
  }
  DBUG_VOID_RETURN;
}


/**
  Nested joins perspective: Remove the last table from the join order.

  The algorithm is the reciprocal of check_interleaving_with_nj(), hence
  parent join nest nodes are updated only when the last table in its child
  node is removed. The ASCII graphic below will clarify.

  %A table nesting such as <tt> t1 x [ ( t2 x t3 ) x ( t4 x t5 ) ] </tt>is
  represented by the below join nest tree.

  @verbatim
                     NJ1
                  _/ /  \
                _/  /    NJ2
              _/   /     / \ 
             /    /     /   \
   t1 x [ (t2 x t3) x (t4 x t5) ]
  @endverbatim

  At the point in time when check_interleaving_with_nj() adds the table t5 to
  the query execution plan, QEP, it also directs the node named NJ2 to mark
  the table as covered. NJ2 does so by incrementing its @c counter
  member. Since all of NJ2's tables are now covered by the QEP, the algorithm
  proceeds up the tree to NJ1, incrementing its counter as well. All join
  nests are now completely covered by the QEP.

  restore_prev_nj_state() does the above in reverse. As seen above, the node
  NJ1 contains the nodes t2, t3, and NJ2. Its counter being equal to 3 means
  that the plan covers t2, t3, and NJ2, @e and that the sub-plan (t4 x t5)
  completely covers NJ2. The removal of t5 from the partial plan will first
  decrement NJ2's counter to 1. It will then detect that NJ2 went from being
  completely to partially covered, and hence the algorithm must continue
  upwards to NJ1 and decrement its counter to 2. %A subsequent removal of t4
  will however not influence NJ1 since it did not un-cover the last table in
  NJ2.

  SYNOPSIS
    restore_prev_nj_state()
      last  join table to remove, it is assumed to be the last in current 
            partial join order.
     
  DESCRIPTION

    Remove the last table from the partial join order and update the nested
    joins counters and join->cur_embedding_map. It is ok to call this 
    function for the first table in join order (for which 
    check_interleaving_with_nj has not been called)

     This function rolls back changes done by:
    - check_interleaving_with_nj(): removes the last table from the partial join
    order and update the nested joins counters and join->cur_embedding_map. It
    is ok to call this for the first table in join order (for which
    check_interleaving_with_nj() has not been called).
    - advance_sj_state(): removes the last table from join->cur_sj_inner_tables
    bitmap.

 @param remaining_tables remaining tables to optimize, assumed to not contain
                          tab (@todo but this assumption is violated in practice)
  @param tab              join table to remove, assumed to be the last in
                          current partial join order.
*/

static void backout_nj_sj_state(const table_map remaining_tables,
                                const JOIN_TAB *tab)
{
  /* Restore the nested join state */
  TABLE_LIST *last_emb= tab->table->pos_in_table_list->embedding;
  JOIN *join= tab->join;
  for (;last_emb != NULL; last_emb= last_emb->embedding)
  {
    if (last_emb->on_expr)
    {
      NESTED_JOIN *nest= last_emb->nested_join;
      DBUG_ASSERT(nest->counter_ > 0);

      bool was_fully_covered= nest->is_fully_covered();

      if (--nest->counter_ == 0)
        join->cur_embedding_map&= ~nest->nj_map;

      if (!was_fully_covered)
        break;

      join->cur_embedding_map|= nest->nj_map;
    }
  }

  /* Restore the semijoin state */
  TABLE_LIST *emb_sj_nest= tab->emb_sj_nest;
  if (emb_sj_nest)
  {
    /* If we're removing the last SJ-inner table, remove the sj-nest */
    if ((remaining_tables & emb_sj_nest->sj_inner_tables) ==
        (emb_sj_nest->sj_inner_tables & ~tab->table->map))
    {
      tab->join->cur_sj_inner_tables &= ~emb_sj_nest->sj_inner_tables;
    }
  }
}


/**
  Optimize conditions by 

     a) applying transitivity to build multiple equality predicates
        (MEP): if x=y and y=z the MEP x=y=z is built. 
     b) apply constants where possible. If the value of x is known to be
        42, x is replaced with a constant of value 42. By transitivity, this
        also applies to MEPs, so the MEP in a) will become 42=x=y=z.
     c) remove conditions that are impossible or always true
  
  @param      join         pointer to the structure providing all context info
                           for the query
  @param      conds        conditions to optimize
  @param      join_list    list of join tables to which the condition
                           refers to
  @param[out] cond_value   Not changed if conds was empty 
                           COND_TRUE if conds is always true
                           COND_FALSE if conds is impossible
                           COND_OK otherwise

  @return optimized conditions
*/
static Item *
optimize_cond(JOIN *join, Item *conds, List<TABLE_LIST> *join_list,
              bool build_equalities, Item::cond_result *cond_value)
{
  THD *thd= join->thd;
  DBUG_ENTER("optimize_cond");

  if (conds)
  {
    /* 
      Build all multiple equality predicates and eliminate equality
      predicates that can be inferred from these multiple equalities.
      For each reference of a field included into a multiple equality
      that occurs in a function set a pointer to the multiple equality
      predicate. Substitute a constant instead of this field if the
      multiple equality contains a constant.
    */ 
    DBUG_EXECUTE("where", print_where(conds, "original", QT_ORDINARY););
    if (build_equalities)
    {
      conds= build_equal_items(join->thd, conds, NULL, join_list,
                               &join->cond_equal);
      DBUG_EXECUTE("where",print_where(conds,"after equal_items", QT_ORDINARY););
    }
    /* change field = field to field = const for each found field = const */
    propagate_cond_constants(thd, (I_List<COND_CMP> *) 0, conds, conds);
    /*
      Remove all instances of item == item
      Remove all and-levels where CONST item != CONST item
    */
    DBUG_EXECUTE("where",print_where(conds,"after const change", QT_ORDINARY););
    conds= remove_eq_conds(thd, conds, cond_value) ;
    DBUG_EXECUTE("info",print_where(conds,"after remove", QT_ORDINARY););
  }
  DBUG_RETURN(conds);
}


/**
  Handles the reqursive job for remove_eq_conds()

  Remove const and eq items. Return new item, or NULL if no condition
  cond_value is set to according:
  COND_OK    query is possible (field = constant)
  COND_TRUE  always true	( 1 = 1 )
  COND_FALSE always false	( 1 = 2 )

  SYNPOSIS
    remove_eq_conds()
    thd 			THD environment
    cond                        the condition to handle
    cond_value                  the resulting value of the condition

  RETURN
    *Item with the simplified condition
*/

static Item *
internal_remove_eq_conds(THD *thd, Item *cond, Item::cond_result *cond_value)
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
      Item *new_item=internal_remove_eq_conds(thd, item, &tmp_cond_value);
      if (!new_item)
	li.remove();
      else if (item != new_item)
      {
	(void) li.replace(new_item);
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
	  return (Item*) 0;			// Always false
	}
	break;
      case Item::COND_TRUE:
	if (!and_level)
	{
	  *cond_value= tmp_cond_value;
	  return (Item*) 0;			// Always true
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
      return (Item*) 0;
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
    Item_func_isnull *func=(Item_func_isnull*) cond;
    Item **args= func->arguments();
    if (args[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) args[0])->field;
      /* fix to replace 'NULL' dates with '0' (shreeve@uci.edu) */
      /*
        datetime_field IS NULL has to be modified to
        datetime_field == 0
      */
      if (((field->type() == MYSQL_TYPE_DATE) ||
           (field->type() == MYSQL_TYPE_DATETIME)) &&
          (field->flags & NOT_NULL_FLAG) && !field->table->maybe_null)
      {
	Item *new_cond;
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
      return (Item*) 0;
    }
  }
  else if (cond->const_item() && !cond->is_expensive())
  /*
    DontEvaluateMaterializedSubqueryTooEarly:
    TODO: 
    Excluding all expensive functions is too restritive we should exclude only
    materialized IN subquery predicates because they can't yet be evaluated
    here (they need additional initialization that is done later on).

    The proper way to exclude the subqueries would be to walk the cond tree
    and check for materialized subqueries there.
  */
  {
    *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
    return (Item*) 0;
  }
  else if ((*cond_value= cond->eq_cmp_result()) != Item::COND_OK)
  {						// boolan compare function
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(right_item,1))
    {
      if (!left_item->maybe_null ||
	  ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC)
	return (Item*) 0;			// Compare of identical items
    }
  }
  *cond_value=Item::COND_OK;
  return cond;					// Point at next and level
}


/**
  Remove const and eq items. Return new item, or NULL if no condition
  cond_value is set to according:
  COND_OK    query is possible (field = constant)
  COND_TRUE  always true	( 1 = 1 )
  COND_FALSE always false	( 1 = 2 )

  SYNPOSIS
    remove_eq_conds()
    thd 			THD environment
    cond                        the condition to handle
    cond_value                  the resulting value of the condition

  NOTES
    calls the inner_remove_eq_conds to check all the tree reqursively

  RETURN
    *Item with the simplified condition
*/

Item *
remove_eq_conds(THD *thd, Item *cond, Item::cond_result *cond_value)
{
  if (cond->type() == Item::FUNC_ITEM &&
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
	  (thd->variables.option_bits & OPTION_AUTO_IS_NULL) &&
	  (thd->first_successful_insert_id_in_prev_stmt > 0 &&
           thd->substitute_null_with_insert_id))
      {
#ifdef HAVE_QUERY_CACHE
	query_cache_abort(&thd->query_cache_tls);
#endif
	Item *new_cond;
	if ((new_cond= new Item_func_eq(args[0],
					new Item_int("last_insert_id()",
                                                     thd->read_first_successful_insert_id_in_prev_stmt(),
                                                     MY_INT64_NUM_DECIMAL_DIGITS))))
	{
	  cond=new_cond;
          /*
            Item_func_eq can't be fixed after creation so we do not check
            cond->fixed, also it do not need tables so we use 0 as second
            argument.
          */
	  cond->fix_fields(thd, &cond);
	}
        /*
          IS NULL should be mapped to LAST_INSERT_ID only for first row, so
          clear for next row
        */
        thd->substitute_null_with_insert_id= FALSE;

        *cond_value= Item::COND_OK;
        return cond;
      }
    }
  }
  return internal_remove_eq_conds(thd, cond, cond_value); // Scan all the condition
}


/* 
  Check if equality can be used in removing components of GROUP BY/DISTINCT
  
  SYNOPSIS
    test_if_equality_guarantees_uniqueness()
      l          the left comparison argument (a field if any)
      r          the right comparison argument (a const of any)
  
  DESCRIPTION    
    Checks if an equality predicate can be used to take away 
    DISTINCT/GROUP BY because it is known to be true for exactly one 
    distinct value (e.g. <expr> == <const>).
    Arguments must be of the same type because e.g. 
    <string_field> = <int_const> may match more than 1 distinct value from 
    the column. 
    We must take into consideration and the optimization done for various 
    string constants when compared to dates etc (see Item_int_with_ref) as
    well as the collation of the arguments.
  
  RETURN VALUE  
    TRUE    can be used
    FALSE   cannot be used
*/
static bool
test_if_equality_guarantees_uniqueness(Item *l, Item *r)
{
  return r->const_item() &&
    /* elements must be compared as dates */
     (Arg_comparator::can_compare_as_dates(l, r, 0) ||
      /* or of the same result type */
      (r->result_type() == l->result_type() &&
       /* and must have the same collation if compared as strings */
       (l->result_type() != STRING_RESULT ||
        l->collation.collation == r->collation.collation)));
}


/*
  Return TRUE if i1 and i2 (if any) are equal items,
  or if i1 is a wrapper item around the f2 field.
*/

static bool equal(Item *i1, Item *i2, Field *f2)
{
  DBUG_ASSERT((i2 == NULL) ^ (f2 == NULL));

  if (i2 != NULL)
    return i1->eq(i2, 1);
  else if (i1->type() == Item::FIELD_ITEM)
    return f2->eq(((Item_field *) i1)->field);
  else
    return FALSE;
}


/**
  Test if a field or an item is equal to a constant value in WHERE

  @param        cond            WHERE clause expression
  @param        comp_item       Item to find in WHERE expression
                                (if comp_field != NULL)
  @param        comp_field      Field to find in WHERE expression
                                (if comp_item != NULL)
  @param[out]   const_item      intermediate arg, set to Item pointer to NULL 

  @return TRUE if the field is a constant value in WHERE

  @note
    comp_item and comp_field parameters are mutually exclusive.
*/
bool
const_expression_in_where(Item *cond, Item *comp_item, Field *comp_field,
                          Item **const_item)
{
  DBUG_ASSERT((comp_item == NULL) ^ (comp_field == NULL));

  Item *intermediate= NULL;
  if (const_item == NULL)
    const_item= &intermediate;

  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype()
		     == Item_func::COND_AND_FUNC);
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      bool res=const_expression_in_where(item, comp_item, comp_field,
                                         const_item);
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
  {						// boolean compare function
    Item_func* func= (Item_func*) cond;
    if (func->functype() != Item_func::EQUAL_FUNC &&
	func->functype() != Item_func::EQ_FUNC)
      return 0;
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (equal(left_item, comp_item, comp_field))
    {
      if (test_if_equality_guarantees_uniqueness (left_item, right_item))
      {
	if (*const_item)
	  return right_item->eq(*const_item, 1);
	*const_item=right_item;
	return 1;
      }
    }
    else if (equal(right_item, comp_item, comp_field))
    {
      if (test_if_equality_guarantees_uniqueness (right_item, left_item))
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

/**
  Create field for temporary table from given field.

  @param thd	       Thread handler
  @param org_field    field from which new field will be created
  @param name         New field name
  @param table	       Temporary table
  @param item	       !=NULL if item->result_field should point to new field.
                      This is relevant for how fill_record() is going to work:
                      If item != NULL then fill_record() will update
                      the record in the original table.
                      If item == NULL then fill_record() will update
                      the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    NULL		on error
  @retval
    new_created field
*/

Field *create_tmp_field_from_field(THD *thd, Field *org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item, uint convert_blob_length)
{
  Field *new_field;

  /* 
    Make sure that the blob fits into a Field_varstring which has 
    2-byte lenght. 
  */
  if (convert_blob_length && convert_blob_length <= Field_varstring::MAX_SIZE &&
      (org_field->flags & BLOB_FLAG))
    new_field= new Field_varstring(convert_blob_length,
                                   org_field->maybe_null(),
                                   org_field->field_name, table->s,
                                   org_field->charset());
  else
    new_field= org_field->new_field(thd->mem_root, table,
                                    table == org_field->table);
  if (new_field)
  {
    new_field->init(table);
    new_field->orig_table= org_field->orig_table;
    if (item)
      item->result_field= new_field;
    else
      new_field->field_name= name;
    new_field->flags|= (org_field->flags & NO_DEFAULT_VALUE_FLAG);
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    if (org_field->type() == MYSQL_TYPE_VAR_STRING ||
        org_field->type() == MYSQL_TYPE_VARCHAR)
      table->s->db_create_options|= HA_OPTION_PACK_RECORD;
    else if (org_field->type() == FIELD_TYPE_DOUBLE)
      ((Field_double *) new_field)->not_fixed= TRUE;
  }
  return new_field;
}

/**
  Create field for temporary table using type of given item.

  @param thd                   Thread handler
  @param item                  Item to create a field for
  @param table                 Temporary table
  @param copy_func             If set and item is a function, store copy of
                               item in this array
  @param modify_item           1 if item->result_field should point to new
                               item. This is relevent for how fill_record()
                               is going to work:
                               If modify_item is 1 then fill_record() will
                               update the record in the original table.
                               If modify_item is 0 then fill_record() will
                               update the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    0  on error
  @retval
    new_created field
*/

static Field *create_tmp_field_from_item(THD *thd, Item *item, TABLE *table,
                                         Item ***copy_func, bool modify_item,
                                         uint convert_blob_length)
{
  bool maybe_null= item->maybe_null;
  Field *new_field;
  LINT_INIT(new_field);

  switch (item->result_type()) {
  case REAL_RESULT:
    new_field= new Field_double(item->max_length, maybe_null,
                                item->name, item->decimals, TRUE);
    break;
  case INT_RESULT:
    /* 
      Select an integer type with the minimal fit precision.
      MY_INT32_NUM_DECIMAL_DIGITS is sign inclusive, don't consider the sign.
      Values with MY_INT32_NUM_DECIMAL_DIGITS digits may or may not fit into 
      Field_long : make them Field_longlong.  
    */
    if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
      new_field=new Field_longlong(item->max_length, maybe_null,
                                   item->name, item->unsigned_flag);
    else
      new_field=new Field_long(item->max_length, maybe_null,
                               item->name, item->unsigned_flag);
    break;
  case STRING_RESULT:
    DBUG_ASSERT(item->collation.collation);
  
    enum enum_field_types type;
    /*
      DATE/TIME and GEOMETRY fields have STRING_RESULT result type. 
      To preserve type they needed to be handled separately.
    */
    if ((type= item->field_type()) == MYSQL_TYPE_DATETIME ||
        type == MYSQL_TYPE_TIME || type == MYSQL_TYPE_DATE ||
        type == MYSQL_TYPE_NEWDATE ||
        type == MYSQL_TYPE_TIMESTAMP || type == MYSQL_TYPE_GEOMETRY)
      new_field= item->tmp_table_field_from_field_type(table, 1);
    /* 
      Make sure that the blob fits into a Field_varstring which has 
      2-byte lenght. 
    */
    else if (item->max_length/item->collation.collation->mbmaxlen > 255 &&
             convert_blob_length <= Field_varstring::MAX_SIZE && 
             convert_blob_length)
      new_field= new Field_varstring(convert_blob_length, maybe_null,
                                     item->name, table->s,
                                     item->collation.collation);
    else
      new_field= item->make_string_field(table);
    new_field->set_derivation(item->collation.derivation);
    break;
  case DECIMAL_RESULT:
    new_field= Field_new_decimal::create_from_item(item);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    new_field= 0;
    break;
  }
  if (new_field)
    new_field->init(table);
    
  if (copy_func && item->is_result_field())
    *((*copy_func)++) = item;			// Save for copy_funcs
  if (modify_item)
    item->set_result_field(new_field);
  if (item->type() == Item::NULL_ITEM)
    new_field->is_created_from_null_item= TRUE;
  return new_field;
}


/**
  Create field for information schema table.

  @param thd		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for

  @retval
    0			on error
  @retval
    new_created field
*/

Field *create_tmp_field_for_schema(THD *thd, Item *item, TABLE *table)
{
  if (item->field_type() == MYSQL_TYPE_VARCHAR)
  {
    Field *field;
    if (item->max_length > MAX_FIELD_VARCHARLENGTH)
      field= new Field_blob(item->max_length, item->maybe_null,
                            item->name, item->collation.collation);
    else
      field= new Field_varstring(item->max_length, item->maybe_null,
                                 item->name,
                                 table->s, item->collation.collation);
    if (field)
      field->init(table);
    return field;
  }
  return item->tmp_table_field_from_field_type(table, 0);
}


/**
  Create field for temporary table.

  @param thd		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for
  @param type		Type of item (normally item->type)
  @param copy_func	If set and item is a function, store copy of item
                       in this array
  @param from_field    if field will be created using other field as example,
                       pointer example field will be written here
  @param default_field	If field has a default value field, store it here
  @param group		1 if we are going to do a relative group by on result
  @param modify_item	1 if item->result_field should point to new item.
                       This is relevent for how fill_record() is going to
                       work:
                       If modify_item is 1 then fill_record() will update
                       the record in the original table.
                       If modify_item is 0 then fill_record() will update
                       the temporary table
  @param convert_blob_length If >0 create a varstring(convert_blob_length)
                             field instead of blob.

  @retval
    0			on error
  @retval
    new_created field
*/

Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
                        Item ***copy_func, Field **from_field,
                        Field **default_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields,
                        bool make_copy_field,
                        uint convert_blob_length)
{
  Field *result;
  Item::Type orig_type= type;
  Item *orig_item= 0;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM)
  {
    orig_item= item;
    item= item->real_item();
    type= Item::FIELD_ITEM;
  }

  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    result= item_sum->create_tmp_field(group, table, convert_blob_length);
    if (!result)
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    return result;
  }
  case Item::FIELD_ITEM:
  case Item::DEFAULT_VALUE_ITEM:
  {
    Item_field *field= (Item_field*) item;
    bool orig_modify= modify_item;
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
        field->result_field= result;
    } 
    else if (table_cant_handle_bit_fields && field->field->type() ==
             MYSQL_TYPE_BIT)
    {
      *from_field= field->field;
      result= create_tmp_field_from_item(thd, item, table, copy_func,
                                        modify_item, convert_blob_length);
      if (result && modify_item)
        field->result_field= result;
    }
    else
      result= create_tmp_field_from_field(thd, (*from_field= field->field),
                                          orig_item ? orig_item->name :
                                          item->name,
                                          table,
                                          modify_item ? field :
                                          NULL,
                                          convert_blob_length);
    if (orig_type == Item::REF_ITEM && orig_modify)
      ((Item_ref*)orig_item)->set_result_field(result);
    if (field->field->eq_def(result))
      *default_field= field->field;
    return result;
  }
  /* Fall through */
  case Item::FUNC_ITEM:
    if (((Item_func *) item)->functype() == Item_func::FUNC_SP)
    {
      Item_func_sp *item_func_sp= (Item_func_sp *) item;
      Field *sp_result_field= item_func_sp->get_sp_result_field();

      if (make_copy_field)
      {
        DBUG_ASSERT(item_func_sp->result_field);
        *from_field= item_func_sp->result_field;
      }
      else
      {
        *((*copy_func)++)= item;
      }

      Field *result_field=
        create_tmp_field_from_field(thd,
                                    sp_result_field,
                                    item_func_sp->name,
                                    table,
                                    NULL,
                                    convert_blob_length);

      if (modify_item)
        item->set_result_field(result_field);

      return result_field;
    }

    /* Fall through */
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
    if (make_copy_field)
    {
      DBUG_ASSERT(((Item_result_field*)item)->result_field);
      *from_field= ((Item_result_field*)item)->result_field;
    }
    return create_tmp_field_from_item(thd, item, table,
                                      (make_copy_field ? 0 : copy_func),
                                       modify_item, convert_blob_length);
  case Item::TYPE_HOLDER:  
    result= ((Item_type_holder *)item)->make_field_by_type(table);
    result->set_derivation(item->collation.derivation);
    return result;
  default:					// Dosen't have to be stored
    return 0;
  }
}

/*
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

void setup_tmp_table_column_bitmaps(TABLE *table, uchar *bitmaps)
{
  uint field_count= table->s->fields;
  bitmap_init(&table->def_read_set, (my_bitmap_map*) bitmaps, field_count,
              FALSE);
  bitmap_init(&table->tmp_set,
              (my_bitmap_map*) (bitmaps+ bitmap_buffer_size(field_count)),
              field_count, FALSE);
  /* write_set and all_set are copies of read_set */
  table->def_write_set= table->def_read_set;
  table->s->all_set= table->def_read_set;
  bitmap_set_all(&table->s->all_set);
  table->default_column_bitmaps();
}


/**
  Create a temp table according to a field list.

  Given field pointers are changed to point at tmp_table for
  send_result_set_metadata. The table object is self contained: it's
  allocated in its own memory root, as well as Field objects
  created for table columns.
  This function will replace Item_sum items in 'fields' list with
  corresponding Item_field items, pointing at the fields in the
  temporary table, unless this was prohibited by TRUE
  value of argument save_sum_fields. The Item_field objects
  are created in THD memory root.

  @param thd                  thread handle
  @param param                a description used as input to create the table
  @param fields               list of items that will be used to define
                              column types of the table (also see NOTES)
  @param group                TODO document
  @param distinct             should table rows be distinct
  @param save_sum_fields      see NOTES
  @param select_options
  @param rows_limit
  @param table_alias          possible name of the temporary table that can
                              be used for name resolving; can be "".
*/

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2
#define MIN_STRING_LENGTH_TO_PACK_ROWS   10

TABLE *
create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 ulonglong select_options, ha_rows rows_limit,
		 const char *table_alias)
{
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  uint	i,field_count,null_count,null_pack_length;
  uint  copy_func_count= param->func_count;
  uint  hidden_null_count, hidden_null_pack_length, hidden_field_count;
  uint  blob_count,group_null_items, string_count;
  uint  temp_pool_slot=MY_BIT_NONE;
  uint fieldnr= 0;
  ulong reclength, string_total_length;
  bool  using_unique_constraint= 0;
  bool  use_packed_rows= 0;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char  *tmpname,path[FN_REFLEN];
  uchar	*pos, *group_buff, *bitmaps;
  uchar *null_flags;
  Field **reg_field, **from_field, **default_field;
  uint *blob_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  /*
    total_uneven_bit_length is uneven bit length for visible fields
    hidden_uneven_bit_length is uneven bit length for hidden fields
  */
  uint total_uneven_bit_length= 0, hidden_uneven_bit_length= 0;
  bool force_copy_fields= param->force_copy_fields;
  /* Treat sum functions as normal ones when loose index scan is used. */
  save_sum_fields|= param->precomputed_group_by;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",
             ("distinct: %d  save_sum_fields: %d  rows_limit: %lu  group: %d",
              (int) distinct, (int) save_sum_fields,
              (ulong) rows_limit,test(group)));

  status_var_increment(thd->status_var.created_tmp_tables);

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
      /*
        marker == 4 means two things:
        - store NULLs in the key, and
        - convert BIT fields to 64-bit long, needed because MEMORY tables
          can't index BIT fields.
      */
      (*tmp->item)->marker= 4;
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

  /*
    When loose index scan is employed as access method, it already
    computes all groups and the result of all aggregate functions. We
    make space for the items of the aggregate function in the list of
    functions TMP_TABLE_PARAM::items_to_copy, so that the values of
    these items are stored in the temporary table.
  */
  if (param->precomputed_group_by)
    copy_func_count+= param->sum_func_count;
  
  init_sql_alloc(&own_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (field_count+1),
                        &default_field, sizeof(Field*) * (field_count),
                        &blob_field, sizeof(uint)*(field_count+1),
                        &from_field, sizeof(Field*)*field_count,
                        &copy_func, sizeof(*copy_func)*(copy_func_count+1),
                        &param->keyinfo, sizeof(*param->keyinfo),
                        &key_part_info,
                        sizeof(*key_part_info)*(param->group_parts+1),
                        &param->start_recinfo,
                        sizeof(*param->recinfo)*(field_count*2+4),
                        &tmpname, (uint) strlen(path)+1,
                        &group_buff, (group && ! using_unique_constraint ?
                                      param->group_length : 0),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NullS))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  /* Copy_field belongs to TMP_TABLE_PARAM, allocate it in THD mem_root */
  if (!(param->copy_field= copy= new (thd->mem_root) Copy_field[field_count]))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    free_root(&own_root, MYF(0));               /* purecov: inspected */
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  param->items_to_copy= copy_func;
  strmov(tmpname,path);
  /* make table according to fields */

  bzero((char*) table,sizeof(*table));
  bzero((char*) reg_field,sizeof(Field*)*(field_count+1));
  bzero((char*) default_field, sizeof(Field*) * (field_count));
  bzero((char*) from_field,sizeof(Field*)*field_count);

  table->mem_root= own_root;
  mem_root_save= thd->mem_root;
  thd->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias= table_alias;
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= thd;
  table->quick_keys.init();
  table->covering_keys.init();
  table->merge_keys.init();
  table->keys_in_use_for_query.init();

  table->s= share;
  init_tmp_table_share(thd, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= param->table_charset;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();
  if (param->schema_table)
    share->db= INFORMATION_SCHEMA_NAME;

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
        if (item->used_tables() & OUTER_REF_TABLE_BIT)
          item->update_used_tables();
        if (type == Item::SUBSELECT_ITEM ||
            (item->used_tables() & ~OUTER_REF_TABLE_BIT))
        {
	  /*
	    Mark that the we have ignored an item that refers to a summary
	    function. We need to know this if someone is going to use
	    DISTINCT on the result.
	  */
	  param->using_indirect_summary_function=1;
	  continue;
        }
      }
      if (item->const_item() && (int) hidden_field_count <= 0)
        continue; // We don't have to store this
    }
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      Item_sum *sum_item= (Item_sum *) item;
      sum_item->result_field=0;
      for (i=0 ; i < sum_item->get_arg_count() ; i++)
      {
	Item *arg= sum_item->get_arg(i);
	if (!arg->const_item())
	{
	  Field *new_field=
            create_tmp_field(thd, table, arg, arg->type(), &copy_func,
                             tmp_from_field, &default_field[fieldnr],
                             group != 0,not_all_columns,
                             distinct, 0,
                             param->convert_blob_length);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  reclength+=new_field->pack_length();
	  if (new_field->flags & BLOB_FLAG)
	  {
	    *blob_field++= fieldnr;
	    blob_count++;
	  }
          if (new_field->type() == MYSQL_TYPE_BIT)
            total_uneven_bit_length+= new_field->field_length & 7;
	  *(reg_field++)= new_field;
          if (new_field->real_type() == MYSQL_TYPE_STRING ||
              new_field->real_type() == MYSQL_TYPE_VARCHAR)
          {
            string_count++;
            string_total_length+= new_field->pack_length();
          }
          thd->mem_root= mem_root_save;
          arg= sum_item->set_arg(i, thd, new Item_field(new_field));
          thd->mem_root= &table->mem_root;
	  if (!(new_field->flags & NOT_NULL_FLAG))
          {
	    null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            arg->maybe_null=1;
          }
          new_field->field_index= fieldnr++;
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
                         tmp_from_field, &default_field[fieldnr],
                         group != 0,
                         !force_copy_fields &&
                           (not_all_columns || group !=0),
                         /*
                           If item->marker == 4 then we force create_tmp_field
                           to create a 64-bit longs for BIT fields because HEAP
                           tables can't index BIT fields directly. We do the same
                           for distinct, as we want the distinct index to be
                           usable in this case too.
                         */
                         item->marker == 4 || param->bit_fields_as_long,
                         force_copy_fields,
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
      if (new_field->type() == MYSQL_TYPE_BIT)
        total_uneven_bit_length+= new_field->field_length & 7;
      if (new_field->flags & BLOB_FLAG)
      {
        *blob_field++= fieldnr;
	blob_count++;
      }
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      new_field->field_index= fieldnr++;
      *(reg_field++)= new_field;
    }
    if (!--hidden_field_count)
    {
      /*
        This was the last hidden field; Remember how many hidden fields could
        have null
      */
      hidden_null_count=null_count;
      /*
	We need to update hidden_field_count as we may have stored group
	functions with constant arguments
      */
      param->hidden_field_count= fieldnr;
      null_count= 0;
      /*
        On last hidden field we store uneven bit length in
        hidden_uneven_bit_length and proceed calculation of
        uneven bits for visible fields into
        total_uneven_bit_length variable.
      */
      hidden_uneven_bit_length= total_uneven_bit_length;
      total_uneven_bit_length= 0;
    }
  }
  DBUG_ASSERT(fieldnr == (uint) (reg_field - table->field));
  DBUG_ASSERT(field_count >= (uint) (reg_field - table->field));
  field_count= fieldnr;
  *reg_field= 0;
  *blob_field= 0;				// End marker
  share->fields= field_count;

  /* If result table is small; use a heap */
  /* future: storage engine selection can be made dynamic? */
  if (blob_count || using_unique_constraint
      || (thd->variables.big_tables && !(select_options & SELECT_SMALL_RESULT))
      || (select_options & TMP_TABLE_FORCE_MYISAM))
  {
    share->db_plugin= ha_lock_engine(0, myisam_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
    if (group &&
	(param->group_parts > table->file->max_key_parts() ||
	 param->group_length > table->file->max_key_length()))
      using_unique_constraint=1;
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  if (!table->file)
    goto err;


  if (!using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  share->blob_fields= blob_count;
  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length= (hidden_null_count + 7 +
                            hidden_uneven_bit_length) / 8;
  null_pack_length= (hidden_null_pack_length +
                     (null_count + total_uneven_bit_length + 7) / 8);
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count ||
      (string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS &&
      (reclength / string_total_length <= RATIO_TO_PACK_ROWS ||
       string_total_length / string_count >= AVG_STRING_LENGTH_TO_PACK_ROWS)))
    use_packed_rows= 1;

  share->reclength= reclength;
  {
    uint alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (uchar*)
                            alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  copy_func[0]=0;				// End marker
  param->func_count= copy_func - param->items_to_copy; 

  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo=param->start_recinfo;
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
    share->null_fields= null_count+ hidden_null_count;
    share->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  for (i=0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint length;
    bzero((uchar*) recinfo,sizeof(*recinfo));

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
	bzero((uchar*) recinfo,sizeof(*recinfo));
      }
      else
      {
	recinfo->null_bit= 1 << (null_count & 7);
	recinfo->null_pos= null_count/8;
      }
      field->move_field(pos,null_flags+null_count/8,
			1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field(pos,(uchar*) 0,0);
    if (field->type() == MYSQL_TYPE_BIT)
    {
      /* We have to reserve place for extra bits among null bits */
      ((Field_bit*) field)->set_bit_ptr(null_flags + null_count / 8,
                                        null_count & 7);
      null_count+= (field->field_length & 7);
    }
    field->reset();

    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    if (default_field[i] && default_field[i]->ptr)
    {
      /* 
         default_field[i] is set only in the cases  when 'field' can
         inherit the default value that is defined for the field referred
         by the Item_field object from which 'field' has been created.
      */
      my_ptrdiff_t diff;
      Field *orig_field= default_field[i];
      /* Get the value from default_values */
      diff= (my_ptrdiff_t) (orig_field->table->s->default_values-
                            orig_field->table->record[0]);
      orig_field->move_field_offset(diff);      // Points now at default_values
      if (orig_field->is_real_null())
        field->set_null();
      else
      {
        field->set_notnull();
        memcpy(field->ptr, orig_field->ptr, field->pack_length());
      }
      orig_field->move_field_offset(-diff);     // Back to record[0]
    } 

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

  if (thd->variables.tmp_table_size == ~ (ulonglong) 0)		// No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 min(thd->variables.tmp_table_size,
                                     thd->variables.max_heap_table_size) :
                                 thd->variables.tmp_table_size) /
			         share->reclength);
  set_if_bigger(share->max_rows,1);		// For dummy start options
  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(share->max_rows, rows_limit);
  param->end_write_records= rows_limit;

  keyinfo= param->keyinfo;

  if (group)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    share->keys=1;
    share->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "group_key";
    ORDER *cur_group= group;
    for (; cur_group ; cur_group= cur_group->next, key_part_info++)
    {
      Field *field=(*cur_group->item)->get_tmp_table_field();
      DBUG_ASSERT(field->table == table);
      bool maybe_null=(*cur_group->item)->maybe_null;
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->record[0]);
      key_part_info->length= (uint16) field->key_length();
      key_part_info->type=   (uint8) field->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
	cur_group->buff=(char*) group_buff;
	if (!(cur_group->field= field->new_key_field(thd->mem_root,table,
                                                     group_buff +
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
          cur_group->buff++;                        // Pointer to field data
	  group_buff++;                         // Skipp null flag
	}
        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL;
	group_buff+= cur_group->field->pack_length();
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (distinct && field_count != param->hidden_field_count)
  {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    DBUG_PRINT("info",("hidden_field_count: %d", param->hidden_field_count));

    if (blob_count)
    {
      /*
        Special mode for index creation in MyISAM used to support unique
        indexes on blobs with arbitrary length. Such indexes cannot be
        used for lookups.
      */
      share->uniques= 1;
    }
    null_pack_length-=hidden_null_pack_length;
    keyinfo->key_parts= ((field_count-param->hidden_field_count)+
			 (share->uniques ? test(null_pack_length) : 0));
    table->distinct= 1;
    share->keys= 1;
    if (!(key_part_info= (KEY_PART_INFO*)
          alloc_root(&table->mem_root,
                     keyinfo->key_parts * sizeof(KEY_PART_INFO))))
      goto err;
    bzero((void*) key_part_info, keyinfo->key_parts * sizeof(KEY_PART_INFO));
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME | HA_NULL_ARE_EQUAL;
    keyinfo->key_length= 0;  // Will compute the sum of the parts below.
    keyinfo->name= (char*) "distinct_key";
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->rec_per_key=0;

    /*
      Create an extra field to hold NULL bits so that unique indexes on
      blobs can distinguish NULL from 0. This extra field is not needed
      when we do not use UNIQUE indexes for blobs.
    */
    if (null_pack_length && share->uniques)
    {
      key_part_info->null_bit=0;
      key_part_info->offset=hidden_null_pack_length;
      key_part_info->length=null_pack_length;
      key_part_info->field= new Field_string(table->record[0],
                                             (uint32) key_part_info->length,
                                             (uchar*) 0,
                                             (uint) 0,
                                             Field::NONE,
                                             NullS, &my_charset_bin);
      if (!key_part_info->field)
        goto err;
      key_part_info->field->init(table);
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
      key_part_info->offset=   (*reg_field)->offset(table->record[0]);
      key_part_info->length=   (uint16) (*reg_field)->pack_length();
      /* TODO:
        The below method of computing the key format length of the
        key part is a copy/paste from opt_range.cc, and table.cc.
        This should be factored out, e.g. as a method of Field.
        In addition it is not clear if any of the Field::*_length
        methods is supposed to compute the same length. If so, it
        might be reused.
      */
      key_part_info->store_length= key_part_info->length;

      if ((*reg_field)->real_maybe_null())
        key_part_info->store_length+= HA_KEY_NULL_LENGTH;
      if ((*reg_field)->type() == MYSQL_TYPE_BLOB || 
          (*reg_field)->real_type() == MYSQL_TYPE_VARCHAR)
        key_part_info->store_length+= HA_KEY_BLOB_LENGTH;

      keyinfo->key_length+= key_part_info->store_length;

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
  share->db_record_offset= 1;
  if (share->db_type() == myisam_hton)
  {
    if (create_myisam_tmp_table(table, param->keyinfo, param->start_recinfo,
                                &param->recinfo, select_options,
                                thd->variables.big_tables))
      goto err;
  }
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
  MI_COLUMNDEF *recinfo, *start_recinfo;
  bool using_unique_constraint=FALSE;
  bool use_packed_rows= FALSE;
  Field *field, *key_field;
  uint blob_count, null_pack_length, null_count;
  uchar *null_flags;
  uchar *pos;
  DBUG_ENTER("create_duplicate_weedout_tmp_table");
  DBUG_ASSERT(!sjtbl->is_confluent);
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
                        &bitmaps, bitmap_buffer_size(1)*2,
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
  table->alias= "weedout-tmp";
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
    share->db_plugin= ha_lock_engine(0, myisam_hton);
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
      recinfo->type= (int) FIELD_BLOB;
    else if (use_packed_rows &&
             field->real_type() == MYSQL_TYPE_STRING &&
	     length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type=FIELD_SKIP_ENDSPACE;
    else
      recinfo->type=FIELD_NORMAL;

    field->table_name= &table->alias;
  }

  //param->recinfo=recinfo;
  //store_record(table,s->default_values);        // Make empty default record

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
  if (share->db_type() == myisam_hton)
  {
    recinfo++;
    if (create_myisam_tmp_table(table, keyinfo, start_recinfo, &recinfo, 0, 0))
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

/****************************************************************************/

/**
  Create a reduced TABLE object with properly set up Field list from a
  list of field definitions.

    The created table doesn't have a table handler associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this TABLE object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in THD mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  @param thd         connection handle
  @param field_list  list of column definitions

  @return
    0 if out of memory, TABLE object in case of success
*/

TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list)
{
  uint field_count= field_list.elements;
  uint blob_count= 0;
  Field **field;
  Create_field *cdef;                           /* column definition */
  uint record_length= 0;
  uint null_count= 0;                 /* number of columns which may be null */
  uint null_pack_length;              /* NULL representation array length */
  uint *blob_field;
  uchar *bitmaps;
  TABLE *table;
  TABLE_SHARE *share;

  if (!multi_alloc_root(thd->mem_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &field, (field_count + 1) * sizeof(Field*),
                        &blob_field, (field_count+1) *sizeof(uint),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NullS))
    return 0;

  bzero(table, sizeof(*table));
  bzero(share, sizeof(*share));
  table->field= field;
  table->s= share;
  table->temp_pool_slot= MY_BIT_NONE;
  share->blob_field= blob_field;
  share->fields= field_count;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  setup_tmp_table_column_bitmaps(table, bitmaps);

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<Create_field> it(field_list);
  while ((cdef= it++))
  {
    *field= make_field(share, 0, cdef->length,
                       (uchar*) (f_maybe_null(cdef->pack_flag) ? "" : 0),
                       f_maybe_null(cdef->pack_flag) ? 1 : 0,
                       cdef->pack_flag, cdef->sql_type, cdef->charset,
                       cdef->geom_type, cdef->unireg_check,
                       cdef->interval, cdef->field_name);
    if (!*field)
      goto error;
    (*field)->init(table);
    record_length+= (*field)->pack_length();
    if (! ((*field)->flags & NOT_NULL_FLAG))
      null_count++;

    if ((*field)->flags & BLOB_FLAG)
      share->blob_field[blob_count++]= (uint) (field - table->field);

    field++;
  }
  *field= NULL;                             /* mark the end of the list */
  share->blob_field[blob_count]= 0;            /* mark the end of the list */
  share->blob_fields= blob_count;

  null_pack_length= (null_count + 7)/8;
  share->reclength= record_length + null_pack_length;
  share->rec_buff_length= ALIGN_SIZE(share->reclength + 1);
  table->record[0]= (uchar*) thd->alloc(share->rec_buff_length);
  if (!table->record[0])
    goto error;

  if (null_pack_length)
  {
    table->null_flags= (uchar*) table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }

  table->in_use= thd;           /* field->reset() may access table->in_use */
  {
    /* Set up field pointers */
    uchar *null_pos= table->record[0];
    uchar *field_pos= null_pos + share->null_bytes;
    uint null_bit= 1;

    for (field= table->field; *field; ++field)
    {
      Field *cur_field= *field;
      if ((cur_field->flags & NOT_NULL_FLAG))
        cur_field->move_field(field_pos);
      else
      {
        cur_field->move_field(field_pos, (uchar*) null_pos, null_bit);
        null_bit<<= 1;
        if (null_bit == (1 << 8))
        {
          ++null_pos;
          null_bit= 1;
        }
      }
      if (cur_field->type() == MYSQL_TYPE_BIT &&
          cur_field->key_type() == HA_KEYTYPE_BIT)
      {
        /* This is a Field_bit since key_type is HA_KEYTYPE_BIT */
        static_cast<Field_bit*>(cur_field)->set_bit_ptr(null_pos, null_bit);
        null_bit+= cur_field->field_length & 7;
        if (null_bit > 7)
        {
          null_pos++;
          null_bit-= 8;
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
  if ((error=table->file->ha_open(table, table->s->table_name.str,O_RDWR,
                                  HA_OPEN_TMP_TABLE | HA_OPEN_INTERNAL_TABLE)))
  {
    table->file->print_error(error,MYF(0)); /* purecov: inspected */
    table->db_stat=0;
    return(1);
  }
  (void) table->file->extra(HA_EXTRA_QUICK);		/* Faster */
  return(0);
}


/*
  Create MyISAM temporary table

  SYNOPSIS
    create_myisam_tmp_table()
      table           Table object that descrimes the table to be created
      keyinfo         Description of the index (there is always one index)
      start_recinfo   MyISAM's column descriptions
      recinfo INOUT   End of MyISAM's column descriptions
      options         Option bits
   
  DESCRIPTION
    Create a MyISAM temporary table according to passed description. The is
    assumed to have one unique index or constraint.

    The passed array or MI_COLUMNDEF structures must have this form:

      1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
         when there are many nullable columns)
      2. Table columns
      3. One free MI_COLUMNDEF element (*recinfo points here)
   
    This function may use the free element to create hash column for unique
    constraint.

   RETURN
     FALSE - OK
     TRUE  - Error
*/

static bool create_myisam_tmp_table(TABLE *table, KEY *keyinfo, 
                                    MI_COLUMNDEF *start_recinfo,
                                    MI_COLUMNDEF **recinfo, 
				    ulonglong options, my_bool big_tables)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  TABLE_SHARE *share= table->s;
  DBUG_ENTER("create_myisam_tmp_table");

  if (share->keys)
  {						// Get keys for ni_create
    bool using_unique_constraint=0;
    HA_KEYSEG *seg= (HA_KEYSEG*) alloc_root(&table->mem_root,
                                            sizeof(*seg) * keyinfo->key_parts);
    if (!seg)
      goto err;

    bzero(seg, sizeof(*seg) * keyinfo->key_parts);
    if (keyinfo->key_length >= table->file->max_key_length() ||
	keyinfo->key_parts > table->file->max_key_parts() ||
	share->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      share->keys=    0;
      share->uniques= 1;
      using_unique_constraint=1;
      bzero((char*) &uniquedef,sizeof(uniquedef));
      uniquedef.keysegs=keyinfo->key_parts;
      uniquedef.seg=seg;
      uniquedef.null_are_equal=1;

      /* Create extra column for hash value */
      bzero((uchar*) *recinfo,sizeof(**recinfo));
      (*recinfo)->type= FIELD_CHECK;
      (*recinfo)->length=MI_UNIQUE_HASH_LENGTH;
      (*recinfo)++;
      share->reclength+=MI_UNIQUE_HASH_LENGTH;
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
	seg->bit_start= (uint8)(field->pack_length() - share->blob_ptr_size);
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

  if (big_tables && !(options & SELECT_SMALL_RESULT))
    create_info.data_file_length= ~(ulonglong) 0;

  if ((error=mi_create(share->table_name.str, share->keys, &keydef,
		       (uint) (*recinfo-start_recinfo),
		       start_recinfo,
		       share->uniques, &uniquedef,
		       &create_info,
		       HA_CREATE_TMP_TABLE)))
  {
    table->file->print_error(error,MYF(0));	/* purecov: inspected */
    table->db_stat=0;
    goto err;
  }
  status_var_increment(table->in_use->status_var.created_tmp_disk_tables);
  share->db_record_offset= 1;
  DBUG_RETURN(0);
 err:
  DBUG_RETURN(1);
}


void
free_tmp_table(THD *thd, TABLE *entry)
{
  MEM_ROOT own_root= entry->mem_root;
  const char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter",("table: %s",entry->alias));

  save_proc_info=thd->proc_info;
  thd_proc_info(thd, "removing tmp table");

  // Release latches since this can take a long time
  ha_release_temporary_latches(thd);

  if (entry->file)
  {
    if (entry->db_stat)
      entry->file->ha_drop_table(entry->s->table_name.str);
    else
      entry->file->ha_delete_table(entry->s->table_name.str);
    delete entry->file;
  }

  /* free blobs */
  for (Field **ptr=entry->field ; *ptr ; ptr++)
    (*ptr)->free();
  free_io_cache(entry);

  if (entry->temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, entry->temp_pool_slot);

  plugin_unlock(0, entry->s->db_plugin);

  free_root(&own_root, MYF(0)); /* the table is allocated in its own root */
  thd_proc_info(thd, save_proc_info);

  DBUG_VOID_RETURN;
}

/**
  If a MEMORY table gets full, create a disk-based table and copy all rows
  to this.

  @param thd             THD reference
  @param table           Table reference
  @param start_recinfo   Engine's column descriptions
  @param recinfo[in,out] End of engine's column descriptions
  @param error           Reason why inserting into MEMORY table failed. 
  @param ignore_last_dup If true, ignore duplicate key error for last
                         inserted key (see detailed description below).
  @param is_duplicate[out] if non-NULL and ignore_last_dup is TRUE,
                         return TRUE if last key was a duplicate,
                         and FALSE otherwise.

  @detail
    Function can be called with any error code, but only HA_ERR_RECORD_FILE_FULL
    will be handled, all other errors cause a fatal error to be thrown.
    The function creates a disk-based temporary table, copies all records
    from the MEMORY table into this new table, deletes the old table and
    switches to use the new table within the table handle.
    The function uses table->record[1] as a temporary buffer while copying.

    The function assumes that table->record[0] contains the row that caused
    the error when inserting into the MEMORY table (the "last row").
    After all existing rows have been copied to the new table, the last row
    is attempted to be inserted as well. If ignore_last_dup is true,
    this row can be a duplicate of an existing row without throwing an error.
    If is_duplicate is non-NULL, an indication of whether the last row was
    a duplicate is returned.
*/

bool create_myisam_from_heap(THD *thd, TABLE *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
			     int error, bool ignore_last_dup,
                             bool *is_duplicate)
{
  TABLE new_table;
  TABLE_SHARE share;
  const char *save_proc_info;
  int write_err;
  DBUG_ENTER("create_myisam_from_heap");

  if (table->s->db_type() != heap_hton || 
      error != HA_ERR_RECORD_FILE_FULL)
  {
    /*
      We don't want this error to be converted to a warning, e.g. in case of
      INSERT IGNORE ... SELECT.
    */
    table->file->print_error(error, MYF(ME_FATALERROR));
    DBUG_RETURN(1);
  }

  // Release latches since this can take a long time
  ha_release_temporary_latches(thd);

  new_table= *table;
  share= *table->s;
  new_table.s= &share;
  new_table.s->db_plugin= ha_lock_engine(thd, myisam_hton);
  if (!(new_table.file= get_new_handler(&share, &new_table.mem_root,
                                        new_table.s->db_type())))
    DBUG_RETURN(1);				// End of memory

  save_proc_info=thd->proc_info;
  thd_proc_info(thd, "converting HEAP to MyISAM");

  if (create_myisam_tmp_table(&new_table, table->key_info, start_recinfo,
                              recinfo,
			      (thd->lex->select_lex.options |
                               thd->variables.option_bits),
                              thd->variables.big_tables))
    goto err2;
  if (open_tmp_table(&new_table))
    goto err1;
  if (table->file->indexes_are_disabled())
    new_table.file->ha_disable_indexes(HA_KEY_SWITCH_ALL);
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
  table->file->info(HA_STATUS_VARIABLE); /* update table->file->stats.records */
  new_table.file->ha_start_bulk_insert(table->file->stats.records);
#else
  /* HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it */
  new_table.file->extra(HA_EXTRA_WRITE_CACHE);
#endif

  /*
    copy all old rows from heap table to MyISAM table
    This is the only code that uses record[1] to read/write but this
    is safe as this is a temporary MyISAM table without timestamp/autoincrement
    or partitioning.
  */
  while (!table->file->ha_rnd_next(new_table.record[1]))
  {
    write_err= new_table.file->ha_write_row(new_table.record[1]);
    DBUG_EXECUTE_IF("raise_error", write_err= HA_ERR_FOUND_DUPP_KEY ;);
    if (write_err)
      goto err;
  }
  /* copy row that filled HEAP table */
  if ((write_err=new_table.file->ha_write_row(table->record[0])))
  {
    if (new_table.file->is_fatal_error(write_err, HA_CHECK_DUP) ||
	!ignore_last_dup)
      goto err;
    if (is_duplicate)
      *is_duplicate= TRUE;
  }
  else
  {
    if (is_duplicate)
      *is_duplicate= FALSE;
  }

  /* remove heap table and change to use myisam table */
  (void) table->file->ha_rnd_end();
  (void) table->file->ha_close();              // This deletes the table !
  delete table->file;
  table->file=0;
  plugin_unlock(0, table->s->db_plugin);
  share.db_plugin= my_plugin_lock(0, &share.db_plugin);
  new_table.s= table->s;                       // Keep old share
  *table= new_table;
  *table->s= share;
  
  table->file->change_table_ptr(table, table->s);
  table->use_all_columns();
  if (save_proc_info)
    thd_proc_info(thd, (!strcmp(save_proc_info,"Copying to tmp table") ?
                  "Copying to tmp table on disk" : save_proc_info));
  DBUG_RETURN(0);

 err:
  DBUG_PRINT("error",("Got error: %d",write_err));
  table->file->print_error(write_err, MYF(0));
  (void) table->file->ha_rnd_end();
  (void) new_table.file->ha_close();
 err1:
  new_table.file->ha_delete_table(new_table.s->table_name.str);
 err2:
  delete new_table.file;
  thd_proc_info(thd, save_proc_info);
  table->mem_root= new_table.mem_root;
  DBUG_RETURN(1);
}


/**
  @details
  Rows produced by a join sweep may end up in a temporary table or be sent
  to a client. Setup the function of the nested loop join algorithm which
  handles final fully constructed and matched records.

  @param join   join to setup the function for.

  @return
    end_select function to use. This function can't fail.
*/

Next_select_func setup_end_select_func(JOIN *join)
{
  TABLE *table= join->tmp_table;
  TMP_TABLE_PARAM *tmp_tbl= &join->tmp_table_param;
  Next_select_func end_select;

  /* Set up select_end */
  if (table)
  {
    if (table->group && tmp_tbl->sum_func_count && 
        !tmp_tbl->precomputed_group_by)
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
    else if (join->sort_and_group && !tmp_tbl->precomputed_group_by)
    {
      DBUG_PRINT("info",("Using end_write_group"));
      end_select=end_write_group;
    }
    else
    {
      DBUG_PRINT("info",("Using end_write"));
      end_select=end_write;
      if (tmp_tbl->precomputed_group_by)
      {
        /*
          A preceding call to create_tmp_table in the case when loose
          index scan is used guarantees that
          TMP_TABLE_PARAM::items_to_copy has enough space for the group
          by functions. It is OK here to use memcpy since we copy
          Item_sum pointers into an array of Item pointers.
        */
        memcpy(tmp_tbl->items_to_copy + tmp_tbl->func_count,
               join->sum_funcs,
               sizeof(Item*)*tmp_tbl->sum_func_count);
        tmp_tbl->items_to_copy[tmp_tbl->func_count+tmp_tbl->sum_func_count]= 0;
      }
    }
  }
  else
  {
    /* 
       Choose method for presenting result to user. Use end_send_group
       if the query requires grouping (has a GROUP BY clause and/or one or
       more aggregate functions). Use end_send if the query should not
       be grouped.
     */
    if ((join->sort_and_group ||
         (join->procedure && join->procedure->flags & PROC_GROUP)) &&
        !tmp_tbl->precomputed_group_by)
    {
      DBUG_PRINT("info",("Using end_send_group"));
      end_select= end_send_group;
    }
    else
    {
      DBUG_PRINT("info",("Using end_send"));
      end_select= end_send;
    }
  }
  return end_select;
}


/**
  Make a join of all tables and write it on socket or to table.

  @retval
    0  if ok
  @retval
    1  if error is sent
  @retval
    -1  if error should be sent
*/

static int
do_select(JOIN *join,List<Item> *fields,TABLE *table,Procedure *procedure)
{
  int rc= 0;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  JOIN_TAB *join_tab= NULL;
  DBUG_ENTER("do_select");
  
  join->procedure=procedure;
  join->tmp_table= table;			/* Save for easy recursion */
  join->fields= fields;

  if (table)
  {
    (void) table->file->extra(HA_EXTRA_WRITE_CACHE);
    empty_record(table);
    if (table->group && join->tmp_table_param.sum_func_count &&
        table->s->keys && !table->file->inited)
      table->file->ha_index_init(0, 0);
  }
  /* Set up select_end */
  Next_select_func end_select= setup_end_select_func(join);
  if (join->tables)
  {
    join->join_tab[join->tables-1].next_select= end_select;

    join_tab=join->join_tab+join->const_tables;
  }
  join->send_records=0;
  if (join->tables == join->const_tables)
  {
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)
    */
    if (!join->conds || join->conds->val_int())
    {
      error= (*end_select)(join, 0, 0);
      if (error == NESTED_LOOP_OK || error == NESTED_LOOP_QUERY_LIMIT)
	error= (*end_select)(join, 0, 1);

      /*
        If we don't go through evaluate_join_record(), do the counting
        here.  join->send_records is increased on success in end_send(),
        so we don't touch it here.
      */
      join->examined_rows++;
      DBUG_ASSERT(join->examined_rows <= 1);
    }
    else if (join->send_row_on_empty_set())
    {
      List<Item> *columns_list= (procedure ? &join->procedure_fields_list :
                                 fields);
      rc= join->result->send_data(*columns_list);
    }
    /*
      An error can happen when evaluating the conds 
      (the join condition and piece of where clause 
      relevant to this join table).
    */
    if (join->thd->is_error())
      error= NESTED_LOOP_ERROR;
  }
  else
  {
    DBUG_ASSERT(join->tables);
    error= join->first_select(join,join_tab,0);
    if (error == NESTED_LOOP_OK || error == NESTED_LOOP_NO_MORE_ROWS)
      error= join->first_select(join,join_tab,1);
    if (error == NESTED_LOOP_QUERY_LIMIT)
      error= NESTED_LOOP_OK;                    /* select_limit used */
  }
  if (error == NESTED_LOOP_NO_MORE_ROWS)
    error= NESTED_LOOP_OK;


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
  else
  {
    /*
      The following will unlock all cursors if the command wasn't an
      update command
    */
    join->join_free();			// Unlock all cursors
  }
  if (error == NESTED_LOOP_OK)
  {
    /*
      Sic: this branch works even if rc != 0, e.g. when
      send_data above returns an error.
    */
    if (!table)					// If sending data to client
    {
      if (join->result->send_eof())
	rc= 1;                                  // Don't send error
    }
    DBUG_PRINT("info",("%ld records output", (long) join->send_records));
  }
  else
    rc= -1;
#ifndef DBUG_OFF
  if (rc)
  {
    DBUG_PRINT("error",("Error: do_select() failed"));
  }
#endif
  rc= join->thd->is_error() ? -1 : rc;
  DBUG_RETURN(rc);
}


int rr_sequential_and_unpack(READ_RECORD *info)
{
  int error;
  if ((error= rr_sequential(info)))
    return error;
  
  for (Copy_field *cp= info->copy_field; cp != info->copy_field_end; cp++)
    (*cp->do_copy)(cp);

  return error;
}


/*
  Semi-join materialization join function

  SYNOPSIS
    sub_select_sjm()
      join            The join
      join_tab        The first table in the materialization nest
      end_of_records  FALSE <=> This call is made to pass another record 
                                combination
                      TRUE  <=> EOF

  DESCRIPTION
    This is a join execution function that does materialization of a join
    suborder before joining it to the rest of the join.

    The table pointed by join_tab is the first of the materialized tables.
    This function first creates the materialized table and then switches to
    joining the materialized table with the rest of the join.

    The materialized table can be accessed in two ways:
     - index lookups
     - full table scan

  RETURN
    One of enum_nested_loop_state values
*/

enum_nested_loop_state
sub_select_sjm(JOIN *join, JOIN_TAB *join_tab, bool end_of_records)
{
  int res;
  enum_nested_loop_state rc;

  DBUG_ENTER("sub_select_sjm");

  if (!join_tab->emb_sj_nest)
  {
    /*
      We're handling GROUP BY/ORDER BY, this is the first table, and we've
      actually executed the join already and now we're just reading the
      result of the join from the temporary table.
      Bypass to regular join handling.
      Yes, it would be nicer if sub_select_sjm wasn't called at all in this
      case but there's no easy way to arrange this.
    */
    rc= sub_select(join, join_tab, end_of_records);
    DBUG_RETURN(rc);
  }

  Semijoin_mat_exec *sjm= join_tab->emb_sj_nest->sj_mat_exec;
  if (end_of_records)
  {
    rc= (*join_tab[sjm->table_count - 1].next_select)
          (join, join_tab + sjm->table_count, end_of_records);
    DBUG_RETURN(rc);
  }
  if (!sjm->materialized)
  {
    /*
      Do the materialization. First, put end_sj_materialize after the last
      inner table so we can catch record combinations of sj-inner tables.
    */
    Next_select_func next_func= join_tab[sjm->table_count - 1].next_select;
    join_tab[sjm->table_count - 1].next_select= end_sj_materialize;

    /*
      Now run the join for the inner tables. The first call is to run the
      join, the second one is to signal EOF (this is essential for some
      join strategies, e.g. it will make join buffering flush the records)
    */
    if ((rc= sub_select(join, join_tab, FALSE)) < 0 ||
        (rc= sub_select(join, join_tab, TRUE/*EOF*/)) < 0)
    {
      join_tab[sjm->table_count - 1].next_select= next_func;
      DBUG_RETURN(rc); /* it's NESTED_LOOP_(ERROR|KILLED)*/
    }
    join_tab[sjm->table_count - 1].next_select= next_func;

    /*
      Ok, materialization finished. Initialize the access to the temptable
    */
    sjm->materialized= TRUE;
    join_tab->read_record.read_record= join_no_more_records;
    if (sjm->is_scan)
    {
      /* Initialize full scan */
      JOIN_TAB *last_tab= join_tab + (sjm->table_count - 1);
      init_read_record(&last_tab->read_record, join->thd,
                       sjm->table, NULL, TRUE, TRUE, FALSE);

      DBUG_ASSERT(last_tab->read_record.read_record == rr_sequential);
      last_tab->read_first_record= join_read_record_no_init;
      last_tab->read_record.copy_field= sjm->copy_field;
      last_tab->read_record.copy_field_end= sjm->copy_field +
                                            sjm->table_cols.elements;
      last_tab->read_record.read_record= rr_sequential_and_unpack;
    }
  }
  else
  {
    if (sjm->is_scan)
    {
      /* Reset the cursor for a new scan over the table */
      if (sjm->table->file->ha_rnd_init(TRUE))
        DBUG_RETURN(NESTED_LOOP_ERROR);
    }
  }

  if (sjm->is_scan)
  {
    /* Do full scan of the materialized table */
    JOIN_TAB *last_tab= join_tab + (sjm->table_count - 1);

    Item *save_cond= last_tab->select_cond;
    last_tab->set_select_cond(sjm->join_cond, __LINE__);
    rc= sub_select(join, last_tab, end_of_records);
    last_tab->set_select_cond(save_cond, __LINE__);
    DBUG_RETURN(rc);
  }
  else
  {
    /* Do index lookup in the materialized table */
    if ((res= join_read_key2(join_tab, sjm->table, sjm->tab_ref)) == 1)
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if (res || !sjm->in_equality->val_int())
      DBUG_RETURN(NESTED_LOOP_NO_MORE_ROWS);
  }
  rc= (*join_tab[sjm->table_count - 1].next_select)
        (join, join_tab + sjm->table_count, end_of_records);
  DBUG_RETURN(rc);
}


/*
  Fill the join buffer with partial records, retrieve all full  matches for them

  SYNOPSIS
    sub_select_cache()
      join     pointer to the structure providing all context info for the query
      join_tab the first next table of the execution plan to be retrieved
      end_records  true when we need to perform final steps of the retrieval

  DESCRIPTION
    For a given table Ti= join_tab from the sequence of tables of the chosen
    execution plan T1,...,Ti,...,Tn the function just put the partial record
    t1,...,t[i-1] into the join buffer associated with table Ti unless this
    is the last record added into the buffer. In this case,  the function
    additionally finds all matching full records for all partial
    records accumulated in the buffer, after which it cleans the buffer up.
    If a partial join record t1,...,ti is extended utilizing a dynamic
    range scan then it is not put into the join buffer. Rather all matching
    records are found for it at once by the function sub_select.

  NOTES
    The function implements the algorithmic schema for both Blocked Nested
    Loop Join and Batched Key Access Join. The difference can be seen only at
    the level of of the implementation of the put_record and join_records
    virtual methods for the cache object associated with the join_tab.
    The put_record method accumulates records in the cache, while the
    join_records method builds all matching join records and send them into
    the output stream.

  RETURN
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

enum_nested_loop_state
sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool end_of_records)
{
  enum_nested_loop_state rc;
  JOIN_CACHE *cache= join_tab->cache;

  /* This function cannot be called if join_tab has no associated join buffer */
  DBUG_ASSERT(cache != NULL);

  cache->reset_join(join);

  DBUG_ENTER("sub_select_cache");

  if (end_of_records)
  {
    rc= cache->join_records(FALSE);
    if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
      rc= sub_select(join, join_tab, end_of_records);
    DBUG_RETURN(rc);
  }
  if (join->thd->killed)
  {
    /* The user has aborted the execution of the query */
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);
  }
  if (!test_if_use_dynamic_range_scan(join_tab))
  {
    if (!cache->put_record())
      DBUG_RETURN(NESTED_LOOP_OK);
    /*
      We has decided that after the record we've just put into the buffer
      won't add any more records. Now try to find all the matching
      extensions for all records in the buffer.
    */
    rc= cache->join_records(FALSE);
    DBUG_RETURN(rc);
  }
  /*
     TODO: Check whether we really need the call below and we can't do
           without it. If it's not the case remove it.
  */
  rc= cache->join_records(TRUE);
  if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= sub_select(join, join_tab, end_of_records);
  DBUG_RETURN(rc);
}


/**
  Retrieve records ends with a given beginning from the result of a join.

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

  @note
    The function effectively implements the  final (n-k) nested loops
    of nested loops join algorithm, where k is the ordinal number of
    the join_tab table and n is the total number of tables in the join query.
    It performs nested loops joins with all conjunctive predicates from
    the where condition pushed as low to the tables as possible.
    E.g. for the query
    @code
      SELECT * FROM t1,t2,t3
      WHERE t1.a=t2.a AND t2.b=t3.b AND t1.a BETWEEN 5 AND 9
    @endcode
    the predicate (t1.a BETWEEN 5 AND 9) will be pushed to table t1,
    given the selected plan prescribes to nest retrievals of the
    joined tables in the following order: t1,t2,t3.
    A pushed down predicate are attached to the table which it pushed to,
    at the field join_tab->select_cond.
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
    @code
      SELECT * FROM t1 LEFT JOIN (t2,t3) ON t3.a=t1.a
      WHERE t1>2 AND (t2.b>5 OR t2.b IS NULL)
    @endcode
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
    the predicates from on expressions are not checked. 

  @par
  @b IMPLEMENTATION
  @par
    The function forms output rows for a current partial join of k
    tables tables recursively.
    For each partial join record ending with a certain row from
    join_tab it calls sub_select that builds all possible matching
    tails from the result set.
    To be able  check predicates conditionally items of the class
    Item_func_trig_cond are employed.
    An object of  this class is constructed from an item of class COND
    and a pointer to a guarding boolean variable.
    When the value of the guard variable is true the value of the object
    is the same as the value of the predicate, otherwise it's just returns
    true. 
    To carry out a return to a nested loop level of join table t the pointer 
    to t is remembered in the field 'return_tab' of the join structure.
    Consider the following query:
    @code
        SELECT * FROM t1,
                      LEFT JOIN
                      (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                      ON t4.a=t2.a
           WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    @endcode
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

    @par
    @b STRUCTURE @b NOTES
    @par
    join_tab->first_unmatched points always backwards to the first inner
    table of the embedding nested join, if any.

  @param join      pointer to the structure providing all context info for
                   the query
  @param join_tab  the first next table of the execution plan to be retrieved
  @param end_records  true when we need to perform final steps of retrival   

  @return
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

enum_nested_loop_state
sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  DBUG_ENTER("sub_select");

  join_tab->table->null_row=0;
  if (end_of_records)
  {
    enum_nested_loop_state nls=
      (*join_tab->next_select)(join,join_tab+1,end_of_records);
    DBUG_RETURN(nls);
  }
  int error;
  enum_nested_loop_state rc;
  READ_RECORD *info= &join_tab->read_record;

  if (join_tab->flush_weedout_table)
  {
    do_sj_reset(join_tab->flush_weedout_table);
  }

  join->return_tab= join_tab;
  join_tab->not_null_compl= TRUE;

  if (join_tab->last_inner)
  {
    /* join_tab is the first inner table for an outer join operation. */

    /* Set initial state of guard variables for this table.*/
    join_tab->found=0;

    /* Set first_unmatched for the last inner table of this group */
    join_tab->last_inner->first_unmatched= join_tab;
  }
  join->thd->warning_info->reset_current_row_for_warning();

  error= (*join_tab->read_first_record)(join_tab);

  if (join_tab->keep_current_rowid)
    join_tab->table->file->position(join_tab->table->record[0]);
  
  rc= evaluate_join_record(join, join_tab, error);
  
  /* 
    Note: psergey has added the 2nd part of the following condition; the 
    change should probably be made in 5.1, too.
  */
  while (rc == NESTED_LOOP_OK && join->return_tab >= join_tab)
  {
    error= info->read_record(info);

    if (join_tab->keep_current_rowid)
      join_tab->table->file->position(join_tab->table->record[0]);
    
    rc= evaluate_join_record(join, join_tab, error);
  }

  if (rc == NESTED_LOOP_NO_MORE_ROWS &&
      join_tab->last_inner && !join_tab->found)
    rc= evaluate_null_complemented_join_record(join, join_tab);

  if (rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= NESTED_LOOP_OK;
  DBUG_RETURN(rc);
}


/**
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

  DBUG_ENTER("do_sj_dups_weedout");

  if (sjtbl->is_confluent)
  {
    if (sjtbl->have_confluent_row) 
      DBUG_RETURN(1);
    else
    {
      sjtbl->have_confluent_row= TRUE;
      DBUG_RETURN(0);
    }
  }

  uchar *ptr= sjtbl->tmp_table->record[0] + 1;
  uchar *nulls_ptr= ptr;
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

  error= sjtbl->tmp_table->file->ha_write_row(sjtbl->tmp_table->record[0]);
  if (error)
  {
    /* If this is a duplicate error, return immediately */
    if (!sjtbl->tmp_table->file->is_fatal_error(error, HA_CHECK_DUP))
      DBUG_RETURN(1);
    /*
      Other error than duplicate error: Attempt to create a temporary table.
    */
    bool is_duplicate;
    if (create_myisam_from_heap(thd, sjtbl->tmp_table,
                                sjtbl->start_recinfo, &sjtbl->recinfo,
                                error, TRUE, &is_duplicate))
      DBUG_RETURN(-1);
    DBUG_RETURN(is_duplicate ? 1 : 0);
  }
  DBUG_RETURN(0);
}


/**
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
  sj_tbl->have_confluent_row= FALSE;
  DBUG_RETURN(0);
}

/**
  @brief Process one row of the nested loop join.

  This function will evaluate parts of WHERE/ON clauses that are
  applicable to the partial row on hand and in case of success
  submit this row to the next level of the nested loop.

  @param  join     - The join object
  @param  join_tab - The most inner join_tab being processed
  @param  error > 0: Error, terminate processing
                = 0: (Partial) row is available
                < 0: No more rows available at this level
  @return Nested loop state (Ok, No_more_rows, Error, Killed)
*/

static enum_nested_loop_state
evaluate_join_record(JOIN *join, JOIN_TAB *join_tab,
                     int error)
{
  bool not_used_in_distinct=join_tab->not_used_in_distinct;
  ha_rows found_records=join->found_records;
  Item *select_cond= join_tab->select_cond;
  bool found= TRUE;

  DBUG_ENTER("evaluate_join_record");

  DBUG_PRINT("enter",
             ("evaluate_join_record join: %p join_tab: %p"
              " cond: %p error: %d", join, join_tab, select_cond, error));
  if (error > 0 || (join->thd->is_error()))     // Fatal error
    DBUG_RETURN(NESTED_LOOP_ERROR);
  if (error < 0)
    DBUG_RETURN(NESTED_LOOP_NO_MORE_ROWS);
  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);            /* purecov: inspected */
  }
  DBUG_PRINT("info", ("select cond 0x%lx", (ulong)select_cond));

  if (select_cond)
  {
    found= test(select_cond->val_int());

    /* check for errors evaluating the condition */
    if (join->thd->is_error())
      DBUG_RETURN(NESTED_LOOP_ERROR);
  }
  if (found)
  {
    /*
      There is no select condition or the attached pushed down
      condition is true => a match is found.
    */
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
        if (tab->table->reginfo.not_exists_optimize)
        {
          /*
            When not_exists_optimizer is set and a matching row is found, the
            outer row should be excluded from the result set: no need to
            explore this record and other records of 'tab', so we return "no
            records". But as we set 'found' above, evaluate_join_record() at
            the upper level will not yield a NULL-complemented record.
          */
          DBUG_RETURN(NESTED_LOOP_NO_MORE_ROWS);
        }
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
            DBUG_RETURN(NESTED_LOOP_OK);
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

    JOIN_TAB *return_tab= join->return_tab;

    if (join_tab->check_weed_out_table && found)
    {
      int res= do_sj_dups_weedout(join->thd, join_tab->check_weed_out_table);
      if (res == -1)
        DBUG_RETURN(NESTED_LOOP_ERROR);
      else if (res == 1)
        found= FALSE;
    }
    else if (join_tab->loosescan_match_tab && 
             join_tab->loosescan_match_tab->found_match)
    { 
      /* 
         Previous row combination for duplicate-generating range,
         generated a match.  Compare keys of this row and previous row
         to determine if this is a duplicate that should be skipped.
       */
      if (key_cmp(join_tab->table->key_info[join_tab->index].key_part,
                  join_tab->loosescan_buf, join_tab->loosescan_key_len))
        /* 
           Keys do not match.  
           Reset found_match for last table of duplicate-generating range, 
           to avoid comparing keys until a new match has been found.
        */
        join_tab->loosescan_match_tab->found_match= FALSE;
      else
        found= FALSE;
    }
    else if (join_tab->do_firstmatch)
    {
      /* 
        We should return to the join_tab->do_firstmatch after we have 
        enumerated all the suffixes for current prefix row combination
      */
      return_tab= join_tab->do_firstmatch;
    }

    join_tab->found_match= TRUE;

    /*
      It was not just a return to lower loop level when one
      of the newly activated predicates is evaluated as false
      (See above join->return_tab= tab).
    */
    join->examined_rows++;
    DBUG_PRINT("counts", ("evaluate_join_record join->examined_rows++: %lu",
                          (ulong) join->examined_rows));

    if (found)
    {
      enum enum_nested_loop_state rc;
      /* A match from join_tab is found for the current partial join. */
      rc= (*join_tab->next_select)(join, join_tab+1, 0);
      join->thd->warning_info->inc_current_row_for_warning();
      if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
        DBUG_RETURN(rc);

      if (join_tab->loosescan_match_tab && 
          join_tab->loosescan_match_tab->found_match)
      {
        /* 
           A match was found for a duplicate-generating range of a semijoin. 
           Copy key to be able to determine whether subsequent rows
           will give duplicates that should be skipped.
        */
        KEY *key= join_tab->table->key_info + join_tab->index;
        key_copy(join_tab->loosescan_buf, join_tab->read_record.record, key, 
                 join_tab->loosescan_key_len);
      }

      if (return_tab < join->return_tab)
        join->return_tab= return_tab;

      if (join->return_tab < join_tab)
        DBUG_RETURN(NESTED_LOOP_OK);
      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        DBUG_RETURN(NESTED_LOOP_NO_MORE_ROWS);
    }
    else
    {
      join->thd->warning_info->inc_current_row_for_warning();
      if (join_tab->not_null_compl)
      {
        /* a NULL-complemented row is not in a table so cannot be locked */
        join_tab->read_record.unlock_row(join_tab);
      }
    }
  }
  else
  {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    join->examined_rows++;
    join->thd->warning_info->inc_current_row_for_warning();
    if (join_tab->not_null_compl)
      join_tab->read_record.unlock_row(join_tab);
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


/**

  @details
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
  Item *select_cond;

  DBUG_ENTER("evaluate_null_complemented_join_record");

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
      DBUG_RETURN(NESTED_LOOP_OK);
  }
  join_tab= last_inner_tab;
  /*
    From the point of view of the rest of execution, this record matches
    (it has been built and satisfies conditions, no need to do more evaluation
    on it). See similar code in evaluate_join_record().
  */
  JOIN_TAB *first_unmatched= join_tab->first_unmatched->first_upper;
  if (first_unmatched != NULL &&
      first_unmatched->last_inner != join_tab)
    first_unmatched= NULL;
  join_tab->first_unmatched= first_unmatched;
  /*
    The row complemented by nulls satisfies all conditions
    attached to inner tables.
    Finish evaluation of record and send it to be joined with
    remaining tables.
    Note that evaluate_join_record will re-evaluate the condition attached
    to the last inner table of the current outer join. This is not deemed to
    have a significant performance impact.
  */
  const enum_nested_loop_state rc= evaluate_join_record(join, join_tab, 0);
  DBUG_RETURN(rc);
}


/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/** Help function when we get some an error from the table handler. */

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
		    error, table->s->path.str);
  table->file->print_error(error,MYF(0));
  return 1;
}


int safe_index_read(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;
  if ((error=table->file->ha_index_read_map(table->record[0],
                                            tab->ref.key_buff,
                                            make_prev_keypart_map(tab->ref.key_parts),
                                            HA_READ_KEY_EXACT)))
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
      pos->ref_depend_map= 0;
      if (!table->maybe_null || error > 0)
	DBUG_RETURN(error);
    }
  }
  else
  {
    if (!table->key_read && table->covering_keys.is_set(tab->ref.key) &&
	!table->no_keyread &&
        (int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY)
    {
      table->set_keyread(TRUE);
      tab->index= tab->ref.key;
    }
    error=join_read_const(tab);
    table->set_keyread(FALSE);
    if (error)
    {
      tab->info="unique row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->records_read=0.0;
      pos->ref_depend_map= 0;
      if (!table->maybe_null || error > 0)
	DBUG_RETURN(error);
    }
  }
  /* We will evaluate on-expressions here only if it is not considered
     expensive.  This also prevents executing materialized subqueries
     in optimization phase.  This is necessary since proper setup for
     such execution has not been done at this stage.  
     (See comment in internal_remove_eq_conds() tagged 
     DontEvaluateMaterializedSubqueryTooEarly).
  */
  if (*tab->on_expr_ref && !table->null_row && 
      !(*tab->on_expr_ref)->is_expensive())
  {
    if ((table->null_row= test((*tab->on_expr_ref)->val_int() == 0)))
      mark_as_null_row(table);  
  }
  if (!table->null_row)
    table->maybe_null=0;

  /* Check appearance of new constant items in Item_equal objects */
  JOIN *join= tab->join;
  if (join->conds)
    update_const_equal_items(join->conds, tab);
  TABLE_LIST *tbl;
  for (tbl= join->select_lex->leaf_tables; tbl; tbl= tbl->next_leaf)
  {
    TABLE_LIST *embedded;
    TABLE_LIST *embedding= tbl;
    do
    {
      embedded= embedding;
      if (embedded->on_expr)
         update_const_equal_items(embedded->on_expr, tab);
      embedding= embedded->embedding;
    }
    while (embedding &&
           embedding->nested_join->join_list.head() == embedded);
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


/**
  Read a [constant] table when there is at most one matching row.

  @param tab			Table to read

  @retval
    0	Row was found
  @retval
    -1   Row was not found
  @retval
    1   Got an error (other than row not found) during read
*/

static int
join_read_const(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    table->status= 0;
    if (cp_buffer_from_ref(tab->join->thd, table, &tab->ref))
      error=HA_ERR_KEY_NOT_FOUND;
    else
    {
      error=table->file->ha_index_read_idx_map(table->record[0],tab->ref.key,
                                               (uchar*) tab->ref.key_buff,
                                               make_prev_keypart_map(tab->ref.key_parts),
                                               HA_READ_KEY_EXACT);
    }
    if (error)
    {
      table->status= STATUS_NOT_FOUND;
      mark_as_null_row(tab->table);
      empty_record(table);
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
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


/*
  eq_ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_key()
      tab  JOIN_TAB of the accessed table

  DESCRIPTION
    This is "read_fist" function for the eq_ref access method. The difference
    from ref access function is that is that it has a one-element lookup 
    cache (see cmp_buffer_with_ref)

  RETURN
    0  - Ok
   -1  - Row not found 
    1  - Error
*/

static int
join_read_key(JOIN_TAB *tab)
{
  return join_read_key2(tab, tab->table, &tab->ref);
}


/* 
  eq_ref access handler but generalized a bit to support TABLE and TABLE_REF
  not from the join_tab. See join_read_key for detailed synopsis.
*/
static int
join_read_key2(JOIN_TAB *tab, TABLE *table, TABLE_REF *table_ref)
{
  int error;
  if (!table->file->inited)
  {
    table->file->ha_index_init(table_ref->key, tab->sorted);
  }

  /*
    We needn't do "Late NULLs Filtering" because eq_ref is restricted to
    indices on NOT NULL columns (see create_ref_for_key()).
  */
  if (cmp_buffer_with_ref(tab->join->thd, table, table_ref) ||
      (table->status & (STATUS_GARBAGE | STATUS_NO_PARENT | STATUS_NULL_ROW)))
  {
    if (table_ref->key_err)
    {
      table->status=STATUS_NOT_FOUND;
      return -1;
    }
    /*
      Moving away from the current record. Unlock the row
      in the handler if it did not match the partial WHERE.
    */
    if (table_ref->has_record && table_ref->use_count == 0)
    {
      table->file->unlock_row();
      table_ref->has_record= FALSE;
    }
    error= table->file->ha_index_read_map(table->record[0],
                                          table_ref->key_buff,
                                          make_prev_keypart_map(table_ref->key_parts),
                                          HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return report_error(table, error);

    if (! error)
    {
      table_ref->has_record= TRUE;
      table_ref->use_count= 1;
    }
  }
  else if (table->status == 0)
  {
    DBUG_ASSERT(table_ref->has_record);
    table_ref->use_count++;
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}

/**
  Since join_read_key may buffer a record, do not unlock
  it if it was not used in this invocation of join_read_key().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  TABLE_REF buffer.
  @sa join_read_key()
*/

static void
join_read_key_unlock_row(st_join_table *tab)
{
  DBUG_ASSERT(tab->ref.use_count);
  if (tab->ref.use_count)
    tab->ref.use_count--;
}

/*
  ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_always_key()
      tab  JOIN_TAB of the accessed table

  DESCRIPTION
    This is "read_fist" function for the "ref" access method.
   
    The functon must leave the index initialized when it returns.
    ref_or_null access implementation depends on that.

  RETURN
    0  - Ok
   -1  - Row not found 
    1  - Error
*/

/*
  ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_always_key()
      tab  JOIN_TAB of the accessed table

  DESCRIPTION
    This is "read_fist" function for the "ref" access method.

  RETURN
    0  - Ok
   -1  - Row not found 
    1  - Error
*/

static int
join_read_always_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  /* Initialize the index first */
  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, tab->sorted);

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  TABLE_REF *ref= &tab->ref;
  if (ref->impossible_null_ref())
  {
    DBUG_PRINT("info", ("join_read_always_key null_rejected"));
    return -1;
  }

  if (cp_buffer_from_ref(tab->join->thd, table, ref))
    return -1;
  if ((error= table->file->ha_index_read_map(table->record[0],
                                             tab->ref.key_buff,
                                             make_prev_keypart_map(tab->ref.key_parts),
                                             HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return report_error(table, error);
    return -1; /* purecov: inspected */
  }
  return 0;
}


/**
  This function is used when optimizing away ORDER BY in 
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/
  
static int
join_read_last_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, tab->sorted);
  if (cp_buffer_from_ref(tab->join->thd, table, &tab->ref))
    return -1;
  if ((error=table->file->index_read_last_map(table->record[0],
                                              tab->ref.key_buff,
                                              make_prev_keypart_map(tab->ref.key_parts))))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
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

  if ((error= table->file->ha_index_next_same(table->record[0],
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

  if ((error= table->file->ha_index_prev(table->record[0])))
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


int read_first_record_seq(JOIN_TAB *tab)
{
  if (tab->read_record.file->ha_rnd_init(1))
    return 1;
  return (*tab->read_record.read_record)(&tab->read_record);
}

static int
test_if_quick_select(JOIN_TAB *tab)
{
  delete tab->select->quick;
  tab->select->quick=0;
  return tab->select->test_quick_select(tab->join->thd, tab->keys,
					(table_map) 0, HA_POS_ERROR, 0,
                                        FALSE);
}


static 
bool test_if_use_dynamic_range_scan(JOIN_TAB *join_tab)
{
    return (join_tab->use_quick == QS_DYNAMIC_RANGE && 
            test_if_quick_select(join_tab) > 0);
}

int join_init_read_record(JOIN_TAB *tab)
{
  int error; 
  if (tab->select && tab->select->quick && (error= tab->select->quick->reset()))
  {
    /* Ensures error status is propageted back to client */
    report_error(tab->table, error);
    return 1;
  }
  init_read_record(&tab->read_record, tab->join->thd, tab->table,
		   tab->select,1,1, FALSE);
  return (*tab->read_record.read_record)(&tab->read_record);
}

static int
join_read_record_no_init(JOIN_TAB *tab)
{
  return (*tab->read_record.read_record)(&tab->read_record);
}

static int
join_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table=tab->table;
  if (table->covering_keys.is_set(tab->index) && !table->no_keyread)
    table->set_keyread(TRUE);
  tab->table->status=0;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  tab->read_record.read_record=join_read_next;

  if (!table->file->inited)
    table->file->ha_index_init(tab->index, tab->sorted);
  if ((error= tab->table->file->ha_index_first(tab->table->record[0])))
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
  if ((error= info->file->ha_index_next(info->record)))
    return report_error(info->table, error);
  return 0;
}


static int
join_read_last(JOIN_TAB *tab)
{
  TABLE *table=tab->table;
  int error;
  if (table->covering_keys.is_set(tab->index) && !table->no_keyread)
    table->set_keyread(TRUE);
  tab->table->status=0;
  tab->read_record.read_record=join_read_prev;
  tab->read_record.table=table;
  tab->read_record.file=table->file;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  if (!table->file->inited)
    table->file->ha_index_init(tab->index, 1);
  if ((error= tab->table->file->ha_index_last(tab->table->record[0])))
    return report_error(table, error);
  return 0;
}


static int
join_read_prev(READ_RECORD *info)
{
  int error;
  if ((error= info->file->ha_index_prev(info->record)))
    return report_error(info->table, error);
  return 0;
}


static int
join_ft_read_first(JOIN_TAB *tab)
{
  int error;
  TABLE *table= tab->table;

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 1);
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


/**
  Reading of key with key reference and one part that may be NULL.
*/

int
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


int
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
    if (join->tables &&
        join->join_tab->is_using_loose_index_scan())
    {
      /* Copy non-aggregated fields when loose index scan is used. */
      copy_fields(&join->tmp_table_param);
    }
    if (join->having && join->having->val_int() == 0)
      DBUG_RETURN(NESTED_LOOP_OK);               // Didn't match having
    error=0;
    if (join->procedure)
      error=join->procedure->send_row(join->procedure_fields_list);
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
	    (jt->table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT) &&
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
	    join->send_records= table->file->stats.records;
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
enum_nested_loop_state
end_send_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	       bool end_of_records)
{
  int idx= -1;
  enum_nested_loop_state ok_code= NESTED_LOOP_OK;
  DBUG_ENTER("end_send_group");

  if (!join->first_record || end_of_records ||
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || 
        (end_of_records && !join->group && !join->group_optimized_away))
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
            List_iterator_fast<Item> it(*join->fields);
            Item *item;
	    /* No matching rows for group function */
	    join->clear();

            while ((item= it++))
              item->no_rows_in_result();
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
      (void)(test_if_item_cache_changed(join->group_fields));
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
enum_nested_loop_state
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
    if (copy_funcs(join->tmp_table_param.items_to_copy, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */

    if (!join->having || join->having->val_int())
    {
      int error;
      join->found_records++;
      if ((error=table->file->ha_write_row(table->record[0])))
      {
        if (!table->file->is_fatal_error(error, HA_CHECK_DUP))
	  goto end;
	if (create_myisam_from_heap(join->thd, table,
                                    join->tmp_table_param.start_recinfo,
                                    &join->tmp_table_param.recinfo,
				    error, TRUE, NULL))
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

/* ARGSUSED */
/** Group by searching after group record and updating it if possible. */

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
      group->buff[-1]= (char) group->field->is_null();
  }
  if (!table->file->ha_index_read_map(table->record[1],
                                      join->tmp_table_param.group_buff,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {						/* Update old record */
    restore_record(table,record[1]);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->ha_update_row(table->record[1],
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
  if (copy_funcs(join->tmp_table_param.items_to_copy, join->thd))
    DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */
  if ((error=table->file->ha_write_row(table->record[0])))
  {
    if (create_myisam_from_heap(join->thd, table,
                                join->tmp_table_param.start_recinfo,
                                &join->tmp_table_param.recinfo,
				error, FALSE, NULL))
      DBUG_RETURN(NESTED_LOOP_ERROR);            // Not a table_is_full error
    /* Change method to update rows */
    table->file->ha_index_init(0, 0);
    join->join_tab[join->tables-1].next_select=end_unique_update;
  }
  join->send_records++;
  DBUG_RETURN(NESTED_LOOP_OK);
}


/** Like end_update, but this is done with unique constraints instead of keys.  */

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
  if (copy_funcs(join->tmp_table_param.items_to_copy, join->thd))
    DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */

  if (!(error=table->file->ha_write_row(table->record[0])))
    join->send_records++;			// New group
  else
  {
    if ((int) table->file->get_dup_key(error) < 0)
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    if (table->file->ha_rnd_pos(table->record[1], table->file->dup_ref))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    restore_record(table,record[1]);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=table->file->ha_update_row(table->record[1],
                                          table->record[0])))
    {
      table->file->print_error(error,MYF(0));	/* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
enum_nested_loop_state
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
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
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
          int error= table->file->ha_write_row(table->record[0]);
          if (error &&
              create_myisam_from_heap(join->thd, table,
                                      join->tmp_table_param.start_recinfo,
                                      &join->tmp_table_param.recinfo,
                                      error, FALSE, NULL))
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
      (void)(test_if_item_cache_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      if (copy_funcs(join->tmp_table_param.items_to_copy, join->thd))
	DBUG_RETURN(NESTED_LOOP_ERROR);
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

/**
  @return
    1 if right_item is used removable reference key on left_item

  @note see comments in make_cond_for_table_from_pred() about careful
  usage/modifications of test_if_ref().
*/

static bool test_if_ref(Item *root_cond, 
                        Item_field *left_item,Item *right_item)
{
  Field *field=left_item->field;
  JOIN_TAB *join_tab= field->table->reginfo.join_tab;
  // No need to change const test
  if (!field->table->const_table && join_tab &&
      (!join_tab->first_inner ||
       *join_tab->first_inner->on_expr_ref == root_cond) &&
      /* "ref_or_null" implements "x=y or x is null", not "x=y" */
      (join_tab->type != JT_REF_OR_NULL))
  {
    Item *ref_item=part_of_refkey(field->table,field);
    if (ref_item && ref_item->eq(right_item,1))
    {
      right_item= right_item->real_item();
      if (right_item->type() == Item::FIELD_ITEM)
	return (field->eq_def(((Item_field *) right_item)->field));
      /* remove equalities injected by IN->EXISTS transformation */
      else if (right_item->type() == Item::CACHE_ITEM)
        return ((Item_cache *)right_item)->eq_def (field);
      if (right_item->const_item() && !(right_item->is_null()))
      {
	/*
	  We can remove binary fields and numerical fields except float,
	  as float comparison isn't 100 % secure
	  We have to keep normal strings to be able to check for end spaces

          sergefp: the above seems to be too restrictive. Counterexample:
            create table t100 (v varchar(10), key(v)) default charset=latin1;
            insert into t100 values ('a'),('a ');
            explain select * from t100 where v='a';
          The EXPLAIN shows 'using Where'. Running the query returns both
          rows, so it seems there are no problems with endspace in the most
          frequent case?
	*/
	if (field->binary() &&
	    field->real_type() != MYSQL_TYPE_STRING &&
	    field->real_type() != MYSQL_TYPE_VARCHAR &&
	    (field->type() != MYSQL_TYPE_FLOAT || field->decimals() == 0))
	{
	  return !store_val_in_field(field, right_item, CHECK_FIELD_WARN);
	}
      }
    }
  }
  return 0;					// keep test
}


/**
   Destructively replaces a sub-condition inside a condition tree. The
   parse tree is also altered.

   @note Because of current requirements for semijoin flattening, we do not
   need to recurse here, hence this function will only examine the top-level
   AND conditions. (see JOIN::prepare, comment starting with "Check if the 
   subquery predicate can be executed via materialization".)
   
   @param join The top-level query.

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
static bool replace_subcondition(JOIN *join, Item **tree, 
                                 Item *old_cond, Item *new_cond,
                                 bool do_fix_fields)
{
  if (*tree == old_cond)
  {
    *tree= new_cond;
    if (do_fix_fields && new_cond->fix_fields(join->thd, tree))
      return TRUE;
    join->select_lex->where= *tree;
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
        if (do_fix_fields && new_cond->fix_fields(join->thd, li.ref()))
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


/**
  Extract a condition that can be checked after reading given table
  
  @param cond       Condition to analyze
  @param tables     Tables for which "current field values" are available
  @param used_table Table that we're extracting the condition for (may 
                    also include PSEUDO_TABLE_BITS, and may be zero)
  @param exclude_expensive_cond  Do not push expensive conditions

  @retval <>NULL Generated condition
  @retval = NULL Already checked, OR error

  @details
    Extract the condition that can be checked after reading the table
    specified in 'used_table', given that current-field values for tables
    specified in 'tables' bitmap are available.
    If 'used_table' is 0, extract conditions for all tables in 'tables'.

    The function assumes that
      - Constant parts of the condition has already been checked.
      - Condition that could be checked for tables in 'tables' has already 
        been checked.
        
    The function takes into account that some parts of the condition are
    guaranteed to be true by employed 'ref' access methods (the code that
    does this is located at the end, search down for "EQ_FUNC").

  @note
    Make sure to keep the implementations of make_cond_for_table() and
    make_cond_after_sjm() synchronized.
    make_cond_for_info_schema() uses similar algorithm as well.
*/

static Item *
make_cond_for_table(Item *cond, table_map tables, table_map used_table,
                    bool exclude_expensive_cond)
{
  return make_cond_for_table_from_pred(cond, cond, tables, used_table,
                                       exclude_expensive_cond);
}

static Item *
make_cond_for_table_from_pred(Item *root_cond, Item *cond,
                              table_map tables, table_map used_table,
                              bool exclude_expensive_cond)
{
  /*
    Ignore this condition if
     1. We are extracting conditions for a specific table, and
     2. that table is not referenced by the condition, and
     3. exclude constant conditions not checked at optimization time if
        the table we are pushing conditions to is the first one.
        As a result, such conditions are not considered as already checked
        and will be checked at execution time, attached to the first table.
  */
  if (used_table &&                                                 // 1
      !(cond->used_tables() & used_table) &&                        // 2
      /*
        psergey: TODO: "used_table & 1" doesn't make sense in nearly any
        context. Look at setup_table_map(), table bits reflect the order 
        the tables were encountered by the parser. Check what we should
        replace this condition with.
      */
      !((used_table & 1) && cond->is_expensive()))                  // 3
    return NULL;

  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond= new Item_cond_and;
      if (!new_cond)
        return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        Item *fix= make_cond_for_table_from_pred(root_cond, item, 
                                                 tables, used_table,
                                                 exclude_expensive_cond);
        if (fix)
          new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
        return NULL;                          // Always true
      case 1:
        return new_cond->argument_list()->head();
      default:
        /*
          Item_cond_and do not need fix_fields for execution, its parameters
          are fixed or do not need fix_fields, too
        */
        new_cond->quick_fix_field();
        new_cond->used_tables_cache=
          ((Item_cond_and*) cond)->used_tables_cache & tables;
          return new_cond;
      }
    }
    else
    {                                         // Or list
      Item_cond_or *new_cond= new Item_cond_or;
      if (!new_cond)
        return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        Item *fix= make_cond_for_table_from_pred(root_cond, item,
                                                 tables, 0L,
                                                 exclude_expensive_cond);
	if (!fix)
          return NULL;                        // Always true
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
    Omit this condition if
     1. It has been marked as omittable before, or
     2. Some tables referred by the condition are not available, or
     3. We are extracting conditions for all tables, the condition is
        considered 'expensive', and we want to delay evaluation of such 
        conditions to the execution phase.
  */
  if (cond->marker == 3 ||                                             // 1
      (cond->used_tables() & ~tables) ||                               // 2
      (!used_table && exclude_expensive_cond && cond->is_expensive())) // 3
    return NULL;

  /*
    Extract this condition if
     1. It has already been marked as applicable, or
     2. It is not a <comparison predicate> (=, <, >, <=, >=, <=>)
  */
  if (cond->marker == 2 ||                                             // 1
      cond->eq_cmp_result() == Item::COND_OK)                          // 2
    return cond;

  /* 
    Remove equalities that are guaranteed to be true by use of 'ref' access
    method.
    Note that ref access implements "table1.field1 <=> table2.indexed_field2",
    i.e. if it passed a NULL field1, it will return NULL indexed_field2 if
    there are.
    Thus the equality "table1.field1 = table2.indexed_field2",
    is equivalent to "ref access AND table1.field1 IS NOT NULL"
    i.e. "ref access and proper setting/testing of ref->null_rejecting".
    Thus, we must be careful, that when we remove equalities below we also
    set ref->null_rejecting, and test it at execution; otherwise wrong NULL
    matches appear.
    So:
    - for the optimization phase, the code which is below, and the code in
    test_if_ref(), and in add_key_field(), must be kept in sync: if the
    applicability conditions in one place are relaxed, they should also be
    relaxed elsewhere.
    - for the execution phase, all possible execution methods must test
    ref->null_rejecting.
  */
  if (cond->type() == Item::FUNC_ITEM &&
      ((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item= ((Item_func*) cond)->arguments()[0]->real_item();
    Item *right_item= ((Item_func*) cond)->arguments()[1]->real_item();
    if ((left_item->type() == Item::FIELD_ITEM &&
         test_if_ref(root_cond, (Item_field*) left_item, right_item)) ||
        (right_item->type() == Item::FIELD_ITEM &&
         test_if_ref(root_cond, (Item_field*) right_item, left_item)))
    {
      cond->marker= 3;                   // Condition can be omitted
      return NULL;
    }
  }
  cond->marker= 2;                      // Mark condition as applicable
  return cond;
}

/**
  Generate a condition that can be checked after materializing a semi-join nest

  @param root_cond  Root condition, ancestor of the condition being analyzed.
  @param cond       Condition to analyze.
  @param tables     Tables in the outer part of the join nest, contains
                    correlated and non-correlated tables already seen.
  @param sjm_tables Tables within the semi-join nest (the inner part).

  @retval <>NULL Generated condition
  @retval = NULL Already checked, OR error

  @details
  A semijoin materialization with lookup is always non-correlated, ie
  the subquery is always resolved by performing a lookup generated in
  create_subquery_equalities, hence this function never needs to produce
  any condition for it.
  For a scan semijoin materialization, this function may return a condition
  to be checked, when there are outer tables before the SJM tables in the
  join prefix.

  @note
    Make sure to keep the implementations of make_cond_for_table() and
    make_cond_after_sjm() synchronized.
*/

static Item *
make_cond_after_sjm(Item *root_cond, Item *cond, table_map tables,
                    table_map sjm_tables)
{
  /*
    We can only test conditions that cover tables from the join prefix
    and tables from the semijoin nest. Other conditions will be handled
    by make_cond_for_table().
  */
  if ((!(cond->used_tables() & ~tables) || 
       !(cond->used_tables() & ~sjm_tables)))
    return NULL;

  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond= new Item_cond_and;
      if (!new_cond)
        return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        Item *fix= make_cond_after_sjm(root_cond, item, tables, sjm_tables);
        if (fix)
          new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
        return NULL;                    // Always true
      case 1:
        return new_cond->argument_list()->head();
      default:
	/*
          Item_cond_and do not need fix_fields for execution, its parameters
          are fixed or do not need fix_fields, too
	*/
        new_cond->quick_fix_field();
        new_cond->used_tables_cache=
          ((Item_cond_and*) cond)->used_tables_cache & tables;
        return new_cond;
      }
    }
    else
    {                                          // Or list
      Item_cond_or *new_cond= new Item_cond_or;
      if (!new_cond)
        return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        Item *fix= make_cond_after_sjm(root_cond, item, tables, 0L);
        if (!fix)
          return NULL;                  // Always true
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
    Omit this condition if
     1. It has been marked as omittable before, or
     2. Some tables referred by the condition are not available.
  */
  if (cond->marker == 3 ||                                             // 1
      cond->used_tables() & ~(tables | sjm_tables))                    // 2
    return NULL;

  /*
    Extract this condition if
     1. It has already been marked as applicable, or
     2. It is not a <comparison predicate> (=, <, >, <=, >=, <=>)
  */
  if (cond->marker == 2 ||                                             // 1
      cond->eq_cmp_result() == Item::COND_OK)                          // 2
    return cond;

  /* 
    Remove equalities that are guaranteed to be true by use of 'ref' access
    method
  */
  if (((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item= ((Item_func*) cond)->arguments()[0]->real_item();
    Item *right_item= ((Item_func*) cond)->arguments()[1]->real_item();
    if ((left_item->type() == Item::FIELD_ITEM &&
	 test_if_ref(root_cond, (Item_field*) left_item, right_item)) ||
        (right_item->type() == Item::FIELD_ITEM &&
	 test_if_ref(root_cond, (Item_field*) right_item, left_item)))
    {
      cond->marker= 3;                  // Condition can be omitted
      return NULL;
    }
  }
  cond->marker= 2;                      // Mark condition as applicable
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
    uint part;

    for (part=0 ; part < ref_parts ; part++)
    {
      if (table->reginfo.join_tab->ref.cond_guards[part])
        return 0;
    }

    for (part=0 ; part < ref_parts ; part++,key_part++)
      if (field->eq(key_part->field) &&
	  !(key_part->key_part_flag & HA_PART_KEY_SEG))
	return table->reginfo.join_tab->ref.items[part];
  }
  return (Item*) 0;
}


/**
  Test if one can use the key to resolve ORDER BY.

  @param order                 Sort order
  @param table                 Table to sort
  @param idx                   Index to check
  @param used_key_parts [out]  NULL by default, otherwise return value for
                               used key parts.


  @note
    used_key_parts is set to correct key parts used if return value != 0
    (On other cases, used_key_part may be changed)
    Note that the value may actually be greater than the number of index 
    key parts. This can happen for storage engines that have the primary 
    key parts as a suffix for every secondary key.

  @retval
    1   key is ok.
  @retval
    0   Key can't be used
  @retval
    -1   Reverse key can be used
*/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint idx,
				uint *used_key_parts= NULL)
{
  KEY_PART_INFO *key_part,*key_part_end;
  key_part=table->key_info[idx].key_part;
  key_part_end=key_part+table->key_info[idx].key_parts;
  key_part_map const_key_parts=table->const_key_parts[idx];
  int reverse=0;
  uint key_parts;
  my_bool on_pk_suffix= FALSE;
  DBUG_ENTER("test_if_order_by_key");

  for (; order ; order=order->next, const_key_parts>>=1)
  {
    Field *field=((Item_field*) (*order->item)->real_item())->field;
    int flag;

    /*
      Skip key parts that are constants in the WHERE clause.
      These are already skipped in the ORDER BY by const_expression_in_where()
    */
    for (; const_key_parts & 1 ; const_key_parts>>= 1)
      key_part++; 

    if (key_part == key_part_end)
    {
      /* 
        We are at the end of the key. Check if the engine has the primary
        key as a suffix to the secondary keys. If it has continue to check
        the primary key as a suffix.
      */
      if (!on_pk_suffix &&
          (table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
          table->s->primary_key != MAX_KEY &&
          table->s->primary_key != idx)
      {
        on_pk_suffix= TRUE;
        key_part= table->key_info[table->s->primary_key].key_part;
        key_part_end=key_part+table->key_info[table->s->primary_key].key_parts;
        const_key_parts=table->const_key_parts[table->s->primary_key];

        for (; const_key_parts & 1 ; const_key_parts>>= 1)
          key_part++; 
        /*
         The primary and secondary key parts were all const (i.e. there's
         one row).  The sorting doesn't matter.
        */
        if (key_part == key_part_end && reverse == 0)
        {
          key_parts= 0;
          reverse= 1;
          goto ok;
        }
      }
      else
        DBUG_RETURN(0);
    }

    if (key_part->field != field || !field->part_of_sortkey.is_set(idx))
      DBUG_RETURN(0);

    /* set flag to 1 if we can use read-next on key, else to -1 */
    flag= ((order->asc == !(key_part->key_part_flag & HA_REVERSE_SORT)) ?
           1 : -1);
    if (reverse && flag != reverse)
      DBUG_RETURN(0);
    reverse=flag;				// Remember if reverse
    key_part++;
  }
  if (on_pk_suffix)
  {
    uint used_key_parts_secondary= table->key_info[idx].key_parts;
    uint used_key_parts_pk=
      (uint) (key_part - table->key_info[table->s->primary_key].key_part);
    key_parts= used_key_parts_pk + used_key_parts_secondary;

    if (reverse == -1 &&
        (!(table->file->index_flags(idx, used_key_parts_secondary - 1, 1) &
           HA_READ_PREV) ||
         !(table->file->index_flags(table->s->primary_key,
                                    used_key_parts_pk - 1, 1) & HA_READ_PREV)))
      reverse= 0;                               // Index can't be used
  }
  else
  {
    key_parts= (uint) (key_part - table->key_info[idx].key_part);
    if (reverse == -1 && 
        !(table->file->index_flags(idx, key_parts-1, 1) & HA_READ_PREV))
      reverse= 0;                               // Index can't be used
  }
ok:
  if (used_key_parts != NULL)
    *used_key_parts= key_parts;
  DBUG_RETURN(reverse);
}


/**
  Find shortest key suitable for full table scan.

  @param table                 Table to scan
  @param usable_keys           Allowed keys

  @note
     As far as 
     1) clustered primary key entry data set is a set of all record
        fields (key fields and not key fields) and
     2) secondary index entry data is a union of its key fields and
        primary key fields (at least InnoDB and its derivatives don't
        duplicate primary key fields there, even if the primary and
        the secondary keys have a common subset of key fields),
     then secondary index entry data is always a subset of primary key entry.
     Unfortunately, key_info[nr].key_length doesn't show the length
     of key/pointer pair but a sum of key field lengths only, thus
     we can't estimate index IO volume comparing only this key_length
     value of secondary keys and clustered PK.
     So, try secondary keys first, and choose PK only if there are no
     usable secondary covering keys or found best secondary key include
     all table fields (i.e. same as PK):

  @return
    MAX_KEY     no suitable key found
    key index   otherwise
*/

uint find_shortest_key(TABLE *table, const key_map *usable_keys)
{
  uint best= MAX_KEY;
  uint usable_clustered_pk= (table->file->primary_key_is_clustered() &&
                             table->s->primary_key != MAX_KEY &&
                             usable_keys->is_set(table->s->primary_key)) ?
                            table->s->primary_key : MAX_KEY;
  if (!usable_keys->is_clear_all())
  {
    uint min_length= (uint) ~0;
    for (uint nr=0; nr < table->s->keys ; nr++)
    {
      if (nr == usable_clustered_pk)
        continue;
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
  if (usable_clustered_pk != MAX_KEY)
  {
    /*
     If the primary key is clustered and found shorter key covers all table
     fields then primary key scan normally would be faster because amount of
     data to scan is the same but PK is clustered.
     It's safe to compare key parts with table fields since duplicate key
     parts aren't allowed.
     */
    if (best == MAX_KEY ||
        table->key_info[best].key_parts >= table->s->fields)
      best= usable_clustered_pk;
  }
  return best;
}

/**
  Test if a second key is the subkey of the first one.

  @param key_part              First key parts
  @param ref_key_part          Second key parts
  @param ref_key_part_end      Last+1 part of the second key

  @note
    Second key MUST be shorter than the first one.

  @retval
    1	is a subkey
  @retval
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

/**
  Test if we can use one of the 'usable_keys' instead of 'ref' key
  for sorting.

  @param ref			Number of key, used for WHERE clause
  @param usable_keys		Keys for testing

  @return
    - MAX_KEY			If we can't use other key
    - the number of found key	Otherwise
*/

static uint
test_if_subkey(ORDER *order, TABLE *table, uint ref, uint ref_key_parts,
	       const key_map *usable_keys)
{
  uint nr;
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  KEY_PART_INFO *ref_key_part= table->key_info[ref].key_part;
  KEY_PART_INFO *ref_key_part_end= ref_key_part + ref_key_parts;

  for (nr= 0 ; nr < table->s->keys ; nr++)
  {
    if (usable_keys->is_set(nr) &&
	table->key_info[nr].key_length < min_length &&
	table->key_info[nr].key_parts >= ref_key_parts &&
	is_subkey(table->key_info[nr].key_part, ref_key_part,
		  ref_key_part_end) &&
	test_if_order_by_key(order, table, nr))
    {
      min_length= table->key_info[nr].key_length;
      best= nr;
    }
  }
  return best;
}


/**
  Check if GROUP BY/DISTINCT can be optimized away because the set is
  already known to be distinct.

  Used in removing the GROUP BY/DISTINCT of the following types of
  statements:
  @code
    SELECT [DISTINCT] <unique_key_cols>... FROM <single_table_ref>
      [GROUP BY <unique_key_cols>,...]
  @endcode

    If (a,b,c is distinct)
    then <any combination of a,b,c>,{whatever} is also distinct

    This function checks if all the key parts of any of the unique keys
    of the table are referenced by a list : either the select list
    through find_field_in_item_list or GROUP BY list through
    find_field_in_order_list.
    If the above holds and the key parts cannot contain NULLs then we 
    can safely remove the GROUP BY/DISTINCT,
    as no result set can be more distinct than an unique key.

  @param table                The table to operate on.
  @param find_func            function to iterate over the list and search
                              for a field

  @retval
    1                    found
  @retval
    0                    not found.
*/

static bool
list_contains_unique_index(TABLE *table,
                          bool (*find_func) (Field *, void *), void *data)
{
  for (uint keynr= 0; keynr < table->s->keys; keynr++)
  {
    if (keynr == table->s->primary_key ||
         (table->key_info[keynr].flags & HA_NOSAME))
    {
      KEY *keyinfo= table->key_info + keynr;
      KEY_PART_INFO *key_part, *key_part_end;

      for (key_part=keyinfo->key_part,
           key_part_end=key_part+ keyinfo->key_parts;
           key_part < key_part_end;
           key_part++)
      {
        if (key_part->field->maybe_null() || 
            !find_func(key_part->field, data))
          break;
      }
      if (key_part == key_part_end)
        return 1;
    }
  }
  return 0;
}


/**
  Helper function for list_contains_unique_index.
  Find a field reference in a list of ORDER structures.
  Finds a direct reference of the Field in the list.

  @param field                The field to search for.
  @param data                 ORDER *.The list to search in

  @retval
    1                    found
  @retval
    0                    not found.
*/

static bool
find_field_in_order_list (Field *field, void *data)
{
  ORDER *group= (ORDER *) data;
  bool part_found= 0;
  for (ORDER *tmp_group= group; tmp_group; tmp_group=tmp_group->next)
  {
    Item *item= (*tmp_group->item)->real_item();
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field->eq(field))
    {
      part_found= 1;
      break;
    }
  }
  return part_found;
}


/**
  Helper function for list_contains_unique_index.
  Find a field reference in a dynamic list of Items.
  Finds a direct reference of the Field in the list.

  @param[in] field             The field to search for.
  @param[in] data              List<Item> *.The list to search in

  @retval
    1                    found
  @retval
    0                    not found.
*/

static bool
find_field_in_item_list (Field *field, void *data)
{
  List<Item> *fields= (List<Item> *) data;
  bool part_found= 0;
  List_iterator<Item> li(*fields);
  Item *item;

  while ((item= li++))
  {
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field->eq(field))
    {
      part_found= 1;
      break;
    }
  }
  return part_found;
}


/**
  Test if we can skip the ORDER BY by using an index.

  SYNOPSIS
    test_if_skip_sort_order()
      tab
      order
      select_limit
      no_changes
      map

  If we can use an index, the JOIN_TAB / tab->select struct
  is changed to use the index.

  The index must cover all fields in <order>, or it will not be considered.

  @param no_changes No changes will be made to the query plan.

  @todo
    - sergeyp: Results of all index merge selects actually are ordered 
    by clustered PK values.

  @retval
    0    We have to use filesort to do the sorting
  @retval
    1    We can use an index.
*/

static bool
test_if_skip_sort_order(JOIN_TAB *tab,ORDER *order,ha_rows select_limit,
			bool no_changes, const key_map *map)
{
  int ref_key;
  uint ref_key_parts;
  int order_direction;
  uint used_key_parts;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  QUICK_SELECT_I *save_quick= 0;
  Item *orig_select_cond= 0;
  bool orig_select_cond_saved= false;
  bool changed_key= false;
  DBUG_ENTER("test_if_skip_sort_order");
  LINT_INIT(ref_key_parts);

  /*
    Keys disabled by ALTER TABLE ... DISABLE KEYS should have already
    been taken into account.
  */
  key_map usable_keys= *map;

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
    save_quick= select->quick;
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
      if (table->covering_keys.is_set(ref_key))
	usable_keys.intersect(table->covering_keys);
      /*
        If part of the select condition has been pushed we use the
        select condition as it was before pushing. The original
        select condition is saved so that it can be restored when
        exiting this function (if we have not changed index).
      */
      if (tab->pre_idx_push_select_cond)
      {
        orig_select_cond= tab->set_cond(tab->pre_idx_push_select_cond, __LINE__);
        orig_select_cond_saved= true;
      }

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
          Key_use *keyuse= tab->keyuse;
          while (keyuse->key != new_ref_key && keyuse->table == tab->table)
            keyuse++;

          if (create_ref_for_key(tab->join, tab, keyuse, 
                                 tab->join->const_table_map))
            goto use_filesort;

          pick_table_access_method(tab);
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
          key_map new_ref_key_map;  // Force the creation of quick select
          new_ref_key_map.set_bit(new_ref_key); // only for new_ref_key.

          if (select->test_quick_select(tab->join->thd, new_ref_key_map, 0,
                                        (tab->join->select_options &
                                         OPTION_FOUND_ROWS) ?
                                        HA_POS_ERROR :
                                        tab->join->unit->select_limit_cnt,0,
                                        TRUE) <=
              0)
            goto use_filesort;
	}
        ref_key= new_ref_key;
        changed_key= true;
      }
    }
    /* Check if we get the rows in requested sorted order by using the key */
    if (usable_keys.is_set(ref_key) &&
        (order_direction= test_if_order_by_key(order,table,ref_key,
					       &used_key_parts)))
      goto check_reverse_order;
  }
  {
    uint best_key_parts= 0;
    uint saved_best_key_parts= 0;
    int best_key_direction= 0;
    int best_key= -1;
    JOIN *join= tab->join;
    ha_rows table_records= table->file->stats.records;

    test_if_cheaper_ordering(tab, order, table, usable_keys,
                             ref_key, select_limit,
                             &best_key, &best_key_direction,
                             &select_limit, &best_key_parts,
                             &saved_best_key_parts);

    /*
      filesort() and join cache are usually faster than reading in 
      index order and not using join cache, except in case that chosen
      index is clustered primary key.
    */
    if ((select_limit >= table_records) &&
        (tab->type == JT_ALL &&
         tab->join->tables > tab->join->const_tables + 1) &&
         ((unsigned) best_key != table->s->primary_key ||
          !table->file->primary_key_is_clustered()))
      goto use_filesort;

    if (best_key >= 0)
    {
      bool quick_created= FALSE;
      if (table->quick_keys.is_set(best_key) && best_key != ref_key)
      {
        key_map map;           // Force the creation of quick select
        map.set_bit(best_key); // only best_key.
        quick_created=         
          select->test_quick_select(join->thd, map, 0,
                                    join->select_options & OPTION_FOUND_ROWS ?
                                    HA_POS_ERROR :
                                    join->unit->select_limit_cnt,
                                    TRUE, FALSE) > 0;
      }
      if (!no_changes)
      {
        /* 
           If ref_key used index tree reading only ('Using index' in EXPLAIN),
           and best_key doesn't, then revert the decision.
        */
        if (!table->covering_keys.is_set(best_key))
          table->set_keyread(FALSE);
        if (!quick_created)
	{
          tab->index= best_key;
          tab->read_first_record= best_key_direction > 0 ?
                                  join_read_first:join_read_last;
          tab->type=JT_NEXT;           // Read with index_first(), index_next()
          if (select && select->quick)
          {
            delete select->quick;
            select->quick= 0;
          }
          if (table->covering_keys.is_set(best_key))
            table->set_keyread(TRUE);
          if (tab->pre_idx_push_select_cond)
          {
            tab->set_cond(tab->pre_idx_push_select_cond, __LINE__);
            /*
              orig_select_cond is a part of pre_idx_push_select_cond,
              no need to restore it.
            */
            orig_select_cond= 0;
            orig_select_cond_saved= false;
          }
          table->file->ha_index_or_rnd_end();
          if (join->select_options & SELECT_DESCRIBE)
          {
            tab->ref.key= -1;
            tab->ref.key_parts= 0;
            if (select_limit < table_records) 
              tab->limit= select_limit;
          }
        }
        else if (tab->type != JT_ALL)
        {
          /*
            We're about to use a quick access to the table.
            We need to change the access method so as the quick access
            method is actually used.
          */
          DBUG_ASSERT(tab->select->quick);
          tab->type=JT_ALL;
          tab->use_quick=QS_RANGE;
          tab->ref.key= -1;
          tab->ref.key_parts=0;		// Don't use ref key.
          tab->read_first_record= join_init_read_record;
          if (tab->is_using_loose_index_scan())
            join->tmp_table_param.precomputed_group_by= TRUE;
          /*
            TODO: update the number of records in join->best_positions[tablenr]
          */
        }
      }
      order_direction= best_key_direction;
      /*
        saved_best_key_parts is actual number of used keyparts found by the
        test_if_order_by_key function. It could differ from keyinfo->key_parts,
        thus we have to restore it in case of desc order as it affects
        QUICK_SELECT_DESC behaviour.
      */
      used_key_parts= (order_direction == -1) ?
        saved_best_key_parts :  best_key_parts;
      changed_key= true;
    }
    else
      goto use_filesort; 
  } 

check_reverse_order:                  
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
        QUICK_SELECT_I *tmp;
        int quick_type= select->quick->get_type();
        if (quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE ||
            quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT ||
            quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION ||
            quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
        {
          tab->limit= 0;
          select->quick= save_quick;
          goto use_filesort;                   // Use filesort
        }
            
        /* ORDER BY range_key DESC */
	tmp= select->quick->make_reverse(used_key_parts);
	if (!tmp)
	{
          select->quick= save_quick;
          tab->limit= 0;
	  goto use_filesort;		// Reverse sort not supported
	}
	select->set_quick(tmp);
      }
    }
    else if (tab->type != JT_NEXT && tab->type != JT_REF_OR_NULL &&
             tab->ref.key >= 0 && tab->ref.key_parts <= used_key_parts)
    {
      /*
	SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC

	Use a traversal function that starts by reading the last row
	with key part (A) and then traverse the index backwards.
      */
      tab->read_first_record= join_read_last_key;
      tab->read_record.read_record= join_read_prev_same;
    }
  }
  else if (select && select->quick)
    select->quick->need_sorted_output();
  /*
    Restore condition only if we didn't chose index different to what we used
    for ICP.
  */
  if (orig_select_cond_saved && !changed_key)
    tab->set_cond(orig_select_cond, __LINE__);
  DBUG_RETURN(1);

use_filesort:
  if (orig_select_cond_saved)
    tab->set_cond(orig_select_cond, __LINE__);
  DBUG_RETURN(0);
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
     is_order_by        true if we are sorting on ORDER BY, false if GROUP BY
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
		  ha_rows filesort_limit, ha_rows select_limit,
                  bool is_order_by)
{
  uint length= 0;
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

  /*
    When there is SQL_BIG_RESULT do not sort using index for GROUP BY,
    and thus force sorting on disk unless a group min-max optimization
    is going to be used as it is applied now only for one table queries
    with covering indexes.
  */
  if ((order != join->group_list || 
       !(join->select_options & SELECT_BIG_RESULT) ||
       (select && select->quick &&
        select->quick->get_type() == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)) &&
      test_if_skip_sort_order(tab,order,select_limit,0, 
                              is_order_by ?  &table->keys_in_use_for_order_by :
                              &table->keys_in_use_for_group_by))
    DBUG_RETURN(0);
  for (ORDER *ord= join->order; ord; ord= ord->next)
    length++;
  if (!(join->sortorder= 
        make_unireg_sortorder(order, &length, join->sortorder)))
    goto err;				/* purecov: inspected */

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
      if (((uint) tab->ref.key != select->quick->index))
        table->set_keyread(FALSE);
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
			    get_ft_select(thd, table, tab->ref.key) :
			    get_quick_select_for_ref(thd, table, &tab->ref, 
                                                     tab->found_records))))
	goto err;
    }
  }

  /* Fill schema tables with data before filesort if it's necessary */
  if ((join->select_lex->options & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(join, PROCESSED_BY_CREATE_SORT_INDEX))
    goto err;

  if (table->s->tmp_table)
    table->file->info(HA_STATUS_VARIABLE);	// Get record count
  table->sort.found_records=filesort(thd, table,join->sortorder, length,
                                     select, filesort_limit, 0,
                                     &examined_rows);
  tab->records= table->sort.found_records;	// For SQL_CALC_ROWS
  if (select)
  {
    /*
      We need to preserve tablesort's output resultset here, because
      QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT (called by
      SQL_SELECT::cleanup()) may free it assuming it's the result of the quick
      select operation that we no longer need. Note that all the other parts of
      this data structure are cleaned up when
      QUICK_INDEX_MERGE_SELECT::get_next encounters end of data, so the next
      SQL_SELECT::cleanup() call changes sort.io_cache alone.
    */
    IO_CACHE *tablesort_result_cache;

    tablesort_result_cache= table->sort.io_cache;
    table->sort.io_cache= NULL;

    select->cleanup();				// filesort did select
    tab->select= 0;
    table->quick_keys.clear_all();  // as far as we cleanup select->quick
    table->sort.io_cache= tablesort_result_cache;
  }
  tab->set_select_cond(NULL, __LINE__);
  tab->last_inner= 0;
  tab->first_unmatched= 0;
  tab->type=JT_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  tab->join->examined_rows+=examined_rows;
  table->set_keyread(FALSE); // Restore if we used indexes
  DBUG_RETURN(table->sort.found_records == HA_POS_ERROR);
err:
  DBUG_RETURN(-1);
}


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

  if (!field_count && !(join->select_options & OPTION_FOUND_ROWS) && !having) 
  {                    // only const items with no OPTION_FOUND_ROWS
    join->unit->select_limit_cnt= 1;		// Only send first row
    DBUG_RETURN(0);
  }
  Field **first_field=entry->field+entry->s->fields - field_count;
  offset= (field_count ? 
           entry->field[entry->s->fields - field_count]->
           offset(entry->record[0]) : 0);
  reclength=entry->s->reclength-offset;

  free_io_cache(entry);				// Safety
  entry->file->info(HA_STATUS_VARIABLE);
  if (entry->s->db_type() == heap_hton ||
      (!entry->s->blob_fields &&
       ((ALIGN_SIZE(reclength) + HASH_OVERHEAD) * entry->file->stats.records <
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
  uchar *record;
  int error;
  ulong reclength= table->s->reclength-offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record=(char*) (record=table->record[0])+offset;
  new_record=(char*) table->record[1]+offset;

  file->ha_rnd_init(1);
  error=file->ha_rnd_next(record);
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
      {
        error= file->ha_rnd_next(record);
        continue;
      }
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->ha_delete_row(record)))
	goto err;
      error=file->ha_rnd_next(record);
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
      if ((error=file->ha_rnd_next(record)))
      {
	if (error == HA_ERR_RECORD_DELETED)
	  continue;
	if (error == HA_ERR_END_OF_FILE)
	  break;
	goto err;
      }
      if (compare_record(table, first_field) == 0)
      {
	if ((error=file->ha_delete_row(record)))
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


/**
  Generate a hash index for each row to quickly find duplicate rows.

  @note
    Note that this will not work on tables with blobs!
*/

static int remove_dup_with_hash_index(THD *thd, TABLE *table,
				      uint field_count,
				      Field **first_field,
				      ulong key_length,
				      Item *having)
{
  uchar *key_buffer, *key_pos, *record=table->record[0];
  int error;
  handler *file= table->file;
  ulong extra_length= ALIGN_SIZE(key_length)-key_length;
  uint *field_lengths,*field_length;
  HASH hash;
  DBUG_ENTER("remove_dup_with_hash_index");

  if (!my_multi_malloc(MYF(MY_WME),
		       &key_buffer,
		       (uint) ((key_length + extra_length) *
			       (long) file->stats.records),
		       &field_lengths,
		       (uint) (field_count*sizeof(*field_lengths)),
		       NullS))
    DBUG_RETURN(1);

  {
    Field **ptr;
    ulong total_length= 0;
    for (ptr= first_field, field_length=field_lengths ; *ptr ; ptr++)
    {
      uint length= (*ptr)->sort_length();
      (*field_length++)= length;
      total_length+= length;
    }
    DBUG_PRINT("info",("field_count: %u  key_length: %lu  total_length: %lu",
                       field_count, key_length, total_length));
    DBUG_ASSERT(total_length <= key_length);
    key_length= total_length;
    extra_length= ALIGN_SIZE(key_length)-key_length;
  }

  if (my_hash_init(&hash, &my_charset_bin, (uint) file->stats.records, 0, 
                   key_length, (my_hash_get_key) 0, 0, 0))
  {
    my_free(key_buffer);
    DBUG_RETURN(1);
  }

  file->ha_rnd_init(1);
  key_pos=key_buffer;
  for (;;)
  {
    uchar *org_key_pos;
    if (thd->killed)
    {
      thd->send_kill_message();
      error=0;
      goto err;
    }
    if ((error=file->ha_rnd_next(record)))
    {
      if (error == HA_ERR_RECORD_DELETED)
	continue;
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->ha_delete_row(record)))
	goto err;
      continue;
    }

    /* copy fields to key buffer */
    org_key_pos= key_pos;
    field_length=field_lengths;
    for (Field **ptr= first_field ; *ptr ; ptr++)
    {
      (*ptr)->sort_string(key_pos,*field_length);
      key_pos+= *field_length++;
    }
    /* Check if it exists before */
    if (my_hash_search(&hash, org_key_pos, key_length))
    {
      /* Duplicated found ; Remove the row */
      if ((error=file->ha_delete_row(record)))
	goto err;
    }
    else
    {
      if (my_hash_insert(&hash, org_key_pos))
        goto err;
    }
    key_pos+=extra_length;
  }
  my_free(key_buffer);
  my_hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->ha_rnd_end();
  DBUG_RETURN(0);

err:
  my_free(key_buffer);
  my_hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->ha_rnd_end();
  if (error)
    file->print_error(error,MYF(0));
  DBUG_RETURN(1);
}


SORT_FIELD *make_unireg_sortorder(ORDER *order, uint *length,
                                  SORT_FIELD *sortorder)
{
  uint count;
  SORT_FIELD *sort,*pos;
  DBUG_ENTER("make_unireg_sortorder");

  count=0;
  for (ORDER *tmp = order; tmp; tmp=tmp->next)
    count++;
  if (!sortorder)
    sortorder= (SORT_FIELD*) sql_alloc(sizeof(SORT_FIELD) *
                                       (max(count, *length) + 1));
  pos= sort= sortorder;

  if (!pos)
    return 0;

  for (;order;order=order->next,pos++)
  {
    Item *item= order->item[0]->real_item();
    pos->field= 0; pos->item= 0;
    if (item->type() == Item::FIELD_ITEM)
      pos->field= ((Item_field*) item)->field;
    else if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item())
      pos->field= ((Item_sum*) item)->get_tmp_table_field();
    else if (item->type() == Item::COPY_STR_ITEM)
    {						// Blob patch
      pos->item= ((Item_copy*) item)->get_item();
    }
    else
      pos->item= *order->item;
    pos->reverse=! order->asc;
  }
  *length=count;
  DBUG_RETURN(sort);
}


/*
  eq_ref: Create the lookup key and check if it is the same as saved key

  SYNOPSIS
    cmp_buffer_with_ref()
      tab      Join tab of the accessed table
      table    The table to read.  This is usually tab->table, except for 
               semi-join when we might need to make a lookup in a temptable
               instead.
      tab_ref  The structure with methods to collect index lookup tuple. 
               This is usually table->ref, except for the case of when we're 
               doing lookup into semi-join materialization table.

  DESCRIPTION 
    Used by eq_ref access method: create the index lookup key and check if 
    we've used this key at previous lookup (If yes, we don't need to repeat
    the lookup - the record has been already fetched)

  RETURN 
    TRUE   No cached record for the key, or failed to create the key (due to
           out-of-domain error)
    FALSE  The created key is the same as the previous one (and the record 
           is already in table->record)
*/

static bool
cmp_buffer_with_ref(THD *thd, TABLE *table, TABLE_REF *tab_ref)
{
  bool no_prev_key;
  if (!tab_ref->disable_cache)
  {
    if (!(no_prev_key= tab_ref->key_err))
    {
      /* Previous access found a row. Copy its key */
      memcpy(tab_ref->key_buff2, tab_ref->key_buff, tab_ref->key_length);
    }
  }
  else 
    no_prev_key= TRUE;
  if ((tab_ref->key_err= cp_buffer_from_ref(thd, table, tab_ref)) ||
      no_prev_key)
    return 1;
  return memcmp(tab_ref->key_buff2, tab_ref->key_buff, tab_ref->key_length)
    != 0;
}


bool
cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref)
{
  enum enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  bool result= 0;

  for (store_key **copy=ref->key_copy ; *copy ; copy++)
  {
    if ((*copy)->copy() & 1)
    {
      result= 1;
      break;
    }
  }
  thd->count_cuted_fields= save_count_cuted_fields;
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return result;
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
find_order_in_list(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
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
    order->item= ref_pointer_array + count - 1;
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
  order->item= ref_pointer_array + el;
  return FALSE;
}


/**
  Change order to point at item in select list.

  If item isn't a number and doesn't exits in the select list, add it the
  the field list.
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


/**
  Intitialize the GROUP BY list.

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

int
setup_group(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
	    List<Item> &fields, List<Item> &all_fields, ORDER *order,
	    bool *hidden_group_fields)
{
  *hidden_group_fields=0;
  ORDER *ord;

  if (!order)
    return 0;				/* Everything is ok */

  uint org_fields=all_fields.elements;

  thd->where="group statement";
  for (ord= order; ord; ord= ord->next)
  {
    if (find_order_in_list(thd, ref_pointer_array, tables, ord, fields,
			   all_fields, TRUE))
      return 1;
    (*ord->item)->marker= UNDEF_POS;		/* Mark found */
    if ((*ord->item)->with_sum_func)
    {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), (*ord->item)->full_name());
      return 1;
    }
  }
  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
  {
    /*
      Don't allow one to use fields that is not used in GROUP BY
      For each select a list of field references that aren't under an
      aggregate function is created. Each field in this list keeps the
      position of the select list expression which it belongs to.

      First we check an expression from the select list against the GROUP BY
      list. If it's found there then it's ok. It's also ok if this expression
      is a constant or an aggregate function. Otherwise we scan the list
      of non-aggregated fields and if we'll find at least one field reference
      that belongs to this expression and doesn't occur in the GROUP BY list
      we throw an error. If there are no fields in the created list for a
      select list expression this means that all fields in it are used under
      aggregate functions.
    */
    Item *item;
    Item_field *field;
    int cur_pos_in_select_list= 0;
    List_iterator<Item> li(fields);
    List_iterator<Item_field> naf_it(thd->lex->current_select->non_agg_fields);

    field= naf_it++;
    while (field && (item=li++))
    {
      if (item->type() != Item::SUM_FUNC_ITEM && item->marker >= 0 &&
          !item->const_item() &&
          !(item->real_item()->type() == Item::FIELD_ITEM &&
            item->used_tables() & OUTER_REF_TABLE_BIT))
      {
        while (field)
        {
          /* Skip fields from previous expressions. */
          if (field->marker < cur_pos_in_select_list)
            goto next_field;
          /* Found a field from the next expression. */
          if (field->marker > cur_pos_in_select_list)
            break;
          /*
            Check whether the field occur in the GROUP BY list.
            Throw the error later if the field isn't found.
          */
          for (ord= order; ord; ord= ord->next)
            if ((*ord->item)->eq((Item*)field, 0))
              goto next_field;
          /*
            TODO: change ER_WRONG_FIELD_WITH_GROUP to more detailed
            ER_NON_GROUPING_FIELD_USED
          */
          my_error(ER_WRONG_FIELD_WITH_GROUP, MYF(0), field->full_name());
          return 1;
next_field:
          field= naf_it++;
        }
      }
      cur_pos_in_select_list++;
    }
  }
  if (org_fields != all_fields.elements)
    *hidden_group_fields=1;			// group fields is not used
  return 0;
}

/**
  Add fields with aren't used at start of field list.

  @return
    FALSE if ok
*/

static bool
setup_new_fields(THD *thd, List<Item> &fields,
		 List<Item> &all_fields, ORDER *new_field)
{
  Item	  **item;
  uint counter;
  enum_resolution_type not_used;
  DBUG_ENTER("setup_new_fields");

  thd->mark_used_columns= MARK_COLUMNS_READ;       // Not really needed, but...
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

/**
  Create a group by that consist of all non const fields.

  Try to use the fields in the order given by 'order' to allow one to
  optimize away 'order by'.
*/

ORDER *
create_distinct_group(THD *thd, Item **ref_pointer_array,
                      ORDER *order_list, List<Item> &fields,
                      List<Item> &all_fields,
		      bool *all_order_by_fields_used)
{
  List_iterator<Item> li(fields);
  Item *item, **orig_ref_pointer_array= ref_pointer_array;
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
      /* 
        Don't put duplicate columns from the SELECT list into the 
        GROUP BY list.
      */
      ORDER *ord_iter;
      for (ord_iter= group; ord_iter; ord_iter= ord_iter->next)
        if ((*ord_iter->item)->eq(item, 1))
          goto next_item;
      
      ORDER *ord=(ORDER*) thd->calloc(sizeof(ORDER));
      if (!ord)
	return 0;

      if (item->type() == Item::FIELD_ITEM &&
          item->field_type() == MYSQL_TYPE_BIT)
      {
        /*
          Because HEAP tables can't index BIT fields we need to use an
          additional hidden field for grouping because later it will be
          converted to a LONG field. Original field will remain of the
          BIT type and will be returned to a client.
        */
        Item_field *new_item= new Item_field(thd, (Item_field*)item);
        int el= all_fields.elements;
        orig_ref_pointer_array[el]= new_item;
        all_fields.push_front(new_item);
        ord->item= orig_ref_pointer_array + el;
      }
      else
      {
        /*
          We have here only field_list (not all_field_list), so we can use
          simple indexing of ref_pointer_array (order in the array and in the
          list are same)
        */
        ord->item= ref_pointer_array;
      }
      ord->asc=1;
      *prev=ord;
      prev= &ord->next;
    }
next_item:
    ref_pointer_array++;
  }
  *prev=0;
  return group;
}


/**
  Update join with count of the different type of fields.
*/

void
count_field_types(SELECT_LEX *select_lex, TMP_TABLE_PARAM *param, 
                  List<Item> &fields, bool reset_with_sum_func)
{
  List_iterator<Item> li(fields);
  Item *field;

  param->field_count=param->sum_func_count=param->func_count=
    param->hidden_field_count=0;
  param->quick_group=1;
  while ((field=li++))
  {
    Item::Type real_type= field->real_item()->type();
    if (real_type == Item::FIELD_ITEM)
      param->field_count++;
    else if (real_type == Item::SUM_FUNC_ITEM)
    {
      if (! field->const_item())
      {
	Item_sum *sum_item=(Item_sum*) field->real_item();
        if (!sum_item->depended_from() ||
            sum_item->depended_from() == select_lex)
        {
          if (!sum_item->quick_group)
            param->quick_group=0;			// UDF SUM function
          param->sum_func_count++;

          for (uint i=0 ; i < sum_item->get_arg_count() ; i++)
          {
            if (sum_item->get_arg(i)->real_item()->type() == Item::FIELD_ITEM)
              param->field_count++;
            else
              param->func_count++;
          }
        }
        param->func_count++;
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


/**
  Return 1 if second is a subpart of first argument.

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

/**
  Return table number if there is only one table in sort order
  and group and order is compatible, else return 0.
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

  for (; !(map & tables->table->map); tables= tables->next_leaf) ;
  if (map != tables->table->map)
    DBUG_RETURN(0);				// More than one table
  DBUG_PRINT("exit",("sort by table: %d",tables->table->tablenr));
  DBUG_RETURN(tables->table);
}


/**
  calc how big buffer we need for comparing group entries.
*/

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
      enum_field_types type;
      if ((type= field->type()) == MYSQL_TYPE_BLOB)
	key_length+=MAX_BLOB_WIDTH;		// Can't be used as a key
      else if (type == MYSQL_TYPE_VARCHAR || type == MYSQL_TYPE_VAR_STRING)
        key_length+= field->field_length + HA_KEY_BLOB_LENGTH;
      else if (type == MYSQL_TYPE_BIT)
      {
        /* Bit is usually stored as a longlong key for group fields */
        key_length+= 8;                         // Big enough
      }
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
      {
        enum enum_field_types type= group_item->field_type();
        /*
          As items represented as DATE/TIME fields in the group buffer
          have STRING_RESULT result type, we increase the length 
          by 8 as maximum pack length of such fields.
        */
        if (type == MYSQL_TYPE_TIME ||
            type == MYSQL_TYPE_DATE ||
            type == MYSQL_TYPE_DATETIME ||
            type == MYSQL_TYPE_TIMESTAMP)
        {
          key_length+= 8;
        }
        else if (type == MYSQL_TYPE_BLOB)
          key_length+= MAX_BLOB_WIDTH;		// Can't be used as a key
        else
        {
          /*
            Group strings are taken as varstrings and require an length field.
            A field is not yet created by create_tmp_field()
            and the sizes should match up.
          */
          key_length+= group_item->max_length + HA_KEY_BLOB_LENGTH;
        }
        break;
      }
      default:
        /* This case should never be choosen */
        DBUG_ASSERT(0);
        my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
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


/**
  allocate group fields or take prepared (cached).

  @param main_join   join of current select
  @param curr_join   current join (join of current select or temporary copy
                     of it)

  @retval
    0   ok
  @retval
    1   failed
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


/**
  Get a list of buffers for saveing last group.

  Groups are saved in reverse order for easyer check loop.
*/

static bool
alloc_group_fields(JOIN *join,ORDER *group)
{
  if (group)
  {
    for (; group ; group=group->next)
    {
      Cached_item *tmp=new_Cached_item(join->thd, *group->item, FALSE);
      if (!tmp || join->group_fields.push_front(tmp))
	return TRUE;
    }
  }
  join->sort_and_group=1;			/* Mark for do_select */
  return FALSE;
}


/*
  Test if a single-row cache of items changed, and update the cache.

  @details Test if a list of items that typically represents a result
  row has changed. If the value of some item changed, update the cached
  value for this item.
  
  @param list list of <item, cached_value> pairs stored as Cached_item.

  @return -1 if no item changed
  @return index of the first item that changed
*/

int test_if_item_cache_changed(List<Cached_item> &list)
{
  DBUG_ENTER("test_if_item_cache_changed");
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


/**
  Setup copy_fields to save fields at start of new group.

  Setup copy_fields to save fields at start of new group

  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_result_set_metadata.

  @param thd                   THD pointer
  @param param                 temporary table parameters
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @todo
    In most cases this result will be sent to the user.
    This should be changed to use copy_int or copy_real depending
    on how the value is to be used: In some cases this may be an
    argument in a group function, like: IF(ISNULL(col),0,COUNT(*))

  @retval
    0     ok
  @retval
    !=0   error
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
  Copy_field *copy_start __attribute__((unused));
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
  copy_start= copy;
  for (i= 0; (pos= li++); i++)
  {
    Field *field;
    uchar *tmp;
    Item *real_pos= pos->real_item();
    /*
      Aggregate functions can be substituted for fields (by e.g. temp tables).
      We need to filter those substituted fields out.
    */
    if (real_pos->type() == Item::FIELD_ITEM &&
        !(real_pos != pos &&
          ((Item_ref *)pos)->ref_type() == Item_ref::AGGREGATE_REF))
    {
      Item_field *item;
      if (!(item= new Item_field(thd, ((Item_field*) real_pos))))
	goto err;
      if (pos->type() == Item::REF_ITEM)
      {
        /* preserve the names of the ref when dereferncing */
        Item_ref *ref= (Item_ref *) pos;
        item->db_name= ref->db_name;
        item->table_name= ref->table_name;
        item->name= ref->name;
      }
      pos= item;
      if (item->field->flags & BLOB_FLAG)
      {
	if (!(pos= Item_copy::create(pos)))
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
	field= item->field;
	item->result_field=field->new_field(thd->mem_root,field->table, 1);
        /*
          We need to allocate one extra byte for null handling and
          another extra byte to not get warnings from purify in
          Field_string::val_int
        */
	if (!(tmp= (uchar*) sql_alloc(field->pack_length()+2)))
	  goto err;
        if (copy)
        {
          DBUG_ASSERT (param->field_count > (uint) (copy - copy_start));
          copy->set(tmp, item->result_field);
          item->result_field->move_field(copy->to_ptr,copy->to_null_ptr,1);
#ifdef HAVE_purify
          copy->to_ptr[copy->from_length]= 0;
#endif
          copy++;
        }
      }
    }
    else if ((real_pos->type() == Item::FUNC_ITEM ||
	      real_pos->type() == Item::SUBSELECT_ITEM ||
	      real_pos->type() == Item::CACHE_ITEM ||
	      real_pos->type() == Item::COND_ITEM) &&
	     !real_pos->with_sum_func)
    {						// Save for send fields
      pos= real_pos;
      /* TODO:
	 In most cases this result will be sent to the user.
	 This should be changed to use copy_int or copy_real depending
	 on how the value is to be used: In some cases this may be an
	 argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      if (!(pos= Item_copy::create(pos)))
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


/**
  Make a copy of all simple SELECT'ed items.

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
  Item_copy *item;
  while ((item = (Item_copy*) it++))
    item->copy();
}


/**
  Make an array of pointers to sum_functions to speed up
  sum_func calculation.

  @retval
    0	ok
  @retval
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
  {
    group_parts+= fields_list.elements;
    /*
      If the ORDER clause is specified then it's possible that
      it also will be optimized, so reserve space for it too
    */
    if (order)
    {
      ORDER *ord;
      for (ord= order; ord; ord= ord->next)
        group_parts++;
    }
  }

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs= (Item_sum**) thd->calloc(sizeof(Item_sum**) * (func_count+1) +
				      sizeof(Item_sum***) * (group_parts+1));
  sum_funcs_end= (Item_sum***) (sum_funcs+func_count+1);
  DBUG_RETURN(sum_funcs == 0);
}


/**
  Initialize 'sum_funcs' array with all Item_sum objects.

  @param field_list        All items
  @param send_result_set_metadata       Items in select list
  @param before_group_by   Set to 1 if this is called before GROUP BY handling
  @param recompute         Set to TRUE if sum_funcs must be recomputed

  @retval
    0  ok
  @retval
    1  error
*/

bool JOIN::make_sum_func_list(List<Item> &field_list, List<Item> &send_result_set_metadata,
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
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
        (!((Item_sum*) item)->depended_from() ||
         ((Item_sum *)item)->depended_from() == select_lex))
      *func++= (Item_sum*) item;
  }
  if (before_group_by && rollup.state == ROLLUP::STATE_INITED)
  {
    rollup.state= ROLLUP::STATE_READY;
    if (rollup_make_fields(field_list, send_result_set_metadata, &func))
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


/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param thd                   THD pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0     ok
  @retval
    !=0   error
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

    if ((item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM) ||
        (item->type() == Item::FUNC_ITEM &&
         ((Item_func*)item)->functype() == Item_func::SUSERVAR_FUNC))
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

        if (item->real_item()->type() != Item::FIELD_ITEM)
          field->orig_table= 0;
	item_field->name= item->name;
        if (item->type() == Item::REF_ITEM)
        {
          Item_field *ifield= (Item_field *) item_field;
          Item_ref *iref= (Item_ref *) item;
          ifield->table_name= iref->table_name;
          ifield->db_name= iref->db_name;
        }
#ifndef DBUG_OFF
	if (!item_field->name)
	{
	  char buff[256];
	  String str(buff,sizeof(buff),&my_charset_bin);
	  str.length(0);
	  item->print(&str, QT_ORDINARY);
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


/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  @param thd                   THD pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0	ok
  @retval
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


/**
  Call ::setup for all sum functions.

  @param thd           thread handler
  @param func_ptr      sum function list

  @retval
    FALSE  ok
  @retval
    TRUE   error
*/

static bool setup_sum_funcs(THD *thd, Item_sum **func_ptr)
{
  Item_sum *func;
  DBUG_ENTER("setup_sum_funcs");
  while ((func= *(func_ptr++)))
  {
    if (func->aggregator_setup(thd))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


static bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct)
{
  Item_sum *func;
  DBUG_ENTER("prepare_sum_aggregators");
  while ((func= *(func_ptr++)))
  {
    if (func->set_aggregator(need_distinct && func->has_with_distinct() ?
                             Aggregator::DISTINCT_AGGREGATOR :
                             Aggregator::SIMPLE_AGGREGATOR))
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


/** Update record 0 in tmp_table from record 1. */

static void
update_tmptable_sum_func(Item_sum **func_ptr,
			 TABLE *tmp_table __attribute__((unused)))
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->update_field();
}


/** Copy result of sum functions to record in tmp_table. */

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
    if ((*func_ptr)->aggregator_add())
      return 1;
  }
  return 0;
}


static bool
update_sum_func(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    if (func->aggregator_add())
      return 1;
  return 0;
}

/** 
  Copy result of functions to record in tmp_table. 

  Uses the thread pointer to check for errors in 
  some of the val_xxx() methods called by the 
  save_in_result_field() function.
  TODO: make the Item::val_xxx() return error code

  @param func_ptr  array of the function Items to copy to the tmp table
  @param thd       pointer to the current thread for error checking
  @retval
    FALSE if OK
  @retval
    TRUE on error  
*/

bool
copy_funcs(Item **func_ptr, const THD *thd)
{
  Item *func;
  for (; (func = *func_ptr) ; func_ptr++)
  {
    func->save_in_result_field(1);
    /*
      Need to check the THD error state because Item::val_xxx() don't
      return error code, but can generate errors
      TODO: change it for a real status check when Item::val_xxx()
      are extended to return status code.
    */  
    if (thd->is_error())
      return TRUE;
  }
  return FALSE;
}

/**
  Create a condition for a const reference for a table.

  @param thd      THD pointer
  @param join_tab pointer to the table

  @return A pointer to the created condition for the const reference.
  @retval !NULL if the condition was created successfully
  @retval NULL if an error has occured
*/

static Item_cond_and *create_cond_for_const_ref(THD *thd, JOIN_TAB *join_tab)
{
  DBUG_ENTER("create_cond_for_const_ref");
  DBUG_ASSERT(join_tab->ref.key_parts);

  TABLE *table= join_tab->table;
  Item_cond_and *cond= new Item_cond_and();
  if (!cond)
    DBUG_RETURN(NULL);

  for (uint i=0 ; i < join_tab->ref.key_parts ; i++)
  {
    Field *field= table->field[table->key_info[join_tab->ref.key].key_part[i].
                               fieldnr-1];
    Item *value= join_tab->ref.items[i];
    cond->add(new Item_func_equal(new Item_field(field), value));
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(NULL);

  if (!cond->fixed)
    cond->fix_fields(thd, (Item**)&cond);

  DBUG_RETURN(cond);
}

/**
  Create a condition for a const reference and add this to the
  currenct select for the table.
*/

static bool add_ref_to_table_cond(THD *thd, JOIN_TAB *join_tab)
{
  DBUG_ENTER("add_ref_to_table_cond");
  if (!join_tab->ref.key_parts)
    DBUG_RETURN(FALSE);

  int error= 0;

  /* Create a condition representing the const reference. */
  Item_cond_and *cond= create_cond_for_const_ref(thd, join_tab);
  if (!cond)
    DBUG_RETURN(TRUE);

  /* Add this condition to the existing select condtion */
  if (join_tab->select)
  {
    if (join_tab->select->cond)
      error=(int) cond->add(join_tab->select->cond);
    join_tab->select->cond= cond;
    join_tab->set_select_cond(cond, __LINE__);
  }
  else if ((join_tab->select= make_select(join_tab->table, 0, 0, cond, 0,
                                          &error)))
    join_tab->set_select_cond(cond, __LINE__);

  /*
    If we have pushed parts of the select condition down to the
    storage engine we also need to add the condition for the const
    reference to the pre_idx_push_select_cond since this might be used
    later (in test_if_skip_sort_order()) instead of the select_cond.
  */
  if (join_tab->pre_idx_push_select_cond)
  {
    cond= create_cond_for_const_ref(thd, join_tab);
    if (!cond)
      DBUG_RETURN(TRUE);
    if (cond->add(join_tab->pre_idx_push_select_cond))
      DBUG_RETURN(TRUE);
    join_tab->pre_idx_push_select_cond = cond;
  }

  DBUG_RETURN(error ? TRUE : FALSE);
}


/**
  Free joins of subselect of this select.

  @param thd      THD pointer
  @param select   pointer to st_select_lex which subselects joins we will free
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
                                        item->name)))
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
  List_iterator<Item> it(all_fields);
  Item *item;
  while ((item= it++))
  {
    ORDER *group_tmp;
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
   Wrap all constant Items in GROUP BY list.

   For ROLLUP queries each constant item referenced in GROUP BY list
   is wrapped up into an Item_func object yielding the same value
   as the constant item. The objects of the wrapper class are never
   considered as constant items and besides they inherit all
   properties of the Item_result_field class.
   This wrapping allows us to ensure writing constant items
   into temporary tables whenever the result of the ROLLUP
   operation has to be written into a temporary table, e.g. when
   ROLLUP is used together with DISTINCT in the SELECT list.
   Usually when creating temporary tables for a intermidiate
   result we do not include fields for constant expressions.

   @retval
     0  if ok
   @retval
     1  on error
*/

bool JOIN::rollup_process_const_fields()
{
  ORDER *group_tmp;
  Item *item;
  List_iterator<Item> it(all_fields);

  for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
  {
    if (!(*group_tmp->item)->const_item())
      continue;
    while ((item= it++))
    {
      if (*group_tmp->item == item)
      {
        Item* new_item= new Item_func_rollup_const(item);
        if (!new_item)
          return 1;
        new_item->fix_fields(thd, (Item **) 0);
        thd->change_item_tree(it.ref(), new_item);
        for (ORDER *tmp= group_tmp; tmp; tmp= tmp->next)
        {
          if (*tmp->item == item)
            thd->change_item_tree(tmp->item, new_item);
        }
        break;
      }
    }
    it.rewind();
  }
  return 0;
}
  

/**
  Fill up rollup structures with pointers to fields to use.

  Creates copies of item_sum items for each sum level.

  @param fields_arg		List of all fields (hidden and real ones)
  @param sel_fields		Pointer to selected fields
  @param func			Store here a pointer to all fields

  @retval
    0	if ok;
    In this case func is pointing to next not used element.
  @retval
    1    on error
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

      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
          (!((Item_sum*) item)->depended_from() ||
           ((Item_sum *)item)->depended_from() == select_lex))
          
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

/**
  Send all rollup levels higher than the current one to the client.

  @b SAMPLE
    @code
      SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx		Level we are on:
                        - 0 = Total sum level
                        - 1 = First group changed  (a)
                        - 2 = Second group changed (a,b)

  @retval
    0   ok
  @retval
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

/**
  Write all rollup levels higher than the current one to a temp table.

  @b SAMPLE
    @code
      SELECT a, b, SUM(c) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx                 Level we are on:
                               - 0 = Total sum level
                               - 1 = First group changed  (a)
                               - 2 = Second group changed (a,b)
  @param table               reference to temp table

  @retval
    0   ok
  @retval
    1   if write_data_failed()
*/

int JOIN::rollup_write_data(uint idx, TABLE *table_arg)
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
      int write_error;
      Item *item;
      List_iterator_fast<Item> it(rollup.fields[i]);
      while ((item= it++))
      {
        if (item->type() == Item::NULL_ITEM && item->is_result_field())
          item->save_in_result_field(1);
      }
      copy_sum_funcs(sum_funcs_end[i+1], sum_funcs_end[i]);
      if ((write_error= table_arg->file->ha_write_row(table_arg->record[0])))
      {
	if (create_myisam_from_heap(thd, table_arg, 
                                    tmp_table_param.start_recinfo,
                                    &tmp_table_param.recinfo,
                                    write_error, FALSE, NULL))
	  return 1;		     
      }
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_pointer_array);
  return 0;
}

/**
  clear results if there are not rows found for group
  (end_send_group/end_write_group)
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

/**
  EXPLAIN handling.

  Send a description about what how the select will be done to stdout.
*/

void select_describe(JOIN *join, bool need_tmp_table, bool need_order,
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

  /* 
    NOTE: the number/types of items pushed into item_list must be in sync with
    EXPLAIN column types as they're "defined" in THD::send_explain_fields()
  */
  if (message)
  {
    item_list.push_back(new Item_int((int32)
				     join->select_lex->select_number));
    item_list.push_back(new Item_string(join->select_lex->type,
					strlen(join->select_lex->type), cs));
    for (uint i=0 ; i < 7; i++)
      item_list.push_back(item_null);
    if (join->thd->lex->describe & DESCRIBE_PARTITIONS)
      item_list.push_back(item_null);
    if (join->thd->lex->describe & DESCRIBE_EXTENDED)
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
    char table_name_buffer[NAME_CHAR_LEN];
    item_list.empty();
    /* id */
    item_list.push_back(new Item_null);
    /* select_type */
    item_list.push_back(new Item_string(join->select_lex->type,
					strlen(join->select_lex->type),
					cs));
    /* table */
    {
      SELECT_LEX *last_select= join->unit->first_select()->last_select();
      // # characters needed to print select_number of last select
      int last_length= (int)log10((double)last_select->select_number)+1;

      SELECT_LEX *sl= join->unit->first_select();
      uint len= 6, lastop= 0;
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
      item_list.push_back(new Item_string(table_name_buffer, len, cs));
    }
    /* partitions */
    if (join->thd->lex->describe & DESCRIBE_PARTITIONS)
      item_list.push_back(item_null);
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
    /* in_rows */
    if (join->thd->lex->describe & DESCRIBE_EXTENDED)
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
    uint last_sjm_table= MAX_TABLES;
    for (uint i=0 ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      TABLE *table=tab->table;
      TABLE_LIST *table_list= tab->table->pos_in_table_list;
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
      {
        TABLE_LIST *real_table= table->pos_in_table_list; 
	item_list.push_back(new Item_string(real_table->alias,
					    strlen(real_table->alias),
					    cs));
      }
      /* "partitions" column */
      if (join->thd->lex->describe & DESCRIBE_PARTITIONS)
      {
#ifdef WITH_PARTITION_STORAGE_ENGINE
        partition_info *part_info;
        if (!table->derived_select_number && 
            (part_info= table->part_info))
        {          
          Item_string *item_str= new Item_string(cs);
          make_used_partitions_str(part_info, &item_str->str_value);
          item_list.push_back(item_str);
        }
        else
          item_list.push_back(item_null);
#else
        /* just produce empty column if partitioning is not compiled in */
        item_list.push_back(item_null); 
#endif
      }
      /* "type" column */
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
        if (table_list->schema_table &&
            table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
        {
          const char *tmp_buff;
          int f_idx;
          if (table_list->has_db_lookup_value)
          {
            f_idx= table_list->schema_table->idx_field1;
            tmp_buff= table_list->schema_table->fields_info[f_idx].field_name;
            tmp2.append(tmp_buff, strlen(tmp_buff), cs);
          }          
          if (table_list->has_table_lookup_value)
          {
            if (table_list->has_db_lookup_value)
              tmp2.append(',');
            f_idx= table_list->schema_table->idx_field2;
            tmp_buff= table_list->schema_table->fields_info[f_idx].field_name;
            tmp2.append(tmp_buff, strlen(tmp_buff), cs);
          }
          if (tmp2.length())
            item_list.push_back(new Item_string(tmp2.ptr(),tmp2.length(),cs));
          else
            item_list.push_back(item_null);
        }
        else
          item_list.push_back(item_null);
	item_list.push_back(item_null);
	item_list.push_back(item_null);
      }
      
      /* Add "rows" field to item_list. */
      if (table_list->schema_table)
      {
        /* in_rows */
        if (join->thd->lex->describe & DESCRIBE_EXTENDED)
          item_list.push_back(item_null);
        /* rows */
        item_list.push_back(item_null);
      }
      else
      {
        double examined_rows;
        if (tab->select && tab->select->quick)
          examined_rows= rows2double(tab->select->quick->records);
        else if (tab->type == JT_NEXT || tab->type == JT_ALL)
        {
          if (tab->limit)
            examined_rows= rows2double(tab->limit);
          else
          {
            tab->table->file->info(HA_STATUS_VARIABLE);
            examined_rows= rows2double(tab->table->file->stats.records);
          }
        }
        else
          examined_rows= join->best_positions[i].records_read; 
 
        item_list.push_back(new Item_int((longlong) (ulonglong) examined_rows, 
                                         MY_INT64_NUM_DECIMAL_DIGITS));

        /* Add "filtered" field to item_list. */
        if (join->thd->lex->describe & DESCRIBE_EXTENDED)
        {
          float f= 0.0; 
          if (examined_rows)
            f= (float) (100.0 * join->best_positions[i].records_read /
                        examined_rows);
          item_list.push_back(new Item_float(f, 2));
        }
      }

      /* Build "Extra" field and add it to item_list. */
      my_bool key_read=table->key_read;
      if ((tab->type == JT_NEXT || tab->type == JT_CONST) &&
          table->covering_keys.is_set(tab->index))
	key_read=1;
      if (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT &&
          !((QUICK_ROR_INTERSECT_SELECT*)tab->select->quick)->need_to_fetch_row)
        key_read=1;
        
      if (tab->info)
	item_list.push_back(new Item_string(tab->info,strlen(tab->info),cs));
      else if (tab->packed_info & TAB_INFO_HAVE_VALUE)
      {
        if (tab->packed_info & TAB_INFO_USING_INDEX)
          extra.append(STRING_WITH_LEN("; Using index"));
        if (tab->packed_info & TAB_INFO_USING_WHERE)
          extra.append(STRING_WITH_LEN("; Using where"));
        if (tab->packed_info & TAB_INFO_FULL_SCAN_ON_NULL)
          extra.append(STRING_WITH_LEN("; Full scan on NULL key"));
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
      else
      {
        uint keyno= MAX_KEY;
        if (tab->ref.key_parts)
          keyno= tab->ref.key;
        else if (tab->select && tab->select->quick)
          keyno = tab->select->quick->index;

        if ((keyno != MAX_KEY && keyno == table->file->pushed_idx_cond_keyno &&
             table->file->pushed_idx_cond) || tab->cache_idx_cond)
          extra.append(STRING_WITH_LEN("; Using index condition"));

        if (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION || 
            quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT ||
            quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE)
        {
          extra.append(STRING_WITH_LEN("; Using "));
          tab->select->quick->add_info_string(&extra);
        }
        if (tab->select)
	{
	  if (tab->use_quick == QS_DYNAMIC_RANGE)
	  {
            /* 4 bits per 1 hex digit + terminating '\0' */
            char buf[MAX_KEY / 4 + 1];
            extra.append(STRING_WITH_LEN("; Range checked for each "
                                         "record (index map: 0x"));
            extra.append(tab->keys.print(buf));
            extra.append(')');
	  }
	  else if (tab->select->cond)
          {
            const Item *pushed_cond= tab->table->file->pushed_cond;

            if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
                pushed_cond)
            {
              extra.append(STRING_WITH_LEN("; Using where with pushed "
                                           "condition"));
              if (thd->lex->describe & DESCRIBE_EXTENDED)
              {
                extra.append(STRING_WITH_LEN(": "));
                ((Item *)pushed_cond)->print(&extra, QT_ORDINARY);
              }
            }
            else
              extra.append(STRING_WITH_LEN("; Using where"));
          }
	}
        if (table_list->schema_table &&
            table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
        {
          if (!table_list->table_open_method)
            extra.append(STRING_WITH_LEN("; Skip_open_table"));
          else if (table_list->table_open_method == OPEN_FRM_ONLY)
            extra.append(STRING_WITH_LEN("; Open_frm_only"));
          else
            extra.append(STRING_WITH_LEN("; Open_full_table"));
          if (table_list->has_db_lookup_value &&
              table_list->has_table_lookup_value)
            extra.append(STRING_WITH_LEN("; Scanned 0 databases"));
          else if (table_list->has_db_lookup_value ||
                   table_list->has_table_lookup_value)
            extra.append(STRING_WITH_LEN("; Scanned 1 database"));
          else
            extra.append(STRING_WITH_LEN("; Scanned all databases"));
        }
        if (key_read)
        {
          if (quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
          {
            QUICK_GROUP_MIN_MAX_SELECT *qgs= 
              (QUICK_GROUP_MIN_MAX_SELECT *) tab->select->quick;
            extra.append(STRING_WITH_LEN("; Using index for group-by"));
            qgs->append_loose_scan_type(&extra);
          }
          else
            extra.append(STRING_WITH_LEN("; Using index"));
        }
        if (table->reginfo.not_exists_optimize)
          extra.append(STRING_WITH_LEN("; Not exists"));

        if (quick_type == QUICK_SELECT_I::QS_TYPE_RANGE &&
            !(((QUICK_RANGE_SELECT*)(tab->select->quick))->mrr_flags &
             HA_MRR_USE_DEFAULT_IMPL))
        {
	  extra.append(STRING_WITH_LEN("; Using MRR"));
        }
        if (need_tmp_table)
        {
          need_tmp_table=0;
          extra.append(STRING_WITH_LEN("; Using temporary"));
        }
        if (need_order)
        {
          need_order=0;
          extra.append(STRING_WITH_LEN("; Using filesort"));
        }
        if (distinct & test_all_bits(used_tables,thd->used_tables))
          extra.append(STRING_WITH_LEN("; Distinct"));

        if (tab->loosescan_match_tab)
        {
          extra.append(STRING_WITH_LEN("; LooseScan"));
        }

        if (tab->flush_weedout_table)
          extra.append(STRING_WITH_LEN("; Start temporary"));
        if (tab->check_weed_out_table)
          extra.append(STRING_WITH_LEN("; End temporary"));
        else if (tab->do_firstmatch)
        {
          if (tab->do_firstmatch == join->join_tab - 1)
            extra.append(STRING_WITH_LEN("; FirstMatch"));
          else
          {
            extra.append(STRING_WITH_LEN("; FirstMatch("));
            TABLE *prev_table=tab->do_firstmatch->table;
            if (prev_table->derived_select_number)
            {
              char namebuf[NAME_LEN];
              /* Derived table name generation */
              int len= my_snprintf(namebuf, sizeof(namebuf)-1,
                                   "<derived%u>",
                                   prev_table->derived_select_number);
              extra.append(namebuf, len);
            }
            else
              extra.append(prev_table->pos_in_table_list->alias);
            extra.append(STRING_WITH_LEN(")"));
          }
        }
        uint sj_strategy= join->best_positions[i].sj_strategy;
        if (sj_is_materialize_strategy(sj_strategy))
        {
          if (join->best_positions[i].n_sj_tables == 1)
            extra.append(STRING_WITH_LEN("; Materialize"));
          else
          {
            last_sjm_table= i + join->best_positions[i].n_sj_tables - 1;
            extra.append(STRING_WITH_LEN("; Start materialize"));
          }
          if (sj_strategy == SJ_OPT_MATERIALIZE_SCAN)
              extra.append(STRING_WITH_LEN("; Scan"));
        }
        else if (last_sjm_table == i)
        {
          extra.append(STRING_WITH_LEN("; End materialize"));
        }

        for (uint part= 0; part < tab->ref.key_parts; part++)
        {
          if (tab->ref.cond_guards[part])
          {
            extra.append(STRING_WITH_LEN("; Full scan on NULL key"));
            break;
          }
        }

        if (i > 0 && tab[-1].next_select == sub_select_cache)
        {
          extra.append(STRING_WITH_LEN("; Using join buffer ("));
          if ((tab->use_join_cache & JOIN_CACHE::ALG_BNL))
            extra.append(STRING_WITH_LEN("BNL"));
          else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA))
            extra.append(STRING_WITH_LEN("BKA"));
          else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA_UNIQUE))
            extra.append(STRING_WITH_LEN("BKA_UNIQUE"));
          else
            DBUG_ASSERT(0);
          if (tab->use_join_cache & JOIN_CACHE::NON_INCREMENTAL_BUFFER)
            extra.append(STRING_WITH_LEN(", regular buffers)"));
          else
            extra.append(STRING_WITH_LEN(", incremental buffers)"));
        }

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
	       (sl->first_inner_unit() || sl->next_select() ? 
		"PRIMARY" : "SIMPLE"):
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
  if (unit->is_union())
  {
    unit->fake_select_lex->select_number= UINT_MAX; // jost for initialization
    unit->fake_select_lex->type= "UNION RESULT";
    unit->fake_select_lex->options|= SELECT_DESCRIBE;
    if (!(res= unit->prepare(thd, result, SELECT_NO_UNLOCK | SELECT_DESCRIBE)))
      res= unit->exec();
    res|= unit->cleanup();
  }
  else
  {
    thd->lex->current_select= first;
    unit->set_limit(unit->global_parameters);
    res= mysql_select(thd, &first->ref_pointer_array,
			first->table_list.first,
			first->with_wild, first->item_list,
			first->where,
			first->order_list.elements +
			first->group_list.elements,
			first->order_list.first,
			first->group_list.first,
			first->having,
			thd->lex->proc_list.first,
			first->options | thd->variables.option_bits | SELECT_DESCRIBE,
			result, unit, first);
  }
  DBUG_RETURN(res || thd->is_error());
}


static void print_table_array(THD *thd, String *str, TABLE_LIST **table, 
                              TABLE_LIST **end, enum_query_type query_type)
{
  (*table)->print(thd, str, query_type);

  for (TABLE_LIST **tbl= table + 1; tbl < end; tbl++)
  {
    TABLE_LIST *curr= *tbl;
    if (curr->outer_join)
    {
      /* MySQL converts right to left joins */
      str->append(STRING_WITH_LEN(" left join "));
    }
    else if (curr->straight)
      str->append(STRING_WITH_LEN(" straight_join "));
    else if (curr->sj_inner_tables)
      str->append(STRING_WITH_LEN(" semi join "));
    else
      str->append(STRING_WITH_LEN(" join "));
    curr->print(thd, str, query_type);
    if (curr->on_expr)
    {
      str->append(STRING_WITH_LEN(" on("));
      curr->on_expr->print(str, query_type);
      str->append(')');
    }
  }
}


/**
  Print joins from the FROM clause.

  @param thd     thread handler
  @param str     string where table should be printed
  @param tables  list of tables in join
  @query_type    type of the query is being generated
*/

static void print_join(THD *thd,
                       String *str,
                       List<TABLE_LIST> *tables,
                       enum_query_type query_type)
{
  /* List is reversed => we should reverse it before using */
  List_iterator_fast<TABLE_LIST> ti(*tables);
  TABLE_LIST **table;
  uint non_const_tables= 0;

  for (TABLE_LIST *t= ti++; t ; t= ti++)
    if (!t->optimized_away)
      non_const_tables++;
  if (!non_const_tables)
  {
    str->append(STRING_WITH_LEN("dual"));
    return; // all tables were optimized away
  }
  ti.rewind();

  if (!(table= (TABLE_LIST **)thd->alloc(sizeof(TABLE_LIST*) *
                                                non_const_tables)))
    return;  // out of memory

  TABLE_LIST *tmp, **t= table + (non_const_tables - 1);
  while ((tmp= ti++))
  {
    if (tmp->optimized_away)
      continue;
    *t--= tmp;
  }

  /* 
    If the first table is a semi-join nest, swap it with something that is
    not a semi-join nest.
  */
  if ((*table)->sj_inner_tables)
  {
    TABLE_LIST **end= table + non_const_tables;
    for (TABLE_LIST **t2= table; t2!=end; t2++)
    {
      if (!(*t2)->sj_inner_tables)
      {
        TABLE_LIST *tmp= *t2;
        *t2= *table;
        *table= tmp;
        break;
      }
    }
  }
  DBUG_ASSERT(non_const_tables >= 1);
  print_table_array(thd, str, table, table + non_const_tables, query_type);
}


/**
  @brief Print an index hint

  @details Prints out the USE|FORCE|IGNORE index hint.

  @param      thd         the current thread
  @param[out] str         appends the index hint here
  @param      hint        what the hint is (as string : "USE INDEX"|
                          "FORCE INDEX"|"IGNORE INDEX")
  @param      hint_length the length of the string in 'hint'
  @param      indexes     a list of index names for the hint
*/

void 
Index_hint::print(THD *thd, String *str)
{
  switch (type)
  {
    case INDEX_HINT_IGNORE: str->append(STRING_WITH_LEN("IGNORE INDEX")); break;
    case INDEX_HINT_USE:    str->append(STRING_WITH_LEN("USE INDEX")); break;
    case INDEX_HINT_FORCE:  str->append(STRING_WITH_LEN("FORCE INDEX")); break;
  }
  str->append (STRING_WITH_LEN(" ("));
  if (key_name.length)
  {
    if (thd && !my_strnncoll(system_charset_info,
                             (const uchar *)key_name.str, key_name.length, 
                             (const uchar *)primary_key_name, 
                             strlen(primary_key_name)))
      str->append(primary_key_name);
    else
      append_identifier(thd, str, key_name.str, key_name.length);
  }
  str->append(')');
}


/**
  Print table as it should be in join list.

  @param str   string where table should be printed
*/

void TABLE_LIST::print(THD *thd, String *str, enum_query_type query_type)
{
  if (nested_join)
  {
    str->append('(');
    print_join(thd, str, &nested_join->join_list, query_type);
    str->append(')');
  }
  else
  {
    const char *cmp_name;                         // Name to compare with alias
    if (view_name.str)
    {
      // A view

      if (!(belong_to_view &&
            belong_to_view->compact_view_format))
      {
        append_identifier(thd, str, view_db.str, view_db.length);
        str->append('.');
      }
      append_identifier(thd, str, view_name.str, view_name.length);
      cmp_name= view_name.str;
    }
    else if (derived)
    {
      // A derived table
      str->append('(');
      derived->print(str, query_type);
      str->append(')');
      cmp_name= "";                               // Force printing of alias
    }
    else
    {
      // A normal table

      if (!(belong_to_view &&
            belong_to_view->compact_view_format))
      {
        append_identifier(thd, str, db, db_length);
        str->append('.');
      }
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
      char t_alias_buff[MAX_ALIAS_NAME];
      const char *t_alias= alias;

      str->append(' ');
      if (lower_case_table_names== 1)
      {
        if (alias && alias[0])
        {
          strmov(t_alias_buff, alias);
          my_casedn_str(files_charset_info, t_alias_buff);
          t_alias= t_alias_buff;
        }
      }

      append_identifier(thd, str, t_alias, strlen(t_alias));
    }

    if (index_hints)
    {
      List_iterator<Index_hint> it(*index_hints);
      Index_hint *hint;

      while ((hint= it++))
      {
        str->append (STRING_WITH_LEN(" "));
        hint->print (thd, str);
      }
    }
  }
}


void st_select_lex::print(THD *thd, String *str, enum_query_type query_type)
{
  /* QQ: thd may not be set for sub queries, but this should be fixed */
  if (!thd)
    thd= current_thd;

  str->append(STRING_WITH_LEN("select "));

  /* First add options */
  if (options & SELECT_STRAIGHT_JOIN)
    str->append(STRING_WITH_LEN("straight_join "));
  if (options & SELECT_HIGH_PRIORITY)
    str->append(STRING_WITH_LEN("high_priority "));
  if (options & SELECT_DISTINCT)
    str->append(STRING_WITH_LEN("distinct "));
  if (options & SELECT_SMALL_RESULT)
    str->append(STRING_WITH_LEN("sql_small_result "));
  if (options & SELECT_BIG_RESULT)
    str->append(STRING_WITH_LEN("sql_big_result "));
  if (options & OPTION_BUFFER_RESULT)
    str->append(STRING_WITH_LEN("sql_buffer_result "));
  if (options & OPTION_FOUND_ROWS)
    str->append(STRING_WITH_LEN("sql_calc_found_rows "));
  switch (sql_cache)
  {
    case SQL_NO_CACHE:
      str->append(STRING_WITH_LEN("sql_no_cache "));
      break;
    case SQL_CACHE:
      str->append(STRING_WITH_LEN("sql_cache "));
      break;
    case SQL_CACHE_UNSPECIFIED:
      break;
    default:
      DBUG_ASSERT(0);
  }

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

    if (master_unit()->item && item->is_autogenerated_name)
    {
      /*
        Do not print auto-generated aliases in subqueries. It has no purpose
        in a view definition or other contexts where the query is printed.
      */
      item->print(str, query_type);
    }
    else
      item->print_item_w_name(str, query_type);
  }

  /*
    from clause
    TODO: support USING/FORCE/IGNORE index
  */
  if (table_list.elements)
  {
    str->append(STRING_WITH_LEN(" from "));
    /* go through join tree */
    print_join(thd, str, &top_join_list, query_type);
  }
  else if (where)
  {
    /*
      "SELECT 1 FROM DUAL WHERE 2" should not be printed as 
      "SELECT 1 WHERE 2": the 1st syntax is valid, but the 2nd is not.
    */
    str->append(STRING_WITH_LEN(" from DUAL "));
  }

  // Where
  Item *cur_where= where;
  if (join)
    cur_where= join->conds;
  if (cur_where || cond_value != Item::COND_UNDEF)
  {
    str->append(STRING_WITH_LEN(" where "));
    if (cur_where)
      cur_where->print(str, query_type);
    else
      str->append(cond_value != Item::COND_FALSE ? "1" : "0");
  }

  // group by & olap
  if (group_list.elements)
  {
    str->append(STRING_WITH_LEN(" group by "));
    print_order(str, group_list.first, query_type);
    switch (olap)
    {
      case CUBE_TYPE:
	str->append(STRING_WITH_LEN(" with cube"));
	break;
      case ROLLUP_TYPE:
	str->append(STRING_WITH_LEN(" with rollup"));
	break;
      default:
	;  //satisfy compiler
    }
  }

  // having
  Item *cur_having= having;
  if (join)
    cur_having= join->having;

  if (cur_having || having_value != Item::COND_UNDEF)
  {
    str->append(STRING_WITH_LEN(" having "));
    if (cur_having)
      cur_having->print(str, query_type);
    else
      str->append(having_value != Item::COND_FALSE ? "1" : "0");
  }

  if (order_list.elements)
  {
    str->append(STRING_WITH_LEN(" order by "));
    print_order(str, order_list.first, query_type);
  }

  // limit
  print_limit(thd, str, query_type);

  // PROCEDURE unsupported here
}


/**
  change select_result object of JOIN.

  @param res		new select_result object

  @retval
    FALSE   OK
  @retval
    TRUE    error
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

/**
  Cache constant expressions in WHERE, HAVING, ON conditions.
*/

void JOIN::cache_const_exprs()
{
  bool cache_flag= FALSE;
  bool *analyzer_arg= &cache_flag;

  /* No need in cache if all tables are constant. */
  if (const_tables == tables)
    return;

  if (conds)
    conds->compile(&Item::cache_const_expr_analyzer, (uchar **)&analyzer_arg,
                  &Item::cache_const_expr_transformer, (uchar *)&cache_flag);
  cache_flag= FALSE;
  if (having)
    having->compile(&Item::cache_const_expr_analyzer, (uchar **)&analyzer_arg,
                    &Item::cache_const_expr_transformer, (uchar *)&cache_flag);

  for (JOIN_TAB *tab= join_tab + const_tables; tab < join_tab + tables ; tab++)
  {
    if (*tab->on_expr_ref)
    {
      cache_flag= FALSE;
      (*tab->on_expr_ref)->compile(&Item::cache_const_expr_analyzer,
                                 (uchar **)&analyzer_arg,
                                 &Item::cache_const_expr_transformer,
                                 (uchar *)&cache_flag);
    }
  }
}


/**
  Find a cheaper access key than a given @a key

  @param          tab                 NULL or JOIN_TAB of the accessed table
  @param          order               Linked list of ORDER BY arguments
  @param          table               Table if tab == NULL or tab->table
  @param          usable_keys         Key map to find a cheaper key in
  @param          ref_key             
                * 0 <= key < MAX_KEY   - key number (hint) to start the search
                * -1                   - no key number provided
  @param          select_limit        LIMIT value
  @param [out]    new_key             Key number if success, otherwise undefined
  @param [out]    new_key_direction   Return -1 (reverse) or +1 if success,
                                      otherwise undefined
  @param [out]    new_select_limit    Return adjusted LIMIT
  @param [out]    new_used_key_parts  NULL by default, otherwise return number
                                      of new_key prefix columns if success
                                      or undefined if the function fails
  @param [out]  saved_best_key_parts  NULL by default, otherwise preserve the
                                      value for further use in QUICK_SELECT_DESC

  @note
    This function takes into account table->quick_condition_rows statistic
    (that is calculated by the make_join_statistics function).
    However, single table procedures such as mysql_update() and mysql_delete()
    never call make_join_statistics, so they have to update it manually
    (@see get_index_for_order()).
*/

static bool
test_if_cheaper_ordering(const JOIN_TAB *tab, ORDER *order, TABLE *table,
                         key_map usable_keys,  int ref_key,
                         ha_rows select_limit,
                         int *new_key, int *new_key_direction,
                         ha_rows *new_select_limit, uint *new_used_key_parts,
                         uint *saved_best_key_parts)
{
  DBUG_ENTER("test_if_cheaper_ordering");
  /*
    Check whether there is an index compatible with the given order
    usage of which is cheaper than usage of the ref_key index (ref_key>=0)
    or a table scan.
    It may be the case if ORDER/GROUP BY is used with LIMIT.
  */
  ha_rows best_select_limit= HA_POS_ERROR;
  JOIN *join= tab ? tab->join : NULL;
  uint nr;
  key_map keys;
  uint best_key_parts= 0;
  int best_key_direction= 0;
  ha_rows best_records= 0;
  double read_time;
  int best_key= -1;
  bool is_best_covering= FALSE;
  double fanout= 1;
  ha_rows table_records= table->file->stats.records;
  bool group= join && join->group && order == join->group_list;
  ha_rows ref_key_quick_rows= HA_POS_ERROR;

  /*
    If not used with LIMIT, only use keys if the whole query can be
    resolved with a key;  This is because filesort() is usually faster than
    retrieving all rows through an index.
  */
  if (select_limit >= table_records)
  {
    keys= *table->file->keys_to_use_for_scanning();
    keys.merge(table->covering_keys);

    /*
      We are adding here also the index specified in FORCE INDEX clause, 
      if any.
      This is to allow users to use index in ORDER BY.
    */
    if (table->force_index) 
      keys.merge(group ? table->keys_in_use_for_group_by :
                         table->keys_in_use_for_order_by);
    keys.intersect(usable_keys);
  }
  else
    keys= usable_keys;

  if (ref_key >= 0 && table->covering_keys.is_set(ref_key))
    ref_key_quick_rows= table->quick_rows[ref_key];

  if (join)
  {
    uint tablenr= tab - join->join_tab;
    read_time= join->best_positions[tablenr].read_time;
    for (uint i= tablenr+1; i < join->tables; i++)
      fanout*= join->best_positions[i].records_read; // fanout is always >= 1
  }
  else
    read_time= table->file->scan_time();

  for (nr=0; nr < table->s->keys ; nr++)
  {
    int direction;
    uint used_key_parts;

    if (keys.is_set(nr) &&
        (direction= test_if_order_by_key(order, table, nr, &used_key_parts)))
    {
      /*
        At this point we are sure that ref_key is a non-ordering
        key (where "ordering key" is a key that will return rows
        in the order required by ORDER BY).
      */
      DBUG_ASSERT (ref_key != (int) nr);

      bool is_covering= table->covering_keys.is_set(nr) ||
                        (nr == table->s->primary_key &&
                        table->file->primary_key_is_clustered());
      
      /* 
        Don't use an index scan with ORDER BY without limit.
        For GROUP BY without limit always use index scan
        if there is a suitable index. 
        Why we hold to this asymmetry hardly can be explained
        rationally. It's easy to demonstrate that using
        temporary table + filesort could be cheaper for grouping
        queries too.
      */ 
      if (is_covering ||
          select_limit != HA_POS_ERROR || 
          (ref_key < 0 && (group || table->force_index)))
      { 
        double rec_per_key;
        double index_scan_time;
        KEY *keyinfo= table->key_info+nr;
        if (select_limit == HA_POS_ERROR)
          select_limit= table_records;
        if (group)
        {
          /* 
            Used_key_parts can be larger than keyinfo->key_parts
            when using a secondary index clustered with a primary 
            key (e.g. as in Innodb). 
            See Bug #28591 for details.
          */  
          rec_per_key= used_key_parts &&
                       used_key_parts <= keyinfo->key_parts ?
                       keyinfo->rec_per_key[used_key_parts-1] : 1;
          set_if_bigger(rec_per_key, 1);
          /*
            With a grouping query each group containing on average
            rec_per_key records produces only one row that will
            be included into the result set.
          */  
          if (select_limit > table_records/rec_per_key)
              select_limit= table_records;
          else
            select_limit= (ha_rows) (select_limit*rec_per_key);
        }
        /* 
          If tab=tk is not the last joined table tn then to get first
          L records from the result set we can expect to retrieve
          only L/fanout(tk,tn) where fanout(tk,tn) says how many
          rows in the record set on average will match each row tk.
          Usually our estimates for fanouts are too pessimistic.
          So the estimate for L/fanout(tk,tn) will be too optimistic
          and as result we'll choose an index scan when using ref/range
          access + filesort will be cheaper.
        */
        select_limit= (ha_rows) (select_limit < fanout ?
                                 1 : select_limit/fanout);
        /*
          We assume that each of the tested indexes is not correlated
          with ref_key. Thus, to select first N records we have to scan
          N/selectivity(ref_key) index entries. 
          selectivity(ref_key) = #scanned_records/#table_records =
          table->quick_condition_rows/table_records.
          In any case we can't select more than #table_records.
          N/(table->quick_condition_rows/table_records) > table_records 
          <=> N > table->quick_condition_rows.
        */ 
        if (select_limit > table->quick_condition_rows)
          select_limit= table_records;
        else
          select_limit= (ha_rows) (select_limit *
                                   (double) table_records /
                                    table->quick_condition_rows);
        rec_per_key= keyinfo->rec_per_key[keyinfo->key_parts-1];
        set_if_bigger(rec_per_key, 1);
        /*
          Here we take into account the fact that rows are
          accessed in sequences rec_per_key records in each.
          Rows in such a sequence are supposed to be ordered
          by rowid/primary key. When reading the data
          in a sequence we'll touch not more pages than the
          table file contains.
          TODO. Use the formula for a disk sweep sequential access
          to calculate the cost of accessing data rows for one 
          index entry.
        */
        index_scan_time= select_limit/rec_per_key *
                         min(rec_per_key, table->file->scan_time());
        if ((ref_key < 0 && is_covering) || 
            (ref_key < 0 && (group || table->force_index)) ||
            index_scan_time < read_time)
        {
          ha_rows quick_records= table_records;
          if ((is_best_covering && !is_covering) ||
              (is_covering && ref_key_quick_rows < select_limit))
            continue;
          if (table->quick_keys.is_set(nr))
            quick_records= table->quick_rows[nr];
          if (best_key < 0 ||
              (select_limit <= min(quick_records,best_records) ?
               keyinfo->key_parts < best_key_parts :
               quick_records < best_records))
          {
            best_key= nr;
            best_key_parts= keyinfo->key_parts;
            if (saved_best_key_parts)
              *saved_best_key_parts= used_key_parts;
            best_records= quick_records;
            is_best_covering= is_covering;
            best_key_direction= direction; 
            best_select_limit= select_limit;
          }
        }   
      }      
    }
  }

  if (best_key < 0 || best_key == ref_key)
    DBUG_RETURN(FALSE);
  
  *new_key= best_key;
  *new_key_direction= best_key_direction;
  *new_select_limit= best_select_limit;
  if (new_used_key_parts != NULL)
    *new_used_key_parts= best_key_parts;

  DBUG_RETURN(TRUE);
}


/**
  Find a key to apply single table UPDATE/DELETE by a given ORDER

  @param       order           Linked list of ORDER BY arguments
  @param       table           Table to find a key
  @param       select          Pointer to access/update select->quick (if any)
  @param       limit           LIMIT clause parameter 
  @param [out] need_sort       TRUE if filesort needed
  @param [out] reverse
    TRUE if the key is reversed again given ORDER (undefined if key == MAX_KEY)

  @return
    - MAX_KEY if no key found                        (need_sort == TRUE)
    - MAX_KEY if quick select result order is OK     (need_sort == FALSE)
    - key number (either index scan or quick select) (need_sort == FALSE)

  @note
    Side effects:
    - may deallocate or deallocate and replace select->quick;
    - may set table->quick_condition_rows and table->quick_rows[...]
      to table->file->stats.records. 
*/

uint get_index_for_order(ORDER *order, TABLE *table, SQL_SELECT *select,
                         ha_rows limit, bool *need_sort, bool *reverse)
{
  if (select && select->quick && select->quick->unique_key_range())
  { // Single row select (always "ordered"): Ok to use with key field UPDATE
    *need_sort= FALSE;
    /*
      Returning of MAX_KEY here prevents updating of used_key_is_modified
      in mysql_update(). Use quick select "as is".
    */
    return MAX_KEY;
  }

  if (!order)
  {
    *need_sort= FALSE;
    if (select && select->quick)
      return select->quick->index; // index or MAX_KEY, use quick select as is
    else
      return table->file->key_used_on_scan; // MAX_KEY or index for some engines
  }

  if (!is_simple_order(order)) // just to cut further expensive checks
  {
    *need_sort= TRUE;
    return MAX_KEY;
  }

  if (select && select->quick)
  {
    if (select->quick->index == MAX_KEY)
    {
      *need_sort= TRUE;
      return MAX_KEY;
    }

    uint used_key_parts;
    switch (test_if_order_by_key(order, table, select->quick->index,
                                 &used_key_parts)) {
    case 1: // desired order
      *need_sort= FALSE;
      return select->quick->index;
    case 0: // unacceptable order
      *need_sort= TRUE;
      return MAX_KEY;
    case -1: // desired order, but opposite direction
      {
        QUICK_SELECT_I *reverse_quick;
        if ((reverse_quick=
               select->quick->make_reverse(used_key_parts)))
        {
          select->set_quick(reverse_quick);
          *need_sort= FALSE;
          return select->quick->index;
        }
        else
        {
          *need_sort= TRUE;
          return MAX_KEY;
        }
      }
    }
    DBUG_ASSERT(0);
  }
  else if (limit != HA_POS_ERROR)
  { // check if some index scan & LIMIT is more efficient than filesort
    
    /*
      Update quick_condition_rows since single table UPDATE/DELETE procedures
      don't call make_join_statistics() and leave this variable uninitialized.
    */
    table->quick_condition_rows= table->file->stats.records;
    
    int key, direction;
    if (test_if_cheaper_ordering(NULL, order, table,
                                 table->keys_in_use_for_order_by, -1,
                                 limit,
                                 &key, &direction, &limit) &&
        !is_key_used(table, key, table->write_set))
    {
      *need_sort= FALSE;
      *reverse= (direction < 0);
      return key;
    }
  }
  *need_sort= TRUE;
  return MAX_KEY;
}


/**
  @} (end of group Query_Optimizer)
*/
