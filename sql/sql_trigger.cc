/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */


#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "parse_file.h"

/*************************************************************************/

template <class T>
inline T *alloc_type(MEM_ROOT *m)
{
  return (T *) alloc_root(m, sizeof (T));
}

/*
  NOTE: Since alloc_type() is declared as inline, alloc_root() calls should
  be inlined by the compiler. So, implementation of alloc_root() is not
  needed. However, let's put the implementation in object file just in case
  of stupid MS or other old compilers.
*/

template LEX_STRING *alloc_type<LEX_STRING>(MEM_ROOT *m);
template ulonglong *alloc_type<ulonglong>(MEM_ROOT *m);

inline LEX_STRING *alloc_lex_string(MEM_ROOT *m)
{
  return alloc_type<LEX_STRING>(m);
}

/*************************************************************************/
/**
  Trigger_creation_ctx -- creation context of triggers.
*/

class Trigger_creation_ctx : public Stored_program_creation_ctx,
                             public Sql_alloc
{
public:
  static Trigger_creation_ctx *create(THD *thd,
                                      const char *db_name,
                                      const char *table_name,
                                      const LEX_STRING *client_cs_name,
                                      const LEX_STRING *connection_cl_name,
                                      const LEX_STRING *db_cl_name);

public:
  virtual Stored_program_creation_ctx *clone(MEM_ROOT *mem_root)
  {
    return new (mem_root) Trigger_creation_ctx(m_client_cs,
                                               m_connection_cl,
                                               m_db_cl);
  }

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const
  {
    return new Trigger_creation_ctx(thd);
  }

private:
  Trigger_creation_ctx(THD *thd)
    :Stored_program_creation_ctx(thd)
  { }

  Trigger_creation_ctx(CHARSET_INFO *client_cs,
                       CHARSET_INFO *connection_cl,
                       CHARSET_INFO *db_cl)
    :Stored_program_creation_ctx(client_cs, connection_cl, db_cl)
  { }
};

/**************************************************************************
  Trigger_creation_ctx implementation.
**************************************************************************/

Trigger_creation_ctx *
Trigger_creation_ctx::create(THD *thd,
                             const char *db_name,
                             const char *table_name,
                             const LEX_STRING *client_cs_name,
                             const LEX_STRING *connection_cl_name,
                             const LEX_STRING *db_cl_name)
{
  CHARSET_INFO *client_cs;
  CHARSET_INFO *connection_cl;
  CHARSET_INFO *db_cl;

  bool invalid_creation_ctx= FALSE;

  if (resolve_charset(client_cs_name->str,
                      thd->variables.character_set_client,
                      &client_cs))
  {
    sql_print_warning("Trigger for table '%s'.'%s': "
                      "invalid character_set_client value (%s).",
                      (const char *) db_name,
                      (const char *) table_name,
                      (const char *) client_cs_name->str);

    invalid_creation_ctx= TRUE;
  }

  if (resolve_collation(connection_cl_name->str,
                        thd->variables.collation_connection,
                        &connection_cl))
  {
    sql_print_warning("Trigger for table '%s'.'%s': "
                      "invalid collation_connection value (%s).",
                      (const char *) db_name,
                      (const char *) table_name,
                      (const char *) connection_cl_name->str);

    invalid_creation_ctx= TRUE;
  }

  if (resolve_collation(db_cl_name->str, NULL, &db_cl))
  {
    sql_print_warning("Trigger for table '%s'.'%s': "
                      "invalid database_collation value (%s).",
                      (const char *) db_name,
                      (const char *) table_name,
                      (const char *) db_cl_name->str);

    invalid_creation_ctx= TRUE;
  }

  if (invalid_creation_ctx)
  {
    push_warning_printf(thd,
                        MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRG_INVALID_CREATION_CTX,
                        ER(ER_TRG_INVALID_CREATION_CTX),
                        (const char *) db_name,
                        (const char *) table_name);
  }

  /*
    If we failed to resolve the database collation, load the default one
    from the disk.
  */

  if (!db_cl)
    db_cl= get_default_db_collation(thd, db_name);

  return new Trigger_creation_ctx(client_cs, connection_cl, db_cl);
}

/*************************************************************************/

static const LEX_STRING triggers_file_type=
  { C_STRING_WITH_LEN("TRIGGERS") };

const char * const TRG_EXT= ".TRG";

/*
  Table of .TRG file field descriptors.
  We have here only one field now because in nearest future .TRG
  files will be merged into .FRM files (so we don't need something
  like md5 or created fields).
*/
static File_option triggers_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("triggers") },
    my_offsetof(class Table_triggers_list, definitions_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("sql_modes") },
    my_offsetof(class Table_triggers_list, definition_modes_list),
    FILE_OPTIONS_ULLLIST
  },
  {
    { C_STRING_WITH_LEN("definers") },
    my_offsetof(class Table_triggers_list, definers_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("client_cs_names") },
    my_offsetof(class Table_triggers_list, client_cs_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("connection_cl_names") },
    my_offsetof(class Table_triggers_list, connection_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("db_cl_names") },
    my_offsetof(class Table_triggers_list, db_cl_names),
    FILE_OPTIONS_STRLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

File_option sql_modes_parameters=
{
  { C_STRING_WITH_LEN("sql_modes") },
  my_offsetof(class Table_triggers_list, definition_modes_list),
  FILE_OPTIONS_ULLLIST
};

/*
  This must be kept up to date whenever a new option is added to the list
  above, as it specifies the number of required parameters of the trigger in
  .trg file.
*/

static const int TRG_NUM_REQUIRED_PARAMETERS= 6;

/*
  Structure representing contents of .TRN file which are used to support
  database wide trigger namespace.
*/

struct st_trigname
{
  LEX_STRING trigger_table;
};

static const LEX_STRING trigname_file_type=
  { C_STRING_WITH_LEN("TRIGGERNAME") };

const char * const TRN_EXT= ".TRN";

static File_option trigname_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("trigger_table")},
    offsetof(struct st_trigname, trigger_table),
    FILE_OPTIONS_ESTRING
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};


const LEX_STRING trg_action_time_type_names[]=
{
  { C_STRING_WITH_LEN("BEFORE") },
  { C_STRING_WITH_LEN("AFTER") }
};

const LEX_STRING trg_event_type_names[]=
{
  { C_STRING_WITH_LEN("INSERT") },
  { C_STRING_WITH_LEN("UPDATE") },
  { C_STRING_WITH_LEN("DELETE") }
};


class Handle_old_incorrect_sql_modes_hook: public Unknown_key_hook
{
private:
  char *path;
public:
  Handle_old_incorrect_sql_modes_hook(char *file_path)
    :path(file_path)
  {};
  virtual bool process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end);
};


class Handle_old_incorrect_trigger_table_hook: public Unknown_key_hook
{
public:
  Handle_old_incorrect_trigger_table_hook(char *file_path,
                                          LEX_STRING *trigger_table_arg)
    :path(file_path), trigger_table_value(trigger_table_arg)
  {};
  virtual bool process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end);
private:
  char *path;
  LEX_STRING *trigger_table_value;
};


/*
  Create or drop trigger for table.

  SYNOPSIS
    mysql_create_or_drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one table for which trigger is created.
      create - whenever we create (TRUE) or drop (FALSE) trigger

  NOTE
    This function is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation/dropping is done inside Table_triggers_list
    methods.

  RETURN VALUE
    FALSE Success
    TRUE  error
*/
bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create)
{
  /*
    FIXME: The code below takes too many different paths depending on the
    'create' flag, so that the justification for a single function
    'mysql_create_or_drop_trigger', compared to two separate functions
    'mysql_create_trigger' and 'mysql_drop_trigger' is not apparent.
    This is a good candidate for a minor refactoring.
  */
  TABLE *table;
  bool result= TRUE;
  String stmt_query;

  DBUG_ENTER("mysql_create_or_drop_trigger");

  /* Charset of the buffer for statement must be system one. */
  stmt_query.set_charset(system_charset_info);

  /*
    QQ: This function could be merged in mysql_alter_table() function
    But do we want this ?
  */

  /*
    Note that once we will have check for TRIGGER privilege in place we won't
    need second part of condition below, since check_access() function also
    checks that db is specified.
  */
  if (!thd->lex->spname->m_db.length || create && !tables->db_length)
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    We don't allow creating triggers on tables in the 'mysql' schema
  */
  if (create && !my_strcasecmp(system_charset_info, "mysql", tables->db))
  {
    my_error(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    There is no DETERMINISTIC clause for triggers, so can't check it.
    But a trigger can in theory be used to do nasty things (if it supported
    DROP for example) so we do the check for privileges. For now there is
    already a stronger test right above; but when this stronger test will
    be removed, the test below will hold. Because triggers have the same
    nature as functions regarding binlogging: their body is implicitly
    binlogged, so they share the same danger, so trust_function_creators
    applies to them too.
  */
  if (!trust_function_creators && mysql_bin_log.is_open() &&
      !(thd->security_ctx->master_access & SUPER_ACL))
  {
    my_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    We don't want perform our operations while global read lock is held
    so we have to wait until its end and then prevent it from occurring
    again until we are done. (Acquiring LOCK_open is not enough because
    global read lock is held without holding LOCK_open).
  */
  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(TRUE);

  VOID(pthread_mutex_lock(&LOCK_open));

  if (!create)
  {
    bool if_exists= thd->lex->drop_if_exists;

    if (add_table_for_trigger(thd, thd->lex->spname, if_exists, & tables))
      goto end;

    if (!tables)
    {
      DBUG_ASSERT(if_exists);
      /*
        Since the trigger does not exist, there is no associated table,
        and therefore :
        - no TRIGGER privileges to check,
        - no trigger to drop,
        - no table to lock/modify,
        so the drop statement is successful.
      */
      result= FALSE;
      /* Still, we need to log the query ... */
      stmt_query.append(thd->query, thd->query_length);
      goto end;
    }
  }

  /*
    Check that the user has TRIGGER privilege on the subject table.
  */
  {
    bool err_status;
    TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
    thd->lex->query_tables_own_last= 0;

    err_status= check_table_access(thd, TRIGGER_ACL, tables, 0);

    thd->lex->query_tables_own_last= save_query_tables_own_last;

    if (err_status)
      goto end;
  }

  /* We should have only one table in table list. */
  DBUG_ASSERT(tables->next_global == 0);

  /* We do not allow creation of triggers on temporary tables. */
  if (create && find_temporary_table(thd, tables->db, tables->table_name))
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), tables->alias);
    goto end;
  }

  if (lock_table_names(thd, tables))
    goto end;

  /*
    If the table is under LOCK TABLES, lock_table_names() does not set
    tables->table. Find the table in open_tables.
  */
  if (!tables->table && thd->locked_tables)
  {
    for (table= thd->open_tables;
         table && (strcmp(table->s->table_name.str, tables->table_name) ||
                   strcmp(table->s->db.str, tables->db));
         table= table->next) {}
    tables->table= table;
  }
  if (!tables->table)
  {
    /* purecov: begin inspected */
    my_error(ER_TABLE_NOT_LOCKED, MYF(0), tables->alias);
    goto end;
    /* purecov: end */
  }

  /* No need to reopen the table if it is locked with LOCK TABLES. */
  if (!thd->locked_tables || (tables->table->in_use != thd))
  {
    /* We also don't allow creation of triggers on views. */
    tables->required_type= FRMTYPE_TABLE;

    if (reopen_name_locked_table(thd, tables, TRUE))
    {
      unlock_table_name(thd, tables);
      goto end;
    }
  }
  table= tables->table;

  if (!table->triggers)
  {
    if (!create)
    {
      my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
      goto end;
    }

    if (!(table->triggers= new (&table->mem_root) Table_triggers_list(table)))
      goto end;
  }

  result= (create ?
           table->triggers->create_trigger(thd, tables, &stmt_query):
           table->triggers->drop_trigger(thd, tables, &stmt_query));

  /* Under LOCK TABLES we must reopen the table to activate the trigger. */
  if (!result && thd->locked_tables)
  {
    /*
      Must not use table->s->db.str or table->s->table_name.str here.
      The strings are used in a loop even after the share may be freed.
    */
    close_data_files_and_morph_locks(thd, tables->db, tables->table_name);
    thd->in_lock_tables= 1;
    result= reopen_tables(thd, 1, 0);
    thd->in_lock_tables= 0;
  }

end:

  if (!result)
  {
    write_bin_log(thd, TRUE, stmt_query.ptr(), stmt_query.length());
  }

  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);

  if (!result)
    send_ok(thd);

  DBUG_RETURN(result);
}


/*
  Create trigger for table.

  SYNOPSIS
    create_trigger()
      thd          - current thread context (including trigger definition in
                     LEX)
      tables       - table list containing one open table for which the
                     trigger is created.
      stmt_query   - [OUT] after successful return, this string contains
                     well-formed statement for creating this trigger.

  NOTE
    - Assumes that trigger name is fully qualified.
    - NULL-string means the following LEX_STRING instance:
      { str = 0; length = 0 }.
    - In other words, definer_user and definer_host should contain
      simultaneously NULL-strings (non-SUID/old trigger) or valid strings
      (SUID/new trigger).

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::create_trigger(THD *thd, TABLE_LIST *tables,
                                         String *stmt_query)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  char file_buff[FN_REFLEN], trigname_buff[FN_REFLEN];
  LEX_STRING file, trigname_file;
  LEX_STRING *trg_def;
  LEX_STRING definer_user;
  LEX_STRING definer_host;
  ulonglong *trg_sql_mode;
  char trg_definer_holder[USER_HOST_BUFF_SIZE];
  LEX_STRING *trg_definer;
  Item_trigger_field *trg_field;
  struct st_trigname trigname;
  LEX_STRING *trg_client_cs_name;
  LEX_STRING *trg_connection_cl_name;
  LEX_STRING *trg_db_cl_name;


  /* Trigger must be in the same schema as target table. */
  if (my_strcasecmp(table_alias_charset, table->s->db.str,
                    lex->spname->m_db.str))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return 1;
  }

  /* We don't allow creation of several triggers of the same type yet */
  if (bodies[lex->trg_chistics.event][lex->trg_chistics.action_time] != 0)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "multiple triggers with the same action time"
             " and event for one table");
    return 1;
  }

  if (!lex->definer)
  {
    /*
      DEFINER-clause is missing.

      If we are in slave thread, this means that we received CREATE TRIGGER
      from the master, that does not support definer in triggers. So, we
      should mark this trigger as non-SUID. Note that this does not happen
      when we parse triggers' definitions during opening .TRG file.
      LEX::definer is ignored in that case.

      Otherwise, we should use CURRENT_USER() as definer.

      NOTE: when CREATE TRIGGER statement is allowed to be executed in PS/SP,
      it will be required to create the definer below in persistent MEM_ROOT
      of PS/SP.
    */

    if (!thd->slave_thread)
    {
      if (!(lex->definer= create_default_definer(thd)))
        return 1;
    }
  }

  /*
    If the specified definer differs from the current user, we should check
    that the current user has SUPER privilege (in order to create trigger
    under another user one must have SUPER privilege).
  */
  
  if (lex->definer &&
      (strcmp(lex->definer->user.str, thd->security_ctx->priv_user) ||
       my_strcasecmp(system_charset_info,
                     lex->definer->host.str,
                     thd->security_ctx->priv_host)))
  {
    if (check_global_access(thd, SUPER_ACL))
    {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
      return TRUE;
    }
  }

  /*
    Let us check if all references to fields in old/new versions of row in
    this trigger are ok.

    NOTE: We do it here more from ease of use standpoint. We still have to
    do some checks on each execution. E.g. we can catch privilege changes
    only during execution. Also in near future, when we will allow access
    to other tables from trigger we won't be able to catch changes in other
    tables...

    Since we don't plan to access to contents of the fields it does not
    matter that we choose for both OLD and NEW values the same versions
    of Field objects here.
  */
  old_field= new_field= table->field;

  for (trg_field= (Item_trigger_field *)(lex->trg_table_fields.first);
       trg_field; trg_field= trg_field->next_trg_field)
  {
    /*
      NOTE: now we do not check privileges at CREATE TRIGGER time. This will
      be changed in the future.
    */
    trg_field->setup_field(thd, table, NULL);

    if (!trg_field->fixed &&
        trg_field->fix_fields(thd, (Item **)0))
      return 1;
  }

  /*
    Here we are creating file with triggers and save all triggers in it.
    sql_create_definition_file() files handles renaming and backup of older
    versions
  */
  file.length= build_table_filename(file_buff, FN_REFLEN - 1,
                                    tables->db, tables->table_name,
                                    TRG_EXT, 0);
  file.str= file_buff;
  trigname_file.length= build_table_filename(trigname_buff, FN_REFLEN-1,
                                             tables->db,
                                             lex->spname->m_name.str,
                                             TRN_EXT, 0);
  trigname_file.str= trigname_buff;

  /* Use the filesystem to enforce trigger namespace constraints. */
  if (!access(trigname_buff, F_OK))
  {
    my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
    return 1;
  }

  trigname.trigger_table.str= tables->table_name;
  trigname.trigger_table.length= tables->table_name_length;

  if (sql_create_definition_file(NULL, &trigname_file, &trigname_file_type,
                                 (uchar*)&trigname, trigname_file_parameters, 0))
    return 1;

  /*
    Soon we will invalidate table object and thus Table_triggers_list object
    so don't care about place to which trg_def->ptr points and other
    invariants (e.g. we don't bother to update names_list)

    QQ: Hmm... probably we should not care about setting up active thread
        mem_root too.
  */
  if (!(trg_def= alloc_lex_string(&table->mem_root)) ||
      definitions_list.push_back(trg_def, &table->mem_root) ||

      !(trg_sql_mode= alloc_type<ulonglong>(&table->mem_root)) ||
      definition_modes_list.push_back(trg_sql_mode, &table->mem_root) ||

      !(trg_definer= alloc_lex_string(&table->mem_root)) ||
      definers_list.push_back(trg_definer, &table->mem_root) ||

      !(trg_client_cs_name= alloc_lex_string(&table->mem_root)) ||
      client_cs_names.push_back(trg_client_cs_name, &table->mem_root) ||

      !(trg_connection_cl_name= alloc_lex_string(&table->mem_root)) ||
      connection_cl_names.push_back(trg_connection_cl_name, &table->mem_root) ||

      !(trg_db_cl_name= alloc_lex_string(&table->mem_root)) ||
      db_cl_names.push_back(trg_db_cl_name, &table->mem_root))
  {
    goto err_with_cleanup;
  }

  *trg_sql_mode= thd->variables.sql_mode;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (lex->definer && !is_acl_user(lex->definer->host.str,
                                   lex->definer->user.str))
  {
    push_warning_printf(thd,
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  if (lex->definer)
  {
    /* SUID trigger. */

    definer_user= lex->definer->user;
    definer_host= lex->definer->host;

    trg_definer->str= trg_definer_holder;
    trg_definer->length= strxmov(trg_definer->str, definer_user.str, "@",
                                 definer_host.str, NullS) - trg_definer->str;
  }
  else
  {
    /* non-SUID trigger. */

    definer_user.str= 0;
    definer_user.length= 0;

    definer_host.str= 0;
    definer_host.length= 0;

    trg_definer->str= (char*) "";
    trg_definer->length= 0;
  }

  /*
    Fill character set information:
      - client character set contains charset info only;
      - connection collation contains pair {character set, collation};
      - database collation contains pair {character set, collation};
  */

  lex_string_set(trg_client_cs_name, thd->charset()->csname);

  lex_string_set(trg_connection_cl_name,
                 thd->variables.collation_connection->name);

  lex_string_set(trg_db_cl_name,
                 get_default_db_collation(thd, tables->db)->name);

  /*
    Create well-formed trigger definition query. Original query is not
    appropriated, because definer-clause can be not truncated.
  */

  stmt_query->append(STRING_WITH_LEN("CREATE "));

  if (trg_definer)
  {
    /*
      Append definer-clause if the trigger is SUID (a usual trigger in
      new MySQL versions).
    */

    append_definer(thd, stmt_query, &definer_user, &definer_host);
  }

  LEX_STRING stmt_definition;
  stmt_definition.str= (char*) thd->lex->stmt_definition_begin;
  stmt_definition.length= thd->lex->stmt_definition_end
    - thd->lex->stmt_definition_begin;
  trim_whitespace(thd->charset(), & stmt_definition);

  stmt_query->append(stmt_definition.str, stmt_definition.length);

  trg_def->str= stmt_query->c_ptr();
  trg_def->length= stmt_query->length();

  /* Create trigger definition file. */

  if (!sql_create_definition_file(NULL, &file, &triggers_file_type,
                                  (uchar*)this, triggers_file_parameters, 0))
    return 0;

err_with_cleanup:
  my_delete(trigname_buff, MYF(MY_WME));
  return 1;
}


/*
  Deletes the .TRG file for a table

  SYNOPSIS
    rm_trigger_file()
      path       - char buffer of size FN_REFLEN to be used
                   for constructing path to .TRG file.
      db         - table's database name
      table_name - table's name

  RETURN VALUE
    False - success
    True  - error
*/

static bool rm_trigger_file(char *path, const char *db,
                            const char *table_name)
{
  build_table_filename(path, FN_REFLEN-1, db, table_name, TRG_EXT, 0);
  return my_delete(path, MYF(MY_WME));
}


/*
  Deletes the .TRN file for a trigger

  SYNOPSIS
    rm_trigname_file()
      path       - char buffer of size FN_REFLEN to be used
                   for constructing path to .TRN file.
      db         - trigger's database name
      table_name - trigger's name

  RETURN VALUE
    False - success
    True  - error
*/

static bool rm_trigname_file(char *path, const char *db,
                             const char *trigger_name)
{
  build_table_filename(path, FN_REFLEN - 1, db, trigger_name, TRN_EXT, 0);
  return my_delete(path, MYF(MY_WME));
}


/*
  Helper function that saves .TRG file for Table_triggers_list object.

  SYNOPSIS
    save_trigger_file()
      triggers    Table_triggers_list object for which file should be saved
      db          Name of database for subject table
      table_name  Name of subject table

  RETURN VALUE
    FALSE  Success
    TRUE   Error
*/

static bool save_trigger_file(Table_triggers_list *triggers, const char *db,
                              const char *table_name)
{
  char file_buff[FN_REFLEN];
  LEX_STRING file;

  file.length= build_table_filename(file_buff, FN_REFLEN - 1, db, table_name,
                                    TRG_EXT, 0);
  file.str= file_buff;
  return sql_create_definition_file(NULL, &file, &triggers_file_type,
                                    (uchar*)triggers, triggers_file_parameters,
                                    0);
}


/*
  Drop trigger for table.

  SYNOPSIS
    drop_trigger()
      thd         - current thread context
                    (including trigger definition in LEX)
      tables      - table list containing one open table for which trigger
                    is dropped.
      stmt_query  - [OUT] after successful return, this string contains
                    well-formed statement for deleting this trigger.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::drop_trigger(THD *thd, TABLE_LIST *tables,
                                       String *stmt_query)
{
  const char *sp_name= thd->lex->spname->m_name.str; // alias

  LEX_STRING *name;
  char path[FN_REFLEN];

  List_iterator_fast<LEX_STRING> it_name(names_list);

  List_iterator<ulonglong> it_mod(definition_modes_list);
  List_iterator<LEX_STRING> it_def(definitions_list);
  List_iterator<LEX_STRING> it_definer(definers_list);
  List_iterator<LEX_STRING> it_client_cs_name(client_cs_names);
  List_iterator<LEX_STRING> it_connection_cl_name(connection_cl_names);
  List_iterator<LEX_STRING> it_db_cl_name(db_cl_names);

  stmt_query->append(thd->query, thd->query_length);

  while ((name= it_name++))
  {
    it_def++;
    it_mod++;
    it_definer++;
    it_client_cs_name++;
    it_connection_cl_name++;
    it_db_cl_name++;

    if (my_strcasecmp(table_alias_charset, sp_name, name->str) == 0)
    {
      /*
        Again we don't care much about other things required for
        clean trigger removing since table will be reopened anyway.
      */
      it_def.remove();
      it_mod.remove();
      it_definer.remove();
      it_client_cs_name.remove();
      it_connection_cl_name.remove();
      it_db_cl_name.remove();

      if (definitions_list.is_empty())
      {
        /*
          TODO: Probably instead of removing .TRG file we should move
          to archive directory but this should be done as part of
          parse_file.cc functionality (because we will need it
          elsewhere).
        */
        if (rm_trigger_file(path, tables->db, tables->table_name))
          return 1;
      }
      else
      {
        if (save_trigger_file(this, tables->db, tables->table_name))
          return 1;
      }

      if (rm_trigname_file(path, tables->db, sp_name))
        return 1;
      return 0;
    }
  }

  my_message(ER_TRG_DOES_NOT_EXIST, ER(ER_TRG_DOES_NOT_EXIST), MYF(0));
  return 1;
}


Table_triggers_list::~Table_triggers_list()
{
  for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
    for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
      delete bodies[i][j];

  if (record1_field)
    for (Field **fld_ptr= record1_field; *fld_ptr; fld_ptr++)
      delete *fld_ptr;
}


/*
  Prepare array of Field objects referencing to TABLE::record[1] instead
  of record[0] (they will represent OLD.* row values in ON UPDATE trigger
  and in ON DELETE trigger which will be called during REPLACE execution).

  SYNOPSIS
    prepare_record1_accessors()
      table - pointer to TABLE object for which we are creating fields.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::prepare_record1_accessors(TABLE *table)
{
  Field **fld, **old_fld;

  if (!(record1_field= (Field **)alloc_root(&table->mem_root,
                                            (table->s->fields + 1) *
                                            sizeof(Field*))))
    return 1;

  for (fld= table->field, old_fld= record1_field; *fld; fld++, old_fld++)
  {
    /*
      QQ: it is supposed that it is ok to use this function for field
      cloning...
    */
    if (!(*old_fld= (*fld)->new_field(&table->mem_root, table,
                                      table == (*fld)->table)))
      return 1;
    (*old_fld)->move_field_offset((my_ptrdiff_t)(table->record[1] -
                                                 table->record[0]));
  }
  *old_fld= 0;

  return 0;
}


/*
  Adjust Table_triggers_list with new TABLE pointer.

  SYNOPSIS
    set_table()
      new_table - new pointer to TABLE instance
*/

void Table_triggers_list::set_table(TABLE *new_table)
{
  trigger_table= new_table;
  for (Field **field= new_table->triggers->record1_field ; *field ; field++)
  {
    (*field)->table= (*field)->orig_table= new_table;
    (*field)->table_name= &new_table->alias;
  }
}


/*
  Check whenever .TRG file for table exist and load all triggers it contains.

  SYNOPSIS
    check_n_load()
      thd        - current thread context
      db         - table's database name
      table_name - table's name
      table      - pointer to table object
      names_only - stop after loading trigger names

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::check_n_load(THD *thd, const char *db,
                                       const char *table_name, TABLE *table,
                                       bool names_only)
{
  char path_buff[FN_REFLEN];
  LEX_STRING path;
  File_parser *parser;
  LEX_STRING save_db;

  DBUG_ENTER("Table_triggers_list::check_n_load");

  path.length= build_table_filename(path_buff, FN_REFLEN - 1,
                                    db, table_name, TRG_EXT, 0);
  path.str= path_buff;

  // QQ: should we analyze errno somehow ?
  if (access(path_buff, F_OK))
    DBUG_RETURN(0);

  /*
    File exists so we got to load triggers.
    FIXME: A lot of things to do here e.g. how about other funcs and being
    more paranoical ?
  */

  if ((parser= sql_parse_prepare(&path, &table->mem_root, 1)))
  {
    if (is_equal(&triggers_file_type, parser->type()))
    {
      Table_triggers_list *triggers=
        new (&table->mem_root) Table_triggers_list(table);
      Handle_old_incorrect_sql_modes_hook sql_modes_hook(path.str);

      if (!triggers)
        DBUG_RETURN(1);

      /*
        We don't have the following attributes in old versions of .TRG file, so
        we should initialize the list for safety:
          - sql_modes;
          - definers;
          - character sets (client, connection, database);
      */
      triggers->definition_modes_list.empty();
      triggers->definers_list.empty();
      triggers->client_cs_names.empty();
      triggers->connection_cl_names.empty();
      triggers->db_cl_names.empty();

      if (parser->parse((uchar*)triggers, &table->mem_root,
                        triggers_file_parameters,
                        TRG_NUM_REQUIRED_PARAMETERS,
                        &sql_modes_hook))
        DBUG_RETURN(1);

      List_iterator_fast<LEX_STRING> it(triggers->definitions_list);
      LEX_STRING *trg_create_str;
      ulonglong *trg_sql_mode;

      if (triggers->definition_modes_list.is_empty() &&
          !triggers->definitions_list.is_empty())
      {
        /*
          It is old file format => we should fill list of sql_modes.

          We use one mode (current) for all triggers, because we have not
          information about mode in old format.
        */
        if (!(trg_sql_mode= alloc_type<ulonglong>(&table->mem_root)))
        {
          DBUG_RETURN(1); // EOM
        }
        *trg_sql_mode= global_system_variables.sql_mode;
        while (it++)
        {
          if (triggers->definition_modes_list.push_back(trg_sql_mode,
                                                        &table->mem_root))
          {
            DBUG_RETURN(1); // EOM
          }
        }
        it.rewind();
      }

      if (triggers->definers_list.is_empty() &&
          !triggers->definitions_list.is_empty())
      {
        /*
          It is old file format => we should fill list of definers.

          If there is no definer information, we should not switch context to
          definer when checking privileges. I.e. privileges for such triggers
          are checked for "invoker" rather than for "definer".
        */

        LEX_STRING *trg_definer;

        if (!(trg_definer= alloc_lex_string(&table->mem_root)))
          DBUG_RETURN(1); // EOM

        trg_definer->str= (char*) "";
        trg_definer->length= 0;

        while (it++)
        {
          if (triggers->definers_list.push_back(trg_definer,
                                                &table->mem_root))
          {
            DBUG_RETURN(1); // EOM
          }
        }

        it.rewind();
      }

      if (!triggers->definitions_list.is_empty() &&
          (triggers->client_cs_names.is_empty() ||
           triggers->connection_cl_names.is_empty() ||
           triggers->db_cl_names.is_empty()))
      {
        /*
          It is old file format => we should fill lists of character sets.
        */

        LEX_STRING *trg_client_cs_name;
        LEX_STRING *trg_connection_cl_name;
        LEX_STRING *trg_db_cl_name;

        if (!triggers->client_cs_names.is_empty() ||
            !triggers->connection_cl_names.is_empty() ||
            !triggers->db_cl_names.is_empty())
        {
          my_error(ER_TRG_CORRUPTED_FILE, MYF(0),
                   (const char *) db,
                   (const char *) table_name);

          DBUG_RETURN(1); // EOM
        }

        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_TRG_NO_CREATION_CTX,
                            ER(ER_TRG_NO_CREATION_CTX),
                            (const char*) db,
                            (const char*) table_name);

        if (!(trg_client_cs_name= alloc_lex_string(&table->mem_root)) ||
            !(trg_connection_cl_name= alloc_lex_string(&table->mem_root)) ||
            !(trg_db_cl_name= alloc_lex_string(&table->mem_root)))
        {
          DBUG_RETURN(1); // EOM
        }

        /*
          Backward compatibility: assume that the query is in the current
          character set.
        */

        lex_string_set(trg_client_cs_name,
                       thd->variables.character_set_client->csname);

        lex_string_set(trg_connection_cl_name,
                       thd->variables.collation_connection->name);

        lex_string_set(trg_db_cl_name,
                       thd->variables.collation_database->name);

        while (it++)
        {
          if (triggers->client_cs_names.push_back(trg_client_cs_name,
                                                  &table->mem_root) ||

              triggers->connection_cl_names.push_back(trg_connection_cl_name,
                                                      &table->mem_root) ||

              triggers->db_cl_names.push_back(trg_db_cl_name,
                                              &table->mem_root))
          {
            DBUG_RETURN(1); // EOM
          }
        }

        it.rewind();
      }

      DBUG_ASSERT(triggers->definition_modes_list.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->definers_list.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->client_cs_names.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->connection_cl_names.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->db_cl_names.elements ==
                  triggers->definitions_list.elements);

      table->triggers= triggers;

      /*
        TODO: This could be avoided if there is no triggers
              for UPDATE and DELETE.
      */
      if (!names_only && triggers->prepare_record1_accessors(table))
        DBUG_RETURN(1);

      List_iterator_fast<ulonglong> itm(triggers->definition_modes_list);
      List_iterator_fast<LEX_STRING> it_definer(triggers->definers_list);
      List_iterator_fast<LEX_STRING> it_client_cs_name(triggers->client_cs_names);
      List_iterator_fast<LEX_STRING> it_connection_cl_name(triggers->connection_cl_names);
      List_iterator_fast<LEX_STRING> it_db_cl_name(triggers->db_cl_names);
      LEX *old_lex= thd->lex, lex;
      sp_rcontext *save_spcont= thd->spcont;
      ulong save_sql_mode= thd->variables.sql_mode;
      LEX_STRING *on_table_name;

      thd->lex= &lex;

      save_db.str= thd->db;
      save_db.length= thd->db_length;
      thd->reset_db((char*) db, strlen(db));
      while ((trg_create_str= it++))
      {
        trg_sql_mode= itm++;
        LEX_STRING *trg_definer= it_definer++;

        thd->variables.sql_mode= (ulong)*trg_sql_mode;

        Lex_input_stream lip(thd, trg_create_str->str, trg_create_str->length);

        Trigger_creation_ctx *creation_ctx=
          Trigger_creation_ctx::create(thd,
                                       db,
                                       table_name,
                                       it_client_cs_name++,
                                       it_connection_cl_name++,
                                       it_db_cl_name++);

        lex_start(thd);
        thd->spcont= NULL;

        if (parse_sql(thd, &lip, creation_ctx))
        {
          /* Currently sphead is always deleted in case of a parse error */
          DBUG_ASSERT(lex.sphead == 0);
          goto err_with_lex_cleanup;
        }
        /*
          Not strictly necessary to invoke this method here, since we know
          that we've parsed CREATE TRIGGER and not an
          UPDATE/DELETE/INSERT/REPLACE/LOAD/CREATE TABLE, but we try to
          maintain the invariant that this method is called for each
          distinct statement, in case its logic is extended with other
          types of analyses in future.
        */
        lex.set_trg_event_type_for_tables();

        lex.sphead->set_info(0, 0, &lex.sp_chistics, (ulong) *trg_sql_mode);

        int event= lex.trg_chistics.event;
        int action_time= lex.trg_chistics.action_time;

        lex.sphead->set_creation_ctx(creation_ctx);
        triggers->bodies[event][action_time]= lex.sphead;

        if (!trg_definer->length)
        {
          /*
            This trigger was created/imported from the previous version of
            MySQL, which does not support triggers definers. We should emit
            warning here.
          */

          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_TRG_NO_DEFINER, ER(ER_TRG_NO_DEFINER),
                              (const char*) db,
                              (const char*) lex.sphead->m_name.str);

          /*
            Set definer to the '' to correct displaying in the information
            schema.
          */

          lex.sphead->set_definer((char*) "", 0);

          /*
            Triggers without definer information are executed under the
            authorization of the invoker.
          */

          lex.sphead->m_chistics->suid= SP_IS_NOT_SUID;
        }
        else
          lex.sphead->set_definer(trg_definer->str, trg_definer->length);

        if (triggers->names_list.push_back(&lex.sphead->m_name,
                                           &table->mem_root))
            goto err_with_lex_cleanup;

        if (!(on_table_name= alloc_lex_string(&table->mem_root)))
          goto err_with_lex_cleanup;

        on_table_name->str= (char*) lex.raw_trg_on_table_name_begin;
        on_table_name->length= lex.raw_trg_on_table_name_end
          - lex.raw_trg_on_table_name_begin;

        if (triggers->on_table_names_list.push_back(on_table_name, &table->mem_root))
          goto err_with_lex_cleanup;

        /*
          Let us check that we correctly update trigger definitions when we
          rename tables with triggers.
        */
        DBUG_ASSERT(!my_strcasecmp(table_alias_charset, lex.query_tables->db, db) &&
                    !my_strcasecmp(table_alias_charset, lex.query_tables->table_name,
                                   table_name));

        if (names_only)
        {
          lex_end(&lex);
          continue;
        }

        /*
          Gather all Item_trigger_field objects representing access to fields
          in old/new versions of row in trigger into lists containing all such
          objects for the triggers with same action and timing.
        */
        triggers->trigger_fields[lex.trg_chistics.event]
                                [lex.trg_chistics.action_time]=
          (Item_trigger_field *)(lex.trg_table_fields.first);
        /*
          Also let us bind these objects to Field objects in table being
          opened.

          We ignore errors here, because if even something is wrong we still
          will be willing to open table to perform some operations (e.g.
          SELECT)...
          Anyway some things can be checked only during trigger execution.
        */
        for (Item_trigger_field *trg_field=
               (Item_trigger_field *)(lex.trg_table_fields.first);
             trg_field;
             trg_field= trg_field->next_trg_field)
        {
          trg_field->setup_field(thd, table,
            &triggers->subject_table_grants[lex.trg_chistics.event]
                                           [lex.trg_chistics.action_time]);
        }

        lex_end(&lex);
      }
      thd->reset_db(save_db.str, save_db.length);
      thd->lex= old_lex;
      thd->spcont= save_spcont;
      thd->variables.sql_mode= save_sql_mode;

      DBUG_RETURN(0);

err_with_lex_cleanup:
      // QQ: anything else ?
      lex_end(&lex);
      thd->lex= old_lex;
      thd->spcont= save_spcont;
      thd->variables.sql_mode= save_sql_mode;
      thd->reset_db(save_db.str, save_db.length);
      DBUG_RETURN(1);
    }

    /*
      We don't care about this error message much because .TRG files will
      be merged into .FRM anyway.
    */
    my_error(ER_WRONG_OBJECT, MYF(0),
             table_name, TRG_EXT + 1, "TRIGGER");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(1);
}


/*
  Obtains and returns trigger metadata

  SYNOPSIS
    get_trigger_info()
      thd       - current thread context
      event     - trigger event type
      time_type - trigger action time
      name      - returns name of trigger
      stmt      - returns statement of trigger
      sql_mode  - returns sql_mode of trigger
      definer_user - returns definer/creator of trigger. The caller is
                  responsible to allocate enough space for storing definer
                  information.

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::get_trigger_info(THD *thd, trg_event_type event,
                                           trg_action_time_type time_type,
                                           LEX_STRING *trigger_name,
                                           LEX_STRING *trigger_stmt,
                                           ulong *sql_mode,
                                           LEX_STRING *definer,
                                           LEX_STRING *client_cs_name,
                                           LEX_STRING *connection_cl_name,
                                           LEX_STRING *db_cl_name)
{
  sp_head *body;
  DBUG_ENTER("get_trigger_info");
  if ((body= bodies[event][time_type]))
  {
    Stored_program_creation_ctx *creation_ctx=
      bodies[event][time_type]->get_creation_ctx();

    *trigger_name= body->m_name;
    *trigger_stmt= body->m_body_utf8;
    *sql_mode= body->m_sql_mode;

    if (body->m_chistics->suid == SP_IS_NOT_SUID)
    {
      definer->str[0]= 0;
      definer->length= 0;
    }
    else
    {
      definer->length= strxmov(definer->str, body->m_definer_user.str, "@",
                               body->m_definer_host.str, NullS) - definer->str;
    }

    lex_string_set(client_cs_name,
                   creation_ctx->get_client_cs()->csname);

    lex_string_set(connection_cl_name,
                   creation_ctx->get_connection_cl()->name);

    lex_string_set(db_cl_name,
                   creation_ctx->get_db_cl()->name);

    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


void Table_triggers_list::get_trigger_info(THD *thd,
                                           int trigger_idx,
                                           LEX_STRING *trigger_name,
                                           ulonglong *sql_mode,
                                           LEX_STRING *sql_original_stmt,
                                           LEX_STRING *client_cs_name,
                                           LEX_STRING *connection_cl_name,
                                           LEX_STRING *db_cl_name)
{
  List_iterator_fast<LEX_STRING> it_trigger_name(names_list);
  List_iterator_fast<ulonglong> it_sql_mode(definition_modes_list);
  List_iterator_fast<LEX_STRING> it_sql_orig_stmt(definitions_list);
  List_iterator_fast<LEX_STRING> it_client_cs_name(client_cs_names);
  List_iterator_fast<LEX_STRING> it_connection_cl_name(connection_cl_names);
  List_iterator_fast<LEX_STRING> it_db_cl_name(db_cl_names);

  for (int i = 0; i < trigger_idx; ++i)
  {
    it_trigger_name.next_fast();
    it_sql_mode.next_fast();
    it_sql_orig_stmt.next_fast();

    it_client_cs_name.next_fast();
    it_connection_cl_name.next_fast();
    it_db_cl_name.next_fast();
  }

  *trigger_name= *(it_trigger_name++);
  *sql_mode= *(it_sql_mode++);
  *sql_original_stmt= *(it_sql_orig_stmt++);

  *client_cs_name= *(it_client_cs_name++);
  *connection_cl_name= *(it_connection_cl_name++);
  *db_cl_name= *(it_db_cl_name++);
}


int Table_triggers_list::find_trigger_by_name(const LEX_STRING *trg_name)
{
  List_iterator_fast<LEX_STRING> it(names_list);

  for (int i = 0; ; ++i)
  {
    LEX_STRING *cur_name= it++;

    if (!cur_name)
      return -1;

    if (strcmp(cur_name->str, trg_name->str) == 0)
      return i;
  }
}

/**
  Find trigger's table from trigger identifier and add it to
  the statement table list.

  @param[in] thd       Thread context.
  @param[in] trg_name  Trigger name.
  @param[in] if_exists TRUE if SQL statement contains "IF EXISTS" clause.
                       That means a warning instead of error should be
                       thrown if trigger with given name does not exist.
  @param[out] table    Pointer to TABLE_LIST object for the
                       table trigger.

  @return Operation status
    @retval FALSE On success.
    @retval TRUE  Otherwise.
*/

bool add_table_for_trigger(THD *thd,
                           const sp_name *trg_name,
                           bool if_exists,
                           TABLE_LIST **table)
{
  LEX *lex= thd->lex;
  char trn_path_buff[FN_REFLEN];
  LEX_STRING trn_path= { trn_path_buff, 0 };
  LEX_STRING tbl_name;

  DBUG_ENTER("add_table_for_trigger");

  build_trn_path(thd, trg_name, &trn_path);

  if (check_trn_exists(&trn_path))
  {
    if (if_exists)
    {
      push_warning_printf(thd,
                          MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TRG_DOES_NOT_EXIST,
                          ER(ER_TRG_DOES_NOT_EXIST));

      *table= NULL;

      DBUG_RETURN(FALSE);
    }

    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (load_table_name_for_trigger(thd, trg_name, &trn_path, &tbl_name))
    DBUG_RETURN(TRUE);

  /* We need to reset statement table list to be PS/SP friendly. */
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;

  *table= sp_add_to_query_tables(thd, lex, trg_name->m_db.str,
                                 tbl_name.str, TL_IGNORE);

  DBUG_RETURN(*table ? FALSE : TRUE);
}


/*
  Drop all triggers for table.

  SYNOPSIS
    drop_all_triggers()
      thd    - current thread context
      db     - schema for table
      name   - name for table

  NOTE
    The calling thread should hold the LOCK_open mutex;

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::drop_all_triggers(THD *thd, char *db, char *name)
{
  TABLE table;
  char path[FN_REFLEN];
  bool result= 0;
  DBUG_ENTER("drop_all_triggers");

  bzero(&table, sizeof(table));
  init_alloc_root(&table.mem_root, 8192, 0);

  if (Table_triggers_list::check_n_load(thd, db, name, &table, 1))
  {
    result= 1;
    goto end;
  }
  if (table.triggers)
  {
    LEX_STRING *trigger;
    List_iterator_fast<LEX_STRING> it_name(table.triggers->names_list);

    while ((trigger= it_name++))
    {
      if (rm_trigname_file(path, db, trigger->str))
      {
        /*
          Instead of immediately bailing out with error if we were unable
          to remove .TRN file we will try to drop other files.
        */
        result= 1;
        continue;
      }
    }

    if (rm_trigger_file(path, db, name))
    {
      result= 1;
      goto end;
    }
  }
end:
  if (table.triggers)
    delete table.triggers;
  free_root(&table.mem_root, MYF(0));
  DBUG_RETURN(result);
}


/*
  Update .TRG file after renaming triggers' subject table
  (change name of table in triggers' definitions).

  SYNOPSIS
    change_table_name_in_triggers()
      thd                 Thread context
      db_name             Database of subject table
      old_table_name      Old subject table's name
      new_table_name      New subject table's name

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

bool
Table_triggers_list::change_table_name_in_triggers(THD *thd,
                                                   const char *db_name,
                                                   LEX_STRING *old_table_name,
                                                   LEX_STRING *new_table_name)
{
  char path_buff[FN_REFLEN];
  LEX_STRING *def, *on_table_name, new_def;
  ulong save_sql_mode= thd->variables.sql_mode;
  List_iterator_fast<LEX_STRING> it_def(definitions_list);
  List_iterator_fast<LEX_STRING> it_on_table_name(on_table_names_list);
  List_iterator_fast<ulonglong> it_mode(definition_modes_list);
  uint on_q_table_name_len, before_on_len;
  String buff;

  DBUG_ASSERT(definitions_list.elements == on_table_names_list.elements &&
              definitions_list.elements == definition_modes_list.elements);

  while ((def= it_def++))
  {
    on_table_name= it_on_table_name++;
    thd->variables.sql_mode= (ulong) *(it_mode++);

    /* Construct CREATE TRIGGER statement with new table name. */
    buff.length(0);

    /* WARNING: 'on_table_name' is supposed to point inside 'def' */
    DBUG_ASSERT(on_table_name->str > def->str);
    DBUG_ASSERT(on_table_name->str < (def->str + def->length));
    before_on_len= on_table_name->str - def->str;

    buff.append(def->str, before_on_len);
    buff.append(STRING_WITH_LEN("ON "));
    append_identifier(thd, &buff, new_table_name->str, new_table_name->length);
    buff.append(STRING_WITH_LEN(" "));
    on_q_table_name_len= buff.length() - before_on_len;
    buff.append(on_table_name->str + on_table_name->length,
                def->length - (before_on_len + on_table_name->length));
    /*
      It is OK to allocate some memory on table's MEM_ROOT since this
      table instance will be thrown out at the end of rename anyway.
    */
    new_def.str= (char*) memdup_root(&trigger_table->mem_root, buff.ptr(),
                                     buff.length());
    new_def.length= buff.length();
    on_table_name->str= new_def.str + before_on_len;
    on_table_name->length= on_q_table_name_len;
    *def= new_def;
  }

  thd->variables.sql_mode= save_sql_mode;

  if (thd->is_fatal_error)
    return TRUE; /* OOM */

  if (save_trigger_file(this, db_name, new_table_name->str))
    return TRUE;
  if (rm_trigger_file(path_buff, db_name, old_table_name->str))
  {
    (void) rm_trigger_file(path_buff, db_name, new_table_name->str);
    return TRUE;
  }
  return FALSE;
}


/*
  Iterate though Table_triggers_list::names_list list and update .TRN files
  after renaming triggers' subject table.

  SYNOPSIS
    change_table_name_in_trignames()
      db_name             Database of subject table
      new_table_name      New subject table's name
      stopper             Pointer to Table_triggers_list::names_list at
                          which we should stop updating.

  RETURN VALUE
    0      Success
    non-0  Failure, pointer to Table_triggers_list::names_list element
           for which update failed.
*/

LEX_STRING*
Table_triggers_list::change_table_name_in_trignames(const char *db_name,
                                                    LEX_STRING *new_table_name,
                                                    LEX_STRING *stopper)
{
  char trigname_buff[FN_REFLEN];
  struct st_trigname trigname;
  LEX_STRING trigname_file;
  LEX_STRING *trigger;
  List_iterator_fast<LEX_STRING> it_name(names_list);

  while ((trigger= it_name++) != stopper)
  {
    trigname_file.length= build_table_filename(trigname_buff, FN_REFLEN-1,
                                               db_name, trigger->str,
                                               TRN_EXT, 0);
    trigname_file.str= trigname_buff;

    trigname.trigger_table= *new_table_name;

    if (sql_create_definition_file(NULL, &trigname_file, &trigname_file_type,
                                   (uchar*)&trigname, trigname_file_parameters,
                                   0))
      return trigger;
  }

  return 0;
}


/**
  @brief Update .TRG and .TRN files after renaming triggers' subject table.

  @param[in,out] thd Thread context
  @param[in] db Old database of subject table
  @param[in] old_table Old name of subject table
  @param[in] new_db New database for subject table
  @param[in] new_table New name of subject table

  @note
    This method tries to leave trigger related files in consistent state,
    i.e. it either will complete successfully, or will fail leaving files
    in their initial state.
    Also this method assumes that subject table is not renamed to itself.
    This method needs to be called under an exclusive table name lock.

  @retval FALSE Success
  @retval TRUE  Error
*/

bool Table_triggers_list::change_table_name(THD *thd, const char *db,
                                            const char *old_table,
                                            const char *new_db,
                                            const char *new_table)
{
  TABLE table;
  bool result= 0;
  LEX_STRING *err_trigname;
  DBUG_ENTER("change_table_name");

  bzero(&table, sizeof(table));
  init_alloc_root(&table.mem_root, 8192, 0);

  /*
    This method interfaces the mysql server code protected by
    either LOCK_open mutex or with an exclusive table name lock.
    In the future, only an exclusive table name lock will be enough.
  */
#ifndef DBUG_OFF
  uchar key[MAX_DBKEY_LENGTH];
  uint key_length= (uint) (strmov(strmov((char*)&key[0], db)+1,
                    old_table)-(char*)&key[0])+1;

  if (!is_table_name_exclusively_locked_by_this_thread(thd, key, key_length))
    safe_mutex_assert_owner(&LOCK_open);
#endif

  DBUG_ASSERT(my_strcasecmp(table_alias_charset, db, new_db) ||
              my_strcasecmp(table_alias_charset, old_table, new_table));

  if (Table_triggers_list::check_n_load(thd, db, old_table, &table, TRUE))
  {
    result= 1;
    goto end;
  }
  if (table.triggers)
  {
    LEX_STRING old_table_name= { (char *) old_table, strlen(old_table) };
    LEX_STRING new_table_name= { (char *) new_table, strlen(new_table) };
    /*
      Since triggers should be in the same schema as their subject tables
      moving table with them between two schemas raises too many questions.
      (E.g. what should happen if in new schema we already have trigger
       with same name ?).
    */
    if (my_strcasecmp(table_alias_charset, db, new_db))
    {
      my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
      result= 1;
      goto end;
    }
    if (table.triggers->change_table_name_in_triggers(thd, db,
                                                      &old_table_name,
                                                      &new_table_name))
    {
      result= 1;
      goto end;
    }
    if ((err_trigname= table.triggers->change_table_name_in_trignames(
                                         db, &new_table_name, 0)))
    {
      /*
        If we were unable to update one of .TRN files properly we will
        revert all changes that we have done and report about error.
        We assume that we will be able to undo our changes without errors
        (we can't do much if there will be an error anyway).
      */
      (void) table.triggers->change_table_name_in_trignames(db,
                                                            &old_table_name,
                                                            err_trigname);
      (void) table.triggers->change_table_name_in_triggers(thd, db,
                                                           &new_table_name,
                                                           &old_table_name);
      result= 1;
      goto end;
    }
  }
end:
  delete table.triggers;
  free_root(&table.mem_root, MYF(0));
  DBUG_RETURN(result);
}


/**
  Execute trigger for given (event, time) pair.

  The operation executes trigger for the specified event (insert, update,
  delete) and time (after, before) if it is set.

  @param thd
  @param event
  @param time_type
  @param old_row_is_record1

  @return Error status.
    @retval FALSE on success.
    @retval TRUE  on error.
*/

bool Table_triggers_list::process_triggers(THD *thd,
                                           trg_event_type event,
                                           trg_action_time_type time_type,
                                           bool old_row_is_record1)
{
  bool err_status;
  Sub_statement_state statement_state;
  sp_head *sp_trigger= bodies[event][time_type];

  if (sp_trigger == NULL)
    return FALSE;

  if (old_row_is_record1)
  {
    old_field= record1_field;
    new_field= trigger_table->field;
  }
  else
  {
    new_field= record1_field;
    old_field= trigger_table->field;
  }
  /*
    This trigger must have been processed by the pre-locking
    algorithm.
  */
  DBUG_ASSERT(trigger_table->pos_in_table_list->trg_event_map &
              static_cast<uint>(1 << static_cast<int>(event)));

  thd->reset_sub_statement_state(&statement_state, SUB_STMT_TRIGGER);

  err_status=
    sp_trigger->execute_trigger(thd,
                                &trigger_table->s->db,
                                &trigger_table->s->table_name,
                                &subject_table_grants[event][time_type]);

  thd->restore_sub_statement_state(&statement_state);

  return err_status;
}


/*
  Mark fields of subject table which we read/set in its triggers as such.

  SYNOPSIS
    mark_fields_used()
      thd    Current thread context
      event  Type of event triggers for which we are going to ins

  DESCRIPTION
    This method marks fields of subject table which are read/set in its
    triggers as such (by properly updating TABLE::read_set/write_set)
    and thus informs handler that values for these fields should be
    retrieved/stored during execution of statement.
*/

void Table_triggers_list::mark_fields_used(trg_event_type event)
{
  int action_time;
  Item_trigger_field *trg_field;

  for (action_time= 0; action_time < (int)TRG_ACTION_MAX; action_time++)
  {
    for (trg_field= trigger_fields[event][action_time]; trg_field;
         trg_field= trg_field->next_trg_field)
    {
      /* We cannot mark fields which does not present in table. */
      if (trg_field->field_idx != (uint)-1)
      {
        bitmap_set_bit(trigger_table->read_set, trg_field->field_idx);
        if (trg_field->get_settable_routine_parameter())
          bitmap_set_bit(trigger_table->write_set, trg_field->field_idx);
      }
    }
  }
  trigger_table->file->column_bitmaps_signal();
}


/*
  Trigger BUG#14090 compatibility hook

  SYNOPSIS
    Handle_old_incorrect_sql_modes_hook::process_unknown_string()
    unknown_key          [in/out] reference on the line with unknown
                                  parameter and the parsing point
    base                 [in] base address for parameter writing (structure
                              like TABLE)
    mem_root             [in] MEM_ROOT for parameters allocation
    end                  [in] the end of the configuration

  NOTE: this hook process back compatibility for incorrectly written
  sql_modes parameter (see BUG#14090).

  RETURN
    FALSE OK
    TRUE  Error
*/

#define INVALID_SQL_MODES_LENGTH 13

bool
Handle_old_incorrect_sql_modes_hook::process_unknown_string(char *&unknown_key,
                                                            uchar* base,
                                                            MEM_ROOT *mem_root,
                                                            char *end)
{
  DBUG_ENTER("Handle_old_incorrect_sql_modes_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key: %60s", unknown_key));

  if (unknown_key + INVALID_SQL_MODES_LENGTH + 1 < end &&
      unknown_key[INVALID_SQL_MODES_LENGTH] == '=' &&
      !memcmp(unknown_key, STRING_WITH_LEN("sql_modes")))
  {
    char *ptr= unknown_key + INVALID_SQL_MODES_LENGTH + 1;

    DBUG_PRINT("info", ("sql_modes affected by BUG#14090 detected"));
    push_warning_printf(current_thd,
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_OLD_FILE_FORMAT,
                        ER(ER_OLD_FILE_FORMAT),
                        (char *)path, "TRIGGER");
    if (get_file_options_ulllist(ptr, end, unknown_key, base,
                                 &sql_modes_parameters, mem_root))
    {
      DBUG_RETURN(TRUE);
    }
    /*
      Set parsing pointer to the last symbol of string (\n)
      1) to avoid problem with \0 in the junk after sql_modes
      2) to speed up skipping this line by parser.
    */
    unknown_key= ptr-1;
  }
  DBUG_RETURN(FALSE);
}

/*
  Trigger BUG#15921 compatibility hook. For details see
  Handle_old_incorrect_sql_modes_hook::process_unknown_string().
*/

#define INVALID_TRIGGER_TABLE_LENGTH 15

bool
Handle_old_incorrect_trigger_table_hook::
process_unknown_string(char *&unknown_key, uchar* base, MEM_ROOT *mem_root,
                       char *end)
{
  DBUG_ENTER("Handle_old_incorrect_trigger_table_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key: %60s", unknown_key));

  if (unknown_key + INVALID_TRIGGER_TABLE_LENGTH + 1 < end &&
      unknown_key[INVALID_TRIGGER_TABLE_LENGTH] == '=' &&
      !memcmp(unknown_key, STRING_WITH_LEN("trigger_table")))
  {
    char *ptr= unknown_key + INVALID_TRIGGER_TABLE_LENGTH + 1;

    DBUG_PRINT("info", ("trigger_table affected by BUG#15921 detected"));
    push_warning_printf(current_thd,
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_OLD_FILE_FORMAT,
                        ER(ER_OLD_FILE_FORMAT),
                        (char *)path, "TRIGGER");

    if (!(ptr= parse_escaped_string(ptr, end, mem_root, trigger_table_value)))
    {
      my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0), "trigger_table",
               unknown_key);
      DBUG_RETURN(TRUE);
    }

    /* Set parsing pointer to the last symbol of string (\n). */
    unknown_key= ptr-1;
  }
  DBUG_RETURN(FALSE);
}


/**
  Contruct path to TRN-file.

  @param[in]  thd        Thread context.
  @param[in]  trg_name   Trigger name.
  @param[out] trn_path   Variable to store constructed path
*/

void build_trn_path(THD *thd, const sp_name *trg_name, LEX_STRING *trn_path)
{
  /* Construct path to the TRN-file. */

  trn_path->length= build_table_filename(trn_path->str,
                                         FN_REFLEN - 1,
                                         trg_name->m_db.str,
                                         trg_name->m_name.str,
                                         TRN_EXT,
                                         0);
}


/**
  Check if TRN-file exists.

  @return
    @retval TRUE  if TRN-file does not exist.
    @retval FALSE if TRN-file exists.
*/

bool check_trn_exists(const LEX_STRING *trn_path)
{
  return access(trn_path->str, F_OK) != 0;
}


/**
  Retrieve table name for given trigger.

  @param[in]  thd        Thread context.
  @param[in]  trg_name   Trigger name.
  @param[in]  trn_path   Path to the corresponding TRN-file.
  @param[out] tbl_name  Variable to store retrieved table name.

  @return Error status.
    @retval FALSE on success.
    @retval TRUE  if table name could not be retrieved.
*/

bool load_table_name_for_trigger(THD *thd,
                                 const sp_name *trg_name,
                                 const LEX_STRING *trn_path,
                                 LEX_STRING *tbl_name)
{
  File_parser *parser;
  struct st_trigname trn_data;

  Handle_old_incorrect_trigger_table_hook trigger_table_hook(
                                          trn_path->str,
                                          &trn_data.trigger_table);

  DBUG_ENTER("load_table_name_for_trigger");

  /* Parse the TRN-file. */

  if (!(parser= sql_parse_prepare(trn_path, thd->mem_root, TRUE)))
    DBUG_RETURN(TRUE);

  if (!is_equal(&trigname_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0),
             trg_name->m_name.str,
             TRN_EXT + 1,
             "TRIGGERNAME");

    DBUG_RETURN(TRUE);
  }

  if (parser->parse((uchar*) &trn_data, thd->mem_root,
                    trigname_file_parameters, 1,
                    &trigger_table_hook))
    DBUG_RETURN(TRUE);

  /* Copy trigger table name. */

  *tbl_name= trn_data.trigger_table;

  /* That's all. */

  DBUG_RETURN(FALSE);
}
