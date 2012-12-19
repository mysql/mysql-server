/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif
#include "mysql_priv.h"
#include <mysql.h>
#include <m_ctype.h>
#include "my_dir.h"
#include "sp_rcontext.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_select.h"

const String my_null_string("NULL", 4, default_charset_info);

static int save_field_in_field(Field *from, bool *null_value,
                               Field *to, bool no_conversions);


/**
  Compare two Items for List<Item>::add_unique()
*/

bool cmp_items(Item *a, Item *b)
{
  return a->eq(b, FALSE);
}

/****************************************************************************/

/* Hybrid_type_traits {_real} */

void Hybrid_type_traits::fix_length_and_dec(Item *item, Item *arg) const
{
  item->decimals= NOT_FIXED_DEC;
  item->max_length= item->float_length(arg->decimals);
}

static const Hybrid_type_traits real_traits_instance;

const Hybrid_type_traits *Hybrid_type_traits::instance()
{
  return &real_traits_instance;
}


my_decimal *
Hybrid_type_traits::val_decimal(Hybrid_type *val, my_decimal *to) const
{
  double2my_decimal(E_DEC_FATAL_ERROR, val->real, val->dec_buf);
  return val->dec_buf;
}


String *
Hybrid_type_traits::val_str(Hybrid_type *val, String *to, uint8 decimals) const
{
  to->set_real(val->real, decimals, &my_charset_bin);
  return to;
}

/* Hybrid_type_traits_decimal */
static const Hybrid_type_traits_decimal decimal_traits_instance;

const Hybrid_type_traits_decimal *Hybrid_type_traits_decimal::instance()
{
  return &decimal_traits_instance;
}


void
Hybrid_type_traits_decimal::fix_length_and_dec(Item *item, Item *arg) const
{
  item->decimals= arg->decimals;
  item->max_length= min(arg->max_length + DECIMAL_LONGLONG_DIGITS,
                        DECIMAL_MAX_STR_LENGTH);
}


void Hybrid_type_traits_decimal::set_zero(Hybrid_type *val) const
{
  my_decimal_set_zero(&val->dec_buf[0]);
  val->used_dec_buf_no= 0;
}


void Hybrid_type_traits_decimal::add(Hybrid_type *val, Field *f) const
{
  my_decimal_add(E_DEC_FATAL_ERROR,
                 &val->dec_buf[val->used_dec_buf_no ^ 1],
                 &val->dec_buf[val->used_dec_buf_no],
                 f->val_decimal(&val->dec_buf[2]));
  val->used_dec_buf_no^= 1;
}


/**
  @todo
  what is '4' for scale?
*/
void Hybrid_type_traits_decimal::div(Hybrid_type *val, ulonglong u) const
{
  int2my_decimal(E_DEC_FATAL_ERROR, u, TRUE, &val->dec_buf[2]);
  /* XXX: what is '4' for scale? */
  my_decimal_div(E_DEC_FATAL_ERROR,
                 &val->dec_buf[val->used_dec_buf_no ^ 1],
                 &val->dec_buf[val->used_dec_buf_no],
                 &val->dec_buf[2], 4);
  val->used_dec_buf_no^= 1;
}


longlong
Hybrid_type_traits_decimal::val_int(Hybrid_type *val, bool unsigned_flag) const
{
  longlong result;
  my_decimal2int(E_DEC_FATAL_ERROR, &val->dec_buf[val->used_dec_buf_no],
                 unsigned_flag, &result);
  return result;
}


double
Hybrid_type_traits_decimal::val_real(Hybrid_type *val) const
{
  my_decimal2double(E_DEC_FATAL_ERROR, &val->dec_buf[val->used_dec_buf_no],
                    &val->real);
  return val->real;
}


String *
Hybrid_type_traits_decimal::val_str(Hybrid_type *val, String *to,
                                    uint8 decimals) const
{
  my_decimal_round(E_DEC_FATAL_ERROR, &val->dec_buf[val->used_dec_buf_no],
                   decimals, FALSE, &val->dec_buf[2]);
  my_decimal2string(E_DEC_FATAL_ERROR, &val->dec_buf[2], 0, 0, 0, to);
  return to;
}

/* Hybrid_type_traits_integer */
static const Hybrid_type_traits_integer integer_traits_instance;

const Hybrid_type_traits_integer *Hybrid_type_traits_integer::instance()
{
  return &integer_traits_instance;
}

void
Hybrid_type_traits_integer::fix_length_and_dec(Item *item, Item *arg) const
{
  item->decimals= 0;
  item->max_length= MY_INT64_NUM_DECIMAL_DIGITS;
  item->unsigned_flag= 0;
}

/*****************************************************************************
** Item functions
*****************************************************************************/

/**
  Init all special items.
*/

void item_init(void)
{
  item_user_lock_init();
  uuid_short_init();
}


/**
  @todo
    Make this functions class dependent
*/

bool Item::val_bool()
{
  switch(result_type()) {
  case INT_RESULT:
    return val_int() != 0;
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *val= val_decimal(&decimal_value);
    if (val)
      return !my_decimal_is_zero(val);
    return 0;
  }
  case REAL_RESULT:
  case STRING_RESULT:
    return val_real() != 0.0;
  case ROW_RESULT:
  case TIME_RESULT:
  case IMPOSSIBLE_RESULT:
    DBUG_ASSERT(0);
    return 0;                                   // Wrong (but safe)
  }
  return 0;                                   // Wrong (but safe)
}


String *Item::val_string_from_real(String *str)
{
  double nr= val_real();
  if (null_value)
    return 0;					/* purecov: inspected */
  str->set_real(nr,decimals, &my_charset_bin);
  return str;
}


String *Item::val_string_from_int(String *str)
{
  longlong nr= val_int();
  if (null_value)
    return 0;
  str->set_int(nr, unsigned_flag, &my_charset_bin);
  return str;
}


String *Item::val_string_from_decimal(String *str)
{
  my_decimal dec_buf, *dec= val_decimal(&dec_buf);
  if (null_value)
    return 0;
  my_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, FALSE, &dec_buf);
  my_decimal2string(E_DEC_FATAL_ERROR, &dec_buf, 0, 0, 0, str);
  return str;
}


String *Item::val_string_from_date(String *str)
{
  MYSQL_TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE) ||
      str->alloc(MAX_DATE_STRING_REP_LENGTH))
  {
    null_value= 1;
    return (String *) 0;
  }
  str->length(my_TIME_to_str(&ltime, const_cast<char*>(str->ptr()), decimals));
  str->set_charset(&my_charset_bin);
  return str;
}


my_decimal *Item::val_decimal_from_real(my_decimal *decimal_value)
{
  double nr= val_real();
  if (null_value)
    return 0;
  double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return (decimal_value);
}


my_decimal *Item::val_decimal_from_int(my_decimal *decimal_value)
{
  longlong nr= val_int();
  if (null_value)
    return 0;
  int2my_decimal(E_DEC_FATAL_ERROR, nr, unsigned_flag, decimal_value);
  return decimal_value;
}


my_decimal *Item::val_decimal_from_string(my_decimal *decimal_value)
{
  String *res;

  if (!(res= val_str(&str_value)))
    return 0;

  if (str2my_decimal(E_DEC_FATAL_ERROR & ~E_DEC_BAD_NUM,
                     res->ptr(), res->length(), res->charset(),
                     decimal_value) & E_DEC_BAD_NUM)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "DECIMAL",
                        str_value.c_ptr());
  }
  return decimal_value;
}


my_decimal *Item::val_decimal_from_date(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE))
  {
    my_decimal_set_zero(decimal_value);
    null_value= 1;                               // set NULL, stop processing
    return 0;
  }
  return date2my_decimal(&ltime, decimal_value);
}


my_decimal *Item::val_decimal_from_time(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  MYSQL_TIME ltime;
  if (get_time(&ltime))
  {
    my_decimal_set_zero(decimal_value);
    return 0;
  }
  return date2my_decimal(&ltime, decimal_value);
}


double Item::val_real_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  double result;
  my_decimal value_buff, *dec_val= val_decimal(&value_buff);
  if (null_value)
    return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, dec_val, &result);
  return result;
}


longlong Item::val_int_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  longlong result;
  my_decimal value, *dec_val= val_decimal(&value);
  if (null_value)
    return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, dec_val, unsigned_flag, &result);
  return result;
}

int Item::save_time_in_field(Field *field)
{
  MYSQL_TIME ltime;
  if (get_time(&ltime))
    return set_field_to_null_with_conversions(field, 0);
  field->set_notnull();
  return field->store_time_dec(&ltime, decimals);
}


int Item::save_date_in_field(Field *field)
{
  MYSQL_TIME ltime;
  if (get_date(&ltime, TIME_FUZZY_DATE))
    return set_field_to_null_with_conversions(field, 0);
  field->set_notnull();
  return field->store_time_dec(&ltime, decimals);
}


/*
  Store the string value in field directly

  SYNOPSIS
    Item::save_str_value_in_field()
    field   a pointer to field where to store
    result  the pointer to the string value to be stored

  DESCRIPTION
    The method is used by Item_*::save_in_field implementations
    when we don't need to calculate the value to store
    See Item_string::save_in_field() implementation for example

  IMPLEMENTATION
    Check if the Item is null and stores the NULL or the
    result value in the field accordingly.

  RETURN
    Nonzero value if error
*/

int Item::save_str_value_in_field(Field *field, String *result)
{
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(result->ptr(), result->length(),
		      collation.collation);
}


Item::Item():
  is_expensive_cache(-1), rsize(0), name(0), orig_name(0), name_length(0),
  fixed(0), is_autogenerated_name(TRUE),
  collation(&my_charset_bin, DERIVATION_COERCIBLE)
{
  marker= 0;
  maybe_null=null_value=with_sum_func=with_field=unsigned_flag=0;
  in_rollup= 0;
  decimals= 0; max_length= 0;
  with_subselect= 0;
  cmp_context= IMPOSSIBLE_RESULT;
   /* Initially this item is not attached to any JOIN_TAB. */
  join_tab_idx= MAX_TABLES;

  /* Put item in free list so that we can free all items at end */
  THD *thd= current_thd;
  next= thd->free_list;
  thd->free_list= this;
  /*
    Item constructor can be called during execution other then SQL_COM
    command => we should check thd->lex->current_select on zero (thd->lex
    can be uninitialised)
  */
  if (thd->lex->current_select)
  {
    enum_parsing_place place= 
      thd->lex->current_select->parsing_place;
    if (place == SELECT_LIST ||
	place == IN_HAVING)
      thd->lex->current_select->select_n_having_items++;
  }
}

/**
  Constructor used by Item_field, Item_ref & aggregate (sum)
  functions.

  Used for duplicating lists in processing queries with temporary
  tables.
*/
Item::Item(THD *thd, Item *item):
  join_tab_idx(item->join_tab_idx),
  is_expensive_cache(-1),
  rsize(0),
  str_value(item->str_value),
  name(item->name),
  orig_name(item->orig_name),
  max_length(item->max_length),
  name_length(item->name_length),
  decimals(item->decimals),
  marker(item->marker),
  maybe_null(item->maybe_null),
  in_rollup(item->in_rollup),
  null_value(item->null_value),
  unsigned_flag(item->unsigned_flag),
  with_sum_func(item->with_sum_func),
  with_field(item->with_field),
  fixed(item->fixed),
  is_autogenerated_name(item->is_autogenerated_name),
  with_subselect(item->with_subselect),
  collation(item->collation),
  cmp_context(item->cmp_context)
{
  next= thd->free_list;				// Put in free list
  thd->free_list= this;
}


uint Item::decimal_precision() const
{
  Item_result restype= result_type();

  if ((restype == DECIMAL_RESULT) || (restype == INT_RESULT))
  {
    uint prec= 
      my_decimal_length_to_precision(max_length, decimals, unsigned_flag);
    return min(prec, DECIMAL_MAX_PRECISION);
  }
  return min(max_length, DECIMAL_MAX_PRECISION);
}


void Item::print_item_w_name(String *str, enum_query_type query_type)
{
  print(str, query_type);

  if (name)
  {
    THD *thd= current_thd;
    str->append(STRING_WITH_LEN(" AS "));
    append_identifier(thd, str, name, (uint) strlen(name));
  }
}


void Item::print_value(String *str)
{
  char buff[MAX_FIELD_WIDTH];
  String *ptr, tmp(buff,sizeof(buff),str->charset());
  ptr= val_str(&tmp);
  if (!ptr)
    str->append("NULL");
  else
  {
    switch (result_type()) {
    case STRING_RESULT:
      append_unescaped(str, ptr->ptr(), ptr->length());
      break;
    case DECIMAL_RESULT:
    case REAL_RESULT:
    case INT_RESULT:
      str->append(*ptr);
      break;
    case ROW_RESULT:
    case TIME_RESULT:
    case IMPOSSIBLE_RESULT:
      DBUG_ASSERT(0);
    }
  }
}


void Item::cleanup()
{
  DBUG_ENTER("Item::cleanup");
  DBUG_PRINT("enter", ("this: %p", this));
  fixed=0;
  marker= 0;
  join_tab_idx= MAX_TABLES;
  if (orig_name)
    name= orig_name;
  DBUG_VOID_RETURN;
}


/**
  cleanup() item if it is 'fixed'.

  @param arg   a dummy parameter, is not used here
*/

bool Item::cleanup_processor(uchar *arg)
{
  if (fixed)
    cleanup();
  return FALSE;
}


/**
  rename item (used for views, cleanup() return original name).

  @param new_name	new name of item;
*/

void Item::rename(char *new_name)
{
  /*
    we can compare pointers to names here, because if name was not changed,
    pointer will be same
  */
  if (!orig_name && new_name != name)
    orig_name= name;
  name= new_name;
}

Item_result Item::cmp_type() const
{
  switch (field_type()) {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
                           return DECIMAL_RESULT;
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_BIT:
                           return INT_RESULT;
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
                           return REAL_RESULT;
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_GEOMETRY:
                           return STRING_RESULT;
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_NEWDATE:
                           return TIME_RESULT;
  };
  DBUG_ASSERT(0);
  return (Item_result)-1;
}

/**
  Traverse item tree possibly transforming it (replacing items).

  This function is designed to ease transformation of Item trees.
  Re-execution note: every such transformation is registered for
  rollback by THD::change_item_tree() and is rolled back at the end
  of execution by THD::rollback_item_tree_changes().

  Therefore:
  - this function can not be used at prepared statement prepare
  (in particular, in fix_fields!), as only permanent
  transformation of Item trees are allowed at prepare.
  - the transformer function shall allocate new Items in execution
  memory root (thd->mem_root) and not anywhere else: allocated
  items will be gone in the end of execution.

  If you don't need to transform an item tree, but only traverse
  it, please use Item::walk() instead.


  @param transformer    functor that performs transformation of a subtree
  @param arg            opaque argument passed to the functor

  @return
    Returns pointer to the new subtree root.  THD::change_item_tree()
    should be called for it if transformation took place, i.e. if a
    pointer to newly allocated item is returned.
*/

Item* Item::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->is_stmt_prepare());

  return (this->*transformer)(arg);
}


/**
  Create and set up an expression cache for this item

  @param thd             Thread handle
  @param depends_on      List of the expression parameters

  @details
  The function creates an expression cache for an item and its parameters
  specified by the 'depends_on' list. Then the expression cache is placed
  into a cache wrapper that is returned as the result of the function.

  @returns
  A pointer to created wrapper item if successful, NULL - otherwise
*/

Item* Item::set_expr_cache(THD *thd)
{
  DBUG_ENTER("Item::set_expr_cache");
  Item_cache_wrapper *wrapper;
  if ((wrapper= new Item_cache_wrapper(this)) &&
      !wrapper->fix_fields(thd, (Item**)&wrapper))
  {
    if (wrapper->set_cache(thd))
      DBUG_RETURN(NULL);
    DBUG_RETURN(wrapper);
  }
  DBUG_RETURN(NULL);
}


Item_ident::Item_ident(Name_resolution_context *context_arg,
                       const char *db_name_arg,const char *table_name_arg,
		       const char *field_name_arg)
  :orig_db_name(db_name_arg), orig_table_name(table_name_arg),
   orig_field_name(field_name_arg), context(context_arg),
   db_name(db_name_arg), table_name(table_name_arg),
   field_name(field_name_arg),
   alias_name_used(FALSE), cached_field_index(NO_CACHED_FIELD_INDEX),
   cached_table(0), depended_from(0), can_be_depended(TRUE)
{
  name = (char*) field_name_arg;
}


Item_ident::Item_ident(TABLE_LIST *view_arg, const char *field_name_arg)
  :orig_db_name(NullS), orig_table_name(view_arg->table_name),
   orig_field_name(field_name_arg), context(&view_arg->view->select_lex.context),
   db_name(NullS), table_name(view_arg->alias),
   field_name(field_name_arg),
   alias_name_used(FALSE), cached_field_index(NO_CACHED_FIELD_INDEX),
   cached_table(NULL), depended_from(NULL), can_be_depended(TRUE)
{
  name = (char*) field_name_arg;
}


/**
  Constructor used by Item_field & Item_*_ref (see Item comment)
*/

Item_ident::Item_ident(THD *thd, Item_ident *item)
  :Item(thd, item),
   orig_db_name(item->orig_db_name),
   orig_table_name(item->orig_table_name), 
   orig_field_name(item->orig_field_name),
   context(item->context),
   db_name(item->db_name),
   table_name(item->table_name),
   field_name(item->field_name),
   alias_name_used(item->alias_name_used),
   cached_field_index(item->cached_field_index),
   cached_table(item->cached_table),
   depended_from(item->depended_from),
   can_be_depended(item->can_be_depended)
{}

void Item_ident::cleanup()
{
  DBUG_ENTER("Item_ident::cleanup");
#ifdef CANT_BE_USED_AS_MEMORY_IS_FREED
		       db_name ? db_name : "(null)",
                       orig_db_name ? orig_db_name : "(null)",
		       table_name ? table_name : "(null)",
                       orig_table_name ? orig_table_name : "(null)",
		       field_name ? field_name : "(null)",
                       orig_field_name ? orig_field_name : "(null)"));
#endif
  Item::cleanup();
  db_name= orig_db_name; 
  table_name= orig_table_name;
  field_name= orig_field_name;
  /* Store if this Item was depended */
  can_be_depended= test(depended_from);
  DBUG_VOID_RETURN;
}

bool Item_ident::remove_dependence_processor(uchar * arg)
{
  DBUG_ENTER("Item_ident::remove_dependence_processor");
  if (get_depended_from() == (st_select_lex *) arg)
    depended_from= 0;
  context= &((st_select_lex *) arg)->context;
  DBUG_RETURN(0);
}


bool Item_ident::collect_outer_ref_processor(uchar *param)
{
  Collect_deps_prm *prm= (Collect_deps_prm *)param;
  if (depended_from && 
      depended_from->nest_level_base == prm->nest_level_base &&
      depended_from->nest_level < prm->nest_level)
    prm->parameters->add_unique(this, &cmp_items);
  return FALSE;
}


/**
  Store the pointer to this item field into a list if not already there.

  The method is used by Item::walk to collect all unique Item_field objects
  from a tree of Items into a set of items represented as a list.

  Item_cond::walk() and Item_func::walk() stop the evaluation of the
  processor function for its arguments once the processor returns
  true.Therefore in order to force this method being called for all item
  arguments in a condition the method must return false.

  @param arg  pointer to a List<Item_field>

  @return
    FALSE to force the evaluation of collect_item_field_processor
    for the subsequent items.
*/

bool Item_field::collect_item_field_processor(uchar *arg)
{
  DBUG_ENTER("Item_field::collect_item_field_processor");
  DBUG_PRINT("info", ("%s", field->field_name ? field->field_name : "noname"));
  List<Item_field> *item_list= (List<Item_field>*) arg;
  List_iterator<Item_field> item_list_it(*item_list);
  Item_field *curr_item;
  while ((curr_item= item_list_it++))
  {
    if (curr_item->eq(this, 1))
      DBUG_RETURN(FALSE); /* Already in the set. */
  }
  item_list->push_back(this);
  DBUG_RETURN(FALSE);
}


bool Item_field::add_field_to_set_processor(uchar *arg)
{
  DBUG_ENTER("Item_field::add_field_to_set_processor");
  DBUG_PRINT("info", ("%s", field->field_name ? field->field_name : "noname"));
  TABLE *table= (TABLE *) arg;
  if (field->table == table)
    bitmap_set_bit(&table->tmp_set, field->field_index);
  DBUG_RETURN(FALSE);
}

/**
  Check if an Item_field references some field from a list of fields.

  Check whether the Item_field represented by 'this' references any
  of the fields in the keyparts passed via 'arg'. Used with the
  method Item::walk() to test whether any keypart in a sequence of
  keyparts is referenced in an expression.

  @param arg   Field being compared, arg must be of type Field

  @retval
    TRUE  if 'this' references the field 'arg'
  @retval
    FALSE otherwise
*/

bool Item_field::find_item_in_field_list_processor(uchar *arg)
{
  KEY_PART_INFO *first_non_group_part= *((KEY_PART_INFO **) arg);
  KEY_PART_INFO *last_part= *(((KEY_PART_INFO **) arg) + 1);
  KEY_PART_INFO *cur_part;

  for (cur_part= first_non_group_part; cur_part != last_part; cur_part++)
  {
    if (field->eq(cur_part->field))
      return TRUE;
  }
  return FALSE;
}


/*
  Mark field in read_map

  NOTES
    This is used by filesort to register used fields in a a temporary
    column read set or to register used fields in a view
*/

bool Item_field::register_field_in_read_map(uchar *arg)
{
  TABLE *table= (TABLE *) arg;
  if (field->table == table || !table)
    bitmap_set_bit(field->table->read_set, field->field_index);
  if (field->vcol_info && field->vcol_info->expr_item)
    return field->vcol_info->expr_item->walk(&Item::register_field_in_read_map, 
                                             1, arg);
  return 0;
}

/*
  @brief
  Mark field in bitmap supplied as *arg
*/

bool Item_field::register_field_in_bitmap(uchar *arg)
{
  MY_BITMAP *bitmap= (MY_BITMAP *) arg;
  DBUG_ASSERT(bitmap);
  bitmap_set_bit(bitmap, field->field_index);
  return 0;
}


/*
  Mark field in write_map

  NOTES
    This is used by UPDATE to register underlying fields of used view fields.
*/

bool Item_field::register_field_in_write_map(uchar *arg)
{
  TABLE *table= (TABLE *) arg;
  if (field->table == table || !table)
    bitmap_set_bit(field->table->write_set, field->field_index);
  return 0;
}


bool Item::check_cols(uint c)
{
  if (c != 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


void Item::set_name(const char *str, uint length, CHARSET_INFO *cs)
{
  if (!length)
  {
    /* Empty string, used by AS or internal function like last_insert_id() */
    name= (char*) str;
    name_length= 0;
    return;
  }
  if (cs->ctype)
  {
    const char *str_start= str;

    /*
      This will probably need a better implementation in the future:
      a function in CHARSET_INFO structure.
    */
    while (length && !my_isgraph(cs,*str))
    {						// Fix problem with yacc
      length--;
      str++;
    }
    if (str != str_start && !is_autogenerated_name)
    {
      char buff[SAFE_NAME_LEN];
      strmake(buff, str_start,
              min(sizeof(buff)-1, length + (int) (str-str_start)));

      if (length == 0)
        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NAME_BECOMES_EMPTY, ER(ER_NAME_BECOMES_EMPTY),
                            buff);
      else
        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_REMOVED_SPACES, ER(ER_REMOVED_SPACES),
                            buff);
    }
  }
  if (!my_charset_same(cs, system_charset_info))
  {
    size_t res_length;
    name= sql_strmake_with_convert(str, length, cs,
				   MAX_ALIAS_NAME, system_charset_info,
				   &res_length);
    name_length= res_length;
  }
  else
    name= sql_strmake(str, (name_length= min(length,MAX_ALIAS_NAME)));
}


void Item::set_name_no_truncate(const char *str, uint length, CHARSET_INFO *cs)
{
  if (!my_charset_same(cs, system_charset_info))
  {
    size_t res_length;
    name= sql_strmake_with_convert(str, length, cs,
				   UINT_MAX, system_charset_info,
				   &res_length);
    name_length= res_length;
  }
  else
    name= sql_strmake(str, (name_length= length));
}


void Item::set_name_for_rollback(THD *thd, const char *str, uint length,
                                 CHARSET_INFO *cs)
{
  char *old_name, *new_name; 
  old_name= name;
  set_name(str, length, cs);
  new_name= name;
  if (old_name != new_name)
  {
    name= old_name;
    thd->change_item_tree((Item **) &name, (Item *) new_name);
  }
}


/**
  @details
  This function is called when:
  - Comparing items in the WHERE clause (when doing where optimization)
  - When trying to find an ORDER BY/GROUP BY item in the SELECT part
*/

bool Item::eq(const Item *item, bool binary_cmp) const
{
  /*
    Note, that this is never TRUE if item is a Item_param:
    for all basic constants we have special checks, and Item_param's
    type() can be only among basic constant types.
  */
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(system_charset_info,name,item->name);
}


Item *Item::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_func_conv_charset *conv= new Item_func_conv_charset(this, tocs, 1);
  return conv->safe ? conv : NULL;
}


/**
  @details
  Created mostly for mysql_prepare_table(). Important
  when a string ENUM/SET column is described with a numeric default value:

  CREATE TABLE t1(a SET('a') DEFAULT 1);

  We cannot use generic Item::safe_charset_converter(), because
  the latter returns a non-fixed Item, so val_str() crashes afterwards.
  Override Item_num method, to return a fixed item.
*/
Item *Item_num::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  char buf[64];
  String *s, tmp(buf, sizeof(buf), &my_charset_bin);
  s= val_str(&tmp);
  if ((conv= new Item_string(s->ptr(), s->length(), s->charset())))
  {
    conv->str_value.copy();
    conv->str_value.mark_as_const();
  }
  return conv;
}


Item *Item_static_float_func::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  char buf[64];
  String *s, tmp(buf, sizeof(buf), &my_charset_bin);
  s= val_str(&tmp);
  if ((conv= new Item_static_string_func(func_name, s->ptr(), s->length(),
                                         s->charset())))
  {
    conv->str_value.copy();
    conv->str_value.mark_as_const();
  }
  return conv;
}


Item *Item_string::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  char *ptr;
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), tocs, &conv_errors);
  if (conv_errors || !(conv= new Item_string(cstr.ptr(), cstr.length(),
                                             cstr.charset(),
                                             collation.derivation)))
  {
    /*
      Safe conversion is not possible (or EOM).
      We could not convert a string into the requested character set
      without data loss. The target charset does not cover all the
      characters from the string. Operation cannot be done correctly.
    */
    return NULL;
  }
  if (!(ptr= current_thd->strmake(cstr.ptr(), cstr.length())))
    return NULL;
  conv->str_value.set(ptr, cstr.length(), cstr.charset());
  /* Ensure that no one is going to change the result string */
  conv->str_value.mark_as_const();
  return conv;
}


Item *Item_param::safe_charset_converter(CHARSET_INFO *tocs)
{
  if (const_item())
  {
    uint cnv_errors;
    String *ostr= val_str(&cnvstr);
    cnvitem->str_value.copy(ostr->ptr(), ostr->length(),
                            ostr->charset(), tocs, &cnv_errors);
    if (cnv_errors)
       return NULL;
    cnvitem->str_value.mark_as_const();
    cnvitem->max_length= cnvitem->str_value.numchars() * tocs->mbmaxlen;
    return cnvitem;
  }
  return NULL;
}


Item *Item_static_string_func::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), tocs, &conv_errors);
  if (conv_errors ||
      !(conv= new Item_static_string_func(func_name,
                                          cstr.ptr(), cstr.length(),
                                          cstr.charset(),
                                          collation.derivation)))
  {
    /*
      Safe conversion is not possible (or EOM).
      We could not convert a string into the requested character set
      without data loss. The target charset does not cover all the
      characters from the string. Operation cannot be done correctly.
    */
    return NULL;
  }
  conv->str_value.copy();
  /* Ensure that no one is going to change the result string */
  conv->str_value.mark_as_const();
  return conv;
}


bool Item_string::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type() && item->basic_const_item())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return (collation.collation == item->collation.collation &&
	    !sortcmp(&str_value, &item->str_value, collation.collation));
  }
  return 0;
}


/**
  Get the value of the function as a MYSQL_TIME structure.
  As a extra convenience the time structure is reset on error or NULL values!
*/

bool Item::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  if (field_type() == MYSQL_TYPE_TIME)
    fuzzydate|= TIME_TIME_ONLY;

  switch (result_type()) {
  case INT_RESULT:
  {
    longlong value= val_int();
    if (field_type() == MYSQL_TYPE_YEAR)
    {
      if (max_length == 2)
      {
        if (value < 70)
          value+= 2000;
        else if (value <= 1900)
          value+= 1900;
      }
      value*= 10000; /* make it YYYYMMHH */
    }
    if (null_value || int_to_datetime_with_warn(value, ltime, fuzzydate,
                                                field_name_or_null()))
      goto err;
    break;
  }
  case REAL_RESULT:
  {
    double value= val_real();
    if (null_value || double_to_datetime_with_warn(value, ltime, fuzzydate,
                                                   field_name_or_null()))
      goto err;
    break;
  }
  case DECIMAL_RESULT:
  {
    my_decimal value, *res;
    if (!(res= val_decimal(&value)) ||
        decimal_to_datetime_with_warn(res, ltime, fuzzydate,
                                      field_name_or_null()))
      goto err;
    break;
  }
  case STRING_RESULT:
  {
    char buff[40];
    String tmp(buff,sizeof(buff), &my_charset_bin),*res;
    if (!(res=val_str(&tmp)) ||
        str_to_datetime_with_warn(res->ptr(), res->length(),
                                  ltime, fuzzydate) <= MYSQL_TIMESTAMP_ERROR)
      goto err;
    break;
  }
  default:
    DBUG_ASSERT(0);
  }

  return 0;

err:
  bzero((char*) ltime,sizeof(*ltime));
  return 1;
}

bool Item::get_seconds(ulonglong *sec, ulong *sec_part)
{
  if (decimals == 0)
  { // optimize for an important special case
    longlong val= val_int();
    bool neg= val < 0 && !unsigned_flag;
    *sec= neg ? -val : val;
    *sec_part= 0;
    return neg;
  }
  my_decimal tmp, *dec= val_decimal(&tmp);
  if (!dec)
    return 0;
  return my_decimal2seconds(dec, sec, sec_part);
}

CHARSET_INFO *Item::default_charset()
{
  return current_thd->variables.collation_connection;
}


/*
  Save value in field, but don't give any warnings

  NOTES
   This is used to temporary store and retrieve a value in a column,
   for example in opt_range to adjust the key value to fit the column.
*/

int Item::save_in_field_no_warnings(Field *field, bool no_conversions)
{
  int res;
  TABLE *table= field->table;
  THD *thd= table->in_use;
  enum_check_fields tmp= thd->count_cuted_fields;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  ulong sql_mode= thd->variables.sql_mode;
  thd->variables.sql_mode&= ~(MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE);
  thd->variables.sql_mode|= MODE_INVALID_DATES;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  res= save_in_field(field, no_conversions);
  thd->count_cuted_fields= tmp;
  dbug_tmp_restore_column_map(table->write_set, old_map);
  thd->variables.sql_mode= sql_mode;
  return res;
}


/*****************************************************************************
  Item_sp_variable methods
*****************************************************************************/

Item_sp_variable::Item_sp_variable(char *sp_var_name_str,
                                   uint sp_var_name_length)
  :m_thd(0)
#ifndef DBUG_OFF
   , m_sp(0)
#endif
{
  m_name.str= sp_var_name_str;
  m_name.length= sp_var_name_length;
}


bool Item_sp_variable::fix_fields(THD *thd, Item **)
{
  Item *it;

  m_thd= thd; /* NOTE: this must be set before any this_xxx() */
  it= this_item();

  DBUG_ASSERT(it->fixed);

  max_length= it->max_length;
  decimals= it->decimals;
  unsigned_flag= it->unsigned_flag;
  fixed= 1;
  collation.set(it->collation.collation, it->collation.derivation);

  return FALSE;
}


double Item_sp_variable::val_real()
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  double ret= it->val_real();
  null_value= it->null_value;
  return ret;
}


longlong Item_sp_variable::val_int()
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  longlong ret= it->val_int();
  null_value= it->null_value;
  return ret;
}


String *Item_sp_variable::val_str(String *sp)
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  String *res= it->val_str(sp);

  null_value= it->null_value;

  if (!res)
    return NULL;

  /*
    This way we mark returned value of val_str as const,
    so that various functions (e.g. CONCAT) won't try to
    modify the value of the Item. Analogous mechanism is
    implemented for Item_param.
    Without this trick Item_splocal could be changed as a
    side-effect of expression computation. Here is an example
    of what happens without it: suppose x is varchar local
    variable in a SP with initial value 'ab' Then
      select concat(x,'c');
    would change x's value to 'abc', as Item_func_concat::val_str()
    would use x's internal buffer to compute the result.
    This is intended behaviour of Item_func_concat. Comments to
    Item_param class contain some more details on the topic.
  */

  if (res != &str_value)
    str_value.set(res->ptr(), res->length(), res->charset());
  else
    res->mark_as_const();

  return &str_value;
}


my_decimal *Item_sp_variable::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  my_decimal *val= it->val_decimal(decimal_value);
  null_value= it->null_value;
  return val;
}


bool Item_sp_variable::is_null()
{
  return this_item()->is_null();
}


/*****************************************************************************
  Item_splocal methods
*****************************************************************************/

Item_splocal::Item_splocal(const LEX_STRING &sp_var_name,
                           uint sp_var_idx,
                           enum_field_types sp_var_type,
                           uint pos_in_q, uint len_in_q)
  :Item_sp_variable(sp_var_name.str, sp_var_name.length),
   m_var_idx(sp_var_idx), pos_in_query(pos_in_q), len_in_query(len_in_q)
{
  maybe_null= TRUE;

  m_type= sp_map_item_type(sp_var_type);
  m_field_type= sp_var_type;
  m_result_type= sp_map_result_type(sp_var_type);
}


Item *
Item_splocal::this_item()
{
  DBUG_ASSERT(m_sp == m_thd->spcont->sp);

  return m_thd->spcont->get_item(m_var_idx);
}

const Item *
Item_splocal::this_item() const
{
  DBUG_ASSERT(m_sp == m_thd->spcont->sp);

  return m_thd->spcont->get_item(m_var_idx);
}


Item **
Item_splocal::this_item_addr(THD *thd, Item **)
{
  DBUG_ASSERT(m_sp == thd->spcont->sp);

  return thd->spcont->get_item_addr(m_var_idx);
}


void Item_splocal::print(String *str, enum_query_type)
{
  str->reserve(m_name.length+8);
  str->append(m_name.str, m_name.length);
  str->append('@');
  str->qs_append(m_var_idx);
}


bool Item_splocal::set_value(THD *thd, sp_rcontext *ctx, Item **it)
{
  return ctx->set_variable(thd, get_var_idx(), it);
}


/*****************************************************************************
  Item_case_expr methods
*****************************************************************************/

Item_case_expr::Item_case_expr(uint case_expr_id)
  :Item_sp_variable( C_STRING_WITH_LEN("case_expr")),
   m_case_expr_id(case_expr_id)
{
}


Item *
Item_case_expr::this_item()
{
  DBUG_ASSERT(m_sp == m_thd->spcont->sp);

  return m_thd->spcont->get_case_expr(m_case_expr_id);
}



const Item *
Item_case_expr::this_item() const
{
  DBUG_ASSERT(m_sp == m_thd->spcont->sp);

  return m_thd->spcont->get_case_expr(m_case_expr_id);
}


Item **
Item_case_expr::this_item_addr(THD *thd, Item **)
{
  DBUG_ASSERT(m_sp == thd->spcont->sp);

  return thd->spcont->get_case_expr_addr(m_case_expr_id);
}


void Item_case_expr::print(String *str, enum_query_type)
{
  if (str->reserve(MAX_INT_WIDTH + sizeof("case_expr@")))
    return;                                    /* purecov: inspected */
  VOID(str->append(STRING_WITH_LEN("case_expr@")));
  str->qs_append(m_case_expr_id);
}


/*****************************************************************************
  Item_name_const methods
*****************************************************************************/

double Item_name_const::val_real()
{
  DBUG_ASSERT(fixed);
  double ret= value_item->val_real();
  null_value= value_item->null_value;
  return ret;
}


longlong Item_name_const::val_int()
{
  DBUG_ASSERT(fixed);
  longlong ret= value_item->val_int();
  null_value= value_item->null_value;
  return ret;
}


String *Item_name_const::val_str(String *sp)
{
  DBUG_ASSERT(fixed);
  String *ret= value_item->val_str(sp);
  null_value= value_item->null_value;
  return ret;
}


my_decimal *Item_name_const::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  my_decimal *val= value_item->val_decimal(decimal_value);
  null_value= value_item->null_value;
  return val;
}


bool Item_name_const::is_null()
{
  return value_item->is_null();
}


Item_name_const::Item_name_const(Item *name_arg, Item *val):
    value_item(val), name_item(name_arg)
{
  if (!(valid_args= name_item->basic_const_item() &&
                    (value_item->basic_const_item() ||
                     ((value_item->type() == FUNC_ITEM) &&
                      ((((Item_func *) value_item)->functype() ==
                         Item_func::COLLATE_FUNC) ||
                      ((((Item_func *) value_item)->functype() ==
                         Item_func::NEG_FUNC) &&
                      (((Item_func *) value_item)->key_item()->type() !=
                         FUNC_ITEM)))))))
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "NAME_CONST");
  Item::maybe_null= TRUE;
}


Item::Type Item_name_const::type() const
{
  /*
    As 
    1. one can try to create the Item_name_const passing non-constant 
    arguments, although it's incorrect and 
    2. the type() method can be called before the fix_fields() to get
    type information for a further type cast, e.g. 
    if (item->type() == FIELD_ITEM) 
      ((Item_field *) item)->... 
    we return NULL_ITEM in the case to avoid wrong casting.

    valid_args guarantees value_item->basic_const_item(); if type is
    FUNC_ITEM, then we have a fudged item_func_neg() on our hands
    and return the underlying type.
    For Item_func_set_collation()
    e.g. NAME_CONST('name', 'value' COLLATE collation) we return its
    'value' argument type. 
  */
  if (!valid_args)
    return NULL_ITEM;
  Item::Type value_type= value_item->type();
  if (value_type == FUNC_ITEM)
  {
    /* 
      The second argument of NAME_CONST('name', 'value') must be 
      a simple constant item or a NEG_FUNC/COLLATE_FUNC.
    */
    DBUG_ASSERT(((Item_func *) value_item)->functype() == 
                Item_func::NEG_FUNC ||
                ((Item_func *) value_item)->functype() == 
                Item_func::COLLATE_FUNC);
    return ((Item_func *) value_item)->key_item()->type();            
  }
  return value_type;
}


bool Item_name_const::fix_fields(THD *thd, Item **ref)
{
  char buf[128];
  String *item_name;
  String s(buf, sizeof(buf), &my_charset_bin);
  s.length(0);

  if (value_item->fix_fields(thd, &value_item) ||
      name_item->fix_fields(thd, &name_item) ||
      !value_item->const_item() ||
      !name_item->const_item() ||
      !(item_name= name_item->val_str(&s))) // Can't have a NULL name 
  {
    my_error(ER_RESERVED_SYNTAX, MYF(0), "NAME_CONST");
    return TRUE;
  }
  if (is_autogenerated_name)
  {
    set_name(item_name->ptr(), (uint) item_name->length(), system_charset_info);
  }
  collation.set(value_item->collation.collation, DERIVATION_IMPLICIT);
  max_length= value_item->max_length;
  decimals= value_item->decimals;
  fixed= 1;
  return FALSE;
}


void Item_name_const::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("NAME_CONST("));
  name_item->print(str, query_type);
  str->append(',');
  value_item->print(str, query_type);
  str->append(')');
}


/*
 need a special class to adjust printing : references to aggregate functions 
 must not be printed as refs because the aggregate functions that are added to
 the front of select list are not printed as well.
*/
class Item_aggregate_ref : public Item_ref
{
public:
  Item_aggregate_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg, const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg) {}

  virtual inline void print (String *str, enum_query_type query_type)
  {
    if (ref)
      (*ref)->print(str, query_type);
    else
      Item_ident::print(str, query_type);
  }
  virtual Ref_Type ref_type() { return AGGREGATE_REF; }
};


/**
  Move SUM items out from item tree and replace with reference.

  @param thd			Thread handler
  @param ref_pointer_array	Pointer to array of reference fields
  @param fields		All fields in select
  @param ref			Pointer to item
  @param skip_registered       <=> function be must skipped for registered
                               SUM items

  @note
    This is from split_sum_func2() for items that should be split

    All found SUM items are added FIRST in the fields list and
    we replace the item with a reference.

    thd->fatal_error() may be called if we are out of memory
*/

void Item::split_sum_func2(THD *thd, Item **ref_pointer_array,
                           List<Item> &fields, Item **ref, 
                           bool skip_registered)
{
  /* An item of type Item_sum  is registered <=> ref_by != 0 */ 
  if (type() == SUM_FUNC_ITEM && skip_registered && 
      ((Item_sum *) this)->ref_by)
    return;
  if ((type() != SUM_FUNC_ITEM && with_sum_func) ||
      (type() == FUNC_ITEM &&
       (((Item_func *) this)->functype() == Item_func::ISNOTNULLTEST_FUNC ||
        ((Item_func *) this)->functype() == Item_func::TRIG_COND_FUNC)))
  {
    /* Will split complicated items and ignore simple ones */
    split_sum_func(thd, ref_pointer_array, fields);
  }
  else if ((type() == SUM_FUNC_ITEM || (used_tables() & ~PARAM_TABLE_BIT)) &&
           type() != SUBSELECT_ITEM &&
           (type() != REF_ITEM ||
           ((Item_ref*)this)->ref_type() == Item_ref::VIEW_REF))
  {
    /*
      Replace item with a reference so that we can easily calculate
      it (in case of sum functions) or copy it (in case of fields)

      The test above is to ensure we don't do a reference for things
      that are constants (PARAM_TABLE_BIT is in effect a constant)
      or already referenced (for example an item in HAVING)
      Exception is Item_direct_view_ref which we need to convert to
      Item_ref to allow fields from view being stored in tmp table.
    */
    Item_aggregate_ref *item_ref;
    uint el= fields.elements;
    /*
      If this is an item_ref, get the original item
      This is a safety measure if this is called for things that is
      already a reference.
    */
    Item *real_itm= real_item();

    ref_pointer_array[el]= real_itm;
    if (!(item_ref= new Item_aggregate_ref(&thd->lex->current_select->context,
                                           ref_pointer_array + el, 0, name)))
      return;                                   // fatal_error is set
    if (type() == SUM_FUNC_ITEM)
      item_ref->depended_from= ((Item_sum *) this)->depended_from(); 
    fields.push_front(real_itm);
    thd->change_item_tree(ref, item_ref);
  }
}


static bool
left_is_superset(DTCollation *left, DTCollation *right)
{
  /* Allow convert to Unicode */
  if (left->collation->state & MY_CS_UNICODE &&
      (left->derivation < right->derivation ||
       (left->derivation == right->derivation &&
        !(right->collation->state & MY_CS_UNICODE))))
    return TRUE;
  /* Allow convert from ASCII */
  if (right->repertoire == MY_REPERTOIRE_ASCII &&
      (left->derivation < right->derivation ||
       (left->derivation == right->derivation &&
        !(left->repertoire == MY_REPERTOIRE_ASCII))))
    return TRUE;
  /* Disallow conversion otherwise */
  return FALSE;
}

/**
  Aggregate two collations together taking
  into account their coercibility (aka derivation):.

  0 == DERIVATION_EXPLICIT  - an explicitly written COLLATE clause @n
  1 == DERIVATION_NONE      - a mix of two different collations @n
  2 == DERIVATION_IMPLICIT  - a column @n
  3 == DERIVATION_COERCIBLE - a string constant.

  The most important rules are:
  -# If collations are the same:
  chose this collation, and the strongest derivation.
  -# If collations are different:
  - Character sets may differ, but only if conversion without
  data loss is possible. The caller provides flags whether
  character set conversion attempts should be done. If no
  flags are substituted, then the character sets must be the same.
  Currently processed flags are:
  MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
  MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
  - two EXPLICIT collations produce an error, e.g. this is wrong:
  CONCAT(expr1 collate latin1_swedish_ci, expr2 collate latin1_german_ci)
  - the side with smaller derivation value wins,
  i.e. a column is stronger than a string constant,
  an explicit COLLATE clause is stronger than a column.
  - if derivations are the same, we have DERIVATION_NONE,
  we'll wait for an explicit COLLATE clause which possibly can
  come from another argument later: for example, this is valid,
  but we don't know yet when collecting the first two arguments:
     @code
       CONCAT(latin1_swedish_ci_column,
              latin1_german1_ci_column,
              expr COLLATE latin1_german2_ci)
  @endcode
*/

bool DTCollation::aggregate(DTCollation &dt, uint flags)
{
  if (!my_charset_same(collation, dt.collation))
  {
    /* 
       We do allow to use binary strings (like BLOBS)
       together with character strings.
       Binaries have more precedence than a character
       string of the same derivation.
    */
    if (collation == &my_charset_bin)
    {
      if (derivation <= dt.derivation)
      {
	/* Do nothing */
      }
      else
      {
	set(dt); 
      }
    }
    else if (dt.collation == &my_charset_bin)
    {
      if (dt.derivation <= derivation)
      {
        set(dt);
      }
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             left_is_superset(this, &dt))
    {
      /* Do nothing */
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             left_is_superset(&dt, this))
    {
      set(dt);
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             derivation < dt.derivation &&
             dt.derivation >= DERIVATION_SYSCONST)
    {
      /* Do nothing */
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             dt.derivation < derivation &&
             derivation >= DERIVATION_SYSCONST)
    {
      set(dt);
    }
    else
    {
      // Cannot apply conversion
      set(&my_charset_bin, DERIVATION_NONE,
          (dt.repertoire|repertoire));
      return 1;
    }
  }
  else if (derivation < dt.derivation)
  {
    /* Do nothing */
  }
  else if (dt.derivation < derivation)
  {
    set(dt);
  }
  else
  { 
    if (collation == dt.collation)
    {
      /* Do nothing */
    }
    else 
    {
      if (derivation == DERIVATION_EXPLICIT)
      {
        set(0, DERIVATION_NONE, 0);
        return 1;
      }
      if (collation->state & MY_CS_BINSORT)
        return 0;
      if (dt.collation->state & MY_CS_BINSORT)
      {
        set(dt);
        return 0;
      }
      CHARSET_INFO *bin= get_charset_by_csname(collation->csname, 
                                               MY_CS_BINSORT,MYF(0));
      set(bin, DERIVATION_NONE);
    }
  }
  repertoire|= dt.repertoire;
  return 0;
}

/******************************/
static
void my_coll_agg_error(DTCollation &c1, DTCollation &c2, const char *fname)
{
  my_error(ER_CANT_AGGREGATE_2COLLATIONS,MYF(0),
           c1.collation->name,c1.derivation_name(),
           c2.collation->name,c2.derivation_name(),
           fname);
}


static
void my_coll_agg_error(DTCollation &c1, DTCollation &c2, DTCollation &c3,
                       const char *fname)
{
  my_error(ER_CANT_AGGREGATE_3COLLATIONS,MYF(0),
  	   c1.collation->name,c1.derivation_name(),
	   c2.collation->name,c2.derivation_name(),
	   c3.collation->name,c3.derivation_name(),
	   fname);
}


static
void my_coll_agg_error(Item** args, uint count, const char *fname,
                       int item_sep)
{
  if (count == 2)
    my_coll_agg_error(args[0]->collation, args[item_sep]->collation, fname);
  else if (count == 3)
    my_coll_agg_error(args[0]->collation, args[item_sep]->collation,
                      args[2*item_sep]->collation, fname);
  else
    my_error(ER_CANT_AGGREGATE_NCOLLATIONS,MYF(0),fname);
}


bool agg_item_collations(DTCollation &c, const char *fname,
                         Item **av, uint count, uint flags, int item_sep)
{
  uint i;
  Item **arg;
  bool unknown_cs= 0;

  c.set(av[0]->collation);
  for (i= 1, arg= &av[item_sep]; i < count; i++, arg++)
  {
    if (c.aggregate((*arg)->collation, flags))
    {
      if (c.derivation == DERIVATION_NONE &&
          c.collation == &my_charset_bin)
      {
        unknown_cs= 1;
        continue;
      }
      my_coll_agg_error(av, count, fname, item_sep);
      return TRUE;
    }
  }

  if (unknown_cs &&
      c.derivation != DERIVATION_EXPLICIT)
  {
    my_coll_agg_error(av, count, fname, item_sep);
    return TRUE;
  }

  if ((flags & MY_COLL_DISALLOW_NONE) &&
      c.derivation == DERIVATION_NONE)
  {
    my_coll_agg_error(av, count, fname, item_sep);
    return TRUE;
  }
  return FALSE;
}


bool agg_item_collations_for_comparison(DTCollation &c, const char *fname,
                                        Item **av, uint count, uint flags)
{
  return (agg_item_collations(c, fname, av, count,
                              flags | MY_COLL_DISALLOW_NONE, 1));
}


bool agg_item_set_converter(DTCollation &coll, const char *fname,
                            Item **args, uint nargs, uint flags, int item_sep)
{
  Item **arg, *safe_args[2]= {NULL, NULL};

  /*
    For better error reporting: save the first and the second argument.
    We need this only if the the number of args is 3 or 2:
    - for a longer argument list, "Illegal mix of collations"
      doesn't display each argument's characteristics.
    - if nargs is 1, then this error cannot happen.
  */
  if (nargs >=2 && nargs <= 3)
  {
    safe_args[0]= args[0];
    safe_args[1]= args[item_sep];
  }

  THD *thd= current_thd;
  Query_arena *arena, backup;
  bool res= FALSE;
  uint i;
  /*
    In case we're in statement prepare, create conversion item
    in its memory: it will be reused on each execute.
  */
  arena= thd->is_stmt_prepare() ? thd->activate_stmt_arena_if_needed(&backup)
                                : NULL;

  for (i= 0, arg= args; i < nargs; i++, arg+= item_sep)
  {
    Item* conv;
    uint32 dummy_offset;
    if (!String::needs_conversion(0, (*arg)->collation.collation,
                                  coll.collation,
                                  &dummy_offset))
      continue;

    if (!(conv= (*arg)->safe_charset_converter(coll.collation)) &&
        ((*arg)->collation.repertoire == MY_REPERTOIRE_ASCII))
      conv= new Item_func_conv_charset(*arg, coll.collation, 1);

    if (!conv)
    {
      if (nargs >=2 && nargs <= 3)
      {
        /* restore the original arguments for better error message */
        args[0]= safe_args[0];
        args[item_sep]= safe_args[1];
      }
      my_coll_agg_error(args, nargs, fname, item_sep);
      res= TRUE;
      break; // we cannot return here, we need to restore "arena".
    }
    if ((*arg)->type() == Item::FIELD_ITEM)
      ((Item_field *)(*arg))->no_const_subst= 1;
    /*
      If in statement prepare, then we create a converter for two
      constant items, do it once and then reuse it.
      If we're in execution of a prepared statement, arena is NULL,
      and the conv was created in runtime memory. This can be
      the case only if the argument is a parameter marker ('?'),
      because for all true constants the charset converter has already
      been created in prepare. In this case register the change for
      rollback.
    */
    if (thd->is_stmt_prepare())
      *arg= conv;
    else
      thd->change_item_tree(arg, conv);
    /*
      We do not check conv->fixed, because Item_func_conv_charset which can
      be return by safe_charset_converter can't be fixed at creation
    */
    conv->fix_fields(thd, arg);
  }
  if (arena)
    thd->restore_active_arena(arena, &backup);
  return res;
}


/* 
  Collect arguments' character sets together.
  We allow to apply automatic character set conversion in some cases.
  The conditions when conversion is possible are:
  - arguments A and B have different charsets
  - A wins according to coercibility rules
    (i.e. a column is stronger than a string constant,
     an explicit COLLATE clause is stronger than a column)
  - character set of A is either superset for character set of B,
    or B is a string constant which can be converted into the
    character set of A without data loss.
    
  If all of the above is true, then it's possible to convert
  B into the character set of A, and then compare according
  to the collation of A.
  
  For functions with more than two arguments:

    collect(A,B,C) ::= collect(collect(A,B),C)

  Since this function calls THD::change_item_tree() on the passed Item **
  pointers, it is necessary to pass the original Item **'s, not copies.
  Otherwise their values will not be properly restored (see BUG#20769).
  If the items are not consecutive (eg. args[2] and args[5]), use the
  item_sep argument, ie.

    agg_item_charsets(coll, fname, &args[2], 2, flags, 3)

*/

bool agg_item_charsets(DTCollation &coll, const char *fname,
                       Item **args, uint nargs, uint flags, int item_sep)
{
  if (agg_item_collations(coll, fname, args, nargs, flags, item_sep))
    return TRUE;

  return agg_item_set_converter(coll, fname, args, nargs, flags, item_sep);
}


void Item_ident_for_show::make_field(Send_field *tmp_field)
{
  tmp_field->table_name= tmp_field->org_table_name= table_name;
  tmp_field->db_name= db_name;
  tmp_field->col_name= tmp_field->org_col_name= field->field_name;
  tmp_field->charsetnr= field->charset()->number;
  tmp_field->length=field->field_length;
  tmp_field->type=field->type();
  tmp_field->flags= field->table->maybe_null ? 
    (field->flags & ~NOT_NULL_FLAG) : field->flags;
  tmp_field->decimals= field->decimals();
}

/**********************************************/

Item_field::Item_field(Field *f)
  :Item_ident(0, NullS, *f->table_name, f->field_name),
   item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  set_field(f);
  /*
    field_name and table_name should not point to garbage
    if this item is to be reused
  */
  orig_table_name= orig_field_name= "";
  with_field= 1;
}


/**
  Constructor used inside setup_wild().

  Ensures that field, table, and database names will live as long as
  Item_field (this is important in prepared statements).
*/

Item_field::Item_field(THD *thd, Name_resolution_context *context_arg,
                       Field *f)
  :Item_ident(context_arg, f->table->s->db.str, *f->table_name, f->field_name),
   item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  /*
    We always need to provide Item_field with a fully qualified field
    name to avoid ambiguity when executing prepared statements like
    SELECT * from d1.t1, d2.t1; (assuming d1.t1 and d2.t1 have columns
    with same names).
    This is because prepared statements never deal with wildcards in
    select list ('*') and always fix fields using fully specified path
    (i.e. db.table.column).
    No check for OOM: if db_name is NULL, we'll just get
    "Field not found" error.
    We need to copy db_name, table_name and field_name because they must
    be allocated in the statement memory, not in table memory (the table
    structure can go away and pop up again between subsequent executions
    of a prepared statement or after the close_tables_for_reopen() call
    in mysql_multi_update_prepare() or due to wildcard expansion in stored
    procedures).
  */
  {
    if (db_name)
      orig_db_name= thd->strdup(db_name);
    if (table_name)
      orig_table_name= thd->strdup(table_name);
    if (field_name)
      orig_field_name= thd->strdup(field_name);
    /*
      We don't restore 'name' in cleanup because it's not changed
      during execution. Still we need it to point to persistent
      memory if this item is to be reused.
    */
    name= (char*) orig_field_name;
  }
  set_field(f);
  with_field= 1;
}


Item_field::Item_field(Name_resolution_context *context_arg,
                       const char *db_arg,const char *table_name_arg,
                       const char *field_name_arg)
  :Item_ident(context_arg, db_arg,table_name_arg,field_name_arg),
   field(0), result_field(0), item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  SELECT_LEX *select= current_thd->lex->current_select;
  collation.set(DERIVATION_IMPLICIT);
  if (select && select->parsing_place != IN_HAVING)
      select->select_n_where_fields++;
  with_field= 1;
}

/**
  Constructor need to process subselect with temporary tables (see Item)
*/

Item_field::Item_field(THD *thd, Item_field *item)
  :Item_ident(thd, item),
   field(item->field),
   result_field(item->result_field),
   item_equal(item->item_equal),
   no_const_subst(item->no_const_subst),
   have_privileges(item->have_privileges),
   any_privileges(item->any_privileges)
{
  collation.set(DERIVATION_IMPLICIT);
  with_field= 1;
}

void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  decimals= field->decimals();
  max_length= field_par->max_display_length();
  table_name= *field_par->table_name;
  field_name= field_par->field_name;
  db_name= field_par->table->s->db.str;
  alias_name_used= field_par->table->alias_name_used;
  unsigned_flag=test(field_par->flags & UNSIGNED_FLAG);
  collation.set(field_par->charset(), field_par->derivation());
  fixed= 1;
  if (field->table->s->tmp_table == SYSTEM_TMP_TABLE)
    any_privileges= 0;
}


/**
  Reset this item to point to a field from the new temporary table.
  This is used when we create a new temporary table for each execution
  of prepared statement.
*/

void Item_field::reset_field(Field *f)
{
  set_field(f);
  /* 'name' is pointing at field->field_name of old field */
  name= (char*) f->field_name;
}


bool Item_field::enumerate_field_refs_processor(uchar *arg)
{
  Field_enumerator *fe= (Field_enumerator*)arg;
  fe->visit_field(this);
  return FALSE;
}

bool Item_field::update_table_bitmaps_processor(uchar *arg)
{
  update_table_bitmaps();
  return FALSE;
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name || !field_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name && db_name[0])
  {
    tmp=(char*) sql_alloc((uint) strlen(db_name)+(uint) strlen(table_name)+
			  (uint) strlen(field_name)+3);
    strxmov(tmp,db_name,".",table_name,".",field_name,NullS);
  }
  else
  {
    if (table_name[0])
    {
      tmp= (char*) sql_alloc((uint) strlen(table_name) +
			     (uint) strlen(field_name) + 2);
      strxmov(tmp, table_name, ".", field_name, NullS);
    }
    else
      tmp= (char*) field_name;
  }
  return tmp;
}

void Item_ident::print(String *str, enum_query_type query_type)
{
  THD *thd= current_thd;
  char d_name_buff[MAX_ALIAS_NAME], t_name_buff[MAX_ALIAS_NAME];
  const char *d_name= db_name, *t_name= table_name;
  if (lower_case_table_names== 1 ||
      (lower_case_table_names == 2 && !alias_name_used))
  {
    if (table_name && table_name[0])
    {
      strmov(t_name_buff, table_name);
      my_casedn_str(files_charset_info, t_name_buff);
      t_name= t_name_buff;
    }
    if (db_name && db_name[0])
    {
      strmov(d_name_buff, db_name);
      my_casedn_str(files_charset_info, d_name_buff);
      d_name= d_name_buff;
    }
  }

  if (!table_name || !field_name || !field_name[0])
  {
    const char *nm= (field_name && field_name[0]) ?
                      field_name : name ? name : "tmp_field";
    append_identifier(thd, str, nm, (uint) strlen(nm));
    return;
  }
  if (db_name && db_name[0] && !alias_name_used)
  {
    if (!(cached_table && cached_table->belong_to_view &&
          cached_table->belong_to_view->compact_view_format))
    {
      append_identifier(thd, str, d_name, (uint)strlen(d_name));
      str->append('.');
    }
    append_identifier(thd, str, t_name, (uint)strlen(t_name));
    str->append('.');
    append_identifier(thd, str, field_name, (uint)strlen(field_name));
  }
  else
  {
    if (table_name[0])
    {
      append_identifier(thd, str, t_name, (uint) strlen(t_name));
      str->append('.');
      append_identifier(thd, str, field_name, (uint) strlen(field_name));
    }
    else
      append_identifier(thd, str, field_name, (uint) strlen(field_name));
  }
}

/* ARGSUSED */
String *Item_field::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return field->val_str(str,&str_value);
}


double Item_field::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}


longlong Item_field::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


my_decimal *Item_field::val_decimal(my_decimal *decimal_value)
{
  if ((null_value= field->is_null()))
    return 0;
  return field->val_decimal(decimal_value);
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return result_field->val_str(str,&str_value);
}

bool Item_field::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  if ((null_value=field->is_null()) || field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

bool Item_field::get_date_result(MYSQL_TIME *ltime,uint fuzzydate)
{
  if ((null_value=result_field->is_null()) ||
      result_field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

void Item_field::save_result(Field *to)
{
  save_field_in_field(result_field, &null_value, to, TRUE);
}


double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

longlong Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}


my_decimal *Item_field::val_decimal_result(my_decimal *decimal_value)
{
  if ((null_value= result_field->is_null()))
    return 0;
  return result_field->val_decimal(decimal_value);
}


bool Item_field::val_bool_result()
{
  if ((null_value= result_field->is_null()))
    return FALSE;
  switch (result_field->result_type()) {
  case INT_RESULT:
    return result_field->val_int() != 0;
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *val= result_field->val_decimal(&decimal_value);
    if (val)
      return !my_decimal_is_zero(val);
    return 0;
  }
  case REAL_RESULT:
  case STRING_RESULT:
    return result_field->val_real() != 0.0;
  case ROW_RESULT:
  case TIME_RESULT:
  case IMPOSSIBLE_RESULT:
    DBUG_ASSERT(0);
    return 0;                                   // Shut up compiler
  }
  return 0;
}


bool Item_field::is_null_result()
{
  return (null_value=result_field->is_null());
}


bool Item_field::eq(const Item *item, bool binary_cmp) const
{
  Item *real_item= ((Item *) item)->real_item();
  if (real_item->type() != FIELD_ITEM)
    return 0;
  
  Item_field *item_field= (Item_field*) real_item;
  if (item_field->field && field)
    return item_field->field == field;
  /*
    We may come here when we are trying to find a function in a GROUP BY
    clause from the select list.
    In this case the '100 % correct' way to do this would be to first
    run fix_fields() on the GROUP BY item and then retry this function, but
    I think it's better to relax the checking a bit as we will in
    most cases do the correct thing by just checking the field name.
    (In cases where we would choose wrong we would have to generate a
    ER_NON_UNIQ_ERROR).
  */
  return (!my_strcasecmp(system_charset_info, item_field->name,
			 field_name) &&
	  (!item_field->table_name || !table_name ||
	   (!my_strcasecmp(table_alias_charset, item_field->table_name,
			   table_name) &&
	    (!item_field->db_name || !db_name ||
	     (item_field->db_name && !strcmp(item_field->db_name,
					     db_name))))));
}


table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return (get_depended_from() ? OUTER_REF_TABLE_BIT : field->table->map);
}

table_map Item_field::all_used_tables() const
{
  return (get_depended_from() ? OUTER_REF_TABLE_BIT : field->table->map);
}

void Item_field::fix_after_pullout(st_select_lex *new_parent, Item **ref)
{
  if (new_parent == get_depended_from())
    depended_from= NULL;
  if (context)
  {
    Name_resolution_context *ctx= new Name_resolution_context();
    ctx->outer_context= NULL; // We don't build a complete name resolver
    ctx->table_list= NULL;    // We rely on first_name_resolution_table instead
    ctx->select_lex= new_parent;
    ctx->first_name_resolution_table= context->first_name_resolution_table;
    ctx->last_name_resolution_table=  context->last_name_resolution_table;
    ctx->error_processor=             context->error_processor;
    ctx->error_processor_data=        context->error_processor_data;
    ctx->resolve_in_select_list=      context->resolve_in_select_list;
    ctx->security_ctx=                context->security_ctx;
    this->context=ctx;
  }
}


Item *Item_field::get_tmp_table_item(THD *thd)
{
  Item_field *new_item= new Item_field(thd, this);
  if (new_item)
    new_item->field= new_item->result_field;
  return new_item;
}

longlong Item_field::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  longlong res= val_int();
  return null_value? LONGLONG_MIN : res;
}

/**
  Create an item from a string we KNOW points to a valid longlong
  end \\0 terminated number string.
  This is always 'signed'. Unsigned values are created with Item_uint()
*/

Item_int::Item_int(const char *str_arg, uint length)
{
  char *end_ptr= (char*) str_arg + length;
  int error;
  value= my_strtoll10(str_arg, &end_ptr, &error);
  max_length= (uint) (end_ptr - str_arg);
  name= (char*) str_arg;
  fixed= 1;
}


my_decimal *Item_int::val_decimal(my_decimal *decimal_value)
{
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_value);
  return decimal_value;
}

String *Item_int::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set_int(value, unsigned_flag, &my_charset_bin);
  return str;
}

void Item_int::print(String *str, enum_query_type query_type)
{
  // my_charset_bin is good enough for numbers
  str_value.set_int(value, unsigned_flag, &my_charset_bin);
  str->append(str_value);
}


Item_uint::Item_uint(const char *str_arg, uint length):
  Item_int(str_arg, length)
{
  unsigned_flag= 1;
}


Item_uint::Item_uint(const char *str_arg, longlong i, uint length):
  Item_int(str_arg, i, length)
{
  unsigned_flag= 1;
}


String *Item_uint::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set((ulonglong) value, &my_charset_bin);
  return str;
}


void Item_uint::print(String *str, enum_query_type query_type)
{
  // latin1 is good enough for numbers
  str_value.set((ulonglong) value, default_charset());
  str->append(str_value);
}


Item_decimal::Item_decimal(const char *str_arg, uint length,
                           CHARSET_INFO *charset)
{
  str2my_decimal(E_DEC_FATAL_ERROR, str_arg, length, charset, &decimal_value);
  name= (char*) str_arg;
  decimals= (uint8) decimal_value.frac;
  fixed= 1;
  max_length= my_decimal_precision_to_length_no_truncation(decimal_value.intg +
                                                           decimals,
                                                           decimals,
                                                           unsigned_flag);
}

Item_decimal::Item_decimal(longlong val, bool unsig)
{
  int2my_decimal(E_DEC_FATAL_ERROR, val, unsig, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  fixed= 1;
  max_length= my_decimal_precision_to_length_no_truncation(decimal_value.intg +
                                                           decimals,
                                                           decimals,
                                                           unsigned_flag);
}


Item_decimal::Item_decimal(double val, int precision, int scale)
{
  double2my_decimal(E_DEC_FATAL_ERROR, val, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  fixed= 1;
  max_length= my_decimal_precision_to_length_no_truncation(decimal_value.intg +
                                                           decimals,
                                                           decimals,
                                                           unsigned_flag);
}


Item_decimal::Item_decimal(const char *str, const my_decimal *val_arg,
                           uint decimal_par, uint length)
{
  my_decimal2decimal(val_arg, &decimal_value);
  name= (char*) str;
  decimals= (uint8) decimal_par;
  max_length= length;
  fixed= 1;
}


Item_decimal::Item_decimal(my_decimal *value_par)
{
  my_decimal2decimal(value_par, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  fixed= 1;
  max_length= my_decimal_precision_to_length_no_truncation(decimal_value.intg +
                                                           decimals,
                                                           decimals,
                                                           unsigned_flag);
}


Item_decimal::Item_decimal(const uchar *bin, int precision, int scale)
{
  binary2my_decimal(E_DEC_FATAL_ERROR, bin,
                    &decimal_value, precision, scale);
  decimals= (uint8) decimal_value.frac;
  fixed= 1;
  max_length= my_decimal_precision_to_length_no_truncation(precision, decimals,
                                                           unsigned_flag);
}


longlong Item_decimal::val_int()
{
  longlong result;
  my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &result);
  return result;
}

double Item_decimal::val_real()
{
  double result;
  my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
  return result;
}

String *Item_decimal::val_str(String *result)
{
  result->set_charset(&my_charset_bin);
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, result);
  return result;
}

void Item_decimal::print(String *str, enum_query_type query_type)
{
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, &str_value);
  str->append(str_value);
}


bool Item_decimal::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type() && item->basic_const_item())
  {
    /*
      We need to cast off const to call val_decimal(). This should
      be OK for a basic constant. Additionally, we can pass 0 as
      a true decimal constant will return its internal decimal
      storage and ignore the argument.
    */
    Item *arg= (Item*) item;
    my_decimal *value= arg->val_decimal(0);
    return !my_decimal_cmp(&decimal_value, value);
  }
  return 0;
}


void Item_decimal::set_decimal_value(my_decimal *value_par)
{
  my_decimal2decimal(value_par, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  unsigned_flag= !decimal_value.sign();
  max_length= my_decimal_precision_to_length_no_truncation(decimal_value.intg +
                                                           decimals,
                                                           decimals,
                                                           unsigned_flag);
}


String *Item_float::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set_real(value,decimals,&my_charset_bin);
  return str;
}


my_decimal *Item_float::val_decimal(my_decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  double2my_decimal(E_DEC_FATAL_ERROR, value, decimal_value);
  return (decimal_value);
}


void Item_string::print(String *str, enum_query_type query_type)
{
  if (query_type != QT_IS && is_cs_specified())
  {
    str->append('_');
    str->append(collation.collation->csname);
  }

  str->append('\'');

  if (query_type != QT_IS ||
      my_charset_same(str_value.charset(), system_charset_info))
  {
    str_value.print(str);
  }
  else
  {
    THD *thd= current_thd;
    LEX_STRING utf8_lex_str;

    thd->convert_string(&utf8_lex_str,
                        system_charset_info,
                        str_value.c_ptr_safe(),
                        str_value.length(),
                        str_value.charset());

    String utf8_str(utf8_lex_str.str,
                    utf8_lex_str.length,
                    system_charset_info);

    utf8_str.print(str);
  }

  str->append('\'');
}


double 
double_from_string_with_check(CHARSET_INFO *cs, const char *cptr,
                              const char *end)
{
  int error;
  char *end_of_num= (char*) end;
  double tmp;

  tmp= my_strntod(cs, (char*) cptr, end - cptr, &end_of_num, &error);
  if (error || (end != end_of_num &&
                !check_if_only_end_space(cs, end_of_num, end)))
  {
    char buff[80];
    strmake(buff, cptr, min(sizeof(buff)-1, (size_t) (end-cptr)));
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "DOUBLE",
                        buff);
  }
  return tmp;
}


double Item_string::val_real()
{
  DBUG_ASSERT(fixed == 1);
  return double_from_string_with_check(str_value.charset(),
                                       str_value.ptr(), 
                                       str_value.ptr() +
                                       str_value.length());
}


longlong 
longlong_from_string_with_check(CHARSET_INFO *cs, const char *cptr,
                                const char *end)
{
  int err;
  longlong tmp;
  char *end_of_num= (char*) end;

  tmp= (*(cs->cset->strtoll10))(cs, cptr, &end_of_num, &err);
  /*
    TODO: Give error if we wanted a signed integer and we got an unsigned
    one
  */
  if (!current_thd->no_errors &&
      (err > 0 ||
       (end != end_of_num && !check_if_only_end_space(cs, end_of_num, end))))
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        cptr);
  }
  return tmp;
}


/**
  @todo
  Give error if we wanted a signed integer and we got an unsigned one
*/
longlong Item_string::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return longlong_from_string_with_check(str_value.charset(), str_value.ptr(),
                                         str_value.ptr()+ str_value.length());
}


my_decimal *Item_string::val_decimal(my_decimal *decimal_value)
{
  return val_decimal_from_string(decimal_value);
}


bool Item_null::eq(const Item *item, bool binary_cmp) const
{ return item->type() == type(); }


double Item_null::val_real()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0.0;
}
longlong Item_null::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0;
}
/* ARGSUSED */
String *Item_null::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0;
}

my_decimal *Item_null::val_decimal(my_decimal *decimal_value)
{
  return 0;
}


Item *Item_null::safe_charset_converter(CHARSET_INFO *tocs)
{
  collation.set(tocs);
  return this;
}

/*********************** Item_param related ******************************/

/** 
  Default function of Item_param::set_param_func, so in case
  of malformed packet the server won't SIGSEGV.
*/

static void
default_set_param_func(Item_param *param,
                       uchar **pos __attribute__((unused)),
                       ulong len __attribute__((unused)))
{
  param->set_null();
}


Item_param::Item_param(uint pos_in_query_arg) :
  state(NO_VALUE),
  item_result_type(STRING_RESULT),
  /* Don't pretend to be a literal unless value for this item is set. */
  item_type(PARAM_ITEM),
  param_type(MYSQL_TYPE_VARCHAR),
  pos_in_query(pos_in_query_arg),
  set_param_func(default_set_param_func),
  limit_clause_param(FALSE)
{
  name= (char*) "?";
  /* 
    Since we can't say whenever this item can be NULL or cannot be NULL
    before mysql_stmt_execute(), so we assuming that it can be NULL until
    value is set.
  */
  maybe_null= 1;
  cnvitem= new Item_string("", 0, &my_charset_bin, DERIVATION_COERCIBLE);
  cnvstr.set(cnvbuf, sizeof(cnvbuf), &my_charset_bin);
}


void Item_param::set_null()
{
  DBUG_ENTER("Item_param::set_null");
  /* These are cleared after each execution by reset() method */
  null_value= 1;
  /* 
    Because of NULL and string values we need to set max_length for each new
    placeholder value: user can submit NULL for any placeholder type, and 
    string length can be different in each execution.
  */
  max_length= 0;
  decimals= 0;
  state= NULL_VALUE;
  item_type= Item::NULL_ITEM;
  DBUG_VOID_RETURN;
}

void Item_param::set_int(longlong i, uint32 max_length_arg)
{
  DBUG_ENTER("Item_param::set_int");
  value.integer= (longlong) i;
  state= INT_VALUE;
  max_length= max_length_arg;
  decimals= 0;
  maybe_null= 0;
  DBUG_VOID_RETURN;
}

void Item_param::set_double(double d)
{
  DBUG_ENTER("Item_param::set_double");
  value.real= d;
  state= REAL_VALUE;
  max_length= DBL_DIG + 8;
  decimals= NOT_FIXED_DEC;
  maybe_null= 0;
  DBUG_VOID_RETURN;
}


/**
  Set decimal parameter value from string.

  @param str      character string
  @param length   string length

  @note
    As we use character strings to send decimal values in
    binary protocol, we use str2my_decimal to convert it to
    internal decimal value.
*/

void Item_param::set_decimal(const char *str, ulong length)
{
  char *end;
  DBUG_ENTER("Item_param::set_decimal");

  end= (char*) str+length;
  str2my_decimal(E_DEC_FATAL_ERROR, str, &decimal_value, &end);
  state= DECIMAL_VALUE;
  decimals= decimal_value.frac;
  max_length=
    my_decimal_precision_to_length_no_truncation(decimal_value.precision(),
                                                 decimals, unsigned_flag);
  maybe_null= 0;
  DBUG_VOID_RETURN;
}


/**
  Set parameter value from MYSQL_TIME value.

  @param tm              datetime value to set (time_type is ignored)
  @param type            type of datetime value
  @param max_length_arg  max length of datetime value as string

  @note
    If we value to be stored is not normalized, zero value will be stored
    instead and proper warning will be produced. This function relies on
    the fact that even wrong value sent over binary protocol fits into
    MAX_DATE_STRING_REP_LENGTH buffer.
*/
void Item_param::set_time(MYSQL_TIME *tm, timestamp_type time_type,
                          uint32 max_length_arg)
{ 
  DBUG_ENTER("Item_param::set_time");

  value.time= *tm;
  value.time.time_type= time_type;

  if (value.time.year > 9999 || value.time.month > 12 ||
      value.time.day > 31 ||
      (time_type != MYSQL_TIMESTAMP_TIME && value.time.hour > 23) ||
      value.time.minute > 59 || value.time.second > 59 ||
      value.time.second_part > TIME_MAX_SECOND_PART)
  {
    Lazy_string_time str(&value.time);
    make_truncated_value_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                 &str, time_type, 0);
    set_zero_time(&value.time, MYSQL_TIMESTAMP_ERROR);
  }

  state= TIME_VALUE;
  maybe_null= 0;
  max_length= max_length_arg;
  decimals= tm->second_part > 0 ? TIME_SECOND_PART_DIGITS : 0;
  DBUG_VOID_RETURN;
}


bool Item_param::set_str(const char *str, ulong length)
{
  DBUG_ENTER("Item_param::set_str");
  /*
    Assign string with no conversion: data is converted only after it's
    been written to the binary log.
  */
  uint dummy_errors;
  if (str_value.copy(str, length, &my_charset_bin, &my_charset_bin,
                     &dummy_errors))
    DBUG_RETURN(TRUE);
  state= STRING_VALUE;
  max_length= length;
  maybe_null= 0;
  /* max_length and decimals are set after charset conversion */
  /* sic: str may be not null-terminated, don't add DBUG_PRINT here */
  DBUG_RETURN(FALSE);
}


bool Item_param::set_longdata(const char *str, ulong length)
{
  DBUG_ENTER("Item_param::set_longdata");

  /*
    If client character set is multibyte, end of long data packet
    may hit at the middle of a multibyte character.  Additionally,
    if binary log is open we must write long data value to the
    binary log in character set of client. This is why we can't
    convert long data to connection character set as it comes
    (here), and first have to concatenate all pieces together,
    write query to the binary log and only then perform conversion.
  */
  if (str_value.length() + length > max_long_data_size)
  {
    my_message(ER_UNKNOWN_ERROR,
               "Parameter of prepared statement which is set through "
               "mysql_send_long_data() is longer than "
               "'max_long_data_size' bytes",
               MYF(0));
    DBUG_RETURN(true);
  }

  if (str_value.append(str, length, &my_charset_bin))
    DBUG_RETURN(TRUE);
  state= LONG_DATA_VALUE;
  maybe_null= 0;

  DBUG_RETURN(FALSE);
}


/**
  Set parameter value from user variable value.

  @param thd   Current thread
  @param entry User variable structure (NULL means use NULL value)

  @retval
    0 OK
  @retval
    1 Out of memory
*/

bool Item_param::set_from_user_var(THD *thd, const user_var_entry *entry)
{
  DBUG_ENTER("Item_param::set_from_user_var");
  if (entry && entry->value)
  {
    item_result_type= entry->type;
    unsigned_flag= entry->unsigned_flag;
    if (limit_clause_param)
    {
      bool unused;
      set_int(entry->val_int(&unused), MY_INT64_NUM_DECIMAL_DIGITS);
      item_type= Item::INT_ITEM;
      DBUG_RETURN(!unsigned_flag && value.integer < 0 ? 1 : 0);
    }
    switch (item_result_type) {
    case REAL_RESULT:
      set_double(*(double*)entry->value);
      item_type= Item::REAL_ITEM;
      param_type= MYSQL_TYPE_DOUBLE;
      break;
    case INT_RESULT:
      set_int(*(longlong*)entry->value, MY_INT64_NUM_DECIMAL_DIGITS);
      item_type= Item::INT_ITEM;
      param_type= MYSQL_TYPE_LONGLONG;
      break;
    case STRING_RESULT:
    {
      CHARSET_INFO *fromcs= entry->collation.collation;
      CHARSET_INFO *tocs= thd->variables.collation_connection;
      uint32 dummy_offset;

      value.cs_info.character_set_of_placeholder= fromcs;
      value.cs_info.character_set_client= thd->variables.character_set_client;
      /*
        Setup source and destination character sets so that they
        are different only if conversion is necessary: this will
        make later checks easier.
      */
      value.cs_info.final_character_set_of_str_value=
        String::needs_conversion(0, fromcs, tocs, &dummy_offset) ?
        tocs : fromcs;
      /*
        Exact value of max_length is not known unless data is converted to
        charset of connection, so we have to set it later.
      */
      item_type= Item::STRING_ITEM;
      param_type= MYSQL_TYPE_VARCHAR;

      if (set_str((const char *)entry->value, entry->length))
        DBUG_RETURN(1);
      break;
    }
    case DECIMAL_RESULT:
    {
      const my_decimal *ent_value= (const my_decimal *)entry->value;
      my_decimal2decimal(ent_value, &decimal_value);
      state= DECIMAL_VALUE;
      decimals= ent_value->frac;
      max_length=
        my_decimal_precision_to_length_no_truncation(ent_value->precision(),
                                                     decimals, unsigned_flag);
      item_type= Item::DECIMAL_ITEM;
      param_type= MYSQL_TYPE_NEWDECIMAL;
      break;
    }
    case ROW_RESULT:
    case TIME_RESULT:
    case IMPOSSIBLE_RESULT:
      DBUG_ASSERT(0);
      set_null();
    }
  }
  else
    set_null();

  DBUG_RETURN(0);
}

/**
  Resets parameter after execution.

  @note
    We clear null_value here instead of setting it in set_* methods,
    because we want more easily handle case for long data.
*/

void Item_param::reset()
{
  DBUG_ENTER("Item_param::reset");
  /* Shrink string buffer if it's bigger than max possible CHAR column */
  if (str_value.alloced_length() > MAX_CHAR_WIDTH)
    str_value.free();
  else
    str_value.length(0);
  str_value_ptr.length(0);
  /*
    We must prevent all charset conversions until data has been written
    to the binary log.
  */
  str_value.set_charset(&my_charset_bin);
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  state= NO_VALUE;
  maybe_null= 1;
  null_value= 0;
  /*
    Don't reset item_type to PARAM_ITEM: it's only needed to guard
    us from item optimizations at prepare stage, when item doesn't yet
    contain a literal of some kind.
    In all other cases when this object is accessed its value is
    set (this assumption is guarded by 'state' and
    DBUG_ASSERTS(state != NO_VALUE) in all Item_param::get_*
    methods).
  */
  DBUG_VOID_RETURN;
}


int Item_param::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();

  switch (state) {
  case INT_VALUE:
    return field->store(value.integer, unsigned_flag);
  case REAL_VALUE:
    return field->store(value.real);
  case DECIMAL_VALUE:
    return field->store_decimal(&decimal_value);
  case TIME_VALUE:
    field->store_time_dec(&value.time, decimals);
    return 0;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return field->store(str_value.ptr(), str_value.length(),
                        str_value.charset());
  case NULL_VALUE:
    return set_field_to_null_with_conversions(field, no_conversions);
  case NO_VALUE:
  default:
    DBUG_ASSERT(0);
  }
  return 1;
}


bool Item_param::get_date(MYSQL_TIME *res, uint fuzzydate)
{
  if (state == TIME_VALUE)
  {
    *res= value.time;
    return 0;
  }
  return Item::get_date(res, fuzzydate);
}


double Item_param::val_real()
{
  switch (state) {
  case REAL_VALUE:
    return value.real;
  case INT_VALUE:
    return (double) value.integer;
  case DECIMAL_VALUE:
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
    return result;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
  {
    int dummy_err;
    char *end_not_used;
    return my_strntod(str_value.charset(), (char*) str_value.ptr(),
                      str_value.length(), &end_not_used, &dummy_err);
  }
  case TIME_VALUE:
    /*
      This works for example when user says SELECT ?+0.0 and supplies
      time value for the placeholder.
    */
    return ulonglong2double(TIME_to_ulonglong(&value.time));
  case NULL_VALUE:
    return 0.0;
  default:
    DBUG_ASSERT(0);
  }
  return 0.0;
} 


longlong Item_param::val_int() 
{ 
  switch (state) {
  case REAL_VALUE:
    return (longlong) rint(value.real);
  case INT_VALUE:
    return value.integer;
  case DECIMAL_VALUE:
  {
    longlong i;
    my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &i);
    return i;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      int dummy_err;
      return my_strntoll(str_value.charset(), str_value.ptr(),
                         str_value.length(), 10, (char**) 0, &dummy_err);
    }
  case TIME_VALUE:
    return (longlong) TIME_to_ulonglong(&value.time);
  case NULL_VALUE:
    return 0; 
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


my_decimal *Item_param::val_decimal(my_decimal *dec)
{
  switch (state) {
  case DECIMAL_VALUE:
    return &decimal_value;
  case REAL_VALUE:
    double2my_decimal(E_DEC_FATAL_ERROR, value.real, dec);
    return dec;
  case INT_VALUE:
    int2my_decimal(E_DEC_FATAL_ERROR, value.integer, unsigned_flag, dec);
    return dec;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    string2my_decimal(E_DEC_FATAL_ERROR, &str_value, dec);
    return dec;
  case TIME_VALUE:
  {
    longlong i= (longlong) TIME_to_ulonglong(&value.time);
    int2my_decimal(E_DEC_FATAL_ERROR, i, 0, dec);
    return dec;
  }
  case NULL_VALUE:
    return 0; 
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


String *Item_param::val_str(String* str) 
{ 
  switch (state) {
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return &str_value_ptr;
  case REAL_VALUE:
    str->set_real(value.real, NOT_FIXED_DEC, &my_charset_bin);
    return str;
  case INT_VALUE:
    str->set(value.integer, &my_charset_bin);
    return str;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) <= 1)
      return str;
    return NULL;
  case TIME_VALUE:
  {
    if (str->reserve(MAX_DATE_STRING_REP_LENGTH))
      break;
    str->length((uint) my_TIME_to_str(&value.time, (char*) str->ptr(),
                decimals));
    str->set_charset(&my_charset_bin);
    return str;
  }
  case NULL_VALUE:
    return NULL; 
  default:
    DBUG_ASSERT(0);
  }
  return str;
}

/**
  Return Param item values in string format, for generating the dynamic 
  query used in update/binary logs.

  @todo
    - Change interface and implementation to fill log data in place
    and avoid one more memcpy/alloc between str and log string.
    - In case of error we need to notify replication
    that binary log contains wrong statement 
*/

const String *Item_param::query_val_str(THD *thd, String* str) const
{
  switch (state) {
  case INT_VALUE:
    str->set_int(value.integer, unsigned_flag, &my_charset_bin);
    break;
  case REAL_VALUE:
    str->set_real(value.real, NOT_FIXED_DEC, &my_charset_bin);
    break;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) > 1)
      return &my_null_string;
    break;
  case TIME_VALUE:
    {
      char *buf, *ptr;
      str->length(0);
      /*
        TODO: in case of error we need to notify replication
        that binary log contains wrong statement 
      */
      if (str->reserve(MAX_DATE_STRING_REP_LENGTH+3))
        break; 

      /* Create date string inplace */
      buf= str->c_ptr_quick();
      ptr= buf;
      *ptr++= '\'';
      ptr+= (uint) my_TIME_to_str(&value.time, ptr, decimals);
      *ptr++= '\'';
      str->length((uint32) (ptr - buf));
      break;
    }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      str->length(0);
      append_query_string(thd, value.cs_info.character_set_client, &str_value,
                          str);
      break;
    }
  case NULL_VALUE:
    return &my_null_string;
  default:
    DBUG_ASSERT(0);
  }
  return str;
}


/**
  Convert string from client character set to the character set of
  connection.
*/

bool Item_param::convert_str_value(THD *thd)
{
  bool rc= FALSE;
  if (state == STRING_VALUE || state == LONG_DATA_VALUE)
  {
    /*
      Check is so simple because all charsets were set up properly
      in setup_one_conversion_function, where typecode of
      placeholder was also taken into account: the variables are different
      here only if conversion is really necessary.
    */
    if (value.cs_info.final_character_set_of_str_value !=
        value.cs_info.character_set_of_placeholder)
    {
      rc= thd->convert_string(&str_value,
                              value.cs_info.character_set_of_placeholder,
                              value.cs_info.final_character_set_of_str_value);
    }
    else
      str_value.set_charset(value.cs_info.final_character_set_of_str_value);
    /* Here str_value is guaranteed to be in final_character_set_of_str_value */

    max_length= str_value.numchars() * str_value.charset()->mbmaxlen;
    decimals= 0;
    /*
      str_value_ptr is returned from val_str(). It must be not alloced
      to prevent it's modification by val_str() invoker.
    */
    str_value_ptr.set(str_value.ptr(), str_value.length(),
                      str_value.charset());
    /* Synchronize item charset with value charset */
    collation.set(str_value.charset(), DERIVATION_COERCIBLE);
  }
  return rc;
}


bool Item_param::basic_const_item() const
{
  if (state == NO_VALUE || state == TIME_VALUE)
    return FALSE;
  return TRUE;
}


Item *
Item_param::clone_item()
{
  /* see comments in the header file */
  switch (state) {
  case NULL_VALUE:
    return new Item_null(name);
  case INT_VALUE:
    return (unsigned_flag ?
            new Item_uint(name, value.integer, max_length) :
            new Item_int(name, value.integer, max_length));
  case REAL_VALUE:
    return new Item_float(name, value.real, decimals, max_length);
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return new Item_string(name, str_value.c_ptr_quick(), str_value.length(),
                           str_value.charset());
  case TIME_VALUE:
    break;
  case NO_VALUE:
  default:
    DBUG_ASSERT(0);
  };
  return 0;
}


bool
Item_param::eq(const Item *arg, bool binary_cmp) const
{
  Item *item;
  if (!basic_const_item() || !arg->basic_const_item() || arg->type() != type())
    return FALSE;
  /*
    We need to cast off const to call val_int(). This should be OK for
    a basic constant.
  */
  item= (Item*) arg;

  switch (state) {
  case NULL_VALUE:
    return TRUE;
  case INT_VALUE:
    return value.integer == item->val_int() &&
           unsigned_flag == item->unsigned_flag;
  case REAL_VALUE:
    return value.real == item->val_real();
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return !sortcmp(&str_value, &item->str_value, collation.collation);
  default:
    break;
  }
  return FALSE;
}

/* End of Item_param related */

void Item_param::print(String *str, enum_query_type query_type)
{
  if (state == NO_VALUE)
  {
    str->append('?');
  }
  else
  {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    String tmp(buffer, sizeof(buffer), &my_charset_bin);
    const String *res;
    res= query_val_str(current_thd, &tmp);
    str->append(*res);
  }
}


/**
  Preserve the original parameter types and values
  when re-preparing a prepared statement.

  @details Copy parameter type information and conversion
  function pointers from a parameter of the old statement
  to the corresponding parameter of the new one.

  Move parameter values from the old parameters to the new
  one. We simply "exchange" the values, which allows
  to save on allocation and character set conversion in
  case a parameter is a string or a blob/clob.

  The old parameter gets the value of this one, which
  ensures that all memory of this parameter is freed
  correctly.

  @param[in]  src   parameter item of the original
                    prepared statement
*/

void
Item_param::set_param_type_and_swap_value(Item_param *src)
{
  unsigned_flag= src->unsigned_flag;
  param_type= src->param_type;
  set_param_func= src->set_param_func;
  item_type= src->item_type;
  item_result_type= src->item_result_type;

  collation.set(src->collation);
  maybe_null= src->maybe_null;
  null_value= src->null_value;
  max_length= src->max_length;
  decimals= src->decimals;
  state= src->state;
  value= src->value;

  decimal_value.swap(src->decimal_value);
  str_value.swap(src->str_value);
  str_value_ptr.swap(src->str_value_ptr);
}

/****************************************************************************
  Item_copy
****************************************************************************/

Item_copy *Item_copy::create (Item *item)
{
  switch (item->result_type())
  {
    case STRING_RESULT:
      return new Item_copy_string (item);
    case REAL_RESULT: 
      return new Item_copy_float (item);
    case INT_RESULT:
      return item->unsigned_flag ? 
        new Item_copy_uint (item) : new Item_copy_int (item);
    case DECIMAL_RESULT:
      return new Item_copy_decimal (item);
    case TIME_RESULT:
    case ROW_RESULT:
  case IMPOSSIBLE_RESULT:
      DBUG_ASSERT (0);
  }
  /* should not happen */
  return NULL;
}

/****************************************************************************
  Item_copy_string
****************************************************************************/

double Item_copy_string::val_real()
{
  int err_not_used;
  char *end_not_used;
  return (null_value ? 0.0 :
          my_strntod(str_value.charset(), (char*) str_value.ptr(),
                     str_value.length(), &end_not_used, &err_not_used));
}

longlong Item_copy_string::val_int()
{
  int err;
  return null_value ? LL(0) : my_strntoll(str_value.charset(),str_value.ptr(),
                                          str_value.length(),10, (char**) 0,
                                          &err); 
}


int Item_copy_string::save_in_field(Field *field, bool no_conversions)
{
  return save_str_value_in_field(field, &str_value);
}


void Item_copy_string::copy()
{
  String *res=item->val_str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::val_str(String *str)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return (String*) 0;
  return &str_value;
}


my_decimal *Item_copy_string::val_decimal(my_decimal *decimal_value)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return (my_decimal *) 0;
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}


/****************************************************************************
  Item_copy_int
****************************************************************************/

void Item_copy_int::copy()
{
  cached_value= item->val_int();
  null_value=item->null_value;
}

static int save_int_value_in_field (Field *, longlong, bool, bool);

int Item_copy_int::save_in_field(Field *field, bool no_conversions)
{
  return save_int_value_in_field(field, cached_value, 
                                 null_value, unsigned_flag);
}


String *Item_copy_int::val_str(String *str)
{
  if (null_value)
    return (String *) 0;

  str->set(cached_value, &my_charset_bin);
  return str;
}


my_decimal *Item_copy_int::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return (my_decimal *) 0;

  int2my_decimal(E_DEC_FATAL_ERROR, cached_value, unsigned_flag, decimal_value);
  return decimal_value;
}


/****************************************************************************
  Item_copy_uint
****************************************************************************/

String *Item_copy_uint::val_str(String *str)
{
  if (null_value)
    return (String *) 0;

  str->set((ulonglong) cached_value, &my_charset_bin);
  return str;
}


/****************************************************************************
  Item_copy_float
****************************************************************************/

String *Item_copy_float::val_str(String *str)
{
  if (null_value)
    return (String *) 0;
  else
  {
    double nr= val_real();
    str->set_real(nr,decimals, &my_charset_bin);
    return str;
  }
}


my_decimal *Item_copy_float::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return (my_decimal *) 0;
  else
  {
    double nr= val_real();
    double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
    return decimal_value;
  }
}


int Item_copy_float::save_in_field(Field *field, bool no_conversions)
{
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(cached_value);
}


/****************************************************************************
  Item_copy_decimal
****************************************************************************/

int Item_copy_decimal::save_in_field(Field *field, bool no_conversions)
{
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store_decimal(&cached_value);
}


String *Item_copy_decimal::val_str(String *result)
{
  if (null_value)
    return (String *) 0;
  result->set_charset(&my_charset_bin);
  my_decimal2string(E_DEC_FATAL_ERROR, &cached_value, 0, 0, 0, result);
  return result;
}


double Item_copy_decimal::val_real()
{
  if (null_value)
    return 0.0;
  else
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, &cached_value, &result);
    return result;
  }
}


longlong Item_copy_decimal::val_int()
{
  if (null_value)
    return LL(0);
  else
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, &cached_value, unsigned_flag, &result);
    return result;
  }
}


void Item_copy_decimal::copy()
{
  my_decimal *nr= item->val_decimal(&cached_value);
  if (nr && nr != &cached_value)
    my_decimal2decimal (nr, &cached_value);
  null_value= item->null_value;
}


/*
  Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd, Item **ref)
{

  // We do not check fields which are fixed during construction
  DBUG_ASSERT(fixed == 0 || basic_const_item());
  fixed= 1;
  return FALSE;
}


void Item_ref_null_helper::save_val(Field *to)
{
  DBUG_ASSERT(fixed == 1);
  (*ref)->save_val(to);
  owner->was_null|= null_value= (*ref)->null_value;
}


double Item_ref_null_helper::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double tmp= (*ref)->val_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


longlong Item_ref_null_helper::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong tmp= (*ref)->val_int_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


my_decimal *Item_ref_null_helper::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  my_decimal *val= (*ref)->val_decimal_result(decimal_value);
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}


bool Item_ref_null_helper::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  bool val= (*ref)->val_bool_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}


String* Item_ref_null_helper::val_str(String* s)
{
  DBUG_ASSERT(fixed == 1);
  String* tmp= (*ref)->str_result(s);
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


bool Item_ref_null_helper::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{  
  return (owner->was_null|= null_value= (*ref)->get_date_result(ltime, fuzzydate));
}


/**
  Mark item and SELECT_LEXs as dependent if item was resolved in
  outer SELECT.

  @param thd             thread handler
  @param last            select from which current item depend
  @param current         current select
  @param resolved_item   item which was resolved in outer SELECT(for warning)
  @param mark_item       item which should be marked (can be differ in case of
                         substitution)
*/

static bool mark_as_dependent(THD *thd, SELECT_LEX *last, SELECT_LEX *current,
                              Item_ident *resolved_item,
                              Item_ident *mark_item)
{
  const char *db_name= (resolved_item->db_name ?
                        resolved_item->db_name : "");
  const char *table_name= (resolved_item->table_name ?
                           resolved_item->table_name : "");
  /* store pointer on SELECT_LEX from which item is dependent */
  if (mark_item && mark_item->can_be_depended)
    mark_item->depended_from= last;
  if (current->mark_as_dependent(thd, last, /** resolved_item psergey-thu
    **/mark_item))
    return TRUE;
  if (thd->lex->describe & DESCRIBE_EXTENDED)
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_WARN_FIELD_RESOLVED, ER(ER_WARN_FIELD_RESOLVED),
                 db_name, (db_name[0] ? "." : ""),
                 table_name, (table_name [0] ? "." : ""),
                 resolved_item->field_name,
                 current->select_number, last->select_number);
  }
  return FALSE;
}


/**
  Mark range of selects and resolved identifier (field/reference)
  item as dependent.

  @param thd             thread handler
  @param last_select     select where resolved_item was resolved
  @param current_sel     current select (select where resolved_item was placed)
  @param found_field     field which was found during resolving
  @param found_item      Item which was found during resolving (if resolved
                         identifier belongs to VIEW)
  @param resolved_item   Identifier which was resolved

  @note
    We have to mark all items between current_sel (including) and
    last_select (excluding) as dependend (select before last_select should
    be marked with actual table mask used by resolved item, all other with
    OUTER_REF_TABLE_BIT) and also write dependence information to Item of
    resolved identifier.
*/

void mark_select_range_as_dependent(THD *thd,
                                    SELECT_LEX *last_select,
                                    SELECT_LEX *current_sel,
                                    Field *found_field, Item *found_item,
                                    Item_ident *resolved_item)
{
  /*
    Go from current SELECT to SELECT where field was resolved (it
    have to be reachable from current SELECT, because it was already
    done once when we resolved this field and cached result of
    resolving)
  */
  SELECT_LEX *previous_select= current_sel;
  for (; previous_select->outer_select() != last_select;
       previous_select= previous_select->outer_select())
  {
    Item_subselect *prev_subselect_item=
      previous_select->master_unit()->item;
    prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
    prev_subselect_item->const_item_cache= 0;
  }
  {
    Item_subselect *prev_subselect_item=
      previous_select->master_unit()->item;
    Item_ident *dependent= resolved_item;
    if (found_field == view_ref_found)
    {
      Item::Type type= found_item->type();
      prev_subselect_item->used_tables_cache|=
        found_item->used_tables();
      dependent= ((type == Item::REF_ITEM || type == Item::FIELD_ITEM) ?
                  (Item_ident*) found_item :
                  0);
    }
    else
      prev_subselect_item->used_tables_cache|=
        found_field->table->map;
    prev_subselect_item->const_item_cache= 0;
    mark_as_dependent(thd, last_select, current_sel, resolved_item,
                      dependent);
  }
}


/**
  Search a GROUP BY clause for a field with a certain name.

  Search the GROUP BY list for a column named as find_item. When searching
  preference is given to columns that are qualified with the same table (and
  database) name as the one being searched for.

  @param find_item     the item being searched for
  @param group_list    GROUP BY clause

  @return
    - the found item on success
    - NULL if find_item is not in group_list
*/

static Item** find_field_in_group_list(Item *find_item, ORDER *group_list)
{
  const char *db_name;
  const char *table_name;
  const char *field_name;
  ORDER      *found_group= NULL;
  int         found_match_degree= 0;
  Item_ident *cur_field;
  int         cur_match_degree= 0;
  char        name_buff[SAFE_NAME_LEN+1];

  if (find_item->type() == Item::FIELD_ITEM ||
      find_item->type() == Item::REF_ITEM)
  {
    db_name=    ((Item_ident*) find_item)->db_name;
    table_name= ((Item_ident*) find_item)->table_name;
    field_name= ((Item_ident*) find_item)->field_name;
  }
  else
    return NULL;

  if (db_name && lower_case_table_names)
  {
    /* Convert database to lower case for comparison */
    strmake(name_buff, db_name, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db_name= name_buff;
  }

  DBUG_ASSERT(field_name != 0);

  for (ORDER *cur_group= group_list ; cur_group ; cur_group= cur_group->next)
  {
    if ((*(cur_group->item))->real_item()->type() == Item::FIELD_ITEM)
    {
      cur_field= (Item_ident*) *cur_group->item;
      cur_match_degree= 0;
      
      DBUG_ASSERT(cur_field->field_name != 0);

      if (!my_strcasecmp(system_charset_info,
                         cur_field->field_name, field_name))
        ++cur_match_degree;
      else
        continue;

      if (cur_field->table_name && table_name)
      {
        /* If field_name is qualified by a table name. */
        if (my_strcasecmp(table_alias_charset, cur_field->table_name, table_name))
          /* Same field names, different tables. */
          return NULL;

        ++cur_match_degree;
        if (cur_field->db_name && db_name)
        {
          /* If field_name is also qualified by a database name. */
          if (strcmp(cur_field->db_name, db_name))
            /* Same field names, different databases. */
            return NULL;
          ++cur_match_degree;
        }
      }

      if (cur_match_degree > found_match_degree)
      {
        found_match_degree= cur_match_degree;
        found_group= cur_group;
      }
      else if (found_group && (cur_match_degree == found_match_degree) &&
               ! (*(found_group->item))->eq(cur_field, 0))
      {
        /*
          If the current resolve candidate matches equally well as the current
          best match, they must reference the same column, otherwise the field
          is ambiguous.
        */
        my_error(ER_NON_UNIQ_ERROR, MYF(0),
                 find_item->full_name(), current_thd->where);
        return NULL;
      }
    }
  }

  if (found_group)
    return found_group->item;
  else
    return NULL;
}


/**
  Resolve a column reference in a sub-select.

  Resolve a column reference (usually inside a HAVING clause) against the
  SELECT and GROUP BY clauses of the query described by 'select'. The name
  resolution algorithm searches both the SELECT and GROUP BY clauses, and in
  case of a name conflict prefers GROUP BY column names over SELECT names. If
  both clauses contain different fields with the same names, a warning is
  issued that name of 'ref' is ambiguous. We extend ANSI SQL in that when no
  GROUP BY column is found, then a HAVING name is resolved as a possibly
  derived SELECT column. This extension is allowed only if the
  MODE_ONLY_FULL_GROUP_BY sql mode isn't enabled.

  @param thd     current thread
  @param ref     column reference being resolved
  @param select  the select that ref is resolved against

  @note
    The resolution procedure is:
    - Search for a column or derived column named col_ref_i [in table T_j]
    in the SELECT clause of Q.
    - Search for a column named col_ref_i [in table T_j]
    in the GROUP BY clause of Q.
    - If found different columns with the same name in GROUP BY and SELECT
    - issue a warning and return the GROUP BY column,
    - otherwise
    - if the MODE_ONLY_FULL_GROUP_BY mode is enabled return error
    - else return the found SELECT column.


  @return
    - NULL - there was an error, and the error was already reported
    - not_found_item - the item was not resolved, no error was reported
    - resolved item - if the item was resolved
*/

static Item**
resolve_ref_in_select_and_group(THD *thd, Item_ident *ref, SELECT_LEX *select)
{
  Item **group_by_ref= NULL;
  Item **select_ref= NULL;
  ORDER *group_list= select->group_list.first;
  bool ambiguous_fields= FALSE;
  uint counter;
  enum_resolution_type resolution;

  /*
    Search for a column or derived column named as 'ref' in the SELECT
    clause of the current select.
  */
  if (!(select_ref= find_item_in_list(ref, *(select->get_item_list()),
                                      &counter, REPORT_EXCEPT_NOT_FOUND,
                                      &resolution)))
    return NULL; /* Some error occurred. */
  if (resolution == RESOLVED_AGAINST_ALIAS)
    ref->alias_name_used= TRUE;

  /* If this is a non-aggregated field inside HAVING, search in GROUP BY. */
  if (select->having_fix_field && !ref->with_sum_func && group_list)
  {
    group_by_ref= find_field_in_group_list(ref, group_list);
    
    /* Check if the fields found in SELECT and GROUP BY are the same field. */
    if (group_by_ref && (select_ref != not_found_item) &&
        !((*group_by_ref)->eq(*select_ref, 0)))
    {
      ambiguous_fields= TRUE;
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR), ref->full_name(),
                          current_thd->where);

    }
  }

  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY &&
      select->having_fix_field  &&
      select_ref != not_found_item && !group_by_ref)
  {
    /*
      Report the error if fields was found only in the SELECT item list and
      the strict mode is enabled.
    */
    my_error(ER_NON_GROUPING_FIELD_USED, MYF(0),
             ref->name, "HAVING");
    return NULL;
  }
  if (select_ref != not_found_item || group_by_ref)
  {
    if (select_ref != not_found_item && !ambiguous_fields)
    {
      DBUG_ASSERT(*select_ref != 0);
      if (!select->ref_pointer_array[counter])
      {
        my_error(ER_ILLEGAL_REFERENCE, MYF(0),
                 ref->name, "forward reference in item list");
        return NULL;
      }
      DBUG_ASSERT((*select_ref)->fixed);
      return (select->ref_pointer_array + counter);
    }
    if (group_by_ref)
      return group_by_ref;
    DBUG_ASSERT(FALSE);
    return NULL; /* So there is no compiler warning. */
  }

  return (Item**) not_found_item;
}


/*
  @brief
  Whether a table belongs to an outer select.

  @param table table to check
  @param select current select

  @details
  Try to find select the table belongs to by ascending the derived tables chain.
*/

static
bool is_outer_table(TABLE_LIST *table, SELECT_LEX *select)
{
  DBUG_ASSERT(table->select_lex != select);
  TABLE_LIST *tl;

  for (tl= select->master_unit()->derived;
       tl && tl->is_merged_derived();
       select= tl->select_lex, tl= select->master_unit()->derived)
  {
    if (tl->select_lex == table->select_lex)
      return FALSE;
  }
  return TRUE;
}


/**
  Resolve the name of an outer select column reference.

  The method resolves the column reference represented by 'this' as a column
  present in outer selects that contain current select.

  In prepared statements, because of cache, find_field_in_tables()
  can resolve fields even if they don't belong to current context.
  In this case this method only finds appropriate context and marks
  current select as dependent. The found reference of field should be
  provided in 'from_field'.

  @param[in] thd             current thread
  @param[in,out] from_field  found field reference or (Field*)not_found_field
  @param[in,out] reference   view column if this item was resolved to a
    view column

  @note
    This is the inner loop of Item_field::fix_fields:
  @code
        for each outer query Q_k beginning from the inner-most one
        {
          search for a column or derived column named col_ref_i
          [in table T_j] in the FROM clause of Q_k;

          if such a column is not found
            Search for a column or derived column named col_ref_i
            [in table T_j] in the SELECT and GROUP clauses of Q_k.
        }
  @endcode

  @retval
    1   column succefully resolved and fix_fields() should continue.
  @retval
    0   column fully fixed and fix_fields() should return FALSE
  @retval
    -1  error occured
*/

int
Item_field::fix_outer_field(THD *thd, Field **from_field, Item **reference)
{
  enum_parsing_place place= NO_MATTER;
  bool field_found= (*from_field != not_found_field);
  bool upward_lookup= FALSE;

  /*
    If there are outer contexts (outer selects, but current select is
    not derived table or view) try to resolve this reference in the
    outer contexts.

    We treat each subselect as a separate namespace, so that different
    subselects may contain columns with the same names. The subselects
    are searched starting from the innermost.
  */
  Name_resolution_context *last_checked_context= context;
  Item **ref= (Item **) not_found_item;
  SELECT_LEX *current_sel= (SELECT_LEX *) thd->lex->current_select;
  Name_resolution_context *outer_context= 0;
  SELECT_LEX *select= 0;
  /* Currently derived tables cannot be correlated */
  if (current_sel->master_unit()->first_select()->linkage !=
      DERIVED_TABLE_TYPE)
    outer_context= context->outer_context;
  for (;
       outer_context;
       outer_context= outer_context->outer_context)
  {
    select= outer_context->select_lex;
    Item_subselect *prev_subselect_item=
      last_checked_context->select_lex->master_unit()->item;
    last_checked_context= outer_context;
    upward_lookup= TRUE;

    place= prev_subselect_item->parsing_place;
    /*
      If outer_field is set, field was already found by first call
      to find_field_in_tables(). Only need to find appropriate context.
    */
    if (field_found && outer_context->select_lex !=
        cached_table->select_lex)
      continue;
    /*
      In case of a view, find_field_in_tables() writes the pointer to
      the found view field into '*reference', in other words, it
      substitutes this Item_field with the found expression.
    */
    if (field_found || (*from_field= find_field_in_tables(thd, this,
                                          outer_context->
                                            first_name_resolution_table,
                                          outer_context->
                                            last_name_resolution_table,
                                          reference,
                                          IGNORE_EXCEPT_NON_UNIQUE,
                                          TRUE, TRUE)) !=
        not_found_field)
    {
      if (*from_field)
      {
        if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY &&
            select->cur_pos_in_select_list != UNDEF_POS)
        {
          /*
            As this is an outer field it should be added to the list of
            non aggregated fields of the outer select.
          */
          marker= select->cur_pos_in_select_list;
          select->non_agg_fields.push_back(this);
        }
        if (*from_field != view_ref_found)
        {
          prev_subselect_item->used_tables_cache|= (*from_field)->table->map;
          prev_subselect_item->const_item_cache= 0;
          set_field(*from_field);
          if (!last_checked_context->select_lex->having_fix_field &&
              select->group_list.elements &&
              (place == SELECT_LIST || place == IN_HAVING))
          {
            Item_outer_ref *rf;
            /*
              If an outer field is resolved in a grouping select then it
              is replaced for an Item_outer_ref object. Otherwise an
              Item_field object is used.
              The new Item_outer_ref object is saved in the inner_refs_list of
              the outer select. Here it is only created. It can be fixed only
              after the original field has been fixed and this is done in the
              fix_inner_refs() function.
            */
            ;
            if (!(rf= new Item_outer_ref(context, this)))
              return -1;
            thd->change_item_tree(reference, rf);
            select->inner_refs_list.push_back(rf);
            rf->in_sum_func= thd->lex->in_sum_func;
          }
          /*
            A reference is resolved to a nest level that's outer or the same as
            the nest level of the enclosing set function : adjust the value of
            max_arg_level for the function if it's needed.
          */
          if (thd->lex->in_sum_func &&
              thd->lex->in_sum_func->nest_level >= select->nest_level)
          {
            Item::Type ref_type= (*reference)->type();
            set_if_bigger(thd->lex->in_sum_func->max_arg_level,
                          select->nest_level);
            set_field(*from_field);
            fixed= 1;
            mark_as_dependent(thd, last_checked_context->select_lex,
                              context->select_lex, this,
                              ((ref_type == REF_ITEM ||
                                ref_type == FIELD_ITEM) ?
                               (Item_ident*) (*reference) : 0));
            return 0;
          }
        }
        else
        {
          Item::Type ref_type= (*reference)->type();
          prev_subselect_item->used_tables_cache|=
            (*reference)->used_tables();
          prev_subselect_item->const_item_cache&=
            (*reference)->const_item();
          mark_as_dependent(thd, last_checked_context->select_lex,
                            context->select_lex, this,
                            ((ref_type == REF_ITEM || ref_type == FIELD_ITEM) ?
                             (Item_ident*) (*reference) :
                             0));
          /*
            A reference to a view field had been found and we
            substituted it instead of this Item (find_field_in_tables
            does it by assigning the new value to *reference), so now
            we can return from this function.
          */
          return 0;
        }
      }
      break;
    }

    /* Search in SELECT and GROUP lists of the outer select. */
    if (place != IN_WHERE && place != IN_ON)
    {
      if (!(ref= resolve_ref_in_select_and_group(thd, this, select)))
        return -1; /* Some error occurred (e.g. ambiguous names). */
      if (ref != not_found_item)
      {
        DBUG_ASSERT(*ref && (*ref)->fixed);
        prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
        prev_subselect_item->const_item_cache&= (*ref)->const_item();
        break;
      }
    }

    /*
      Reference is not found in this select => this subquery depend on
      outer select (or we just trying to find wrong identifier, in this
      case it does not matter which used tables bits we set)
    */
    prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
    prev_subselect_item->const_item_cache= 0;
  }

  DBUG_ASSERT(ref != 0);
  if (!*from_field)
    return -1;
  if (ref == not_found_item && *from_field == not_found_field)
  {
    if (upward_lookup)
    {
      // We can't say exactly what absent table or field
      my_error(ER_BAD_FIELD_ERROR, MYF(0), full_name(), thd->where);
    }
    else
    {
      /* Call find_field_in_tables only to report the error */
      find_field_in_tables(thd, this,
                           context->first_name_resolution_table,
                           context->last_name_resolution_table,
                           reference, REPORT_ALL_ERRORS,
                           !any_privileges, TRUE);
    }
    return -1;
  }
  else if (ref != not_found_item)
  {
    Item *save;
    Item_ref *rf;

    /* Should have been checked in resolve_ref_in_select_and_group(). */
    DBUG_ASSERT(*ref && (*ref)->fixed);
    /*
      Here, a subset of actions performed by Item_ref::set_properties
      is not enough. So we pass ptr to NULL into Item_[direct]_ref
      constructor, so no initialization is performed, and call 
      fix_fields() below.
    */
    save= *ref;
    *ref= NULL;                             // Don't call set_properties()
    rf= (place == IN_HAVING ?
         new Item_ref(context, ref, (char*) table_name,
                      (char*) field_name, alias_name_used) :
         (!select->group_list.elements ?
         new Item_direct_ref(context, ref, (char*) table_name,
                             (char*) field_name, alias_name_used) :
         new Item_outer_ref(context, ref, (char*) table_name,
                            (char*) field_name, alias_name_used)));
    *ref= save;
    if (!rf)
      return -1;

    if (place != IN_HAVING && select->group_list.elements)
    {
      outer_context->select_lex->inner_refs_list.push_back((Item_outer_ref*)rf);
      ((Item_outer_ref*)rf)->in_sum_func= thd->lex->in_sum_func;
    }
    thd->change_item_tree(reference, rf);
    /*
      rf is Item_ref => never substitute other items (in this case)
      during fix_fields() => we can use rf after fix_fields()
    */
    DBUG_ASSERT(!rf->fixed);                // Assured by Item_ref()
    if (rf->fix_fields(thd, reference) || rf->check_cols(1))
      return -1;

    mark_as_dependent(thd, last_checked_context->select_lex,
                      context->select_lex, rf,
                      rf);

    return 0;
  }
  else
  {
    mark_as_dependent(thd, last_checked_context->select_lex,
                      context->select_lex,
                      this, (Item_ident*)*reference);
    if (last_checked_context->select_lex->having_fix_field)
    {
      Item_ref *rf;
      rf= new Item_ref(context,
                       (cached_table->db[0] ? cached_table->db : 0),
                       (char*) cached_table->alias, (char*) field_name);
      if (!rf)
        return -1;
      thd->change_item_tree(reference, rf);
      /*
        rf is Item_ref => never substitute other items (in this case)
        during fix_fields() => we can use rf after fix_fields()
      */
      DBUG_ASSERT(!rf->fixed);                // Assured by Item_ref()
      if (rf->fix_fields(thd, reference) || rf->check_cols(1))
        return -1;
      return 0;
    }
  }
  return 1;
}


/**
  Resolve the name of a column reference.

  The method resolves the column reference represented by 'this' as a column
  present in one of: FROM clause, SELECT clause, GROUP BY clause of a query
  Q, or in outer queries that contain Q.

  The name resolution algorithm used is (where [T_j] is an optional table
  name that qualifies the column name):

  @code
    resolve_column_reference([T_j].col_ref_i)
    {
      search for a column or derived column named col_ref_i
      [in table T_j] in the FROM clause of Q;

      if such a column is NOT found AND    // Lookup in outer queries.
         there are outer queries
      {
        for each outer query Q_k beginning from the inner-most one
        {
          search for a column or derived column named col_ref_i
          [in table T_j] in the FROM clause of Q_k;

          if such a column is not found
            Search for a column or derived column named col_ref_i
            [in table T_j] in the SELECT and GROUP clauses of Q_k.
        }
      }
    }
  @endcode

    Notice that compared to Item_ref::fix_fields, here we first search the FROM
    clause, and then we search the SELECT and GROUP BY clauses.

  @param[in]     thd        current thread
  @param[in,out] reference  view column if this item was resolved to a
    view column

  @retval
    TRUE  if error
  @retval
    FALSE on success
*/

bool Item_field::fix_fields(THD *thd, Item **reference)
{
  DBUG_ASSERT(fixed == 0);
  Field *from_field= (Field *)not_found_field;
  bool outer_fixed= false;

  if (!field)					// If field is not checked
  {
    /*
      In case of view, find_field_in_tables() write pointer to view field
      expression to 'reference', i.e. it substitute that expression instead
      of this Item_field
    */
    if ((from_field= find_field_in_tables(thd, this,
                                          context->first_name_resolution_table,
                                          context->last_name_resolution_table,
                                          reference,
                                          thd->lex->use_only_table_context ?
                                            REPORT_ALL_ERRORS : 
                                            IGNORE_EXCEPT_NON_UNIQUE,
                                          !any_privileges,
                                          TRUE)) ==
	not_found_field)
    {
      int ret;
      /* Look up in current select's item_list to find aliased fields */
      if (thd->lex->current_select->is_item_list_lookup)
      {
        uint counter;
        enum_resolution_type resolution;
        Item** res= find_item_in_list(this, thd->lex->current_select->item_list,
                                      &counter, REPORT_EXCEPT_NOT_FOUND,
                                      &resolution);
        if (!res)
          return 1;
        if (resolution == RESOLVED_AGAINST_ALIAS)
          alias_name_used= TRUE;
        if (res != (Item **)not_found_item)
        {
          if ((*res)->type() == Item::FIELD_ITEM)
          {
            /*
              It's an Item_field referencing another Item_field in the select
              list.
              Use the field from the Item_field in the select list and leave
              the Item_field instance in place.
            */

            Field *new_field= (*((Item_field**)res))->field;

            if (new_field == NULL)
            {
              /* The column to which we link isn't valid. */
              my_error(ER_BAD_FIELD_ERROR, MYF(0), (*res)->name, 
                       current_thd->where);
              return(1);
            }

            set_field(new_field);
            return 0;
          }
          else
          {
            /*
              It's not an Item_field in the select list so we must make a new
              Item_ref to point to the Item in the select list and replace the
              Item_field created by the parser with the new Item_ref.
            */
            Item_ref *rf= new Item_ref(context, db_name,table_name,field_name);
            if (!rf)
              return 1;
            bool ret= rf->fix_fields(thd, (Item **) &rf) || rf->check_cols(1);
            if (ret)
              return TRUE;
           
            SELECT_LEX *select= thd->lex->current_select;
            thd->change_item_tree(reference,
                                  select->parsing_place == IN_GROUP_BY && 
				  alias_name_used  ?  *rf->ref : rf);

            return FALSE;
          }
        }
      }
      if ((ret= fix_outer_field(thd, &from_field, reference)) < 0)
        goto error;
      outer_fixed= TRUE;
      if (!ret)
        goto mark_non_agg_field;
    }
    else if (!from_field)
      goto error;

    if (!outer_fixed && cached_table && cached_table->select_lex &&
        context->select_lex &&
        cached_table->select_lex != context->select_lex &&
        !context->select_lex->is_merged_child_of(cached_table->select_lex) &&
        is_outer_table(cached_table, context->select_lex))
    {
      int ret;
      if ((ret= fix_outer_field(thd, &from_field, reference)) < 0)
        goto error;
      outer_fixed= 1;
      if (!ret)
        goto mark_non_agg_field;
    }

    /*
      if it is not expression from merged VIEW we will set this field.

      We can leave expression substituted from view for next PS/SP rexecution
      (i.e. do not register this substitution for reverting on cleanup()
      (register_item_tree_changing())), because this subtree will be
      fix_field'ed during setup_tables()->setup_underlying() (i.e. before
      all other expressions of query, and references on tables which do
      not present in query will not make problems.

      Also we suppose that view can't be changed during PS/SP life.
    */
    if (from_field == view_ref_found)
      return FALSE;

    set_field(from_field);
    if (thd->lex->in_sum_func &&
        thd->lex->in_sum_func->nest_level == 
        thd->lex->current_select->nest_level)
      set_if_bigger(thd->lex->in_sum_func->max_arg_level,
                    thd->lex->current_select->nest_level);
  }
  else if (thd->mark_used_columns != MARK_COLUMNS_NONE)
  {
    TABLE *table= field->table;
    MY_BITMAP *current_bitmap, *other_bitmap;
    if (thd->mark_used_columns == MARK_COLUMNS_READ)
    {
      current_bitmap= table->read_set;
      other_bitmap=   table->write_set;
    }
    else
    {
      current_bitmap= table->write_set;
      other_bitmap=   table->read_set;
    }
    if (!bitmap_fast_test_and_set(current_bitmap, field->field_index))
    {
      if (!bitmap_is_set(other_bitmap, field->field_index))
      {
        /* First usage of column */
        table->used_fields++;                     // Used to optimize loops
        /* purecov: begin inspected */
        table->covering_keys.intersect(field->part_of_key);
        /* purecov: end */
      }
    }
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (any_privileges)
  {
    char *db, *tab;
    db= cached_table->get_db_name();
    tab= cached_table->get_table_name();
    if (!(have_privileges= (get_column_grant(thd, &field->table->grant,
                                             db, tab, field_name) &
                            VIEW_ANY_ACL)))
    {
      my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
               "ANY", thd->security_ctx->priv_user,
               thd->security_ctx->host_or_ip, field_name, tab);
      goto error;
    }
  }
#endif
  fixed= 1;
  if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY &&
      !outer_fixed && !thd->lex->in_sum_func &&
      thd->lex->current_select->cur_pos_in_select_list != UNDEF_POS)
  {
    thd->lex->current_select->non_agg_fields.push_back(this);
    marker= thd->lex->current_select->cur_pos_in_select_list;
  }
mark_non_agg_field:
  if (fixed && thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
  {
    /*
      Mark selects according to presence of non aggregated fields.
      Fields from outer selects added to the aggregate function
      outer_fields list as its unknown at the moment whether it's
      aggregated or not.
      We're using either the select lex of the cached table (if present)
      or the field's resolution context. context->select_lex is 
      safe for use because it's either the SELECT we want to use 
      (the current level) or a stub added by non-SELECT queries.
    */
    SELECT_LEX *select_lex= cached_table ? 
      cached_table->select_lex : context->select_lex;
    if (!thd->lex->in_sum_func)
      select_lex->set_non_agg_field_used(true);
    else
    {
      if (outer_fixed)
        thd->lex->in_sum_func->outer_fields.push_back(this);
      else if (thd->lex->in_sum_func->nest_level !=
          thd->lex->current_select->nest_level)
        select_lex->set_non_agg_field_used(true);
    }
  }
  return FALSE;

error:
  context->process_error(thd);
  return TRUE;
}

/*
  @brief
  Mark virtual columns as used in a partitioning expression 
*/

bool Item_field::vcol_in_partition_func_processor(uchar *int_arg)
{
  DBUG_ASSERT(fixed);
  if (field->vcol_info)
  {
    field->vcol_info->mark_as_in_partitioning_expr();
  }
  return FALSE;
}


Item *Item_field::safe_charset_converter(CHARSET_INFO *tocs)
{
  no_const_subst= 1;
  return Item::safe_charset_converter(tocs);
}


void Item_field::cleanup()
{
  DBUG_ENTER("Item_field::cleanup");
  Item_ident::cleanup();
  depended_from= NULL;
  /*
    Even if this object was created by direct link to field in setup_wild()
    it will be linked correctly next time by name of field and table alias.
    I.e. we can drop 'field'.
   */
  field= result_field= 0;
  item_equal= NULL;
  null_value= FALSE;
  DBUG_VOID_RETURN;
}

/**
  Find a field among specified multiple equalities.

  The function first searches the field among multiple equalities
  of the current level (in the cond_equal->current_level list).
  If it fails, it continues searching in upper levels accessed
  through a pointer cond_equal->upper_levels.
  The search terminates as soon as a multiple equality containing 
  the field is found. 

  @param cond_equal   reference to list of multiple equalities where
                      the field (this object) is to be looked for

  @return
    - First Item_equal containing the field, if success
    - 0, otherwise
*/

Item_equal *Item_field::find_item_equal(COND_EQUAL *cond_equal)
{
  Item_equal *item= 0;
  while (cond_equal)
  {
    List_iterator_fast<Item_equal> li(cond_equal->current_level);
    while ((item= li++))
    {
      if (item->contains(field))
        return item;
    }
    /* 
      The field is not found in any of the multiple equalities
      of the current level. Look for it in upper levels
    */
    cond_equal= cond_equal->upper_levels;
  }
  return 0;
}


/**
  Check whether a field item can be substituted for an equal item

  @details
  The function checks whether a substitution of a field item for
  an equal item is valid.

  @param arg   *arg != NULL <-> the field is in the context
               where substitution for an equal item is valid

  @note
    The following statement is not always true:
  @n
    x=y => F(x)=F(x/y).
  @n
    This means substitution of an item for an equal item not always
    yields an equavalent condition. Here's an example:
    @code
    'a'='a '
    (LENGTH('a')=1) != (LENGTH('a ')=2)
  @endcode
    Such a substitution is surely valid if either the substituted
    field is not of a STRING type or if it is an argument of
    a comparison predicate.

  @retval
    TRUE   substitution is valid
  @retval
    FALSE  otherwise
*/

bool Item_field::subst_argument_checker(uchar **arg)
{
  return *arg &&
         (*arg == (uchar *) Item::ANY_SUBST ||
          result_type() != STRING_RESULT || 
          (field->flags & BINARY_FLAG));
}


/**
  Convert a numeric value to a zero-filled string

  @param[in,out]  item   the item to operate on
  @param          field  The field that this value is equated to

  This function converts a numeric value to a string. In this conversion
  the zero-fill flag of the field is taken into account.
  This is required so the resulting string value can be used instead of
  the field reference when propagating equalities.
*/

static void convert_zerofill_number_to_string(Item **item, Field_num *field)
{
  char buff[MAX_FIELD_WIDTH],*pos;
  String tmp(buff,sizeof(buff), field->charset()), *res;

  res= (*item)->val_str(&tmp);
  if ((*item)->is_null())
    *item= new Item_null();
  else
  {
    field->prepend_zeros(res);
    pos= (char *) sql_strmake (res->ptr(), res->length());
    *item= new Item_string(pos, res->length(), field->charset());
  }
}


/**
  Set a pointer to the multiple equality the field reference belongs to
  (if any).

  @details
  The function looks for a multiple equality containing the field item
  among those referenced by arg.
  In the case such equality exists the function does the following.
  If the found multiple equality contains a constant, then the field
  reference is substituted for this constant, otherwise it sets a pointer
  to the multiple equality in the field item.


  @param arg    reference to list of multiple equalities where
                the field (this object) is to be looked for

  @note
    This function is supposed to be called as a callback parameter in calls
    of the compile method.

  @return
    - pointer to the replacing constant item, if the field item was substituted
    - pointer to the field item, otherwise.
*/

Item *Item_field::equal_fields_propagator(uchar *arg)
{
  if (no_const_subst)
    return this;
  item_equal= find_item_equal((COND_EQUAL *) arg);
  Item *item= 0;
  if (item_equal)
    item= item_equal->get_const();
  /*
    Disable const propagation for items used in different comparison contexts.
    This must be done because, for example, Item_hex_string->val_int() is not
    the same as (Item_hex_string->val_str() in BINARY column)->val_int().
    We cannot simply disable the replacement in a particular context (
    e.g. <bin_col> = <int_col> AND <bin_col> = <hex_string>) since
    Items don't know the context they are in and there are functions like 
    IF (<hex_string>, 'yes', 'no').
    The same problem occurs when comparing a DATE/TIME field with a
    DATE/TIME represented as an int and as a string.
  */
  if (!item ||
      (cmp_context != IMPOSSIBLE_RESULT && item->cmp_context != cmp_context))
    item= this;
  else if (field && (field->flags & ZEROFILL_FLAG) && IS_NUM(field->type()))
  {
    if (item && field->type() != FIELD_TYPE_TIMESTAMP &&
        cmp_context != INT_RESULT)
      convert_zerofill_number_to_string(&item, (Field_num *)field);
    else
      item= this;
  }
  return item;
}


/**
  Mark the item to not be part of substitution if it's not a binary item.

  See comments in Arg_comparator::set_compare_func() for details.
*/

bool Item_field::set_no_const_sub(uchar *arg)
{
  if (field->charset() != &my_charset_bin)
    no_const_subst=1;
  return FALSE;
}


/**
  Replace an Item_field for an equal Item_field that evaluated earlier
  (if any).

  @details
  If this->item_equal points to some item and coincides with arg then
  the function returns a pointer to an item that is taken from
  the very beginning of the item_equal list which the Item_field
  object refers to (belongs to) unless item_equal contains  a constant
  item. In this case the function returns this constant item, 
  (if the substitution does not require conversion).   
  If the Item_field object does not refer any Item_equal object
  'this' is returned .

  @param arg   NULL or points to so some item of the Item_equal type  


  @note
    This function is supposed to be called as a callback parameter in calls
    of the transformer method.

  @return
    - pointer to a replacement Item_field if there is a better equal item or
      a pointer to a constant equal item;
    - this - otherwise.
*/

Item *Item_field::replace_equal_field(uchar *arg)
{
  REPLACE_EQUAL_FIELD_ARG* param= (REPLACE_EQUAL_FIELD_ARG*)arg;
  if (item_equal && item_equal == param->item_equal)
  {
    Item *const_item= item_equal->get_const();
    if (const_item)
    {
      if (cmp_context != IMPOSSIBLE_RESULT &&
          const_item->cmp_context != cmp_context)
        return this;
      return const_item;
    }
    Item_field *subst= 
      (Item_field *)(item_equal->get_first(param->context_tab, this));
    if (subst)
      subst= (Item_field *) (subst->real_item());
    if (subst && !field->eq(subst->field))
      return subst;
  }
  return this;
}


void Item::init_make_field(Send_field *tmp_field,
			   enum enum_field_types field_type_arg)
{
  char *empty_name= (char*) "";
  tmp_field->db_name=		empty_name;
  tmp_field->org_table_name=	empty_name;
  tmp_field->org_col_name=	empty_name;
  tmp_field->table_name=	empty_name;
  tmp_field->col_name=		name;
  tmp_field->charsetnr=         collation.collation->number;
  tmp_field->flags=             (maybe_null ? 0 : NOT_NULL_FLAG) | 
                                (my_binary_compare(collation.collation) ?
                                 BINARY_FLAG : 0);
  tmp_field->type=              field_type_arg;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
  if (unsigned_flag)
    tmp_field->flags |= UNSIGNED_FLAG;
}

void Item::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, field_type());
}


enum_field_types Item::string_field_type() const
{
  enum_field_types f_type= MYSQL_TYPE_VAR_STRING;
  if (max_length >= 16777216)
    f_type= MYSQL_TYPE_LONG_BLOB;
  else if (max_length >= 65536)
    f_type= MYSQL_TYPE_MEDIUM_BLOB;
  return f_type;
}


void Item_empty_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, string_field_type());
}


enum_field_types Item::field_type() const
{
  switch (result_type()) {
  case STRING_RESULT:  return string_field_type();
  case INT_RESULT:     return MYSQL_TYPE_LONGLONG;
  case DECIMAL_RESULT: return MYSQL_TYPE_NEWDECIMAL;
  case REAL_RESULT:    return MYSQL_TYPE_DOUBLE;
  case ROW_RESULT:
  case TIME_RESULT:
  case IMPOSSIBLE_RESULT:
    DBUG_ASSERT(0);
    return MYSQL_TYPE_VARCHAR;
  }
  return MYSQL_TYPE_VARCHAR;
}


String *Item::check_well_formed_result(String *str, bool send_error)
{
  /* Check whether we got a well-formed string */
  CHARSET_INFO *cs= str->charset();
  int well_formed_error;
  uint wlen= cs->cset->well_formed_len(cs,
                                       str->ptr(), str->ptr() + str->length(),
                                       str->length(), &well_formed_error);
  if (wlen < str->length())
  {
    THD *thd= current_thd;
    char hexbuf[7];
    enum MYSQL_ERROR::enum_warning_level level;
    uint diff= str->length() - wlen;
    set_if_smaller(diff, 3);
    octet2hex(hexbuf, str->ptr() + wlen, diff);
    if (send_error)
    {
      my_error(ER_INVALID_CHARACTER_STRING, MYF(0),
               cs->csname,  hexbuf);
      return 0;
    }
    if ((thd->variables.sql_mode &
         (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES)))
    {
      level= MYSQL_ERROR::WARN_LEVEL_ERROR;
      null_value= 1;
      str= 0;
    }
    else
    {
      level= MYSQL_ERROR::WARN_LEVEL_WARN;
      str->length(wlen);
    }
    push_warning_printf(thd, level, ER_INVALID_CHARACTER_STRING,
                        ER(ER_INVALID_CHARACTER_STRING), cs->csname, hexbuf);
  }
  return str;
}

/*
  Compare two items using a given collation
  
  SYNOPSIS
    eq_by_collation()
    item               item to compare with
    binary_cmp         TRUE <-> compare as binaries
    cs                 collation to use when comparing strings

  DESCRIPTION
    This method works exactly as Item::eq if the collation cs coincides with
    the collation of the compared objects. Otherwise, first the collations that
    differ from cs are replaced for cs and then the items are compared by
    Item::eq. After the comparison the original collations of items are
    restored.

  RETURN
    1    compared items has been detected as equal   
    0    otherwise
*/

bool Item::eq_by_collation(Item *item, bool binary_cmp, CHARSET_INFO *cs)
{
  CHARSET_INFO *save_cs= 0;
  CHARSET_INFO *save_item_cs= 0;
  if (collation.collation != cs)
  {
    save_cs= collation.collation;
    collation.collation= cs;
  }
  if (item->collation.collation != cs)
  {
    save_item_cs= item->collation.collation;
    item->collation.collation= cs;
  }
  bool res= eq(item, binary_cmp);
  if (save_cs)
    collation.collation= save_cs;
  if (save_item_cs)
    item->collation.collation= save_item_cs;
  return res;
}  


/**
  Create a field to hold a string value from an item.

  If max_length > CONVERT_IF_BIGGER_TO_BLOB create a blob @n
  If max_length > 0 create a varchar @n
  If max_length == 0 create a CHAR(0) 

  @param table		Table for which the field is created
*/

Field *Item::make_string_field(TABLE *table)
{
  Field *field;
  DBUG_ASSERT(collation.collation);
  /* 
    Note: the following check is repeated in 
    subquery_types_allow_materialization():
  */
  if (max_length/collation.collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB)
    field= new Field_blob(max_length, maybe_null, name,
                          collation.collation);
  /* Item_type_holder holds the exact type, do not change it */
  else if (max_length > 0 &&
      (type() != Item::TYPE_HOLDER || field_type() != MYSQL_TYPE_STRING))
    field= new Field_varstring(max_length, maybe_null, name, table->s,
                               collation.collation);
  else
    field= new Field_string(max_length, maybe_null, name,
                            collation.collation);
  if (field)
    field->init(table);
  return field;
}


/**
  Create a field based on field_type of argument.

  For now, this is only used to create a field for
  IFNULL(x,something) and time functions

  @retval
    NULL  error
  @retval
    \#    Created field
*/

Field *Item::tmp_table_field_from_field_type(TABLE *table, bool fixed_length)
{
  /*
    The field functions defines a field to be not null if null_ptr is not 0
  */
  uchar *null_ptr= maybe_null ? (uchar*) "" : 0;
  Field *field;

  switch (field_type()) {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    field= Field_new_decimal::create_from_item(this);
    break;
  case MYSQL_TYPE_TINY:
    field= new Field_tiny((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			  name, 0, unsigned_flag);
    break;
  case MYSQL_TYPE_SHORT:
    field= new Field_short((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			   name, 0, unsigned_flag);
    break;
  case MYSQL_TYPE_LONG:
    field= new Field_long((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			  name, 0, unsigned_flag);
    break;
#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    field= new Field_longlong((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			      name, 0, unsigned_flag);
    break;
#endif
  case MYSQL_TYPE_FLOAT:
    field= new Field_float((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			   name, decimals, 0, unsigned_flag);
    break;
  case MYSQL_TYPE_DOUBLE:
    field= new Field_double((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			    name, decimals, 0, unsigned_flag);
    break;
  case MYSQL_TYPE_NULL:
    field= new Field_null((uchar*) 0, max_length, Field::NONE,
			  name, &my_charset_bin);
    break;
  case MYSQL_TYPE_INT24:
    field= new Field_medium((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			    name, 0, unsigned_flag);
    break;
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_DATE:
    field= new Field_newdate(0, null_ptr, 0, Field::NONE, name, &my_charset_bin);
    break;
  case MYSQL_TYPE_TIME:
    field= new_Field_time(0, null_ptr, 0, Field::NONE, name,
                              decimals, &my_charset_bin);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    field= new_Field_timestamp(0, null_ptr, 0,
                               Field::NONE, name, 0, decimals, &my_charset_bin);
    break;
  case MYSQL_TYPE_DATETIME:
    field= new_Field_datetime(0, null_ptr, 0, Field::NONE, name,
                              decimals, &my_charset_bin);
    break;
  case MYSQL_TYPE_YEAR:
    field= new Field_year((uchar*) 0, max_length, null_ptr, 0, Field::NONE,
			  name);
    break;
  case MYSQL_TYPE_BIT:
    field= new Field_bit_as_char(NULL, max_length, null_ptr, 0,
                                 Field::NONE, name);
    break;
  default:
    /* This case should never be chosen */
    DBUG_ASSERT(0);
    /* If something goes awfully wrong, it's better to get a string than die */
  case MYSQL_TYPE_STRING:
    if (fixed_length && max_length < CONVERT_IF_BIGGER_TO_BLOB)
    {
      field= new Field_string(max_length, maybe_null, name,
                              collation.collation);
      break;
    }
    /* Fall through to make_string_field() */
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    return make_string_field(table);
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
    if (this->type() == Item::TYPE_HOLDER)
      field= new Field_blob(max_length, maybe_null, name, collation.collation,
                            1);
    else
      field= new Field_blob(max_length, maybe_null, name, collation.collation);
    break;					// Blob handled outside of case
#ifdef HAVE_SPATIAL
  case MYSQL_TYPE_GEOMETRY:
    field= new Field_geom(max_length, maybe_null,
                          name, table->s, get_geometry_type());
#endif /* HAVE_SPATIAL */
  }
  if (field)
    field->init(table);
  return field;
}


/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field)
{
  field->make_field(tmp_field);
  DBUG_ASSERT(tmp_field->table_name != 0);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
  if (table_name)
    tmp_field->table_name= table_name;
  if (db_name)
    tmp_field->db_name= db_name;
}


/**
  Save a field value in another field

  @param from             Field to take the value from
  @param [out] null_value Pointer to the null_value flag to set
  @param to               Field to save the value in
  @param no_conversions   How to deal with NULL value

  @details
  The function takes the value of the field 'from' and, if this value
  is not null, it saves in the field 'to' setting off the flag referenced
  by 'null_value'. Otherwise this flag is set on and field 'to' is
  also set to null possibly with conversion.

  @note
  This function is used by the functions Item_field::save_in_field,
  Item_field::save_org_in_field and Item_ref::save_in_field

  @retval FALSE OK
  @retval TRUE  Error

*/

static int save_field_in_field(Field *from, bool *null_value,
                               Field *to, bool no_conversions)
{
  int res;
  DBUG_ENTER("save_field_in_field");
  if (from->is_null())
  {
    (*null_value)= 1;
    DBUG_RETURN(set_field_to_null_with_conversions(to, no_conversions));
  }
  to->set_notnull();

  /*
    If we're setting the same field as the one we're reading from there's 
    nothing to do. This can happen in 'SET x = x' type of scenarios.
  */
  if (to == from)
  {
    (*null_value)= 0;
    DBUG_RETURN(0);
  }

  res= field_conv(to, from);
  (*null_value)= 0;
  DBUG_RETURN(res);
}


/**
  Set a field's value from a item.
*/

void Item_field::save_org_in_field(Field *to)
{
  save_field_in_field(field, &null_value, to, TRUE);
}


int Item_field::save_in_field(Field *to, bool no_conversions)
{
  return save_field_in_field(result_field, &null_value, to, no_conversions);
}


/**
  Store null in field.

  This is used on INSERT.
  Allow NULL to be inserted in timestamp and auto_increment values.

  @param field		Field where we want to store NULL

  @retval
    0   ok
  @retval
    1   Field doesn't support NULL values and can't handle 'field = NULL'
*/

int Item_null::save_in_field(Field *field, bool no_conversions)
{
  return set_field_to_null_with_conversions(field, no_conversions);
}


/**
  Store null in field.

  @param field		Field where we want to store NULL

  @retval
    0	 OK
  @retval
    1	 Field doesn't support NULL values
*/

int Item_null::save_safe_in_field(Field *field)
{
  return set_field_to_null(field);
}


/*
  This implementation can lose str_value content, so if the
  Item uses str_value to store something, it should
  reimplement it's ::save_in_field() as Item_string, for example, does.

  Note: all Item_XXX::val_str(str) methods must NOT rely on the fact that
  str != str_value. For example, see fix for bug #44743.
*/

int Item::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if (result_type() == STRING_RESULT ||
      (result_type() == REAL_RESULT &&
       field->result_type() == STRING_RESULT))
  {
    String *result;
    CHARSET_INFO *cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result=val_str(&str_value);
    if (null_value)
    {
      str_value.set_quick(0, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }

    /* NOTE: If null_value == FALSE, "result" must be not NULL.  */

    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr= val_real();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else if (result_type() == DECIMAL_RESULT)
  {
    my_decimal decimal_value;
    my_decimal *value= val_decimal(&decimal_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store_decimal(value);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr, unsigned_flag);
  }
  return error ? error : (field->table->in_use->is_error() ? 1 : 0);
}


int Item_string::save_in_field(Field *field, bool no_conversions)
{
  String *result;
  result=val_str(&str_value);
  return save_str_value_in_field(field, result);
}


static int save_int_value_in_field (Field *field, longlong nr, 
                                    bool null_value, bool unsigned_flag)
{
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr, unsigned_flag);
}


int Item_int::save_in_field(Field *field, bool no_conversions)
{
  return save_int_value_in_field (field, val_int(), null_value, unsigned_flag);
}


void Item_datetime::set(longlong packed)
{
  unpack_time(packed, &ltime);
}

int Item_datetime::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();
  return field->store_time_dec(&ltime, decimals);
}

longlong Item_datetime::val_int()
{
  return TIME_to_ulonglong(&ltime);
}

int Item_decimal::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();
  return field->store_decimal(&decimal_value);
}


bool Item_int::eq(const Item *arg, bool binary_cmp) const
{
  /* No need to check for null value as basic constant can't be NULL */
  if (arg->basic_const_item() && arg->type() == type())
  {
    /*
      We need to cast off const to call val_int(). This should be OK for
      a basic constant.
    */
    Item *item= (Item*) arg;
    return (item->val_int() == value &&
            ((longlong) value >= 0 ||
             (item->unsigned_flag == unsigned_flag)));
  }
  return FALSE;
}


Item *Item_int_with_ref::clone_item()
{
  DBUG_ASSERT(ref->const_item());
  /*
    We need to evaluate the constant to make sure it works with
    parameter markers.
  */
  return (ref->unsigned_flag ?
          new Item_uint(ref->name, ref->val_int(), ref->max_length) :
          new Item_int(ref->name, ref->val_int(), ref->max_length));
}


Item_num *Item_uint::neg()
{
  Item_decimal *item= new Item_decimal(value, 1);
  return item->neg();
}


static uint nr_of_decimals(const char *str, const char *end)
{
  const char *decimal_point;

  /* Find position for '.' */
  for (;;)
  {
    if (str == end)
      return 0;
    if (*str == 'e' || *str == 'E')
      return NOT_FIXED_DEC;    
    if (*str++ == '.')
      break;
  }
  decimal_point= str;
  for (; my_isdigit(system_charset_info, *str) ; str++)
    ;
  if (*str == 'e' || *str == 'E')
    return NOT_FIXED_DEC;
  return (uint) (str - decimal_point);
}


/**
  This function is only called during parsing:
  - when parsing SQL query from sql_yacc.yy
  - when parsing XPath query from item_xmlfunc.cc
  We will signal an error if value is not a true double value (overflow):
  eng: Illegal %s '%-.192s' value found during parsing
  
  Note: the string is NOT null terminated when called from item_xmlfunc.cc,
  so this->name will contain some SQL query tail behind the "length" bytes.
  This is Ok for now, as this Item is never seen in SHOW,
  or EXPLAIN, or anywhere else in metadata.
  Item->name should be fixed to use LEX_STRING eventually.
*/

Item_float::Item_float(const char *str_arg, uint length)
{
  int error;
  char *end_not_used;
  value= my_strntod(&my_charset_bin, (char*) str_arg, length, &end_not_used,
                    &error);
  if (error)
  {
    char tmp[NAME_LEN + 1];
    my_snprintf(tmp, sizeof(tmp), "%.*s", length, str_arg);
    my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "double", tmp);
  }
  presentation= name=(char*) str_arg;
  decimals=(uint8) nr_of_decimals(str_arg, str_arg+length);
  max_length=length;
  fixed= 1;
}


int Item_float::save_in_field(Field *field, bool no_conversions)
{
  double nr= val_real();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr);
}


void Item_float::print(String *str, enum_query_type query_type)
{
  if (presentation)
  {
    str->append(presentation);
    return;
  }
  char buffer[20];
  String num(buffer, sizeof(buffer), &my_charset_bin);
  num.set_real(value, decimals, &my_charset_bin);
  str->append(num);
}


/*
  hex item
  In string context this is a binary string.
  In number context this is a longlong value.
*/

bool Item_float::eq(const Item *arg, bool binary_cmp) const
{
  if (arg->basic_const_item() && arg->type() == type())
  {
    /*
      We need to cast off const to call val_int(). This should be OK for
      a basic constant.
    */
    Item *item= (Item*) arg;
    return item->val_real() == value;
  }
  return FALSE;
}


inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}

Item_hex_string::Item_hex_string()
{
  hex_string_init("", 0);
}

Item_hex_string::Item_hex_string(const char *str, uint str_length)
{
  hex_string_init(str, str_length);
}

void Item_hex_string::hex_string_init(const char *str, uint str_length)
{
  max_length=(str_length+1)/2;
  char *ptr=(char*) sql_alloc(max_length+1);
  if (!ptr)
  {
    str_value.set("", 0, &my_charset_bin);
    return;
  }
  str_value.set(ptr,max_length,&my_charset_bin);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
  unsigned_flag= 1;
}

longlong Item_hex_string::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  char *end=(char*) str_value.ptr()+str_value.length(),
       *ptr=end-min(str_value.length(),sizeof(longlong));

  ulonglong value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (ulonglong) (uchar) *ptr;
  return (longlong) value;
}


my_decimal *Item_hex_string::val_decimal(my_decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  ulonglong value= (ulonglong)val_int();
  int2my_decimal(E_DEC_FATAL_ERROR, value, TRUE, decimal_value);
  return (decimal_value);
}


int Item_hex_string::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
    return field->store(str_value.ptr(), str_value.length(), 
                        collation.collation);

  ulonglong nr;
  uint32 length= str_value.length();
  if (!length)
    return 1;

  if (length > 8)
  {
    nr= field->flags & UNSIGNED_FLAG ? ULONGLONG_MAX : LONGLONG_MAX;
    goto warn;
  }
  nr= (ulonglong) val_int();
  if ((length == 8) && !(field->flags & UNSIGNED_FLAG) && (nr > LONGLONG_MAX))
  {
    nr= LONGLONG_MAX;
    goto warn;
  }
  return field->store((longlong) nr, TRUE);  // Assume hex numbers are unsigned

warn:
  if (!field->store((longlong) nr, TRUE))
    field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE,
                       1);
  return 1;
}


void Item_hex_string::print(String *str, enum_query_type query_type)
{
  char *end= (char*) str_value.ptr() + str_value.length(),
       *ptr= end - min(str_value.length(), sizeof(longlong));
  str->append("0x");
  for (; ptr != end ; ptr++)
  {
    str->append(_dig_vec_lower[((uchar) *ptr) >> 4]);
    str->append(_dig_vec_lower[((uchar) *ptr) & 0x0F]);
  }
}


bool Item_hex_string::eq(const Item *arg, bool binary_cmp) const
{
  if (arg->basic_const_item() && arg->type() == type())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &arg->str_value);
    return !sortcmp(&str_value, &arg->str_value, collation.collation);
  }
  return FALSE;
}


Item *Item_hex_string::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  String tmp, *str= val_str(&tmp);

  if (!(conv= new Item_string(str->ptr(), str->length(), tocs)))
    return NULL;
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}


/*
  bin item.
  In string context this is a binary string.
  In number context this is a longlong value.
*/
  
Item_bin_string::Item_bin_string(const char *str, uint str_length)
{
  const char *end= str + str_length - 1;
  uchar bits= 0;
  uint power= 1;

  max_length= (str_length + 7) >> 3;
  char *ptr= (char*) sql_alloc(max_length + 1);
  if (!ptr)
    return;
  str_value.set(ptr, max_length, &my_charset_bin);

  if (max_length > 0)
  {
    ptr+= max_length - 1;
    ptr[1]= 0;                     // Set end null for string
    for (; end >= str; end--)
    {
      if (power == 256)
      {
        power= 1;
        *ptr--= bits;
        bits= 0;
      }
      if (*end == '1')
        bits|= power;
      power<<= 1;
    }
    *ptr= (char) bits;
  }
  else
    ptr[0]= 0;

  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
}


/**
  Pack data in buffer for sending.
*/

bool Item_null::send(Protocol *protocol, String *packet)
{
  return protocol->store_null();
}

/**
  This is only called from items that is not of type item_field.
*/

bool Item::send(Protocol *protocol, String *buffer)
{
  bool UNINIT_VAR(result);                       // Will be set if null_value == 0
  enum_field_types f_type;

  switch ((f_type=field_type())) {
  default:
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_NEWDECIMAL:
  {
    String *res;
    if ((res=val_str(buffer)))
    {
      DBUG_ASSERT(!null_value);
      result= protocol->store(res->ptr(),res->length(),res->charset());
    }
    else
    {
      DBUG_ASSERT(null_value);
    }
    break;
  }
  case MYSQL_TYPE_TINY:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_tiny(nr);
    break;
  }
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_YEAR:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_short(nr);
    break;
  }
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_long(nr);
    break;
  }
  case MYSQL_TYPE_LONGLONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_longlong(nr, unsigned_flag);
    break;
  }
  case MYSQL_TYPE_FLOAT:
  {
    float nr;
    nr= (float) val_real();
    if (!null_value)
      result= protocol->store(nr, decimals, buffer);
    break;
  }
  case MYSQL_TYPE_DOUBLE:
  {
    double nr= val_real();
    if (!null_value)
      result= protocol->store(nr, decimals, buffer);
    break;
  }
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIMESTAMP:
  {
    MYSQL_TIME tm;
    get_date(&tm, TIME_FUZZY_DATE | sql_mode_for_dates());
    if (!null_value)
    {
      if (f_type == MYSQL_TYPE_DATE)
	return protocol->store_date(&tm);
      else
	result= protocol->store(&tm, decimals);
    }
    break;
  }
  case MYSQL_TYPE_TIME:
  {
    MYSQL_TIME tm;
    get_time(&tm);
    if (!null_value)
      result= protocol->store_time(&tm, decimals);
    break;
  }
  }
  if (null_value)
    result= protocol->store_null();
  return result;
}


bool Item_field::send(Protocol *protocol, String *buffer)
{
  return protocol->store(result_field);
}


void Item_field::update_null_value() 
{ 
  /* 
    need to set no_errors to prevent warnings about type conversion 
    popping up.
  */
  THD *thd= field->table->in_use;
  int no_errors;

  no_errors= thd->no_errors;
  thd->no_errors= 1;
  Item::update_null_value();
  thd->no_errors= no_errors;
}


/*
  Add the field to the select list and substitute it for the reference to
  the field.

  SYNOPSIS
    Item_field::update_value_transformer()
    select_arg      current select

  DESCRIPTION
    If the field doesn't belong to the table being inserted into then it is
    added to the select list, pointer to it is stored in the ref_pointer_array
    of the select and the field itself is substituted for the Item_ref object.
    This is done in order to get correct values from update fields that
    belongs to the SELECT part in the INSERT .. SELECT .. ON DUPLICATE KEY
    UPDATE statement.

  RETURN
    0             if error occured
    ref           if all conditions are met
    this field    otherwise
*/

Item *Item_field::update_value_transformer(uchar *select_arg)
{
  SELECT_LEX *select= (SELECT_LEX*)select_arg;
  DBUG_ASSERT(fixed);

  if (field->table != select->context.table_list->table &&
      type() != Item::TRIGGER_FIELD_ITEM)
  {
    List<Item> *all_fields= &select->join->all_fields;
    Item **ref_pointer_array= select->ref_pointer_array;
    int el= all_fields->elements;
    Item_ref *ref;

    ref_pointer_array[el]= (Item*)this;
    all_fields->push_front((Item*)this);
    ref= new Item_ref(&select->context, ref_pointer_array + el,
                      table_name, field_name);
    return ref;
  }
  return this;
}


void Item_field::print(String *str, enum_query_type query_type)
{
  if (field && field->table->const_table)
  {
    print_value(str);
    return;
  }
  Item_ident::print(str, query_type);
}


Item_ref::Item_ref(Name_resolution_context *context_arg,
                   Item **item, const char *table_name_arg,
                   const char *field_name_arg,
                   bool alias_name_used_arg)
  :Item_ident(context_arg, NullS, table_name_arg, field_name_arg),
   result_field(0), ref(item), reference_trough_name(0)
{
  alias_name_used= alias_name_used_arg;
  /*
    This constructor used to create some internals references over fixed items
  */
  if (ref && *ref && (*ref)->fixed)
    set_properties();
}

/*
  A Field_enumerator-compatible class that invokes mark_as_dependent() for
  each field that is a reference to some ancestor of current_select.
*/
class Dependency_marker: public Field_enumerator
{
public:
  THD *thd;
  st_select_lex *current_select;
  virtual void visit_field(Item_field *item)
  {
    // Find which select the field is in. This is achieved by walking up 
    // the select tree and looking for the table of interest.
    st_select_lex *sel;
    for (sel= current_select; sel; sel= sel->outer_select())
    {
      List_iterator<TABLE_LIST> li(sel->leaf_tables);
      TABLE_LIST *tbl;
      while ((tbl= li++))
      {
        if (tbl->table == item->field->table)
        {
          if (sel != current_select)
            mark_as_dependent(thd, sel, current_select, item, item);
          return;
        }
      }
    }
  }
};

Item_ref::Item_ref(TABLE_LIST *view_arg, Item **item,
                   const char *field_name_arg, bool alias_name_used_arg)
  :Item_ident(view_arg, field_name_arg),
   result_field(NULL), ref(item), reference_trough_name(0)
{
  alias_name_used= alias_name_used_arg;
  /*
    This constructor is used to create some internal references over fixed items
  */
  if (ref && *ref && (*ref)->fixed)
    set_properties();
}


/**
  Resolve the name of a reference to a column reference.

  The method resolves the column reference represented by 'this' as a column
  present in one of: GROUP BY clause, SELECT clause, outer queries. It is
  used typically for columns in the HAVING clause which are not under
  aggregate functions.

  POSTCONDITION @n
  Item_ref::ref is 0 or points to a valid item.

  @note
    The name resolution algorithm used is (where [T_j] is an optional table
    name that qualifies the column name):

  @code
        resolve_extended([T_j].col_ref_i)
        {
          Search for a column or derived column named col_ref_i [in table T_j]
          in the SELECT and GROUP clauses of Q.

          if such a column is NOT found AND    // Lookup in outer queries.
             there are outer queries
          {
            for each outer query Q_k beginning from the inner-most one
           {
              Search for a column or derived column named col_ref_i
              [in table T_j] in the SELECT and GROUP clauses of Q_k.

              if such a column is not found AND
                 - Q_k is not a group query AND
                 - Q_k is not inside an aggregate function
                 OR
                 - Q_(k-1) is not in a HAVING or SELECT clause of Q_k
              {
                search for a column or derived column named col_ref_i
                [in table T_j] in the FROM clause of Q_k;
              }
            }
          }
        }
  @endcode
  @n
    This procedure treats GROUP BY and SELECT clauses as one namespace for
    column references in HAVING. Notice that compared to
    Item_field::fix_fields, here we first search the SELECT and GROUP BY
    clauses, and then we search the FROM clause.

  @param[in]     thd        current thread
  @param[in,out] reference  view column if this item was resolved to a
    view column

  @todo
    Here we could first find the field anyway, and then test this
    condition, so that we can give a better error message -
    ER_WRONG_FIELD_WITH_GROUP, instead of the less informative
    ER_BAD_FIELD_ERROR which we produce now.

  @retval
    TRUE  if error
  @retval
    FALSE on success
*/

bool Item_ref::fix_fields(THD *thd, Item **reference)
{
  enum_parsing_place place= NO_MATTER;
  DBUG_ASSERT(fixed == 0);
  SELECT_LEX *current_sel= thd->lex->current_select;

  if (!ref || ref == not_found_item)
  {
    DBUG_ASSERT(reference_trough_name != 0);
    if (!(ref= resolve_ref_in_select_and_group(thd, this,
                                               context->select_lex)))
      goto error;             /* Some error occurred (e.g. ambiguous names). */

    if (ref == not_found_item) /* This reference was not resolved. */
    {
      Name_resolution_context *last_checked_context= context;
      Name_resolution_context *outer_context= context->outer_context;
      Field *from_field;
      ref= 0;

      if (!outer_context)
      {
        /* The current reference cannot be resolved in this query. */
        my_error(ER_BAD_FIELD_ERROR,MYF(0),
                 this->full_name(), current_thd->where);
        goto error;
      }

      /*
        If there is an outer context (select), and it is not a derived table
        (which do not support the use of outer fields for now), try to
        resolve this reference in the outer select(s).

        We treat each subselect as a separate namespace, so that different
        subselects may contain columns with the same names. The subselects are
        searched starting from the innermost.
      */
      from_field= (Field*) not_found_field;

      do
      {
        SELECT_LEX *select= outer_context->select_lex;
        Item_subselect *prev_subselect_item=
          last_checked_context->select_lex->master_unit()->item;
        last_checked_context= outer_context;

        /* Search in the SELECT and GROUP lists of the outer select. */
        if (outer_context->resolve_in_select_list)
        {
          if (!(ref= resolve_ref_in_select_and_group(thd, this, select)))
            goto error; /* Some error occurred (e.g. ambiguous names). */
          if (ref != not_found_item)
          {
            DBUG_ASSERT(*ref && (*ref)->fixed);
            prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
            prev_subselect_item->const_item_cache&= (*ref)->const_item();
            break;
          }
          /*
            Set ref to 0 to ensure that we get an error in case we replaced
            this item with another item and still use this item in some
            other place of the parse tree.
          */
          ref= 0;
        }

        place= prev_subselect_item->parsing_place;
        /*
          Check table fields only if the subquery is used somewhere out of
          HAVING or the outer SELECT does not use grouping (i.e. tables are
          accessible).
          TODO:
          Here we could first find the field anyway, and then test this
          condition, so that we can give a better error message -
          ER_WRONG_FIELD_WITH_GROUP, instead of the less informative
          ER_BAD_FIELD_ERROR which we produce now.
        */
        if ((place != IN_HAVING ||
             (!select->with_sum_func &&
              select->group_list.elements == 0)))
        {
          /*
            In case of view, find_field_in_tables() write pointer to view
            field expression to 'reference', i.e. it substitute that
            expression instead of this Item_ref
          */
          from_field= find_field_in_tables(thd, this,
                                           outer_context->
                                             first_name_resolution_table,
                                           outer_context->
                                             last_name_resolution_table,
                                           reference,
                                           IGNORE_EXCEPT_NON_UNIQUE,
                                           TRUE, TRUE);
          if (! from_field)
            goto error;
          if (from_field == view_ref_found)
          {
            Item::Type refer_type= (*reference)->type();
            prev_subselect_item->used_tables_cache|=
              (*reference)->used_tables();
            prev_subselect_item->const_item_cache&=
              (*reference)->const_item();
            DBUG_ASSERT((*reference)->type() == REF_ITEM);
            mark_as_dependent(thd, last_checked_context->select_lex,
                              context->select_lex, this,
                              ((refer_type == REF_ITEM ||
                                refer_type == FIELD_ITEM) ?
                               (Item_ident*) (*reference) :
                               0));
            /*
              view reference found, we substituted it instead of this
              Item, so can quit
            */
            return FALSE;
          }
          if (from_field != not_found_field)
          {
            if (cached_table && cached_table->select_lex &&
                outer_context->select_lex &&
                cached_table->select_lex != outer_context->select_lex)
            {
              /*
                Due to cache, find_field_in_tables() can return field which
                doesn't belong to provided outer_context. In this case we have
                to find proper field context in order to fix field correcly.
              */
              do
              {
                outer_context= outer_context->outer_context;
                select= outer_context->select_lex;
                prev_subselect_item=
                  last_checked_context->select_lex->master_unit()->item;
                last_checked_context= outer_context;
              } while (outer_context && outer_context->select_lex &&
                       cached_table->select_lex != outer_context->select_lex);
            }
            prev_subselect_item->used_tables_cache|= from_field->table->map;
            prev_subselect_item->const_item_cache= 0;
            break;
          }
        }
        DBUG_ASSERT(from_field == not_found_field);

        /* Reference is not found => depend on outer (or just error). */
        prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
        prev_subselect_item->const_item_cache= 0;

        outer_context= outer_context->outer_context;
      } while (outer_context);

      DBUG_ASSERT(from_field != 0 && from_field != view_ref_found);
      if (from_field != not_found_field)
      {
        Item_field* fld;
        if (!(fld= new Item_field(thd, last_checked_context, from_field)))
          goto error;
        thd->change_item_tree(reference, fld);
        mark_as_dependent(thd, last_checked_context->select_lex,
                          thd->lex->current_select, fld, fld);
        /*
          A reference is resolved to a nest level that's outer or the same as
          the nest level of the enclosing set function : adjust the value of
          max_arg_level for the function if it's needed.
        */
        if (thd->lex->in_sum_func &&
            thd->lex->in_sum_func->nest_level >= 
            last_checked_context->select_lex->nest_level)
          set_if_bigger(thd->lex->in_sum_func->max_arg_level,
                        last_checked_context->select_lex->nest_level);
        return FALSE;
      }
      if (ref == 0)
      {
        /* The item was not a table field and not a reference */
        my_error(ER_BAD_FIELD_ERROR, MYF(0),
                 this->full_name(), current_thd->where);
        goto error;
      }
      /* Should be checked in resolve_ref_in_select_and_group(). */
      DBUG_ASSERT(*ref && (*ref)->fixed);
      mark_as_dependent(thd, last_checked_context->select_lex,
                        context->select_lex, this, this);
      /*
        A reference is resolved to a nest level that's outer or the same as
        the nest level of the enclosing set function : adjust the value of
        max_arg_level for the function if it's needed.
      */
      if (thd->lex->in_sum_func &&
          thd->lex->in_sum_func->nest_level >= 
          last_checked_context->select_lex->nest_level)
        set_if_bigger(thd->lex->in_sum_func->max_arg_level,
                      last_checked_context->select_lex->nest_level);
    }
  }
  else if (ref_type() != VIEW_REF)
  {
    /*
      It could be that we're referring to something that's in ancestor selects.
      We must make an appropriate mark_as_dependent() call for each such
      outside reference.
    */
    Dependency_marker dep_marker;
    dep_marker.current_select= current_sel;
    dep_marker.thd= thd;
    (*ref)->walk(&Item::enumerate_field_refs_processor, FALSE,
                 (uchar*)&dep_marker);
  }

  DBUG_ASSERT(*ref);
  /*
    Check if this is an incorrect reference in a group function or forward
    reference. Do not issue an error if this is:
      1. outer reference (will be fixed later by the fix_inner_refs function);
      2. an unnamed reference inside an aggregate function.
  */
  if (!((*ref)->type() == REF_ITEM &&
       ((Item_ref *)(*ref))->ref_type() == OUTER_REF) &&
      (((*ref)->with_sum_func && name &&
        !(current_sel->linkage != GLOBAL_OPTIONS_TYPE &&
          current_sel->having_fix_field)) ||
       !(*ref)->fixed))
  {
    my_error(ER_ILLEGAL_REFERENCE, MYF(0),
             name, ((*ref)->with_sum_func?
                    "reference to group function":
                    "forward reference in item list"));
    goto error;
  }

  set_properties();

  if ((*ref)->check_cols(1))
    goto error;
  return FALSE;

error:
  context->process_error(thd);
  return TRUE;
}


void Item_ref::set_properties()
{
  max_length= (*ref)->max_length;
  maybe_null= (*ref)->maybe_null;
  decimals=   (*ref)->decimals;
  collation.set((*ref)->collation);
  /*
    We have to remember if we refer to a sum function, to ensure that
    split_sum_func() doesn't try to change the reference.
  */
  with_sum_func= (*ref)->with_sum_func;
  with_field= (*ref)->with_field;
  unsigned_flag= (*ref)->unsigned_flag;
  fixed= 1;
  if (alias_name_used)
    return;
  if ((*ref)->type() == FIELD_ITEM)
    alias_name_used= ((Item_ident *) (*ref))->alias_name_used;
  else
    alias_name_used= TRUE; // it is not field, so it is was resolved by alias
}


void Item_ref::cleanup()
{
  DBUG_ENTER("Item_ref::cleanup");
  Item_ident::cleanup();
  result_field= 0;
  if (reference_trough_name)
  {
    /* We have to reset the reference as it may been freed */
    ref= 0;
  }
  DBUG_VOID_RETURN;
}


/**
  Transform an Item_ref object with a transformer callback function.

  The function first applies the transform method to the item
  referenced by this Item_reg object. If this returns a new item the
  old item is substituted for a new one. After this the transformer
  is applied to the Item_ref object.

  @param transformer   the transformer callback function to be applied to
                       the nodes of the tree of the object
  @param argument      parameter to be passed to the transformer

  @return Item returned as the result of transformation of the Item_ref object
    @retval !NULL The transformation was successful
    @retval NULL  Out of memory error
*/

Item* Item_ref::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->is_stmt_prepare());
  DBUG_ASSERT((*ref) != NULL);

  /* Transform the object we are referencing. */
  Item *new_item= (*ref)->transform(transformer, arg);
  if (!new_item)
    return NULL;

  /*
    THD::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (*ref != new_item)
    current_thd->change_item_tree(ref, new_item);

  /* Transform the item ref object. */
  return (this->*transformer)(arg);
}


/**
  Compile an Item_ref object with a processor and a transformer
  callback functions.

  First the function applies the analyzer to the Item_ref object. Then
  if the analizer succeeeds we first applies the compile method to the
  object the Item_ref object is referencing. If this returns a new
  item the old item is substituted for a new one.  After this the
  transformer is applied to the Item_ref object itself.
  The compile function is not called if the analyzer returns NULL
  in the parameter arg_p. 

  @param analyzer      the analyzer callback function to be applied to the
                       nodes of the tree of the object
  @param[in,out] arg_p parameter to be passed to the processor
  @param transformer   the transformer callback function to be applied to the
                       nodes of the tree of the object
  @param arg_t         parameter to be passed to the transformer

  @return Item returned as the result of transformation of the Item_ref object
*/

Item* Item_ref::compile(Item_analyzer analyzer, uchar **arg_p,
                        Item_transformer transformer, uchar *arg_t)
{
  /* Analyze this Item object. */
  if (!(this->*analyzer)(arg_p))
    return NULL;

  /* Compile the Item we are referencing. */
  DBUG_ASSERT((*ref) != NULL);
  if (*arg_p)
  {
    uchar *arg_v= *arg_p;
    Item *new_item= (*ref)->compile(analyzer, &arg_v, transformer, arg_t);
    if (new_item && *ref != new_item)
      current_thd->change_item_tree(ref, new_item);
  }

  /* Transform this Item object. */
  return (this->*transformer)(arg_t);
}


void Item_ref::print(String *str, enum_query_type query_type)
{
  if (ref)
  {
    if ((*ref)->type() != Item::CACHE_ITEM && ref_type() != VIEW_REF &&
        !table_name && name && alias_name_used)
    {
      THD *thd= current_thd;
      append_identifier(thd, str, (*ref)->real_item()->name,
                        strlen((*ref)->real_item()->name));
    }
    else
      (*ref)->print(str, query_type);
  }
  else
    Item_ident::print(str, query_type);
}


bool Item_ref::send(Protocol *prot, String *tmp)
{
  if (result_field)
    return prot->store(result_field);
  return (*ref)->send(prot, tmp);
}


double Item_ref::val_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0.0;
    return result_field->val_real();
  }
  return val_real();
}


bool Item_ref::is_null_result()
{
  if (result_field)
    return (null_value=result_field->is_null());

  return is_null();
}


longlong Item_ref::val_int_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_int();
  }
  return val_int();
}


String *Item_ref::str_result(String* str)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    str->set_charset(str_value.charset());
    return result_field->val_str(str, &str_value);
  }
  return val_str(str);
}


my_decimal *Item_ref::val_decimal_result(my_decimal *decimal_value)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_decimal(decimal_value);
  }
  return val_decimal(decimal_value);
}


bool Item_ref::val_bool_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    switch (result_field->result_type()) {
    case INT_RESULT:
      return result_field->val_int() != 0;
    case DECIMAL_RESULT:
    {
      my_decimal decimal_value;
      my_decimal *val= result_field->val_decimal(&decimal_value);
      if (val)
        return !my_decimal_is_zero(val);
      return 0;
    }
    case REAL_RESULT:
    case STRING_RESULT:
      return result_field->val_real() != 0.0;
    case ROW_RESULT:
    case TIME_RESULT:
    case IMPOSSIBLE_RESULT:
      DBUG_ASSERT(0);
    }
  }
  return val_bool();
}


void Item_ref::save_result(Field *to)
{
  if (result_field)
  {
    save_field_in_field(result_field, &null_value, to, TRUE);
    return;
  }
  (*ref)->save_result(to);
  null_value= (*ref)->null_value;
}


void Item_ref::save_val(Field *to)
{
  (*ref)->save_result(to);
  null_value= (*ref)->null_value;
}


double Item_ref::val_real()
{
  DBUG_ASSERT(fixed);
  double tmp=(*ref)->val_result();
  null_value=(*ref)->null_value;
  return tmp;
}


longlong Item_ref::val_int()
{
  DBUG_ASSERT(fixed);
  longlong tmp=(*ref)->val_int_result();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::val_bool()
{
  DBUG_ASSERT(fixed);
  bool tmp= (*ref)->val_bool_result();
  null_value= (*ref)->null_value;
  return tmp;
}


String *Item_ref::val_str(String* tmp)
{
  DBUG_ASSERT(fixed);
  tmp=(*ref)->str_result(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::is_null()
{
  DBUG_ASSERT(fixed);
  bool tmp=(*ref)->is_null_result();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  return (null_value=(*ref)->get_date_result(ltime,fuzzydate));
}


my_decimal *Item_ref::val_decimal(my_decimal *decimal_value)
{
  my_decimal *val= (*ref)->val_decimal_result(decimal_value);
  null_value= (*ref)->null_value;
  return val;
}

int Item_ref::save_in_field(Field *to, bool no_conversions)
{
  int res;
  if (result_field)
  {
    if (result_field->is_null())
    {
      null_value= 1;
      res= set_field_to_null_with_conversions(to, no_conversions);
      return res;
    }
    to->set_notnull();
    res= field_conv(to, result_field);
    null_value= 0;
    return res;
  }
  res= (*ref)->save_in_field(to, no_conversions);
  null_value= (*ref)->null_value;
  return res;
}


void Item_ref::save_org_in_field(Field *field)
{
  (*ref)->save_org_in_field(field);
}


void Item_ref::make_field(Send_field *field)
{
  (*ref)->make_field(field);
  /* Non-zero in case of a view */
  if (name)
    field->col_name= name;
  if (table_name)
    field->table_name= table_name;
  if (db_name)
    field->db_name= db_name;
  if (orig_field_name)
    field->org_col_name= orig_field_name;
  if (orig_table_name)
    field->org_table_name= orig_table_name;
}


Item *Item_ref::get_tmp_table_item(THD *thd)
{
  if (!result_field)
    return (*ref)->get_tmp_table_item(thd);

  Item_field *item= new Item_field(result_field);
  if (item)
  {
    item->table_name= table_name;
    item->db_name= db_name;
  }
  return item;
}


void Item_ref_null_helper::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("<ref_null_helper>("));
  if (ref)
    (*ref)->print(str, query_type);
  else
    str->append('?');
  str->append(')');
}


void Item_direct_ref::save_val(Field *to)
{
  (*ref)->save_val(to);
  null_value=(*ref)->null_value;
}


double Item_direct_ref::val_real()
{
  double tmp=(*ref)->val_real();
  null_value=(*ref)->null_value;
  return tmp;
}


longlong Item_direct_ref::val_int()
{
  longlong tmp=(*ref)->val_int();
  null_value=(*ref)->null_value;
  return tmp;
}


String *Item_direct_ref::val_str(String* tmp)
{
  tmp=(*ref)->val_str(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}


my_decimal *Item_direct_ref::val_decimal(my_decimal *decimal_value)
{
  my_decimal *tmp= (*ref)->val_decimal(decimal_value);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::val_bool()
{
  bool tmp= (*ref)->val_bool();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::is_null()
{
  return (*ref)->is_null();
}


bool Item_direct_ref::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  return (null_value=(*ref)->get_date(ltime,fuzzydate));
}


Item_cache_wrapper::~Item_cache_wrapper()
{
  DBUG_ASSERT(expr_cache == 0);
}


Item_cache_wrapper::Item_cache_wrapper(Item *item_arg)
:orig_item(item_arg), expr_cache(NULL), expr_value(NULL)
{
  DBUG_ASSERT(orig_item->fixed);
  max_length= orig_item->max_length;
  maybe_null= orig_item->maybe_null;
  decimals=   orig_item->decimals;
  collation.set(orig_item->collation);
  with_sum_func= orig_item->with_sum_func;
  with_field= orig_item->with_field;
  unsigned_flag= orig_item->unsigned_flag;
  name= item_arg->name;
  name_length= item_arg->name_length;
  with_subselect=  orig_item->with_subselect;

  if ((expr_value= Item_cache::get_cache(orig_item)))
    expr_value->setup(orig_item);

  fixed= 1;
}


/**
  Initialize the cache if it is needed
*/

void Item_cache_wrapper::init_on_demand()
{
    if (!expr_cache->is_inited())
    {
      orig_item->get_cache_parameters(parameters);
      expr_cache->init();
    }
}


void Item_cache_wrapper::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  if (expr_cache)
  {
    init_on_demand();
    expr_cache->print(str, query_type);
  }
  else
    str->append(STRING_WITH_LEN("<<DISABLED>>"));
  str->append('(');
  orig_item->print(str, query_type);
  str->append(')');
}


/**
  Prepare the expression cache wrapper (do nothing)

  @retval FALSE OK
*/

bool Item_cache_wrapper::fix_fields(THD *thd  __attribute__((unused)),
                                    Item **it __attribute__((unused)))
{
  DBUG_ASSERT(orig_item->fixed);
  DBUG_ASSERT(fixed);
  return FALSE;
}


/**
  Clean the expression cache wrapper up before reusing it.
*/

void Item_cache_wrapper::cleanup()
{
  DBUG_ENTER("Item_cache_wrapper::cleanup");
  Item_result_field::cleanup();
  delete expr_cache;
  expr_cache= 0;
  /* expr_value is Item so it will be destroyed from list of Items */
  expr_value= 0;
  parameters.empty();
  DBUG_VOID_RETURN;
}


/**
  Create an expression cache that uses a temporary table

  @param thd           Thread handle
  @param depends_on    Parameters of the expression to create cache for

  @details
  The function takes 'depends_on' as the list of all parameters for
  the expression wrapped into this object and creates an expression
  cache in a temporary table containing the field for the parameters
  and the result of the expression.

  @retval FALSE OK
  @retval TRUE  Error
*/

bool Item_cache_wrapper::set_cache(THD *thd)
{
  DBUG_ENTER("Item_cache_wrapper::set_cache");
  DBUG_ASSERT(expr_cache == 0);
  expr_cache= new Expression_cache_tmptable(thd, parameters, expr_value);
  DBUG_RETURN(expr_cache == NULL);
}


/**
  Check if the current values of the parameters are in the expression cache

  @details
  The function checks whether the current set of the parameters of the
  referenced item can be found in the expression cache. If so the function
  returns the item by which the result of the expression can be easily
  extracted from the cache with the corresponding val_* method.

  @retval NULL    - parameters are not in the cache
  @retval <item*> - item providing the result of the expression found in cache
*/

Item *Item_cache_wrapper::check_cache()
{
  DBUG_ENTER("Item_cache_wrapper::check_cache");
  if (expr_cache)
  {
    Expression_cache_tmptable::result res;
    Item *cached_value;
    init_on_demand();
    res= expr_cache->check_value(&cached_value);
    if (res == Expression_cache_tmptable::HIT)
      DBUG_RETURN(cached_value);
  }
  DBUG_RETURN(NULL);
}


/**
  Get the value of the cached expression and put it in the cache
*/

inline void Item_cache_wrapper::cache()
{
  expr_value->store(orig_item);
  expr_value->cache_value();
  expr_cache->put_value(expr_value); // put in expr_cache
}


/**
  Get the value of the possibly cached item into the field.
*/

void Item_cache_wrapper::save_val(Field *to)
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_int");
  if (!expr_cache)
  {
    orig_item->save_val(to);
    null_value= orig_item->null_value;
    DBUG_VOID_RETURN;
  }

  if ((cached_value= check_cache()))
  {
    cached_value->save_val(to);
    null_value= cached_value->null_value;
    DBUG_VOID_RETURN;
  }
  cache();
  null_value= expr_value->null_value;
  expr_value->save_val(to);
  DBUG_VOID_RETURN;
}


/**
  Get the integer value of the possibly cached item.
*/

longlong Item_cache_wrapper::val_int()
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_int");
  if (!expr_cache)
  {
    longlong tmp= orig_item->val_int();
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    longlong tmp= cached_value->val_int();
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  null_value= expr_value->null_value;
  DBUG_RETURN(expr_value->val_int());
}


/**
  Get the real value of the possibly cached item
*/

double Item_cache_wrapper::val_real()
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_real");
  if (!expr_cache)
  {
    double tmp= orig_item->val_real();
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    double tmp= cached_value->val_real();
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  null_value= expr_value->null_value;
  DBUG_RETURN(expr_value->val_real());
}


/**
  Get the string value of the possibly cached item
*/

String *Item_cache_wrapper::val_str(String* str)
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_str");
  if (!expr_cache)
  {
    String *tmp= orig_item->val_str(str);
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    String *tmp= cached_value->val_str(str);
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  if ((null_value= expr_value->null_value))
    DBUG_RETURN(NULL);
  DBUG_RETURN(expr_value->val_str(str));
}


/**
  Get the decimal value of the possibly cached item
*/

my_decimal *Item_cache_wrapper::val_decimal(my_decimal* decimal_value)
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_decimal");
  if (!expr_cache)
  {
    my_decimal *tmp= orig_item->val_decimal(decimal_value);
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    my_decimal *tmp= cached_value->val_decimal(decimal_value);
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  if ((null_value= expr_value->null_value))
    DBUG_RETURN(NULL);
  DBUG_RETURN(expr_value->val_decimal(decimal_value));
}


/**
  Get the boolean value of the possibly cached item
*/

bool Item_cache_wrapper::val_bool()
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::val_bool");
  if (!expr_cache)
  {
    bool tmp= orig_item->val_bool();
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    bool tmp= cached_value->val_bool();
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  null_value= expr_value->null_value;
  DBUG_RETURN(expr_value->val_bool());
}


/**
  Check for NULL the value of the possibly cached item
*/

bool Item_cache_wrapper::is_null()
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::is_null");
  if (!expr_cache)
  {
    bool tmp= orig_item->is_null();
    null_value= orig_item->null_value;
    DBUG_RETURN(tmp);
  }

  if ((cached_value= check_cache()))
  {
    bool tmp= cached_value->is_null();
    null_value= cached_value->null_value;
    DBUG_RETURN(tmp);
  }
  cache();
  DBUG_RETURN((null_value= expr_value->null_value));
}


/**
  Get the date value of the possibly cached item
*/

bool Item_cache_wrapper::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{
  Item *cached_value;
  DBUG_ENTER("Item_cache_wrapper::get_date");
  if (!expr_cache)
    DBUG_RETURN((null_value= orig_item->get_date(ltime, fuzzydate)));

  if ((cached_value= check_cache()))
    DBUG_RETURN((null_value= cached_value->get_date(ltime, fuzzydate)));

  cache();
  DBUG_RETURN((null_value= expr_value->get_date(ltime, fuzzydate)));
}


int Item_cache_wrapper::save_in_field(Field *to, bool no_conversions)
{
  int res;
  DBUG_ASSERT(!result_field);
  res= orig_item->save_in_field(to, no_conversions);
  null_value= orig_item->null_value;
  return res;
}


Item* Item_cache_wrapper::get_tmp_table_item(THD *thd_arg)
{
  if (!orig_item->with_sum_func && !orig_item->const_item())
    return new Item_field(result_field);
  return copy_or_same(thd_arg);
}


/**
  Prepare referenced field then call usual Item_direct_ref::fix_fields .

  @param thd         thread handler
  @param reference   reference on reference where this item stored

  @retval
    FALSE   OK
  @retval
    TRUE    Error
*/

bool Item_direct_view_ref::fix_fields(THD *thd, Item **reference)
{
  DBUG_ASSERT(1);
  /* view fild reference must be defined */
  DBUG_ASSERT(*ref);
  /* (*ref)->check_cols() will be made in Item_direct_ref::fix_fields */
  if ((*ref)->fixed)
  {
    Item *ref_item= (*ref)->real_item();
    if (ref_item->type() == Item::FIELD_ITEM)
    {
      /*
        In some cases we need to update table read set(see bug#47150).
        If ref item is FIELD_ITEM and fixed then field and table
        have proper values. So we can use them for update.
      */
      Field *fld= ((Item_field*) ref_item)->field;
      DBUG_ASSERT(fld && fld->table);
      if (thd->mark_used_columns == MARK_COLUMNS_READ)
        bitmap_set_bit(fld->table->read_set, fld->field_index);
    }
  }
  else if (!(*ref)->fixed &&
           ((*ref)->fix_fields(thd, ref)))
    return TRUE;

  if (Item_direct_ref::fix_fields(thd, reference))
    return TRUE;
  if (view->table && view->table->maybe_null)
    maybe_null= TRUE;
  return FALSE;
}

/*
  Prepare referenced outer field then call usual Item_direct_ref::fix_fields

  SYNOPSIS
    Item_outer_ref::fix_fields()
    thd         thread handler
    reference   reference on reference where this item stored

  RETURN
    FALSE   OK
    TRUE    Error
*/

bool Item_outer_ref::fix_fields(THD *thd, Item **reference)
{
  bool err;
  /* outer_ref->check_cols() will be made in Item_direct_ref::fix_fields */
  if ((*ref) && !(*ref)->fixed && ((*ref)->fix_fields(thd, reference)))
    return TRUE;
  err= Item_direct_ref::fix_fields(thd, reference);
  if (!outer_ref)
    outer_ref= *ref;
  if ((*ref)->type() == Item::FIELD_ITEM)
    table_name= ((Item_field*)outer_ref)->table_name;
  return err;
}


void Item_outer_ref::fix_after_pullout(st_select_lex *new_parent, Item **ref)
{
  if (get_depended_from() == new_parent)
  {
    *ref= outer_ref;
    (*ref)->fix_after_pullout(new_parent, ref);
  }
}

void Item_ref::fix_after_pullout(st_select_lex *new_parent, Item **refptr)
{
  (*ref)->fix_after_pullout(new_parent, ref);
  if (get_depended_from() == new_parent)
    depended_from= NULL;
}


/**
  Mark references from inner selects used in group by clause

  The method is used by the walk method when called for the expressions
  from the group by clause. The callsare  occurred in the function
  fix_inner_refs invoked by JOIN::prepare.
  The parameter passed to Item_outer_ref::check_inner_refs_processor
  is the iterator over the list of inner references from the subselects
  of the select to be prepared. The function marks those references
  from this list whose occurrences are encountered in the group by 
  expressions passed to the walk method.  
 
  @param arg  pointer to the iterator over a list of inner references

  @return
    FALSE always
*/

bool Item_outer_ref::check_inner_refs_processor(uchar *arg)
{
  List_iterator_fast<Item_outer_ref> *it=
    ((List_iterator_fast<Item_outer_ref> *) arg);
  Item_outer_ref *ref;
  while ((ref= (*it)++))
  {
    if (ref == this)
    {
      ref->found_in_group_by= 1;
      break;
    }
  }
  (*it).rewind();
  return FALSE;
}


/**
  Compare two view column references for equality.

  A view column reference is considered equal to another column
  reference if the second one is a view column and if both column
  references resolve to the same item. It is assumed that both
  items are of the same type.

  @param item        item to compare with
  @param binary_cmp  make binary comparison

  @retval
    TRUE    Referenced item is equal to given item
  @retval
    FALSE   otherwise
*/

bool Item_direct_view_ref::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() == REF_ITEM)
  {
    Item_ref *item_ref= (Item_ref*) item;
    if (item_ref->ref_type() == VIEW_REF)
    {
      Item *item_ref_ref= *(item_ref->ref);
      return ((*ref)->real_item() == item_ref_ref->real_item());
    }
  }
  return FALSE;
}


Item_equal *Item_direct_view_ref::find_item_equal(COND_EQUAL *cond_equal)
{
  Item* field_item= real_item();
  if (field_item->type() != FIELD_ITEM)
    return NULL;
  return ((Item_field *) field_item)->find_item_equal(cond_equal);  
}


/**
  Check whether a reference to field item can be substituted for an equal item

  @details
  The function checks whether a substitution of a reference to field item for
  an equal item is valid.

  @param arg   *arg != NULL <-> the reference is in the context
               where substitution for an equal item is valid

  @note
    See also the note for Item_field::subst_argument_checker

  @retval
    TRUE   substitution is valid
  @retval
    FALSE  otherwise
*/
bool Item_direct_view_ref::subst_argument_checker(uchar **arg)
{
  bool res= FALSE;
  if (*arg)
  { 
    Item *item= real_item();
    if (item->type() == FIELD_ITEM &&
        (*arg == (uchar *) Item::ANY_SUBST || 
         result_type() != STRING_RESULT ||
         (((Item_field *) item)->field->flags & BINARY_FLAG)))
      res= TRUE;
  }
  /* Block any substitution into the wrapped object */
  if (*arg)
    *arg= NULL; 
  return res; 
}


/**
  Set a pointer to the multiple equality the view field reference belongs to
  (if any).

  @details
  The function looks for a multiple equality containing this item of the type
  Item_direct_view_ref among those referenced by arg.
  In the case such equality exists the function does the following.
  If the found multiple equality contains a constant, then the item
  is substituted for this constant, otherwise the function sets a pointer
  to the multiple equality in the item.

  @param arg    reference to list of multiple equalities where
                the item (this object) is to be looked for

  @note
    This function is supposed to be called as a callback parameter in calls
    of the compile method.

  @note 
    The function calls Item_field::equal_fields_propagator for the field item
    this->real_item() to do the job. Then it takes the pointer to equal_item
    from this field item and assigns it to this->item_equal.

  @return
    - pointer to the replacing constant item, if the field item was substituted
    - pointer to the field item, otherwise.
*/

Item *Item_direct_view_ref::equal_fields_propagator(uchar *arg)
{
  Item *field_item= real_item();
  if (field_item->type() != FIELD_ITEM)
    return this;
  Item *item= field_item->equal_fields_propagator(arg);
  set_item_equal(field_item->get_item_equal());
  field_item->set_item_equal(NULL);
  if (item != field_item)
    return item;
  return this;
}


/**
  Replace an Item_direct_view_ref for an equal Item_field evaluated earlier
  (if any).

  @details
  If this->item_equal points to some item and coincides with arg then
  the function returns a pointer to a field item that is referred to by the 
  first element of the item_equal list which the Item_direct_view_ref
  object belongs to unless item_equal contains  a constant item. In this
  case the function returns this constant item (if the substitution does
   not require conversion).   
  If the Item_direct_view_item object does not refer any Item_equal object
  'this' is returned .

  @param arg   NULL or points to so some item of the Item_equal type  

  @note
    This function is supposed to be called as a callback parameter in calls
    of the transformer method.

  @note 
    The function calls Item_field::replace_equal_field for the field item
    this->real_item() to do the job.

  @return
    - pointer to a replacement Item_field if there is a better equal item or
      a pointer to a constant equal item;
    - this - otherwise.
*/

Item *Item_direct_view_ref::replace_equal_field(uchar *arg)
{
  Item *field_item= real_item();
  if (field_item->type() != FIELD_ITEM)
    return this;
  field_item->set_item_equal(item_equal);
  Item *item= field_item->replace_equal_field(arg);
  field_item->set_item_equal(0);
  return item != field_item ? item : this;
}


bool Item_default_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == DEFAULT_VALUE_ITEM && 
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_default_value::fix_fields(THD *thd, Item **items)
{
  Item *real_arg;
  Item_field *field_arg;
  Field *def_field;
  DBUG_ASSERT(fixed == 0);

  if (!arg)
  {
    fixed= 1;
    return FALSE;
  }
  if (!arg->fixed && arg->fix_fields(thd, &arg))
    goto error;


  real_arg= arg->real_item();
  if (real_arg->type() != FIELD_ITEM)
  {
    my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), arg->name);
    goto error;
  }

  field_arg= (Item_field *)real_arg;
  if (field_arg->field->flags & NO_DEFAULT_VALUE_FLAG)
  {
    my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), field_arg->field->field_name);
    goto error;
  }
  if (!(def_field= (Field*) sql_alloc(field_arg->field->size_of())))
    goto error;
  memcpy(def_field, field_arg->field, field_arg->field->size_of());
  def_field->move_field_offset((my_ptrdiff_t)
                               (def_field->table->s->default_values -
                                def_field->table->record[0]));
  set_field(def_field);
  return FALSE;

error:
  context->process_error(thd);
  return TRUE;
}


void Item_default_value::print(String *str, enum_query_type query_type)
{
  if (!arg)
  {
    str->append(STRING_WITH_LEN("default"));
    return;
  }
  str->append(STRING_WITH_LEN("default("));
  arg->print(str, query_type);
  str->append(')');
}


int Item_default_value::save_in_field(Field *field_arg, bool no_conversions)
{
  if (!arg)
  {
    if (field_arg->flags & NO_DEFAULT_VALUE_FLAG &&
        field_arg->real_type() != MYSQL_TYPE_ENUM)
    {
      if (field_arg->reset())
      {
        my_message(ER_CANT_CREATE_GEOMETRY_OBJECT,
                   ER(ER_CANT_CREATE_GEOMETRY_OBJECT), MYF(0));
        return -1;
      }

      if (context->error_processor == &view_error_processor)
      {
        TABLE_LIST *view= cached_table->top_table();
        push_warning_printf(field_arg->table->in_use,
                            MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_VIEW_FIELD,
                            ER(ER_NO_DEFAULT_FOR_VIEW_FIELD),
                            view->view_db.str,
                            view->view_name.str);
      }
      else
      {
        push_warning_printf(field_arg->table->in_use,
                            MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_FIELD,
                            ER(ER_NO_DEFAULT_FOR_FIELD),
                            field_arg->field_name);
      }
      return 1;
    }
    field_arg->set_default();
    return 0;
  }
  return Item_field::save_in_field(field_arg, no_conversions);
}


/**
  This method like the walk method traverses the item tree, but at the
  same time it can replace some nodes in the tree.
*/ 

Item *Item_default_value::transform(Item_transformer transformer, uchar *args)
{
  DBUG_ASSERT(!current_thd->is_stmt_prepare());

  /*
    If the value of arg is NULL, then this object represents a constant,
    so further transformation is unnecessary (and impossible).
  */
  if (!arg)
    return 0;

  Item *new_item= arg->transform(transformer, args);
  if (!new_item)
    return 0;

  /*
    THD::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (arg != new_item)
    current_thd->change_item_tree(&arg, new_item);
  return (this->*transformer)(args);
}


bool Item_insert_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == INSERT_VALUE_ITEM &&
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_insert_value::fix_fields(THD *thd, Item **items)
{
  DBUG_ASSERT(fixed == 0);
  /* We should only check that arg is in first table */
  if (!arg->fixed)
  {
    bool res;
    TABLE_LIST *orig_next_table= context->last_name_resolution_table;
    context->last_name_resolution_table= context->first_name_resolution_table;
    res= arg->fix_fields(thd, &arg);
    context->last_name_resolution_table= orig_next_table;
    if (res)
      return TRUE;
  }

  if (arg->type() == REF_ITEM)
    arg= static_cast<Item_ref *>(arg)->ref[0];
  if (arg->type() != FIELD_ITEM)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0), "", "VALUES() function");
    return TRUE;
  }

  Item_field *field_arg= (Item_field *)arg;

  if (field_arg->field->table->insert_values)
  {
    Field *def_field= (Field*) sql_alloc(field_arg->field->size_of());
    if (!def_field)
      return TRUE;
    memcpy(def_field, field_arg->field, field_arg->field->size_of());
    def_field->move_field_offset((my_ptrdiff_t)
                                 (def_field->table->insert_values -
                                  def_field->table->record[0]));
    set_field(def_field);
  }
  else
  {
    Field *tmp_field= field_arg->field;
    /* charset doesn't matter here, it's to avoid sigsegv only */
    tmp_field= new Field_null(0, 0, Field::NONE, field_arg->field->field_name,
                          &my_charset_bin);
    if (tmp_field)
    {
      tmp_field->init(field_arg->field->table);
      set_field(tmp_field);
    }
  }
  return FALSE;
}

void Item_insert_value::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("values("));
  arg->print(str, query_type);
  str->append(')');
}


/**
  Find index of Field object which will be appropriate for item
  representing field of row being changed in trigger.

  @param thd     current thread context
  @param table   table of trigger (and where we looking for fields)
  @param table_grant_info   GRANT_INFO of the subject table

  @note
    This function does almost the same as fix_fields() for Item_field
    but is invoked right after trigger definition parsing. Since at
    this stage we can't say exactly what Field object (corresponding
    to TABLE::record[0] or TABLE::record[1]) should be bound to this
    Item, we only find out index of the Field and then select concrete
    Field object in fix_fields() (by that time Table_trigger_list::old_field/
    new_field should point to proper array of Fields).
    It also binds Item_trigger_field to Table_triggers_list object for
    table of trigger which uses this item.
*/

void Item_trigger_field::setup_field(THD *thd, TABLE *table,
                                     GRANT_INFO *table_grant_info)
{
  /*
    It is too early to mark fields used here, because before execution
    of statement that will invoke trigger other statements may use same
    TABLE object, so all such mark-up will be wiped out.
    So instead we do it in Table_triggers_list::mark_fields_used()
    method which is called during execution of these statements.
  */
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
  thd->mark_used_columns= MARK_COLUMNS_NONE;
  /*
    Try to find field by its name and if it will be found
    set field_idx properly.
  */
  (void)find_field_in_table(thd, table, field_name, (uint) strlen(field_name),
                            0, &field_idx);
  thd->mark_used_columns= save_mark_used_columns;
  triggers= table->triggers;
  table_grants= table_grant_info;
}


bool Item_trigger_field::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == TRIGGER_FIELD_ITEM &&
         row_version == ((Item_trigger_field *)item)->row_version &&
         !my_strcasecmp(system_charset_info, field_name,
                        ((Item_trigger_field *)item)->field_name);
}


void Item_trigger_field::set_required_privilege(bool rw)
{
  /*
    Require SELECT and UPDATE privilege if this field will be read and
    set, and only UPDATE privilege for setting the field.
  */
  want_privilege= (rw ? SELECT_ACL | UPDATE_ACL : UPDATE_ACL);
}


bool Item_trigger_field::set_value(THD *thd, sp_rcontext * /*ctx*/, Item **it)
{
  Item *item= sp_prepare_func_item(thd, it);

  return (!item || (!fixed && fix_fields(thd, 0)) ||
          (item->save_in_field(field, 0) < 0));
}


bool Item_trigger_field::fix_fields(THD *thd, Item **items)
{
  /*
    Since trigger is object tightly associated with TABLE object most
    of its set up can be performed during trigger loading i.e. trigger
    parsing! So we have little to do in fix_fields. :)
  */

  DBUG_ASSERT(fixed == 0);

  /* Set field. */

  if (field_idx != (uint)-1)
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /*
      Check access privileges for the subject table. We check privileges only
      in runtime.
    */

    if (table_grants)
    {
      table_grants->want_privilege= want_privilege;

      if (check_grant_column(thd, table_grants, triggers->trigger_table->s->db.str,
                             triggers->trigger_table->s->table_name.str, field_name,
                             strlen(field_name), thd->security_ctx))
        return TRUE;
    }
#endif // NO_EMBEDDED_ACCESS_CHECKS

    field= (row_version == OLD_ROW) ? triggers->old_field[field_idx] :
                                      triggers->new_field[field_idx];
    set_field(field);
    fixed= 1;
    return FALSE;
  }

  my_error(ER_BAD_FIELD_ERROR, MYF(0), field_name,
           (row_version == NEW_ROW) ? "NEW" : "OLD");
  return TRUE;
}


void Item_trigger_field::print(String *str, enum_query_type query_type)
{
  str->append((row_version == NEW_ROW) ? "NEW" : "OLD", 3);
  str->append('.');
  str->append(field_name);
}


void Item_trigger_field::cleanup()
{
  want_privilege= original_privilege;
  /*
    Since special nature of Item_trigger_field we should not do most of
    things from Item_field::cleanup() or Item_ident::cleanup() here.
  */
  Item::cleanup();
}


Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else if (a == ROW_RESULT || b == ROW_RESULT)
    return ROW_RESULT;
  else if (a == TIME_RESULT || b == TIME_RESULT)
    return TIME_RESULT;
  if ((a == INT_RESULT || a == DECIMAL_RESULT) &&
      (b == INT_RESULT || b == DECIMAL_RESULT))
    return DECIMAL_RESULT;
  return REAL_RESULT;
}


void resolve_const_item(THD *thd, Item **ref, Item *comp_item)
{
  Item *item= *ref;
  Item *new_item= NULL;
  if (item->basic_const_item())
    return;                                     // Can't be better
  Item_result res_type=item_cmp_type(comp_item->cmp_type(), item->cmp_type());
  char *name=item->name;			// Alloced by sql_alloc

  switch (res_type) {
  case TIME_RESULT:
  {
    bool is_null;
    Item **ref_copy= ref;
    /* the following call creates a constant and puts it in new_item */
    get_datetime_value(thd, &ref_copy, &new_item, comp_item, &is_null);
    if (is_null)
      new_item= new Item_null(name);
    break;
  }
  case STRING_RESULT:
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),&my_charset_bin),*result;
    result=item->val_str(&tmp);
    if (item->null_value)
      new_item= new Item_null(name);
    else
    {
      uint length= result->length();
      char *tmp_str= sql_strmake(result->ptr(), length);
      new_item= new Item_string(name, tmp_str, length, result->charset());
    }
    break;
  }
  case INT_RESULT:
  {
    longlong result=item->val_int();
    uint length=item->max_length;
    bool null_value=item->null_value;
    new_item= (null_value ? (Item*) new Item_null(name) :
               (Item*) new Item_int(name, result, length));
    break;
  }
  case ROW_RESULT:
  if (item->type() == Item::ROW_ITEM && comp_item->type() == Item::ROW_ITEM)
  {
    /*
      Substitute constants only in Item_rows. Don't affect other Items
      with ROW_RESULT (eg Item_singlerow_subselect).

      For such Items more optimal is to detect if it is constant and replace
      it with Item_row. This would optimize queries like this:
      SELECT * FROM t1 WHERE (a,b) = (SELECT a,b FROM t2 LIMIT 1);
    */
    Item_row *item_row= (Item_row*) item;
    Item_row *comp_item_row= (Item_row*) comp_item;
    uint col;
    new_item= 0;
    /*
      If item and comp_item are both Item_rows and have same number of cols
      then process items in Item_row one by one.
      We can't ignore NULL values here as this item may be used with <=>, in
      which case NULL's are significant.
    */
    DBUG_ASSERT(item->result_type() == comp_item->result_type());
    DBUG_ASSERT(item_row->cols() == comp_item_row->cols());
    col= item_row->cols();
    while (col-- > 0)
      resolve_const_item(thd, item_row->addr(col),
                         comp_item_row->element_index(col));
    break;
  }
  /* Fallthrough */
  case REAL_RESULT:
  {						// It must REAL_RESULT
    double result= item->val_real();
    uint length=item->max_length,decimals=item->decimals;
    bool null_value=item->null_value;
    new_item= (null_value ? (Item*) new Item_null(name) : (Item*)
               new Item_float(name, result, decimals, length));
    break;
  }
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *result= item->val_decimal(&decimal_value);
    uint length= item->max_length, decimals= item->decimals;
    bool null_value= item->null_value;
    new_item= (null_value ?
               (Item*) new Item_null(name) :
               (Item*) new Item_decimal(name, result, length, decimals));
    break;
  }
  case IMPOSSIBLE_RESULT:
    DBUG_ASSERT(0);
    break;
  }
  if (new_item)
    thd->change_item_tree(ref, new_item);
}

/**
  Compare the value stored in field with the expression from the query.

  @param field   Field which the Item is stored in after conversion
  @param item    Original expression from query

  @return Returns an integer greater than, equal to, or less than 0 if
          the value stored in the field is greater than, equal to,
          or less than the original Item. A 0 may also be returned if 
          out of memory.          

  @note We only use this on the range optimizer/partition pruning,
        because in some cases we can't store the value in the field
        without some precision/character loss.

  @todo rewrite it to use Arg_comparator (currently it's a simplified and
        incomplete version of it)
*/

int stored_field_cmp_to_item(THD *thd, Field *field, Item *item)
{
  Item_result res_type=item_cmp_type(field->result_type(),
				     item->result_type());
  if (res_type == STRING_RESULT)
  {
    char item_buff[MAX_FIELD_WIDTH];
    char field_buff[MAX_FIELD_WIDTH];
    
    String item_tmp(item_buff,sizeof(item_buff),&my_charset_bin);
    String field_tmp(field_buff,sizeof(field_buff),&my_charset_bin);
    String *item_result= item->val_str(&item_tmp);
    /*
      Some implementations of Item::val_str(String*) actually modify
      the field Item::null_value, hence we can't check it earlier.
    */
    if (item->null_value)
      return 0;
    String *field_result= field->val_str(&field_tmp);

    enum_field_types field_type= field->type();

    if (field_type == MYSQL_TYPE_DATE || field_type == MYSQL_TYPE_DATETIME ||
        field_type == MYSQL_TYPE_TIMESTAMP)
    {
      enum_mysql_timestamp_type type= MYSQL_TIMESTAMP_ERROR;

      if (field_type == MYSQL_TYPE_DATE)
        type= MYSQL_TIMESTAMP_DATE;
      else
        type= MYSQL_TIMESTAMP_DATETIME;
        
      const char *field_name= field->field_name;
      MYSQL_TIME field_time, item_time;
      get_mysql_time_from_str(thd, field_result, type, field_name, &field_time);
      get_mysql_time_from_str(thd, item_result, type, field_name,  &item_time);

      return my_time_compare(&field_time, &item_time);
    }
    return sortcmp(field_result, item_result, field->charset());
  }
  if (res_type == INT_RESULT)
    return 0;					// Both are of type int
  if (res_type == DECIMAL_RESULT)
  {
    my_decimal item_buf, *item_val,
               field_buf, *field_val;
    item_val= item->val_decimal(&item_buf);
    if (item->null_value)
      return 0;
    field_val= field->val_decimal(&field_buf);
    return my_decimal_cmp(field_val, item_val);
  }
  /*
    We have to check field->cmp_type() instead of res_type,
    as result_type() - and thus res_type - can never be TIME_RESULT (yet).
  */
  if (field->cmp_type() == TIME_RESULT)
  {
    MYSQL_TIME field_time, item_time;
    if (field->type() == MYSQL_TYPE_TIME)
    {
      field->get_time(&field_time);
      item->get_time(&item_time);
    }
    else
    {
      field->get_date(&field_time, TIME_FUZZY_DATE | TIME_INVALID_DATES);
      item->get_date(&item_time, TIME_FUZZY_DATE | TIME_INVALID_DATES);
    }
    return my_time_compare(&field_time, &item_time);
  }
  double result= item->val_real();
  if (item->null_value)
    return 0;
  double field_result= field->val_real();
  if (field_result < result)
    return -1;
  else if (field_result > result)
    return 1;
  return 0;
}

Item_cache* Item_cache::get_cache(const Item *item)
{
  return get_cache(item, item->result_type());
}


/**
  Get a cache item of given type.

  @param item         value to be cached
  @param type         required type of cache

  @return cache item
*/

Item_cache* Item_cache::get_cache(const Item *item, const Item_result type)
{
  switch (type) {
  case INT_RESULT:
    return new Item_cache_int(item->field_type());
  case REAL_RESULT:
    return new Item_cache_real();
  case DECIMAL_RESULT:
    return new Item_cache_decimal();
  case STRING_RESULT:
    return new Item_cache_str(item);
  case ROW_RESULT:
    return new Item_cache_row();
  case TIME_RESULT:
    return new Item_cache_temporal(item->field_type());
  case IMPOSSIBLE_RESULT:
    DBUG_ASSERT(0);
    break;
  }
  return 0;                                     // Impossible
}

void Item_cache::store(Item *item)
{
  example= item;
  if (!item)
    null_value= TRUE;
  value_cached= FALSE;
}

void Item_cache::print(String *str, enum_query_type query_type)
{
  if (value_cached)
  {
    print_value(str);
    return;
  }
  str->append(STRING_WITH_LEN("<cache>("));
  if (example)
    example->print(str, query_type);
  else
    Item::print(str, query_type);
  str->append(')');
}

/**
  Assign to this cache NULL value if it is possible
*/

void Item_cache::set_null()
{
  if (maybe_null)
  {
    null_value= TRUE;
    value_cached= TRUE;
  }
}


bool  Item_cache_int::cache_value()
{
  if (!example)
    return FALSE;
  value_cached= TRUE;
  value= example->val_int_result();
  null_value= example->null_value;
  unsigned_flag= example->unsigned_flag;
  return TRUE;
}


String *Item_cache_int::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  str->set_int(value, unsigned_flag, default_charset());
  return str;
}


my_decimal *Item_cache_int::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_val);
  return decimal_val;
}

double Item_cache_int::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0.0;
  }
  return (double) value;
}

longlong Item_cache_int::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0;
  }
  return value;
}


int Item_cache_int::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if ((!value_cached && !cache_value()) || null_value)
    return set_field_to_null_with_conversions(field, no_conversions);

  field->set_notnull();
  error= field->store(value, unsigned_flag);

  return error ? error : field->table->in_use->is_error() ? 1 : 0;
}


String *Item_cache_temporal::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= true;
    return NULL;
  }
  return val_string_from_date(str);
}


bool  Item_cache_temporal::cache_value()
{
  if (!example)
    return false;
  value_cached= true;
 
  MYSQL_TIME ltime;
  if (example->get_date_result(&ltime, TIME_FUZZY_DATE))
    value=0;
  else
    value= pack_time(&ltime);
  null_value= example->null_value;
  return true;
}


bool Item_cache_temporal::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{
  Lazy_string_num str(value);

  if (!value_cached && !cache_value())
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }

  unpack_time(value, ltime);
  ltime->time_type= mysql_type_to_time_type(field_type());
  if (ltime->time_type == MYSQL_TIMESTAMP_TIME)
  {
    ltime->hour+= (ltime->month*32+ltime->day)*24;
    ltime->month= ltime->day= 0;
  }
  return 0;
 
}


int Item_cache_temporal::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if ((!value_cached && !cache_value()) || null_value)
    return set_field_to_null_with_conversions(field, no_conversions);

  field->set_notnull();
 
  MYSQL_TIME ltime;
  unpack_time(value, &ltime);
  ltime.time_type= mysql_type_to_time_type(field_type());
  error= field->store_time_dec(&ltime, decimals);
 

  return error ? error : field->table->in_use->is_error() ? 1 : 0;
}


void Item_cache_temporal::store_packed(longlong val_arg, Item *example)
{
  /* An explicit values is given, save it. */
  store(example);
  value_cached= true;
  value= val_arg;
  null_value= false;
}


bool Item_cache_real::cache_value()
{
  if (!example)
    return FALSE;
  value_cached= TRUE;
  value= example->val_result();
  null_value= example->null_value;
  return TRUE;
}


double Item_cache_real::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0.0;
  }
  return value;
}

longlong Item_cache_real::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0;
  }
  return (longlong) rint(value);
}


String* Item_cache_real::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  str->set_real(value, decimals, default_charset());
  return str;
}


my_decimal *Item_cache_real::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  double2my_decimal(E_DEC_FATAL_ERROR, value, decimal_val);
  return decimal_val;
}


bool Item_cache_decimal::cache_value()
{
  if (!example)
    return FALSE;
  value_cached= TRUE;
  my_decimal *val= example->val_decimal_result(&decimal_value);
  if (!(null_value= example->null_value) && val != &decimal_value)
    my_decimal2decimal(val, &decimal_value);
  return TRUE;
}

double Item_cache_decimal::val_real()
{
  DBUG_ASSERT(fixed);
  double res;
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0.0;
  }
  my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &res);
  return res;
}

longlong Item_cache_decimal::val_int()
{
  DBUG_ASSERT(fixed);
  longlong res;
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0;
  }
  my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &res);
  return res;
}

String* Item_cache_decimal::val_str(String *str)
{
  DBUG_ASSERT(fixed);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  my_decimal_round(E_DEC_FATAL_ERROR, &decimal_value, decimals, FALSE,
                   &decimal_value);
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, str);
  return str;
}

my_decimal *Item_cache_decimal::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  return &decimal_value;
}


bool Item_cache_str::cache_value()
{
  if (!example)
    return FALSE;
  value_cached= TRUE;
  value_buff.set(buffer, sizeof(buffer), example->collation.collation);
  value= example->str_result(&value_buff);
  if ((null_value= example->null_value))
    value= 0;
  else if (value != &value_buff)
  {
    /*
      We copy string value to avoid changing value if 'item' is table field
      in queries like following (where t1.c is varchar):
      select a, 
             (select a,b,c from t1 where t1.a=t2.a) = ROW(a,2,'a'),
             (select c from t1 where a=t2.a)
        from t2;
    */
    value_buff.copy(*value);
    value= &value_buff;
  }
  return TRUE;
}

double Item_cache_str::val_real()
{
  DBUG_ASSERT(fixed == 1);
  int err_not_used;
  char *end_not_used;
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0.0;
  }
  return my_strntod(value->charset(), (char*) value->ptr(),
                    value->length(), &end_not_used, &err_not_used);
}


longlong Item_cache_str::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int err;
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0;
  }
  return my_strntoll(value->charset(), value->ptr(),
                     value->length(), 10, (char**) 0, &err);
}


String* Item_cache_str::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return 0;
  }
  return value;
}


my_decimal *Item_cache_str::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  if ((!value_cached && !cache_value()) || null_value)
  {
    null_value= TRUE;
    return NULL;
  }
  string2my_decimal(E_DEC_FATAL_ERROR, value, decimal_val);
  return decimal_val;
}


int Item_cache_str::save_in_field(Field *field, bool no_conversions)
{
  if ((!value_cached && !cache_value()) || null_value)
    return set_field_to_null_with_conversions(field, no_conversions);

  int res= Item_cache::save_in_field(field, no_conversions);
  return (is_varbinary && field->type() == MYSQL_TYPE_STRING &&
          value->length() < field->field_length) ? 1 : res;
}


bool Item_cache_row::allocate(uint num)
{
  item_count= num;
  THD *thd= current_thd;
  return (!(values= 
	    (Item_cache **) thd->calloc(sizeof(Item_cache *)*item_count)));
}


bool Item_cache_row::setup(Item * item)
{
  example= item;
  if (!values && allocate(item->cols()))
    return 1;
  for (uint i= 0; i < item_count; i++)
  {
    Item *el= item->element_index(i);
    Item_cache *tmp;
    if (!(tmp= values[i]= Item_cache::get_cache(el)))
      return 1;
    tmp->setup(el);
  }
  return 0;
}


void Item_cache_row::store(Item * item)
{
  example= item;
  if (!item)
  {
    null_value= TRUE;
    return;
  }
  for (uint i= 0; i < item_count; i++)
    values[i]->store(item->element_index(i));
}


bool Item_cache_row::cache_value()
{
  if (!example)
    return FALSE;
  value_cached= TRUE;
  null_value= 0;
  example->bring_value();
  for (uint i= 0; i < item_count; i++)
  {
    values[i]->cache_value();
    null_value|= values[i]->null_value;
  }
  return TRUE;
}


void Item_cache_row::illegal_method_call(const char *method)
{
  DBUG_ENTER("Item_cache_row::illegal_method_call");
  DBUG_PRINT("error", ("!!! %s method was called for row item", method));
  DBUG_ASSERT(0);
  my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
  DBUG_VOID_RETURN;
}


bool Item_cache_row::check_cols(uint c)
{
  if (c != item_count)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


bool Item_cache_row::null_inside()
{
  for (uint i= 0; i < item_count; i++)
  {
    if (values[i]->cols() > 1)
    {
      if (values[i]->null_inside())
	return 1;
    }
    else
    {
      values[i]->update_null_value();
      if (values[i]->null_value)
	return 1;
    }
  }
  return 0;
}


void Item_cache_row::bring_value()
{
  if (!example)
    return;
  example->bring_value();
  null_value= example->null_value;
  for (uint i= 0; i < item_count; i++)
    values[i]->bring_value();
}


/**
  Assign to this cache NULL value if it is possible
*/

void Item_cache_row::set_null()
{
  Item_cache::set_null();
  if (!values)
    return;
  for (uint i= 0; i < item_count; i++)
    values[i]->set_null();
};


Item_type_holder::Item_type_holder(THD *thd, Item *item)
  :Item(thd, item), enum_set_typelib(0), fld_type(get_real_type(item))
{
  DBUG_ASSERT(item->fixed);
  maybe_null= item->maybe_null;
  collation.set(item->collation);
  get_full_info(item);
  /* fix variable decimals which always is NOT_FIXED_DEC */
  if (Field::result_merge_type(fld_type) == INT_RESULT)
    decimals= 0;
  prev_decimal_int_part= item->decimal_int_part();
#ifdef HAVE_SPATIAL
  if (item->field_type() == MYSQL_TYPE_GEOMETRY)
    geometry_type= item->get_geometry_type();
#endif /* HAVE_SPATIAL */
}


/**
  Return expression type of Item_type_holder.

  @return
    Item_result (type of internal MySQL expression result)
*/

Item_result Item_type_holder::result_type() const
{
  return Field::result_merge_type(fld_type);
}


/**
  Find real field type of item.

  @return
    type of field which should be created to store item value
*/

enum_field_types Item_type_holder::get_real_type(Item *item)
{
  if (item->type() == REF_ITEM)
    item= item->real_item();
  switch(item->type())
  {
  case FIELD_ITEM:
  {
    /*
      Item_field::field_type ask Field_type() but sometimes field return
      a different type, like for enum/set, so we need to ask real type.
    */
    Field *field= ((Item_field *) item)->field;
    enum_field_types type= field->real_type();
    if (field->is_created_from_null_item)
      return MYSQL_TYPE_NULL;
    /* work around about varchar type field detection */
    if (type == MYSQL_TYPE_STRING && field->type() == MYSQL_TYPE_VAR_STRING)
      return MYSQL_TYPE_VAR_STRING;
    return type;
  }
  case SUM_FUNC_ITEM:
  {
    /*
      Argument of aggregate function sometimes should be asked about field
      type
    */
    Item_sum *item_sum= (Item_sum *) item;
    if (item_sum->keep_field_type())
      return get_real_type(item_sum->get_arg(0));
    break;
  }
  case FUNC_ITEM:
    if (((Item_func *) item)->functype() == Item_func::GUSERVAR_FUNC)
    {
      /*
        There are work around of problem with changing variable type on the
        fly and variable always report "string" as field type to get
        acceptable information for client in send_field, so we make field
        type from expression type.
      */
      switch (item->result_type()) {
      case STRING_RESULT:
        return MYSQL_TYPE_VAR_STRING;
      case INT_RESULT:
        return MYSQL_TYPE_LONGLONG;
      case REAL_RESULT:
        return MYSQL_TYPE_DOUBLE;
      case DECIMAL_RESULT:
        return MYSQL_TYPE_NEWDECIMAL;
      case ROW_RESULT:
      case TIME_RESULT:
      case IMPOSSIBLE_RESULT:
        DBUG_ASSERT(0);
        return MYSQL_TYPE_VAR_STRING;
      }
    }
    break;
  default:
    break;
  }
  return item->field_type();
}

/**
  Find field type which can carry current Item_type_holder type and
  type of given Item.

  @param thd     thread handler
  @param item    given item to join its parameters with this item ones

  @retval
    TRUE   error - types are incompatible
  @retval
    FALSE  OK
*/

bool Item_type_holder::join_types(THD *thd, Item *item)
{
  uint max_length_orig= max_length;
  uint decimals_orig= decimals;
  DBUG_ENTER("Item_type_holder::join_types");
  DBUG_PRINT("info:", ("was type %d len %d, dec %d name %s",
                       fld_type, max_length, decimals,
                       (name ? name : "<NULL>")));
  DBUG_PRINT("info:", ("in type %d len %d, dec %d",
                       get_real_type(item),
                       item->max_length, item->decimals));
  fld_type= Field::field_type_merge(fld_type, get_real_type(item));
  {
    uint item_decimals= item->decimals;
    /* fix variable decimals which always is NOT_FIXED_DEC */
    if (Field::result_merge_type(fld_type) == INT_RESULT)
      item_decimals= 0;
    decimals= max(decimals, item_decimals);
  }
  if (Field::result_merge_type(fld_type) == DECIMAL_RESULT)
  {
    decimals= min(max(decimals, item->decimals), DECIMAL_MAX_SCALE);
    int item_int_part= item->decimal_int_part();
    int item_prec = max(prev_decimal_int_part, item_int_part) + decimals;
    int precision= min(item_prec, DECIMAL_MAX_PRECISION);
    unsigned_flag&= item->unsigned_flag;
    max_length= my_decimal_precision_to_length_no_truncation(precision,
                                                             decimals,
                                                             unsigned_flag);
  }

  switch (Field::result_merge_type(fld_type))
  {
  case STRING_RESULT:
  {
    const char *old_cs, *old_derivation;
    uint32 old_max_chars= max_length / collation.collation->mbmaxlen;
    old_cs= collation.collation->name;
    old_derivation= collation.derivation_name();
    if (collation.aggregate(item->collation, MY_COLL_ALLOW_CONV))
    {
      my_error(ER_CANT_AGGREGATE_2COLLATIONS, MYF(0),
	       old_cs, old_derivation,
	       item->collation.collation->name,
	       item->collation.derivation_name(),
	       "UNION");
      DBUG_RETURN(TRUE);
    }
    /*
      To figure out max_length, we have to take into account possible
      expansion of the size of the values because of character set
      conversions.
     */
    if (collation.collation != &my_charset_bin)
    {
      max_length= max(old_max_chars * collation.collation->mbmaxlen,
                      display_length(item) /
                      item->collation.collation->mbmaxlen *
                      collation.collation->mbmaxlen);
    }
    else
      set_if_bigger(max_length, display_length(item));
    break;
  }
  case REAL_RESULT:
  {
    if (decimals != NOT_FIXED_DEC)
    {
      /*
        For FLOAT(M,D)/DOUBLE(M,D) do not change precision
         if both fields have the same M and D
      */
      if (item->max_length != max_length_orig ||
          item->decimals != decimals_orig)
      {
        int delta1= max_length_orig - decimals_orig;
        int delta2= item->max_length - item->decimals;
        max_length= max(delta1, delta2) + decimals;
        if (fld_type == MYSQL_TYPE_FLOAT && max_length > FLT_DIG + 2)
        {
          max_length= MAX_FLOAT_STR_LENGTH;
          decimals= NOT_FIXED_DEC;
        } 
        else if (fld_type == MYSQL_TYPE_DOUBLE && max_length > DBL_DIG + 2)
        {
          max_length= MAX_DOUBLE_STR_LENGTH;
          decimals= NOT_FIXED_DEC;
        }
      }
    }
    else
      max_length= (fld_type == MYSQL_TYPE_FLOAT) ? FLT_DIG+6 : DBL_DIG+7;
    break;
  }
  default:
    max_length= max(max_length, display_length(item));
  };
  maybe_null|= item->maybe_null;
  get_full_info(item);

  /* Remember decimal integer part to be used in DECIMAL_RESULT handleng */
  prev_decimal_int_part= decimal_int_part();
  DBUG_PRINT("info", ("become type: %d  len: %u  dec: %u",
                      (int) fld_type, max_length, (uint) decimals));
  DBUG_RETURN(FALSE);
}

/**
  Calculate lenth for merging result for given Item type.

  @param item  Item for length detection

  @return
    length
*/

uint32 Item_type_holder::display_length(Item *item)
{
  if (item->type() == Item::FIELD_ITEM)
    return ((Item_field *)item)->max_disp_length();

  switch (item->field_type())
  {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_NEWDECIMAL:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_GEOMETRY:
    return item->max_length;
  case MYSQL_TYPE_TINY:
    return 4;
  case MYSQL_TYPE_SHORT:
    return 6;
  case MYSQL_TYPE_LONG:
    return MY_INT32_NUM_DECIMAL_DIGITS;
  case MYSQL_TYPE_FLOAT:
    return 25;
  case MYSQL_TYPE_DOUBLE:
    return 53;
  case MYSQL_TYPE_NULL:
    return 0;
  case MYSQL_TYPE_LONGLONG:
    return 20;
  case MYSQL_TYPE_INT24:
    return 8;
  default:
    DBUG_ASSERT(0); // we should never go there
    return 0;
  }
}


/**
  Make temporary table field according collected information about type
  of UNION result.

  @param table  temporary table for which we create fields

  @return
    created field
*/

Field *Item_type_holder::make_field_by_type(TABLE *table)
{
  /*
    The field functions defines a field to be not null if null_ptr is not 0
  */
  uchar *null_ptr= maybe_null ? (uchar*) "" : 0;
  Field *field;

  switch (fld_type) {
  case MYSQL_TYPE_ENUM:
    DBUG_ASSERT(enum_set_typelib);
    field= new Field_enum((uchar *) 0, max_length, null_ptr, 0,
                          Field::NONE, name,
                          get_enum_pack_length(enum_set_typelib->count),
                          enum_set_typelib, collation.collation);
    if (field)
      field->init(table);
    return field;
  case MYSQL_TYPE_SET:
    DBUG_ASSERT(enum_set_typelib);
    field= new Field_set((uchar *) 0, max_length, null_ptr, 0,
                         Field::NONE, name,
                         get_set_pack_length(enum_set_typelib->count),
                         enum_set_typelib, collation.collation);
    if (field)
      field->init(table);
    return field;
  case MYSQL_TYPE_NULL:
    return make_string_field(table);
  default:
    break;
  }
  return tmp_table_field_from_field_type(table, 0);
}


/**
  Get full information from Item about enum/set fields to be able to create
  them later.

  @param item    Item for information collection
*/
void Item_type_holder::get_full_info(Item *item)
{
  if (fld_type == MYSQL_TYPE_ENUM ||
      fld_type == MYSQL_TYPE_SET)
  {
    if (item->type() == Item::SUM_FUNC_ITEM &&
        (((Item_sum*)item)->sum_func() == Item_sum::MAX_FUNC ||
         ((Item_sum*)item)->sum_func() == Item_sum::MIN_FUNC))
      item = ((Item_sum*)item)->get_arg(0);
    /*
      We can have enum/set type after merging only if we have one enum|set
      field (or MIN|MAX(enum|set field)) and number of NULL fields
    */
    DBUG_ASSERT((enum_set_typelib &&
                 get_real_type(item) == MYSQL_TYPE_NULL) ||
                (!enum_set_typelib &&
                 item->real_item()->type() == Item::FIELD_ITEM &&
                 (get_real_type(item->real_item()) == MYSQL_TYPE_ENUM ||
                  get_real_type(item->real_item()) == MYSQL_TYPE_SET) &&
                 ((Field_enum*)((Item_field *) item->real_item())->field)->typelib));
    if (!enum_set_typelib)
    {
      enum_set_typelib= ((Field_enum*)((Item_field *) item->real_item())->field)->typelib;
    }
  }
}


double Item_type_holder::val_real()
{
  DBUG_ASSERT(0); // should never be called
  return 0.0;
}


longlong Item_type_holder::val_int()
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

my_decimal *Item_type_holder::val_decimal(my_decimal *)
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

String *Item_type_holder::val_str(String*)
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

void Item_result_field::cleanup()
{
  DBUG_ENTER("Item_result_field::cleanup()");
  Item::cleanup();
  result_field= 0;
  DBUG_VOID_RETURN;
}

/**
  Dummy error processor used by default by Name_resolution_context.

  @note
    do nothing
*/

void dummy_error_processor(THD *thd, void *data)
{}

/**
  Wrapper of hide_view_error call for Name_resolution_context error
  processor.

  @note
    hide view underlying tables details in error messages
*/

void view_error_processor(THD *thd, void *data)
{
  ((TABLE_LIST *)data)->hide_view_error(thd);
}


st_select_lex *Item_ident::get_depended_from() const
{
  st_select_lex *dep;
  if ((dep= depended_from))
    for ( ; dep->merged_into; dep= dep->merged_into) ;
  return dep;
}


table_map Item_ref::used_tables() const		
{
  return get_depended_from() ? OUTER_REF_TABLE_BIT : (*ref)->used_tables(); 
}


void Item_ref::update_used_tables() 
{ 
  if (!get_depended_from())
    (*ref)->update_used_tables(); 
}


table_map Item_direct_view_ref::used_tables() const
{
  return get_depended_from() ?
         OUTER_REF_TABLE_BIT :
         ((view->is_merged_derived() || view->merged || !view->table) ?
          (*ref)->used_tables() :
          view->table->map);
}

table_map Item_direct_view_ref::not_null_tables() const
{
  return get_depended_from() ?
         0 :
         ((view->is_merged_derived() || view->merged || !view->table) ?
          (*ref)->not_null_tables() :
          view->table->map);
}

/*
  we add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE
*/
table_map Item_ref_null_helper::used_tables() const
{
  return (get_depended_from() ?
          OUTER_REF_TABLE_BIT :
          (*ref)->used_tables() | RAND_TABLE_BIT);
}


#ifndef DBUG_OFF

/* Debugger help function */
static char dbug_item_print_buf[256];

const char *dbug_print_item(Item *item)
{
  char *buf= dbug_item_print_buf;
  String str(buf, sizeof(dbug_item_print_buf), &my_charset_bin);
  str.length(0);
  if (!item)
    return "(Item*)NULL";
  item->print(&str ,QT_ORDINARY);
  if (str.c_ptr() == buf)
    return buf;
  else
    return "Couldn't fit into buffer";
}

#endif /*DBUG_OFF*/

/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<Item>;
template class List_iterator<Item>;
template class List_iterator_fast<Item>;
template class List_iterator_fast<Item_field>;
template class List<List_item>;
#endif
