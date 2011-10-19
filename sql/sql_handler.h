#ifndef SQL_HANDLER_INCLUDED
#define SQL_HANDLER_INCLUDED
/* Copyright (C) 2010 Monty Program Ab
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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "sql_class.h"                 /* enum_ha_read_mode */
#include "my_base.h"                   /* ha_rkey_function, ha_rows */
#include "sql_list.h"                  /* List */

/* Open handlers are stored here */

class SQL_HANDLER {
public:
  TABLE *table;
  List<Item> fields;                            /* Fields, set on open */
  THD *thd;
  LEX_STRING handler_name;
  LEX_STRING db;
  LEX_STRING table_name;
  MEM_ROOT mem_root;
  MYSQL_LOCK *lock;
  MDL_request mdl_request;

  key_part_map keypart_map;
  int keyno;                                    /* Used key */
  uint key_len;
  enum enum_ha_read_modes mode;

  /* This is only used when deleting many handler objects */
  SQL_HANDLER *next;

  Query_arena arena;
  char *base_data;
  SQL_HANDLER(THD *thd_arg) :
    thd(thd_arg), arena(&mem_root, Query_arena::STMT_INITIALIZED)
  { init(); clear_alloc_root(&mem_root); base_data= 0; }
  void init()
  {
    keyno= -1;
    table= 0;
    lock= 0;
    mdl_request.ticket= 0;
  }
  void reset();

  ~SQL_HANDLER();
};

class THD;
struct TABLE_LIST;

bool mysql_ha_open(THD *thd, TABLE_LIST *tables, SQL_HANDLER *reopen);
bool mysql_ha_close(THD *thd, TABLE_LIST *tables);
bool mysql_ha_read(THD *, TABLE_LIST *,enum enum_ha_read_modes,char *,
                   List<Item> *,enum ha_rkey_function,Item *,ha_rows,ha_rows);
void mysql_ha_flush(THD *thd);
void mysql_ha_flush_tables(THD *thd, TABLE_LIST *all_tables);
void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables);
void mysql_ha_cleanup(THD *thd);
void mysql_ha_set_explicit_lock_duration(THD *thd);

SQL_HANDLER *mysql_ha_read_prepare(THD *thd, TABLE_LIST *tables,
                                   enum enum_ha_read_modes mode, char *keyname,
                                   List<Item> *key_expr, Item *cond);
#endif
