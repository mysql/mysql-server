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


/* Optimizing of many different type of queries with GROUP functions */

#include "mysql_priv.h"
#include "sql_select.h"

static bool find_range_key(TABLE_REF *ref, Field* field,COND *cond);

/*
  Substitutes constants for some COUNT(), MIN() and MAX() functions.

  SYNOPSIS
    opt_sum_query()
    tables		Tables in query
    all_fields		All fields to be returned
    conds		WHERE clause

  NOTE:
    This function is only called for queries with sum functions and no
    GROUP BY part.

 RETURN VALUES
    0 No errors
    1 if all items was resolved
   -1 on impossible  conditions
*/

int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds)
{
  List_iterator_fast<Item> it(all_fields);
  int const_result= 1;
  bool recalc_const_item= 0;
  table_map removed_tables= 0, outer_tables= 0, used_tables= 0;
  table_map where_tables= 0;
  Item *item;
  COND *org_conds= conds;

  if (conds)
    where_tables= conds->used_tables();

  /* Don't replace expression on a table that is part of an outer join */
  for (TABLE_LIST *tl=tables; tl ; tl= tl->next)
  {
    if (tl->on_expr)
    {
      outer_tables|= tl->table->map;

      /*
	We can't optimise LEFT JOIN in cases where the WHERE condition
	restricts the table that is used, like in:
          SELECT MAX(t1.a) FROM t1 LEFT JOIN t2 join-condition
	  WHERE t2.field IS NULL;
      */
      if (tl->table->map & where_tables)
	return 0;
    }
    else
      used_tables|= tl->table->map;
  }

  /*
    Iterate through item is select part and replace COUNT(), MIN() and MAX()
    with constants (if possible)
  */

  while ((item= it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM)
    {
      Item_sum *item_sum= (((Item_sum*) item));
      switch (item_sum->sum_func()) {
      case Item_sum::COUNT_FUNC:
	/*
	  If the expr in count(expr) can never be null we can change this
	  to the number of rows in the tables
	*/
	if (!conds && !((Item_sum_count*) item)->args[0]->maybe_null)
	{
	  longlong count=1;
	  TABLE_LIST *table;
	  for (table=tables; table ; table=table->next)
	  {
	    if (outer_tables || (table->table->file->table_flags() &
				 HA_NOT_EXACT_COUNT))
	    {
	      const_result=0;			// Can't optimize left join
	      break;
	    }
	    tables->table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
	    count*= table->table->file->records;
	  }
	  if (!table)
	  {
	    ((Item_sum_count*) item)->make_const(count);
	    recalc_const_item=1;
	  }
	}
	else
	  const_result=0;
	break;
      case Item_sum::MIN_FUNC:
      {
	/*
	  If MIN(expr) is the first part of a key or if all previous
	  parts of the key is found in the COND, then we can use
	  indexes to find the key.
	*/
	Item *expr=item_sum->args[0];
	if (expr->type() == Item::FIELD_ITEM)
	{
	  byte key_buff[MAX_KEY_LENGTH];
	  TABLE_REF ref;
	  ref.key_buff=key_buff;
	  Item_field *item_field= ((Item_field*) expr);
	  TABLE *table= item_field->field->table;

	  if ((outer_tables & table->map) ||
	      (!find_range_key(&ref, item_field->field,conds)))
	  {
	    const_result=0;
	    break;
	  }
	  bool error= table->file->index_init((uint) ref.key);
	  enum ha_rkey_function find_flag= HA_READ_KEY_OR_NEXT; 
	  uint prefix_len= ref.key_length;
	  /*
	    If we are doing MIN() on a column with NULL fields
	    we must read the key after the NULL column
	  */
	  if (item_field->field->null_bit)
	  {
	    ref.key_buff[ref.key_length++]=1;
	    find_flag= HA_READ_AFTER_KEY;
	  }

	  if (!ref.key_length)
	    error=table->file->index_first(table->record[0]) !=0;
	  else
	    error=table->file->index_read(table->record[0],key_buff,
					  ref.key_length,
					  find_flag) ||
	      key_cmp(table, key_buff, ref.key, prefix_len);
	  if (table->key_read)
	  {
	    table->key_read=0;
	    table->file->extra(HA_EXTRA_NO_KEYREAD);
	  }
	  table->file->index_end();
	  if (error)
	    return -1;				// No rows matching where
	  removed_tables|= table->map;
	}
	else if (!expr->const_item())		// This is VERY seldom false
	{
	  const_result=0;
	  break;
	}
	((Item_sum_min*) item_sum)->reset();
	((Item_sum_min*) item_sum)->make_const();
	recalc_const_item=1;
	break;
      }
      case Item_sum::MAX_FUNC:
      {
	/*
	  If MAX(expr) is the first part of a key or if all previous
	  parts of the key is found in the COND, then we can use
	  indexes to find the key.
	*/
	Item *expr=item_sum->args[0];
	if (expr->type() == Item::FIELD_ITEM)
	{
	  byte key_buff[MAX_KEY_LENGTH];
	  TABLE_REF ref;
	  ref.key_buff=key_buff;
	  TABLE *table=((Item_field*) expr)->field->table;

	  if ((outer_tables & table->map) ||
	      !find_range_key(&ref, ((Item_field*) expr)->field,conds))
	  {
	    const_result=0;
	    break;
	  }
	  if ((table->file->table_flags() & HA_NOT_READ_AFTER_KEY))
	  {
	    const_result=0;
	    break;
	  }
	  bool error=table->file->index_init((uint) ref.key);

	  if (!ref.key_length)
	    error=table->file->index_last(table->record[0]) !=0;
	  else
	  {
	    error = table->file->index_read(table->record[0], key_buff,
					  ref.key_length,
					  HA_READ_PREFIX_LAST) ||
	      key_cmp(table,key_buff,ref.key,ref.key_length);
	  }
	  if (table->key_read)
	  {
	    table->key_read=0;
	    table->file->extra(HA_EXTRA_NO_KEYREAD);
	  }
	  table->file->index_end();
	  if (error)
	    return -1;				// Impossible query
	  removed_tables|= table->map;
	}
	else if (!expr->const_item())		// This is VERY seldom false
	{
	  const_result=0;
	  break;
	}
	((Item_sum_min*) item_sum)->reset();
	((Item_sum_min*) item_sum)->make_const();
	recalc_const_item=1;
	break;
      }
      default:
	const_result=0;
	break;
      }
    }
    else if (const_result)
    {
      if (recalc_const_item)
	item->update_used_tables();
      if (!item->const_item())
	const_result=0;
    }
  }
  /*
    If we have a where clause, we can only ignore searching in the
    tables if MIN/MAX optimisation replaced all used tables
    This is to not to use replaced values in case of:
    SELECT MIN(key) FROM table_1, empty_table
    removed_tables is != 0 if we have used MIN() or MAX().
  */
  if (removed_tables && used_tables != removed_tables)
    const_result= 0;				// We didn't remove all tables
  return const_result;
}

/* Count in how many times table is used (up to MAX_KEY_PARTS+1) */

uint count_table_entries(COND *cond,TABLE *table)
{
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return (cond->used_tables() & table->map) ? MAX_REF_PARTS+1 : 0;

    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    uint count=0;
    while ((item=li++))
    {
      if ((count+=count_table_entries(item,table)) > MAX_REF_PARTS)
	return MAX_REF_PARTS+1;
    }
    return count;
  }
  if (cond->type() == Item::FUNC_ITEM &&
      (((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
       (((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC)) &&
      cond->used_tables() == table->map)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM)
    {
      if (!(((Item_field*) left_item)->field->flags & PART_KEY_FLAG) ||
	  !right_item->const_item())
	return MAX_REF_PARTS+1;
      return 1;
    }
    if (right_item->type() == Item::FIELD_ITEM)
    {
      if (!(((Item_field*) right_item)->field->flags & PART_KEY_FLAG) ||
	  !left_item->const_item())
	return MAX_REF_PARTS+1;
      return 1;
    }
  }
  return (cond->used_tables() & table->map) ? MAX_REF_PARTS+1 : 0;
}


/* check that the field is usable as key part */

bool part_of_cond(COND *cond,Field *field)
{
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return 0;					// Already checked

    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      if (part_of_cond(item,field))
	return 1;
    }
    return 0;
  }
  if (cond->type() == Item::FUNC_ITEM &&
      (((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
       ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC) &&
      cond->used_tables() == field->table->map)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM)
    {
      if (((Item_field*) left_item)->field != field ||
	  !right_item->const_item())
	return 0;
    }
    else if (right_item->type() == Item::FIELD_ITEM)
    {
      if (((Item_field*) right_item)->field != field ||
	  !left_item->const_item())
	return 0;
      right_item=left_item;			// const item in right
    }
    store_val_in_field(field,right_item);
    return 1;
  }
  return 0;
}


/* Check if we can get value for field by using a key */

static bool find_range_key(TABLE_REF *ref, Field* field, COND *cond)
{
  if (!(field->flags & PART_KEY_FLAG))
    return 0;				// Not part of a key. Skip it

  TABLE *table=field->table;
  uint idx=0;

  /* Check if some key has field as first key part */
  if ((field->key_start & field->table->keys_in_use_for_query) &&
      (! cond || ! (cond->used_tables() & table->map)))
  {
    for (key_map key=field->key_start ;;) 
    {
      for (; !(key & 1) ; idx++)
	key>>=1;
      if (!(table->file->index_flags(idx) & HA_WRONG_ASCII_ORDER))
	break;					// Key is ok
      /* Can't use this key, for looking up min() or max(), end if last one */
      if (key == 1)
	return 0;
    }
    ref->key_length=0;
    ref->key=idx;
    if (field->part_of_key & ((key_map) 1 << idx))
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
    }
    return 1;					// Ok to use key
  }
  /*
  ** Check if WHERE consist of exactly the previous key parts for some key
  */
  if (!cond)
    return 0;
  uint table_entries= count_table_entries(cond,table);
  if (!table_entries || table_entries > MAX_REF_PARTS)
    return 0;

  KEY *keyinfo,*keyinfo_end;
  idx=0;
  for (keyinfo=table->key_info, keyinfo_end=keyinfo+table->keys ;
       keyinfo != keyinfo_end;
       keyinfo++,idx++)
  {
    if (table_entries < keyinfo->key_parts)
    {
      byte *key_ptr=ref->key_buff;
      KEY_PART_INFO *part,*part_end;
      int left_length=MAX_KEY_LENGTH;

      for (part=keyinfo->key_part, part_end=part+table_entries ;
	   part != part_end ;
	   part++)
      {
	if (!part_of_cond(cond,part->field) ||
	    left_length < part->store_length ||
	    (table->file->index_flags(idx) & HA_WRONG_ASCII_ORDER))
	  break;
	// Save found constant
	if (part->null_bit)
	  *key_ptr++= (byte) test(part->field->is_null());
	part->field->get_key_image((char*) key_ptr,part->length);
	key_ptr+=part->store_length - test(part->null_bit);
	left_length-=part->store_length;
      }
      if (part == part_end && part->field == field)
      {
	ref->key_length= (uint) (key_ptr-ref->key_buff);
	ref->key=idx;
	if (field->part_of_key & ((key_map) 1 << idx))
	{
	  table->key_read=1;
	  table->file->extra(HA_EXTRA_KEYREAD);
	}
	return 1;				// Ok to use key
      }
    }
  }
  return 0;					// No possible key
}
