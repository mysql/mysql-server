/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef LOCK_INCLUDED
#define LOCK_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "mdl.h"
#include "my_inttypes.h"
#include "sql_hset.h"        // Hash_set

class THD;
// Forward declarations
struct TABLE;

typedef struct st_thr_lock_data THR_LOCK_DATA;

typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;

MYSQL_LOCK *mysql_lock_tables(THD *thd, TABLE **table, size_t count, uint flags);
void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count);
void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table);
void mysql_lock_abort_for_thread(THD *thd, TABLE *table);
MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b);
/* Lock based on name */
bool lock_schema_name(THD *thd, const char *db);

/* Lock based on tablespace name */
bool lock_tablespace_name(THD *thd, const char *tablespace);

// Function generating hash key for Tablespace_hash_set.
const uchar *tablespace_set_get_key(const uchar *record, size_t *length);

// Hash_set to hold set of tablespace names.
typedef Hash_set<char, tablespace_set_get_key> Tablespace_hash_set;

// Lock tablespace names.
bool lock_tablespace_names(
       THD *thd,
       Tablespace_hash_set *tablespace_set,
       ulong lock_wait_timeout);

/* Lock based on stored routine name */
bool lock_object_name(THD *thd, MDL_key::enum_mdl_namespace mdl_type,
                      const char *db, const char *name);

#endif /* LOCK_INCLUDED */
