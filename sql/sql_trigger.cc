/* Copyright (C) 2004-2005 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */


#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "parse_file.h"

static const LEX_STRING triggers_file_type=
  { C_STRING_WITH_LEN("TRIGGERS") };

const char * const triggers_file_ext= ".TRG";

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
    offsetof(class Table_triggers_list, definitions_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("sql_modes") },
    offsetof(class Table_triggers_list, definition_modes_list),
    FILE_OPTIONS_ULLLIST
  },
  {
    { C_STRING_WITH_LEN("definers") },
    offsetof(class Table_triggers_list, definers_list),
    FILE_OPTIONS_STRLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

File_option sql_modes_parameters=
{
  { C_STRING_WITH_LEN("sql_modes") },
  offsetof(class Table_triggers_list, definition_modes_list),
  FILE_OPTIONS_ULLLIST
};

/*
  This must be kept up to date whenever a new option is added to the list
  above, as it specifies the number of required parameters of the trigger in
  .trg file.
*/

static const int TRG_NUM_REQUIRED_PARAMETERS= 4;

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

const char * const trigname_file_ext= ".TRN";

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


static TABLE_LIST *add_table_for_trigger(THD *thd, sp_name *trig);

class Handle_old_incorrect_sql_modes_hook: public Unknown_key_hook
{
private:
  char *path;
public:
  Handle_old_incorrect_sql_modes_hook(char *file_path)
    :path(file_path)
  {};
  virtual bool process_unknown_string(char *&unknown_key, gptr base,
                                      MEM_ROOT *mem_root, char *end);
};

class Handle_old_incorrect_trigger_table_hook: public Unknown_key_hook
{
public:
  Handle_old_incorrect_trigger_table_hook(char *file_path,
                                          LEX_STRING *trigger_table_arg)
    :path(file_path), trigger_table_value(trigger_table_arg)
  {};
  virtual bool process_unknown_string(char *&unknown_key, gptr base,
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
  TABLE *table;
  bool result= TRUE;
  LEX_STRING definer_user;
  LEX_STRING definer_host;

  DBUG_ENTER("mysql_create_or_drop_trigger");

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

  if (!create &&
      !(tables= add_table_for_trigger(thd, thd->lex->spname)))
    DBUG_RETURN(TRUE);

  /*
    We don't allow creating triggers on tables in the 'mysql' schema
  */
  if (create && !my_strcasecmp(system_charset_info, "mysql", tables->db))
  {
    my_error(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* We should have only one table in table list. */
  DBUG_ASSERT(tables->next_global == 0);

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
      DBUG_RETURN(TRUE);
  }

  /*
    There is no DETERMINISTIC clause for triggers, so can't check it.
    But a trigger can in theory be used to do nasty things (if it supported
    DROP for example) so we do the check for privileges. Triggers have the
    same nature as functions regarding binlogging: their body is implicitely
    binlogged, so they share the same danger, so trust_function_creators
    applies to them too.
  */
  if (!trust_function_creators && mysql_bin_log.is_open() &&
      !(thd->security_ctx->master_access & SUPER_ACL))
  {
    my_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* We do not allow creation of triggers on temporary tables. */
  if (create && find_temporary_table(thd, tables))
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), tables->alias);
    DBUG_RETURN(TRUE);
  }

  /*
    We don't want perform our operations while global read lock is held
    so we have to wait until its end and then prevent it from occuring
    again until we are done. (Acquiring LOCK_open is not enough because
    global read lock is held without helding LOCK_open).
  */
  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(TRUE);

  VOID(pthread_mutex_lock(&LOCK_open));

  if (lock_table_names(thd, tables))
    goto end;

  /* We also don't allow creation of triggers on views. */
  tables->required_type= FRMTYPE_TABLE;

  if (reopen_name_locked_table(thd, tables))
  {
    unlock_table_name(thd, tables);
    goto end;
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
           table->triggers->create_trigger(thd, tables, &definer_user, &definer_host):
           table->triggers->drop_trigger(thd, tables));

end:
  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);

  if (!result)
  {
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();

      String log_query(thd->query, thd->query_length, system_charset_info);

      if (create)
      {
        log_query.set((char *) 0, 0, system_charset_info); /* reset log_query */

        log_query.append(STRING_WITH_LEN("CREATE "));

        if (definer_user.str && definer_host.str)
        {
          /*
            Append definer-clause if the trigger is SUID (a usual trigger in
            new MySQL versions).
          */

          append_definer(thd, &log_query, &definer_user, &definer_host);
        }

        log_query.append(thd->lex->stmt_definition_begin,
                         (char *)thd->lex->sphead->m_body_begin -
                         thd->lex->stmt_definition_begin +
                         thd->lex->sphead->m_body.length);
      }

      /* Such a statement can always go directly to binlog, no trans cache. */
      Query_log_event qinfo(thd, log_query.ptr(), log_query.length(), 0, FALSE);
      mysql_bin_log.write(&qinfo);
    }

    send_ok(thd);
  }

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
      definer_user - [out] after a call it points to 0-terminated string or
                     contains the NULL-string:
                       - 0-terminated is returned if the trigger is SUID. The
                         string contains user name part of the actual trigger
                         definer.
                       - NULL-string is returned if the trigger is non-SUID.
                     Anyway, the caller is responsible to provide memory for
                     storing LEX_STRING object.
      definer_host - [out] after a call it points to 0-terminated string or
                     contains the NULL-string:
                       - 0-terminated string is returned if the trigger is
                         SUID. The string contains host name part of the
                         actual trigger definer.
                       - NULL-string is returned if the trigger is non-SUID.
                     Anyway, the caller is responsible to provide memory for
                     storing LEX_STRING object.

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
                                         LEX_STRING *definer_user,
                                         LEX_STRING *definer_host)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  char file_buff[FN_REFLEN], trigname_buff[FN_REFLEN];
  LEX_STRING file, trigname_file;
  LEX_STRING *trg_def, *name;
  ulonglong *trg_sql_mode;
  char trg_definer_holder[USER_HOST_BUFF_SIZE];
  LEX_STRING *trg_definer;
  Item_trigger_field *trg_field;
  struct st_trigname trigname;


  /* Trigger must be in the same schema as target table. */
  if (my_strcasecmp(table_alias_charset, table->s->db.str,
                    lex->spname->m_db.str))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return 1;
  }

  /* We don't allow creation of several triggers of the same type yet */
  if (bodies[lex->trg_chistics.event][lex->trg_chistics.action_time])
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
  file.length= build_table_filename(file_buff, FN_REFLEN-1,
                                    tables->db, tables->table_name,
                                    triggers_file_ext, 0);
  file.str= file_buff;
  trigname_file.length= build_table_filename(trigname_buff, FN_REFLEN-1,
                                             tables->db,
                                             lex->spname->m_name.str,
                                             trigname_file_ext, 0);
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
                                 (gptr)&trigname, trigname_file_parameters, 0))
    return 1;

  /*
    Soon we will invalidate table object and thus Table_triggers_list object
    so don't care about place to which trg_def->ptr points and other
    invariants (e.g. we don't bother to update names_list)

    QQ: Hmm... probably we should not care about setting up active thread
        mem_root too.
  */
  if (!(trg_def= (LEX_STRING *)alloc_root(&table->mem_root,
                                          sizeof(LEX_STRING))) ||
      definitions_list.push_back(trg_def, &table->mem_root) ||
      !(trg_sql_mode= (ulonglong*)alloc_root(&table->mem_root,
                                             sizeof(ulonglong))) ||
      definition_modes_list.push_back(trg_sql_mode, &table->mem_root) ||
      !(trg_definer= (LEX_STRING*) alloc_root(&table->mem_root,
                                              sizeof(LEX_STRING))) ||
      definers_list.push_back(trg_definer, &table->mem_root))
    goto err_with_cleanup;

  trg_def->str= thd->query;
  trg_def->length= thd->query_length;
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

    *definer_user= lex->definer->user;
    *definer_host= lex->definer->host;

    trg_definer->str= trg_definer_holder;
    trg_definer->length= strxmov(trg_definer->str, definer_user->str, "@",
                                 definer_host->str, NullS) - trg_definer->str;
  }
  else
  {
    /* non-SUID trigger. */

    definer_user->str= 0;
    definer_user->length= 0;

    definer_host->str= 0;
    definer_host->length= 0;

    trg_definer->str= (char*) "";
    trg_definer->length= 0;
  }

  if (!sql_create_definition_file(NULL, &file, &triggers_file_type,
                                  (gptr)this, triggers_file_parameters, 0))
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
  build_table_filename(path, FN_REFLEN-1, db, table_name, triggers_file_ext, 0);
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
  build_table_filename(path, FN_REFLEN-1,
                       db, trigger_name, trigname_file_ext, 0);
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

  file.length= build_table_filename(file_buff, FN_REFLEN-1, db, table_name,
                                    triggers_file_ext, 0);
  file.str= file_buff;
  return sql_create_definition_file(NULL, &file, &triggers_file_type,
                                    (gptr)triggers, triggers_file_parameters,
                                    0);
}


/*
  Drop trigger for table.

  SYNOPSIS
    drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one open table for which trigger is
               dropped.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::drop_trigger(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  LEX_STRING *name;
  List_iterator_fast<LEX_STRING> it_name(names_list);
  List_iterator<LEX_STRING>      it_def(definitions_list);
  List_iterator<ulonglong>       it_mod(definition_modes_list);
  List_iterator<LEX_STRING>      it_definer(definers_list);
  char path[FN_REFLEN];

  while ((name= it_name++))
  {
    it_def++;
    it_mod++;
    it_definer++;

    if (my_strcasecmp(table_alias_charset, lex->spname->m_name.str,
                      name->str) == 0)
    {
      /*
        Again we don't care much about other things required for
        clean trigger removing since table will be reopened anyway.
      */
      it_def.remove();
      it_mod.remove();
      it_definer.remove();

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

      if (rm_trigname_file(path, tables->db, lex->spname->m_name.str))
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
  table= new_table;
  for (Field **field= table->triggers->record1_field ; *field ; field++)
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

  path.length= build_table_filename(path_buff, FN_REFLEN-1,
                                    db, table_name, triggers_file_ext, 0);
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
      */
      triggers->definition_modes_list.empty();
      triggers->definers_list.empty();

      if (parser->parse((gptr)triggers, &table->mem_root,
                        triggers_file_parameters,
                        TRG_NUM_REQUIRED_PARAMETERS,
                        &sql_modes_hook))
        DBUG_RETURN(1);

      List_iterator_fast<LEX_STRING> it(triggers->definitions_list);
      LEX_STRING *trg_create_str, *trg_name_str;
      ulonglong *trg_sql_mode;

      if (triggers->definition_modes_list.is_empty() &&
          !triggers->definitions_list.is_empty())
      {
        /*
          It is old file format => we should fill list of sql_modes.

          We use one mode (current) for all triggers, because we have not
          information about mode in old format.
        */
        if (!(trg_sql_mode= (ulonglong*)alloc_root(&table->mem_root,
                                                   sizeof(ulonglong))))
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

        if (! (trg_definer= (LEX_STRING*)alloc_root(&table->mem_root,
                                                    sizeof(LEX_STRING))))
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

      DBUG_ASSERT(triggers->definition_modes_list.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->definers_list.elements ==
                  triggers->definitions_list.elements);

      table->triggers= triggers;

      /*
        Construct key that will represent triggers for this table in the set
        of routines used by statement.
      */
      triggers->sroutines_key.length= 1+strlen(db)+1+strlen(table_name)+1;
      if (!(triggers->sroutines_key.str=
              alloc_root(&table->mem_root, triggers->sroutines_key.length)))
        DBUG_RETURN(1);
      triggers->sroutines_key.str[0]= TYPE_ENUM_TRIGGER;
      strxmov(triggers->sroutines_key.str+1, db, ".", table_name, NullS);

      /*
        TODO: This could be avoided if there is no triggers
              for UPDATE and DELETE.
      */
      if (!names_only && triggers->prepare_record1_accessors(table))
        DBUG_RETURN(1);

      List_iterator_fast<ulonglong> itm(triggers->definition_modes_list);
      List_iterator_fast<LEX_STRING> it_definer(triggers->definers_list);
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
        lex_start(thd, (uchar*)trg_create_str->str, trg_create_str->length);

	thd->spcont= 0;
        if (MYSQLparse((void *)thd) || thd->is_fatal_error)
        {
          /*
            Free lex associated resources.
            QQ: Do we really need all this stuff here ?
          */
          delete lex.sphead;
          goto err_with_lex_cleanup;
        }

        lex.sphead->set_info(0, 0, &lex.sp_chistics, *trg_sql_mode);

        triggers->bodies[lex.trg_chistics.event]
                             [lex.trg_chistics.action_time]= lex.sphead;

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

        if (!(on_table_name= (LEX_STRING*) alloc_root(&table->mem_root,
                                                      sizeof(LEX_STRING))))
          goto err_with_lex_cleanup;
        *on_table_name= lex.ident;
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
             table_name, triggers_file_ext+1, "TRIGGER");
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
                                           LEX_STRING *definer)
{
  sp_head *body;
  DBUG_ENTER("get_trigger_info");
  if ((body= bodies[event][time_type]))
  {
    *trigger_name= body->m_name;
    *trigger_stmt= body->m_body;
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

    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


/*
  Find trigger's table from trigger identifier and add it to
  the statement table list.

  SYNOPSIS
    mysql_table_for_trigger()
      thd    - current thread context
      trig   - identifier for trigger

  RETURN VALUE
    0 - error
    # - pointer to TABLE_LIST object for the table
*/

static TABLE_LIST *add_table_for_trigger(THD *thd, sp_name *trig)
{
  LEX *lex= thd->lex;
  char path_buff[FN_REFLEN];
  LEX_STRING path;
  File_parser *parser;
  struct st_trigname trigname;
  Handle_old_incorrect_trigger_table_hook trigger_table_hook(
                                          path_buff, &trigname.trigger_table);
  
  DBUG_ENTER("add_table_for_trigger");

  path.length= build_table_filename(path_buff, FN_REFLEN-1,
                                    trig->m_db.str, trig->m_name.str,
                                    trigname_file_ext, 0);
  path.str= path_buff;

  if (access(path_buff, F_OK))
  {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    DBUG_RETURN(0);
  }

  if (!(parser= sql_parse_prepare(&path, thd->mem_root, 1)))
    DBUG_RETURN(0);

  if (!is_equal(&trigname_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0), trig->m_name.str, trigname_file_ext+1,
             "TRIGGERNAME");
    DBUG_RETURN(0);
  }

  if (parser->parse((gptr)&trigname, thd->mem_root,
                    trigname_file_parameters, 1,
                    &trigger_table_hook))
    DBUG_RETURN(0);

  /* We need to reset statement table list to be PS/SP friendly. */
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
  DBUG_RETURN(sp_add_to_query_tables(thd, lex, trig->m_db.str,
                                     trigname.trigger_table.str, TL_IGNORE));
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

  safe_mutex_assert_owner(&LOCK_open);

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
  ulonglong *sql_mode;
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
    thd->variables.sql_mode= *(it_mode++);

    /* Construct CREATE TRIGGER statement with new table name. */
    buff.length(0);
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
    new_def.str= memdup_root(&table->mem_root, buff.ptr(), buff.length());
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
                                               trigname_file_ext, 0);
    trigname_file.str= trigname_buff;

    trigname.trigger_table= *new_table_name;

    if (sql_create_definition_file(NULL, &trigname_file, &trigname_file_type,
                                   (gptr)&trigname, trigname_file_parameters,
                                   0))
      return trigger;
  }

  return 0;
}


/*
  Update .TRG and .TRN files after renaming triggers' subject table.

  SYNOPSIS
    change_table_name()
      thd        Thread context
      db         Old database of subject table
      old_table  Old name of subject table
      new_db     New database for subject table
      new_table  New name of subject table

  NOTE
    This method tries to leave trigger related files in consistent state,
    i.e. it either will complete successfully, or will fail leaving files
    in their initial state.
    Also this method assumes that subject table is not renamed to itself.

  RETURN VALUE
    FALSE  Success
    TRUE   Error
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

  safe_mutex_assert_owner(&LOCK_open);

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


bool Table_triggers_list::process_triggers(THD *thd, trg_event_type event,
                                           trg_action_time_type time_type,
                                           bool old_row_is_record1)
{
  bool err_status= FALSE;
  sp_head *sp_trigger= bodies[event][time_type];

  if (sp_trigger)
  {
    Sub_statement_state statement_state;

    if (old_row_is_record1)
    {
      old_field= record1_field;
      new_field= table->field;
    }
    else
    {
      new_field= record1_field;
      old_field= table->field;
    }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    Security_context *save_ctx;

    if (sp_change_security_context(thd, sp_trigger, &save_ctx))
      return TRUE;

    /*
      Fetch information about table-level privileges to GRANT_INFO structure for
      subject table. Check of privileges that will use it and information about
      column-level privileges will happen in Item_trigger_field::fix_fields().
    */

    fill_effective_table_privileges(thd,
                                    &subject_table_grants[event][time_type],
                                    table->s->db.str, table->s->table_name.str);

    /* Check that the definer has TRIGGER privilege on the subject table. */

    if (!(subject_table_grants[event][time_type].privilege & TRIGGER_ACL))
    {
      char priv_desc[128];
      get_privilege_desc(priv_desc, sizeof(priv_desc), TRIGGER_ACL);

      my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), priv_desc,
               thd->security_ctx->priv_user, thd->security_ctx->host_or_ip,
               table->s->table_name.str);

      sp_restore_security_context(thd, save_ctx);
      return TRUE;
    }
#endif // NO_EMBEDDED_ACCESS_CHECKS

    thd->reset_sub_statement_state(&statement_state, SUB_STMT_TRIGGER);
    err_status= sp_trigger->execute_trigger
      (thd, table->s->db.str, table->s->table_name.str,
       &subject_table_grants[event][time_type]);
    thd->restore_sub_statement_state(&statement_state);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    sp_restore_security_context(thd, save_ctx);
#endif // NO_EMBEDDED_ACCESS_CHECKS
  }

  return err_status;
}


/*
  Mark fields of subject table which we read/set in its triggers as such.

  SYNOPSIS
    mark_fields_used()
      thd    Current thread context
      event  Type of event triggers for which we are going to inspect

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
        bitmap_set_bit(table->read_set, trg_field->field_idx);
        if (trg_field->get_settable_routine_parameter())
          bitmap_set_bit(table->write_set, trg_field->field_idx);
      }
    }
  }
  table->file->column_bitmaps_signal();
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
                                                            gptr base,
                                                            MEM_ROOT *mem_root,
                                                            char *end)
{
  DBUG_ENTER("Handle_old_incorrect_sql_modes_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key:%60s", unknown_key));

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
process_unknown_string(char *&unknown_key, gptr base, MEM_ROOT *mem_root,
                       char *end)
{
  DBUG_ENTER("Handle_old_incorrect_trigger_table_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key:%60s", unknown_key));

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
