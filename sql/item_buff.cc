/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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
  Buffers to save and compare item values
*/

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"          // THD
#include "set_var.h"            // Cached_item, Cached_item_field, ...

using std::min;
using std::max;

/**
  Create right type of Cached_item for an item.
*/

Cached_item *new_Cached_item(THD *thd, Item *item, bool use_result_field)
{
  if (item->real_item()->type() == Item::FIELD_ITEM &&
      !(((Item_field *) (item->real_item()))->field->flags & BLOB_FLAG))
  {
    Item_field *real_item= (Item_field *) item->real_item();
    Field *cached_field= use_result_field ? real_item->result_field :
                                            real_item->field;
    return new Cached_item_field(cached_field);
  }
  switch (item->result_type()) {
  case STRING_RESULT:
    if (item->is_temporal())
      return new Cached_item_temporal((Item_field *) item);
    return new Cached_item_str(thd, (Item_field *) item);
  case INT_RESULT:
    return new Cached_item_int((Item_field *) item);
  case REAL_RESULT:
    return new Cached_item_real(item);
  case DECIMAL_RESULT:
    return new Cached_item_decimal(item);
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}

Cached_item::~Cached_item() {}

/**
  Compare with old value and replace value with new value.

  @return
    Return true if values have changed
*/

Cached_item_str::Cached_item_str(THD *thd, Item *arg)
  :item(arg),
   value_max_length(min<uint32>(arg->max_length, thd->variables.max_sort_length)),
   value(value_max_length)
{}

bool Cached_item_str::cmp(void)
{
  String *res;
  bool tmp;

  DBUG_ENTER("Cached_item_str::cmp");
  DBUG_ASSERT(!item->is_temporal());
  if ((res=item->val_str(&tmp_value)))
    res->length(min(res->length(), value_max_length));
  DBUG_PRINT("info", ("old: %s, new: %s",
                      value.c_ptr_safe(), res ? res->c_ptr_safe() : ""));
  if (null_value != item->null_value)
  {
    if ((null_value= item->null_value))
      DBUG_RETURN(TRUE);			// New value was null
    tmp=TRUE;
  }
  else if (null_value)
    DBUG_RETURN(0);				// new and old value was null
  else
    tmp= sortcmp(&value,res,item->collation.collation) != 0;
  if (tmp)
    value.copy(*res);				// Remember for next cmp
  DBUG_RETURN(tmp);
}

Cached_item_str::~Cached_item_str()
{
  item=0;					// Safety
}

bool Cached_item_real::cmp(void)
{
  DBUG_ENTER("Cached_item_real::cmp");
  double nr= item->val_real();
  DBUG_PRINT("info", ("old: %f, new: %f", value, nr));
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}

bool Cached_item_int::cmp(void)
{
  DBUG_ENTER("Cached_item_int::cmp");
  longlong nr=item->val_int();
  DBUG_PRINT("info", ("old: %lld, new: %lld", value, nr));
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


bool Cached_item_temporal::cmp(void)
{
  DBUG_ENTER("Cached_item_temporal::cmp");
  longlong nr= item->val_temporal_by_field_type();
  DBUG_PRINT("info", ("old: %lld, new: %lld", value, nr)); 
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value= nr;
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


bool Cached_item_field::cmp(void)
{
  DBUG_ENTER("Cached_item_field::cmp");
  DBUG_EXECUTE("info", dbug_print(););

  bool different= false;

  if (field->is_null())
  {
    if (!null_value)
    {
      different= true;
      null_value= true;
    }
  }
  else
  {
    if (null_value)
    {
      different= true;
      null_value= false;
      field->get_image(buff, length, field->charset());
    }
    else if (field->cmp(buff))                  // Not a blob: cmp() is OK
    {
      different= true;
      field->get_image(buff, length, field->charset());
    }
  }

  DBUG_RETURN(different);
}


Cached_item_decimal::Cached_item_decimal(Item *it)
  :item(it)
{
  my_decimal_set_zero(&value);
}


bool Cached_item_decimal::cmp()
{
  my_decimal tmp;
  my_decimal *ptmp= item->val_decimal(&tmp);
  if (null_value != item->null_value ||
      (!item->null_value && my_decimal_cmp(&value, ptmp)))
  {
    null_value= item->null_value;
    /* Save only not null values */
    if (!null_value)
    {
      my_decimal2decimal(ptmp, &value);
      return TRUE;
    }
    return FALSE;
  }
  return FALSE;
}
