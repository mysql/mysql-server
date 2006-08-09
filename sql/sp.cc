/* Copyright (C) 2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "sp.h"
#include "sp_head.h"
#include "sp_cache.h"
#include "sql_trigger.h"

#include <my_user.h>

static bool
create_string(THD *thd, String *buf,
	      int sp_type,
	      sp_name *name,
	      const char *params, ulong paramslen,
	      const char *returns, ulong returnslen,
	      const char *body, ulong bodylen,
	      st_sp_chistics *chistics,
              const LEX_STRING *definer_user,
              const LEX_STRING *definer_host);
static int
db_load_routine(THD *thd, int type, sp_name *name, sp_head **sphp,
                ulong sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified);

/*
 *
 * DB storage of Stored PROCEDUREs and FUNCTIONs
 *
 */

enum
{
  MYSQL_PROC_FIELD_DB = 0,
  MYSQL_PROC_FIELD_NAME,
  MYSQL_PROC_FIELD_TYPE,
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
  MYSQL_PROC_FIELD_COUNT
};

bool mysql_proc_table_exists= 1;

/* Tells what SP_DEFAULT_ACCESS should be mapped to */
#define SP_DEFAULT_ACCESS_MAPPING SP_CONTAINS_SQL


/*
  Close mysql.proc, opened with open_proc_table_for_read().

  SYNOPSIS
    close_proc_table()
      thd     Thread context
      backup  Pointer to Open_tables_state instance which holds
              information about tables which were open before we
              decided to access mysql.proc.
*/

void close_proc_table(THD *thd, Open_tables_state *backup)
{
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(backup);
}


/*
  Open the mysql.proc table for read.

  SYNOPSIS
    open_proc_table_for_read()
      thd     Thread context
      backup  Pointer to Open_tables_state instance where information about
              currently open tables will be saved, and from which will be
              restored when we will end work with mysql.proc.

  NOTES
    Thanks to restrictions which we put on opening and locking of
    this table for writing, we can open and lock it for reading
    even when we already have some other tables open and locked.
    One must call close_proc_table() to close table opened with
    this call.

  RETURN
    0	Error
    #	Pointer to TABLE object of mysql.proc
*/

TABLE *open_proc_table_for_read(THD *thd, Open_tables_state *backup)
{
  TABLE_LIST tables;
  TABLE *table;
  bool not_used;
  DBUG_ENTER("open_proc_table");

  /*
    Speed up things if mysql.proc doesn't exists. mysql_proc_table_exists
    is set when we create or read stored procedure or on flush privileges.
  */
  if (!mysql_proc_table_exists)
    DBUG_RETURN(0);

  thd->reset_n_backup_open_tables_state(backup);

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*)"proc";
  if (!(table= open_table(thd, &tables, thd->mem_root, &not_used,
                          MYSQL_LOCK_IGNORE_FLUSH)))
  {
    thd->restore_backup_open_tables_state(backup);
    mysql_proc_table_exists= 0;
    DBUG_RETURN(0);
  }
  table->use_all_columns();

  DBUG_ASSERT(table->s->system_table);

  table->reginfo.lock_type= TL_READ;
  /*
    We have to ensure we are not blocked by a flush tables, as this
    could lead to a deadlock if we have other tables opened.
  */
  if (!(thd->lock= mysql_lock_tables(thd, &table, 1,
                                     MYSQL_LOCK_IGNORE_FLUSH, &not_used)))
  {
    close_proc_table(thd, backup);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(table);
}


/*
  Open the mysql.proc table for update.

  SYNOPSIS
    open_proc_table_for_update()
      thd  Thread context

  NOTES
    Table opened with this call should closed using close_thread_tables().

  RETURN
    0	Error
    #	Pointer to TABLE object of mysql.proc
*/

static TABLE *open_proc_table_for_update(THD *thd)
{
  TABLE_LIST tables;
  TABLE *table;
  DBUG_ENTER("open_proc_table");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*)"proc";
  tables.lock_type= TL_WRITE;

  table= open_ltable(thd, &tables, TL_WRITE);
  if (table)
    table->use_all_columns();

  /*
    Under explicit LOCK TABLES or in prelocked mode we should not
    say that mysql.proc table does not exist if we are unable to
    open and lock it for writing since this condition may be
    transient.
  */
  if (!(thd->locked_tables || thd->prelocked_mode) || table)
    mysql_proc_table_exists= test(table);

  DBUG_RETURN(table);
}


/*
  Find row in open mysql.proc table representing stored routine.

  SYNOPSIS
    db_find_routine_aux()
      thd    Thread context
      type   Type of routine to find (function or procedure)
      name   Name of routine
      table  TABLE object for open mysql.proc table.

  RETURN VALUE
    SP_OK           - Routine found
    SP_KEY_NOT_FOUND- No routine with given name
*/

static int
db_find_routine_aux(THD *thd, int type, sp_name *name, TABLE *table)
{
  byte key[MAX_KEY_LENGTH];	// db, name, optional key length type
  DBUG_ENTER("db_find_routine_aux");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, name->m_name.length, name->m_name.str));

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

  if (table->file->index_read_idx(table->record[0], 0,
				  key, table->key_info->key_length,
				  HA_READ_KEY_EXACT))
    DBUG_RETURN(SP_KEY_NOT_FOUND);

  DBUG_RETURN(SP_OK);
}


/*
  Find routine definition in mysql.proc table and create corresponding
  sp_head object for it.

  SYNOPSIS
    db_find_routine()
      thd   Thread context
      type  Type of routine (TYPE_ENUM_PROCEDURE/...)
      name  Name of routine
      sphp  Out parameter in which pointer to created sp_head
            object is returned (0 in case of error).

  NOTE
    This function may damage current LEX during execution, so it is good
    idea to create temporary LEX and make it active before calling it.

  RETURN VALUE
    0     - Success
    non-0 - Error (may be one of special codes like SP_KEY_NOT_FOUND)
*/

static int
db_find_routine(THD *thd, int type, sp_name *name, sp_head **sphp)
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
  ulong sql_mode;
  Open_tables_state open_tables_state_backup;
  DBUG_ENTER("db_find_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, name->m_name.length, name->m_name.str));

  *sphp= 0;                                     // In case of errors
  if (!(table= open_proc_table_for_read(thd, &open_tables_state_backup)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if ((ret= db_find_routine_aux(thd, type, name, table)) != SP_OK)
    goto done;

  if (table->s->fields != MYSQL_PROC_FIELD_COUNT)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  bzero((char *)&chistics, sizeof(chistics));
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

  if (type == TYPE_ENUM_PROCEDURE)
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

  sql_mode= (ulong) table->field[MYSQL_PROC_FIELD_SQL_MODE]->val_int();

  table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= thd->strmake(str.ptr(), length);
  chistics.comment.str= ptr;
  chistics.comment.length= length;

  close_proc_table(thd, &open_tables_state_backup);
  table= 0;

  ret= db_load_routine(thd, type, name, sphp,
                       sql_mode, params, returns, body, chistics,
                       definer, created, modified);
                       
 done:
  if (table)
    close_proc_table(thd, &open_tables_state_backup);
  DBUG_RETURN(ret);
}


static int
db_load_routine(THD *thd, int type, sp_name *name, sp_head **sphp,
                ulong sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified)
{
  LEX *old_lex= thd->lex, newlex;
  String defstr;
  char old_db_buf[NAME_LEN+1];
  LEX_STRING old_db= { old_db_buf, sizeof(old_db_buf) };
  bool dbchanged;
  ulong old_sql_mode= thd->variables.sql_mode;
  ha_rows old_select_limit= thd->variables.select_limit;
  sp_rcontext *old_spcont= thd->spcont;

  char definer_user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING definer_user_name= { definer_user_name_holder, USERNAME_LENGTH };

  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING definer_host_name= { definer_host_name_holder, HOSTNAME_LENGTH };

  int ret;

  thd->variables.sql_mode= sql_mode;
  thd->variables.select_limit= HA_POS_ERROR;

  thd->lex= &newlex;
  newlex.current_select= NULL;

  parse_user(definer, strlen(definer),
             definer_user_name.str, &definer_user_name.length,
             definer_host_name.str, &definer_host_name.length);

  defstr.set_charset(system_charset_info);

  /*
    We have to add DEFINER clause and provide proper routine characterstics in
    routine definition statement that we build here to be able to use this
    definition for SHOW CREATE PROCEDURE later.
   */

  if (!create_string(thd, &defstr,
		     type,
		     name,
		     params, strlen(params),
		     returns, strlen(returns),
		     body, strlen(body),
		     &chistics, &definer_user_name, &definer_host_name))
  {
    ret= SP_INTERNAL_ERROR;
    goto end;
  }

  if ((ret= sp_use_new_db(thd, name->m_db, &old_db, 1, &dbchanged)))
    goto end;

  lex_start(thd, (uchar*)defstr.c_ptr(), defstr.length());

  thd->spcont= 0;
  if (MYSQLparse(thd) || thd->is_fatal_error || newlex.sphead == NULL)
  {
    sp_head *sp= newlex.sphead;

    if (dbchanged && (ret= mysql_change_db(thd, old_db.str, 1)))
      goto end;
    delete sp;
    ret= SP_PARSE_ERROR;
  }
  else
  {
    if (dbchanged && (ret= mysql_change_db(thd, old_db.str, 1)))
      goto end;
    *sphp= newlex.sphead;
    (*sphp)->set_definer(&definer_user_name, &definer_host_name);
    (*sphp)->set_info(created, modified, &chistics, sql_mode);
    (*sphp)->optimize();
  }
end:
  lex_end(thd->lex);
  thd->spcont= old_spcont;
  thd->variables.sql_mode= old_sql_mode;
  thd->variables.select_limit= old_select_limit;
  thd->lex= old_lex;
  return ret;
}


static void
sp_returns_type(THD *thd, String &result, sp_head *sp)
{
  TABLE table;
  TABLE_SHARE share;
  Field *field;
  bzero((char*) &table, sizeof(table));
  bzero((char*) &share, sizeof(share));
  table.in_use= thd;
  table.s = &share;
  field= sp->create_result_field(0, 0, &table);
  field->sql_type(result);
  delete field;
}

static int
db_create_routine(THD *thd, int type, sp_head *sp)
{
  int ret;
  TABLE *table;
  char definer[USER_HOST_BUFF_SIZE];
  char old_db_buf[NAME_LEN+1];
  LEX_STRING old_db= { old_db_buf, sizeof(old_db_buf) };
  bool dbchanged;
  DBUG_ENTER("db_create_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",type,sp->m_name.length,
                       sp->m_name.str));

  if ((ret= sp_use_new_db(thd, sp->m_db, &old_db, 0, &dbchanged)))
  {
    ret= SP_NO_DB_ERROR;
    goto done;
  }

  if (!(table= open_proc_table_for_update(thd)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    restore_record(table, s->default_values); // Get default values for fields

    /* NOTE: all needed privilege checks have been already done. */
    strxnmov(definer, sizeof(definer)-1, thd->lex->definer->user.str, "@",
            thd->lex->definer->host.str, NullS);

    if (table->s->fields != MYSQL_PROC_FIELD_COUNT)
    {
      ret= SP_GET_FIELD_FAILED;
      goto done;
    }

    if (system_charset_info->cset->numchars(system_charset_info,
                                            sp->m_name.str,
                                            sp->m_name.str+sp->m_name.length) >
        table->field[MYSQL_PROC_FIELD_NAME]->char_length())
    {
      ret= SP_BAD_IDENTIFIER;
      goto done;
    }
    if (sp->m_body.length > table->field[MYSQL_PROC_FIELD_BODY]->field_length)
    {
      ret= SP_BODY_TOO_LONG;
      goto done;
    }
    table->field[MYSQL_PROC_FIELD_DB]->
      store(sp->m_db.str, sp->m_db.length, system_charset_info);
    table->field[MYSQL_PROC_FIELD_NAME]->
      store(sp->m_name.str, sp->m_name.length, system_charset_info);
    table->field[MYSQL_PROC_FIELD_TYPE]->
      store((longlong)type, TRUE);
    table->field[MYSQL_PROC_FIELD_SPECIFIC_NAME]->
      store(sp->m_name.str, sp->m_name.length, system_charset_info);
    if (sp->m_chistics->daccess != SP_DEFAULT_ACCESS)
      table->field[MYSQL_PROC_FIELD_ACCESS]->
	store((longlong)sp->m_chistics->daccess, TRUE);
    table->field[MYSQL_PROC_FIELD_DETERMINISTIC]->
      store((longlong)(sp->m_chistics->detistic ? 1 : 2), TRUE);
    if (sp->m_chistics->suid != SP_IS_DEFAULT_SUID)
      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]->
	store((longlong)sp->m_chistics->suid, TRUE);
    table->field[MYSQL_PROC_FIELD_PARAM_LIST]->
      store(sp->m_params.str, sp->m_params.length, system_charset_info);
    if (sp->m_type == TYPE_ENUM_FUNCTION)
    {
      String retstr(64);
      sp_returns_type(thd, retstr, sp);
      table->field[MYSQL_PROC_FIELD_RETURNS]->
	store(retstr.ptr(), retstr.length(), system_charset_info);
    }
    table->field[MYSQL_PROC_FIELD_BODY]->
      store(sp->m_body.str, sp->m_body.length, system_charset_info);
    table->field[MYSQL_PROC_FIELD_DEFINER]->
      store(definer, (uint)strlen(definer), system_charset_info);
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_CREATED])->set_time();
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_MODIFIED])->set_time();
    table->field[MYSQL_PROC_FIELD_SQL_MODE]->
      store((longlong)thd->variables.sql_mode, TRUE);
    if (sp->m_chistics->comment.str)
      table->field[MYSQL_PROC_FIELD_COMMENT]->
	store(sp->m_chistics->comment.str, sp->m_chistics->comment.length,
	      system_charset_info);

    if ((sp->m_type == TYPE_ENUM_FUNCTION) &&
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
	  my_message(ER_BINLOG_UNSAFE_ROUTINE,
		     ER(ER_BINLOG_UNSAFE_ROUTINE), MYF(0));
	  ret= SP_INTERNAL_ERROR;
	  goto done;
	}
      }
      if (!(thd->security_ctx->master_access & SUPER_ACL))
      {
	my_message(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER,
		   ER(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER), MYF(0));
	ret= SP_INTERNAL_ERROR;
	goto done;
      }
    }

    ret= SP_OK;
    if (table->file->ha_write_row(table->record[0]))
      ret= SP_WRITE_ROW_FAILED;
    else if (mysql_bin_log.is_open())
    {
      thd->clear_error();

      String log_query;
      log_query.set_charset(system_charset_info);
      log_query.append(STRING_WITH_LEN("CREATE "));
      append_definer(thd, &log_query, &thd->lex->definer->user,
                     &thd->lex->definer->host);
      log_query.append(thd->lex->stmt_definition_begin);

      /* Such a statement can always go directly to binlog, no trans cache */
      thd->binlog_query(THD::MYSQL_QUERY_TYPE,
                        log_query.c_ptr(), log_query.length(), FALSE, FALSE);
    }

  }

done:
  close_thread_tables(thd);
  if (dbchanged)
    (void) mysql_change_db(thd, old_db.str, 1);
  DBUG_RETURN(ret);
}


static int
db_drop_routine(THD *thd, int type, sp_name *name)
{
  TABLE *table;
  int ret;
  DBUG_ENTER("db_drop_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, name->m_name.length, name->m_name.str));

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    if (table->file->ha_delete_row(table->record[0]))
      ret= SP_DELETE_ROW_FAILED;
  }
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}


static int
db_update_routine(THD *thd, int type, sp_name *name, st_sp_chistics *chistics)
{
  TABLE *table;
  int ret;
  DBUG_ENTER("db_update_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, name->m_name.length, name->m_name.str));

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    store_record(table,record[1]);
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_MODIFIED])->set_time();
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
    if ((table->file->ha_update_row(table->record[1],table->record[0])))
      ret= SP_WRITE_ROW_FAILED;
  }
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}


struct st_used_field
{
  const char *field_name;
  uint field_length;
  enum enum_field_types field_type;
  Field *field;
};

static struct st_used_field init_fields[]=
{
  { "Db",       NAME_LEN, MYSQL_TYPE_STRING,    0},
  { "Name",     NAME_LEN, MYSQL_TYPE_STRING,    0},
  { "Type",            9, MYSQL_TYPE_STRING,    0},
  { "Definer",        77, MYSQL_TYPE_STRING,    0},
  { "Modified",        0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Created",         0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Security_type",   1, MYSQL_TYPE_STRING,    0},
  { "Comment",  NAME_LEN, MYSQL_TYPE_STRING,    0},
  { 0,                 0, MYSQL_TYPE_STRING,    0}
};


static int
print_field_values(THD *thd, TABLE *table,
		   struct st_used_field *used_fields,
		   int type, const char *wild)
{
  Protocol *protocol= thd->protocol;

  if (table->field[MYSQL_PROC_FIELD_TYPE]->val_int() == type)
  {
    String db_string;
    String name_string;
    struct st_used_field *used_field= used_fields;

    if (get_field(thd->mem_root, used_field->field, &db_string))
      db_string.set_ascii("", 0);
    used_field+= 1;
    get_field(thd->mem_root, used_field->field, &name_string);

    if (!wild || !wild[0] || !wild_compare(name_string.ptr(), wild, 0))
    {
      protocol->prepare_for_resend();
      protocol->store(&db_string);
      protocol->store(&name_string);
      for (used_field++;
	   used_field->field_name;
	   used_field++)
      {
	switch (used_field->field_type) {
	case MYSQL_TYPE_TIMESTAMP:
	  {
	    TIME tmp_time;

	    bzero((char *)&tmp_time, sizeof(tmp_time));
	    ((Field_timestamp *) used_field->field)->get_time(&tmp_time);
	    protocol->store(&tmp_time);
	  }
	  break;
	default:
	  {
	    String tmp_string;

	    get_field(thd->mem_root, used_field->field, &tmp_string);
	    protocol->store(&tmp_string);
	  }
	  break;
	}
      }
      if (protocol->write())
	return SP_INTERNAL_ERROR;
    }
  }
  return SP_OK;
}


static int
db_show_routine_status(THD *thd, int type, const char *wild)
{
  TABLE *table;
  TABLE_LIST tables;
  int res;
  DBUG_ENTER("db_show_routine_status");

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.table_name= tables.alias= (char*)"proc";

  if (! (table= open_ltable(thd, &tables, TL_READ)))
  {
    res= SP_OPEN_TABLE_FAILED;
    goto done;
  }
  else
  {
    Item *item;
    List<Item> field_list;
    struct st_used_field *used_field;
    TABLE_LIST *leaves= 0;
    st_used_field used_fields[array_elements(init_fields)];

    table->use_all_columns();
    memcpy((char*) used_fields, (char*) init_fields, sizeof(used_fields));
    /* Init header */
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      switch (used_field->field_type) {
      case MYSQL_TYPE_TIMESTAMP:
	field_list.push_back(item=new Item_datetime(used_field->field_name));
	break;
      default:
	field_list.push_back(item=new Item_empty_string(used_field->field_name,
							used_field->
							field_length));
	break;
      }
    }
    /* Print header */
    if (thd->protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                                Protocol::SEND_EOF))
    {
      res= SP_INTERNAL_ERROR;
      goto err_case;
    }

    /*
      Init fields

      tables is not VIEW for sure => we can pass 0 as condition
    */
    thd->lex->select_lex.context.resolve_in_table_list_only(&tables);
    setup_tables(thd, &thd->lex->select_lex.context,
                 &thd->lex->select_lex.top_join_list,
                 &tables, &leaves, FALSE);
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      Item_field *field= new Item_field(&thd->lex->select_lex.context,
                                        "mysql", "proc",
					used_field->field_name);
      if (!field ||
          !(used_field->field= find_field_in_tables(thd, field, &tables, NULL,
						    0, REPORT_ALL_ERRORS, 1,
                                                    TRUE)))
      {
	res= SP_INTERNAL_ERROR;
	goto err_case1;
      }
    }

    table->file->ha_index_init(0, 1);
    if ((res= table->file->index_first(table->record[0])))
    {
      res= (res == HA_ERR_END_OF_FILE) ? 0 : SP_INTERNAL_ERROR;
      goto err_case1;
    }
    if ((res= print_field_values(thd, table, used_fields, type, wild)))
      goto err_case1;
    while (!table->file->index_next(table->record[0]))
    {
      if ((res= print_field_values(thd, table, used_fields, type, wild)))
	goto err_case1;
    }
    res= SP_OK;
  }

err_case1:
  send_eof(thd);
err_case:
  table->file->ha_index_end();
  close_thread_tables(thd);
done:
  DBUG_RETURN(res);
}


/* Drop all routines in database 'db' */
int
sp_drop_db_routines(THD *thd, char *db)
{
  TABLE *table;
  int ret;
  uint key_len;
  DBUG_ENTER("sp_drop_db_routines");
  DBUG_PRINT("enter", ("db: %s", db));

  ret= SP_OPEN_TABLE_FAILED;
  if (!(table= open_proc_table_for_update(thd)))
    goto err;

  table->field[MYSQL_PROC_FIELD_DB]->store(db, strlen(db), system_charset_info);
  key_len= table->key_info->key_part[0].store_length;

  ret= SP_OK;
  table->file->ha_index_init(0, 1);
  if (! table->file->index_read(table->record[0],
                                (byte *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
				key_len, HA_READ_KEY_EXACT))
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
    } while (! (nxtres= table->file->index_next_same(table->record[0],
                                (byte *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
						     key_len)));
    if (nxtres != HA_ERR_END_OF_FILE)
      ret= SP_KEY_NOT_FOUND;
    if (deleted)
      sp_cache_invalidate();
  }
  table->file->ha_index_end();

  close_thread_tables(thd);

err:
  DBUG_RETURN(ret);
}


/*****************************************************************************
  PROCEDURE
******************************************************************************/

/*
  Obtain object representing stored procedure/function by its name from
  stored procedures cache and looking into mysql.proc if needed.

  SYNOPSIS
    sp_find_routine()
      thd        - thread context
      type       - type of object (TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE)
      name       - name of procedure
      cp         - hash to look routine in
      cache_only - if true perform cache-only lookup
                   (Don't look in mysql.proc).

  RETURN VALUE
    Non-0 pointer to sp_head object for the procedure, or
    0 - in case of error.
*/

sp_head *
sp_find_routine(THD *thd, int type, sp_name *name, sp_cache **cp,
                bool cache_only)
{
  sp_head *sp;
  ulong depth= (type == TYPE_ENUM_PROCEDURE ?
                thd->variables.max_sp_recursion_depth :
                0);
  DBUG_ENTER("sp_find_routine");
  DBUG_PRINT("enter", ("name:  %.*s.%.*s, type: %d, cache only %d",
                       name->m_db.length, name->m_db.str,
                       name->m_name.length, name->m_name.str,
                       type, cache_only));

  if ((sp= sp_cache_lookup(cp, name)))
  {
    ulong level;
    sp_head *new_sp;
    const char *returns= "";
    char definer[USER_HOST_BUFF_SIZE];
    String retstr(64);

    DBUG_PRINT("info", ("found: 0x%lx", (ulong)sp));
    if (sp->m_first_free_instance)
    {
      DBUG_PRINT("info", ("first free: 0x%lx, level: %lu, flags %x",
                          (ulong)sp->m_first_free_instance,
                          sp->m_first_free_instance->m_recursion_level,
                          sp->m_first_free_instance->m_flags));
      DBUG_ASSERT(!(sp->m_first_free_instance->m_flags & sp_head::IS_INVOKED));
      if (sp->m_first_free_instance->m_recursion_level > depth)
      {
        sp->recursion_level_error(thd);
        DBUG_RETURN(0);
      }
      DBUG_RETURN(sp->m_first_free_instance);
    }
    level= sp->m_last_cached_sp->m_recursion_level + 1;
    if (level > depth)
    {
      sp->recursion_level_error(thd);
      DBUG_RETURN(0);
    }

    strxmov(definer, sp->m_definer_user.str, "@",
            sp->m_definer_host.str, NullS);
    if (type == TYPE_ENUM_FUNCTION)
    {
      sp_returns_type(thd, retstr, sp);
      returns= retstr.ptr();
    }
    if (db_load_routine(thd, type, name, &new_sp,
                        sp->m_sql_mode, sp->m_params.str, returns,
                        sp->m_body.str, *sp->m_chistics, definer,
                        sp->m_created, sp->m_modified) == SP_OK)
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


/*
  This is used by sql_acl.cc:mysql_routine_grant() and is used to find
  the routines in 'routines'.
*/

int
sp_exist_routines(THD *thd, TABLE_LIST *routines, bool any, bool no_error)
{
  TABLE_LIST *routine;
  bool result= 0;
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
    name= new sp_name(lex_db, lex_name);
    name->init_qname(thd);
    sp_object_found= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, name,
                                     &thd->sp_proc_cache, FALSE) != NULL ||
                     sp_find_routine(thd, TYPE_ENUM_FUNCTION, name,
                                     &thd->sp_func_cache, FALSE) != NULL;
    mysql_reset_errors(thd, TRUE);
    if (sp_object_found)
    {
      if (any)
        DBUG_RETURN(1);
      result= 1;
    }
    else if (!any)
    {
      if (!no_error)
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION or PROCEDURE", 
		 routine->table_name);
	DBUG_RETURN(-1);
      }
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(result);
}


/*
  Check if a routine exists in the mysql.proc table, without actually
  parsing the definition. (Used for dropping)

  SYNOPSIS
    sp_routine_exists_in_table()
      thd        - thread context
      name       - name of procedure

  RETURN VALUE
    0     - Success
    non-0 - Error;  SP_OPEN_TABLE_FAILED or SP_KEY_NOT_FOUND
*/

int
sp_routine_exists_in_table(THD *thd, int type, sp_name *name)
{
  TABLE *table;
  int ret;
  Open_tables_state open_tables_state_backup;

  if (!(table= open_proc_table_for_read(thd, &open_tables_state_backup)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    if ((ret= db_find_routine_aux(thd, type, name, table)) != SP_OK)
      ret= SP_KEY_NOT_FOUND;
    close_proc_table(thd, &open_tables_state_backup);
  }
  return ret;
}


int
sp_create_procedure(THD *thd, sp_head *sp)
{
  int ret;
  DBUG_ENTER("sp_create_procedure");
  DBUG_PRINT("enter", ("name: %.*s", sp->m_name.length, sp->m_name.str));

  ret= db_create_routine(thd, TYPE_ENUM_PROCEDURE, sp);
  DBUG_RETURN(ret);
}


int
sp_drop_procedure(THD *thd, sp_name *name)
{
  int ret;
  DBUG_ENTER("sp_drop_procedure");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  ret= db_drop_routine(thd, TYPE_ENUM_PROCEDURE, name);
  if (!ret)
    sp_cache_invalidate();
  DBUG_RETURN(ret);
}


int
sp_update_procedure(THD *thd, sp_name *name, st_sp_chistics *chistics)
{
  int ret;
  DBUG_ENTER("sp_update_procedure");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  ret= db_update_routine(thd, TYPE_ENUM_PROCEDURE, name, chistics);
  if (!ret)
    sp_cache_invalidate();
  DBUG_RETURN(ret);
}


int
sp_show_create_procedure(THD *thd, sp_name *name)
{
  sp_head *sp;
  DBUG_ENTER("sp_show_create_procedure");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  if ((sp= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, name,
                           &thd->sp_proc_cache, FALSE)))
  {
    int ret= sp->show_create_procedure(thd);

    DBUG_RETURN(ret);
  }

  DBUG_RETURN(SP_KEY_NOT_FOUND);
}


int
sp_show_status_procedure(THD *thd, const char *wild)
{
  int ret;
  DBUG_ENTER("sp_show_status_procedure");

  ret= db_show_routine_status(thd, TYPE_ENUM_PROCEDURE, wild);
  DBUG_RETURN(ret);
}


/*****************************************************************************
  FUNCTION
******************************************************************************/

int
sp_create_function(THD *thd, sp_head *sp)
{
  int ret;
  DBUG_ENTER("sp_create_function");
  DBUG_PRINT("enter", ("name: %.*s", sp->m_name.length, sp->m_name.str));

  ret= db_create_routine(thd, TYPE_ENUM_FUNCTION, sp);
  DBUG_RETURN(ret);
}


int
sp_drop_function(THD *thd, sp_name *name)
{
  int ret;
  DBUG_ENTER("sp_drop_function");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  ret= db_drop_routine(thd, TYPE_ENUM_FUNCTION, name);
  if (!ret)
    sp_cache_invalidate();
  DBUG_RETURN(ret);
}


int
sp_update_function(THD *thd, sp_name *name, st_sp_chistics *chistics)
{
  int ret;
  DBUG_ENTER("sp_update_procedure");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  ret= db_update_routine(thd, TYPE_ENUM_FUNCTION, name, chistics);
  if (!ret)
    sp_cache_invalidate();
  DBUG_RETURN(ret);
}


int
sp_show_create_function(THD *thd, sp_name *name)
{
  sp_head *sp;
  DBUG_ENTER("sp_show_create_function");
  DBUG_PRINT("enter", ("name: %.*s", name->m_name.length, name->m_name.str));

  if ((sp= sp_find_routine(thd, TYPE_ENUM_FUNCTION, name,
                           &thd->sp_func_cache, FALSE)))
  {
    int ret= sp->show_create_function(thd);

    DBUG_RETURN(ret);
  }
  DBUG_RETURN(SP_KEY_NOT_FOUND);
}


int
sp_show_status_function(THD *thd, const char *wild)
{
  int ret;
  DBUG_ENTER("sp_show_status_function");
  ret= db_show_routine_status(thd, TYPE_ENUM_FUNCTION, wild);
  DBUG_RETURN(ret);
}


/*
  Structure that represents element in the set of stored routines
  used by statement or routine.
*/
struct Sroutine_hash_entry;

struct Sroutine_hash_entry
{
  /* Set key consisting of one-byte routine type and quoted routine name. */
  LEX_STRING key;
  /*
    Next element in list linking all routines in set. See also comments
    for LEX::sroutine/sroutine_list and sp_head::m_sroutines.
  */
  Sroutine_hash_entry *next;
  /*
    Uppermost view which directly or indirectly uses this routine.
    0 if routine is not used in view. Note that it also can be 0 if
    statement uses routine both via view and directly.
  */
  TABLE_LIST *belong_to_view;
};


extern "C" byte* sp_sroutine_key(const byte *ptr, uint *plen, my_bool first)
{
  Sroutine_hash_entry *rn= (Sroutine_hash_entry *)ptr;
  *plen= rn->key.length;
  return (byte *)rn->key.str;
}


/*
  Check if
   - current statement (the one in thd->lex) needs table prelocking
   - first routine in thd->lex->sroutines_list needs to execute its body in
     prelocked mode.

  SYNOPSIS
    sp_get_prelocking_info()
      thd                  Current thread, thd->lex is the statement to be
                           checked.
      need_prelocking      OUT TRUE  - prelocked mode should be activated
                                       before executing the statement
                               FALSE - Don't activate prelocking 
      first_no_prelocking  OUT TRUE  - Tables used by first routine in
                                       thd->lex->sroutines_list should be
                                       prelocked.
                               FALSE - Otherwise.
  NOTES 
    This function assumes that for any "CALL proc(...)" statement routines_list 
    will have 'proc' as first element (it may have several, consider e.g.
    "proc(sp_func(...)))". This property is currently guaranted by the parser.
*/

void sp_get_prelocking_info(THD *thd, bool *need_prelocking, 
                            bool *first_no_prelocking)
{
  Sroutine_hash_entry *routine;
  routine= (Sroutine_hash_entry*)thd->lex->sroutines_list.first;

  DBUG_ASSERT(routine);
  bool first_is_procedure= (routine->key.str[0] == TYPE_ENUM_PROCEDURE);

  *first_no_prelocking= first_is_procedure;
  *need_prelocking= !first_is_procedure || test(routine->next);
}


/*
  Auxilary function that adds new element to the set of stored routines
  used by statement.

  SYNOPSIS
    add_used_routine()
      lex             LEX representing statement
      arena           Arena in which memory for new element will be allocated
      key             Key for the hash representing set
      belong_to_view  Uppermost view which uses this routine
                      (0 if routine is not used by view)

  NOTES
    Will also add element to end of 'LEX::sroutines_list' list.

    In case when statement uses stored routines but does not need
    prelocking (i.e. it does not use any tables) we will access the
    elements of LEX::sroutines set on prepared statement re-execution.
    Because of this we have to allocate memory for both hash element
    and copy of its key in persistent arena.

  TODO
    When we will got rid of these accesses on re-executions we will be
    able to allocate memory for hash elements in non-persitent arena
    and directly use key values from sp_head::m_sroutines sets instead
    of making their copies.

  RETURN VALUE
    TRUE  - new element was added.
    FALSE - element was not added (because it is already present in the set).
*/

static bool add_used_routine(LEX *lex, Query_arena *arena,
                             const LEX_STRING *key,
                             TABLE_LIST *belong_to_view)
{
  if (!hash_search(&lex->sroutines, (byte *)key->str, key->length))
  {
    Sroutine_hash_entry *rn=
      (Sroutine_hash_entry *)arena->alloc(sizeof(Sroutine_hash_entry) +
                                          key->length);
    if (!rn)              // OOM. Error will be reported using fatal_error().
      return FALSE;
    rn->key.length= key->length;
    rn->key.str= (char *)rn + sizeof(Sroutine_hash_entry);
    memcpy(rn->key.str, key->str, key->length);
    my_hash_insert(&lex->sroutines, (byte *)rn);
    lex->sroutines_list.link_in_list((byte *)rn, (byte **)&rn->next);
    rn->belong_to_view= belong_to_view;
    return TRUE;
  }
  return FALSE;
}


/*
  Add routine which is explicitly used by statement to the set of stored
  routines used by this statement.

  SYNOPSIS
    sp_add_used_routine()
      lex     - LEX representing statement
      arena   - arena in which memory for new element of the set
                will be allocated
      rt      - routine name
      rt_type - routine type (one of TYPE_ENUM_PROCEDURE/...)

  NOTES
    Will also add element to end of 'LEX::sroutines_list' list (and will
    take into account that this is explicitly used routine).

    To be friendly towards prepared statements one should pass
    persistent arena as second argument.
*/

void sp_add_used_routine(LEX *lex, Query_arena *arena,
                         sp_name *rt, char rt_type)
{
  rt->set_routine_type(rt_type);
  (void)add_used_routine(lex, arena, &rt->m_sroutines_key, 0);
  lex->sroutines_list_own_last= lex->sroutines_list.next;
  lex->sroutines_list_own_elements= lex->sroutines_list.elements;
}


/*
  Remove routines which are only indirectly used by statement from
  the set of routines used by this statement.

  SYNOPSIS
    sp_remove_not_own_routines()
      lex  LEX representing statement
*/

void sp_remove_not_own_routines(LEX *lex)
{
  Sroutine_hash_entry *not_own_rt, *next_rt;
  for (not_own_rt= *(Sroutine_hash_entry **)lex->sroutines_list_own_last;
       not_own_rt; not_own_rt= next_rt)
  {
    /*
      It is safe to obtain not_own_rt->next after calling hash_delete() now
      but we want to be more future-proof.
    */
    next_rt= not_own_rt->next;
    hash_delete(&lex->sroutines, (byte *)not_own_rt);
  }

  *(Sroutine_hash_entry **)lex->sroutines_list_own_last= NULL;
  lex->sroutines_list.next= lex->sroutines_list_own_last;
  lex->sroutines_list.elements= lex->sroutines_list_own_elements;
}


/*
  Merge contents of two hashes representing sets of routines used
  by statements or by other routines.

  SYNOPSIS
    sp_update_sp_used_routines()
      dst - hash to which elements should be added
      src - hash from which elements merged

  NOTE
    This procedure won't create new Sroutine_hash_entry objects,
    instead it will simply add elements from source to destination
    hash. Thus time of life of elements in destination hash becomes
    dependant on time of life of elements from source hash. It also
    won't touch lists linking elements in source and destination
    hashes.
*/

void sp_update_sp_used_routines(HASH *dst, HASH *src)
{
  for (uint i=0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)hash_element(src, i);
    if (!hash_search(dst, (byte *)rt->key.str, rt->key.length))
      my_hash_insert(dst, (byte *)rt);
  }
}


/*
  Add contents of hash representing set of routines to the set of
  routines used by statement.

  SYNOPSIS
    sp_update_stmt_used_routines()
      thd             Thread context
      lex             LEX representing statement
      src             Hash representing set from which routines will be added
      belong_to_view  Uppermost view which uses these routines, 0 if none

  NOTE
    It will also add elements to end of 'LEX::sroutines_list' list.
*/

static void
sp_update_stmt_used_routines(THD *thd, LEX *lex, HASH *src,
                             TABLE_LIST *belong_to_view)
{
  for (uint i=0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)hash_element(src, i);
    (void)add_used_routine(lex, thd->stmt_arena, &rt->key, belong_to_view);
  }
}


/*
  Add contents of list representing set of routines to the set of
  routines used by statement.

  SYNOPSIS
    sp_update_stmt_used_routines()
      thd             Thread context
      lex             LEX representing statement
      src             List representing set from which routines will be added
      belong_to_view  Uppermost view which uses these routines, 0 if none

  NOTE
    It will also add elements to end of 'LEX::sroutines_list' list.
*/

static void sp_update_stmt_used_routines(THD *thd, LEX *lex, SQL_LIST *src,
                                         TABLE_LIST *belong_to_view)
{
  for (Sroutine_hash_entry *rt= (Sroutine_hash_entry *)src->first;
       rt; rt= rt->next)
    (void)add_used_routine(lex, thd->stmt_arena, &rt->key, belong_to_view);
}


/*
  Cache sub-set of routines used by statement, add tables used by these
  routines to statement table list. Do the same for all routines used
  by these routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_aux()
      thd              - thread context
      lex              - LEX representing statement
      start            - first routine from the list of routines to be cached
                         (this list defines mentioned sub-set).
      first_no_prelock - If true, don't add tables or cache routines used by
                         the body of the first routine (i.e. *start)
                         will be executed in non-prelocked mode.
  NOTE
    If some function is missing this won't be reported here.
    Instead this fact will be discovered during query execution.

  RETURN VALUE
     0     - success
     non-0 - failure
*/

static int
sp_cache_routines_and_add_tables_aux(THD *thd, LEX *lex,
                                     Sroutine_hash_entry *start, 
                                     bool first_no_prelock)
{
  int ret= 0;
  bool first= TRUE;
  DBUG_ENTER("sp_cache_routines_and_add_tables_aux");

  for (Sroutine_hash_entry *rt= start; rt; rt= rt->next)
  {
    sp_name name(rt->key.str, rt->key.length);
    int type= rt->key.str[0];
    sp_head *sp;

    if (!(sp= sp_cache_lookup((type == TYPE_ENUM_FUNCTION ?
                              &thd->sp_func_cache : &thd->sp_proc_cache),
                              &name)))
    {
      name.m_name.str= strchr(name.m_qname.str, '.');
      name.m_db.length= name.m_name.str - name.m_qname.str;
      name.m_db.str= strmake_root(thd->mem_root, name.m_qname.str,
                                  name.m_db.length);
      name.m_name.str+= 1;
      name.m_name.length= name.m_qname.length - name.m_db.length - 1;

      switch ((ret= db_find_routine(thd, type, &name, &sp)))
      {
      case SP_OK:
        {
          if (type == TYPE_ENUM_FUNCTION)
            sp_cache_insert(&thd->sp_func_cache, sp);
          else
            sp_cache_insert(&thd->sp_proc_cache, sp);
        }
        break;
      case SP_KEY_NOT_FOUND:
        ret= SP_OK;
        break;
      case SP_OPEN_TABLE_FAILED:
        /*
          Force it to attempt opening it again on subsequent calls;
          otherwise we will get one error message the first time, and
          then ER_SP_PROC_TABLE_CORRUPT (below) on subsequent tries.
        */
        mysql_proc_table_exists= 1;
        /* Fall through */
      default:
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
        if (!thd->net.report_error)
        {
          char n[NAME_LEN*2+2];

          /* m_qname.str is not always \0 terminated */
          memcpy(n, name.m_qname.str, name.m_qname.length);
          n[name.m_qname.length]= '\0';
          my_error(ER_SP_PROC_TABLE_CORRUPT, MYF(0), n, ret);
        }
        break;
      }
    }
    if (sp)
    {
      if (!(first && first_no_prelock))
      {
        sp_update_stmt_used_routines(thd, lex, &sp->m_sroutines,
                                     rt->belong_to_view);
        (void)sp->add_used_tables_to_table_list(thd, &lex->query_tables_last,
                                                rt->belong_to_view);
      }
      sp->propagate_attributes(lex);
    }
    first= FALSE;
  }
  DBUG_RETURN(ret);
}


/*
  Cache all routines from the set of used by statement, add tables used
  by those routines to statement table list. Do the same for all routines
  used by those routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables()
      thd              - thread context
      lex              - LEX representing statement
      first_no_prelock - If true, don't add tables or cache routines used by
                         the body of the first routine (i.e. *start)

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables(THD *thd, LEX *lex, bool first_no_prelock)
{
  return sp_cache_routines_and_add_tables_aux(thd, lex,
           (Sroutine_hash_entry *)lex->sroutines_list.first,
           first_no_prelock);
}


/*
  Add all routines used by view to the set of routines used by statement.
  Add tables used by those routines to statement table list. Do the same
  for all routines used by these routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_for_view()
      thd   Thread context
      lex   LEX representing statement
      view  Table list element representing view

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables_for_view(THD *thd, LEX *lex, TABLE_LIST *view)
{
  Sroutine_hash_entry **last_cached_routine_ptr=
                          (Sroutine_hash_entry **)lex->sroutines_list.next;
  sp_update_stmt_used_routines(thd, lex, &view->view->sroutines_list,
                               view->top_table());
  return sp_cache_routines_and_add_tables_aux(thd, lex,
                                              *last_cached_routine_ptr, FALSE);
}


/*
  Add triggers for table to the set of routines used by statement.
  Add tables used by them to statement table list. Do the same for
  all implicitly used routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_for_triggers()
      thd    thread context
      lex    LEX respresenting statement
      table  Table list element for table with trigger

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables_for_triggers(THD *thd, LEX *lex,
                                              TABLE_LIST *table)
{
  int ret= 0;
  Table_triggers_list *triggers= table->table->triggers;
  if (add_used_routine(lex, thd->stmt_arena, &triggers->sroutines_key,
                       table->belong_to_view))
  {
    Sroutine_hash_entry **last_cached_routine_ptr=
                            (Sroutine_hash_entry **)lex->sroutines_list.next;
    for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
    {
      for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
      {
        sp_head *trigger_body= triggers->bodies[i][j];
        if (trigger_body)
        {
          (void)trigger_body->
            add_used_tables_to_table_list(thd, &lex->query_tables_last,
                                          table->belong_to_view);
          sp_update_stmt_used_routines(thd, lex,
                                       &trigger_body->m_sroutines,
                                       table->belong_to_view);
          trigger_body->propagate_attributes(lex);
        }
      }
    }
    ret= sp_cache_routines_and_add_tables_aux(thd, lex,
                                              *last_cached_routine_ptr, FALSE);
  }
  return ret;
}


/*
 * Generates the CREATE... string from the table information.
 * Returns TRUE on success, FALSE on (alloc) failure.
 */
static bool
create_string(THD *thd, String *buf,
	      int type,
	      sp_name *name,
	      const char *params, ulong paramslen,
	      const char *returns, ulong returnslen,
	      const char *body, ulong bodylen,
	      st_sp_chistics *chistics,
              const LEX_STRING *definer_user,
              const LEX_STRING *definer_host)
{
  /* Make some room to begin with */
  if (buf->alloc(100 + name->m_qname.length + paramslen + returnslen + bodylen +
		 chistics->comment.length + 10 /* length of " DEFINER= "*/ +
                 USER_HOST_BUFF_SIZE))
    return FALSE;

  buf->append(STRING_WITH_LEN("CREATE "));
  append_definer(thd, buf, definer_user, definer_host);
  if (type == TYPE_ENUM_FUNCTION)
    buf->append(STRING_WITH_LEN("FUNCTION "));
  else
    buf->append(STRING_WITH_LEN("PROCEDURE "));
  append_identifier(thd, buf, name->m_name.str, name->m_name.length);
  buf->append('(');
  buf->append(params, paramslen);
  buf->append(')');
  if (type == TYPE_ENUM_FUNCTION)
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
  return TRUE;
}



/*
  Change the current database if needed.

  SYNOPSIS
    sp_use_new_db()
      thd            thread handle

      new_db         new database name (a string and its length)

      old_db         [IN] str points to a buffer where to store the old
                          database, length contains the size of the buffer
                     [OUT] if old db was not NULL, its name is copied
                     to the buffer pointed at by str and length is updated
                     accordingly. Otherwise str[0] is set to '\0' and length
                     is set to 0. The out parameter should be used only if
                     the database name has been changed (see dbchangedp).

     dbchangedp      [OUT] is set to TRUE if the current database is changed,
                     FALSE otherwise. A database is not changed if the old
                     name is the same as the new one, both names are empty,
                     or an error has occurred.

  RETURN VALUE
    0                success
    1                access denied or out of memory (the error message is
                     set in THD)
*/

int
sp_use_new_db(THD *thd, LEX_STRING new_db, LEX_STRING *old_db,
	      bool no_access_check, bool *dbchangedp)
{
  int ret;
  DBUG_ENTER("sp_use_new_db");
  DBUG_PRINT("enter", ("newdb: %s", new_db.str));

  /*
    Set new_db to an empty string if it's NULL, because mysql_change_db
    requires a non-NULL argument.
    new_db.str can be NULL only if we're restoring the old database after
    execution of a stored procedure and there were no current database
    selected. The stored procedure itself must always have its database
    initialized.
  */
  if (new_db.str == NULL)
    new_db.str= empty_c_string;

  if (thd->db)
  {
    old_db->length= (strmake(old_db->str, thd->db, old_db->length) -
                     old_db->str);
  }
  else
  {
    old_db->str[0]= '\0';
    old_db->length= 0;
  }

  /* Don't change the database if the new name is the same as the old one. */
  if (my_strcasecmp(system_charset_info, old_db->str, new_db.str) == 0)
  {
    *dbchangedp= FALSE;
    DBUG_RETURN(0);
  }

  ret= mysql_change_db(thd, new_db.str, no_access_check);

  *dbchangedp= ret == 0;
  DBUG_RETURN(ret);
}

