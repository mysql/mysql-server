/* Copyright (c) 2002, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <sys/types.h>

#include "binary_log_types.h"
#include "hash.h"
#include "item.h"            // Item::Type
#include "lex_string.h"
#include "mdl.h"             // MDL_request
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql_com.h"
#include "sp_head.h"         // Stored_program_creation_ctx
#include "sql_admin.h"
#include "sql_alloc.h"
#include "sql_class.h"
#include "sql_lex.h"
#include "sql_plugin.h"
#include "sql_servers.h"

class Object_creation_ctx;

namespace dd {
  class Routine;
  class Schema;
}

class Field;
class Sroutine_hash_entry;
class String;
class sp_cache;
struct TABLE;
struct TABLE_LIST;

typedef struct st_hash HASH;
typedef ulonglong sql_mode_t;
template <typename T> class SQL_I_List;

enum class enum_sp_type;

/* Tells what SP_DEFAULT_ACCESS should be mapped to */
#define SP_DEFAULT_ACCESS_MAPPING SP_CONTAINS_SQL

/* Tells what SP_IS_DEFAULT_SUID should be mapped to */
#define SP_DEFAULT_SUID_MAPPING SP_IS_SUID

/* Max length(LONGBLOB field type length) of stored routine body */
static const uint MYSQL_STORED_ROUTINE_BODY_LENGTH= 4294967295U;

/* Max length(TEXT field type length) of stored routine comment */
static const int MYSQL_STORED_ROUTINE_COMMENT_LENGTH= 65535;

enum enum_sp_return_code
{
  SP_OK= 0,

  // Schema does not exists
  SP_NO_DB_ERROR,

  // Routine does not exists
  SP_DOES_NOT_EXISTS,

  // Routine already exists
  SP_ALREADY_EXISTS,

  // Create routine failed
  SP_STORE_FAILED,

  // Drop routine failed
  SP_DROP_FAILED,

  // Alter routine failed
  SP_ALTER_FAILED,

  // Routine load failed
  SP_LOAD_FAILED,

  // Routine parse failed
  SP_PARSE_ERROR,

  // Internal errors
  SP_INTERNAL_ERROR
};


/*
  Fields in mysql.proc table in 5.7. This enum is used to read and
  update mysql.routines dictionary table during upgrade scenario.

  Note:  This enum should not be used for other purpose
         as it will be removed eventually.
*/
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


/*************************************************************************/

/**
  Stored_routine_creation_ctx -- creation context of stored routines
  (stored procedures and functions).
*/

class Stored_routine_creation_ctx : public Stored_program_creation_ctx,
                                    public Sql_alloc
{
public:
  static Stored_routine_creation_ctx *
  create_routine_creation_ctx(const dd::Routine *routine);

  static Stored_routine_creation_ctx *
  load_from_db(THD *thd, const sp_name *name, TABLE *proc_tbl);
public:
  virtual Stored_program_creation_ctx *clone(MEM_ROOT *mem_root)
  {
    return new (mem_root) Stored_routine_creation_ctx(m_client_cs,
                                                      m_connection_cl,
                                                      m_db_cl);
  }

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const
  {
    DBUG_ENTER("Stored_routine_creation_ctx::create_backup_ctx");
    DBUG_RETURN(new Stored_routine_creation_ctx(thd));
  }

private:
  Stored_routine_creation_ctx(THD *thd)
    : Stored_program_creation_ctx(thd)
  { }

  Stored_routine_creation_ctx(const CHARSET_INFO *client_cs,
                              const CHARSET_INFO *connection_cl,
                              const CHARSET_INFO *db_cl)
    : Stored_program_creation_ctx(client_cs, connection_cl, db_cl)
  { }
};



/* Drop all routines in database 'db' */
enum_sp_return_code sp_drop_db_routines(THD *thd, const dd::Schema &schema);

/**
   Acquires exclusive metadata lock on all stored routines in the
   given database.

   @param  thd     Thread handler
   @param  schema  Schema object

   @retval  false  Success
   @retval  true   Failure
 */
bool lock_db_routines(THD *thd, const dd::Schema &schema);

sp_head *sp_find_routine(THD *thd, enum_sp_type type, sp_name *name,
                         sp_cache **cp, bool cache_only);

sp_head *sp_setup_routine(THD *thd, enum_sp_type type, sp_name *name,
                         sp_cache **cp);

enum_sp_return_code sp_cache_routine(THD *thd, Sroutine_hash_entry *rt,
                                     bool lookup_only, sp_head **sp);

enum_sp_return_code sp_cache_routine(THD *thd, enum_sp_type type, sp_name *name,
                                     bool lookup_only, sp_head **sp);

bool sp_exist_routines(THD *thd, TABLE_LIST *procs, bool is_proc);

bool sp_show_create_routine(THD *thd, enum_sp_type type, sp_name *name);

enum_sp_return_code
db_load_routine(THD *thd, enum_sp_type type, const char *sp_db,
                size_t sp_db_len, const char *sp_name, size_t sp_name_len,
                sp_head **sphp, sql_mode_t sql_mode, const char *params,
                const char *returns, const char *body, st_sp_chistics *chistics,
                const char *definer_user, const char *definer_host,
                longlong created, longlong modified,
                Stored_program_creation_ctx *creation_ctx);

bool sp_create_routine(THD *thd, sp_head *sp, const LEX_USER *definer);

enum_sp_return_code sp_update_routine(THD *thd, enum_sp_type type,
                                      sp_name *name, st_sp_chistics *chistics);

enum_sp_return_code sp_drop_routine(THD *thd, enum_sp_type type, sp_name *name);


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
  int64 m_sp_cache_version;
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

const uchar* sp_sroutine_key(const uchar *ptr, size_t *plen);

sp_head *sp_load_for_information_schema(THD *thd, LEX_CSTRING db_name,
                                        const dd::Routine *routine,
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
                                   const char *db, const char *name);

Item *sp_prepare_func_item(THD* thd, Item **it_addr);

bool sp_eval_expr(THD *thd, Field *result_field, Item **expr_item_ptr);

String *sp_get_item_value(THD *thd, Item *item, String *str);

///////////////////////////////////////////////////////////////////////////

#endif /* _SP_H_ */
