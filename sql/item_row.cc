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
  Item(), array_holder(1)
{
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
  my_error(ER_CARDINALITY_COL, MYF(0), arg_count);
  DBUG_VOID_RETURN;
}

bool Item_row::fix_fields(THD *thd, TABLE_LIST *tabl, Item **ref)
{
  tables= 0;
  for (uint i= 0; i < arg_count; i++)
  {
    if (items[i]->fix_fields(thd, tabl, items+i))
      return 1;
    tables |= items[i]->used_tables();
  }
  return 0;
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
