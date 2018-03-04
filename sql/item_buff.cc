/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/**
  @file

  @brief
  Buffers to save and compare item values
*/

#include <stddef.h>
#include <algorithm>

#include "binary_log_types.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/udf_registration_types.h"
#include "sql/current_thd.h"    // current_thd
#include "sql/item.h"           // Cached_item, ...
#include "sql/json_dom.h"       // Json_wrapper
#include "sql/my_decimal.h"
#include "sql/sql_class.h"      // THD
#include "sql/system_variables.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"
#include "template_utils.h"

using std::min;
using std::max;

/**
  Create right type of Cached_item for an item.
*/

Cached_item *new_Cached_item(THD *thd, Item *item)
{
  switch (item->result_type()) {
  case STRING_RESULT:
    if (item->is_temporal())
      return new (*THR_MALLOC) Cached_item_temporal(item);
    if (item->data_type() == MYSQL_TYPE_JSON)
      return new (*THR_MALLOC) Cached_item_json(item);
    return new (*THR_MALLOC) Cached_item_str(thd, item);
  case INT_RESULT:
    return new (*THR_MALLOC) Cached_item_int(item);
  case REAL_RESULT:
    return new (*THR_MALLOC) Cached_item_real(item);
  case DECIMAL_RESULT:
    return new (*THR_MALLOC) Cached_item_decimal(item);
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
  :Cached_item(arg),
   value_max_length(min<uint32>(arg->max_length, thd->variables.max_sort_length)),
   value(value_max_length)
{}

bool Cached_item_str::cmp(void)
{
  String *res;
  bool tmp;

  DBUG_ENTER("Cached_item_str::cmp");
  DBUG_ASSERT(!item->is_temporal());
  DBUG_ASSERT(item->data_type() != MYSQL_TYPE_JSON);
  if ((res=item->val_str(&tmp_value)))
    res->length(min(res->length(), static_cast<size_t>(value_max_length)));
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


Cached_item_json::Cached_item_json(Item *item)
  : Cached_item(item), m_value(new (*THR_MALLOC) Json_wrapper())
{}


Cached_item_json::~Cached_item_json()
{
  delete m_value;
}


/**
  Compare the new JSON value in member 'item' with the previous value.
  @retval true   if the new value is different from the previous value,
                 or if there is no previously cached value
  @retval false  if the new value is the same as the already cached value
*/
bool Cached_item_json::cmp()
{
  Json_wrapper wr;
  if (item->val_json(&wr))
  {
    null_value= true;                         /* purecov: inspected */
    return true;                              /* purecov: inspected */
  }
  if (null_value != item->null_value)
  {
    null_value= item->null_value;
    if (null_value)
      return true;                              // New value is null.
  }
  else if (null_value)
  {
    return false;                               // New and old are null.
  }
  else if (!m_value->empty() && m_value->compare(wr) == 0)
  {
    return false;                               // New and old are equal.
  }

  /*
    Otherwise, old and new are not equal, and new is not null.
    Remember the current value till the next time we're called.
  */
  *m_value= std::move(wr);

  /*
    The row buffer may change, which would garble the JSON binary
    representation pointed to by m_value. Convert to DOM so that we
    own the copy.
  */
  m_value->to_dom(current_thd);

  return true;
}

void Cached_item_json::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_json*>(i_c)->store_value(item, m_value);
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
  DBUG_PRINT("info", ("old: 0x%.16llx, new: 0x%.16llx", (ulonglong)value, (ulonglong)nr));

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


Cached_item_decimal::Cached_item_decimal(Item *it)
  :Cached_item(it)
{
  my_decimal_set_zero(&value);
}


void Cached_item_real::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_real*>(i_c)->store_value(item, value);
}


void Cached_item_int::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_int*>(i_c)->store_value(item, value);
}


void Cached_item_temporal::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_datetime*>(i_c)->store_value(item, value);
}


void Cached_item_str::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_str*>(i_c)->store_value(item, value);
}

void Cached_item_decimal::copy_to_Item_cache(Item_cache *i_c)
{
  down_cast<Item_cache_decimal*>(i_c)->store_value(item, &value);
}


bool Cached_item_decimal::cmp()
{
  my_decimal tmp;
  my_decimal *ptmp= item->val_decimal(&tmp);
  DBUG_ENTER("Cached_item_decimal::cmp");
  /*
    NULL handling is wrong here, see Bug#25407964 GROUP BY DESC GIVES WRONG
    RESULT WHEN GROUPS ON DECIMAL AND SEES A NULL.
  */
  if (null_value != item->null_value ||
      (!item->null_value && my_decimal_cmp(&value, ptmp)))
  {
    null_value= item->null_value;
    /* Save only not null values */
    if (!null_value)
      my_decimal2decimal(ptmp, &value);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}
