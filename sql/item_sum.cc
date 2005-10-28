/* Copyright (C) 2000-2003 MySQL AB

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


/* Sum functions (COUNT, MIN...) */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

/* 
  Prepare an aggregate function item for checking context conditions

  SYNOPSIS
    init_sum_func_check()
    thd      reference to the thread context info

  DESCRIPTION
    The function initializes the members of the Item_sum object created
    for a set function that are used to check validity of the set function
    occurrence.
    If the set function is not allowed in any subquery where it occurs
    an error is reported immediately.

  NOTES
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the descent of this traversal.
  
  RETURN
    TRUE   if an error is reported     
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
  nest_level= thd->lex->current_select->nest_level;
  ref_by= 0;
  aggr_level= -1;
  max_arg_level= -1;
  max_sum_func_level= -1;
  return FALSE;
}

/* 
  Check constraints imposed on a usage of a set function

  SYNOPSIS
    check_sum_func()
    thd      reference to the thread context info
    ref      location of the pointer to this item in the embedding expression 

  DESCRIPTION
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

  NOTES
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the ascent of this traversal.

  IMPLEMENTATION
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
      SELECT SUM(t1.b) FROM t1 GROUP BY t1.a
        HAVING t1.a IN (SELECT t2.c FROM t2 WHERE AVG(t1.b) > 20) AND
               t1.a > (SELECT MIN(t2.d) FROM t2);
    allow_sum_func will contain: 
    for SUM(t1.b) - 1 at the first position 
    for AVG(t1.b) - 1 at the first position, 0 at the second position
    for MIN(t2.d) - 1 at the first position, 1 at the second position.

  RETURN
    TRUE   if an error is reported     
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
    invalid= !(allow_sum_func & (1 << max_arg_level));
  }
  else if (max_arg_level >= 0 || !(allow_sum_func & (1 << nest_level)))
  {
    /*
      The set function can be aggregated only in outer subqueries.
      Try to find a subquery where it can be aggregated;
      If we fail to find such a subquery report an error.
    */
    if (register_sum_func(thd, ref))
      return TRUE;
    invalid= aggr_level < 0 && !(allow_sum_func & (1 << nest_level));
  }
  if (!invalid && aggr_level < 0)
    aggr_level= nest_level;
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
  invalid= aggr_level <= max_sum_func_level;
  if (invalid)  
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }
  if (in_sum_func && in_sum_func->nest_level == nest_level)
  {
    /*
      If the set function is nested adjust the value of
      max_sum_func_level for the nesting set function.
    */
    set_if_bigger(in_sum_func->max_sum_func_level, aggr_level);
  }
  thd->lex->in_sum_func= in_sum_func;
  return FALSE;
}

/* 
  Attach a set function to the subquery where it must be aggregated

  SYNOPSIS
    register_sum_func()
    thd      reference to the thread context info
    ref      location of the pointer to this item in the embedding expression
 
  DESCRIPTION
    The function looks for an outer subquery where the set function must be
    aggregated. If it finds such a subquery then aggr_level is set to
    the nest level of this subquery and the item for the set function
    is added to the list of set functions used in nested subqueries
    inner_sum_func_list defined for each subquery. When the item is placed 
    there the field 'ref_by' is set to ref.

  NOTES.
    Now we 'register' only set functions that are aggregated in outer
    subqueries. Actually it makes sense to link all set function for
    a subquery in one chain. It would simplify the process of 'splitting'
    for set functions.

  RETURN
    FALSE  if the executes without failures (currently always)
    TRUE   otherwise
*/  

bool Item_sum::register_sum_func(THD *thd, Item **ref)
{
  SELECT_LEX *sl;
  SELECT_LEX *aggr_sl= NULL;
  nesting_map allow_sum_func= thd->lex->allow_sum_func;
  for (sl= thd->lex->current_select->master_unit()->outer_select() ;
       sl && sl->nest_level > max_arg_level;
       sl= sl->master_unit()->outer_select() )
  {
    if (aggr_level < 0 && (allow_sum_func & (1 << sl->nest_level)))
    {
      /* Found the most nested subquery where the function can be aggregated */
      aggr_level= sl->nest_level;
      aggr_sl= sl;
    }
  }
  if (sl && (allow_sum_func & (1 << sl->nest_level)))
  {
    /* 
      We reached the subquery of level max_arg_level and checked
      that the function can be aggregated here. 
      The set function will be aggregated in this subquery.
    */   
    aggr_level= sl->nest_level;
    aggr_sl= sl;
  }
  if (aggr_level >= 0)
  {
    ref_by= ref;
    /* Add the object to the list of registered objects assigned to aggr_sl */
    if (!aggr_sl->inner_sum_func_list)
      next= this;
    else
    {
      next= aggr_sl->inner_sum_func_list->next;
      aggr_sl->inner_sum_func_list->next= this;
    }
    aggr_sl->inner_sum_func_list= this;
      
  }
  return FALSE;
}


Item_sum::Item_sum(List<Item> &list)
  :arg_count(list.elements)
{
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator_fast<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
    }
  }
  mark_as_sum_func();
  list.empty();					// Fields are used
}


/*
  Constructor used in processing select with temporary tebles
*/

Item_sum::Item_sum(THD *thd, Item_sum *item):
  Item_result_field(thd, item), arg_count(item->arg_count),
  quick_group(item->quick_group)
{
  if (arg_count <= 2)
    args=tmp_args;
  else
    if (!(args= (Item**) thd->alloc(sizeof(Item*)*arg_count)))
      return;
  memcpy(args, item->args, sizeof(Item*)*arg_count);
}


void Item_sum::mark_as_sum_func()
{
  current_thd->lex->current_select->with_sum_func= 1;
  with_sum_func= 1;
}


void Item_sum::make_field(Send_field *tmp_field)
{
  if (args[0]->type() == Item::FIELD_ITEM && keep_field_type())
  {
    ((Item_field*) args[0])->field->make_field(tmp_field);
    tmp_field->db_name=(char*)"";
    tmp_field->org_table_name=tmp_field->table_name=(char*)"";
    tmp_field->org_col_name=tmp_field->col_name=name;
    if (maybe_null)
      tmp_field->flags&= ~NOT_NULL_FLAG;
  }
  else
    init_make_field(tmp_field, field_type());
}


void Item_sum::print(String *str)
{
  str->append(func_name());
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
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

Item *Item_sum::get_tmp_table_item(THD *thd)
{
  Item_sum* sum_item= (Item_sum *) copy_or_same(thd);
  if (sum_item && sum_item->result_field)	   // If not a const sum func
  {
    Field *result_field_tmp= sum_item->result_field;
    for (uint i=0 ; i < sum_item->arg_count ; i++)
    {
      Item *arg= sum_item->args[i];
      if (!arg->const_item())
      {
	if (arg->type() == Item::FIELD_ITEM)
	  ((Item_field*) arg)->field= result_field_tmp++;
	else
	  sum_item->args[i]= new Item_field(result_field_tmp++);
      }
    }
  }
  return sum_item;
}


bool Item_sum::walk (Item_processor processor, byte *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      if ((*arg)->walk(processor, argument))
	return 1;
    }
  }
  return (this->*processor)(argument);
}


Field *Item_sum::create_tmp_field(bool group, TABLE *table,
                                  uint convert_blob_length)
{
  switch (result_type()) {
  case REAL_RESULT:
    return new Field_double(max_length,maybe_null,name,table,decimals);
  case INT_RESULT:
    return new Field_longlong(max_length,maybe_null,name,table,unsigned_flag);
  case STRING_RESULT:
    if (max_length > 255 && convert_blob_length)
      return new Field_varstring(convert_blob_length, maybe_null,
                                 name, table,
                                 collation.collation);
    return make_string_field(table);
  case DECIMAL_RESULT:
    return new Field_new_decimal(max_length, maybe_null, name, table,
                                 decimals, unsigned_flag);
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
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

  decimals=0;
  maybe_null=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (args[i]->fix_fields(thd, args + i) || args[i]->check_cols(1))
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


Item_sum_hybrid::Item_sum_hybrid(THD *thd, Item_sum_hybrid *item)
  :Item_sum(thd, item), value(item->value), hybrid_type(item->hybrid_type),
  hybrid_field_type(item->hybrid_field_type), cmp_sign(item->cmp_sign),
  used_table_cache(item->used_table_cache), was_values(item->was_values)
{
  /* copy results from old value */
  switch (hybrid_type) {
  case INT_RESULT:
    sum_int= item->sum_int;
    break;
  case DECIMAL_RESULT:
    my_decimal2decimal(&item->sum_dec, &sum_dec);
    break;
  case REAL_RESULT:
    sum= item->sum;
    break;
  case STRING_RESULT:
    /*
      This can happen with ROLLUP. Note that the value is already
      copied at function call.
    */
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  collation.set(item->collation);
}

bool
Item_sum_hybrid::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  Item *item= args[0];

  if (init_sum_func_check(thd))
    return TRUE;

  // 'item' can be changed during fix_fields
  if (!item->fixed &&
      item->fix_fields(thd, args) ||
      (item= args[0])->check_cols(1))
    return TRUE;
  decimals=item->decimals;

  switch (hybrid_type= item->result_type()) {
  case INT_RESULT:
    max_length= 20;
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    max_length= item->max_length;
    my_decimal_set_zero(&sum_dec);
    break;
  case REAL_RESULT:
    max_length= float_length(decimals);
    sum= 0.0;
    break;
  case STRING_RESULT:
    max_length= item->max_length;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  };
  /* MIN/MAX can return NULL for empty set indepedent of the used column */
  maybe_null= 1;
  unsigned_flag=item->unsigned_flag;
  collation.set(item->collation);
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  if (item->type() == Item::FIELD_ITEM)
    hybrid_field_type= ((Item_field*) item)->field->type();
  else
    hybrid_field_type= Item::field_type();

  if (check_sum_func(thd, ref))
    return TRUE;

  fixed= 1;
  return FALSE;
}

Field *Item_sum_hybrid::create_tmp_field(bool group, TABLE *table,
					 uint convert_blob_length)
{
  if (args[0]->type() == Item::FIELD_ITEM)
  {
    Field *field= ((Item_field*) args[0])->field;
    
    if ((field= create_tmp_field_from_field(current_thd, field, name, table,
					    NULL, convert_blob_length)))
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
    return new Field_date(maybe_null, name, table, collation.collation);
  case MYSQL_TYPE_TIME:
    return new Field_time(maybe_null, name, table, collation.collation);
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATETIME:
    return new Field_datetime(maybe_null, name, table, collation.collation);
  default:
    break;
  }
  return Item_sum::create_tmp_field(group, table, convert_blob_length);
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

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
  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
  {
    /* SUM result can't be longer than length(arg) + length(MAX_ROWS) */
    int precision= args[0]->decimal_precision() + DECIMAL_LONGLONG_DIGITS;
    max_length= my_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    curr_dec_buff= 0;
    hybrid_type= DECIMAL_RESULT;
    my_decimal_set_zero(dec_buffs);
    break;
  }
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
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
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff^1),
                     val, dec_buffs + curr_dec_buff);
      curr_dec_buff^= 1;
      null_value= 0;
    }
  }
  else
  {
    sum+= args[0]->val_real();
    if (!args[0]->null_value)
      null_value= 0;
  }
  DBUG_RETURN(0);
}


longlong Item_sum_sum::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, unsigned_flag,
                   &result);
    return result;
  }
  return (longlong) val_real();
}


double Item_sum_sum::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
    my_decimal2double(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, &sum);
  return sum;
}


String *Item_sum_sum::val_str(String *str)
{
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


my_decimal *Item_sum_sum::val_decimal(my_decimal *val)
{
  if (hybrid_type == DECIMAL_RESULT)
    return (dec_buffs + curr_dec_buff);
  return val_decimal_from_real(val);
}

/***************************************************************************/

C_MODE_START

/* Declarations for auxilary C-callbacks */

static int simple_raw_key_cmp(void* arg, const void* key1, const void* key2)
{
    return memcmp(key1, key2, *(uint *) arg);
}


static int item_sum_distinct_walk(void *element, element_count num_of_dups,
                                  void *item)
{
    return ((Item_sum_distinct*) (item))->unique_walk_function(element);
}

C_MODE_END

/* Item_sum_distinct */

Item_sum_distinct::Item_sum_distinct(Item *item_arg)
  :Item_sum_num(item_arg), tree(0)
{
  /*
    quick_group is an optimizer hint, which means that GROUP BY can be
    handled with help of index on grouped columns.
    By setting quick_group to zero we force creation of temporary table
    to perform GROUP BY.
  */
  quick_group= 0;
}


Item_sum_distinct::Item_sum_distinct(THD *thd, Item_sum_distinct *original)
  :Item_sum_num(thd, original), val(original->val), tree(0),
  table_field_type(original->table_field_type)
{
  quick_group= 0;
}


/*
  Behaves like an Integer except to fix_length_and_dec().
  Additionally div() converts val with this traits to a val with true
  decimal traits along with conversion of integer value to decimal value.
  This is to speedup SUM/AVG(DISTINCT) evaluation for 8-32 bit integer
  values.
*/
struct Hybrid_type_traits_fast_decimal: public
       Hybrid_type_traits_integer
{
  virtual Item_result type() const { return DECIMAL_RESULT; }
  virtual void fix_length_and_dec(Item *item, Item *arg) const
  { Hybrid_type_traits_decimal::instance()->fix_length_and_dec(item, arg); }

  virtual void div(Hybrid_type *val, ulonglong u) const
  {
    int2my_decimal(E_DEC_FATAL_ERROR, val->integer, 0, val->dec_buf);
    val->used_dec_buf_no= 0;
    val->traits= Hybrid_type_traits_decimal::instance();
    val->traits->div(val, u);
  }
  static const Hybrid_type_traits_fast_decimal *instance();
  Hybrid_type_traits_fast_decimal() {};
};

static const Hybrid_type_traits_fast_decimal fast_decimal_traits_instance;

const Hybrid_type_traits_fast_decimal
  *Hybrid_type_traits_fast_decimal::instance()
{
  return &fast_decimal_traits_instance;
}

void Item_sum_distinct::fix_length_and_dec()
{
  DBUG_ASSERT(args[0]->fixed);

  table_field_type= args[0]->field_type();

  /* Adjust tmp table type according to the chosen aggregation type */
  switch (args[0]->result_type()) {
  case STRING_RESULT:
  case REAL_RESULT:
    val.traits= Hybrid_type_traits::instance();
    if (table_field_type != MYSQL_TYPE_FLOAT)
      table_field_type= MYSQL_TYPE_DOUBLE;
    break;
  case INT_RESULT:
  /*
    Preserving int8, int16, int32 field types gives ~10% performance boost
    as the size of result tree becomes significantly smaller.
    Another speed up we gain by using longlong for intermediate
    calculations. The range of int64 is enough to hold sum 2^32 distinct
    integers each <= 2^32.
  */
  if (table_field_type == MYSQL_TYPE_INT24 ||
      table_field_type >= MYSQL_TYPE_TINY &&
      table_field_type <= MYSQL_TYPE_LONG)
  {
    val.traits= Hybrid_type_traits_fast_decimal::instance();
    break;
  }
  table_field_type= MYSQL_TYPE_LONGLONG;
  /* fallthrough */
  case DECIMAL_RESULT:
    val.traits= Hybrid_type_traits_decimal::instance();
    if (table_field_type != MYSQL_TYPE_LONGLONG)
      table_field_type= MYSQL_TYPE_NEWDECIMAL;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  val.traits->fix_length_and_dec(this, args[0]);
}


bool Item_sum_distinct::setup(THD *thd)
{
  List<create_field> field_list;
  create_field field_def;                              /* field definition */
  DBUG_ENTER("Item_sum_distinct::setup");
  DBUG_ASSERT(tree == 0);

  /*
    Virtual table and the tree are created anew on each re-execution of
    PS/SP. Hence all further allocations are performed in the runtime
    mem_root.
  */
  if (field_list.push_back(&field_def))
    return TRUE;

  null_value= maybe_null= 1;
  quick_group= 0;

  DBUG_ASSERT(args[0]->fixed);

  field_def.init_for_tmp_table(table_field_type, args[0]->max_length,
                               args[0]->decimals, args[0]->maybe_null,
                               args[0]->unsigned_flag);

  if (! (table= create_virtual_tmp_table(thd, field_list)))
    return TRUE;

  /* XXX: check that the case of CHAR(0) works OK */
  tree_key_length= table->s->reclength - table->s->null_bytes;

  /*
    Unique handles all unique elements in a tree until they can't fit
    in.  Then the tree is dumped to the temporary file. We can use
    simple_raw_key_cmp because the table contains numbers only; decimals
    are converted to binary representation as well.
  */
  tree= new Unique(simple_raw_key_cmp, &tree_key_length, tree_key_length,
                   thd->variables.max_heap_table_size);

  DBUG_RETURN(tree == 0);
}


bool Item_sum_distinct::add()
{
  args[0]->save_in_field(table->field[0], FALSE);
  if (!table->field[0]->is_null())
  {
    DBUG_ASSERT(tree);
    null_value= 0;
    /*
      '0' values are also stored in the tree. This doesn't matter
      for SUM(DISTINCT), but is important for AVG(DISTINCT)
    */
    return tree->unique_add(table->field[0]->ptr);
  }
  return 0;
}


bool Item_sum_distinct::unique_walk_function(void *element)
{
  memcpy(table->field[0]->ptr, element, tree_key_length);
  ++count;
  val.traits->add(&val, table->field[0]);
  return 0;
}


void Item_sum_distinct::clear()
{
  DBUG_ENTER("Item_sum_distinct::clear");
  DBUG_ASSERT(tree != 0);                        /* we always have a tree */
  null_value= 1;
  tree->reset();
  DBUG_VOID_RETURN;
}

void Item_sum_distinct::cleanup()
{
  Item_sum_num::cleanup();
  delete tree;
  tree= 0;
  table= 0;
}

Item_sum_distinct::~Item_sum_distinct()
{
  delete tree;
  /* no need to free the table */
}


void Item_sum_distinct::calculate_val_and_count()
{
  count= 0;
  val.traits->set_zero(&val);
  /*
    We don't have a tree only if 'setup()' hasn't been called;
    this is the case of sql_select.cc:return_zero_rows.
  */
  if (tree)
  {
    table->field[0]->set_notnull();
    tree->walk(item_sum_distinct_walk, (void*) this);
  }
}


double Item_sum_distinct::val_real()
{
  calculate_val_and_count();
  return val.traits->val_real(&val);
}


my_decimal *Item_sum_distinct::val_decimal(my_decimal *to)
{
  calculate_val_and_count();
  if (null_value)
    return 0;
  return val.traits->val_decimal(&val, to);
}


longlong Item_sum_distinct::val_int()
{
  calculate_val_and_count();
  return val.traits->val_int(&val, unsigned_flag);
}


String *Item_sum_distinct::val_str(String *str)
{
  calculate_val_and_count();
  if (null_value)
    return 0;
  return val.traits->val_str(&val, str, decimals);
}

/* end of Item_sum_distinct */

/* Item_sum_avg_distinct */

void
Item_sum_avg_distinct::fix_length_and_dec()
{
  Item_sum_distinct::fix_length_and_dec();
  prec_increment= current_thd->variables.div_precincrement;
  /*
    AVG() will divide val by count. We need to reserve digits
    after decimal point as the result can be fractional.
  */
  decimals= min(decimals + prec_increment, NOT_FIXED_DEC);
}


void
Item_sum_avg_distinct::calculate_val_and_count()
{
  Item_sum_distinct::calculate_val_and_count();
  if (count)
    val.traits->div(&val, count);
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
  if (!args[0]->maybe_null)
    count++;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      count++;
  }
  return 0;
}

longlong Item_sum_count::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return (longlong) count;
}


void Item_sum_count::cleanup()
{
  DBUG_ENTER("Item_sum_count::cleanup");
  Item_sum_int::cleanup();
  used_table_cache= ~(table_map) 0;
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
    decimals= min(args[0]->decimals + prec_increment, DECIMAL_MAX_SCALE);
    max_length= my_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    f_precision= min(precision+DECIMAL_LONGLONG_DIGITS, DECIMAL_MAX_PRECISION);
    f_scale=  args[0]->decimals;
    dec_bin_size= my_decimal_get_binary_size(f_precision, f_scale);
  }
  else
    decimals= min(args[0]->decimals + prec_increment, NOT_FIXED_DEC);
}


Item *Item_sum_avg::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_avg(thd, this);
}


Field *Item_sum_avg::create_tmp_field(bool group, TABLE *table,
                                      uint convert_blob_len)
{
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easyest way is to do this is to store both value in a string
      and unpack on access.
    */
    return new Field_string(((hybrid_type == DECIMAL_RESULT) ?
                             dec_bin_size : sizeof(double)) + sizeof(longlong),
                            0, name, table, &my_charset_bin);
  }
  if (hybrid_type == DECIMAL_RESULT)
    return new Field_new_decimal(max_length, maybe_null, name, table,
                                 decimals, unsigned_flag);
  return new Field_double(max_length, maybe_null, name, table, decimals);
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
  if (!args[0]->null_value)
    count++;
  return FALSE;
}

double Item_sum_avg::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return Item_sum_sum::val_real() / ulonglong2double(count);
}


my_decimal *Item_sum_avg::val_decimal(my_decimal *val)
{
  my_decimal sum, cnt;
  const my_decimal *sum_dec;
  DBUG_ASSERT(fixed == 1);
  if (!count)
  {
    null_value=1;
    return NULL;
  }
  sum_dec= Item_sum_sum::val_decimal(&sum);
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &cnt);
  my_decimal_div(E_DEC_FATAL_ERROR, val, sum_dec, &cnt, prec_increment);
  return val;
}


String *Item_sum_avg::val_str(String *str)
{
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
  double tmp= Item_sum_variance::val_real();
  return tmp <= 0.0 ? 0.0 : sqrt(tmp);
}

Item *Item_sum_std::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_std(thd, this);
}


/*
  Variance
*/


Item_sum_variance::Item_sum_variance(THD *thd, Item_sum_variance *item):
  Item_sum_num(thd, item), hybrid_type(item->hybrid_type),
    cur_dec(item->cur_dec), count(item->count), sample(item->sample),
    prec_increment(item->prec_increment)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    memcpy(dec_sum, item->dec_sum, sizeof(item->dec_sum));
    memcpy(dec_sqr, item->dec_sqr, sizeof(item->dec_sqr));
    for (int i=0; i<2; i++)
    {
      dec_sum[i].fix_buffer_pointer();
      dec_sqr[i].fix_buffer_pointer();
    }
  }
  else
  {
    sum= item->sum;
    sum_sqr= item->sum_sqr;
  }
}


void Item_sum_variance::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_variance::fix_length_and_dec");
  maybe_null= null_value= 1;
  prec_increment= current_thd->variables.div_precincrement;
  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    decimals= min(args[0]->decimals + 4, NOT_FIXED_DEC);
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
  {
    int precision= args[0]->decimal_precision()*2 + prec_increment;
    decimals= min(args[0]->decimals + prec_increment, DECIMAL_MAX_SCALE);
    max_length= my_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    cur_dec= 0;
    hybrid_type= DECIMAL_RESULT;
    my_decimal_set_zero(dec_sum);
    my_decimal_set_zero(dec_sqr);

    /*
      The maxium value to usable for variance is DECIMAL_MAX_LENGTH/2
      becasue we need to be able to calculate in dec_bin_size1
      column_value * column_value
    */
    f_scale0= args[0]->decimals;
    f_precision0= min(args[0]->decimal_precision() + DECIMAL_LONGLONG_DIGITS,
                      DECIMAL_MAX_PRECISION);
    f_scale1= min(args[0]->decimals * 2, DECIMAL_MAX_SCALE);
    f_precision1= min(args[0]->decimal_precision()*2 + DECIMAL_LONGLONG_DIGITS,
                      DECIMAL_MAX_PRECISION);
    dec_bin_size0= my_decimal_get_binary_size(f_precision0, f_scale0);
    dec_bin_size1= my_decimal_get_binary_size(f_precision1, f_scale1);
    break;
  }
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("Type: %s (%d, %d)",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--"),
                      max_length,
                      (int)decimals));
  DBUG_VOID_RETURN;
}


Item *Item_sum_variance::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_variance(thd, this);
}


Field *Item_sum_variance::create_tmp_field(bool group, TABLE *table,
                                           uint convert_blob_len)
{
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easyest way is to do this is to store both value in a string
      and unpack on access.
    */
    return new Field_string(((hybrid_type == DECIMAL_RESULT) ?
                             dec_bin_size0 + dec_bin_size1 :
                             sizeof(double)*2) + sizeof(longlong),
                            0, name, table, &my_charset_bin);
  }
  if (hybrid_type == DECIMAL_RESULT)
    return new Field_new_decimal(max_length, maybe_null, name, table,
                                 decimals, unsigned_flag);
  return new Field_double(max_length, maybe_null,name,table,decimals);
}


void Item_sum_variance::clear()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal_set_zero(dec_sum);
    my_decimal_set_zero(dec_sqr);
    cur_dec= 0;
  }
  else
    sum=sum_sqr=0.0; 
  count=0; 
}

bool Item_sum_variance::add()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal dec_buf, *dec= args[0]->val_decimal(&dec_buf);
    my_decimal sqr_buf;
    if (!args[0]->null_value)
    {
      count++;
      int next_dec= cur_dec ^ 1;
      my_decimal_mul(E_DEC_FATAL_ERROR, &sqr_buf, dec, dec);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sqr+next_dec,
                     dec_sqr+cur_dec, &sqr_buf);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sum+next_dec,
                     dec_sum+cur_dec, dec);
      cur_dec= next_dec;
    }
  }
  else
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      sum+=nr;
      sum_sqr+=nr*nr;
      count++;
    }
  }
  return 0;
}

double Item_sum_variance::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  if (count <= sample)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  /* Avoid problems when the precision isn't good enough */
  double tmp=ulonglong2double(count);
  double tmp2= (sum_sqr - sum*sum/tmp)/(tmp - (double)sample);
  return tmp2 <= 0.0 ? 0.0 : tmp2;
}


my_decimal *Item_sum_variance::val_decimal(my_decimal *dec_buf)
{
  my_decimal count_buf, count1_buf, sum_sqr_buf;
  DBUG_ASSERT(fixed ==1 );
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  if (count <= sample)
  {
    null_value= 1;
    return 0;
  }
  null_value= 0;
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &count_buf);
  int2my_decimal(E_DEC_FATAL_ERROR, count-sample, 0, &count1_buf);
  my_decimal_mul(E_DEC_FATAL_ERROR, &sum_sqr_buf,
                 dec_sum+cur_dec, dec_sum+cur_dec);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf,
                 &sum_sqr_buf, &count_buf, prec_increment);
  my_decimal_sub(E_DEC_FATAL_ERROR, &sum_sqr_buf, dec_sqr+cur_dec, dec_buf);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf,
                 &sum_sqr_buf, &count1_buf, prec_increment);
  return dec_buf;
}


void Item_sum_variance::reset_field()
{
  double nr;
  char *res= result_field->ptr;

  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_dec, *arg2_dec;
    longlong tmp;

    arg_dec= args[0]->val_decimal(&value);
    if (args[0]->null_value)
    {
      arg_dec= arg2_dec= &decimal_zero;
      tmp= 0;
    }
    else
    {
      my_decimal_mul(E_DEC_FATAL_ERROR, dec_sum, arg_dec, arg_dec);
      arg2_dec= dec_sum;
      tmp= 1;
    }
    my_decimal2binary(E_DEC_FATAL_ERROR, arg_dec,
                      res, f_precision0, f_scale0);
    my_decimal2binary(E_DEC_FATAL_ERROR, arg2_dec,
                      res+dec_bin_size0, f_precision1, f_scale1);
    res+= dec_bin_size0 + dec_bin_size1;
    int8store(res,tmp);
    return;
  }
  nr= args[0]->val_real();

  if (args[0]->null_value)
    bzero(res,sizeof(double)*2+sizeof(longlong));
  else
  {
    longlong tmp;
    float8store(res,nr);
    nr*=nr;
    float8store(res+sizeof(double),nr);
    tmp= 1;
    int8store(res+sizeof(double)*2,tmp);
  }
}


void Item_sum_variance::update_field()
{
  longlong field_count;
  char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      binary2my_decimal(E_DEC_FATAL_ERROR, res,
                        dec_sum+1, f_precision0, f_scale0);
      binary2my_decimal(E_DEC_FATAL_ERROR, res+dec_bin_size0,
                        dec_sqr+1, f_precision1, f_scale1);
      field_count= sint8korr(res + (dec_bin_size0 + dec_bin_size1));
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sum, arg_val, dec_sum+1);
      my_decimal_mul(E_DEC_FATAL_ERROR, dec_sum+1, arg_val, arg_val);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sqr, dec_sqr+1, dec_sum+1);
      field_count++;
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_sum,
                        res, f_precision0, f_scale0);
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_sqr,
                        res+dec_bin_size0, f_precision1, f_scale1);
      res+= dec_bin_size0 + dec_bin_size1;
      int8store(res, field_count);
    }
    return;
  }

  double nr,old_nr,old_sqr;
  float8get(old_nr, res);
  float8get(old_sqr, res+sizeof(double));
  field_count=sint8korr(res+sizeof(double)*2);

  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    old_nr+=nr;
    old_sqr+=nr*nr;
    field_count++;
  }
  float8store(res,old_nr);
  float8store(res+sizeof(double),old_sqr);
  res+= sizeof(double)*2;
  int8store(res,field_count);
}


/* min & max */

void Item_sum_hybrid::clear()
{
  switch (hybrid_type) {
  case INT_RESULT:
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    my_decimal_set_zero(&sum_dec);
    break;
  case REAL_RESULT:
    sum= 0.0;
    break;
  default:
    value.length(0);
  }
  null_value= 1;
}

double Item_sum_hybrid::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0.0;
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    char *end_not_used;
    int err_not_used;
    String *res;  res=val_str(&str_value);
    return (res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			     &end_not_used, &err_not_used) : 0.0);
  }
  case INT_RESULT:
    if (unsigned_flag)
      return ulonglong2double(sum_int);
    return (double) sum_int;
  case DECIMAL_RESULT:
    my_decimal2double(E_DEC_FATAL_ERROR, &sum_dec, &sum);
    return sum;
  case REAL_RESULT:
    return sum;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
}

longlong Item_sum_hybrid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case INT_RESULT:
    return sum_int;
  case DECIMAL_RESULT:
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, &sum_dec, unsigned_flag, &result);
    return sum_int;
  }
  default:
    return (longlong) Item_sum_hybrid::val_real();
  }
}


my_decimal *Item_sum_hybrid::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case STRING_RESULT:
    string2my_decimal(E_DEC_FATAL_ERROR, &value, val);
    break;
  case REAL_RESULT:
    double2my_decimal(E_DEC_FATAL_ERROR, sum, val);
    break;
  case DECIMAL_RESULT:
    val= &sum_dec;
    break;
  case INT_RESULT:
    int2my_decimal(E_DEC_FATAL_ERROR, sum_int, unsigned_flag, val);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return val;					// Keep compiler happy
}


String *
Item_sum_hybrid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case STRING_RESULT:
    return &value;
  case REAL_RESULT:
    str->set(sum,decimals, &my_charset_bin);
    break;
  case DECIMAL_RESULT:
    my_decimal2string(E_DEC_FATAL_ERROR, &sum_dec, 0, 0, 0, str);
    return str;
  case INT_RESULT:
    if (unsigned_flag)
      str->set((ulonglong) sum_int, &my_charset_bin);
    else
      str->set((longlong) sum_int, &my_charset_bin);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return str;					// Keep compiler happy
}


void Item_sum_hybrid::cleanup()
{
  DBUG_ENTER("Item_sum_hybrid::cleanup");
  Item_sum::cleanup();
  used_table_cache= ~(table_map) 0;

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
  return new (thd->mem_root) Item_sum_min(thd, this);
}


bool Item_sum_min::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,collation.collation) > 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
  break;
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();
    if (!args[0]->null_value && (null_value ||
				 (unsigned_flag && 
				  (ulonglong) nr < (ulonglong) sum_int) ||
				 (!unsigned_flag && nr < sum_int)))
    {
      sum_int=nr;
      null_value=0;
    }
  }
  break;
  case DECIMAL_RESULT:
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value &&
        (null_value || (my_decimal_cmp(&sum_dec, val) > 0)))
    {
      my_decimal2decimal(val, &sum_dec);
      null_value= 0;
    }
  }
  break;
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value && (null_value || nr < sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return 0;
}


Item *Item_sum_max::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_max(thd, this);
}


bool Item_sum_max::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,collation.collation) < 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
  break;
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();
    if (!args[0]->null_value && (null_value ||
				 (unsigned_flag && 
				  (ulonglong) nr > (ulonglong) sum_int) ||
				 (!unsigned_flag && nr > sum_int)))
    {
      sum_int=nr;
      null_value=0;
    }
  }
  break;
  case DECIMAL_RESULT:
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value &&
        (null_value || (my_decimal_cmp(val, &sum_dec) > 0)))
    {
      my_decimal2decimal(val, &sum_dec);
      null_value= 0;
    }
  }
  break;
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value && (null_value || nr > sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
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
  char *res=result_field->ptr;

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
    my_decimal value, *arg_dec= args[0]->val_decimal(&value);

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
  char *res=result_field->ptr;
  longlong nr=0;

  if (!args[0]->maybe_null)
    nr=1;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      nr=1;
  }
  int8store(res,nr);
}


void Item_sum_avg::reset_field()
{
  char *res=result_field->ptr;
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
      bzero(res,sizeof(double)+sizeof(longlong));
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
  reset();
  int8store(result_field->ptr, bits);
}

void Item_sum_bit::update_field()
{
  char *res=result_field->ptr;
  bits= uint8korr(res);
  add();
  int8store(res, bits);
}


/*
** calc next value and merge it with field_value
*/

void Item_sum_sum::update_field()
{
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
    char *res=result_field->ptr;

    float8get(old_nr,res);
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
  char *res=result_field->ptr;

  nr=sint8korr(res);
  if (!args[0]->maybe_null)
    nr++;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      nr++;
  }
  int8store(res,nr);
}


void Item_sum_avg::update_field()
{
  longlong field_count;
  char *res=result_field->ptr;
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
      float8get(old_nr, res);
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


void
Item_sum_hybrid::min_max_update_str_field()
{
  String *res_str=args[0]->val_str(&value);

  if (!args[0]->null_value)
  {
    res_str->strip_sp();
    result_field->val_str(&tmp_value);

    if (result_field->is_null() ||
	(cmp_sign * sortcmp(res_str,&tmp_value,collation.collation)) < 0)
      result_field->store(res_str->ptr(),res_str->length(),res_str->charset());
    result_field->set_notnull();
  }
}


void
Item_sum_hybrid::min_max_update_real_field()
{
  double nr,old_nr;

  old_nr=result_field->val_real();
  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_int_field()
{
  longlong nr,old_nr;

  old_nr=result_field->val_int();
  nr=args[0]->val_int();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
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
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr, unsigned_flag);
}


void
Item_sum_hybrid::min_max_update_decimal_field()
{
  /* TODO: optimize: do not get result_field in case of args[0] is NULL */
  my_decimal old_val, nr_val;
  const my_decimal *old_nr= result_field->val_decimal(&old_val);
  const my_decimal *nr= args[0]->val_decimal(&nr_val);
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
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
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store_decimal(old_nr);
}


Item_avg_field::Item_avg_field(Item_result res_type, Item_sum_avg *item)
{
  name=item->name;
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
  char *res;

  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  float8get(nr,field->ptr);
  res= (field->ptr+sizeof(double));
  count= sint8korr(res);

  if ((null_value= !count))
    return 0.0;
  return nr/(double) count;
}


longlong Item_avg_field::val_int()
{
  return (longlong) val_real();
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
  if (hybrid_type == REAL_RESULT)
  {
    /*
      We can't call Item_variance_field::val_real() on a DECIMAL_RESULT
      as this would call Item_std_field::val_decimal() and we would
      calculate sqrt() twice
    */
    nr= Item_variance_field::val_real();
  }
  else
  {
    my_decimal dec_buf,*dec;
    dec= Item_variance_field::val_decimal(&dec_buf);
    if (!dec)
      nr= 0.0;                                  // NULL; Return 0.0
    else
      my_decimal2double(E_DEC_FATAL_ERROR, dec, &nr);
  }
  return nr <= 0.0 ? 0.0 : sqrt(nr);
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
  nr= nr <= 0.0 ? 0.0 : sqrt(nr);
  double2my_decimal(E_DEC_FATAL_ERROR, nr, &tmp_dec);
  my_decimal_round(E_DEC_FATAL_ERROR, &tmp_dec, decimals, FALSE, dec_buf);
  return dec_buf;
}


Item_variance_field::Item_variance_field(Item_sum_variance *item)
{
  name=item->name;
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

  double sum,sum_sqr;
  longlong count;
  float8get(sum,field->ptr);
  float8get(sum_sqr,(field->ptr+sizeof(double)));
  count=sint8korr(field->ptr+sizeof(double)*2);

  if ((null_value= (count <= sample)))
    return 0.0;

  double tmp= (double) count;
  double tmp2= (sum_sqr - sum*sum/tmp)/(tmp - (double)sample);
  return tmp2 <= 0.0 ? 0.0 : tmp2;
}


String *Item_variance_field::val_str(String *str)
{
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


my_decimal *Item_variance_field::val_decimal(my_decimal *dec_buf)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  longlong count= sint8korr(field->ptr+dec_bin_size0+dec_bin_size1);
  if ((null_value= (count <= sample)))
    return 0;

  my_decimal dec_count, dec1_count, dec_sum, dec_sqr, tmp;
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &dec_count);
  int2my_decimal(E_DEC_FATAL_ERROR, count-sample, 0, &dec1_count);
  binary2my_decimal(E_DEC_FATAL_ERROR, field->ptr,
                    &dec_sum, f_precision0, f_scale0);
  binary2my_decimal(E_DEC_FATAL_ERROR, field->ptr+dec_bin_size0,
                    &dec_sqr, f_precision1, f_scale1);
  my_decimal_mul(E_DEC_FATAL_ERROR, &tmp, &dec_sum, &dec_sum);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &tmp, &dec_count, prec_increment);
  my_decimal_sub(E_DEC_FATAL_ERROR, &dec_sum, &dec_sqr, dec_buf);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf,
                 &dec_sum, &dec1_count, prec_increment);
  return dec_buf;
}


/****************************************************************************
** COUNT(DISTINCT ...)
****************************************************************************/

int simple_str_key_cmp(void* arg, byte* key1, byte* key2)
{
  Field *f= (Field*) arg;
  return f->cmp((const char*)key1, (const char*)key2);
}

/*
  Did not make this one static - at least gcc gets confused when
  I try to declare a static function as a friend. If you can figure
  out the syntax to make a static function a friend, make this one
  static
*/

int composite_key_cmp(void* arg, byte* key1, byte* key2)
{
  Item_sum_count_distinct* item = (Item_sum_count_distinct*)arg;
  Field **field    = item->table->field;
  Field **field_end= field + item->table->s->fields;
  uint32 *lengths=item->field_lengths;
  for (; field < field_end; ++field)
  {
    Field* f = *field;
    int len = *lengths++;
    int res = f->cmp((char *) key1, (char *) key2);
    if (res)
      return res;
    key1 += len;
    key2 += len;
  }
  return 0;
}


C_MODE_START

static int count_distinct_walk(void *elem, element_count count, void *arg)
{
  (*((ulonglong*)arg))++;
  return 0;
}

C_MODE_END


void Item_sum_count_distinct::cleanup()
{
  DBUG_ENTER("Item_sum_count_distinct::cleanup");
  Item_sum_int::cleanup();

  /* Free objects only if we own them. */
  if (!original)
  {
    /*
      We need to delete the table and the tree in cleanup() as
      they were allocated in the runtime memroot. Using the runtime
      memroot reduces memory footprint for PS/SP and simplifies setup().
    */
    delete tree;
    tree= 0;
    if (table)
    {
      free_tmp_table(table->in_use, table);
      table= 0;
    }
    delete tmp_table_param;
    tmp_table_param= 0;
  }
  always_null= FALSE;
  DBUG_VOID_RETURN;
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_sum_count_distinct::make_unique()
{
  table=0;
  original= 0;
  tree= 0;
  tmp_table_param= 0;
  always_null= FALSE;
}


Item_sum_count_distinct::~Item_sum_count_distinct()
{
  cleanup();
}


bool Item_sum_count_distinct::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= thd->lex->current_select;

  /*
    Setup can be called twice for ROLLUP items. This is a bug.
    Please add DBUG_ASSERT(tree == 0) here when it's fixed.
  */
  if (tree || table || tmp_table_param)
    return FALSE;

  if (!(tmp_table_param= new TMP_TABLE_PARAM))
    return TRUE;

  /* Create a table with an unique key over all parameters */
  for (uint i=0; i < arg_count ; i++)
  {
    Item *item=args[i];
    if (list.push_back(item))
      return TRUE;                              // End of memory
    if (item->const_item())
    {
      (void) item->val_int();
      if (item->null_value)
	always_null=1;
    }
  }
  if (always_null)
    return FALSE;
  count_field_types(tmp_table_param,list,0);
  DBUG_ASSERT(table == 0);
  if (!(table= create_tmp_table(thd, tmp_table_param, list, (ORDER*) 0, 1,
				0,
				(select_lex->options | thd->options),
				HA_POS_ERROR, (char*)"")))
    return TRUE;
  table->file->extra(HA_EXTRA_NO_ROWS);		// Don't update rows
  table->no_rows=1;

  if (table->s->db_type == DB_TYPE_HEAP)
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
      if (!f->binary() && (type == MYSQL_TYPE_STRING ||
                           type == MYSQL_TYPE_VAR_STRING ||
                           type == MYSQL_TYPE_VARCHAR))
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
                     thd->variables.max_heap_table_size);
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


Item *Item_sum_count_distinct::copy_or_same(THD* thd) 
{
  return new (thd->mem_root) Item_sum_count_distinct(thd, this);
}


void Item_sum_count_distinct::clear()
{
  /* tree and table can be both null only if always_null */
  if (tree)
    tree->reset();
  else if (table)
  {
    table->file->extra(HA_EXTRA_NO_CACHE);
    table->file->delete_all_rows();
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  }
}

bool Item_sum_count_distinct::add()
{
  int error;
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  copy_funcs(tmp_table_param->items_to_copy);

  for (Field **field=table->field ; *field ; field++)
    if ((*field)->is_real_null(0))
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
  if ((error= table->file->write_row(table->record[0])) &&
      error != HA_ERR_FOUND_DUPP_KEY &&
      error != HA_ERR_FOUND_DUPP_UNIQUE)
    return TRUE;
  return FALSE;
}


longlong Item_sum_count_distinct::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (!table)					// Empty query
    return LL(0);
  if (tree)
  {
    ulonglong count;

    if (tree->elements == 0)
      return (longlong) tree->elements_in_tree(); // everything fits in memory
    count= 0;
    tree->walk(count_distinct_walk, (void*) &count);
    return (longlong) count;
  }
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  return table->file->records;
}


/****************************************************************************
** Functions to handle dynamic loadable aggregates
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Adapted for UDAs by: Andreas F. Bobak <bobak@relog.ch>.
** Rewritten by: Monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

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


void Item_udf_sum::print(String *str)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
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


/* Default max_length is max argument length */

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
   DISTINCT and ORDER BY only works if ORDER BY uses all fields and only fields
   in expression list
   Blobs doesn't work with DISTINCT or ORDER BY
*****************************************************************************/

/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,...)
*/

int group_concat_key_cmp_with_distinct(void* arg, byte* key1,
				       byte* key2)
{
  Item_func_group_concat* grp_item= (Item_func_group_concat*)arg;
  TABLE *table= grp_item->table;
  Item **field_item, **end;

  for (field_item= grp_item->args, end= field_item + grp_item->arg_count_field;
       field_item < end;
       field_item++)
  {
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= (*field_item)->get_tmp_table_field();
    /* 
      If field_item is a const item then either get_tp_table_field returns 0
      or it is an item over a const table. 
    */
    if (field && !(*field_item)->const_item())
    {
      int res;
      uint offset= field->offset() - table->s->null_bytes;
      if ((res= field->cmp((char *) key1 + offset, (char *) key2 + offset)))
	return res;
    }
  }
  return 0;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(expr,... ORDER BY col,... )
*/

int group_concat_key_cmp_with_order(void* arg, byte* key1, byte* key2)
{
  Item_func_group_concat* grp_item= (Item_func_group_concat*) arg;
  ORDER **order_item, **end;
  TABLE *table= grp_item->table;

  for (order_item= grp_item->order, end=order_item+ grp_item->arg_count_order;
       order_item < end;
       order_item++)
  {
    Item *item= *(*order_item)->item;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= item->get_tmp_table_field();
    /* 
      If item is a const item then either get_tp_table_field returns 0
      or it is an item over a const table. 
    */
    if (field && !item->const_item())
    {
      int res;
      uint offset= field->offset() - table->s->null_bytes;
      if ((res= field->cmp((char *) key1 + offset, (char *) key2 + offset)))
        return (*order_item)->asc ? res : -res;
    }
  }
  /*
    We can't return 0 because in that case the tree class would remove this
    item as double value. This would cause problems for case-changes and
    if the returned values are not the same we do the sort on.
  */
  return 1;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,... ORDER BY col,... )

  BUG:
    This doesn't work in the case when the order by contains data that
    is not part of the field list because tree-insert will not notice
    the duplicated values when inserting things sorted by ORDER BY
*/

int group_concat_key_cmp_with_distinct_and_order(void* arg,byte* key1,
						 byte* key2)
{
  if (!group_concat_key_cmp_with_distinct(arg,key1,key2))
    return 0;
  return(group_concat_key_cmp_with_order(arg,key1,key2));
}


/*
  Append data from current leaf to item->result
*/

int dump_leaf_key(byte* key, element_count count __attribute__((unused)),
                  Item_func_group_concat *item)
{
  TABLE *table= item->table;
  String tmp((char *)table->record[1], table->s->reclength,
             default_charset_info);
  String tmp2;
  String *result= &item->result;
  Item **arg= item->args, **arg_end= item->args + item->arg_count_field;

  if (item->no_appended)
    item->no_appended= FALSE;
  else
    result->append(*item->separator);

  tmp.length(0);

  for (; arg < arg_end; arg++)
  {
    String *res;
    if (! (*arg)->const_item())
    {
      /*
	We have to use get_tmp_table_field() instead of
	real_item()->get_tmp_table_field() because we want the field in
	the temporary table, not the original field
        We also can't use table->field array to access the fields
        because it contains both order and arg list fields.
      */
      Field *field= (*arg)->get_tmp_table_field();
      uint offset= field->offset() - table->s->null_bytes;
      DBUG_ASSERT(offset < table->s->reclength);
      res= field->val_str(&tmp, (char *) key + offset);
    }
    else
      res= (*arg)->val_str(&tmp);
    if (res)
      result->append(*res);
  }

  /* stop if length of result more than max_length */
  if (result->length() > item->max_length)
  {
    item->count_cut_values++;
    result->length(item->max_length);
    item->warning_for_row= TRUE;
    return 1;
  }
  return 0;
}


/*
  Constructor of Item_func_group_concat
  distinct_arg - distinct
  select_list - list of expression for show values
  order_list - list of sort columns
  separator_arg - string value of separator
*/

Item_func_group_concat::
Item_func_group_concat(Name_resolution_context *context_arg,
                       bool distinct_arg, List<Item> *select_list,
                       SQL_LIST *order_list, String *separator_arg)
  :tmp_table_param(0), warning(0),
   separator(separator_arg), tree(0), table(0),
   order(0), context(context_arg),
   arg_count_order(order_list ? order_list->elements : 0),
   arg_count_field(select_list->elements),
   count_cut_values(0),
   distinct(distinct_arg),
   warning_for_row(FALSE),
   original(0)
{
  Item *item_select;
  Item **arg_ptr;

  quick_group= FALSE;
  arg_count= arg_count_field + arg_count_order;

  /*
    We need to allocate:
    args - arg_count_field+arg_count_order
           (for possible order items in temporare tables)
    order - arg_count_order
  */
  if (!(args= (Item**) sql_alloc(sizeof(Item*) * arg_count +
                                 sizeof(ORDER*)*arg_count_order)))
    return;

  order= (ORDER**)(args + arg_count);

  /* fill args items of show and sort */
  List_iterator_fast<Item> li(*select_list);

  for (arg_ptr=args ; (item_select= li++) ; arg_ptr++)
    *arg_ptr= item_select;

  if (arg_count_order)
  {
    ORDER **order_ptr= order;
    for (ORDER *order_item= (ORDER*) order_list->first;
         order_item != NULL;
         order_item= order_item->next)
    {
      (*order_ptr++)= order_item;
      *arg_ptr= *order_item->item;
      order_item->item= arg_ptr++;
    }
  }
}


Item_func_group_concat::Item_func_group_concat(THD *thd,
                                               Item_func_group_concat *item)
  :Item_sum(thd, item),
  tmp_table_param(item->tmp_table_param),
  warning(item->warning),
  separator(item->separator),
  tree(item->tree),
  table(item->table),
  order(item->order),
  context(item->context),
  arg_count_order(item->arg_count_order),
  arg_count_field(item->arg_count_field),
  count_cut_values(item->count_cut_values),
  distinct(item->distinct),
  warning_for_row(item->warning_for_row),
  always_null(item->always_null),
  original(item)
{
  quick_group= item->quick_group;
}



void Item_func_group_concat::cleanup()
{
  THD *thd= current_thd;

  DBUG_ENTER("Item_func_group_concat::cleanup");
  Item_sum::cleanup();

  /* Adjust warning message to include total number of cut values */
  if (warning)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    sprintf(warn_buff, ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
    warning->set_msg(thd, warn_buff);
    warning= 0;
  }

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
      free_tmp_table(thd, table);
      table= 0;
      if (tree)
      {
        delete_tree(tree);
        tree= 0;
      }
      if (warning)
      {
        char warn_buff[MYSQL_ERRMSG_SIZE];
        sprintf(warn_buff, ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
        warning->set_msg(thd, warn_buff);
        warning= 0;
      }
    }
    DBUG_ASSERT(tree == 0);
    DBUG_ASSERT(warning == 0);
  }
  DBUG_VOID_RETURN;
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
  no_appended= TRUE;
  if (tree)
    reset_tree(tree);
  /* No need to reset the table as we never call write_row */
}


bool Item_func_group_concat::add()
{
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  copy_funcs(tmp_table_param->items_to_copy);

  for (uint i= 0; i < arg_count_field; i++)
  {
    Item *show_item= args[i];
    if (!show_item->const_item())
    {
      Field *f= show_item->get_tmp_table_field();
      if (f->is_null_in_record((const uchar*) table->record[0]))
        return 0;                               // Skip row if it contains null
    }
  }

  null_value= FALSE;

  TREE_ELEMENT *el= 0;                          // Only for safety
  if (tree)
    el= tree_insert(tree, table->record[0] + table->s->null_bytes, 0,
                    tree->custom_arg);
  /*
    If the row is not a duplicate (el->count == 1)
    we can dump the row here in case of GROUP_CONCAT(DISTINCT...)
    instead of doing tree traverse later.
  */
  if (result.length() <= max_length &&
      !warning_for_row &&
      (!tree || (el->count == 1 && distinct && !arg_count_order)))
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

  if (agg_item_charsets(collation, func_name(),
                        args, arg_count, MY_COLL_ALLOW_CONV))
    return 1;

  result.set_charset(collation.collation);
  result_field= 0;
  null_value= 1;
  max_length= thd->variables.group_concat_max_len;

  if (check_sum_func(thd, ref))
    return TRUE;

  fixed= 1;
  return FALSE;
}


bool Item_func_group_concat::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= thd->lex->current_select;
  qsort_cmp2 compare_key;
  DBUG_ENTER("Item_func_group_concat::setup");

  /*
    Currently setup() can be called twice. Please add
    assertion here when this is fixed.
  */
  if (table || tree)
    DBUG_RETURN(FALSE);

  if (!(tmp_table_param= new TMP_TABLE_PARAM))
    DBUG_RETURN(TRUE);

  /* We'll convert all blobs to varchar fields in the temporary table */
  tmp_table_param->convert_blob_length= max_length;
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
      setup_order(thd, args, context->table_list, list, all_fields, *order))
    DBUG_RETURN(TRUE);

  count_field_types(tmp_table_param,all_fields,0);
  DBUG_ASSERT(table == 0);
  /*
    We have to create a temporary table to get descriptions of fields
    (types, sizes and so on).

    Note that in the table, we first have the ORDER BY fields, then the
    field list.

    We need to set set_sum_field in true for storing value of blob in buffer
    of a record instead of a pointer of one.
  */
  if (!(table= create_tmp_table(thd, tmp_table_param, all_fields,
                                (ORDER*) 0, 0, TRUE,
                                (select_lex->options | thd->options),
                                HA_POS_ERROR, (char*) "")))
    DBUG_RETURN(TRUE);
  table->file->extra(HA_EXTRA_NO_ROWS);
  table->no_rows= 1;


  if (distinct || arg_count_order)
  {
    /*
      Need sorting: init tree and choose a function to sort.
      Don't reserve space for NULLs: if any of gconcat arguments is NULL,
      the row is not added to the result.
    */
    uint tree_key_length= table->s->reclength - table->s->null_bytes;

    tree= &tree_base;
    if (arg_count_order)
    {
      if (distinct)
        compare_key= (qsort_cmp2) group_concat_key_cmp_with_distinct_and_order;
      else
        compare_key= (qsort_cmp2) group_concat_key_cmp_with_order;
    }
    else
    {
      compare_key= (qsort_cmp2) group_concat_key_cmp_with_distinct;
    }
    /*
      Create a tree for sorting. The tree is used to sort and to remove
      duplicate values (according to the syntax of this function). If there
      is no DISTINCT or ORDER BY clauses, we don't create this tree.
    */
    init_tree(tree, min(thd->variables.max_heap_table_size,
                        thd->variables.sortbuff_size/16), 0,
              tree_key_length, compare_key, 0, NULL, (void*) this);
  }

  DBUG_RETURN(FALSE);
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_func_group_concat::make_unique()
{
  tmp_table_param= 0;
  table=0;
  original= 0;
  tree= 0;
}


String* Item_func_group_concat::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  if (count_cut_values && !warning)
  {
    /*
      ER_CUT_VALUE_GROUP_CONCAT needs an argument, but this gets set in
      Item_func_group_concat::cleanup().
    */
    DBUG_ASSERT(table);
    warning= push_warning(table->in_use, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_CUT_VALUE_GROUP_CONCAT,
                          ER(ER_CUT_VALUE_GROUP_CONCAT));
  }
  if (result.length())
    return &result;
  if (tree)
    tree_walk(tree, (tree_walk_action)&dump_leaf_key, (void*)this,
              left_root_right);
  return &result;
}


void Item_func_group_concat::print(String *str)
{
  str->append("group_concat(", 13);
  if (distinct)
    str->append("distinct ", 9);
  for (uint i= 0; i < arg_count_field; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  if (arg_count_order)
  {
    str->append(" order by ", 10);
    for (uint i= 0 ; i < arg_count_order ; i++)
    {
      if (i)
        str->append(',');
      (*order[i]->item)->print(str);
    }
  }
  str->append(" separator \'", 12);
  str->append(*separator);
  str->append("\')", 2);
}
