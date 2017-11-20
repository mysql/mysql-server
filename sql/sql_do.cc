/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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


/* Execute DO statement */

#include "sql/sql_do.h"

#include "m_ctype.h"
#include "my_dbug.h"
#include "sql/item.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_list.h"
#include "sql_string.h"
 

bool Query_result_do::send_data(List<Item> &items)
{
  DBUG_ENTER("Query_result_do::send_data");

  char buffer[MAX_FIELD_WIDTH];
  String str_buffer(buffer, sizeof (buffer), &my_charset_bin);
  List_iterator_fast<Item> it(items);

  // Evaluate all fields, but do not send them
  for (Item *item= it++; item; item= it++)
  {
    if (item->evaluate(thd, &str_buffer))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


bool Query_result_do::send_eof()
{
  /* 
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error())
    return true;
  ::my_ok(thd);
  return false;
}
