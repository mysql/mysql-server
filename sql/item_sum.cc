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

#include "item_json_func.h"                // get_atom_null_as_null ..
#include "json_dom.h"
#include "sql_exception_handler.h"         // handle_std_exception
#include "sql_select.h"
#include "sql_tmp_table.h"                 // create_tmp_table
#include "sql_resolver.h"                  // setup_order, fix_inner_refs
#include "sql_optimizer.h"                 // JOIN
#include "uniques.h"
#include "parse_tree_helpers.h"
#include "parse_tree_nodes.h"
#include "aggregate_check.h"

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
  Prepare an aggregate function item for checking context conditions.

    The function initializes the members of the Item_sum object created
    for a set function that are used to check validity of the set function
    occurrence.
    If the set function is not allowed in any subquery where it occurs
    an error is reported immediately.

  @param thd      reference to the thread context info

  @note
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the descent of this traversal.
  @retval
    TRUE   if an error is reported
  @retval
    FALSE  otherwise
*/
 
bool Item_sum::init_sum_func_check(THD *thd)
{
  if (!thd->lex->allow_sum_func)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }
  /* Set a reference to the nesting set function if there is  any */
  in_sum_func= thd->lex->in_sum_func;
  /* Save a pointer to object to be used in items for nested set functions */
  thd->lex->in_sum_func= this;
  nest_level= thd->lex->current_select()->nest_level;
  ref_by= 0;
  aggr_level= -1;
  aggr_sel= NULL;
  max_arg_level= -1;
  max_sum_func_level= -1;
  base_select= thd->lex->current_select();
  return FALSE;
}

/**
  Check constraints imposed on a usage of a set function.

    The method verifies whether context conditions imposed on a usage
    of any set function are met for this occurrence.
    It checks whether the set function occurs in the position where it
    can be aggregated and, when it happens to occur in argument of another
    set function, the method checks that these two functions are aggregated in
    different subqueries.
    If the context conditions are not met the method reports an error.
    If the set function is aggregated in some outer subquery the method
    adds it to the chain of items for such set functions that is attached
    to the the st_select_lex structure for this subquery.

    A number of designated members of the object are used to check the
    conditions. They are specified in the comment before the Item_sum
    class declaration.
    Additionally a bitmap variable called allow_sum_func is employed.
    It is included into the thd->lex structure.
    The bitmap contains 1 at n-th position if the set function happens
    to occur under a construct of the n-th level subquery where usage
    of set functions are allowed (i.e either in the SELECT list or
    in the HAVING clause of the corresponding subquery)
    Consider the query:
    @code
       SELECT SUM(t1.b) FROM t1 GROUP BY t1.a
         HAVING t1.a IN (SELECT t2.c FROM t2 WHERE AVG(t1.b) > 20) AND
                t1.a > (SELECT MIN(t2.d) FROM t2);
    @endcode
    allow_sum_func will contain: 
    - for SUM(t1.b) - 1 at the first position 
    - for AVG(t1.b) - 1 at the first position, 0 at the second position
    - for MIN(t2.d) - 1 at the first position, 1 at the second position.

  @param thd  reference to the thread context info
  @param ref  location of the pointer to this item in the embedding expression

  @note
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the ascent of this traversal.

  @retval
    TRUE   if an error is reported
  @retval
    FALSE  otherwise
*/
 
bool Item_sum::check_sum_func(THD *thd, Item **ref)
{
  bool invalid= FALSE;
  nesting_map allow_sum_func= thd->lex->allow_sum_func;
  /*  
    The value of max_arg_level is updated if an argument of the set function
    contains a column reference resolved  against a subquery whose level is
    greater than the current value of max_arg_level.
    max_arg_level cannot be greater than nest level.
    nest level is always >= 0  
  */
  if (nest_level == max_arg_level)
  {
    /*
      The function must be aggregated in the current subquery, 
      If it is there under a construct where it is not allowed 
      we report an error. 
    */ 
    invalid= !(allow_sum_func & ((nesting_map)1 << max_arg_level));
  }
  else if (max_arg_level >= 0 ||
           !(allow_sum_func & ((nesting_map)1 << nest_level)))
  {
    /*
      The set function can be aggregated only in outer subqueries.
      Try to find a subquery where it can be aggregated;
      If we fail to find such a subquery report an error.
    */
    if (register_sum_func(thd, ref))
      return TRUE;
    invalid= aggr_level < 0 &&
             !(allow_sum_func & ((nesting_map)1 << nest_level));
    if (!invalid && thd->variables.sql_mode & MODE_ANSI)
      invalid= aggr_level < 0 && max_arg_level < nest_level;
  }
  if (!invalid && aggr_level < 0)
  {
    aggr_level= nest_level;
    aggr_sel= thd->lex->current_select();
  }
  /*
    By this moment we either found a subquery where the set function is
    to be aggregated  and assigned a value that is  >= 0 to aggr_level,
    or set the value of 'invalid' to TRUE to report later an error. 
  */
  /* 
    Additionally we have to check whether possible nested set functions
    are acceptable here: they are not, if the level of aggregation of
    some of them is less than aggr_level.
  */
  if (!invalid) 
    invalid= aggr_level <= max_sum_func_level;
  if (invalid)  
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }

  if (in_sum_func)
  {
    /*
      If the set function is nested adjust the value of
      max_sum_func_level for the nesting set function.
      We take into account only enclosed set functions that are to be 
      aggregated on the same level or above of the nest level of 
      the enclosing set function.
      But we must always pass up the max_sum_func_level because it is
      the maximum nested level of all directly and indirectly enclosed
      set functions. We must do that even for set functions that are
      aggregated inside of their enclosing set function's nest level
      because the enclosing function may contain another enclosing
      function that is to be aggregated outside or on the same level
      as its parent's nest level.
    */
    if (in_sum_func->nest_level >= aggr_level)
      set_if_bigger(in_sum_func->max_sum_func_level, aggr_level);
    set_if_bigger(in_sum_func->max_sum_func_level, max_sum_func_level);
  }

  aggr_sel->set_agg_func_used(true);
  if (sum_func() == JSON_AGG_FUNC)
    aggr_sel->set_json_agg_func_used(true);
  update_used_tables();
  thd->lex->in_sum_func= in_sum_func;
  return FALSE;
}

/**
  Attach a set function to the subquery where it must be aggregated.

    The function looks for an outer subquery where the set function must be
    aggregated. If it finds such a subquery then aggr_level is set to
    the nest level of this subquery and the item for the set function
    is added to the list of set functions used in nested subqueries
    inner_sum_func_list defined for each subquery. When the item is placed 
    there the field 'ref_by' is set to ref.

  @note
    Now we 'register' only set functions that are aggregated in outer
    subqueries. Actually it makes sense to link all set function for
    a subquery in one chain. It would simplify the process of 'splitting'
    for set functions.

  @param thd  reference to the thread context info
  @param ref  location of the pointer to this item in the embedding expression

  @retval
    FALSE  if the executes without failures (currently always)
  @retval
    TRUE   otherwise
*/  

bool Item_sum::register_sum_func(THD *thd, Item **ref)
{
  nesting_map allow_sum_func= thd->lex->allow_sum_func;

  // Find the outer-most query block where this function can be aggregated.

  for (SELECT_LEX *sl= thd->lex->current_select()->outer_select() ;
       sl && sl->nest_level >= max_arg_level;
       sl= sl->outer_select() )
  {
    if (allow_sum_func & ((nesting_map)1 << sl->nest_level))
    {
      aggr_level= sl->nest_level;
      aggr_sel= sl;
    }
  }

  if (aggr_level >= 0)
  {
    ref_by= ref;
    /* Add the object to the list of registered objects assigned to aggr_sel */
    if (!aggr_sel->inner_sum_func_list)
      next= this;
    else
    {
      next= aggr_sel->inner_sum_func_list->next;
      aggr_sel->inner_sum_func_list->next= this;
    }
    aggr_sel->inner_sum_func_list= this;
    aggr_sel->with_sum_func= true;

    /* 
      Mark Item_subselect(s) as containing aggregate function all the way up
      to aggregate function's calculation context.
      Note that we must not mark the Item of calculation context itself
      because with_sum_func on the calculation context st_select_lex is
      already set above.

      with_sum_func being set for an Item means that this Item refers 
      (somewhere in it, e.g. one of its arguments if it's a function) directly
      or through intermediate items to an aggregate function that is calculated
      in a context "outside" of the Item (e.g. in the current or outer select).

      with_sum_func being set for an st_select_lex means that this query block
      has aggregate functions directly referenced (i.e. not through a subquery).
    */
    for (SELECT_LEX *sl= thd->lex->current_select(); 
         sl && sl != aggr_sel && sl->master_unit()->item;
         sl= sl->outer_select())
      sl->master_unit()->item->with_sum_func= true;
  }
  thd->lex->current_select()->mark_as_dependent(aggr_sel);
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
  aggr_sel(item->aggr_sel),
  nest_level(item->nest_level), aggr_level(item->aggr_level),
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


void Item_sum::mark_as_sum_func(st_select_lex *cur_select)
{
  cur_select->n_sum_items++;
  cur_select->with_sum_func= 1;
  with_sum_func= 1;
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


void Item_sum::fix_length_and_dec()
{
  maybe_null=1;
  null_value=1;

  const Sumfunctype t= sum_func();

  // None except these 3 types are allowed for geometry arguments.
  if (!(t == COUNT_FUNC || t == COUNT_DISTINCT_FUNC || t == SUM_BIT_FUNC))
    reject_geometry_args(arg_count, args, this);
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
  SELECT_LEX it was moved to by Item_sum::register_sum_func().

  This is done to undo some of the effects of
  Item_sum::register_sum_func() so that the item may be removed from
  the query.

  @note This doesn't completely undo Item_sum::register_sum_func(), as
  with_sum_func information is left untouched. This means that if this
  item is removed, aggr_sel and all Item_subselects between aggr_sel
  and this item may be left with with_sum_func set to true, even if
  there are no aggregation functions. To our knowledge, this has no
  impact on the query result.

  @see Item_sum::register_sum_func()
  @see remove_redundant_subquery_clauses()
 */
bool Item_sum::clean_up_after_removal(uchar *arg)
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
  if (!fixed ||                                                    // 1
      aggr_sel == NULL || aggr_sel->inner_sum_func_list == NULL || // 2
      next == NULL)                                                // 3
    return false;

  if (next == this)
    aggr_sel->inner_sum_func_list= NULL;
  else
  {
    Item_sum *prev;
    for (prev= this; prev->next != this; prev= prev->next)
      ;
    prev->next= next;
    next= NULL;

    if (aggr_sel->inner_sum_func_list == this)
      aggr_sel->inner_sum_func_list= prev;
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
  if (aggr_sel == dc->select)
    return true;

  return false;
}


bool Item_sum::aggregate_check_group(uchar *arg)
{
  DBUG_ASSERT(fixed);
  Group_check *gc= reinterpret_cast<Group_check *>(arg);

  if (gc->is_stopped(this))
    return false;

  if (aggr_sel != gc->select)
  {
    /*
      If aggr_sel is inner to gc's select_lex, this aggregate function might
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


Field *Item_sum::create_tmp_field(bool group, TABLE *table)
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
    with_subselect= false;
    with_stored_program= false;
    for (uint i=0 ; i < arg_count ; i++)
    {
      args[i]->update_used_tables();
      used_tables_cache|= args[i]->used_tables();
      with_subselect|= args[i]->has_subquery();
      with_stored_program|= args[i]->has_stored_program();
    }

    used_tables_cache&= PSEUDO_TABLE_BITS;

    /*
     if the function is aggregated into its local context, it can
     be calculated only after evaluating the full join, thus it
     depends on all tables of this join. Otherwise, it depends on
     outer tables, even if its arguments args[] do not explicitly
     reference an outer table, like COUNT (*) or COUNT(123).
    */
    used_tables_cache|= aggr_level == nest_level ?
      ((table_map)1 << aggr_sel->leaf_table_count) - 1 :
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
  @param    key1    left key image
  @param    key2    right key image
  @return   comparison result
    @retval < 0       if key1 < key2
    @retval = 0       if key1 = key2
    @retval > 0       if key1 > key2
*/

static int simple_str_key_cmp(void* arg, uchar* key1, uchar* key2)
{
  Field *f= (Field*) arg;
  return f->cmp(key1, key2);
}


/**
  Correctly compare composite keys.
 
  Used by the Unique class to compare keys. Will do correct comparisons
  for composite keys with various field types.

  @param arg     Pointer to the relevant Aggregator_distinct instance
  @param key1    left key image
  @param key2    right key image
  @return        comparison result
    @retval <0       if key1 < key2
    @retval =0       if key1 = key2
    @retval >0       if key1 > key2
*/

int Aggregator_distinct::composite_key_cmp(void* arg, uchar* key1, uchar* key2)
{
  Aggregator_distinct *aggr= (Aggregator_distinct *) arg;
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


static int item_sum_distinct_walk(void *element, element_count num_of_dups,
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

  if (item_sum->setup(thd))
    return TRUE;
  if (item_sum->sum_func() == Item_sum::COUNT_FUNC || 
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC)
  {
    List<Item> list;
    SELECT_LEX *select_lex= thd->lex->current_select();

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
      qsort_cmp2 compare_key;
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
        compare_key= (qsort_cmp2) simple_raw_key_cmp;
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
          compare_key= (qsort_cmp2) simple_str_key_cmp;
          cmp_arg= (void*) table->field[0];
          /* tree_key_length has been set already */
        }
        else
        {
          uint32 *length;
          compare_key= (qsort_cmp2) composite_key_cmp;
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


    enum enum_field_types field_type;

    field_type= calc_tmp_field_type(arg->field_type(),
                              arg->result_type());
    field_def.init_for_tmp_table(field_type, 
                                 arg->max_length,
                                 arg->decimals, 
                                 arg->maybe_null,
                                 arg->unsigned_flag);

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


bool
Item_sum_num::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  if (init_sum_func_check(thd))
    return TRUE;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  decimals=0;
  maybe_null=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return TRUE;
    set_if_bigger(decimals, args[i]->decimals);
    maybe_null |= args[i]->maybe_null;
  }
  result_field=0;
  max_length=float_length(decimals);
  null_value=1;
  fix_length_and_dec();

  if (check_sum_func(thd, ref))
    return TRUE;

  fixed= 1;
  return FALSE;
}


bool
Item_sum_hybrid::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  Item *item= args[0];

  if (init_sum_func_check(thd))
    return TRUE;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  // 'item' can be changed during fix_fields
  if ((!item->fixed && item->fix_fields(thd, args)) ||
      (item= args[0])->check_cols(1))
    return TRUE;
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
  setup_hybrid(args[0], NULL);
  /* MIN/MAX can return NULL for empty set indepedent of the used column */
  maybe_null= 1;
  unsigned_flag=item->unsigned_flag;
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  if (thd->is_error())
    return true;
  item= item->real_item();
  if (item->type() == Item::FIELD_ITEM)
    hybrid_field_type= ((Item_field*) item)->field->type();
  else
    hybrid_field_type= Item::field_type();

  if (check_sum_func(thd, ref))
    return TRUE;

  fixed= 1;
  return FALSE;
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

void Item_sum_hybrid::setup_hybrid(Item *item, Item *value_arg)
{
  value= Item_cache::get_cache(item);
  value->setup(item);
  value->store(value_arg);
  arg_cache= Item_cache::get_cache(item);
  arg_cache->setup(item);
  cmp= new Arg_comparator();
  cmp->set_cmp_func(this, (Item**)&arg_cache, (Item**)&value, FALSE);
  collation.set(item->collation);
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
  switch (args[0]->field_type()) {
  case MYSQL_TYPE_DATE:
    field= new Field_newdate(maybe_null, item_name.ptr());
    break;
  case MYSQL_TYPE_TIME:
    field= new Field_timef(maybe_null, item_name.ptr(), decimals);
    break;
  case MYSQL_TYPE_TIMESTAMP:
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


void Item_sum_sum::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_sum::fix_length_and_dec");
  maybe_null=null_value=1;
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

  reject_geometry_args(arg_count, args, this);

  DBUG_PRINT("info", ("Type: %s (%d, %d)",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--"),
                      max_length,
                      (int)decimals));
  DBUG_VOID_RETURN;
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


/*
  Avgerage
*/
void Item_sum_avg::fix_length_and_dec()
{
  Item_sum_sum::fix_length_and_dec();
  maybe_null=null_value=1;
  prec_increment= current_thd->variables.div_precincrement;
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


void Item_sum_variance::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_variance::fix_length_and_dec");
  maybe_null= null_value= 1;

  /*
    According to the SQL2003 standard (Part 2, Foundations; sec 10.9,
    aggregate function; paragraph 7h of Syntax Rules), "the declared 
    type of the result is an implementation-defined aproximate numeric
    type.
  */
  hybrid_type= REAL_RESULT;
  decimals= NOT_FIXED_DEC;
  max_length= float_length(decimals);

  reject_geometry_args(arg_count, args, this);
  DBUG_PRINT("info", ("Type: REAL_RESULT (%d, %d)", max_length, (int)decimals));
  DBUG_VOID_RETURN;
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
  item->setup_hybrid(args[0], value);
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
  item->setup_hybrid(args[0], value);
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


/* bit_or and bit_and */

longlong Item_sum_bit::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return (longlong) bits;
}


void Item_sum_bit::clear()
{
  bits= reset_bits;
}

Item *Item_sum_or::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_or(thd, this);
}


bool Item_sum_or::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits|=value;
  return 0;
}

Item *Item_sum_xor::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_xor(thd, this);
}


bool Item_sum_xor::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits^=value;
  return 0;
}

Item *Item_sum_and::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_and(thd, this);
}


bool Item_sum_and::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits&=value;
  return 0;
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
  int8store(result_field->ptr, bits);
}

void Item_sum_bit::update_field()
{
  uchar *res=result_field->ptr;
  bits= uint8korr(res);
  add();
  int8store(res, bits);
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
  maybe_null=1;
  hybrid_type= res_type;
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
  decimals=item->decimals;
  max_length=item->max_length;
  unsigned_flag= item->unsigned_flag;
  field=item->result_field;
  maybe_null=1;
  sample= item->sample;
  prec_increment= item->prec_increment;
  if ((hybrid_type= item->hybrid_type) == DECIMAL_RESULT)
  {
    f_scale0= item->f_scale0;
    f_precision0= item->f_precision0;
    dec_bin_size0= item->dec_bin_size0;
    f_scale1= item->f_scale1;
    f_precision1= item->f_precision1;
    dec_bin_size1= item->dec_bin_size1;
  }
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

#ifdef HAVE_DLOPEN

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

void Item_sum_udf_str::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_udf_str::fix_length_and_dec");
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  DBUG_VOID_RETURN;
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

#endif /* HAVE_DLOPEN */


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
      return ((order_item)->direction == ORDER::ORDER_ASC) ? res : -res;
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
                        ER_CUT_VALUE_GROUP_CONCAT, ER(ER_CUT_VALUE_GROUP_CONCAT),
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

  @param distinct_arg   distinct
  @param select_list    list of expression for show values
  @param order_list     list of sort columns
  @param separator_arg  string value of separator.
*/

Item_func_group_concat::Item_func_group_concat(const POS &pos,
                       bool distinct_arg, PT_item_list *select_list,
                       PT_order_list *opt_order_list,
                       String *separator_arg)
  :super(pos), tmp_table_param(0), separator(separator_arg), tree(0),
   unique_filter(NULL), table(0),
   order_array(*my_thread_get_THR_MALLOC()),
   arg_count_order(opt_order_list ? opt_order_list->value.elements : 0),
   arg_count_field(select_list->elements()),
   row_count(0),
   distinct(distinct_arg),
   warning_for_row(FALSE),
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
  if (row_eligible && !warning_for_row && tree == NULL && !distinct)
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
  List<Item> list;
  SELECT_LEX *select_lex= thd->lex->current_select();
  const bool order_or_distinct= MY_TEST(arg_count_order > 0 || distinct);
  DBUG_ENTER("Item_func_group_concat::setup");

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
      setup_order(thd, Ref_ptr_array(args, arg_count),
                  context->table_list, list, all_fields, order_array.begin()))
    DBUG_RETURN(TRUE);

  count_field_types(select_lex, tmp_table_param, all_fields, false, true);
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
                                select_lex->active_options(),
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


String* Item_func_group_concat::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;

  if (!m_result_finalized) // Result yet to be written.
  {
    if (tree != NULL) // order by
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
                        ER_CUT_VALUE_GROUP_CONCAT, ER(ER_CUT_VALUE_GROUP_CONCAT),
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
      if (order_array[i].direction == ORDER::ORDER_ASC)
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
  result_field= NULL;

  if (init_sum_func_check(thd))
    return true;

  Disable_semijoin_flattening DSF(thd->lex->current_select(), true);

  for (uint i= 0; i < arg_count; i++)
  {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }
  fix_length_and_dec();

  if (check_sum_func(thd, ref))
    return true;

  max_length= MAX_BLOB_WIDTH;
  maybe_null= true;
  null_value= true;
  fixed= true;
  return false;
}

String *Item_sum_json::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value || m_wrapper.empty())
    return NULL;
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
  Json_dom *dom= m_wrapper.clone_dom();
  Json_wrapper tmp(dom);
  wr->steal(&tmp);

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


bool Item_sum_json::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
{
  if (null_value || m_wrapper.empty())
    return true;

  return m_wrapper.coerce_date(ltime, fuzzydate, func_name());
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

  Json_wrapper tmp(&m_json_array);
  // Set the array to the m_wrapper.
  m_wrapper.steal(&tmp);
  // But let Item_sum_json_array keep the ownership.
  m_wrapper.set_alias();
}


void Item_sum_json_object::clear()
{
  null_value= true;
  m_json_object.clear();

  Json_wrapper tmp(&m_json_object);
  // Set the object to the m_wrapper.
  m_wrapper.steal(&tmp);
  // But let Item_sum_json_object keep the ownership.
  m_wrapper.set_alias();
}


bool Item_sum_json_array::add()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ASSERT(arg_count == 1);

  const THD *thd= current_thd;
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
    Json_array *arr= down_cast<Json_array *>(m_wrapper.to_dom());
    if (arr->append_alias(value_wrapper.to_dom()))
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

  const THD *thd= current_thd;
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
                        func_name(), &safep, &safe_length))
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
    Json_object *object= down_cast<Json_object *>(m_wrapper.to_dom());
    if (object->add_alias(key, value_wrapper.to_dom()))
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
