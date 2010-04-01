/* Copyright 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_DELETE_INCLUDED
#define SQL_DELETE_INCLUDED

#include "my_base.h"                            /* ha_rows */

class THD;
class TABLE_LIST;
class Item;

typedef class Item COND;
typedef struct st_sql_list SQL_LIST;

int mysql_prepare_delete(THD *thd, TABLE_LIST *table_list, Item **conds);
bool mysql_delete(THD *thd, TABLE_LIST *table_list, COND *conds,
                  SQL_LIST *order, ha_rows rows, ulonglong options,
                  bool reset_auto_increment);
bool mysql_truncate(THD *thd, TABLE_LIST *table_list, bool dont_send_ok);

#endif /* SQL_DELETE_INCLUDED */
