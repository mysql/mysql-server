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


/* Optimizing of many different type of queries with GROUP functions */

#include "mysql_priv.h"
#include "sql_select.h"

static bool find_range_key(TABLE_REF *ref, Field* field,COND *cond);

/*****************************************************************************
** This function is only called for queries with sum functions and no
** GROUP BY part.
** This substitutes constants for some COUNT(), MIN() and MAX() functions.
** The function returns 1 if all items was resolved and -1 on impossible
** conditions
****************************************************************************/

int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds)
{
  List_iterator<Item> it(all_fields);
  int const_result=1;
  bool recalc_const_item=0;
  table_map removed_tables=0;
  Item *item;

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
	    if (table->on_expr || (table->table->file->option_flag() &
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

	  if (!find_range_key(&ref, ((Item_field*) expr)->field,conds))
	  {
	    const_result=0;
	    break;
	  }
	  TABLE *table=((Item_field*) expr)->field->table;
	  bool error=table->file->index_init((uint) ref.key);
	  if (!ref.key_length)
	    error=table->file->index_first(table->record[0]) !=0;
	  else
	    error=table->file->index_read(table->record[0],key_buff,
					  ref.key_length,
					  HA_READ_KEY_OR_NEXT) ||
	      key_cmp(table, key_buff, ref.key, ref.key_length);
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

	  if (!find_range_key(&ref, ((Item_field*) expr)->field,conds))
	  {
	    const_result=0;
	    break;
	  }
	  TABLE *table=((Item_field*) expr)->field->table;
	  if ((table->file->option_flag() & HA_NOT_READ_AFTER_KEY))
	  {
	    const_result=0;
	    break;
	  }
	  bool error=table->file->index_init((uint) ref.key);

	  if (!ref.key_length)
	    error=table->file->index_last(table->record[0]) !=0;
	  else
	  {
	    (void) table->file->index_read(table->record[0], key_buff,
					   ref.key_length,
					   HA_READ_AFTER_KEY);
	    error=table->file->index_prev(table->record[0]) ||
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
  if (conds && (conds->used_tables() & ~ removed_tables))
    const_result=0;
  return const_result;
}

/* Count in how many times table is used (up to MAX_KEY_PARTS+1) */

uint count_table_entries(COND *cond,TABLE *table)
{
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return (cond->used_tables() & table->map) ? MAX_REF_PARTS+1 : 0;

    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
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

    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
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
    return 0;				// Not part of a key. Skipp it

  TABLE *table=field->table;
  if (table->file->option_flag() & HA_WRONG_ASCII_ORDER)
    return(0);				// Can't use key to find last row
  uint idx=0;

  /* Check if some key has field as first key part */
  if (field->key_start && (! cond || ! (cond->used_tables() & table->map)))
  {
    for (key_map key=field->key_start ; !(key & 1) ; idx++)
      key>>=1;
    ref->key_length=0;
    ref->key=idx;
    if (field->part_of_key & ((table_map) 1 << idx))
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
	    left_length < part->store_length)
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
	if (field->part_of_key & ((table_map) 1 << idx))
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
