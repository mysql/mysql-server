/*
   Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql_priv.h"
#include "unireg.h"
#include "sql_base.h"                           // close_thread_tables
#include "sql_parse.h"                          // parse_sql
#include "key.h"                                // key_copy
#include "sql_show.h"             // append_definer, append_identifier
#include "sql_db.h" // get_default_db_collation, mysql_opt_change_db,
                    // mysql_change_db, check_db_dir_existence,
                    // load_db_opt_by_name
#include "sql_table.h"                          // write_bin_log
#include "sql_acl.h"                       // SUPER_ACL
#include "sp_head.h"
#include "sp_cache.h"
#include "lock.h"                               // lock_object_name
#include "sp.h"

#include <my_user.h>

/* Used in error handling only */
#define SP_TYPE_STRING(LP) \
    ((LP)->sphead->m_type == SP_TYPE_FUNCTION ? "FUNCTION" : "PROCEDURE")
static bool
create_string(THD *thd, String *buf,
	      enum_sp_type sp_type,
	      const char *db, ulong dblen,
	      const char *name, ulong namelen,
	      const char *params, ulong paramslen,
	      const char *returns, ulong returnslen,
	      const char *body, ulong bodylen,
	      st_sp_chistics *chistics,
              const LEX_STRING *definer_user,
              const LEX_STRING *definer_host,
              sql_mode_t sql_mode);

static int
db_load_routine(THD *thd, enum_sp_type type, sp_name *name, sp_head **sphp,
                sql_mode_t sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified,
                Stored_program_creation_ctx *creation_ctx);

static const
TABLE_FIELD_TYPE proc_table_fields[MYSQL_PROC_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("specific_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("language") },
    { C_STRING_WITH_LEN("enum('SQL')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_data_access") },
    { C_STRING_WITH_LEN("enum('CONTAINS_SQL','NO_SQL','READS_SQL_DATA','MODIFIES_SQL_DATA')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("is_deterministic") },
    { C_STRING_WITH_LEN("enum('YES','NO')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("security_type") },
    { C_STRING_WITH_LEN("enum('INVOKER','DEFINER')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("param_list") },
    { C_STRING_WITH_LEN("blob") },
    { NULL, 0 }
  },

  {
    { C_STRING_WITH_LEN("returns") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(77)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  proc_table_def= {MYSQL_PROC_FIELD_COUNT, proc_table_fields};

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

/**************************************************************************
  Stored_routine_creation_ctx implementation.
**************************************************************************/

bool load_charset(MEM_ROOT *mem_root,
                  Field *field,
                  const CHARSET_INFO *dflt_cs,
                  const CHARSET_INFO **cs)
{
  String cs_name;

  if (get_field(mem_root, field, &cs_name))
  {
    *cs= dflt_cs;
    return TRUE;
  }

  *cs= get_charset_by_csname(cs_name.c_ptr(), MY_CS_PRIMARY, MYF(0));

  if (*cs == NULL)
  {
    *cs= dflt_cs;
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************/

bool load_collation(MEM_ROOT *mem_root,
                    Field *field,
                    const CHARSET_INFO *dflt_cl,
                    const CHARSET_INFO **cl)
{
  String cl_name;

  if (get_field(mem_root, field, &cl_name))
  {
    *cl= dflt_cl;
    return TRUE;
  }

  *cl= get_charset_by_name(cl_name.c_ptr(), MYF(0));

  if (*cl == NULL)
  {
    *cl= dflt_cl;
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************/

Stored_routine_creation_ctx *
Stored_routine_creation_ctx::load_from_db(THD *thd,
                                         const sp_name *name,
                                         TABLE *proc_tbl)
{
  /* Load character set/collation attributes. */

  const CHARSET_INFO *client_cs;
  const CHARSET_INFO *connection_cl;
  const CHARSET_INFO *db_cl;

  const char *db_name= thd->strmake(name->m_db.str, name->m_db.length);
  const char *sr_name= thd->strmake(name->m_name.str, name->m_name.length);

  bool invalid_creation_ctx= FALSE;

  if (load_charset(thd->mem_root,
                   proc_tbl->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT],
                   thd->variables.character_set_client,
                   &client_cs))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.character_set_client.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (load_collation(thd->mem_root,
                     proc_tbl->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION],
                     thd->variables.collation_connection,
                     &connection_cl))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.collation_connection.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (load_collation(thd->mem_root,
                     proc_tbl->field[MYSQL_PROC_FIELD_DB_COLLATION],
                     NULL,
                     &db_cl))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.db_collation.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (invalid_creation_ctx)
  {
    push_warning_printf(thd,
                        Sql_condition::SL_WARNING,
                        ER_SR_INVALID_CREATION_CTX,
                        ER(ER_SR_INVALID_CREATION_CTX),
                        (const char *) db_name,
                        (const char *) sr_name);
  }

  /*
    If we failed to retrieve the database collation, load the default one
    from the disk.
  */

  if (!db_cl)
    db_cl= get_default_db_collation(thd, name->m_db.str);

  /* Create the context. */

  return new Stored_routine_creation_ctx(client_cs, connection_cl, db_cl);
}

/*************************************************************************/

class Proc_table_intact : public Table_check_intact
{
private:
  bool m_print_once;

public:
  Proc_table_intact() : m_print_once(TRUE) {}

protected:
  void report_error(uint code, const char *fmt, ...);
};


/**
  Report failure to validate the mysql.proc table definition.
  Print a message to the error log only once.
*/

void Proc_table_intact::report_error(uint code, const char *fmt, ...)
{
  va_list args;
  char buf[512];

  va_start(args, fmt);
  my_vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (code)
    my_message(code, buf, MYF(0));
  else
    my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "proc");

  if (m_print_once)
  {
    m_print_once= FALSE;
    sql_print_error("%s", buf);
  }
};


/** Single instance used to control printing to the error log. */
static Proc_table_intact proc_table_intact;


/**
  Open the mysql.proc table for read.

  @param thd     Thread context
  @param backup  Pointer to Open_tables_state instance where information about
                 currently open tables will be saved, and from which will be
                 restored when we will end work with mysql.proc.

  @retval
    0	Error
  @retval
    \#	Pointer to TABLE object of mysql.proc
*/

TABLE *open_proc_table_for_read(THD *thd, Open_tables_backup *backup)
{
  TABLE_LIST table;

  DBUG_ENTER("open_proc_table_for_read");

  table.init_one_table("mysql", 5, "proc", 4, "proc", TL_READ);

  if (open_system_tables_for_read(thd, &table, backup))
    DBUG_RETURN(NULL);

  if (!proc_table_intact.check(table.table, &proc_table_def))
    DBUG_RETURN(table.table);

  close_system_tables(thd, backup);

  DBUG_RETURN(NULL);
}


/**
  Open the mysql.proc table for update.

  @param thd  Thread context

  @note
    Table opened with this call should closed using close_thread_tables().

  @retval
    0	Error
  @retval
    \#	Pointer to TABLE object of mysql.proc
*/

static TABLE *open_proc_table_for_update(THD *thd)
{
  TABLE_LIST table_list;
  TABLE *table;
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  DBUG_ENTER("open_proc_table_for_update");

  table_list.init_one_table("mysql", 5, "proc", 4, "proc", TL_WRITE);

  if (!(table= open_system_table_for_update(thd, &table_list)))
    DBUG_RETURN(NULL);

  if (!proc_table_intact.check(table, &proc_table_def))
    DBUG_RETURN(table);

  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  DBUG_RETURN(NULL);
}


/**
  Return appropriate error about recursion limit reaching

  @param thd  Thread handle

  @remark For functions and triggers we return error about
          prohibited recursion. For stored procedures we
          return about reaching recursion limit.
*/

static void recursion_level_error(THD *thd, sp_head *sp)
{
  if (sp->m_type == SP_TYPE_PROCEDURE)
  {
    my_error(ER_SP_RECURSION_LIMIT, MYF(0),
             static_cast<int>(thd->variables.max_sp_recursion_depth),
             sp->m_name.str);
  }
  else
    my_error(ER_SP_NO_RECURSION, MYF(0));
}


/**
  Find row in open mysql.proc table representing stored routine.

  @param thd    Thread context
  @param type   Type of routine to find (function or procedure)
  @param name   Name of routine
  @param table  TABLE object for open mysql.proc table.

  @retval
    SP_OK             Routine found
  @retval
    SP_KEY_NOT_FOUND  No routine with given name
*/

static int
db_find_routine_aux(THD *thd, enum_sp_type type, sp_name *name, TABLE *table)
{
  uchar key[MAX_KEY_LENGTH];	// db, name, optional key length type
  DBUG_ENTER("db_find_routine_aux");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the three first fields in the table are
    'db', 'name' and 'type' and the first key is the primary key over the
    same fields.
  */
  if (name->m_name.length > table->field[1]->field_length)
    DBUG_RETURN(SP_KEY_NOT_FOUND);
  table->field[0]->store(name->m_db.str, name->m_db.length, &my_charset_bin);
  table->field[1]->store(name->m_name.str, name->m_name.length,
                         &my_charset_bin);
  table->field[2]->store((longlong) type, TRUE);
  key_copy(key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->ha_index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                         HA_READ_KEY_EXACT))
    DBUG_RETURN(SP_KEY_NOT_FOUND);

  DBUG_RETURN(SP_OK);
}


/**
  Find routine definition in mysql.proc table and create corresponding
  sp_head object for it.

  @param thd   Thread context
  @param type  Type of routine (SP_TYPE_PROCEDURE/...)
  @param name  Name of routine
  @param sphp  Out parameter in which pointer to created sp_head
               object is returned (0 in case of error).

  @note
    This function may damage current LEX during execution, so it is good
    idea to create temporary LEX and make it active before calling it.

  @retval
    0       Success
  @retval
    non-0   Error (may be one of special codes like SP_KEY_NOT_FOUND)
*/

static int
db_find_routine(THD *thd, enum_sp_type type, sp_name *name, sp_head **sphp)
{
  TABLE *table;
  const char *params, *returns, *body;
  int ret;
  const char *definer;
  longlong created;
  longlong modified;
  st_sp_chistics chistics;
  char *ptr;
  uint length;
  char buff[65];
  String str(buff, sizeof(buff), &my_charset_bin);
  bool saved_time_zone_used= thd->time_zone_used;
  sql_mode_t sql_mode, saved_mode= thd->variables.sql_mode;
  Open_tables_backup open_tables_state_backup;
  Stored_program_creation_ctx *creation_ctx;

  DBUG_ENTER("db_find_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  *sphp= 0;                                     // In case of errors
  if (!(table= open_proc_table_for_read(thd, &open_tables_state_backup)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  /* Reset sql_mode during data dictionary operations. */
  thd->variables.sql_mode= 0;

  if ((ret= db_find_routine_aux(thd, type, name, table)) != SP_OK)
    goto done;

  if (table->s->fields < MYSQL_PROC_FIELD_COUNT)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  memset(&chistics, 0, sizeof(chistics));
  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_ACCESS])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  switch (ptr[0]) {
  case 'N':
    chistics.daccess= SP_NO_SQL;
    break;
  case 'C':
    chistics.daccess= SP_CONTAINS_SQL;
    break;
  case 'R':
    chistics.daccess= SP_READS_SQL_DATA;
    break;
  case 'M':
    chistics.daccess= SP_MODIFIES_SQL_DATA;
    break;
  default:
    chistics.daccess= SP_DEFAULT_ACCESS_MAPPING;
  }

  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_DETERMINISTIC])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  chistics.detistic= (ptr[0] == 'N' ? FALSE : TRUE);    

  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  chistics.suid= (ptr[0] == 'I' ? SP_IS_NOT_SUID : SP_IS_SUID);

  if ((params= get_field(thd->mem_root,
			 table->field[MYSQL_PROC_FIELD_PARAM_LIST])) == NULL)
  {
    params= "";
  }

  if (type == SP_TYPE_PROCEDURE)
    returns= "";
  else if ((returns= get_field(thd->mem_root,
			       table->field[MYSQL_PROC_FIELD_RETURNS])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  if ((body= get_field(thd->mem_root,
		       table->field[MYSQL_PROC_FIELD_BODY])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  // Get additional information
  if ((definer= get_field(thd->mem_root,
			  table->field[MYSQL_PROC_FIELD_DEFINER])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  modified= table->field[MYSQL_PROC_FIELD_MODIFIED]->val_int();
  created= table->field[MYSQL_PROC_FIELD_CREATED]->val_int();

  sql_mode= (sql_mode_t) table->field[MYSQL_PROC_FIELD_SQL_MODE]->val_int();

  table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= thd->strmake(str.ptr(), length);
  chistics.comment.str= ptr;
  chistics.comment.length= length;

  creation_ctx= Stored_routine_creation_ctx::load_from_db(thd, name, table);

  close_system_tables(thd, &open_tables_state_backup);
  table= 0;

  ret= db_load_routine(thd, type, name, sphp,
                       sql_mode, params, returns, body, chistics,
                       definer, created, modified, creation_ctx);
 done:
  /* 
    Restore the time zone flag as the timezone usage in proc table
    does not affect replication.
  */  
  thd->time_zone_used= saved_time_zone_used;
  if (table)
    close_system_tables(thd, &open_tables_state_backup);
  thd->variables.sql_mode= saved_mode;
  DBUG_RETURN(ret);
}


/**
  Silence DEPRECATED SYNTAX warnings when loading a stored procedure
  into the cache.
*/
struct Silence_deprecated_warning : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level level,
                                const char* msg,
                                Sql_condition ** cond_hdl);
};

bool
Silence_deprecated_warning::handle_condition(
  THD *,
  uint sql_errno,
  const char*,
  Sql_condition::enum_severity_level level,
  const char*,
  Sql_condition ** cond_hdl)
{
  *cond_hdl= NULL;
  if (sql_errno == ER_WARN_DEPRECATED_SYNTAX &&
      level == Sql_condition::SL_WARNING)
    return TRUE;

  return FALSE;
}


/**
  @brief    The function parses input strings and returns SP stucture.

  @param[in]      thd               Thread handler
  @param[in]      defstr            CREATE... string
  @param[in]      sql_mode          SQL mode
  @param[in]      creation_ctx      Creation context of stored routines
                                    
  @return     Pointer on sp_head struct
    @retval   #                     Pointer on sp_head struct
    @retval   0                     error
*/

static sp_head *sp_compile(THD *thd, String *defstr, sql_mode_t sql_mode,
                           Stored_program_creation_ctx *creation_ctx)
{
  sp_head *sp;
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  ha_rows old_select_limit= thd->variables.select_limit;
  sp_rcontext *sp_runtime_ctx_saved= thd->sp_runtime_ctx;
  Silence_deprecated_warning warning_handler;
  Parser_state parser_state;
  PSI_statement_locker *parent_locker= thd->m_statement_psi;

  thd->variables.sql_mode= sql_mode;
  thd->variables.select_limit= HA_POS_ERROR;

  if (parser_state.init(thd, defstr->c_ptr(), defstr->length()))
  {
    thd->variables.sql_mode= old_sql_mode;
    thd->variables.select_limit= old_select_limit;
    return NULL;
  }

  lex_start(thd);
  thd->push_internal_handler(&warning_handler);
  thd->sp_runtime_ctx= NULL;

  thd->m_statement_psi= NULL;
  if (parse_sql(thd, & parser_state, creation_ctx) || thd->lex == NULL)
  {
    sp= thd->lex->sphead;
    delete sp;
    sp= 0;
  }
  else
  {
    sp= thd->lex->sphead;
  }
  thd->m_statement_psi= parent_locker;

  thd->pop_internal_handler();
  thd->sp_runtime_ctx= sp_runtime_ctx_saved;
  thd->variables.sql_mode= old_sql_mode;
  thd->variables.select_limit= old_select_limit;
  return sp;
}


class Bad_db_error_handler : public Internal_error_handler
{
public:
  Bad_db_error_handler()
    :m_error_caught(false)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level level,
                                const char* message,
                                Sql_condition ** cond_hdl);

  bool error_caught() const { return m_error_caught; }

private:
  bool m_error_caught;
};

bool
Bad_db_error_handler::handle_condition(THD *thd,
                                       uint sql_errno,
                                       const char* sqlstate,
                                       Sql_condition::enum_severity_level level,
                                       const char* message,
                                       Sql_condition ** cond_hdl)
{
  if (sql_errno == ER_BAD_DB_ERROR)
  {
    m_error_caught= true;
    return true;
  }
  return false;
}


static int
db_load_routine(THD *thd, enum_sp_type type, sp_name *name, sp_head **sphp,
                sql_mode_t sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified,
                Stored_program_creation_ctx *creation_ctx)
{
  LEX *old_lex= thd->lex, newlex;
  String defstr;
  char saved_cur_db_name_buf[NAME_LEN+1];
  LEX_STRING saved_cur_db_name=
    { saved_cur_db_name_buf, sizeof(saved_cur_db_name_buf) };
  bool cur_db_changed;
  Bad_db_error_handler db_not_exists_handler;
  char definer_user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING definer_user_name= { definer_user_name_holder,
                                  USERNAME_LENGTH };

  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING definer_host_name= { definer_host_name_holder, HOSTNAME_LENGTH };

  int ret= 0;

  thd->lex= &newlex;
  newlex.current_select= NULL;

  parse_user(definer, strlen(definer),
             definer_user_name.str, &definer_user_name.length,
             definer_host_name.str, &definer_host_name.length);

  defstr.set_charset(creation_ctx->get_client_cs());

  /*
    We have to add DEFINER clause and provide proper routine characterstics in
    routine definition statement that we build here to be able to use this
    definition for SHOW CREATE PROCEDURE later.
   */

  if (!create_string(thd, &defstr,
                     type,
                     NULL, 0,
                     name->m_name.str, name->m_name.length,
                     params, strlen(params),
                     returns, strlen(returns),
                     body, strlen(body),
                     &chistics, &definer_user_name, &definer_host_name,
                     sql_mode))
  {
    ret= SP_INTERNAL_ERROR;
    goto end;
  }

  thd->push_internal_handler(&db_not_exists_handler);
  /*
    Change the current database (if needed).

    TODO: why do we force switch here?
  */

  if (mysql_opt_change_db(thd, &name->m_db, &saved_cur_db_name, TRUE,
                          &cur_db_changed))
  {
    ret= SP_INTERNAL_ERROR;
    thd->pop_internal_handler();
    goto end;
  }
  thd->pop_internal_handler();
  if (db_not_exists_handler.error_caught())
  {
    ret= SP_INTERNAL_ERROR;
    my_error(ER_BAD_DB_ERROR, MYF(0), name->m_db.str);

    goto end;
  }

  {
    *sphp= sp_compile(thd, &defstr, sql_mode, creation_ctx);
    /*
      Force switching back to the saved current database (if changed),
      because it may be NULL. In this case, mysql_change_db() would
      generate an error.
    */

    if (cur_db_changed && mysql_change_db(thd, &saved_cur_db_name, TRUE))
    {
      ret= SP_INTERNAL_ERROR;
      goto end;
    }

    if (!*sphp)
    {
      ret= SP_PARSE_ERROR;
      goto end;
    }

    (*sphp)->set_definer(&definer_user_name, &definer_host_name);
    (*sphp)->set_info(created, modified, &chistics, sql_mode);
    (*sphp)->set_creation_ctx(creation_ctx);
    (*sphp)->optimize();
    /*
      Not strictly necessary to invoke this method here, since we know
      that we've parsed CREATE PROCEDURE/FUNCTION and not an
      UPDATE/DELETE/INSERT/REPLACE/LOAD/CREATE TABLE, but we try to
      maintain the invariant that this method is called for each
      distinct statement, in case its logic is extended with other
      types of analyses in future.
    */
    newlex.set_trg_event_type_for_tables();
  }

end:
  thd->lex->sphead= NULL;
  lex_end(thd->lex);
  thd->lex= old_lex;
  return ret;
}


static void
sp_returns_type(THD *thd, String &result, sp_head *sp)
{
  TABLE table;
  TABLE_SHARE share;
  Field *field;
  memset(&table, 0, sizeof(table));
  memset(&share, 0, sizeof(share));
  table.in_use= thd;
  table.s = &share;
  field= sp->create_result_field(0, 0, &table);
  field->sql_type(result);

  if (field->has_charset())
  {
    result.append(STRING_WITH_LEN(" CHARSET "));
    result.append(field->charset()->csname);
    if (!(field->charset()->state & MY_CS_PRIMARY))
    {
      result.append(STRING_WITH_LEN(" COLLATE "));
      result.append(field->charset()->name);
    }
  }

  delete field;
}


/**
  Write stored-routine object into mysql.proc.

  This operation stores attributes of the stored procedure/function into
  the mysql.proc.

  @param thd  Thread context.
  @param sp   Stored routine object to store.

  @note Opens and closes the thread tables. Therefore assumes
  that there are no locked tables in this thread at the time of
  invocation.
  Unlike some other DDL statements, *does* close the tables
  in the end, since the call to this function is normally
  followed by an implicit grant (sp_grant_privileges())
  and this subsequent call opens and closes mysql.procs_priv.

  @retval false success
  @retval true  error
*/

bool sp_create_routine(THD *thd, sp_head *sp)
{
  bool error= true;
  TABLE *table;
  char definer[USER_HOST_BUFF_SIZE];
  sql_mode_t saved_mode= thd->variables.sql_mode;
  MDL_key::enum_mdl_namespace mdl_type= (sp->m_type == SP_TYPE_FUNCTION) ?
                                        MDL_key::FUNCTION : MDL_key::PROCEDURE;

  const CHARSET_INFO *db_cs= get_default_db_collation(thd, sp->m_db.str);

  enum_check_fields saved_count_cuted_fields;

  bool store_failed= FALSE;

  bool save_binlog_row_based;

  DBUG_ENTER("sp_create_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",sp->m_type,
                       (int) sp->m_name.length, sp->m_name.str));
  String retstr(64);
  retstr.set_charset(system_charset_info);

  DBUG_ASSERT(sp->m_type == SP_TYPE_PROCEDURE ||
              sp->m_type == SP_TYPE_FUNCTION);

  /* Grab an exclusive MDL lock. */
  if (lock_object_name(thd, mdl_type, sp->m_db.str, sp->m_name.str))
  {
    my_error(ER_SP_STORE_FAILED, MYF(0),
             SP_TYPE_STRING(thd->lex),sp->m_name.str);
    DBUG_RETURN(true);
  }

  /*
   Check that a database directory with this name
   exists. Design note: This won't work on virtual databases
   like information_schema.
  */
  if (check_db_dir_existence(sp->m_db.str))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), sp->m_db.str);
    DBUG_RETURN(true);
  }

  /* Reset sql_mode during data dictionary operations. */
  thd->variables.sql_mode= 0;

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  saved_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_WARN;

  if (!(table= open_proc_table_for_update(thd)))
  {
    my_error(ER_SP_STORE_FAILED, MYF(0),
             SP_TYPE_STRING(thd->lex),sp->m_name.str);
  }
  else
  {
    restore_record(table, s->default_values); // Get default values for fields

    /* NOTE: all needed privilege checks have been already done. */
    strxnmov(definer, sizeof(definer)-1, thd->lex->definer->user.str, "@",
            thd->lex->definer->host.str, NullS);

    if (table->s->fields < MYSQL_PROC_FIELD_COUNT)
    {
      my_error(ER_SP_STORE_FAILED, MYF(0),
               SP_TYPE_STRING(thd->lex),sp->m_name.str);
      goto done;
    }

    if (system_charset_info->cset->numchars(system_charset_info,
                                            sp->m_name.str,
                                            sp->m_name.str+sp->m_name.length) >
        table->field[MYSQL_PROC_FIELD_NAME]->char_length())
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), sp->m_name.str);
      goto done;
    }
    if (sp->m_body.length > table->field[MYSQL_PROC_FIELD_BODY]->field_length)
    {
      my_error(ER_TOO_LONG_BODY, MYF(0), sp->m_name.str);
      goto done;
    }

    store_failed=
      table->field[MYSQL_PROC_FIELD_DB]->
        store(sp->m_db.str, sp->m_db.length, system_charset_info);

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_NAME]->
        store(sp->m_name.str, sp->m_name.length, system_charset_info);

    store_failed= store_failed ||
      table->field[MYSQL_PROC_MYSQL_TYPE]->
        store((longlong)sp->m_type, TRUE);

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_SPECIFIC_NAME]->
        store(sp->m_name.str, sp->m_name.length, system_charset_info);

    if (sp->m_chistics->daccess != SP_DEFAULT_ACCESS)
    {
      store_failed= store_failed ||
        table->field[MYSQL_PROC_FIELD_ACCESS]->
          store((longlong)sp->m_chistics->daccess, TRUE);
    }

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_DETERMINISTIC]->
        store((longlong)(sp->m_chistics->detistic ? 1 : 2), TRUE);

    if (sp->m_chistics->suid != SP_IS_DEFAULT_SUID)
    {
      store_failed= store_failed ||
        table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]->
          store((longlong)sp->m_chistics->suid, TRUE);
    }

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_PARAM_LIST]->
        store(sp->m_params.str, sp->m_params.length, system_charset_info);

    if (sp->m_type == SP_TYPE_FUNCTION)
    {
      sp_returns_type(thd, retstr, sp);

      store_failed= store_failed ||
        table->field[MYSQL_PROC_FIELD_RETURNS]->
          store(retstr.ptr(), retstr.length(), system_charset_info);
    }

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_BODY]->
        store(sp->m_body.str, sp->m_body.length, system_charset_info);

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_DEFINER]->
        store(definer, (uint)strlen(definer), system_charset_info);

    Item_func_now_local::store_in(table->field[MYSQL_PROC_FIELD_CREATED]);
    Item_func_now_local::store_in(table->field[MYSQL_PROC_FIELD_MODIFIED]);

    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_SQL_MODE]->
        store((longlong)saved_mode, TRUE);

    if (sp->m_chistics->comment.str)
    {
      store_failed= store_failed ||
        table->field[MYSQL_PROC_FIELD_COMMENT]->
          store(sp->m_chistics->comment.str, sp->m_chistics->comment.length,
                system_charset_info);
    }

    if ((sp->m_type == SP_TYPE_FUNCTION) &&
        !trust_function_creators && mysql_bin_log.is_open())
    {
      if (!sp->m_chistics->detistic)
      {
	/*
	  Note that this test is not perfect; one could use
	  a non-deterministic read-only function in an update statement.
	*/
	enum enum_sp_data_access access=
	  (sp->m_chistics->daccess == SP_DEFAULT_ACCESS) ?
	  SP_DEFAULT_ACCESS_MAPPING : sp->m_chistics->daccess;
	if (access == SP_CONTAINS_SQL ||
	    access == SP_MODIFIES_SQL_DATA)
	{
          my_error(ER_BINLOG_UNSAFE_ROUTINE, MYF(0));
	  goto done;
	}
      }
      if (!(thd->security_ctx->master_access & SUPER_ACL))
      {
        my_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER,MYF(0));
	goto done;
      }
    }

    table->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT]->set_notnull();
    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT]->store(
        thd->charset()->csname,
        strlen(thd->charset()->csname),
        system_charset_info);

    table->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION]->set_notnull();
    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION]->store(
        thd->variables.collation_connection->name,
        strlen(thd->variables.collation_connection->name),
        system_charset_info);

    table->field[MYSQL_PROC_FIELD_DB_COLLATION]->set_notnull();
    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_DB_COLLATION]->store(
        db_cs->name, strlen(db_cs->name), system_charset_info);

    table->field[MYSQL_PROC_FIELD_BODY_UTF8]->set_notnull();
    store_failed= store_failed ||
      table->field[MYSQL_PROC_FIELD_BODY_UTF8]->store(
        sp->m_body_utf8.str, sp->m_body_utf8.length, system_charset_info);

    if (store_failed)
    {
      my_error(ER_CANT_CREATE_SROUTINE, MYF(0), sp->m_name.str);
      goto done;
    }

    if (table->file->ha_write_row(table->record[0]))
    {
       my_error(ER_SP_ALREADY_EXISTS, MYF(0),
                SP_TYPE_STRING(thd->lex), sp->m_name.str);
       goto done;
    }

    sp_cache_invalidate();

    error= false;
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();

      String log_query;
      log_query.set_charset(system_charset_info);

      if (!create_string(thd, &log_query,
                         sp->m_type,
                         (sp->m_explicit_name ? sp->m_db.str : NULL), 
                         (sp->m_explicit_name ? sp->m_db.length : 0), 
                         sp->m_name.str, sp->m_name.length,
                         sp->m_params.str, sp->m_params.length,
                         retstr.c_ptr(), retstr.length(),
                         sp->m_body.str, sp->m_body.length,
                         sp->m_chistics, &(thd->lex->definer->user),
                         &(thd->lex->definer->host),
                         saved_mode))
      {
        my_error(ER_SP_STORE_FAILED, MYF(0),
                 SP_TYPE_STRING(thd->lex), sp->m_name.str);
        error= true;
        goto done;
      }
      /* restore sql_mode when binloging */
      thd->variables.sql_mode= saved_mode;
      thd->add_to_binlog_accessed_dbs(sp->m_db.str);
      /* Such a statement can always go directly to binlog, no trans cache */
      if (thd->binlog_query(THD::STMT_QUERY_TYPE,
                            log_query.c_ptr(), log_query.length(),
                            FALSE, FALSE, FALSE, 0))
      {
        my_error(ER_SP_STORE_FAILED, MYF(0),
                 SP_TYPE_STRING(thd->lex), sp->m_name.str);
        error= true;
      };
      thd->variables.sql_mode= 0;
    }
  }

done:
  thd->count_cuted_fields= saved_count_cuted_fields;
  thd->variables.sql_mode= saved_mode;
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(error);
}


/**
  Delete the record for the stored routine object from mysql.proc.

  The operation deletes the record for the stored routine specified by name
  from the mysql.proc table and invalidates the stored-routine cache.

  @param thd  Thread context.
  @param type Stored routine type
              (SP_TYPE_PROCEDURE or SP_TYPE_FUNCTION)
  @param name Stored routine name.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int sp_drop_routine(THD *thd, enum_sp_type type, sp_name *name)
{
  TABLE *table;
  int ret;
  bool save_binlog_row_based;
  MDL_key::enum_mdl_namespace mdl_type= (type == SP_TYPE_FUNCTION) ?
                                        MDL_key::FUNCTION : MDL_key::PROCEDURE;
  DBUG_ENTER("sp_drop_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  DBUG_ASSERT(type == SP_TYPE_PROCEDURE || type == SP_TYPE_FUNCTION);

  /* Grab an exclusive MDL lock. */
  if (lock_object_name(thd, mdl_type, name->m_db.str, name->m_name.str))
    DBUG_RETURN(SP_DELETE_ROW_FAILED);

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    if (table->file->ha_delete_row(table->record[0]))
      ret= SP_DELETE_ROW_FAILED;
  }

  if (ret == SP_OK)
  {
    thd->add_to_binlog_accessed_dbs(name->m_db.str);
    if (write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
      ret= SP_INTERNAL_ERROR;
    sp_cache_invalidate();

    /*
      A lame workaround for lack of cache flush:
      make sure the routine is at least gone from the
      local cache.
    */
    {
      sp_head *sp;
      sp_cache **spc= (type == SP_TYPE_FUNCTION ?
                      &thd->sp_func_cache : &thd->sp_proc_cache);
      sp= sp_cache_lookup(spc, name);
      if (sp)
        sp_cache_flush_obsolete(spc, &sp);
    }
  }
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(ret);
}


/**
  Find and updated the record for the stored routine object in mysql.proc.

  The operation finds the record for the stored routine specified by name
  in the mysql.proc table and updates it with new attributes. After
  successful update, the cache is invalidated.

  @param thd      Thread context.
  @param type     Stored routine type
                  (SP_TYPE_PROCEDURE or SP_TYPE_FUNCTION)
  @param name     Stored routine name.
  @param chistics New values of stored routine attributes to write.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int sp_update_routine(THD *thd, enum_sp_type type, sp_name *name,
                      st_sp_chistics *chistics)
{
  TABLE *table;
  int ret;
  bool save_binlog_row_based;
  MDL_key::enum_mdl_namespace mdl_type= (type == SP_TYPE_FUNCTION) ?
                                        MDL_key::FUNCTION : MDL_key::PROCEDURE;
  DBUG_ENTER("sp_update_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  DBUG_ASSERT(type == SP_TYPE_PROCEDURE || type == SP_TYPE_FUNCTION);

  /* Grab an exclusive MDL lock. */
  if (lock_object_name(thd, mdl_type, name->m_db.str, name->m_name.str))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication. The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    if (type == SP_TYPE_FUNCTION && ! trust_function_creators &&
        mysql_bin_log.is_open() &&
        (chistics->daccess == SP_CONTAINS_SQL ||
         chistics->daccess == SP_MODIFIES_SQL_DATA))
    {
      char *ptr;
      bool is_deterministic;
      ptr= get_field(thd->mem_root,
                     table->field[MYSQL_PROC_FIELD_DETERMINISTIC]);
      if (ptr == NULL)
      {
        ret= SP_INTERNAL_ERROR;
        goto err;
      }
      is_deterministic= ptr[0] == 'N' ? FALSE : TRUE;
      if (!is_deterministic)
      {
        my_message(ER_BINLOG_UNSAFE_ROUTINE,
                   ER(ER_BINLOG_UNSAFE_ROUTINE), MYF(0));
        ret= SP_INTERNAL_ERROR;
        goto err;
      }
    }

    store_record(table,record[1]);
    Item_func_now_local::store_in(table->field[MYSQL_PROC_FIELD_MODIFIED]);
    if (chistics->suid != SP_IS_DEFAULT_SUID)
      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]->
	store((longlong)chistics->suid, TRUE);
    if (chistics->daccess != SP_DEFAULT_ACCESS)
      table->field[MYSQL_PROC_FIELD_ACCESS]->
	store((longlong)chistics->daccess, TRUE);
    if (chistics->comment.str)
      table->field[MYSQL_PROC_FIELD_COMMENT]->store(chistics->comment.str,
						    chistics->comment.length,
						    system_charset_info);
    if ((ret= table->file->ha_update_row(table->record[1],table->record[0])) &&
        ret != HA_ERR_RECORD_IS_THE_SAME)
      ret= SP_WRITE_ROW_FAILED;
    else
      ret= 0;
  }

  if (ret == SP_OK)
  {
    if (write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
      ret= SP_INTERNAL_ERROR;
    sp_cache_invalidate();
  }
err:
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(ret);
}


/**
  This internal handler is used to trap errors from opening mysql.proc.
*/

class Lock_db_routines_error_handler : public Internal_error_handler
{
public:
  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level level,
                        const char* msg,
                        Sql_condition ** cond_hdl)
  {
    if (sql_errno == ER_NO_SUCH_TABLE ||
        sql_errno == ER_CANNOT_LOAD_FROM_TABLE_V2 ||
        sql_errno == ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE ||
        sql_errno == ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2)
      return true;
    return false;
  }
};


/**
   Acquires exclusive metadata lock on all stored routines in the
   given database.

   @note Will also return false (=success) if mysql.proc can't be opened
         or is outdated. This allows DROP DATABASE to continue in these
         cases.
 */

bool lock_db_routines(THD *thd, char *db)
{
  TABLE *table;
  uint key_len;
  Open_tables_backup open_tables_state_backup;
  MDL_request_list mdl_requests;
  Lock_db_routines_error_handler err_handler;
  DBUG_ENTER("lock_db_routines");

  /*
    mysql.proc will be re-opened during deletion, so we can ignore
    errors when opening the table here. The error handler is
    used to avoid getting the same warning twice.
  */
  thd->push_internal_handler(&err_handler);
  table= open_proc_table_for_read(thd, &open_tables_state_backup);
  thd->pop_internal_handler();
  if (!table)
  {
    /*
      DROP DATABASE should not fail even if mysql.proc does not exist
      or is outdated. We therefore only abort mysql_rm_db() if we
      have errors not handled by the error handler.
    */
    DBUG_RETURN(thd->is_error() || thd->killed);
  }

  table->field[MYSQL_PROC_FIELD_DB]->store(db, strlen(db), system_charset_info);
  key_len= table->key_info->key_part[0].store_length;
  int nxtres= table->file->ha_index_init(0, 1);
  if (nxtres)
  {
    table->file->print_error(nxtres, MYF(0));
    close_system_tables(thd, &open_tables_state_backup);
    DBUG_RETURN(true);
  }

  if (! table->file->index_read_map(table->record[0],
                                    table->field[MYSQL_PROC_FIELD_DB]->ptr,
                                    (key_part_map)1, HA_READ_KEY_EXACT))
  {
    do
    {
      char *sp_name= get_field(thd->mem_root,
                               table->field[MYSQL_PROC_FIELD_NAME]);
      longlong sp_type= table->field[MYSQL_PROC_MYSQL_TYPE]->val_int();
      MDL_request *mdl_request= new (thd->mem_root) MDL_request;
      mdl_request->init(sp_type == SP_TYPE_FUNCTION ?
                        MDL_key::FUNCTION : MDL_key::PROCEDURE,
                        db, sp_name, MDL_EXCLUSIVE, MDL_TRANSACTION);
      mdl_requests.push_front(mdl_request);
    } while (! (nxtres= table->file->index_next_same(table->record[0],
                                         table->field[MYSQL_PROC_FIELD_DB]->ptr,
						     key_len)));
  }
  table->file->ha_index_end();
  if (nxtres != 0 && nxtres != HA_ERR_END_OF_FILE)
  {
    table->file->print_error(nxtres, MYF(0));
    close_system_tables(thd, &open_tables_state_backup);
    DBUG_RETURN(true);
  }
  close_system_tables(thd, &open_tables_state_backup);

  /* We should already hold a global IX lock and a schema X lock. */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::GLOBAL, "", "",
                                             MDL_INTENTION_EXCLUSIVE) &&
              thd->mdl_context.is_lock_owner(MDL_key::SCHEMA, db, "",
                                             MDL_EXCLUSIVE));
  DBUG_RETURN(thd->mdl_context.acquire_locks(&mdl_requests,
                                             thd->variables.lock_wait_timeout));
}


/**
  Drop all routines in database 'db'

  @note Close the thread tables, the calling code might want to
  delete from other system tables afterwards.

  @todo We need to change this function to call a my_error()/print_error()
  once we stop ignoring return value of sp_drop_db_routines().
*/

int
sp_drop_db_routines(THD *thd, char *db)
{
  TABLE *table;
  int ret;
  uint key_len;
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  DBUG_ENTER("sp_drop_db_routines");
  DBUG_PRINT("enter", ("db: %s", db));

  ret= SP_OPEN_TABLE_FAILED;
  if (!(table= open_proc_table_for_update(thd)))
    goto err;

  table->field[MYSQL_PROC_FIELD_DB]->store(db, strlen(db), system_charset_info);
  key_len= table->key_info->key_part[0].store_length;

  ret= SP_OK;
  if (table->file->ha_index_init(0, 1))
  {
    ret= SP_KEY_NOT_FOUND;
    goto err_idx_init;
  }

  if (! table->file->ha_index_read_map(table->record[0],
                                       (uchar *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
                                       (key_part_map)1, HA_READ_KEY_EXACT))
  {
    int nxtres;
    bool deleted= FALSE;

    do
    {
      if (! table->file->ha_delete_row(table->record[0]))
	deleted= TRUE;		/* We deleted something */
      else
      {
	ret= SP_DELETE_ROW_FAILED;
	nxtres= 0;
	break;
      }
    } while (! (nxtres= table->file->ha_index_next_same(table->record[0],
                                (uchar *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
						     key_len)));
    if (nxtres != HA_ERR_END_OF_FILE)
      ret= SP_KEY_NOT_FOUND;
    if (deleted)
      sp_cache_invalidate();
  }
  table->file->ha_index_end();

err_idx_init:
  close_thread_tables(thd);
  /*
    Make sure to only release the MDL lock on mysql.proc, not other
    metadata locks DROP DATABASE might have acquired.
  */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

err:
  DBUG_RETURN(ret);
}


/**
  Implement SHOW CREATE statement for stored routines.

  The operation finds the stored routine object specified by name and then
  calls sp_head::show_create_routine() for the object.

  @param thd  Thread context.
  @param type Stored routine type
              (SP_TYPE_PROCEDURE or SP_TYPE_FUNCTION)
  @param name Stored routine name.

  @return Error status.
    @retval FALSE on success
    @retval TRUE on error
*/

bool sp_show_create_routine(THD *thd, enum_sp_type type, sp_name *name)
{
  sp_head *sp;

  DBUG_ENTER("sp_show_create_routine");
  DBUG_PRINT("enter", ("name: %.*s",
                       (int) name->m_name.length,
                       name->m_name.str));

  DBUG_ASSERT(type == SP_TYPE_PROCEDURE || type == SP_TYPE_FUNCTION);

  /*
    @todo: Consider using prelocking for this code as well. Currently
    SHOW CREATE PROCEDURE/FUNCTION is a dirty read of the data
    dictionary, i.e. takes no metadata locks.
    It is "safe" to do as long as it doesn't affect the results
    of the binary log or the query cache, which currently it does not.
  */
  if (sp_cache_routine(thd, type, name, FALSE, &sp))
    DBUG_RETURN(TRUE);

  if (sp == NULL || sp->show_create_routine(thd, type))
  {
    /*
      If we have insufficient privileges, pretend the routine
      does not exist.
    */
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
             type == SP_TYPE_FUNCTION ? "FUNCTION" : "PROCEDURE",
             name->m_name.str);
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/**
  Obtain object representing stored procedure/function by its name from
  stored procedures cache and looking into mysql.proc if needed.

  @param thd          thread context
  @param type         type of object (SP_TYPE_FUNCTION or SP_TYPE_PROCEDURE)
  @param name         name of procedure
  @param cp           hash to look routine in
  @param cache_only   if true perform cache-only lookup
                      (Don't look in mysql.proc).

  @retval
    NonNULL pointer to sp_head object for the procedure
  @retval
    NULL    in case of error.
*/

sp_head *sp_find_routine(THD *thd, enum_sp_type type, sp_name *name,
                         sp_cache **cp, bool cache_only)
{
  sp_head *sp;
  ulong depth= (type == SP_TYPE_PROCEDURE ?
                thd->variables.max_sp_recursion_depth :
                0);
  DBUG_ENTER("sp_find_routine");
  DBUG_PRINT("enter", ("name:  %.*s.%.*s  type: %d  cache only %d",
                       (int) name->m_db.length, name->m_db.str,
                       (int) name->m_name.length, name->m_name.str,
                       type, cache_only));

  if ((sp= sp_cache_lookup(cp, name)))
  {
    ulong level;
    sp_head *new_sp;
    const char *returns= "";
    char definer[USER_HOST_BUFF_SIZE];

    /*
      String buffer for RETURNS data type must have system charset;
      64 -- size of "returns" column of mysql.proc.
    */
    String retstr(64);
    retstr.set_charset(sp->get_creation_ctx()->get_client_cs());

    DBUG_PRINT("info", ("found: 0x%lx", (ulong)sp));
    if (sp->m_first_free_instance)
    {
      DBUG_PRINT("info", ("first free: 0x%lx  level: %lu  flags %x",
                          (ulong)sp->m_first_free_instance,
                          sp->m_first_free_instance->m_recursion_level,
                          sp->m_first_free_instance->m_flags));
      DBUG_ASSERT(!(sp->m_first_free_instance->m_flags & sp_head::IS_INVOKED));
      if (sp->m_first_free_instance->m_recursion_level > depth)
      {
        recursion_level_error(thd, sp);
        DBUG_RETURN(0);
      }
      DBUG_RETURN(sp->m_first_free_instance);
    }
    /*
      Actually depth could be +1 than the actual value in case a SP calls
      SHOW CREATE PROCEDURE. Hence, the linked list could hold up to one more
      instance.
    */

    level= sp->m_last_cached_sp->m_recursion_level + 1;
    if (level > depth)
    {
      recursion_level_error(thd, sp);
      DBUG_RETURN(0);
    }

    strxmov(definer, sp->m_definer_user.str, "@",
            sp->m_definer_host.str, NullS);
    if (type == SP_TYPE_FUNCTION)
    {
      sp_returns_type(thd, retstr, sp);
      returns= retstr.ptr();
    }
    if (db_load_routine(thd, type, name, &new_sp,
                        sp->m_sql_mode, sp->m_params.str, returns,
                        sp->m_body.str, *sp->m_chistics, definer,
                        sp->m_created, sp->m_modified,
                        sp->get_creation_ctx()) == SP_OK)
    {
      sp->m_last_cached_sp->m_next_cached_sp= new_sp;
      new_sp->m_recursion_level= level;
      new_sp->m_first_instance= sp;
      sp->m_last_cached_sp= sp->m_first_free_instance= new_sp;
      DBUG_PRINT("info", ("added level: 0x%lx, level: %lu, flags %x",
                          (ulong)new_sp, new_sp->m_recursion_level,
                          new_sp->m_flags));
      DBUG_RETURN(new_sp);
    }
    DBUG_RETURN(0);
  }
  if (!cache_only)
  {
    if (db_find_routine(thd, type, name, &sp) == SP_OK)
    {
      sp_cache_insert(cp, sp);
      DBUG_PRINT("info", ("added new: 0x%lx, level: %lu, flags %x",
                          (ulong)sp, sp->m_recursion_level,
                          sp->m_flags));
    }
  }
  DBUG_RETURN(sp);
}


/**
  This is used by sql_acl.cc:mysql_routine_grant() and is used to find
  the routines in 'routines'.

  @param thd Thread handler
  @param routines List of needles in the hay stack
  @param is_proc  Indicates whether routines in the list are procedures
                  or functions.

  @return
    @retval FALSE Found.
    @retval TRUE  Not found
*/

bool
sp_exist_routines(THD *thd, TABLE_LIST *routines, bool is_proc)
{
  TABLE_LIST *routine;
  bool sp_object_found;
  DBUG_ENTER("sp_exists_routine");
  for (routine= routines; routine; routine= routine->next_global)
  {
    sp_name *name;
    LEX_STRING lex_db;
    LEX_STRING lex_name;
    lex_db.length= strlen(routine->db);
    lex_name.length= strlen(routine->table_name);
    lex_db.str= thd->strmake(routine->db, lex_db.length);
    lex_name.str= thd->strmake(routine->table_name, lex_name.length);
    name= new sp_name(lex_db, lex_name, true);
    name->init_qname(thd);
    sp_object_found= is_proc ? sp_find_routine(thd, SP_TYPE_PROCEDURE,
                                               name, &thd->sp_proc_cache,
                                               FALSE) != NULL :
                               sp_find_routine(thd, SP_TYPE_FUNCTION,
                                               name, &thd->sp_func_cache,
                                               FALSE) != NULL;
    thd->get_stmt_da()->reset_condition_info(thd->query_id);
    if (! sp_object_found)
    {
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION or PROCEDURE",
               routine->table_name);
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}


extern "C" uchar* sp_sroutine_key(const uchar *ptr, size_t *plen,
                                  my_bool first)
{
  Sroutine_hash_entry *rn= (Sroutine_hash_entry *)ptr;
  *plen= rn->mdl_request.key.length();
  return (uchar *)rn->mdl_request.key.ptr();
}


/**
  Auxilary function that adds new element to the set of stored routines
  used by statement.

  In case when statement uses stored routines but does not need
  prelocking (i.e. it does not use any tables) we will access the
  elements of Query_tables_list::sroutines set on prepared statement
  re-execution. Because of this we have to allocate memory for both
  hash element and copy of its key in persistent arena.

  @param prelocking_ctx  Prelocking context of the statement
  @param arena           Arena in which memory for new element will be
                         allocated
  @param key             Key for the hash representing set
  @param belong_to_view  Uppermost view which uses this routine
                         (0 if routine is not used by view)

  @note
    Will also add element to end of 'Query_tables_list::sroutines_list' list.

  @todo
    When we will got rid of these accesses on re-executions we will be
    able to allocate memory for hash elements in non-persitent arena
    and directly use key values from sp_head::m_sroutines sets instead
    of making their copies.

  @retval
    TRUE   new element was added.
  @retval
    FALSE  element was not added (because it is already present in
    the set).
*/

bool sp_add_used_routine(Query_tables_list *prelocking_ctx, Query_arena *arena,
                         const MDL_key *key, TABLE_LIST *belong_to_view)
{
  my_hash_init_opt(&prelocking_ctx->sroutines, system_charset_info,
                   Query_tables_list::START_SROUTINES_HASH_SIZE,
                   0, 0, sp_sroutine_key, 0, 0);

  if (!my_hash_search(&prelocking_ctx->sroutines, key->ptr(), key->length()))
  {
    Sroutine_hash_entry *rn=
      (Sroutine_hash_entry *)arena->alloc(sizeof(Sroutine_hash_entry));
    if (!rn)              // OOM. Error will be reported using fatal_error().
      return FALSE;
    rn->mdl_request.init(key, MDL_SHARED, MDL_TRANSACTION);
    if (my_hash_insert(&prelocking_ctx->sroutines, (uchar *)rn))
      return FALSE;
    prelocking_ctx->sroutines_list.link_in_list(rn, &rn->next);
    rn->belong_to_view= belong_to_view;
    rn->m_sp_cache_version= 0;
    return TRUE;
  }
  return FALSE;
}


/**
  Add routine which is explicitly used by statement to the set of stored
  routines used by this statement.

  To be friendly towards prepared statements one should pass
  persistent arena as second argument.

  @param prelocking_ctx  Prelocking context of the statement
  @param arena           Arena in which memory for new element of the set
                         will be allocated
  @param rt              Routine name
  @param rt_type         Routine type (one of SP_TYPE_PROCEDURE/...)

  @note
    Will also add element to end of 'Query_tables_list::sroutines_list' list
    (and will take into account that this is an explicitly used routine).
*/

void sp_add_used_routine(Query_tables_list *prelocking_ctx, Query_arena *arena,
                         sp_name *rt, enum_sp_type rt_type)
{
  MDL_key key((rt_type == SP_TYPE_FUNCTION) ? MDL_key::FUNCTION :
                                                MDL_key::PROCEDURE,
              rt->m_db.str, rt->m_name.str);
  (void)sp_add_used_routine(prelocking_ctx, arena, &key, 0);
  prelocking_ctx->sroutines_list_own_last= prelocking_ctx->sroutines_list.next;
  prelocking_ctx->sroutines_list_own_elements=
                    prelocking_ctx->sroutines_list.elements;
}


/**
  Remove routines which are only indirectly used by statement from
  the set of routines used by this statement.

  @param prelocking_ctx  Prelocking context of the statement
*/

void sp_remove_not_own_routines(Query_tables_list *prelocking_ctx)
{
  Sroutine_hash_entry *not_own_rt, *next_rt;
  for (not_own_rt= *prelocking_ctx->sroutines_list_own_last;
       not_own_rt; not_own_rt= next_rt)
  {
    /*
      It is safe to obtain not_own_rt->next after calling hash_delete() now
      but we want to be more future-proof.
    */
    next_rt= not_own_rt->next;
    my_hash_delete(&prelocking_ctx->sroutines, (uchar *)not_own_rt);
  }

  *prelocking_ctx->sroutines_list_own_last= NULL;
  prelocking_ctx->sroutines_list.next= prelocking_ctx->sroutines_list_own_last;
  prelocking_ctx->sroutines_list.elements= 
                    prelocking_ctx->sroutines_list_own_elements;
}


/**
  Add contents of hash representing set of routines to the set of
  routines used by statement.

  @param thd             Thread context
  @param prelocking_ctx  Prelocking context of the statement
  @param src             Hash representing set from which routines will
                         be added
  @param belong_to_view  Uppermost view which uses these routines, 0 if none

  @note It will also add elements to end of
        'Query_tables_list::sroutines_list' list.
*/

void
sp_update_stmt_used_routines(THD *thd, Query_tables_list *prelocking_ctx,
                             HASH *src, TABLE_LIST *belong_to_view)
{
  for (uint i=0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)my_hash_element(src, i);
    (void)sp_add_used_routine(prelocking_ctx, thd->stmt_arena,
                              &rt->mdl_request.key, belong_to_view);
  }
}


/**
  Add contents of list representing set of routines to the set of
  routines used by statement.

  @param thd             Thread context
  @param prelocking_ctx  Prelocking context of the statement
  @param src             List representing set from which routines will
                         be added
  @param belong_to_view  Uppermost view which uses these routines, 0 if none

  @note It will also add elements to end of
        'Query_tables_list::sroutines_list' list.
*/

void sp_update_stmt_used_routines(THD *thd, Query_tables_list *prelocking_ctx,
                                  SQL_I_List<Sroutine_hash_entry> *src,
                                  TABLE_LIST *belong_to_view)
{
  for (Sroutine_hash_entry *rt= src->first; rt; rt= rt->next)
    (void)sp_add_used_routine(prelocking_ctx, thd->stmt_arena,
                              &rt->mdl_request.key, belong_to_view);
}


/**
  A helper wrapper around sp_cache_routine() to use from
  prelocking until 'sp_name' is eradicated as a class.
*/

int sp_cache_routine(THD *thd, Sroutine_hash_entry *rt,
                     bool lookup_only, sp_head **sp)
{
  char qname_buff[NAME_LEN*2+1+1];
  sp_name name(&rt->mdl_request.key, qname_buff);
  MDL_key::enum_mdl_namespace mdl_type= rt->mdl_request.key.mdl_namespace();
  enum_sp_type type= (mdl_type == MDL_key::FUNCTION) ?
                     SP_TYPE_FUNCTION : SP_TYPE_PROCEDURE;

  /*
    Check that we have an MDL lock on this routine, unless it's a top-level
    CALL. The assert below should be unambiguous: the first element
    in sroutines_list has an MDL lock unless it's a top-level call, or a
    trigger, but triggers can't occur here (see the preceding assert).
  */
  DBUG_ASSERT(rt->mdl_request.ticket || rt == thd->lex->sroutines_list.first);

  return sp_cache_routine(thd, type, &name, lookup_only, sp);
}


/**
  Ensure that routine is present in cache by loading it from the mysql.proc
  table if needed. If the routine is present but old, reload it.
  Emit an appropriate error if there was a problem during
  loading.

  @param[in]  thd   Thread context.
  @param[in]  type  Type of object (SP_TYPE_FUNCTION or SP_TYPE_PROCEDURE).
  @param[in]  name  Name of routine.
  @param[in]  lookup_only Only check that the routine is in the cache.
                    If it's not, don't try to load. If it is present,
                    but old, don't try to reload.
  @param[out] sp    Pointer to sp_head object for routine, NULL if routine was
                    not found.

  @retval 0      Either routine is found and was succesfully loaded into cache
                 or it does not exist.
  @retval non-0  Error while loading routine from mysql,proc table.
*/

int sp_cache_routine(THD *thd, enum_sp_type type, sp_name *name,
                     bool lookup_only, sp_head **sp)
{
  int ret= 0;
  sp_cache **spc= (type == SP_TYPE_FUNCTION) ?
                  &thd->sp_func_cache : &thd->sp_proc_cache;

  DBUG_ENTER("sp_cache_routine");

  DBUG_ASSERT(type == SP_TYPE_FUNCTION || type == SP_TYPE_PROCEDURE);


  *sp= sp_cache_lookup(spc, name);

  if (lookup_only)
    DBUG_RETURN(SP_OK);

  if (*sp)
  {
    sp_cache_flush_obsolete(spc, sp);
    if (*sp)
      DBUG_RETURN(SP_OK);
  }

  switch ((ret= db_find_routine(thd, type, name, sp)))
  {
    case SP_OK:
      sp_cache_insert(spc, *sp);
      break;
    case SP_KEY_NOT_FOUND:
      ret= SP_OK;
      break;
    default:
      /* Query might have been killed, don't set error. */
      if (thd->killed)
        break;
      /*
        Any error when loading an existing routine is either some problem
        with the mysql.proc table, or a parse error because the contents
        has been tampered with (in which case we clear that error).
      */
      if (ret == SP_PARSE_ERROR)
        thd->clear_error();
      /*
        If we cleared the parse error, or when db_find_routine() flagged
        an error with it's return value without calling my_error(), we
        set the generic "mysql.proc table corrupt" error here.
      */
      if (! thd->is_error())
      {
        /*
          SP allows full NAME_LEN chars thus he have to allocate enough
          size in bytes. Otherwise there is stack overrun could happen
          if multibyte sequence is `name`. `db` is still safe because the
          rest of the server checks agains NAME_LEN bytes and not chars.
          Hence, the overrun happens only if the name is in length > 32 and
          uses multibyte (cyrillic, greek, etc.)
        */
        char n[NAME_LEN*2+2];

        /* m_qname.str is not always \0 terminated */
        memcpy(n, name->m_qname.str, name->m_qname.length);
        n[name->m_qname.length]= '\0';
        my_error(ER_SP_PROC_TABLE_CORRUPT, MYF(0), n, ret);
      }
      break;
  }
  DBUG_RETURN(ret);
}


/**
  Generates the CREATE... string from the table information.

  @return
    Returns TRUE on success, FALSE on (alloc) failure.
*/
static bool create_string(THD *thd, String *buf,
                          enum_sp_type type,
                          const char *db, ulong dblen,
                          const char *name, ulong namelen,
                          const char *params, ulong paramslen,
                          const char *returns, ulong returnslen,
                          const char *body, ulong bodylen,
                          st_sp_chistics *chistics,
                          const LEX_STRING *definer_user,
                          const LEX_STRING *definer_host,
                          sql_mode_t sql_mode)
{
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  /* Make some room to begin with */
  if (buf->alloc(100 + dblen + 1 + namelen + paramslen + returnslen + bodylen +
		 chistics->comment.length + 10 /* length of " DEFINER= "*/ +
                 USER_HOST_BUFF_SIZE))
    return FALSE;

  thd->variables.sql_mode= sql_mode;
  buf->append(STRING_WITH_LEN("CREATE "));
  append_definer(thd, buf, definer_user, definer_host);
  if (type == SP_TYPE_FUNCTION)
    buf->append(STRING_WITH_LEN("FUNCTION "));
  else
    buf->append(STRING_WITH_LEN("PROCEDURE "));
  if (dblen > 0)
  {
    append_identifier(thd, buf, db, dblen);
    buf->append('.');
  }
  append_identifier(thd, buf, name, namelen);
  buf->append('(');
  buf->append(params, paramslen);
  buf->append(')');
  if (type == SP_TYPE_FUNCTION)
  {
    buf->append(STRING_WITH_LEN(" RETURNS "));
    buf->append(returns, returnslen);
  }
  buf->append('\n');
  switch (chistics->daccess) {
  case SP_NO_SQL:
    buf->append(STRING_WITH_LEN("    NO SQL\n"));
    break;
  case SP_READS_SQL_DATA:
    buf->append(STRING_WITH_LEN("    READS SQL DATA\n"));
    break;
  case SP_MODIFIES_SQL_DATA:
    buf->append(STRING_WITH_LEN("    MODIFIES SQL DATA\n"));
    break;
  case SP_DEFAULT_ACCESS:
  case SP_CONTAINS_SQL:
    /* Do nothing */
    break;
  }
  if (chistics->detistic)
    buf->append(STRING_WITH_LEN("    DETERMINISTIC\n"));
  if (chistics->suid == SP_IS_NOT_SUID)
    buf->append(STRING_WITH_LEN("    SQL SECURITY INVOKER\n"));
  if (chistics->comment.length)
  {
    buf->append(STRING_WITH_LEN("    COMMENT "));
    append_unescaped(buf, chistics->comment.str, chistics->comment.length);
    buf->append('\n');
  }
  buf->append(body, bodylen);
  thd->variables.sql_mode= old_sql_mode;
  return TRUE;
}


/**
  @brief    The function loads sp_head struct for information schema purposes
            (used for I_S ROUTINES & PARAMETERS tables).

  @param[in]      thd               thread handler
  @param[in]      proc_table        mysql.proc table structurte
  @param[in]      db                database name
  @param[in]      name              sp name
  @param[in]      sql_mode          SQL mode
  @param[in]      type              Routine type
  @param[in]      returns           'returns' string
  @param[in]      params            parameters definition string
  @param[out]     free_sp_head      returns 1 if we need to free sp_head struct
                                    otherwise returns 0
                                    
  @return     Pointer on sp_head struct
    @retval   #                     Pointer on sp_head struct
    @retval   0                     error
*/

sp_head *
sp_load_for_information_schema(THD *thd, TABLE *proc_table, String *db,
                               String *name, sql_mode_t sql_mode,
                               enum_sp_type type,
                               const char *returns, const char *params,
                               bool *free_sp_head)
{
  const char *sp_body;
  String defstr;
  struct st_sp_chistics sp_chistics;
  const LEX_STRING definer_user= {(char*)STRING_WITH_LEN("")};
  const LEX_STRING definer_host= {(char*)STRING_WITH_LEN("")}; 
  LEX_STRING sp_db_str;
  LEX_STRING sp_name_str;
  sp_head *sp;
  sp_cache **spc= (type == SP_TYPE_FUNCTION) ?
                  &thd->sp_func_cache : &thd->sp_proc_cache;
  sp_db_str.str= db->c_ptr();
  sp_db_str.length= db->length();
  sp_name_str.str= name->c_ptr();
  sp_name_str.length= name->length();
  sp_name sp_name_obj(sp_db_str, sp_name_str, true);
  sp_name_obj.init_qname(thd);
  *free_sp_head= 0;
  if ((sp= sp_cache_lookup(spc, &sp_name_obj)))
  {
    return sp;
  }

  LEX *old_lex= thd->lex, newlex;
  Stored_program_creation_ctx *creation_ctx= 
    Stored_routine_creation_ctx::load_from_db(thd, &sp_name_obj, proc_table);
  sp_body= (type == SP_TYPE_FUNCTION) ? "RETURN NULL" : "BEGIN END";
  memset(&sp_chistics, 0, sizeof(sp_chistics));
  defstr.set_charset(creation_ctx->get_client_cs());
  if (!create_string(thd, &defstr, type, 
                     sp_db_str.str, sp_db_str.length, 
                     sp_name_obj.m_name.str, sp_name_obj.m_name.length, 
                     params, strlen(params),
                     returns, strlen(returns), 
                     sp_body, strlen(sp_body),
                     &sp_chistics, &definer_user, &definer_host, sql_mode))
    return 0;

  thd->lex= &newlex;
  newlex.current_select= NULL; 
  sp= sp_compile(thd, &defstr, sql_mode, creation_ctx);
  *free_sp_head= 1;
  thd->lex->sphead= NULL;
  lex_end(thd->lex);
  thd->lex= old_lex;
  return sp;
}


/**
  Start parsing of a stored program.

  This function encapsulates all the steps necessary to initialize sp_head to
  start parsing SP.

  Every successful call of sp_start_parsing() must finish with
  sp_finish_parsing().

  @param thd      Thread context.
  @param sp_type  The stored program type
  @param sp_name  The stored progam name

  @return properly initialized sp_head-instance in case of success, or NULL is
  case of out-of-memory error.
*/
sp_head *sp_start_parsing(THD *thd,
                          enum_sp_type sp_type,
                          sp_name *sp_name)
{
  // The order is important:
  // 1. new sp_head()

  sp_head *sp= new sp_head(sp_type);

  if (!sp)
    return NULL;

  // 2. start_parsing_sp_body()

  sp->m_parser_data.start_parsing_sp_body(thd, sp);

  // 3. finish initialization.

  sp->m_root_parsing_ctx= new (thd->mem_root) sp_pcontext();

  if (!sp->m_root_parsing_ctx)
    return NULL;

  thd->lex->set_sp_current_parsing_ctx(sp->m_root_parsing_ctx);

  // 4. set name.

  sp->init_sp_name(thd, sp_name);

  return sp;
}


/**
  Finish parsing of a stored program.

  This is a counterpart of sp_start_parsing().

  @param thd  Thread context.
*/
void sp_finish_parsing(THD *thd)
{
  sp_head *sp= thd->lex->sphead;

  DBUG_ASSERT(sp);

  sp->set_body_end(thd);

  sp->m_parser_data.finish_parsing_sp_body(thd);
}


/// @return Item_result code corresponding to the RETURN-field type code.
Item_result sp_map_result_type(enum enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
    return INT_RESULT;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    return DECIMAL_RESULT;
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    return REAL_RESULT;
  default:
    return STRING_RESULT;
  }
}


/// @return Item::Type code corresponding to the RETURN-field type code.
Item::Type sp_map_item_type(enum enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
    return Item::INT_ITEM;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    return Item::DECIMAL_ITEM;
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    return Item::REAL_ITEM;
  default:
    return Item::STRING_ITEM;
  }
}


/**
  @param lex LEX-object, representing an SQL-statement inside SP.

  @return a combination of:
    - sp_head::MULTI_RESULTS: added if the 'cmd' is a command that might
      result in multiple result sets being sent back.
    - sp_head::CONTAINS_DYNAMIC_SQL: added if 'cmd' is one of PREPARE,
      EXECUTE, DEALLOCATE.
*/
uint sp_get_flags_for_command(LEX *lex)
{
  uint flags;

  switch (lex->sql_command) {
  case SQLCOM_SELECT:
    if (lex->result)
    {
      flags= 0;                      /* This is a SELECT with INTO clause */
      break;
    }
    /* fallthrough */
  case SQLCOM_ANALYZE:
  case SQLCOM_OPTIMIZE:
  case SQLCOM_PRELOAD_KEYS:
  case SQLCOM_ASSIGN_TO_KEYCACHE:
  case SQLCOM_CHECKSUM:
  case SQLCOM_CHECK:
  case SQLCOM_HA_READ:
  case SQLCOM_SHOW_BINLOGS:
  case SQLCOM_SHOW_BINLOG_EVENTS:
  case SQLCOM_SHOW_RELAYLOG_EVENTS:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_CREATE_DB:
  case SQLCOM_SHOW_CREATE_FUNC:
  case SQLCOM_SHOW_CREATE_PROC:
  case SQLCOM_SHOW_CREATE_EVENT:
  case SQLCOM_SHOW_CREATE_TRIGGER:
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_ERRORS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_FUNC_CODE:
  case SQLCOM_SHOW_GRANTS:
  case SQLCOM_SHOW_ENGINE_STATUS:
  case SQLCOM_SHOW_ENGINE_LOGS:
  case SQLCOM_SHOW_ENGINE_MUTEX:
  case SQLCOM_SHOW_EVENTS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_MASTER_STAT:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_PRIVILEGES:
  case SQLCOM_SHOW_PROCESSLIST:
  case SQLCOM_SHOW_PROC_CODE:
  case SQLCOM_SHOW_SLAVE_HOSTS:
  case SQLCOM_SHOW_SLAVE_STAT:
  case SQLCOM_SHOW_SLAVE_STAT_NONBLOCKING:
  case SQLCOM_SHOW_STATUS:
  case SQLCOM_SHOW_STATUS_FUNC:
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_WARNS:
  case SQLCOM_REPAIR:
    flags= sp_head::MULTI_RESULTS;
    break;
  /*
    EXECUTE statement may return a result set, but doesn't have to.
    We can't, however, know it in advance, and therefore must add
    this statement here. This is ok, as is equivalent to a result-set
    statement within an IF condition.
  */
  case SQLCOM_EXECUTE:
    flags= sp_head::MULTI_RESULTS | sp_head::CONTAINS_DYNAMIC_SQL;
    break;
  case SQLCOM_PREPARE:
  case SQLCOM_DEALLOCATE_PREPARE:
    flags= sp_head::CONTAINS_DYNAMIC_SQL;
    break;
  case SQLCOM_CREATE_TABLE:
    if (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)
      flags= 0;
    else
      flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  case SQLCOM_DROP_TABLE:
    if (lex->drop_temporary)
      flags= 0;
    else
      flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  case SQLCOM_FLUSH:
    flags= sp_head::HAS_SQLCOM_FLUSH;
    break;
  case SQLCOM_RESET:
    flags= sp_head::HAS_SQLCOM_RESET;
    break;
  case SQLCOM_CREATE_INDEX:
  case SQLCOM_CREATE_DB:
  case SQLCOM_CREATE_VIEW:
  case SQLCOM_CREATE_TRIGGER:
  case SQLCOM_CREATE_USER:
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_GRANT:
  case SQLCOM_REVOKE:
  case SQLCOM_BEGIN:
  case SQLCOM_RENAME_TABLE:
  case SQLCOM_RENAME_USER:
  case SQLCOM_DROP_INDEX:
  case SQLCOM_DROP_DB:
  case SQLCOM_REVOKE_ALL:
  case SQLCOM_DROP_USER:
  case SQLCOM_DROP_VIEW:
  case SQLCOM_DROP_TRIGGER:
  case SQLCOM_TRUNCATE:
  case SQLCOM_COMMIT:
  case SQLCOM_ROLLBACK:
  case SQLCOM_LOAD:
  case SQLCOM_LOCK_TABLES:
  case SQLCOM_CREATE_PROCEDURE:
  case SQLCOM_CREATE_SPFUNCTION:
  case SQLCOM_ALTER_PROCEDURE:
  case SQLCOM_ALTER_FUNCTION:
  case SQLCOM_DROP_PROCEDURE:
  case SQLCOM_DROP_FUNCTION:
  case SQLCOM_CREATE_EVENT:
  case SQLCOM_ALTER_EVENT:
  case SQLCOM_DROP_EVENT:
  case SQLCOM_INSTALL_PLUGIN:
  case SQLCOM_UNINSTALL_PLUGIN:
    flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  default:
    flags= lex->describe ? sp_head::MULTI_RESULTS : 0;
    break;
  }
  return flags;
}


/**
  Check that the name 'ident' is ok.  It's assumed to be an 'ident'
  from the parser, so we only have to check length and trailing spaces.
  The former is a standard requirement (and 'show status' assumes a
  non-empty name), the latter is a mysql:ism as trailing spaces are
  removed by get_field().

  @retval true    bad name
  @retval false   name is ok
*/

bool sp_check_name(LEX_STRING *ident)
{
  if (!ident || !ident->str || !ident->str[0] ||
      ident->str[ident->length-1] == ' ')
  {
    my_error(ER_SP_WRONG_NAME, MYF(0), ident->str);
    return true;
  }

  if (check_string_char_length(ident, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), ident->str);
    return true;
  }

  return false;
}


/**
  Simple function for adding an explicitly named (systems) table to
  the global table list, e.g. "mysql", "proc".
*/
TABLE_LIST *sp_add_to_query_tables(THD *thd, LEX *lex,
                                   const char *db, const char *name,
                                   thr_lock_type locktype,
                                   enum_mdl_type mdl_type)
{
  TABLE_LIST *table= (TABLE_LIST *)thd->calloc(sizeof(TABLE_LIST));

  if (!table)
    return NULL;

  table->db_length= strlen(db);
  table->db= thd->strmake(db, table->db_length);
  table->table_name_length= strlen(name);
  table->table_name= thd->strmake(name, table->table_name_length);
  table->alias= thd->strdup(name);
  table->lock_type= locktype;
  table->select_lex= lex->current_select;
  table->cacheable_table= 1;
  table->mdl_request.init(MDL_key::TABLE, table->db, table->table_name,
                          mdl_type, MDL_TRANSACTION);

  lex->add_to_query_tables(table);

  return table;
}


/**
  Prepare an Item for evaluation (call of fix_fields).

  @param thd       thread handler
  @param it_addr   pointer on item reference

  @retval
    NULL      error
  @retval
    non-NULL  prepared item
*/
Item *sp_prepare_func_item(THD* thd, Item **it_addr)
{
  it_addr= (*it_addr)->this_item_addr(thd, it_addr);

  if (!(*it_addr)->fixed &&
      ((*it_addr)->fix_fields(thd, it_addr) ||
       (*it_addr)->check_cols(1)))
  {
    DBUG_PRINT("info", ("fix_fields() failed"));
    return NULL;
  }

  return *it_addr;
}


/**
  Evaluate an expression and store the result in the field.

  @param thd                    current thread object
  @param result_field           the field to store the result
  @param expr_item_ptr          the root item of the expression

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/
bool sp_eval_expr(THD *thd, Field *result_field, Item **expr_item_ptr)
{
  Item *expr_item;
  enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  bool save_abort_on_warning= thd->abort_on_warning;
  unsigned int stmt_unsafe_rollback_flags=
    thd->transaction.stmt.get_unsafe_rollback_flags();

  if (!*expr_item_ptr)
    goto error;

  if (!(expr_item= sp_prepare_func_item(thd, expr_item_ptr)))
    goto error;

  /*
    Set THD flags to emit warnings/errors in case of overflow/type errors
    during saving the item into the field.

    Save original values and restore them after save.
  */

  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  thd->abort_on_warning= thd->is_strict_mode();
  thd->transaction.stmt.reset_unsafe_rollback_flags();

  /* Save the value in the field. Convert the value if needed. */

  expr_item->save_in_field(result_field, 0);

  thd->count_cuted_fields= save_count_cuted_fields;
  thd->abort_on_warning= save_abort_on_warning;
  thd->transaction.stmt.set_unsafe_rollback_flags(stmt_unsafe_rollback_flags);

  if (!thd->is_error())
    return false;

error:
  /*
    In case of error during evaluation, leave the result field set to NULL.
    Sic: we can't do it in the beginning of the function because the 
    result field might be needed for its own re-evaluation, e.g. case of 
    set x = x + 1;
  */
  result_field->set_null();
  return true;
}


/**
  Return a string representation of the Item value.

  @param thd  Thread context.
  @param str  String buffer for representation of the value.

  @note
    If the item has a string result type, the string is escaped
    according to its character set.

  @retval NULL      on error
  @retval non-NULL  a pointer to valid a valid string on success
*/
String *sp_get_item_value(THD *thd, Item *item, String *str)
{
  switch (item->result_type()) {
  case REAL_RESULT:
  case INT_RESULT:
  case DECIMAL_RESULT:
    if (item->field_type() != MYSQL_TYPE_BIT)
      return item->val_str(str);
    else {/* Bit type is handled as binary string */}
  case STRING_RESULT:
    {
      String *result= item->val_str(str);

      if (!result)
        return NULL;

      {
        char buf_holder[STRING_BUFFER_USUAL_SIZE];
        String buf(buf_holder, sizeof(buf_holder), result->charset());
        const CHARSET_INFO *cs= thd->variables.character_set_client;

        /* We must reset length of the buffer, because of String specificity. */
        buf.length(0);

        buf.append('_');
        buf.append(result->charset()->csname);
        if (cs->escape_with_backslash_is_dangerous)
          buf.append(' ');
        append_query_string(thd, cs, result, &buf);
        buf.append(" COLLATE '");
        buf.append(item->collation.collation->name);
        buf.append('\'');
        str->copy(buf);

        return str;
      }
    }

  case ROW_RESULT:
  default:
    return NULL;
  }
}
