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


/* Buffers to save and compare item values */

#include "mysql_priv.h"

/*
** Create right type of item_buffer for an item
*/

Item_buff *new_Item_buff(THD *thd, Item *item)
{
  if (item->type() == Item::FIELD_ITEM &&
      !(((Item_field *) item)->field->flags & BLOB_FLAG))
    return new Item_field_buff((Item_field *) item);
  switch (item->result_type()) {
  case STRING_RESULT:
    return new Item_str_buff(thd, (Item_field *) item);
  case INT_RESULT:
    return new Item_int_buff((Item_field *) item);
  case REAL_RESULT:
    return new Item_real_buff(item);
  case DECIMAL_RESULT:
    return new Item_decimal_buff(item);
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}

Item_buff::~Item_buff() {}

/*
** Compare with old value and replace value with new value
** Return true if values have changed
*/

Item_str_buff::Item_str_buff(THD *thd, Item *arg)
  :item(arg), value(min(arg->max_length, thd->variables. max_sort_length))
{}

bool Item_str_buff::cmp(void)
{
  String *res;
  bool tmp;

  res=item->val_str(&tmp_value);
  res->length(min(res->length(), value.alloced_length()));
  if (null_value != item->null_value)
  {
    if ((null_value= item->null_value))
      return TRUE;				// New value was null
    tmp=TRUE;
  }
  else if (null_value)
    return 0;					// new and old value was null
  else
    tmp= sortcmp(&value,res,item->collation.collation) != 0;
  if (tmp)
    value.copy(*res);				// Remember for next cmp
  return tmp;
}

Item_str_buff::~Item_str_buff()
{
  item=0;					// Safety
}

bool Item_real_buff::cmp(void)
{
  double nr= item->val_real();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return TRUE;
  }
  return FALSE;
}

bool Item_int_buff::cmp(void)
{
  longlong nr=item->val_int();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return TRUE;
  }
  return FALSE;
}


bool Item_field_buff::cmp(void)
{
  bool tmp= field->cmp(buff) != 0;		// This is not a blob!
  if (tmp)
    field->get_image(buff,length,field->charset());
  if (null_value != field->is_null())
  {
    null_value= !null_value;
    tmp=TRUE;
  }
  return tmp;
}


Item_decimal_buff::Item_decimal_buff(Item *it)
  :item(it)
{
  my_decimal_set_zero(&value);
}


bool Item_decimal_buff::cmp()
{
  my_decimal tmp;
  my_decimal *ptmp= item->val_decimal(&tmp);
  if (null_value != item->null_value || my_decimal_cmp(&value, ptmp) == 0)
  {
    null_value= item->null_value;
    my_decimal2decimal(ptmp, &value);
    return TRUE;
  }
  return FALSE;
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item_buff>;
template class List_iterator<Item_buff>;
#endif
