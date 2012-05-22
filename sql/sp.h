/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _SP_H_
#define _SP_H_

#include "sql_string.h"                         // LEX_STRING
#include "sp_head.h"                            // enum_sp_type


class Field;
class Open_tables_backup;
class Open_tables_state;
class Query_arena;
class Query_tables_list;
class Sroutine_hash_entry;
class THD;
class sp_cache;
struct st_sp_chistics;
struct LEX;
struct TABLE;
struct TABLE_LIST;
typedef struct st_hash HASH;
template <typename T> class SQL_I_List;


/* Tells what SP_DEFAULT_ACCESS should be mapped to */
#define SP_DEFAULT_ACCESS_MAPPING SP_CONTAINS_SQL

// Return codes from sp_create_*, sp_drop_*, and sp_show_*:
#define SP_OK                 0
#define SP_KEY_NOT_FOUND     -1
#define SP_OPEN_TABLE_FAILED -2
#define SP_WRITE_ROW_FAILED  -3
#define SP_DELETE_ROW_FAILED -4
#define SP_GET_FIELD_FAILED  -5
#define SP_PARSE_ERROR       -6
#define SP_INTERNAL_ERROR    -7
#define SP_NO_DB_ERROR       -8
#define SP_BAD_IDENTIFIER    -9
#define SP_BODY_TOO_LONG    -10
#define SP_FLD_STORE_FAILED -11

/* DB storage of Stored PROCEDUREs and FUNCTIONs */
enum
{
  MYSQL_PROC_FIELD_DB = 0,
  MYSQL_PROC_FIELD_NAME,
  MYSQL_PROC_MYSQL_TYPE,
  MYSQL_PROC_FIELD_SPECIFIC_NAME,
  MYSQL_PROC_FIELD_LANGUAGE,
  MYSQL_PROC_FIELD_ACCESS,
  MYSQL_PROC_FIELD_DETERMINISTIC,
  MYSQL_PROC_FIELD_SECURITY_TYPE,
  MYSQL_PROC_FIELD_PARAM_LIST,
  MYSQL_PROC_FIELD_RETURNS,
  MYSQL_PROC_FIELD_BODY,
  MYSQL_PROC_FIELD_DEFINER,
  MYSQL_PROC_FIELD_CREATED,
  MYSQL_PROC_FIELD_MODIFIED,
  MYSQL_PROC_FIELD_SQL_MODE,
  MYSQL_PROC_FIELD_COMMENT,
  MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT,
  MYSQL_PROC_FIELD_COLLATION_CONNECTION,
  MYSQL_PROC_FIELD_DB_COLLATION,
  MYSQL_PROC_FIELD_BODY_UTF8,
  MYSQL_PROC_FIELD_COUNT
};

/* Drop all routines in database 'db' */
int sp_drop_db_routines(THD *thd, char *db);

/**
   Acquires exclusive metadata lock on all stored routines in the
   given database.

   @param  thd  Thread handler
   @param  db   Database name

   @retval  false  Success
   @retval  true   Failure
 */
bool lock_db_routines(THD *thd, char *db);

sp_head *sp_find_routine(THD *thd, enum_sp_type type, sp_name *name,
                         sp_cache **cp, bool cache_only);

int sp_cache_routine(THD *thd, Sroutine_hash_entry *rt,
                     bool lookup_only, sp_head **sp);

int sp_cache_routine(THD *thd, enum_sp_type type, sp_name *name,
                     bool lookup_only, sp_head **sp);

bool sp_exist_routines(THD *thd, TABLE_LIST *procs, bool is_proc);

bool sp_show_create_routine(THD *thd, enum_sp_type type, sp_name *name);

int sp_create_routine(THD *thd, sp_head *sp);

int sp_update_routine(THD *thd, enum_sp_type type, sp_name *name,
                      st_sp_chistics *chistics);

int sp_drop_routine(THD *thd, enum_sp_type type, sp_name *name);


/**
  Structure that represents element in the set of stored routines
  used by statement or routine.
*/

class Sroutine_hash_entry
{
public:
  /**
    Metadata lock request for routine.
    MDL_key in this request is also used as a key for set.
  */
  MDL_request mdl_request;
  /**
    Next element in list linking all routines in set. See also comments
    for LEX::sroutine/sroutine_list and sp_head::m_sroutines.
  */
  Sroutine_hash_entry *next;
  /**
    Uppermost view which directly or indirectly uses this routine.
    0 if routine is not used in view. Note that it also can be 0 if
    statement uses routine both via view and directly.
  */
  TABLE_LIST *belong_to_view;
  /**
    This is for prepared statement validation purposes.
    A statement looks up and pre-loads all its stored functions
    at prepare. Later on, if a function is gone from the cache,
    execute may fail.
    Remember the version of sp_head at prepare to be able to
    invalidate the prepared statement at execute if it
    changes.
  */
  ulong m_sp_cache_version;
};


/*
  Procedures for handling sets of stored routines used by statement or routine.
*/
void sp_add_used_routine(Query_tables_list *prelocking_ctx, Query_arena *arena,
                         sp_name *rt, enum_sp_type rt_type);
bool sp_add_used_routine(Query_tables_list *prelocking_ctx, Query_arena *arena,
                         const MDL_key *key, TABLE_LIST *belong_to_view);
void sp_remove_not_own_routines(Query_tables_list *prelocking_ctx);
void sp_update_stmt_used_routines(THD *thd, Query_tables_list *prelocking_ctx,
                                  HASH *src, TABLE_LIST *belong_to_view);
void sp_update_stmt_used_routines(THD *thd, Query_tables_list *prelocking_ctx,
                                  SQL_I_List<Sroutine_hash_entry> *src,
                                  TABLE_LIST *belong_to_view);

extern "C" uchar* sp_sroutine_key(const uchar *ptr, size_t *plen,
                                  my_bool first);

/*
  Routines which allow open/lock and close mysql.proc table even when
  we already have some tables open and locked.
*/
TABLE *open_proc_table_for_read(THD *thd, Open_tables_backup *backup);

sp_head *sp_load_for_information_schema(THD *thd, TABLE *proc_table, String *db,
                                        String *name, sql_mode_t sql_mode,
                                        enum_sp_type type,
                                        const char *returns, const char *params,
                                        bool *free_sp_head);

bool load_charset(MEM_ROOT *mem_root,
                  Field *field,
                  const CHARSET_INFO *dflt_cs,
                  const CHARSET_INFO **cs);

bool load_collation(MEM_ROOT *mem_root,
                    Field *field,
                    const CHARSET_INFO *dflt_cl,
                    const CHARSET_INFO **cl);

///////////////////////////////////////////////////////////////////////////

sp_head *sp_start_parsing(THD *thd,
                          enum_sp_type sp_type,
                          sp_name *sp_name);

void sp_finish_parsing(THD *thd);

///////////////////////////////////////////////////////////////////////////

Item_result sp_map_result_type(enum enum_field_types type);
Item::Type sp_map_item_type(enum enum_field_types type);
uint sp_get_flags_for_command(LEX *lex);

bool sp_check_name(LEX_STRING *ident);

TABLE_LIST *sp_add_to_query_tables(THD *thd, LEX *lex,
                                   const char *db, const char *name,
                                   thr_lock_type locktype,
                                   enum_mdl_type mdl_type);

bool sp_check_show_access(THD *thd, sp_head *sp, bool *full_access);

Item *sp_prepare_func_item(THD* thd, Item **it_addr);

bool sp_eval_expr(THD *thd, Field *result_field, Item **expr_item_ptr);

String *sp_get_item_value(THD *thd, Item *item, String *str);

///////////////////////////////////////////////////////////////////////////

#endif /* _SP_H_ */
