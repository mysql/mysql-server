/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_TEST_INCLUDED
#define SQL_TEST_INCLUDED

#include <sys/types.h>

#include "enum_query_type.h"    // enum_query_type
#include "mem_root_array.h"     // Mem_root_array
#include "sql_select.h"
#include "thr_lock.h"           // TL_WRITE_ONLY

class Item;
class JOIN;
class SELECT_LEX;
struct TABLE_LIST;

typedef Mem_root_array<Key_use> Key_use_array;

extern const char *lock_descriptions[TL_WRITE_ONLY + 1];

#ifndef DBUG_OFF
void print_where(Item *cond,const char *info, enum_query_type query_type);
void TEST_join(JOIN *join);
void print_plan(JOIN* join,uint idx, double record_count, double read_time,
                double current_read_time, const char *info);
void dump_TABLE_LIST_graph(SELECT_LEX *select_lex, TABLE_LIST* tl);
#endif
void mysql_print_status();
class Opt_trace_context;

void print_keyuse_array(Opt_trace_context *trace,
                        const Key_use_array *keyuse_array);
#endif /* SQL_TEST_INCLUDED */
