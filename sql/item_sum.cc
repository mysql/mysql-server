/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
  Sum functions (COUNT, MIN...)
*/

#include "item_sum.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>

#include "aggregate_check.h"               // Distinct_check
#include "current_thd.h"                   // current_thd
#include "decimal.h"
#include "derror.h"                        // ER_THD
#include "field.h"
#include "handler.h"
#include "item_cmpfunc.h"
#include "item_func.h"
#include "item_json_func.h"
#include "item_subselect.h"
#include "json_dom.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_double2ulonglong.h"
#include "my_sys.h"
#include "mysql/psi/mysql_statement.h"
#include "mysqld.h"
#include "mysqld_error.h"
#include "parse_tree_helpers.h"            // PT_item_list
#include "parse_tree_nodes.h"              // PT_order_list
#include "sql_array.h"
#include "sql_class.h"                     // THD
#include "sql_error.h"
#include "sql_exception_handler.h"         // handle_std_exception
#include "sql_executor.h"                  // copy_fields
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_resolver.h"                  // setup_order
#include "sql_security_ctx.h"
#include "sql_select.h"                    // count_field_types
#include "sql_tmp_table.h"                 // create_tmp_table
#include "temp_table_param.h"              // Temp_table_param
#include "thr_malloc.h"
#include "uniques.h"                       // Unique

using std::min;
using std::max;

bool Item_sum::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  mark_as_sum_func(pc->select);
  pc->select->in_sum_expr++;
  for (uint i= 0; i < arg_count; i++)
  {
    if (args[i]->itemize(pc, &args[i]))
      return true;
  }
  pc->select->in_sum_expr--;
  return false;
}


/**
  Calculate the affordable RAM limit for structures like TREE or Unique
  used in Item_sum_*
*/

ulonglong Item_sum::ram_limitation(THD *thd)
{
  ulonglong limitation= min(thd->variables.tmp_table_size,
                            thd->variables.max_heap_table_size);

  DBUG_EXECUTE_IF("simulate_low_itemsum_ram_limitation", limitation= 32;);

  return limitation;
}


/**
  Prepare an aggregate function for checking of context.

    The function initializes the members of the Item_sum object.
    It also checks the general validity of the set function:
    If none of the currently active query blocks allow evaluation of
    set functions, an error is reported.

  @note
    This function must be called for all set functions when expressions are
    resolved. It must be invoked in prefix order, ie at the descent of this
    traversal. @see corresponding Item_sum::check_sum_func(), which should
    be called on ascent.

  @param thd      reference to the thread context info

  @returns false if success, true if error
*/
 
bool Item_sum::init_sum_func_check(THD *thd)
{
  if (!thd->lex->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
    return true;
  }
  // Set a reference to the containing set function if there is one
  in_sum_func= thd->lex->in_sum_func;
  /*
    Set this object as the current containing set function, used when
    checking arguments of this set function.
  */
  thd->lex->in_sum_func= this;
  // @todo: When resolving once, move following code to constructor
  base_select= thd->lex->current_select();
  aggr_select= NULL;           // Aggregation query block is undetermined yet
  ref_by= NULL;
  max_aggr_level= -1;
  max_sum_func_level= -1;
  return false;
}


/**
  Validate the semantic requirements of a set function.

    Check whether the context of the set function allows it to be aggregated
    and, when it is an argument of another set function, directly or indirectly,
    the function makes sure that these two set functions are aggregated in
    different query blocks.
    If the context conditions are not met, an error is reported.
    If the set function is aggregated in some outer query block, it is
    added to the chain of items inner_sum_func_list attached to the
    aggregating query block.

    A number of designated members of the object are used to check the
    conditions. They are specified in the comment before the Item_sum
    class declaration.
    Additionally a bitmap variable called allow_sum_func is employed.
    It is included into the LEX structure.
    The bitmap contains 1 at n-th position if the query block at level "n"
    allows a set function reference (i.e the current resolver context for
    the query block is either in the SELECT list or in the HAVING or
    ORDER BY clause).

    Consider the query:
    @code
       SELECT SUM(t1.b) FROM t1 GROUP BY t1.a
         HAVING t1.a IN (SELECT t2.c FROM t2 WHERE AVG(t1.b) > 20) AND
                t1.a > (SELECT MIN(t2.d) FROM t2);
    @endcode
    when the set functions are resolved, allow_sum_func will contain:
    - for SUM(t1.b) - 1 at position 0 (SUM is in SELECT list)
    - for AVG(t1.b) - 1 at position 0 (subquery is in HAVING clause)
                      0 at position 1 (AVG is in WHERE clause)
    - for MIN(t2.d) - 1 at position 0 (subquery is in HAVING clause)
                      1 at position 1 (MIN is in SELECT list)

  @note
    This function must be called for all set functions when expressions are
    resolved. It must be invoked in postfix order, ie at the ascent of this
    traversal.

  @param thd  reference to the thread context info
  @param ref  location of the pointer to this item in the containing expression

  @returns false if success, true if error
*/
 
bool Item_sum::check_sum_func(THD *thd, Item **ref)
{
  const nesting_map allow_sum_func= thd->lex->allow_sum_func;
  const nesting_map nest_level_map= (nesting_map)1 << base_select->nest_level;

  DBUG_ASSERT(thd->lex->current_select() == base_select);
  DBUG_ASSERT(aggr_select == NULL);

  /*
    max_aggr_level is the level of the innermost qualifying query block of
    the column references of this set function. If the set function contains
    no column references, max_aggr_level is -1.
    max_aggr_level cannot be greater than nest level of the current query block.
  */
  DBUG_ASSERT(max_aggr_level <= base_select->nest_level);

  if (base_select->nest_level == max_aggr_level)
  {
    /*
      The function must be aggregated in the current query block,
      and it must be referred within a clause where it is valid
      (ie. HAVING clause, ORDER BY clause or SELECT list)
    */ 
    if ((allow_sum_func & nest_level_map) != 0)
      aggr_select= base_select;
  }
  else if (max_aggr_level >= 0 || !(allow_sum_func & nest_level_map))
  {
    /*
      Look for an outer query block where the set function should be
      aggregated. If it finds such a query block, then aggr_select is set
      to this query block
    */
    for (SELECT_LEX *sl= base_select->outer_select();
         sl && sl->nest_level >= max_aggr_level;
         sl= sl->outer_select() )
    {
      if (allow_sum_func & ((nesting_map)1 << sl->nest_level))
        aggr_select= sl;
    }
  }
  else // max_aggr_level < 0
  {
    /*
      Set function without column reference is aggregated in innermost query,
      without any validation.
    */
    aggr_select= base_select;
  }

  if (aggr_select == NULL &&
      (allow_sum_func & nest_level_map) != 0 &&
      !(thd->variables.sql_mode & MODE_ANSI))
    aggr_select= base_select;

  /*
    At this place a query block where the set function is to be aggregated
    has been found and is assigned to aggr_select, or aggr_select is NULL to
    indicate an invalid set function.

    Additionally, check whether possible nested set functions are acceptable
    here: their aggregation level must be greater than this set function's
    aggregation level.
  */
  if (aggr_select == NULL || aggr_select->nest_level <= max_sum_func_level)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
    return true;
  }

  if (aggr_select != base_select)
  {
    ref_by= ref;
    /*
      Add the set function to the list inner_sum_func_list for the
      aggregating query block.

      @note
        Now we 'register' only set functions that are aggregated in outer
        query blocks. Actually it makes sense to link all set functions for
        a query block in one chain. It would simplify the process of 'splitting'
        for set functions.
    */
    if (!aggr_select->inner_sum_func_list)
      next= this;
    else
    {
      next= aggr_select->inner_sum_func_list->next;
      aggr_select->inner_sum_func_list->next= this;
    }
    aggr_select->inner_sum_func_list= this;
    aggr_select->with_sum_func= true;

    /* 
      Mark subqueries as containing set function all the way up to the
      set function's aggregation query block.
      Note that we must not mark the Item of calculation context itself
      because with_sum_func on the aggregation query block is already set above.

      has_aggregation() being set for an Item means that this Item refers
      (somewhere in it, e.g. one of its arguments if it's a function) directly
      or indirectly to a set function that is calculated in a
      context "outside" of the Item (e.g. in the current or outer query block).

      with_sum_func being set for a query block means that this query block
      has set functions directly referenced (i.e. not through a subquery).
    */
    for (SELECT_LEX *sl= base_select;
         sl && sl != aggr_select && sl->master_unit()->item;
         sl= sl->outer_select())
      sl->master_unit()->item->set_aggregation();

    base_select->mark_as_dependent(aggr_select);
  }

  if (in_sum_func)
  {
    /*
      If the set function is nested adjust the value of
      max_sum_func_level for the containing set function.
      We take into account only set functions that are to be aggregated on
      the same level or outer compared to the nest level of the containing
      set function.
      But we must always pass up the max_sum_func_level because it is
      the maximum nest level of all directly and indirectly contained
      set functions. We must do that even for set functions that are
      aggregated inside of their containing set function's nest level
      because the containing function may contain another containing
      function that is to be aggregated outside or on the same level
      as its parent's nest level.
    */
    if (in_sum_func->base_select->nest_level >= aggr_select->nest_level)
      set_if_bigger(in_sum_func->max_sum_func_level, aggr_select->nest_level);
    set_if_bigger(in_sum_func->max_sum_func_level, max_sum_func_level);
  }

  aggr_select->set_agg_func_used(true);
  if (sum_func() == JSON_AGG_FUNC)
    aggr_select->set_json_agg_func_used(true);
  update_used_tables();
  thd->lex->in_sum_func= in_sum_func;

  return false;
}


Item_sum::Item_sum(const POS &pos, PT_item_list *opt_list)
: super(pos), next(NULL),
  arg_count(opt_list == NULL ? 0 : opt_list->elements()),
  forced_const(FALSE)
{
  if (arg_count > 0)
  {
    args= (Item**) sql_alloc(sizeof(Item*) * arg_count);
    if (args == NULL)
    {
      return; // OOM
    }
    uint i=0;
    List_iterator_fast<Item> li(opt_list->value);
    Item *item;

    while ((item=li++))
      args[i++]= item;
  }
  init_aggregator();
}


/**
  Constructor used in processing select with temporary tebles.
*/

Item_sum::Item_sum(THD *thd, Item_sum *item):
  Item_result_field(thd, item),
  next(NULL),
  base_select(item->base_select),
  aggr_select(item->aggr_select),
  quick_group(item->quick_group),
  arg_count(item->arg_count),
  used_tables_cache(item->used_tables_cache),
  forced_const(item->forced_const) 
{
  if (arg_count <= 2)
    args= tmp_args;
  else if (!(args= (Item**) thd->alloc(sizeof(Item*)*arg_count)))
    return;
  memcpy(args, item->args, sizeof(Item*)*arg_count);
  init_aggregator();
  with_distinct= item->with_distinct;
  if (item->aggr)
    set_aggregator(item->aggr->Aggrtype());
}


void Item_sum::mark_as_sum_func()
{
  mark_as_sum_func(current_thd->lex->current_select());
}


void Item_sum::mark_as_sum_func(SELECT_LEX *cur_select)
{
  cur_select->n_sum_items++;
  cur_select->with_sum_func= true;
  set_aggregation();
}


void Item_sum::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str, query_type);
  }
  str->append(')');
}

void Item_sum::fix_num_length_and_dec()
{
  decimals=0;
  for (uint i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
}


bool Item_sum::resolve_type(THD *)
{
  maybe_null= true;
  null_value= TRUE;

  const Sumfunctype t= sum_func();

  // None except these 3 types are allowed for geometry arguments.
  if (!(t == COUNT_FUNC || t == COUNT_DISTINCT_FUNC || t == SUM_BIT_FUNC))
    return reject_geometry_args(arg_count, args, this);
  return false;
}

bool Item_sum::walk(Item_processor processor, enum_walk walk, uchar *argument)
{
  if ((walk & WALK_PREFIX) && (this->*processor)(argument))
    return true;

  Item **arg,**arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
  {
    if ((*arg)->walk(processor, walk, argument))
      return true;
  }
  return (walk & WALK_POSTFIX) && (this->*processor)(argument);
}


/**
  Remove the item from the list of inner aggregation functions in the
  SELECT_LEX it was moved to by Item_sum::check_sum_func().

  This is done to undo some of the effects of Item_sum::check_sum_func() so
  that the item may be removed from the query.

  @note This doesn't completely undo Item_sum::check_sum_func(), as
  aggregation information is left untouched. This means that if this
  item is removed, aggr_select and all subquery items between aggr_select
  and this item may be left with has_aggregation() set to true, even if
  there are no aggregation functions. To our knowledge, this has no
  impact on the query result.

  @see Item_sum::check_sum_func()
  @see remove_redundant_subquery_clauses()
 */
bool Item_sum::clean_up_after_removal(uchar*)
{
  /*
    Don't do anything if
    1) this is an unresolved item (This may happen if an
       expression occurs twice in the same query. In that case, the
       whole item tree for the second occurence is replaced by the
       item tree for the first occurence, without calling fix_fields()
       on the second tree. Therefore there's nothing to clean up.), or
    2) there is no inner_sum_func_list, or
    3) the item is not an element in the inner_sum_func_list.
  */
  if (!fixed ||                                                          // 1
      aggr_select == NULL || aggr_select->inner_sum_func_list == NULL || // 2
      next == NULL)                                                      // 3
    return false;

  if (next == this)
    aggr_select->inner_sum_func_list= NULL;
  else
  {
    Item_sum *prev;
    for (prev= this; prev->next != this; prev= prev->next)
      ;
    prev->next= next;
    next= NULL;

    if (aggr_select->inner_sum_func_list == this)
      aggr_select->inner_sum_func_list= prev;
  }

  return false;
}


/// @note Please keep in sync with Item_func::eq().
bool Item_sum::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return true;
  if (item->type() != type())
    return false;
  const Item_sum *const item_sum= static_cast<const Item_sum *>(item);
  const enum Sumfunctype my_sum_func= sum_func();
  if (item_sum->sum_func() != my_sum_func)
    return false;
  if (arg_count != item_sum->arg_count ||
      (my_sum_func != Item_sum::UDF_SUM_FUNC &&
       func_name() != item_sum->func_name()) ||
      (my_sum_func == Item_sum::UDF_SUM_FUNC &&
       my_strcasecmp(system_charset_info, func_name(), item_sum->func_name())))
    return false;
  for (uint i= 0; i < arg_count ; i++)
  {
    if (!args[i]->eq(item_sum->args[i], binary_cmp))
      return false;
  }
  return true;
}


bool Item_sum::aggregate_check_distinct(uchar *arg)
{
  DBUG_ASSERT(fixed);
  Distinct_check *dc= reinterpret_cast<Distinct_check *>(arg);

  if (dc->is_stopped(this))
    return false;

  /*
    In the Standard, ORDER BY cannot contain an aggregate function;
    we are less strict, we allow it.
    However, if the aggregate in ORDER BY is not in the SELECT list, it
    might not be functionally dependent on all selected expressions, and thus
    might produce random order in combination with DISTINCT; then we reject
    it.

    One case where the aggregate is surely functionally dependent on the
    selected expressions, is if all GROUP BY expressions are in the SELECT
    list. But in that case DISTINCT is redundant and we have removed it in
    SELECT_LEX::prepare().
  */
  if (aggr_select == dc->select)
    return true;

  return false;
}


bool Item_sum::aggregate_check_group(uchar *arg)
{
  DBUG_ASSERT(fixed);
  Group_check *gc= reinterpret_cast<Group_check *>(arg);

  if (gc->is_stopped(this))
    return false;

  if (aggr_select != gc->select)
  {
    /*
      If aggr_select is inner to gc's select_lex, this aggregate function might
      reference some columns of gc, so we need to analyze its arguments.
      If it is outer, analyzing its arguments should not cause a problem, we
      will meet outer references which we will ignore.
    */
    return false;
  }

  if (gc->is_fd_on_source(this))
  {
    gc->stop_at(this);
    return false;
  }

  return true;
}


Field *Item_sum::create_tmp_field(bool, TABLE *table)
{
  Field *field;
  switch (result_type()) {
  case REAL_RESULT:
    field= new Field_double(max_length, maybe_null, item_name.ptr(), decimals, TRUE);
    break;
  case INT_RESULT:
    field= new Field_longlong(max_length, maybe_null, item_name.ptr(), unsigned_flag);
    break;
  case STRING_RESULT:
    return make_string_field(table);
  case DECIMAL_RESULT:
    field= Field_new_decimal::create_from_item(this);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
  if (field)
    field->init(table);
  return field;
}


void Item_sum::update_used_tables ()
{
  if (!forced_const)
  {
    used_tables_cache= 0;
    // Re-accumulate all properties except "aggregation"
    m_accum_properties&= PROP_AGGREGATION;

    for (uint i=0 ; i < arg_count ; i++)
    {
      args[i]->update_used_tables();
      used_tables_cache|= args[i]->used_tables();
      add_accum_properties(args[i]);
    }

    used_tables_cache&= PSEUDO_TABLE_BITS;

    /*
     if the function is aggregated into its local context, it can
     be calculated only after evaluating the full join, thus it
     depends on all tables of this join. Otherwise, it depends on
     outer tables, even if its arguments args[] do not explicitly
     reference an outer table, like COUNT (*) or COUNT(123).
    */
    used_tables_cache|= aggr_select == base_select ?
      ((table_map)1 << aggr_select->leaf_table_count) - 1 :
      OUTER_REF_TABLE_BIT;
  }
}


Item *Item_sum::set_arg(uint i, THD *thd, Item *new_val) 
{
  thd->change_item_tree(args + i, new_val);
  return new_val;
}


int Item_sum::set_aggregator(Aggregator::Aggregator_type aggregator)
{
  /*
    Dependent subselects may be executed multiple times, making
    set_aggregator to be called multiple times. The aggregator type
    will be the same, but it needs to be reset so that it is
    reevaluated with the new dependent data.
    This function may also be called multiple times during query optimization.
    In this case, the type may change, so we delete the old aggregator,
    and create a new one.
  */
  if (aggr && aggregator == aggr->Aggrtype())
  {
    aggr->clear();
    return FALSE;
  }

  delete aggr;
  switch (aggregator)
  {
  case Aggregator::DISTINCT_AGGREGATOR:
    aggr= new Aggregator_distinct(this);
    break;
  case Aggregator::SIMPLE_AGGREGATOR:
    aggr= new Aggregator_simple(this);
    break;
  };
  return aggr ? FALSE : TRUE;
}


void Item_sum::cleanup()
{
  if (aggr)
  {
    delete aggr;
    aggr= NULL;
  }
  Item_result_field::cleanup();
  forced_const= FALSE; 
}


/**
  Compare keys consisting of single field that cannot be compared as binary.

  Used by the Unique class to compare keys. Will do correct comparisons
  for all field types.

  @param    arg     Pointer to the relevant Field class instance
  @param    a       left key image
  @param    b       right key image
  @return   comparison result
    @retval < 0       if key1 < key2
    @retval = 0       if key1 = key2
    @retval > 0       if key1 > key2
*/

static int simple_str_key_cmp(const void* arg, const void* a, const void* b)
{
  Field *f= const_cast<Field*>(pointer_cast<const Field*>(arg));
  const uchar *key1= pointer_cast<const uchar*>(a);
  const uchar *key2= pointer_cast<const uchar*>(b);
  return f->cmp(key1, key2);
}


/**
  Correctly compare composite keys.

  Used by the Unique class to compare keys. Will do correct comparisons
  for composite keys with various field types.

  @param arg     Pointer to the relevant Aggregator_distinct instance
  @param a       left key image
  @param b       right key image
  @return        comparison result
    @retval <0       if key1 < key2
    @retval =0       if key1 = key2
    @retval >0       if key1 > key2
*/

int Aggregator_distinct::composite_key_cmp(const void *arg,
                                           const void *a, const void *b)
{
  Aggregator_distinct *aggr= (Aggregator_distinct *) arg;
  const uchar* key1= pointer_cast<const uchar*>(a);
  const uchar* key2= pointer_cast<const uchar*>(b);
  Field **field    = aggr->table->field;
  Field **field_end= field + aggr->table->s->fields;
  uint32 *lengths=aggr->field_lengths;
  for (; field < field_end; ++field)
  {
    Field* f = *field;
    int len = *lengths++;
    int res = f->cmp(key1, key2);
    if (res)
      return res;
    key1 += len;
    key2 += len;
  }
  return 0;
}


static enum enum_field_types 
calc_tmp_field_type(enum enum_field_types table_field_type, 
                    Item_result result_type)
{
  /* Adjust tmp table type according to the chosen aggregation type */
  switch (result_type) {
  case STRING_RESULT:
  case REAL_RESULT:
    if (table_field_type != MYSQL_TYPE_FLOAT)
      table_field_type= MYSQL_TYPE_DOUBLE;
    break;
  case INT_RESULT:
    table_field_type= MYSQL_TYPE_LONGLONG;
    /* fallthrough */
  case DECIMAL_RESULT:
    if (table_field_type != MYSQL_TYPE_LONGLONG)
      table_field_type= MYSQL_TYPE_NEWDECIMAL;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  return table_field_type;
}


/***************************************************************************/

C_MODE_START

/* Declarations for auxilary C-callbacks */

static int simple_raw_key_cmp(const void* arg,
                              const void* key1, const void* key2)
{
    return memcmp(key1, key2, *(const uint *) arg);
}


static int item_sum_distinct_walk(void *element, element_count,
                                  void *item)
{
  return ((Aggregator_distinct*) (item))->unique_walk_function(element);
}

C_MODE_END

/***************************************************************************/
/**
  Called before feeding the first row. Used to allocate/setup
  the internal structures used for aggregation.
 
  @param thd Thread descriptor
  @return status
    @retval FALSE success
    @retval TRUE  faliure  

    Prepares Aggregator_distinct to process the incoming stream.
    Creates the temporary table and the Unique class if needed.
    Called by Item_sum::aggregator_setup()
*/

bool Aggregator_distinct::setup(THD *thd)
{
  endup_done= FALSE;
  /*
    Setup can be called twice for ROLLUP items. This is a bug.
    Please add DBUG_ASSERT(tree == 0) here when it's fixed.
  */
  if (tree || table || tmp_table_param)
    return FALSE;

  DBUG_ASSERT(thd->lex->current_select() == item_sum->aggr_select);

  if (item_sum->setup(thd))
    return TRUE;
  if (item_sum->sum_func() == Item_sum::COUNT_FUNC || 
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC)
  {
    List<Item> list;
    SELECT_LEX *select_lex= item_sum->aggr_select;

    if (!(tmp_table_param= new (thd->mem_root) Temp_table_param))
      return TRUE;

    /**
      Create a table with an unique key over all parameters.
      If the list contains only const values, const_distinct
      is set to CONST_NOT_NULL to avoid creation of temp table
      and thereby counting as count(distinct of const values)
      will always be 1. If any of these const values is null,
      const_distinct is set to CONST_NULL to ensure aggregation
      does not happen.
     */
    uint const_items= 0;
    uint num_args= item_sum->get_arg_count();
    DBUG_ASSERT(num_args);
    for (uint i=0; i < num_args; i++)
    {
      Item *item=item_sum->get_arg(i);
      if (list.push_back(item))
        return true;                              // End of memory
      if (item->const_item())
      {
        if (item->is_null())
        {
          const_distinct= CONST_NULL;
          return false;
        }
        else
          const_items++;
      }
    }
    if (num_args == const_items)
    {
      const_distinct= CONST_NOT_NULL;
      return false;
    }
    count_field_types(select_lex, tmp_table_param, list, false, false);
    tmp_table_param->force_copy_fields= item_sum->has_force_copy_fields();
    DBUG_ASSERT(table == 0);
    /*
      Make create_tmp_table() convert BIT columns to BIGINT.
      This is needed because BIT fields store parts of their data in table's
      null bits, and we don't have methods to compare two table records, which
      is needed by Unique which is used when HEAP table is used.
    */
    {
      List_iterator_fast<Item> li(list);
      Item *item;
      while ((item= li++))
      {    
        if (item->type() == Item::FIELD_ITEM &&
            ((Item_field*)item)->field->type() == FIELD_TYPE_BIT)
          item->marker=4;
      }    
    }    
    if (!(table= create_tmp_table(thd, tmp_table_param, list, NULL, true, false,
                                  select_lex->active_options(),
                                  HA_POS_ERROR, "")))
      return TRUE;
    table->file->extra(HA_EXTRA_NO_ROWS);		// Don't update rows
    table->no_rows=1;
    if (table->hash_field)
      table->file->ha_index_init(0, 0);

    if (table->s->db_type() == heap_hton)
    {
      /*
        No blobs, otherwise it would have been MyISAM: set up a compare
        function and its arguments to use with Unique.
      */
      qsort2_cmp compare_key;
      void* cmp_arg;
      Field **field= table->field;
      Field **field_end= field + table->s->fields;
      bool all_binary= TRUE;

      for (tree_key_length= 0; field < field_end; ++field)
      {
        Field *f= *field;
        enum enum_field_types type= f->type();
        tree_key_length+= f->pack_length();
        if ((type == MYSQL_TYPE_VARCHAR) ||
            (!f->binary() && (type == MYSQL_TYPE_STRING ||
                             type == MYSQL_TYPE_VAR_STRING)))
        {
          all_binary= FALSE;
          break;
        }
      }
      if (all_binary)
      {
        cmp_arg= (void*) &tree_key_length;
        compare_key= simple_raw_key_cmp;
      }
      else
      {
        if (table->s->fields == 1)
        {
          /*
            If we have only one field, which is the most common use of
            count(distinct), it is much faster to use a simpler key
            compare method that can take advantage of not having to worry
            about other fields.
          */
          compare_key= simple_str_key_cmp;
          cmp_arg= (void*) table->field[0];
          /* tree_key_length has been set already */
        }
        else
        {
          uint32 *length;
          compare_key= composite_key_cmp;
          cmp_arg= (void*) this;
          field_lengths= (uint32*) thd->alloc(table->s->fields * sizeof(uint32));
          for (tree_key_length= 0, length= field_lengths, field= table->field;
               field < field_end; ++field, ++length)
          {
            *length= (*field)->pack_length();
            tree_key_length+= *length;
          }
        }
      }
      DBUG_ASSERT(tree == 0);
      tree= new Unique(compare_key, cmp_arg, tree_key_length,
                       item_sum->ram_limitation(thd));
      /*
        The only time tree_key_length could be 0 is if someone does
        count(distinct) on a char(0) field - stupid thing to do,
        but this has to be handled - otherwise someone can crash
        the server with a DoS attack
      */
      if (! tree)
        return TRUE;
    }
    return FALSE;
  }
  else
  {
    List<Create_field> field_list;
    Create_field field_def;                              /* field definition */
    Item *arg;
    DBUG_ENTER("Aggregator_distinct::setup");
    /* It's legal to call setup() more than once when in a subquery */
    if (tree)
      DBUG_RETURN(FALSE);

    /*
      Virtual table and the tree are created anew on each re-execution of
      PS/SP. Hence all further allocations are performed in the runtime
      mem_root.
    */
    if (field_list.push_back(&field_def))
      DBUG_RETURN(TRUE);

    item_sum->null_value= item_sum->maybe_null= 1;
    item_sum->quick_group= 0;

    DBUG_ASSERT(item_sum->get_arg(0)->fixed);

    arg= item_sum->get_arg(0);
    if (arg->const_item())
    {
      (void) arg->val_int();
      if (arg->null_value)
      {
        const_distinct= CONST_NULL;
        DBUG_RETURN(false);
      }
    }


    enum enum_field_types field_type=
      calc_tmp_field_type(arg->data_type(), arg->result_type());

    field_def.init_for_tmp_table(field_type, 
                                 arg->max_length,
                                 arg->decimals, 
                                 arg->maybe_null,
                                 arg->unsigned_flag,
                                 0);

    if (! (table= create_virtual_tmp_table(thd, field_list)))
      DBUG_RETURN(TRUE);

    /* XXX: check that the case of CHAR(0) works OK */
    tree_key_length= table->s->reclength - table->s->null_bytes;

    /*
      Unique handles all unique elements in a tree until they can't fit
      in.  Then the tree is dumped to the temporary file. We can use
      simple_raw_key_cmp because the table contains numbers only; decimals
      are converted to binary representation as well.
    */
    tree= new Unique(simple_raw_key_cmp, &tree_key_length, tree_key_length,
                     item_sum->ram_limitation(thd));

    DBUG_RETURN(tree == 0);
  }
}


/**
  Invalidate calculated value and clear the distinct rows.
 
  Frees space used by the internal data structures.
  Removes the accumulated distinct rows. Invalidates the calculated result.
*/

void Aggregator_distinct::clear()
{
  endup_done= FALSE;
  item_sum->clear();
  if (tree)
    tree->reset();
  /* tree and table can be both null only if const_distinct is enabled*/
  if (item_sum->sum_func() == Item_sum::COUNT_FUNC || 
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC)
  {
    if (!tree && table)
    {
      table->file->extra(HA_EXTRA_NO_CACHE);
      table->file->ha_index_or_rnd_end();
      table->file->ha_delete_all_rows();
      if (table->hash_field)
        table->file->ha_index_init(0, 0);
      table->file->extra(HA_EXTRA_WRITE_CACHE);
    }
  }
  else
  {
    item_sum->null_value= 1;
  }
}


/**
  Process incoming row. 
  
  Add it to Unique/temp hash table if it's unique. Skip the row if 
  not unique.
  Prepare Aggregator_distinct to process the incoming stream.
  Create the temporary table and the Unique class if needed.
  Called by Item_sum::aggregator_add().
  To actually get the result value in item_sum's buffers 
  Aggregator_distinct::endup() must be called.

  @return status
    @retval FALSE     success
    @retval TRUE      failure
*/

bool Aggregator_distinct::add()
{
  if (const_distinct == CONST_NULL)
    return 0;

  if (item_sum->sum_func() == Item_sum::COUNT_FUNC || 
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC)
  {
    int error;

    if (const_distinct == CONST_NOT_NULL)
    {
      DBUG_ASSERT(item_sum->fixed == 1);
      Item_sum_count *sum= (Item_sum_count *)item_sum;
      sum->count= 1;
      return 0;
    }
    if (copy_fields(tmp_table_param, table->in_use))
      return true;
    if (copy_funcs(tmp_table_param->items_to_copy, table->in_use))
      return TRUE;

    for (Field **field=table->field ; *field ; field++)
      if ((*field)->is_real_null())
        return 0;					// Don't count NULL

    if (tree)
    {
      /*
        The first few bytes of record (at least one) are just markers
        for deleted and NULLs. We want to skip them since they will
        bloat the tree without providing any valuable info. Besides,
        key_length used to initialize the tree didn't include space for them.
      */
      return tree->unique_add(table->record[0] + table->s->null_bytes);
    }

    if (!check_unique_constraint(table))
      return false;
    if ((error= table->file->ha_write_row(table->record[0])) &&
        !table->file->is_ignorable_error(error))
      return TRUE;
    return FALSE;
  }
  else
  {
    item_sum->get_arg(0)->save_in_field(table->field[0], false);
    if (table->field[0]->is_null())
      return 0;
    DBUG_ASSERT(tree);
    item_sum->null_value= 0;
    /*
      '0' values are also stored in the tree. This doesn't matter
      for SUM(DISTINCT), but is important for AVG(DISTINCT)
    */
    return tree->unique_add(table->field[0]->ptr);
  }
}


/**
  Calculate the aggregate function value.
 
  Since Distinct_aggregator::add() just collects the distinct rows,
  we must go over the distinct rows and feed them to the aggregation
  function before returning its value.
  This is what endup () does. It also sets the result validity flag
  endup_done to TRUE so it will not recalculate the aggregate value
  again if the Item_sum hasn't been reset.
*/

void Aggregator_distinct::endup()
{
  /* prevent consecutive recalculations */
  if (endup_done)
    return;

  if (const_distinct ==  CONST_NOT_NULL)
  {
    endup_done= TRUE;
    return;
  }

  /* we are going to calculate the aggregate value afresh */
  item_sum->clear();

  /* The result will definitely be null : no more calculations needed */
  if (const_distinct == CONST_NULL)
    return;

  if (item_sum->sum_func() == Item_sum::COUNT_FUNC || 
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC)
  {
    DBUG_ASSERT(item_sum->fixed == 1);
    Item_sum_count *sum= (Item_sum_count *)item_sum;

    if (tree && tree->elements == 0)
    {
      /* everything fits in memory */
      sum->count= (longlong) tree->elements_in_tree();
      endup_done= TRUE;
    }
    if (!tree)
    {
      /* there were blobs */
      table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
      if (table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT)
        sum->count= table->file->stats.records;
      else
      {
        // index must be closed before ha_records() is called
        if (table->file->inited)
          table->file->ha_index_or_rnd_end();
        ha_rows num_rows= 0;
        table->file->ha_records(&num_rows);
        // We have to initialize hash_index because update_sum_func needs it
        if (table->hash_field)
          table->file->ha_index_init(0, false);
        sum->count= static_cast<longlong>(num_rows);
      }
      endup_done= TRUE;
    }
  }

 /*
   We don't have a tree only if 'setup()' hasn't been called;
   this is the case of sql_executor.cc:return_zero_rows.
 */
  if (tree && !endup_done)
  {
   /*
     All tree's values are not NULL.
     Note that value of field is changed as we walk the tree, in
     Aggregator_distinct::unique_walk_function, but it's always not NULL.
   */
   table->field[0]->set_notnull();
    /* go over the tree of distinct keys and calculate the aggregate value */
    use_distinct_values= TRUE;
    tree->walk(item_sum_distinct_walk, (void*) this);
    use_distinct_values= FALSE;
  }
  /* prevent consecutive recalculations */
  endup_done= TRUE;
}


String *
Item_sum_num::val_str(String *str)
{
  return val_string_from_real(str);
}


my_decimal *Item_sum_num::val_decimal(my_decimal *decimal_value)
{
  return val_decimal_from_real(decimal_value);
}


String *
Item_sum_int::val_str(String *str)
{
  return val_string_from_int(str);
}


my_decimal *Item_sum_int::val_decimal(my_decimal *decimal_value)
{
  return val_decimal_from_int(decimal_value);
}


bool Item_sum_num::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  if (init_sum_func_check(thd))
    return true;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  maybe_null= false;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
    maybe_null|= args[i]->maybe_null;
  }

  // Set this value before calling resolve_type()
  null_value= TRUE;

  if (resolve_type(thd))
    return true;

  if (check_sum_func(thd, ref))
    return true;

  fixed= true;
  return false;
}

bool
Item_sum_bit::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(!fixed);

  if (init_sum_func_check(thd))
    return true;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  for (uint i= 0 ; i < arg_count ; i++)
  {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (resolve_type(thd))
    return true;

  if (thd->is_error())
    return true;

  if (check_sum_func(thd, ref))
    return true;

  fixed= true;
  return false;
}


bool Item_sum_bit::resolve_type(THD *)
{
  max_length= 0;
  if (bit_func_returns_binary(args[0], nullptr))
  {
    hybrid_type= STRING_RESULT;
    for (uint i= 0 ; i < arg_count ; i++)
      max_length= std::max(max_length, args[i]->max_length);
    if (max_length > (CONVERT_IF_BIGGER_TO_BLOB - 1))
    {
      /*
        Implementation of Item_sum_bit_field expects that "result_field" is
        Field_varstring, not Field_blob, so that the buffer's content is easily
        modifiable.
        The above check guarantees that the tmp table code will choose a
        Field_varstring over a Field_blob, and an assertion is present in the
        constructor of Item_sum_bit_field to verify the Field.
      */
      my_error(ER_INVALID_BITWISE_AGGREGATE_OPERANDS_SIZE, MYF(0), func_name());
      return true;
    }
    /*
     One extra byte needed to store a per-group boolean flag
     if Item_sum_bit_field is used.
    */
    max_length++;
    set_data_type(MYSQL_TYPE_VARCHAR);
  }
  else
  {
    hybrid_type= INT_RESULT;
    max_length= MAX_BIGINT_WIDTH + 1;
    set_data_type(MYSQL_TYPE_LONGLONG);
  }

  maybe_null= false;
  null_value= false;
  result_field= nullptr;
  decimals= 0;
  unsigned_flag= true;

  return reject_geometry_args(arg_count, args, this);
}

/**
   Executes the requested bitwise operation, taking args[0] as argument.
   If the result type is 'binary string':
   - takes value_buff as second argument and stores the result in value_buff.
   - sets the last character of value_buff to be a 'char' equal to
   1 if at least one non-NULL value has been seen for this group, to 0
   otherwise.
   If the result type is integer:
   - takes 'bits' as second argument and stores the result in 'bits'.
*/
template<class Char_op, class Int_op> bool
Item_sum_bit::eval_op(Char_op char_op, Int_op int_op)
{
  if (hybrid_type == STRING_RESULT)
  {
    String tmp_str;
    const String *s1= args[0]->val_str(&tmp_str);

    if (!s1 || args[0]->null_value)
      return false;

    DBUG_ASSERT(value_buff.length() > 0);
    // See if there has been a non-NULL value in this group:
    const bool non_nulls= value_buff.ptr()[value_buff.length() - 1];
    if (!non_nulls)
    {
      // Allocate length of argument + one extra byte for non_nulls
      value_buff.alloc(s1->length() + 1);
      value_buff.length(s1->length() + 1);
      // This is the first non-NULL value of the group, accumulate it.
      std::memcpy(value_buff.c_ptr_quick(), s1->ptr(), s1->length());
      // Store that a non-NULL value has been seen.
      value_buff.c_ptr_quick()[s1->length()]= 1;

      return false;
    }

    DBUG_ASSERT(value_buff.length() > 0);
    size_t buff_length= value_buff.length() - 1;
    /*
      If current value's length is different from the length of the
      accumulated value for this group, return error.
     */
    if (buff_length != s1->length())
    {
      my_error(ER_INVALID_BITWISE_OPERANDS_SIZE, MYF(0), func_name());
      return false;
    }

    // At this point the values should be not-null and have the same size.
    const uchar *s1_c_p= pointer_cast<const uchar *>(s1->ptr());
    uchar *str_bits=
      pointer_cast<uchar *>(const_cast<char *>(value_buff.ptr()));
    size_t i= 0;
    // Execute the bitwise operation.
    while (i + sizeof(longlong) <= buff_length)
    {
      int8store(&str_bits[i],
                int_op(uint8korr(&s1_c_p[i]), uint8korr(&str_bits[i])));
      i+= sizeof(longlong);
    }
    while (i < buff_length)
    {
      str_bits[i]= char_op(s1_c_p[i], str_bits[i]);
      i++;
    }

    return false;
  } // end hybrid_type == STRING_RESULT
  else // hybrid_type == INT_RESULT
  {
    ulonglong value= (ulonglong) args[0]->val_int();
    if (!args[0]->null_value)
      bits= int_op(bits, value);
  }

  return false;
}


bool
Item_sum_hybrid::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  Item *item= args[0];

  if (init_sum_func_check(thd))
    return true;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  // 'item' can be changed during fix_fields
  if ((!item->fixed && item->fix_fields(thd, args)) ||
      (item= args[0])->check_cols(1))
    return true;
  decimals=item->decimals;

  switch (hybrid_type= item->result_type()) {
  case INT_RESULT:
  case DECIMAL_RESULT:
  case STRING_RESULT:
    max_length= item->max_length;
    break;
  case REAL_RESULT:
    max_length= float_length(decimals);
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  };
  if (setup_hybrid(args[0], NULL))
    return true;
  /* MIN/MAX can return NULL for empty set indepedent of the used column */
  maybe_null= true;
  unsigned_flag=item->unsigned_flag;
  result_field= NULL;
  null_value= TRUE;
  if (resolve_type(thd))
    return true;
  item= item->real_item();
  if (item->type() == Item::FIELD_ITEM)
    set_data_type(item->data_type());
  else
    set_data_type_from_result(hybrid_type, max_length);

  if (check_sum_func(thd, ref))
    return true;

  fixed= true;
  return false;
}


/**
  MIN/MAX function setup.

  @param item       argument of MIN/MAX function
  @param value_arg  calculated value of MIN/MAX function

  @details
    Setup cache/comparator of MIN/MAX functions. When called by the
    copy_or_same function value_arg parameter contains calculated value
    of the original MIN/MAX object and it is saved in this object's cache.
*/

bool Item_sum_hybrid::setup_hybrid(Item *item, Item *value_arg)
{
  value= Item_cache::get_cache(item);
  value->setup(item);
  value->store(value_arg);
  arg_cache= Item_cache::get_cache(item);
  if (arg_cache == NULL)
    return true;
  arg_cache->setup(item);
  cmp= new Arg_comparator();
  if (cmp == NULL)
    return true;
  if (cmp->set_cmp_func(this, pointer_cast<Item **>(&arg_cache),
                              pointer_cast<Item **>(&value), false))
    return true;
  collation.set(item->collation);

  return false;
}


Field *Item_sum_hybrid::create_tmp_field(bool group, TABLE *table)
{
  Field *field;
  if (args[0]->type() == Item::FIELD_ITEM)
  {
    field= ((Item_field*) args[0])->field;
    
    if ((field= create_tmp_field_from_field(current_thd, field, item_name.ptr(),
                                            table, NULL)))
      field->flags&= ~NOT_NULL_FLAG;
    return field;
  }
  /*
    DATE/TIME fields have STRING_RESULT result types.
    In order to preserve field type, it's needed to handle DATE/TIME
    fields creations separately.
  */
  switch (args[0]->data_type()) {
  case MYSQL_TYPE_DATE:
    field= new Field_newdate(maybe_null, item_name.ptr());
    break;
  case MYSQL_TYPE_TIME:
    field= new Field_timef(maybe_null, item_name.ptr(), decimals);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    field= new Field_timestampf(maybe_null, item_name.ptr(), decimals);
    break;
  case MYSQL_TYPE_DATETIME:
    field= new Field_datetimef(maybe_null, item_name.ptr(), decimals);
    break;
  default:
    return Item_sum::create_tmp_field(group, table);
  }
  if (field)
    field->init(table);
  return field;
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

/**
  @todo
  check if the following assignments are really needed
*/
Item_sum_sum::Item_sum_sum(THD *thd, Item_sum_sum *item) 
  :Item_sum_num(thd, item), hybrid_type(item->hybrid_type),
   curr_dec_buff(item->curr_dec_buff)
{
  /* TODO: check if the following assignments are really needed */
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal2decimal(item->dec_buffs, dec_buffs);
    my_decimal2decimal(item->dec_buffs + 1, dec_buffs + 1);
  }
  else
    sum= item->sum;
}

Item *Item_sum_sum::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_sum(thd, this);
}


void Item_sum_sum::clear()
{
  DBUG_ENTER("Item_sum_sum::clear");
  null_value=1;
  if (hybrid_type == DECIMAL_RESULT)
  {
    curr_dec_buff= 0;
    my_decimal_set_zero(dec_buffs);
  }
  else
    sum= 0.0;
  DBUG_VOID_RETURN;
}


bool Item_sum_sum::resolve_type(THD *)
{
  DBUG_ENTER("Item_sum_sum::resolve_type");
  maybe_null= true;
  null_value= TRUE;
  decimals= args[0]->decimals;

  switch (args[0]->numeric_context_result_type()) {
  case REAL_RESULT:
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
  {
    /* SUM result can't be longer than length(arg) + length(MAX_ROWS) */
    int precision= args[0]->decimal_precision() + DECIMAL_LONGLONG_DIGITS;
    max_length= my_decimal_precision_to_length_no_truncation(precision,
                                                             decimals,
                                                             unsigned_flag);
    curr_dec_buff= 0;
    hybrid_type= DECIMAL_RESULT;
    my_decimal_set_zero(dec_buffs);
    break;
  }
  case STRING_RESULT:
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }

  if (reject_geometry_args(arg_count, args, this))
    DBUG_RETURN(true);

  set_data_type_from_result(hybrid_type, max_length);

  DBUG_PRINT("info", ("Type: %s (%d, %d)",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--"),
                      max_length,
                      (int)decimals));
  DBUG_RETURN(false);
}


bool Item_sum_sum::add()
{
  DBUG_ENTER("Item_sum_sum::add");
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value;
    const my_decimal *val= aggr->arg_val_decimal(&value);
    if (!aggr->arg_is_null(true))
    {
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff^1),
                     val, dec_buffs + curr_dec_buff);
      curr_dec_buff^= 1;
      null_value= 0;
    }
  }
  else
  {
    sum+= aggr->arg_val_real();
    if (!aggr->arg_is_null(true))
      null_value= 0;
  }
  DBUG_RETURN(0);
}


longlong Item_sum_sum::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (aggr)
    aggr->endup();
  if (hybrid_type == DECIMAL_RESULT)
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, unsigned_flag,
                   &result);
    return result;
  }
  return (longlong) rint(val_real());
}


double Item_sum_sum::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (aggr)
    aggr->endup();
  if (hybrid_type == DECIMAL_RESULT)
    my_decimal2double(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, &sum);
  return sum;
}


String *Item_sum_sum::val_str(String *str)
{
  if (aggr)
    aggr->endup();
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


my_decimal *Item_sum_sum::val_decimal(my_decimal *val)
{
  if (aggr)
    aggr->endup();
  if (hybrid_type == DECIMAL_RESULT)
    return (dec_buffs + curr_dec_buff);
  return val_decimal_from_real(val);
}

/**
  Aggregate a distinct row from the distinct hash table.
 
  Called for each row into the hash table 'Aggregator_distinct::table'.
  Includes the current distinct row into the calculation of the 
  aggregate value. Uses the Field classes to get the value from the row.
  This function is used for AVG/SUM(DISTINCT). For COUNT(DISTINCT) 
  it's called only when there are no blob arguments and the data don't
  fit into memory (so Unique makes persisted trees on disk). 

  @param element     pointer to the row data.
  
  @return status
    @retval FALSE     success
    @retval TRUE      failure
*/
  
bool Aggregator_distinct::unique_walk_function(void *element)
{
  memcpy(table->field[0]->ptr, element, tree_key_length);
  item_sum->add();
  return 0;
}


Aggregator_distinct::~Aggregator_distinct()
{
  if (tree)
  {
    delete tree;
    tree= NULL;
  }
  if (table)
  {
    if (table->file)
      table->file->ha_index_or_rnd_end();
    free_tmp_table(table->in_use, table);
    table=NULL;
  }
  if (tmp_table_param)
  {
    delete tmp_table_param;
    tmp_table_param= NULL;
  }
}


my_decimal *Aggregator_simple::arg_val_decimal(my_decimal *value)
{
  return item_sum->args[0]->val_decimal(value);
}


double Aggregator_simple::arg_val_real()
{
  return item_sum->args[0]->val_real();
}


bool Aggregator_simple::arg_is_null(bool use_null_value)
{
  Item **item= item_sum->args;
  const uint item_count= item_sum->arg_count;
  if (use_null_value)
  {
    for (uint i= 0; i < item_count; i++)
    {
      if (item[i]->null_value)
        return true;
    }
  }
  else
  {
    for (uint i= 0; i < item_count; i++)
    {
      if (item[i]->maybe_null && item[i]->is_null())
        return true;
    }
  }
  return false;
}


my_decimal *Aggregator_distinct::arg_val_decimal(my_decimal * value)
{
  return use_distinct_values ? table->field[0]->val_decimal(value) :
    item_sum->args[0]->val_decimal(value);
}


double Aggregator_distinct::arg_val_real()
{
  return use_distinct_values ? table->field[0]->val_real() :
    item_sum->args[0]->val_real();
}


bool Aggregator_distinct::arg_is_null(bool use_null_value)
{
  if (use_distinct_values)
  {
    const bool rc= table->field[0]->is_null();
    DBUG_ASSERT(!rc); // NULLs are never stored in 'tree'
    return rc;
  }
  return use_null_value ?
    item_sum->args[0]->null_value :
    (item_sum->args[0]->maybe_null && item_sum->args[0]->is_null());
}


Item *Item_sum_count::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_count(thd, this);
}


void Item_sum_count::clear()
{
  count= 0;
}


bool Item_sum_count::add()
{
  if (aggr->arg_is_null(false))
    return 0;
  count++;
  return 0;
}

longlong Item_sum_count::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (aggr)
    aggr->endup();
  return count;
}


void Item_sum_count::cleanup()
{
  DBUG_ENTER("Item_sum_count::cleanup");
  count= 0;
  Item_sum_int::cleanup();
  DBUG_VOID_RETURN;
}


bool Item_sum_avg::resolve_type(THD *thd)
{
  if (Item_sum_sum::resolve_type(thd))
    return true;

  maybe_null= true;
  null_value= TRUE;
  prec_increment= thd->variables.div_precincrement;
  if (hybrid_type == DECIMAL_RESULT)
  {
    int precision= args[0]->decimal_precision() + prec_increment;
    decimals= min<uint>(args[0]->decimals + prec_increment, DECIMAL_MAX_SCALE);
    max_length= my_decimal_precision_to_length_no_truncation(precision,
                                                             decimals,
                                                             unsigned_flag);
    f_precision= min(precision+DECIMAL_LONGLONG_DIGITS, DECIMAL_MAX_PRECISION);
    f_scale=  args[0]->decimals;
    dec_bin_size= my_decimal_get_binary_size(f_precision, f_scale);
  }
  else {
    decimals= min<uint>(args[0]->decimals + prec_increment, NOT_FIXED_DEC);
    max_length= args[0]->max_length + prec_increment;
  }
  set_data_type_from_result(hybrid_type, max_length);
  return false;
}


Item *Item_sum_avg::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_avg(thd, this);
}


Field *Item_sum_avg::create_tmp_field(bool group, TABLE *table)
{
  Field *field;
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    field= new Field_string(((hybrid_type == DECIMAL_RESULT) ?
                             dec_bin_size : sizeof(double)) + sizeof(longlong),
                            0, item_name.ptr(), &my_charset_bin);
  }
  else if (hybrid_type == DECIMAL_RESULT)
    field= Field_new_decimal::create_from_item(this);
  else
    field= new Field_double(max_length, maybe_null, item_name.ptr(), decimals, TRUE);
  if (field)
    field->init(table);
  return field;
}


void Item_sum_avg::clear()
{
  Item_sum_sum::clear();
  count=0;
}


bool Item_sum_avg::add()
{
  if (Item_sum_sum::add())
    return TRUE;
  if (!aggr->arg_is_null(true))
    count++;
  return FALSE;
}

double Item_sum_avg::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (aggr)
    aggr->endup();
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return Item_sum_sum::val_real() / ulonglong2double(count);
}


my_decimal *Item_sum_avg::val_decimal(my_decimal *val)
{
  my_decimal sum_buff, cnt;
  const my_decimal *sum_dec;
  DBUG_ASSERT(fixed == 1);
  if (aggr)
    aggr->endup();
  if (!count)
  {
    null_value=1;
    return NULL;
  }

  /*
    For non-DECIMAL hybrid_type the division will be done in
    Item_sum_avg::val_real().
  */
  if (hybrid_type != DECIMAL_RESULT)
    return val_decimal_from_real(val);

  sum_dec= dec_buffs + curr_dec_buff;
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &cnt);
  my_decimal_div(E_DEC_FATAL_ERROR, val, sum_dec, &cnt, prec_increment);
  return val;
}


String *Item_sum_avg::val_str(String *str)
{
  if (aggr)
    aggr->endup();
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


/*
  Standard deviation
*/

double Item_sum_std::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double nr= Item_sum_variance::val_real();
  DBUG_ASSERT(nr >= 0.0);
  return sqrt(nr);
}

Item *Item_sum_std::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_std(thd, this);
}


/*
  Variance
*/


/**
  Variance implementation for floating-point implementations, without
  catastrophic cancellation, from Knuth's _TAoCP_, 3rd ed, volume 2, pg232.
  This alters the value at m, s, and increments count.
*/

/*
  These two functions are used by the Item_sum_variance and the
  Item_variance_field classes, which are unrelated, and each need to calculate
  variance.  The difference between the two classes is that the first is used
  for a mundane SELECT, while the latter is used in a GROUPing SELECT.
*/
static void variance_fp_recurrence_next(double *m, double *s, ulonglong *count, double nr)
{
  *count += 1;

  if (*count == 1) 
  {
    *m= nr;
    *s= 0;
  }
  else
  {
    double m_kminusone= *m;
    *m= m_kminusone + (nr - m_kminusone) / (double) *count;
    *s= *s + (nr - m_kminusone) * (nr - *m);
  }
}


static double variance_fp_recurrence_result(double s, ulonglong count, bool is_sample_variance)
{
  if (count == 1)
    return 0.0;

  if (is_sample_variance)
    return s / (count - 1);

  /* else, is a population variance */
  return s / count;
}


Item_sum_variance::Item_sum_variance(THD *thd, Item_sum_variance *item):
  Item_sum_num(thd, item), hybrid_type(item->hybrid_type),
    count(item->count), sample(item->sample),
    prec_increment(item->prec_increment)
{
  recurrence_m= item->recurrence_m;
  recurrence_s= item->recurrence_s;
}


bool Item_sum_variance::resolve_type(THD *)
{
  DBUG_ENTER("Item_sum_variance::resolve_type");
  maybe_null= true;
  null_value= TRUE;

  /*
    According to the SQL2003 standard (Part 2, Foundations; sec 10.9,
    aggregate function; paragraph 7h of Syntax Rules), "the declared 
    type of the result is an implementation-defined aproximate numeric
    type.
  */
  set_data_type_double();
  hybrid_type= REAL_RESULT;

  if (reject_geometry_args(arg_count, args, this))
    DBUG_RETURN(true);
  DBUG_PRINT("info", ("Type: REAL_RESULT (%d, %d)", max_length, (int)decimals));
  DBUG_RETURN(false);
}


Item *Item_sum_variance::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_variance(thd, this);
}


/**
  Create a new field to match the type of value we're expected to yield.
  If we're grouping, then we need some space to serialize variables into, to
  pass around.
*/
Field *Item_sum_variance::create_tmp_field(bool group, TABLE *table)
{
  Field *field;
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    field= new Field_string(sizeof(double)*2 + sizeof(longlong), 0, item_name.ptr(), &my_charset_bin);
  }
  else
    field= new Field_double(max_length, maybe_null, item_name.ptr(), decimals, TRUE);

  if (field != NULL)
    field->init(table);

  return field;
}


void Item_sum_variance::clear()
{
  count= 0; 
}

bool Item_sum_variance::add()
{
  /* 
    Why use a temporary variable?  We don't know if it is null until we
    evaluate it, which has the side-effect of setting null_value .
  */
  double nr= args[0]->val_real();
  
  if (!args[0]->null_value)
    variance_fp_recurrence_next(&recurrence_m, &recurrence_s, &count, nr);
  return 0;
}

double Item_sum_variance::val_real()
{
  DBUG_ASSERT(fixed == 1);

  /*
    'sample' is a 1/0 boolean value.  If it is 1/true, id est this is a sample
    variance call, then we should set nullness when the count of the items
    is one or zero.  If it's zero, i.e. a population variance, then we only
    set nullness when the count is zero.

    Another way to read it is that 'sample' is the numerical threshhold, at and
    below which a 'count' number of items is called NULL.
  */
  DBUG_ASSERT((sample == 0) || (sample == 1));
  if (count <= sample)
  {
    null_value=1;
    return 0.0;
  }

  null_value=0;
  return variance_fp_recurrence_result(recurrence_s, count, sample);
}


my_decimal *Item_sum_variance::val_decimal(my_decimal *dec_buf)
{
  DBUG_ASSERT(fixed == 1);
  return val_decimal_from_real(dec_buf);
}


void Item_sum_variance::reset_field()
{
  double nr;
  uchar *res= result_field->ptr;

  nr= args[0]->val_real();              /* sets null_value as side-effect */

  if (args[0]->null_value)
    memset(res, 0, sizeof(double)*2+sizeof(longlong));
  else
  {
    /* Serialize format is (double)m, (double)s, (longlong)count */
    ulonglong tmp_count;
    double tmp_s;
    float8store(res, nr);               /* recurrence variable m */
    tmp_s= 0.0;
    float8store(res + sizeof(double), tmp_s);
    tmp_count= 1;
    int8store(res + sizeof(double)*2, tmp_count);
  }
}


void Item_sum_variance::update_field()
{
  ulonglong field_count;
  uchar *res=result_field->ptr;

  double nr= args[0]->val_real();       /* sets null_value as side-effect */

  if (args[0]->null_value)
    return;

  /* Serialize format is (double)m, (double)s, (longlong)count */
  double field_recurrence_m, field_recurrence_s;
  float8get(&field_recurrence_m, res);
  float8get(&field_recurrence_s, res + sizeof(double));
  field_count=sint8korr(res+sizeof(double)*2);

  variance_fp_recurrence_next(&field_recurrence_m, &field_recurrence_s, &field_count, nr);

  float8store(res, field_recurrence_m);
  float8store(res + sizeof(double), field_recurrence_s);
  res+= sizeof(double)*2;
  int8store(res,field_count);
}


/* min & max */

void Item_sum_hybrid::clear()
{
  value->clear();
  null_value= 1;
}

double Item_sum_hybrid::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0.0;
  double retval= value->val_real();
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == 0.0);
  return retval;
}

longlong Item_sum_hybrid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  longlong retval= value->val_int();
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == 0);
  return retval;
}


longlong Item_sum_hybrid::val_time_temporal()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  longlong retval= value->val_time_temporal();
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == 0);
  return retval;
}


longlong Item_sum_hybrid::val_date_temporal()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  longlong retval= value->val_date_temporal();
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == 0);
  return retval;
}


my_decimal *Item_sum_hybrid::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  my_decimal *retval= value->val_decimal(val);
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == NULL || my_decimal_is_zero(retval));
  return retval;
}


bool Item_sum_hybrid::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return true;
  return (null_value= value->get_date(ltime, fuzzydate));
}


bool Item_sum_hybrid::get_time(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return true;
  return (null_value= value->get_time(ltime));
}


String *
Item_sum_hybrid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  String *retval= value->val_str(str);
  if ((null_value= value->null_value))
    DBUG_ASSERT(retval == NULL);
  return retval;
}


bool Item_sum_hybrid::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed);
  if (null_value)
    return false;
  bool ok= value->val_json(wr);
  null_value= value->null_value;
  return ok;
}


void Item_sum_hybrid::cleanup()
{
  DBUG_ENTER("Item_sum_hybrid::cleanup");
  Item_sum::cleanup();
  forced_const= FALSE;
  if (cmp)
    delete cmp;
  cmp= 0;
  /*
    by default it is TRUE to avoid TRUE reporting by
    Item_func_not_all/Item_func_nop_all if this item was never called.

    no_rows_in_result() set it to FALSE if was not results found.
    If some results found it will be left unchanged.
  */
  was_values= TRUE;
  DBUG_VOID_RETURN;
}

void Item_sum_hybrid::no_rows_in_result()
{
  was_values= FALSE;
  clear();
}


Item *Item_sum_min::copy_or_same(THD* thd)
{
  Item_sum_min *item= new (thd->mem_root) Item_sum_min(thd, this);
  if (item == NULL)
    return NULL;
  if (item->setup_hybrid(args[0], value))
    return NULL;
  return item;
}


bool Item_sum_min::add()
{
  /* args[0] < value */
  arg_cache->cache_value();
  if (!arg_cache->null_value &&
      (null_value || cmp->compare() < 0))
  {
    value->store(arg_cache);
    value->cache_value();
    null_value= 0;
  }
  return 0;
}


Item *Item_sum_max::copy_or_same(THD* thd)
{
  Item_sum_max *item= new (thd->mem_root) Item_sum_max(thd, this);
  if (item == NULL)
    return NULL;
  if (item->setup_hybrid(args[0], value))
    return NULL;
  return item;
}


bool Item_sum_max::add()
{
  /* args[0] > value */
  arg_cache->cache_value();
  if (!arg_cache->null_value &&
      (null_value || cmp->compare() > 0))
  {
    value->store(arg_cache);
    value->cache_value();
    null_value= 0;
  }
  return 0;
}


String *Item_sum_bit::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
    return val_string_from_int(str);

  DBUG_ASSERT(value_buff.length() > 0);
  const bool non_nulls= value_buff.ptr()[value_buff.length() - 1];
  // If the group has no non-NULLs repeat the default value max_length times.
  if (!non_nulls)
  {
    if (str->alloc(max_length - 1))
      return nullptr;
    std::memset(const_cast<char *>(str->ptr()),
                static_cast<int>(reset_bits), max_length - 1);
    str->length(max_length - 1);
  }
  else
    // Remove the flag from result
    str->set(value_buff, 0, value_buff.length() - 1);

  str->set_charset(&my_charset_bin);
  return str;
}


bool Item_sum_bit::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
{
  if (hybrid_type == INT_RESULT)
    return get_date_from_int(ltime, fuzzydate);
  else
    return get_date_from_string(ltime, fuzzydate);
}


bool Item_sum_bit::get_time(MYSQL_TIME *ltime)
{
  if (hybrid_type == INT_RESULT)
    return get_time_from_int(ltime);
  else
    return get_time_from_string(ltime);
}


my_decimal *Item_sum_bit::val_decimal(my_decimal *dec_buf)
{
  if (hybrid_type == INT_RESULT)
    return val_decimal_from_int(dec_buf);
  else
    return val_decimal_from_string(dec_buf);
}



double Item_sum_bit::val_real()
{
  DBUG_ASSERT(fixed);
  if (hybrid_type == INT_RESULT)
    return bits;
  String *res;
  if (!(res= val_str(&str_value)))
    return 0.0;

  int ovf_error;
  char *from= const_cast<char *>(res->ptr());
  size_t len= res->length();
  char *end= from + len;
  return my_strtod(from, &end, &ovf_error);
}
/* bit_or and bit_and */

longlong Item_sum_bit::val_int()
{
  DBUG_ASSERT(fixed);
  if (hybrid_type == INT_RESULT)
    return (longlong) bits;

  String *res;
  if (!(res= val_str(&str_value)))
    return 0;

  int ovf_error;
  char *from= const_cast<char *>(res->ptr());
  size_t len= res->length();
  char *end= from + len;
  return my_strtoll10(from, &end, &ovf_error);
}


void Item_sum_bit::clear()
{
  if (hybrid_type == INT_RESULT)
    bits= reset_bits;
  else
    // Prepare value_buff for a new group.
    value_buff.set(initial_value_buff_storage, 1, &my_charset_bin);
}

Item *Item_sum_or::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_or(thd, this);
}


bool Item_sum_or::add()
{
  return eval_op(std::bit_or<char>(), std::bit_or<ulonglong>());
}

Item *Item_sum_xor::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_xor(thd, this);
}


bool Item_sum_xor::add()
{
  return eval_op(std::bit_xor<char>(), std::bit_xor<ulonglong>());
}

Item *Item_sum_and::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_and(thd, this);
}


bool Item_sum_and::add()
{
  return eval_op(std::bit_and<char>(), std::bit_and<ulonglong>());
}

/************************************************************************
** reset result of a Item_sum with is saved in a tmp_table
*************************************************************************/

void Item_sum_num::reset_field()
{
  double nr= args[0]->val_real();
  uchar *res=result_field->ptr;

  if (maybe_null)
  {
    if (args[0]->null_value)
    {
      nr=0.0;
      result_field->set_null();
    }
    else
      result_field->set_notnull();
  }
  float8store(res,nr);
}


void Item_sum_hybrid::reset_field()
{
  switch(hybrid_type) {
  case STRING_RESULT:
  {
    if (args[0]->is_temporal())
    {
      longlong nr= args[0]->val_temporal_by_field_type();
      if (maybe_null)
      {
        if (args[0]->null_value)
        {
          nr= 0;
          result_field->set_null();
        }
        else
          result_field->set_notnull();
      }
      result_field->store_packed(nr);
      break;
    }
    
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),result_field->charset()),*res;

    res=args[0]->val_str(&tmp);
    if (args[0]->null_value)
    {
      result_field->set_null();
      result_field->reset();
    }
    else
    {
      result_field->set_notnull();
      result_field->store(res->ptr(),res->length(),tmp.charset());
    }
    break;
  }
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();

    if (maybe_null)
    {
      if (args[0]->null_value)
      {
	nr=0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr, unsigned_flag);
    break;
  }
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();

    if (maybe_null)
    {
      if (args[0]->null_value)
      {
	nr=0.0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr);
    break;
  }
  case DECIMAL_RESULT:
  {
    my_decimal value_buff, *arg_dec= args[0]->val_decimal(&value_buff);

    if (maybe_null)
    {
      if (args[0]->null_value)
        result_field->set_null();
      else
        result_field->set_notnull();
    }
    /*
      We must store zero in the field as we will use the field value in
      add()
    */
    if (!arg_dec)                               // Null
      arg_dec= &decimal_zero;
    result_field->store_decimal(arg_dec);
    break;
  }
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
}


void Item_sum_sum::reset_field()
{
  DBUG_ASSERT (aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!arg_val)                               // Null
      arg_val= &decimal_zero;
    result_field->store_decimal(arg_val);
  }
  else
  {
    DBUG_ASSERT(hybrid_type == REAL_RESULT);
    double nr= args[0]->val_real();			// Nulls also return 0
    float8store(result_field->ptr, nr);
  }
  if (args[0]->null_value)
    result_field->set_null();
  else
    result_field->set_notnull();
}


void Item_sum_count::reset_field()
{
  uchar *res=result_field->ptr;
  longlong nr=0;
  DBUG_ASSERT (aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);

  if (!args[0]->maybe_null || !args[0]->is_null())
    nr=1;
  int8store(res,nr);
}


void Item_sum_avg::reset_field()
{
  uchar *res=result_field->ptr;
  DBUG_ASSERT (aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT)
  {
    longlong tmp;
    my_decimal value, *arg_dec= args[0]->val_decimal(&value);
    if (args[0]->null_value)
    {
      arg_dec= &decimal_zero;
      tmp= 0;
    }
    else
      tmp= 1;
    my_decimal2binary(E_DEC_FATAL_ERROR, arg_dec, res, f_precision, f_scale);
    res+= dec_bin_size;
    int8store(res, tmp);
  }
  else
  {
    double nr= args[0]->val_real();

    if (args[0]->null_value)
      memset(res, 0, sizeof(double)+sizeof(longlong));
    else
    {
      longlong tmp= 1;
      float8store(res,nr);
      res+=sizeof(double);
      int8store(res,tmp);
    }
  }
}


void Item_sum_bit::reset_field()
{
  reset_and_add();
  if (hybrid_type == INT_RESULT)
    // Store the result in result_field
    result_field->store(bits, unsigned_flag);
  else
    result_field->store(value_buff.ptr(), value_buff.length(),
                        value_buff.charset());
}

void Item_sum_bit::update_field()
{
  if (hybrid_type == INT_RESULT)
  {
    // Restore previous value to bits
    bits= result_field->val_int();
    // Add the current value to the group determined value.
    add();
    // Store the value in the result_field
    result_field->store(bits, unsigned_flag);
  }
  else // hybrid_type == STRING_RESULT
  {
    // Restore previous value to result_field
    result_field->val_str(&value_buff);
    // Add the current value to the previously determined one
    add();
    // Store the value in the result_field
    result_field->store((char*) value_buff.ptr(), value_buff.length(),
                        default_charset());
  }
}


/**
  calc next value and merge it with field_value.
*/

void Item_sum_sum::update_field()
{
  DBUG_ASSERT (aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      if (!result_field->is_null())
      {
        my_decimal field_value,
                   *field_val= result_field->val_decimal(&field_value);
        my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, field_val);
        result_field->store_decimal(dec_buffs);
      }
      else
      {
        result_field->store_decimal(arg_val);
        result_field->set_notnull();
      }
    }
  }
  else
  {
    double old_nr,nr;
    uchar *res=result_field->ptr;

    float8get(&old_nr,res);
    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      old_nr+=nr;
      result_field->set_notnull();
    }
    float8store(res,old_nr);
  }
}


void Item_sum_count::update_field()
{
  longlong nr;
  uchar *res=result_field->ptr;

  nr=sint8korr(res);
  if (!args[0]->maybe_null || !args[0]->is_null())
    nr++;
  int8store(res,nr);
}


void Item_sum_avg::update_field()
{
  longlong field_count;
  uchar *res=result_field->ptr;

  DBUG_ASSERT (aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);

  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      binary2my_decimal(E_DEC_FATAL_ERROR, res,
                        dec_buffs + 1, f_precision, f_scale);
      field_count= sint8korr(res + dec_bin_size);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, dec_buffs + 1);
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_buffs,
                        res, f_precision, f_scale);
      res+= dec_bin_size;
      field_count++;
      int8store(res, field_count);
    }
  }
  else
  {
    double nr;

    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      double old_nr;
      float8get(&old_nr, res);
      field_count= sint8korr(res + sizeof(double));
      old_nr+= nr;
      float8store(res,old_nr);
      res+= sizeof(double);
      field_count++;
      int8store(res, field_count);
    }
  }
}


void Item_sum_hybrid::update_field()
{
  switch (hybrid_type) {
  case STRING_RESULT:
    if (args[0]->is_temporal())
      min_max_update_temporal_field();
    else
      min_max_update_str_field();
    break;
  case INT_RESULT:
    min_max_update_int_field();
    break;
  case DECIMAL_RESULT:
    min_max_update_decimal_field();
    break;
  default:
    min_max_update_real_field();
  }
}


void Item_sum_hybrid::min_max_update_temporal_field()
{
  longlong nr, old_nr;
  old_nr= result_field->val_temporal_by_field_type();
  nr= args[0]->val_temporal_by_field_type();
  if (!args[0]->null_value)
  {
    if (result_field->is_null())
      old_nr= nr;
    else
    {
      bool res= unsigned_flag ?
                (ulonglong) old_nr > (ulonglong) nr : old_nr > nr;
      if ((cmp_sign > 0) ^ (!res))
        old_nr= nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null())
    result_field->set_null();
  result_field->store_packed(old_nr);
}


void Item_sum_hybrid::min_max_update_str_field()
{
  DBUG_ASSERT(cmp);
  String *res_str=args[0]->val_str(&cmp->value1);

  if (!args[0]->null_value)
  {
    result_field->val_str(&cmp->value2);

    if (result_field->is_null() ||
	(cmp_sign * sortcmp(res_str,&cmp->value2,collation.collation)) < 0)
      result_field->store(res_str->ptr(),res_str->length(),res_str->charset());
    result_field->set_notnull();
  }
}


void Item_sum_hybrid::min_max_update_real_field()
{
  double nr,old_nr;

  old_nr=result_field->val_real();
  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    if (result_field->is_null() ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null())
    result_field->set_null();
  result_field->store(old_nr);
}


void Item_sum_hybrid::min_max_update_int_field()
{
  longlong nr,old_nr;

  old_nr=result_field->val_int();
  nr=args[0]->val_int();
  if (!args[0]->null_value)
  {
    if (result_field->is_null())
      old_nr=nr;
    else
    {
      bool res=(unsigned_flag ?
		(ulonglong) old_nr > (ulonglong) nr :
		old_nr > nr);
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
	old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null())
    result_field->set_null();
  result_field->store(old_nr, unsigned_flag);
}


/**
  @todo
  optimize: do not get result_field in case of args[0] is NULL
*/
void Item_sum_hybrid::min_max_update_decimal_field()
{
  /* TODO: optimize: do not get result_field in case of args[0] is NULL */
  my_decimal old_val, nr_val;
  const my_decimal *old_nr= result_field->val_decimal(&old_val);
  const my_decimal *nr= args[0]->val_decimal(&nr_val);
  if (!args[0]->null_value)
  {
    if (result_field->is_null())
      old_nr=nr;
    else
    {
      bool res= my_decimal_cmp(old_nr, nr) > 0;
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
        old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null())
    result_field->set_null();
  result_field->store_decimal(old_nr);
}


Item_avg_field::Item_avg_field(Item_result res_type, Item_sum_avg *item)
{
  item_name= item->item_name;
  decimals=item->decimals;
  max_length= item->max_length;
  unsigned_flag= item->unsigned_flag;
  field=item->result_field;
  maybe_null= true;
  hybrid_type= res_type;
  set_data_type(hybrid_type == DECIMAL_RESULT ?
                MYSQL_TYPE_NEWDECIMAL : MYSQL_TYPE_DOUBLE);
  prec_increment= item->prec_increment;
  if (hybrid_type == DECIMAL_RESULT)
  {
    f_scale= item->f_scale;
    f_precision= item->f_precision;
    dec_bin_size= item->dec_bin_size;
  }
}

double Item_avg_field::val_real()
{
  // fix_fields() never calls for this Item
  double nr;
  longlong count;
  uchar *res;

  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  float8get(&nr,field->ptr);
  res= (field->ptr+sizeof(double));
  count= sint8korr(res);

  if ((null_value= !count))
    return 0.0;
  return nr/(double) count;
}


my_decimal *Item_avg_field::val_decimal(my_decimal *dec_buf)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  longlong count= sint8korr(field->ptr + dec_bin_size);
  if ((null_value= !count))
    return 0;

  my_decimal dec_count, dec_field;
  binary2my_decimal(E_DEC_FATAL_ERROR,
                    field->ptr, &dec_field, f_precision, f_scale);
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &dec_count);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf,
                 &dec_field, &dec_count, prec_increment);
  return dec_buf;
}


String *Item_avg_field::val_str(String *str)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


Item_sum_bit_field::Item_sum_bit_field(Item_result res_type,
                                       Item_sum_bit *item,
                                       ulonglong neutral_element)
{
  reset_bits= neutral_element;
  item_name= item->item_name;
  decimals= item->decimals;
  max_length= item->max_length;
  unsigned_flag= item->unsigned_flag;
  field= item->result_field;
  maybe_null= false;
  hybrid_type= res_type;
  DBUG_ASSERT(hybrid_type == INT_RESULT || hybrid_type == STRING_RESULT);
  if (hybrid_type == INT_RESULT)
    set_data_type(MYSQL_TYPE_LONGLONG);
  else if (hybrid_type == STRING_RESULT)
    set_data_type(MYSQL_TYPE_VARCHAR);
  // Implementation requires a non-Blob for string results.
  DBUG_ASSERT(hybrid_type != STRING_RESULT ||
              field->type() == MYSQL_TYPE_VARCHAR);
}

longlong Item_sum_bit_field::val_int()
{
  if (hybrid_type == INT_RESULT)
    return uint8korr(field->ptr);
  else
  {
    String *res;
    if (!(res= val_str(&str_value)))
      return 0;

    int ovf_error;
    char *from= const_cast<char *>(res->ptr());
    size_t len= res->length();
    char *end= from + len;
    return my_strtoll10(from, &end, &ovf_error);
  }
}


double Item_sum_bit_field::val_real()
{
  if (hybrid_type == INT_RESULT)
  {
    ulonglong result= uint8korr(field->ptr);
    return result;
  }
  else
  {
    String *res;
    if (!(res= val_str(&str_value)))
      return 0.0;

    int ovf_error;
    char *from= const_cast<char *>(res->ptr());
    size_t len= res->length();
    char *end= from + len;

    return my_strtod(from, &end, &ovf_error);
  }
}


my_decimal *Item_sum_bit_field::val_decimal(my_decimal *dec_buf)
{
  if (hybrid_type == INT_RESULT)
    return val_decimal_from_int(dec_buf);
  else
    return val_decimal_from_string(dec_buf);
}


/// @see Item_sum_bit::val_str()
String *Item_sum_bit_field::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
    return val_string_from_int(str);
  else
  {
    String *res_str= field->val_str(str);
    const bool non_nulls= res_str->ptr()[res_str->length() - 1];
    if (!non_nulls)
    {
      DBUG_EXECUTE_IF("simulate_sum_out_of_memory", {return nullptr;});
      if (res_str->alloc(max_length - 1))
        return nullptr;
      std::memset(const_cast<char *>(res_str->ptr()),
                  static_cast<int>(reset_bits), max_length - 1);
      res_str->length(max_length - 1);
      res_str->set_charset(&my_charset_bin);
    }
    else
      res_str->length(res_str->length() - 1);
    return res_str;
  }
}

bool Item_sum_bit_field::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
{
  if (hybrid_type == INT_RESULT)
    return get_date_from_decimal(ltime, fuzzydate);
  else
    return get_date_from_string(ltime, fuzzydate);
}
bool Item_sum_bit_field::get_time(MYSQL_TIME *ltime)
{
  if (hybrid_type == INT_RESULT)
    return get_time_from_numeric(ltime);
  else
    return get_time_from_string(ltime);
}


Item_std_field::Item_std_field(Item_sum_std *item)
  : Item_variance_field(item)
{
}


double Item_std_field::val_real()
{
  double nr;
  // fix_fields() never calls for this Item
  nr= Item_variance_field::val_real();
  DBUG_ASSERT(nr >= 0.0);
  return sqrt(nr);
}


my_decimal *Item_std_field::val_decimal(my_decimal *dec_buf)
{
  /*
    We can't call val_decimal_from_real() for DECIMAL_RESULT as
    Item_variance_field::val_real() would cause an infinite loop
  */
  my_decimal tmp_dec, *dec;
  double nr;
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  dec= Item_variance_field::val_decimal(dec_buf);
  if (!dec)
    return 0;
  my_decimal2double(E_DEC_FATAL_ERROR, dec, &nr);
  DBUG_ASSERT(nr >= 0.0);
  nr= sqrt(nr);
  double2my_decimal(E_DEC_FATAL_ERROR, nr, &tmp_dec);
  my_decimal_round(E_DEC_FATAL_ERROR, &tmp_dec, decimals, FALSE, dec_buf);
  return dec_buf;
}


Item_variance_field::Item_variance_field(Item_sum_variance *item)
{
  item_name= item->item_name;
  decimals= item->decimals;
  max_length= item->max_length;
  unsigned_flag= item->unsigned_flag;
  field=item->result_field;
  maybe_null= true;
  sample= item->sample;
  hybrid_type= item->hybrid_type;
  DBUG_ASSERT(hybrid_type == REAL_RESULT);
  set_data_type(MYSQL_TYPE_DOUBLE);
}


double Item_variance_field::val_real()
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  double recurrence_s;
  ulonglong count;
  float8get(&recurrence_s, (field->ptr + sizeof(double)));
  count=sint8korr(field->ptr+sizeof(double)*2);

  if ((null_value= (count <= sample)))
    return 0.0;

  return variance_fp_recurrence_result(recurrence_s, count, sample);
}


/****************************************************************************
** Functions to handle dynamic loadable aggregates
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Adapted for UDAs by: Andreas F. Bobak <bobak@relog.ch>.
** Rewritten by: Monty.
****************************************************************************/

bool Item_udf_sum::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UDF);
  pc->thd->lex->safe_to_cache_query= false;
  return false;
}


void Item_udf_sum::clear()
{
  DBUG_ENTER("Item_udf_sum::clear");
  udf.clear();
  DBUG_VOID_RETURN;
}

bool Item_udf_sum::add()
{
  DBUG_ENTER("Item_udf_sum::add");
  udf.add(&null_value);
  DBUG_RETURN(0);
}

void Item_udf_sum::cleanup()
{
  /*
    udf_handler::cleanup() nicely handles case when we have not
    original item but one created by copy_or_same() method.
  */
  udf.cleanup();
  Item_sum::cleanup();
}


void Item_udf_sum::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str, query_type);
  }
  str->append(')');
}


Item *Item_sum_udf_float::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_float(thd, this);
}

double Item_sum_udf_float::val_real()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}


String *Item_sum_udf_float::val_str(String *str)
{
  return val_string_from_real(str);
}


my_decimal *Item_sum_udf_float::val_decimal(my_decimal *dec)
{
  return val_decimal_from_real(dec);
}


String *Item_sum_udf_decimal::val_str(String *str)
{
  return val_string_from_decimal(str);
}


double Item_sum_udf_decimal::val_real()
{
  return val_real_from_decimal();
}


longlong Item_sum_udf_decimal::val_int()
{
  return val_int_from_decimal();
}


my_decimal *Item_sum_udf_decimal::val_decimal(my_decimal *dec_buf)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_udf_decimal::val_decimal");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
                     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_decimal(&null_value, dec_buf));
}


Item *Item_sum_udf_decimal::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_decimal(thd, this);
}


Item *Item_sum_udf_int::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_int(thd, this);
}

longlong Item_sum_udf_int::val_int()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val_int(&null_value));
}


String *Item_sum_udf_int::val_str(String *str)
{
  return val_string_from_int(str);
}

my_decimal *Item_sum_udf_int::val_decimal(my_decimal *dec)
{
  return val_decimal_from_int(dec);
}


/** Default max_length is max argument length. */

bool Item_sum_udf_str::resolve_type(THD *)
{
  set_data_type(MYSQL_TYPE_VARCHAR);
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  return false;
}


Item *Item_sum_udf_str::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_str(thd, this);
}


my_decimal *Item_sum_udf_str::val_decimal(my_decimal *dec)
{
  return val_decimal_from_string(dec);
}

String *Item_sum_udf_str::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_str::str");
  String *res=udf.val_str(str,&str_value);
  null_value = !res;
  DBUG_RETURN(res);
}


/*****************************************************************************
 GROUP_CONCAT function

 SQL SYNTAX:
  GROUP_CONCAT([DISTINCT] expr,... [ORDER BY col [ASC|DESC],...]
    [SEPARATOR str_const])

 concat of values from "group by" operation

 BUGS
   Blobs doesn't work with DISTINCT or ORDER BY
*****************************************************************************/



/** 
  Compares the values for fields in expr list of GROUP_CONCAT.
  @note
       
     GROUP_CONCAT([DISTINCT] expr [,expr ...]
              [ORDER BY {unsigned_integer | col_name | expr}
                  [ASC | DESC] [,col_name ...]]
              [SEPARATOR str_val])
 
  @return
  @retval -1 : key1 < key2 
  @retval  0 : key1 = key2
  @retval  1 : key1 > key2 
*/

extern "C"
int group_concat_key_cmp_with_distinct(const void* arg, const void* key1, 
                                       const void* key2)
{
  Item_func_group_concat *item_func= (Item_func_group_concat*)arg;
  TABLE *table= item_func->table;

  for (uint i= 0; i < item_func->arg_count_field; i++)
  {
    Item *item= item_func->args[i];
    /*
      If item is a const item then either get_tmp_table_field returns 0
      or it is an item over a const table.
    */
    if (item->const_item())
      continue;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= item->get_tmp_table_field();

    if (!field)
      continue;

    uint offset= field->offset(field->table->record[0])-table->s->null_bytes;
    int res= field->cmp((uchar*)key1 + offset, (uchar*)key2 + offset);
    if (res)
      return res;
  }
  return 0;
}


/**
  function of sort for syntax: GROUP_CONCAT(expr,... ORDER BY col,... )
*/

extern "C"
int group_concat_key_cmp_with_order(const void* arg, const void* key1, 
                                    const void* key2)
{
  const Item_func_group_concat* grp_item= (Item_func_group_concat*) arg;
  const ORDER *order_item, *end;
  TABLE *table= grp_item->table;

  for (order_item= grp_item->order_array.begin(),
         end= grp_item->order_array.end();
       order_item < end;
       order_item++)
  {
    Item *item= *(order_item)->item;
    /*
      If item is a const item then either get_tmp_table_field returns 0
      or it is an item over a const table.
    */
    if (item->const_item())
      continue;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
     */
    Field *field= item->get_tmp_table_field();
    if (!field)
      continue;

    uint offset= (field->offset(field->table->record[0]) -
                  table->s->null_bytes);
    int res= field->cmp((uchar*)key1 + offset, (uchar*)key2 + offset);
    if (res)
      return ((order_item)->direction == ORDER_ASC) ? res : -res;
  }
  /*
    We can't return 0 because in that case the tree class would remove this
    item as double value. This would cause problems for case-changes and
    if the returned values are not the same we do the sort on.
  */
  return 1;
}


/**
  Append data from current leaf to item->result.
*/

extern "C"
int dump_leaf_key(void* key_arg, element_count count MY_ATTRIBUTE((unused)),
                  void* item_arg)
{
  Item_func_group_concat *item= (Item_func_group_concat *) item_arg;
  TABLE *table= item->table;
  String tmp((char *)table->record[1], table->s->reclength,
             default_charset_info);
  String tmp2;
  uchar *key= (uchar *) key_arg;
  String *result= &item->result;
  Item **arg= item->args, **arg_end= item->args + item->arg_count_field;
  size_t old_length= result->length();

  if (!item->m_result_finalized)
    item->m_result_finalized= true;
  else
    result->append(*item->separator);

  tmp.length(0);

  for (; arg < arg_end; arg++)
  {
    String *res;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
      We also can't use table->field array to access the fields
      because it contains both order and arg list fields.
     */
    if ((*arg)->const_item())
      res= (*arg)->val_str(&tmp);
    else
    {
      Field *field= (*arg)->get_tmp_table_field();
      if (field)
      {
        uint offset= (field->offset(field->table->record[0]) -
                      table->s->null_bytes);
        DBUG_ASSERT(offset < table->s->reclength);
        res= field->val_str(&tmp, key + offset);
      }
      else
        res= (*arg)->val_str(&tmp);
    }
    if (res)
      result->append(*res);
  }

  item->row_count++;

  /* stop if length of result more than max_length */
  if (result->length() > item->max_length)
  {
    int well_formed_error;
    const CHARSET_INFO *cs= item->collation.collation;
    const char *ptr= result->ptr();
    size_t add_length;
    /*
      It's ok to use item->result.length() as the fourth argument
      as this is never used to limit the length of the data.
      Cut is done with the third argument.
    */
    add_length= cs->cset->well_formed_len(cs,
                                          ptr + old_length,
                                          ptr + item->max_length,
                                          result->length(),
                                          &well_formed_error);
    result->length(old_length + add_length);
    item->warning_for_row= TRUE;
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_CUT_VALUE_GROUP_CONCAT,
                        ER_THD(current_thd, ER_CUT_VALUE_GROUP_CONCAT),
                        item->row_count);

    /**
       To avoid duplicated warnings in Item_func_group_concat::val_str()
    */
    if (table && table->blob_storage)
      table->blob_storage->set_truncated_value(false);
    return 1;
  }
  return 0;
}


/**
  Constructor of Item_func_group_concat.

  @param pos The token's position.
  @param distinct_arg   distinct
  @param select_list    list of expression for show values
  @param opt_order_list list of sort columns
  @param separator_arg  string value of separator.
*/

Item_func_group_concat::Item_func_group_concat(const POS &pos,
                       bool distinct_arg, PT_item_list *select_list,
                       PT_order_list *opt_order_list,
                       String *separator_arg)
  :super(pos), tmp_table_param(0), separator(separator_arg), tree(0),
   unique_filter(NULL), table(0),
   order_array(*THR_MALLOC),
   arg_count_order(opt_order_list ? opt_order_list->value.elements : 0),
   arg_count_field(select_list->elements()),
   row_count(0),
   distinct(distinct_arg),
   warning_for_row(FALSE),
   always_null(false),
   force_copy_fields(0), original(0)
{
  Item *item_select;
  Item **arg_ptr;

  quick_group= FALSE;
  arg_count= arg_count_field + arg_count_order;

  if (!(args= (Item**) sql_alloc(sizeof(Item*) * arg_count)))
    return;

  if (order_array.reserve(arg_count_order))
    return;

  /* fill args items of show and sort */
  List_iterator_fast<Item> li(select_list->value);

  for (arg_ptr=args ; (item_select= li++) ; arg_ptr++)
    *arg_ptr= item_select;

  if (arg_count_order)
  {
    for (ORDER *order_item= opt_order_list->value.first;
         order_item != NULL;
         order_item= order_item->next)
    {
      order_array.push_back(*order_item);
      *arg_ptr= *order_item->item;
      order_array.back().item= arg_ptr++;
    }
    for (ORDER *ord= order_array.begin(); ord < order_array.end(); ++ord)
      ord->next= ord != &order_array.back() ? ord + 1 : NULL;
  }
}


bool Item_func_group_concat::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  context= pc->thd->lex->current_context();
  return false;
}


Item_func_group_concat::Item_func_group_concat(THD *thd,
                                               Item_func_group_concat *item)
  :Item_sum(thd, item),
  tmp_table_param(item->tmp_table_param),
  separator(item->separator),
  tree(item->tree),
  unique_filter(item->unique_filter),
  table(item->table),
  order_array(thd->mem_root),
  context(item->context),
  arg_count_order(item->arg_count_order),
  arg_count_field(item->arg_count_field),
  row_count(item->row_count),
  distinct(item->distinct),
  warning_for_row(item->warning_for_row),
  always_null(item->always_null),
  force_copy_fields(item->force_copy_fields),
  original(item)
{
  quick_group= item->quick_group;
  result.set_charset(collation.collation);

  /*
    Since the ORDER structures pointed to by the elements of the 'order' array
    may be modified in find_order_in_list() called from
    Item_func_group_concat::setup(), create a copy of those structures so that
    such modifications done in this object would not have any effect on the
    object being copied.
  */
  if (order_array.reserve(arg_count_order))
    return;

  for (uint i= 0; i < arg_count_order; i++)
  {
    /*
      Compiler generated copy constructor is used to
      to copy all the members of ORDER struct.
      It's also necessary to update ORDER::next pointer
      so that it points to new ORDER element.
    */
    order_array.push_back(item->order_array[i]);
  }
  if (arg_count_order)
  {
    for (ORDER *ord= order_array.begin(); ord < order_array.end(); ++ord)
      ord->next= ord != &order_array.back() ? ord + 1 : NULL;
  }
}



void Item_func_group_concat::cleanup()
{
  DBUG_ENTER("Item_func_group_concat::cleanup");
  Item_sum::cleanup();

  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    delete tmp_table_param;
    tmp_table_param= 0;
    if (table)
    {
      THD *thd= table->in_use;
      if (table->blob_storage)
        delete table->blob_storage;
      free_tmp_table(thd, table);
      table= 0;
      if (tree)
      {
        delete_tree(tree);
        tree= 0;
      }
      if (unique_filter)
      {
        delete unique_filter;
        unique_filter= NULL;
      }
    }
    DBUG_ASSERT(tree == 0);
  }
  /*
   As the ORDER structures pointed to by the elements of the
   'order' array may be modified in find_order_in_list() called
   from Item_func_group_concat::setup() to point to runtime
   created objects, we need to reset them back to the original
   arguments of the function.
   */
  for (uint i= 0; i < arg_count_order; i++)
  {
    if (order_array[i].is_position)
      args[arg_count_field + i]= order_array[i].item_ptr;
  }
  DBUG_VOID_RETURN;
}


Field *Item_func_group_concat::make_string_field(TABLE *table_arg)
{
  Field *field;
  DBUG_ASSERT(collation.collation);
  /*
    max_characters is maximum number of characters
    what can fit into max_length size. It's necessary
    to use field size what allows to store group_concat
    result without truncation. For this purpose we use
    max_characters * CS->mbmaxlen.
  */
  const uint32 max_characters= max_length / collation.collation->mbminlen;
  if (max_characters > CONVERT_IF_BIGGER_TO_BLOB)
    field= new Field_blob(max_characters * collation.collation->mbmaxlen,
                          maybe_null, item_name.ptr(),
                          collation.collation, true);
  else
    field= new Field_varstring(max_characters * collation.collation->mbmaxlen,
                               maybe_null, item_name.ptr(), table_arg->s, collation.collation);

  if (field)
    field->init(table_arg);
  return field;
}


Item *Item_func_group_concat::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_func_group_concat(thd, this);
}


void Item_func_group_concat::clear()
{
  result.length(0);
  result.copy();
  null_value= TRUE;
  warning_for_row= FALSE;
  m_result_finalized= false;
  if (tree)
    reset_tree(tree);
  if (unique_filter)
    unique_filter->reset();
  if (table && table->blob_storage)
    table->blob_storage->reset();
  /* No need to reset the table as we never call write_row */
}


bool Item_func_group_concat::add()
{
  if (always_null)
    return 0;
  if (copy_fields(tmp_table_param, table->in_use))
    return true;
  if (copy_funcs(tmp_table_param->items_to_copy, table->in_use))
    return TRUE;

  for (uint i= 0; i < arg_count_field; i++)
  {
    Item *show_item= args[i];
    if (show_item->const_item())
      continue;

    Field *field= show_item->get_tmp_table_field();
    if (field && field->is_null_in_record((const uchar*) table->record[0]))
        return 0;                               // Skip row if it contains null
  }

  null_value= FALSE;
  bool row_eligible= TRUE;

  if (distinct) 
  {
    /* Filter out duplicate rows. */
    uint count= unique_filter->elements_in_tree();
    unique_filter->unique_add(table->record[0] + table->s->null_bytes);
    if (count == unique_filter->elements_in_tree())
      row_eligible= FALSE;
  }

  TREE_ELEMENT *el= 0;                          // Only for safety
  if (row_eligible && tree)
  {
    DBUG_EXECUTE_IF("trigger_OOM_in_gconcat_add",
                     DBUG_SET("+d,simulate_persistent_out_of_memory"););
    el= tree_insert(tree, table->record[0] + table->s->null_bytes, 0,
                    tree->custom_arg);
    DBUG_EXECUTE_IF("trigger_OOM_in_gconcat_add",
                    DBUG_SET("-d,simulate_persistent_out_of_memory"););
    /* check if there was enough memory to insert the row */
    if (!el)
      return 1;
  }
  /*
    In case of GROUP_CONCAT with DISTINCT or ORDER BY (or both) don't dump the
    row to the output buffer here. That will be done in val_str.
  */
  if (row_eligible && !warning_for_row && tree == nullptr && !distinct)
    dump_leaf_key(table->record[0] + table->s->null_bytes, 1, this);

  return 0;
}


bool
Item_func_group_concat::fix_fields(THD *thd, Item **ref)
{
  uint i;                       /* for loop variable */
  DBUG_ASSERT(fixed == 0);

  if (init_sum_func_check(thd))
    return TRUE;

  maybe_null= 1;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  /*
    Fix fields for select list and ORDER clause
  */

  for (i=0 ; i < arg_count ; i++)
  {
    if ((!args[i]->fixed &&
         args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return TRUE;
  }

  /* skip charset aggregation for order columns */
  if (agg_item_charsets_for_string_result(collation, func_name(),
                                          args, arg_count - arg_count_order))
    return 1;

  result.set_charset(collation.collation);
  result_field= 0;
  null_value= 1;
  max_length= thd->variables.group_concat_max_len;
  set_data_type(max_length/collation.collation->mbmaxlen >
                CONVERT_IF_BIGGER_TO_BLOB ?
                MYSQL_TYPE_BLOB : MYSQL_TYPE_VARCHAR);

  size_t offset;
  if (separator->needs_conversion(separator->length(), separator->charset(),
                                  collation.collation, &offset))
  {
    size_t buflen= collation.collation->mbmaxlen * separator->length();
    uint errors;
    size_t conv_length;
    char *buf;
    String *new_separator;

    if (!(buf= (char*) thd->stmt_arena->alloc(buflen)) ||
        !(new_separator= new(thd->stmt_arena->mem_root)
                           String(buf, buflen, collation.collation)))
      return TRUE;
    
    conv_length= copy_and_convert(buf, buflen, collation.collation,
                                  separator->ptr(), separator->length(),
                                  separator->charset(), &errors);
    new_separator->length(conv_length);
    separator= new_separator;
  }

  if (check_sum_func(thd, ref))
    return TRUE;

  fixed= 1;
  return FALSE;
}


bool Item_func_group_concat::setup(THD *thd)
{
  DBUG_ENTER("Item_func_group_concat::setup");

  List<Item> list;
  DBUG_ASSERT(thd->lex->current_select() == aggr_select);

  const bool order_or_distinct= MY_TEST(arg_count_order > 0 || distinct);

  /*
    Currently setup() can be called twice. Please add
    assertion here when this is fixed.
  */
  if (table || tree)
    DBUG_RETURN(FALSE);

  if (!(tmp_table_param= new (thd->mem_root) Temp_table_param))
    DBUG_RETURN(TRUE);

  /* Push all not constant fields to the list and create a temp table */
  always_null= 0;
  for (uint i= 0; i < arg_count_field; i++)
  {
    Item *item= args[i];
    if (list.push_back(item))
      DBUG_RETURN(TRUE);
    if (item->const_item())
    {
      if (item->is_null())
      {
        always_null= 1;
        DBUG_RETURN(FALSE);
      }
    }
  }

  List<Item> all_fields(list);
  /*
    Try to find every ORDER expression in the list of GROUP_CONCAT
    arguments. If an expression is not found, prepend it to
    "all_fields". The resulting field list is used as input to create
    tmp table columns.
  */
  if (arg_count_order &&
      setup_order(thd, Ref_item_array(args, arg_count),
                  context->table_list, list, all_fields, order_array.begin()))
    DBUG_RETURN(TRUE);

  count_field_types(aggr_select, tmp_table_param, all_fields, false, true);
  tmp_table_param->force_copy_fields= force_copy_fields;
  DBUG_ASSERT(table == 0);
  if (order_or_distinct)
  {
    /*
      Force the create_tmp_table() to convert BIT columns to INT
      as we cannot compare two table records containg BIT fields
      stored in the the tree used for distinct/order by.
      Moreover we don't even save in the tree record null bits 
      where BIT fields store parts of their data.
    */
    List_iterator_fast<Item> li(all_fields);
    Item *item;
    while ((item= li++))
    {
      if (item->type() == Item::FIELD_ITEM && 
          ((Item_field*) item)->field->type() == FIELD_TYPE_BIT)
        item->marker= 4;
    }
  }

  /*
    We have to create a temporary table to get descriptions of fields
    (types, sizes and so on).

    Note that in the table, we first have the ORDER BY fields, then the
    field list.
  */
  if (!(table= create_tmp_table(thd, tmp_table_param, all_fields,
                                NULL, false, true,
                                aggr_select->active_options(),
                                HA_POS_ERROR, (char*) "")))
    DBUG_RETURN(TRUE);
  table->file->extra(HA_EXTRA_NO_ROWS);
  table->no_rows= 1;

  /**
    Initialize blob_storage if GROUP_CONCAT is used
    with ORDER BY | DISTINCT and BLOB field count > 0.    
  */
  if (order_or_distinct && table->s->blob_fields)
    table->blob_storage= new Blob_mem_storage();

  /*
     Need sorting or uniqueness: init tree and choose a function to sort.
     Don't reserve space for NULLs: if any of gconcat arguments is NULL,
     the row is not added to the result.
  */
  uint tree_key_length= table->s->reclength - table->s->null_bytes;

  if (arg_count_order)
  {
    tree= &tree_base;
    /*
      Create a tree for sorting. The tree is used to sort (according to the
      syntax of this function). If there is no ORDER BY clause, we don't
      create this tree.
    */
    init_tree(tree,  min(static_cast<ulong>(thd->variables.max_heap_table_size),
                               thd->variables.sortbuff_size/16), 0,
              tree_key_length, 
              group_concat_key_cmp_with_order , 0, NULL, (void*) this);
  }

  if (distinct)
    unique_filter= new Unique(group_concat_key_cmp_with_distinct,
                              (void*)this,
                              tree_key_length,
                              ram_limitation(thd));
  
  DBUG_RETURN(FALSE);
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_func_group_concat::make_unique()
{
  tmp_table_param= 0;
  table=0;
  original= 0;
  force_copy_fields= 1;
  tree= 0;
}


String* Item_func_group_concat::val_str(String*)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;

  if (!m_result_finalized) // Result yet to be written.
  {
    if (tree != nullptr) // order by
      tree_walk(tree, &dump_leaf_key, this, left_root_right);
    else if (distinct) // distinct (and no order by).
      unique_filter->walk(&dump_leaf_key, this);
    else
      DBUG_ASSERT(false); // Can't happen
  }

  if (table && table->blob_storage && 
      table->blob_storage->is_truncated_value())
  {
    warning_for_row= true;
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_CUT_VALUE_GROUP_CONCAT,
                        ER_THD(current_thd, ER_CUT_VALUE_GROUP_CONCAT),
                        row_count);
  }

  return &result;
}


void Item_func_group_concat::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("group_concat("));
  if (distinct)
    str->append(STRING_WITH_LEN("distinct "));
  for (uint i= 0; i < arg_count_field; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str, query_type);
  }
  if (arg_count_order)
  {
    str->append(STRING_WITH_LEN(" order by "));
    for (uint i= 0 ; i < arg_count_order ; i++)
    {
      if (i)
        str->append(',');
      args[i + arg_count_field]->print(str, query_type);
      if (order_array[i].direction == ORDER_ASC)
        str->append(STRING_WITH_LEN(" ASC"));
      else
        str->append(STRING_WITH_LEN(" DESC"));
    }
  }
  str->append(STRING_WITH_LEN(" separator \'"));

  if (query_type & QT_TO_SYSTEM_CHARSET)
  {
    // Convert to system charset.
   convert_and_print(separator, str, system_charset_info);
  }
  else if (query_type & QT_TO_ARGUMENT_CHARSET)
  {
    /*
      Convert the string literals to str->charset(),
      which is typically equal to charset_set_client.
    */
   convert_and_print(separator, str, str->charset());
  }
  else
  {
    separator->print(str);
  }
  str->append(STRING_WITH_LEN("\')"));
}


Item_func_group_concat::~Item_func_group_concat()
{
  if (!original && unique_filter)
    delete unique_filter;    
}


bool Item_sum_json::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(!fixed);
  result_field= nullptr;

  if (init_sum_func_check(thd))
    return true;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  for (uint i= 0; i < arg_count; i++)
  {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (resolve_type(thd))
    return true;

  if (check_sum_func(thd, ref))
    return true;

  maybe_null= true;
  null_value= true;
  fixed= true;
  return false;
}

String *Item_sum_json::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value || m_wrapper.empty())
    return nullptr;
  str->length(0);
  if (m_wrapper.to_string(str, true, func_name()))
    return error_str();

  return str;
}


bool Item_sum_json::val_json(Json_wrapper *wr)
{
  if (null_value || m_wrapper.empty())
    return true;

  /*
    val_* functions are called more than once in aggregates and
    by passing the dom some function will destroy it so a clone is needed.
  */
  *wr= Json_wrapper(m_wrapper.clone_dom(current_thd));
  return false;
}


double Item_sum_json::val_real()
{
  if (null_value || m_wrapper.empty())
    return 0.0;

  return m_wrapper.coerce_real(func_name());
}


longlong Item_sum_json::val_int()
{
  if (null_value || m_wrapper.empty())
    return 0;

  return m_wrapper.coerce_int(func_name());
}


my_decimal *Item_sum_json::val_decimal(my_decimal *decimal_value)
{
  if (null_value || m_wrapper.empty())
  {
    my_decimal_set_zero(decimal_value);
    return decimal_value;
  }

  return m_wrapper.coerce_decimal(decimal_value, func_name());
}


bool Item_sum_json::get_date(MYSQL_TIME *ltime, my_time_flags_t)
{
  if (null_value || m_wrapper.empty())
    return true;

  return m_wrapper.coerce_date(ltime, func_name());
}


bool Item_sum_json::get_time(MYSQL_TIME *ltime)
{
  if (null_value || m_wrapper.empty())
    return true;

  return m_wrapper.coerce_time(ltime, func_name());
}


void Item_sum_json::reset_field()
{
  /* purecov: begin inspected */
  DBUG_ASSERT(0); // Check JOIN::with_json_agg for more details.
  // Create the container
  clear();
  // Append element to the container.
  add();

  /*
    field_type is MYSQL_TYPE_JSON so Item::make_string_field will always
    create a Field_json(in Item_sum::create_tmp_field).
    The cast is need since Field does not expose store_json function.
  */
  Field_json *json_result_field= down_cast<Field_json *>(result_field);
  json_result_field->set_notnull();
  // Store the container inside the field.
  json_result_field->store_json(&m_wrapper);
  /* purecov: end */
}


void Item_sum_json::update_field()
{
  /* purecov: begin inspected */
  DBUG_ASSERT(0); // Check JOIN::with_json_agg for more details.
  /*
    field_type is MYSQL_TYPE_JSON so Item::make_string_field will always
    create a Field_json(in Item_sum::create_tmp_field).
    The cast is need since Field does not expose store_json function.
  */
  Field_json *json_result_field= down_cast<Field_json *>(result_field);
  // Restore the container(m_wrapper) from the field
  json_result_field->val_json(&m_wrapper);

  // Append elements to the container.
  add();
  // Store the container inside the field.
  json_result_field->store_json(&m_wrapper);
  json_result_field->set_notnull();
  /* purecov: end */
}


void Item_sum_json_array::clear()
{
  null_value= true;
  m_json_array.clear();

  // Set the array to the m_wrapper.
  m_wrapper= Json_wrapper(&m_json_array);
  // But let Item_sum_json_array keep the ownership.
  m_wrapper.set_alias();
}


void Item_sum_json_object::clear()
{
  null_value= true;
  m_json_object.clear();

  // Set the object to the m_wrapper.
  m_wrapper= Json_wrapper(&m_json_object);
  // But let Item_sum_json_object keep the ownership.
  m_wrapper.set_alias();
}


bool Item_sum_json_array::add()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ASSERT(arg_count == 1);

  const THD *thd= base_select->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error())
    return error_json();

  try
  {
    Json_wrapper value_wrapper;
    // Get the value.
    if (get_atom_null_as_null(args, 0, func_name(), &m_value,
                              &m_conversion_buffer,
                              &value_wrapper))
      return error_json();

    /*
      The m_wrapper always points to m_json_array or the result of
      deserializing the result_field in reset/update_field.
    */
    const auto arr= down_cast<Json_array *>(m_wrapper.to_dom(thd));
    if (arr->append_alias(value_wrapper.to_dom(thd)))
      return error_json();              /* purecov: inspected */

    null_value= false;
    value_wrapper.set_alias(); // release the DOM
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  return false;
}


Item *Item_sum_json_array::copy_or_same(THD *thd)
{
  return new (thd->mem_root) Item_sum_json_array(thd, this);
}


bool Item_sum_json_object::add()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ASSERT(arg_count == 2);

  const THD *thd= base_select->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error())
    return error_json();

  try
  {
    // key
    Item *key_item= args[0];
    const char *safep;         // contents of key_item, possibly converted
    size_t safe_length;        // length of safep

    if (get_json_string(key_item, &m_tmp_key_value, &m_conversion_buffer,
                        &safep, &safe_length))
    {
      my_error(ER_JSON_DOCUMENT_NULL_KEY, MYF(0));
      return error_json();
    }

    std::string key(safep, safe_length);

    // value
    Json_wrapper value_wrapper;
    if (get_atom_null_as_null(args, 1, func_name(), &m_value,
                              &m_conversion_buffer, &value_wrapper))
      return error_json();

    /*
      The m_wrapper always points to m_json_object or the result of
      deserializing the result_field in reset/update_field.
    */
    Json_object *object= down_cast<Json_object *>(m_wrapper.to_dom(thd));
    if (object->add_alias(key, value_wrapper.to_dom(thd)))
      return error_json();              /* purecov: inspected */

    null_value= false;
    // object will take ownership of the value
    value_wrapper.set_alias();
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  return false;
}


Item *Item_sum_json_object::copy_or_same(THD *thd)
{
  return new (thd->mem_root) Item_sum_json_object(thd, this);
}

/**
  Resolve the fields in the GROUPING function.
  The GROUPING function can only appear in SELECT list or
  in HAVING clause and requires WITH ROLLUP. Check that this holds.
  We also need to check if all the arguments of the function
  are present in GROUP BY clause. As GROUP BY columns are not
  resolved at this time, we do it in SELECT_LEX::resolve_rollup().
  However, if the GROUPING function is found in HAVING clause,
  we can check here. Also, resolve_rollup() does not
  check for items present in HAVING clause.

  @param[in]     thd        current thread
  @param[in,out] ref        reference to place where item is
                            stored
  @retval
    TRUE  if error
  @retval
    FALSE on success

*/
bool Item_func_grouping::fix_fields(THD *thd, Item **ref)
{
  /*
    We do not allow GROUPING by position. However GROUP BY allows
    it for now.
  */
  Item **arg,**arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
  {
    if ((*arg)->type() == Item::INT_ITEM && (*arg)->basic_const_item())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "GROUPING function");
      return true;
    }
  }

  if (Item_func::fix_fields(thd, ref))
    return true;

  /*
    More than 64 args cannot be supported as the bitmask which is
    used to represent the result cannot accomodate.
  */
  if (arg_count > 64)
  {
    my_error(ER_INVALID_NO_OF_ARGS, MYF(0), "GROUPING", arg_count, "64");
    return true;
  }

  /*
    GROUPING() is allowed to be present only in SELECT list and
    HAVING clause.
  */
  SELECT_LEX *select= thd->lex->current_select();

  if (select->olap == UNSPECIFIED_OLAP_TYPE ||
      (select->resolve_place != SELECT_LEX::RESOLVE_SELECT_LIST &&
       select->resolve_place != SELECT_LEX::RESOLVE_HAVING))
  {
    my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
    return true;
  }

  /*
    If GROUPING() is present in HAVING clause, check if all the
    arguments are present in GROUP BY.
  */
  if (select->resolve_place == SELECT_LEX::RESOLVE_HAVING)
  {
    for (uint i= 0; i < arg_count; i++)
    {
      Item *const real_item= args[i]->real_item();
      bool found_in_group= false;

      for (ORDER *group= select->group_list.first; group; group= group->next)
      {
        if (real_item->eq((*group->item)->real_item(), 0))
        {
          found_in_group= true;
          break;
        }
      }
      if (!found_in_group)
      {
        my_error(ER_FIELD_IN_GROUPING_NOT_GROUP_BY, MYF(0), (i+1));
        return true;
      }
    }
  }
  return false;
}

/**
  Evaluation of the GROUPING function.
  We check the type of the item for all the arguments of
  GROUPING function. If it's a NULL_RESULT_ITEM, set the bit for
  the field in the result. The result of the GROUPING function
  would be the integer bit mask having 1's for the arguments
  of type NULL_RESULT_ITEM.

  @return
  integer bit mask having 1's for the arguments which have a
  NULL in their result becuase of ROLLUP operation.
*/
longlong Item_func_grouping::val_int()
{
  longlong result= 0;
  for (uint i= 0; i<arg_count; i++)
  {
    Item *real_item = args[i];
    while (real_item->type() == REF_ITEM)
      real_item = *((down_cast<Item_ref *>(real_item))->ref);
    /*
      Note: if the current input argument is an 'Item_null_result',
      then we know it is generated by rollup handler to fill the
      subtotal rows.
    */
    if (real_item->type() == NULL_RESULT_ITEM)
      result+= 1<<(arg_count-(i+1));
  }
  return result;
}


/**
  This function is expected to check if GROUPING function with
  its arguments is "group-invariant".
  However, GROUPING function produces only one value per
  group similar to the other set functions and the arguments
  to the GROUPING function are always present in GROUP BY (this
  is checked in resolve_rollup() which is called much earlier to
  aggregate_check_group). As a result, aggregate_check_group does
  not have to determine if the result of this function is
  "group-invariant".

  @retval
    TRUE  if error
  @retval
    FALSE on success
*/
bool Item_func_grouping::aggregate_check_group(uchar *arg)
{
  Group_check *gc= reinterpret_cast<Group_check *>(arg);

  if (gc->is_stopped(this))
    return false;

  if (gc->is_fd_on_source(this))
  {
    gc->stop_at(this);
    return false;
  }
  return true;
}


/**
  Resets the aggregation property which was set during creation
  of references to GROUP BY fields in SELECT_LEX::change_group_ref.
  Calls Item_int_func::cleanup() to do the rest of the cleanup.
*/
void Item_func_grouping::cleanup()
{
  reset_aggregation();
  Item_int_func::cleanup();
}
