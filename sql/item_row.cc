/* Copyright (C) 2000 MySQL AB

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

#include "mysql_priv.h"
#include "assert.h"

Item_row::Item_row(List<Item> &arg):
  Item(), array_holder(1), used_tables_cache(0), const_item_cache(1)
{

  //TODO: think placing 2-3 component items in item (as it done for function)
  if ((arg_count= arg.elements))
    items= (Item**) sql_alloc(sizeof(Item*)*arg_count);
  else
    items= 0;
  List_iterator<Item> li(arg);
  uint i= 0;
  Item *item;
  while ((item= li++))
  {
    items[i]= item;
    i++;    
  }
}

void Item_row::illegal_method_call(const char *method)
{
  DBUG_ENTER("Item_row::illegal_method_call");
  DBUG_PRINT("error", ("!!! %s method was called for row item", method));
  DBUG_ASSERT(0);
  my_error(ER_CARDINALITY_COL, MYF(0), 1);
  DBUG_VOID_RETURN;
}

bool Item_row::fix_fields(THD *thd, TABLE_LIST *tabl, Item **ref)
{
  null_value= 0;
  maybe_null= 0;
  for (uint i= 0; i < arg_count; i++)
  {
    if (items[i]->fix_fields(thd, tabl, items+i))
      return 1;
    used_tables_cache |= items[i]->used_tables();
    const_item_cache&= items[i]->const_item();
    maybe_null|= items[i]->maybe_null;
  }
  return 0;
}

void Item_row::update_used_tables()
{
  used_tables_cache= 0;
  const_item_cache= 1;
  for (uint i= 0; i < arg_count; i++)
  {
    items[i]->update_used_tables();
    used_tables_cache|= items[i]->used_tables();
    const_item_cache&= items[i]->const_item();
  }
}

bool Item_row::check_cols(uint c)
{
  if (c != arg_count)
  {
    my_error(ER_CARDINALITY_COL, MYF(0), c);
    return 1;
  }
  return 0;
}

bool Item_row::null_inside()
{
  for (uint i= 0; i < arg_count; i++)
  {
    if (items[i]->cols() > 1)
    {
      if (items[i]->null_inside())
	return 1;
    }
    else
    {
      items[i]->val_int();
      if (items[i]->null_value)
	return 1;
    }
  }
  return 0;
}

void Item_row::bring_value()
{
  for (uint i= 0; i < arg_count; i++)
    items[i]->bring_value();
}

void Item_row::set_outer_resolving()
{
  for (uint i= 0; i < arg_count; i++)
    items[i]->set_outer_resolving();
}

bool Item_row::check_loop(uint id)
{
  if (Item::check_loop(id))
    return 1;
  for (uint i= 0; i < arg_count; i++)
    if (items[i]->check_loop(id))
      return 1;
  return 0;
}
