/*
 * Copyright (c)  2000
 * SWsoft  company
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */
/* Copy data from a textfile to table */

#include "mysql_priv.h"
#include <my_dir.h>
#include <m_ctype.h>


int
mysql_load_internal(THD * thd, sql_exchange * ex, TABLE_LIST * table_list,
List<Item> & fields, enum enum_duplicates handle_duplicates,
bool read_file_from_client, thr_lock_type lock_type);

int
mysql_load(THD * thd, sql_exchange * ex, TABLE_LIST * table_list,
List<Item> & fields, enum enum_duplicates handle_duplicates,
bool read_file_from_client, thr_lock_type lock_type)
{
	printf("SWSOFT_MYSQL load: \n");
  read_file_from_client  = 0; //server is always in the same process 
    return  mysql_load_internal(thd, ex, table_list, fields, handle_duplicates,
 read_file_from_client, lock_type);

}

#define mysql_load mysql_load_internal

#include "../sql/sql_load.cc"
