/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#define MYSQL_LEX 1

#include "my_global.h"
#include "table_trigger_dispatcher.h"
#include "sql_priv.h"
#include "unireg.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_parse.h"                          // parse_sql
#include "parse_file.h"
#include "sp.h"
#include "sql_base.h"                          // find_temporary_table
#include "sql_show.h"                // append_definer, append_identifier
#include "sql_table.h"                        // build_table_filename,
                                              // check_n_cut_mysql50_prefix
#include "sql_db.h"                        // get_default_db_collation
#include "sql_acl.h"                       // *_ACL, is_acl_user
#include "sql_handler.h"                        // mysql_ha_rm_tables
#include "sp_cache.h"                     // sp_invalidate_cache

#include "trigger_loader.h"

///////////////////////////////////////////////////////////////////////////

/**
  Create trigger for table.

  @param thd           current thread context (including trigger definition in
                       LEX)
  @param tables        table list containing one open table for which the
                       trigger is created.
  @param[out] stmt_query    after successful return, this string contains
                            well-formed statement for creation this trigger.

  @note
    - Assumes that trigger name is fully qualified.
    - NULL-string means the following LEX_STRING instance:
    { str = 0; length = 0 }.
    - In other words, definer_user and definer_host should contain
    simultaneously NULL-strings (non-SUID/old trigger) or valid strings
    (SUID/new trigger).

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::create_trigger(THD *thd, TABLE_LIST *tables,
                                              String *stmt_query)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  LEX_STRING *trg_def;
  LEX_STRING definer_user;
  LEX_STRING definer_host;
  sql_mode_t trg_sql_mode;
  LEX_STRING *trg_definer;
  LEX_STRING *trg_client_cs_name;
  LEX_STRING *trg_connection_cl_name;
  LEX_STRING *trg_db_cl_name;

  if (check_for_broken_triggers())
    return true;

  /* Trigger must be in the same schema as target table. */
  if (my_strcasecmp(table_alias_charset, table->s->db.str,
                    lex->spname->m_db.str))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return true;
  }

  if (Trigger_loader::check_for_uniqueness(tables->db,
                                           thd->lex->spname->m_name.str))
    return true;

  sp_head *trg= lex->sphead;

  /* We don't allow creation of several triggers of the same type yet */
  if (get_trigger(trg->m_trg_chistics.event, trg->m_trg_chistics.action_time))
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "multiple triggers with the same action time"
             " and event for one table");
    return true;
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
        return true;
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
      return true;
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
  m_old_field= table->field;
  m_new_field= table->field;

  for (Item_trigger_field *trg_field= lex->sphead->m_trg_table_fields.first;
       trg_field; trg_field= trg_field->next_trg_field)
  {
    /*
      NOTE: now we do not check privileges at CREATE TRIGGER time. This will
      be changed in the future.
    */
    trg_field->setup_field(thd, table, this, NULL);

    if (!trg_field->fixed &&
        trg_field->fix_fields(thd, (Item **)0))
      return true;
  }


  /*
    Soon we will invalidate table object and thus Table_trigger_dispatcher
    object so don't care about place to which trg_def->ptr points and other
    invariants (e.g. we don't bother to update names_list)

    QQ: Hmm... probably we should not care about setting up active thread
        mem_root too.
  */
  if (!(trg_def= alloc_lex_string(&table->mem_root)) ||
      !(trg_definer= alloc_lex_string(&table->mem_root)) ||
      !(trg_client_cs_name= alloc_lex_string(&table->mem_root)) ||
      !(trg_connection_cl_name= alloc_lex_string(&table->mem_root)) ||
      !(trg_db_cl_name= alloc_lex_string(&table->mem_root)))
  {
    return true;
  }

  trg_sql_mode= thd->variables.sql_mode;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (lex->definer && !is_acl_user(lex->definer->host.str,
                                   lex->definer->user.str))
  {
    push_warning_printf(thd,
                        Sql_condition::SL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  if (lex->definer)
  {
    char trg_definer_buf[USER_HOST_BUFF_SIZE];
    /* SUID trigger. */

    definer_user= lex->definer->user;
    definer_host= lex->definer->host;

    size_t trg_definer_len= strxmov(trg_definer_buf, definer_user.str, "@",
                                    definer_host.str, NullS) - trg_definer_buf;
    lex_string_copy(&table->mem_root, trg_definer, trg_definer_buf,
                    trg_definer_len);
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

  /*
    Append definer-clause if the trigger is SUID (a usual trigger in
    new MySQL versions).
  */

  append_definer(thd, stmt_query, &definer_user, &definer_host);

  LEX_STRING stmt_definition;
  stmt_definition.str= (char*) thd->lex->stmt_definition_begin;
  stmt_definition.length= thd->lex->stmt_definition_end
    - thd->lex->stmt_definition_begin;
  trim_whitespace(thd->charset(), & stmt_definition);

  stmt_query->append(stmt_definition.str, stmt_definition.length);

  lex_string_copy(&table->mem_root, trg_def, stmt_query->c_ptr(),
                  stmt_query->length());

  Trigger *new_trigger=
      new (&table->mem_root) Trigger(&trigger_table->s->db,
                                     &trigger_table->s->table_name,
                                     trg_def, trg_sql_mode,
                                     trg_definer, trg_client_cs_name,
                                     trg_connection_cl_name, trg_db_cl_name);

  new_trigger->set_trigger_name(&lex->spname->m_name);

  if (m_triggers.push_back(new_trigger, &table->mem_root))
  {
    delete new_trigger;
    return true;
  }

  return Trigger_loader::store_trigger(tables, new_trigger, &m_triggers);
}


/**
  Drop trigger for table.

  @param thd           current thread context
                       (including trigger definition in LEX)
  @param tables        table list containing one open table for which trigger
                       is dropped.
  @param[out] stmt_query    after successful return, this string contains
                            well-formed statement for creation this trigger.

  @todo
    Probably instead of removing .TRG file we should move
    to archive directory but this should be done as part of
    parse_file.cc functionality (because we will need it
    elsewhere).

  @retval
    False   success
  @retval
    True    error
*/
bool Table_trigger_dispatcher::drop_trigger(THD *thd, TABLE_LIST *tables,
                                            String *stmt_query)
{
  const char *sp_name= thd->lex->spname->m_name.str; // alias
  stmt_query->append(thd->query(), thd->query_length());

  return Trigger_loader::drop_trigger(tables, sp_name, &m_triggers);
}


Table_trigger_dispatcher::~Table_trigger_dispatcher()
{
  // TODO: use m_triggers for destroying triggers.
  for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
    for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
      delete m_trigger_map[i][j];

  if (m_record1_field)
  {
    for (Field **fld_ptr= m_record1_field; *fld_ptr; fld_ptr++)
      delete *fld_ptr;
  }
}


/**
  Prepare array of Field objects referencing to TABLE::record[1] instead
  of record[0] (they will represent OLD.* row values in ON UPDATE trigger
  and in ON DELETE trigger which will be called during REPLACE execution).

  @retval
    False   success
  @retval
    True    error
*/

bool Table_trigger_dispatcher::prepare_record1_accessors()
{
  Field **fld, **old_fld;

  m_record1_field= (Field **) alloc_root(
    &trigger_table->mem_root,
    (trigger_table->s->fields + 1) * sizeof (Field*));

  if (!m_record1_field)
    return true;

  for (fld= trigger_table->field, old_fld= m_record1_field; *fld; fld++, old_fld++)
  {
    /*
      QQ: it is supposed that it is ok to use this function for field
      cloning...
    */
    *old_fld= (*fld)->new_field(&trigger_table->mem_root, trigger_table,
                                trigger_table == (*fld)->table);

    if (!(*old_fld))
      return true;

    (*old_fld)->move_field_offset(
      (my_ptrdiff_t)(trigger_table->record[1] - trigger_table->record[0]));
  }

  *old_fld= 0;

  return false;
}


/**
  Adjust Table_trigger_dispatcher with new TABLE pointer.

  @param new_table   new pointer to TABLE instance
*/

void Table_trigger_dispatcher::set_table(TABLE *new_table)
{
  trigger_table= new_table;
  for (Field **field= new_table->triggers->m_record1_field ; *field ; field++)
  {
    (*field)->table= (*field)->orig_table= new_table;
    (*field)->table_name= &new_table->alias;
  }
}


/**
  Check whenever .TRG file for table exist and load all triggers it contains.

  @param thd          current thread context
  @param db_name      table's database name
  @param table_name   table's name
  @param table        pointer to table object
  @param names_only   stop after loading trigger names

  @todo
    A lot of things to do here e.g. how about other funcs and being
    more paranoical ?

  @todo
    This could be avoided if there is no triggers for UPDATE and DELETE.

  @retval
    False   success
  @retval
    True    error
*/

bool Table_trigger_dispatcher::check_n_load(THD *thd,
                                            const char *db_name,
                                            const char *table_name,
                                            TABLE *table,
                                            bool names_only)
{
  DBUG_ENTER("Table_trigger_dispatcher::check_n_load");

  Table_trigger_dispatcher *table_trigger_dispatcher=
    new (&table->mem_root) Table_trigger_dispatcher(table);

  if (!table_trigger_dispatcher)
    DBUG_RETURN(true);

  bool triggers_not_found;

  if (Trigger_loader::load_triggers(thd, db_name, table_name, table,
                                    &table_trigger_dispatcher->m_triggers,
                                    &triggers_not_found))
  {
    delete table_trigger_dispatcher;
    DBUG_RETURN(!triggers_not_found);
  }

  table_trigger_dispatcher->parse_triggers(thd);

  // TODO: This could be avoided if there is no triggers for UPDATE and DELETE.

  if (!names_only && table_trigger_dispatcher->prepare_record1_accessors())
  {
    delete table_trigger_dispatcher;
    DBUG_RETURN(true);
  }

  table_trigger_dispatcher->setup_triggers(thd, names_only);

  /*
    Finally, assign TABLE::triggers. Note, it should be done at the end of the
    initialization. Otherwise, we might end up having a dangling pointer there
    in case of an error.
  */

  table->triggers= table_trigger_dispatcher;

  // That's it.

  DBUG_RETURN(false);
}


/**
  This is an internal function, which is called when triggers are just loaded
  into Table_trigger_dispatcher::m_triggers list. The function initializes
  m_trigger_map array and calls Trigger::setup_fields() for every trigger.
*/

void Table_trigger_dispatcher::setup_triggers(THD *thd, bool names_only)
{
  List_iterator_fast<Trigger> it(m_triggers);

  while (true)
  {
    Trigger *t= it++;

    if (!t)
      break;

    m_trigger_map[t->get_event()][t->get_action_time()]= t;

    if (!names_only)
      t->setup_fields(thd, trigger_table, this);
  }
}


/**
  Obtains and returns trigger metadata.

  @param thd           current thread context
  @param event         trigger event type
  @param action_time   trigger action time
  @param trigger_name  returns name of trigger
  @param trigger_stmt  returns statement of trigger
  @param sql_mode      returns sql_mode of trigger
  @param definer       returns definer/creator of trigger. The caller is
                       responsible to allocate enough space for storing
                       definer information.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::get_trigger_info(THD *thd, trg_event_type event,
                                                trg_action_time_type action_time,
                                                LEX_STRING *trigger_name,
                                                LEX_STRING *trigger_stmt,
                                                sql_mode_t *sql_mode,
                                                LEX_STRING *definer,
                                                LEX_STRING *client_cs_name,
                                                LEX_STRING *connection_cl_name,
                                                LEX_STRING *db_cl_name)
{
  DBUG_ENTER("get_trigger_info");

  Trigger *trigger= get_trigger(event, action_time);

  if (!trigger)
    DBUG_RETURN(true);

  trigger->get_info(trigger_name, trigger_stmt, sql_mode, definer, NULL,
                    client_cs_name, connection_cl_name, db_cl_name);

  DBUG_RETURN(false);
}


/**
  Get information about trigger by its index.

  @param [in] thd                  thread handle
  @param [in] trigger_idx          ordinal number of trigger in the list
  @param [out] trigger_name        pointer to variable where to store
                                   the trigger name
  @param [out] sql_mode            pointer to variable where to store
                                   the sql mode
  @param [out] sql_original_stmt   pointer to variable where to store
                                   the trigger definition
  @param [out] client_cs_name      client character set
  @param [out] connection_cl_name  connection collation
  @param [out] db_cl_name          database collation
*/

void Table_trigger_dispatcher::get_trigger_info(THD *thd,
                                                int trigger_idx,
                                                LEX_STRING *trigger_name,
                                                sql_mode_t *sql_mode,
                                                LEX_STRING *sql_original_stmt,
                                                LEX_STRING *client_cs_name,
                                                LEX_STRING *connection_cl_name,
                                                LEX_STRING *db_cl_name)
{
  List_iterator_fast<Trigger> it_table_triggers(m_triggers);
  for (int i = 0; i < trigger_idx; ++i)
  {
    it_table_triggers.next_fast();
  }

  Trigger *found_trigger= it_table_triggers++;
  found_trigger->get_info(trigger_name, NULL, sql_mode, NULL,
                          sql_original_stmt, client_cs_name,
                          connection_cl_name, db_cl_name);
}


/**
  Get trigger ordinal number by trigger name.

  @param [in] trigger_name        trigger name

  @return  ordinal number of trigger in the list
    @retval -1  if trigger not found
    @retval >= 0  trigger index
*/

int Table_trigger_dispatcher::find_trigger_by_name(const LEX_STRING *trg_name)
{
  List_iterator_fast<Trigger> it_table_triggers(m_triggers);
  for (int i = 0; ; ++i)
  {
    Trigger *trigger= it_table_triggers++;

    if (!trigger)
      return -1;

    if (strcmp(trigger->get_trigger_name()->str, trg_name->str) == 0)
      return i;
  }
}


/**
  Drop all triggers for table.

  @param thd      current thread context
  @param db_name  schema for table
  @param name     name for table

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::drop_all_triggers(THD *thd, char *db_name, char *name)
{
  TABLE table;
  bool result= 0;
  DBUG_ENTER("drop_all_triggers");

  memset(&table, 0, sizeof(table));
  init_sql_alloc(&table.mem_root, 8192, 0);

  if (Table_trigger_dispatcher::check_n_load(thd, db_name, name, &table, 1))
  {
    result= 1;
    goto end;
  }

  if (table.triggers)
  {
    result= Trigger_loader::drop_all_triggers(
        db_name, name,
        &table.triggers->m_triggers);

  }

end:
  if (table.triggers)
    delete table.triggers;
  table.triggers= NULL;
  free_root(&table.mem_root, MYF(0));
  DBUG_RETURN(result);
}


/**
  Update .TRG file after renaming triggers' subject table
  (change name of table in triggers' definitions).

  @param thd                 Thread context
  @param old_db_name         Old database of subject table
  @param new_db_name         New database of subject table
  @param old_table_name      Old subject table's name
  @param new_table_name      New subject table's name

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::change_table_name_in_triggers(
  THD *thd,
  const char *old_db_name,
  const char *new_db_name,
  LEX_STRING *old_table_name,
  LEX_STRING *new_table_name,
  bool upgrading50to51)
{
  LEX_STRING *def, *on_table_name, new_def;
  sql_mode_t save_sql_mode= thd->variables.sql_mode;
  List_iterator_fast<Trigger> it_table_triggers(m_triggers);
  size_t on_q_table_name_len, before_on_len;
  String buff;
  Trigger *trigger;

  while ((trigger= it_table_triggers++))
  {
    def= trigger->get_definition();
    on_table_name= trigger->get_on_table_name();
    thd->variables.sql_mode= *trigger->get_sql_mode();

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
    return true; /* OOM */

  if (Trigger_loader::rename_table_in_trigger(trigger_table,
                                              &m_triggers,
                                              old_db_name, old_table_name,
                                              new_db_name, new_table_name,
                                              upgrading50to51))
    return true;

  return false;
}


/**
  Update .TRG and .TRN files after renaming triggers' subject table.

  @param[i]] thd        Thread context
  @param[in] db_name    Old (current) database of subject table
  @param[in] old_alias  Old (current) alias of subject table
  @param[in] old_table  Old (current) name of subject table
  @param[in] new_db     New database for subject table
  @param[in] new_table  New name of subject table

  @note
    This method tries to leave trigger related files in consistent state,
    i.e. it either will complete successfully, or will fail leaving files
    in their initial state.
    Also this method assumes that subject table is not renamed to itself.
    This method needs to be called under an exclusive table metadata lock.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::change_table_name(THD *thd,
                                                 const char *db_name,
                                                 const char *old_alias,
                                                 const char *old_table,
                                                 const char *new_db,
                                                 const char *new_table)
{
  TABLE table;
  bool result= false;
  bool upgrading50to51= false;
  DBUG_ENTER("change_table_name");

  memset(&table, 0, sizeof(table));
  init_sql_alloc(&table.mem_root, 8192, 0);

  /*
    This method interfaces the mysql server code protected by
    an exclusive metadata lock.
  */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE, db_name, old_table,
                                             MDL_EXCLUSIVE));

  DBUG_ASSERT(my_strcasecmp(table_alias_charset, db_name, new_db) ||
              my_strcasecmp(table_alias_charset, old_alias, new_table));

  if (Table_trigger_dispatcher::check_n_load(thd, db_name, old_table, &table, true))
  {
    result= true;
    goto end;
  }
  if (table.triggers)
  {
    if (table.triggers->check_for_broken_triggers())
    {
      result= true;
      goto end;
    }
    LEX_STRING old_table_name= { (char *) old_alias, strlen(old_alias) };
    LEX_STRING new_table_name= { (char *) new_table, strlen(new_table) };
    /*
      Since triggers should be in the same schema as their subject tables
      moving table with them between two schemas raises too many questions.
      (E.g. what should happen if in new schema we already have trigger
       with same name ?).
       
      In case of "ALTER DATABASE `#mysql50#db1` UPGRADE DATA DIRECTORY NAME"
      we will be given table name with "#mysql50#" prefix
      To remove this prefix we use check_n_cut_mysql50_prefix().
    */
    if (my_strcasecmp(table_alias_charset, db_name, new_db))
    {
      char dbname[NAME_LEN + 1];
      if (check_n_cut_mysql50_prefix(db_name, dbname, sizeof(dbname)) && 
          !my_strcasecmp(table_alias_charset, dbname, new_db))
      {
        upgrading50to51= true;
      }
      else
      {
        my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
        result= true;
        goto end;
      }
    }

    if (table.triggers->change_table_name_in_triggers(thd, db_name, new_db,
                                                      &old_table_name,
                                                      &new_table_name,
                                                      upgrading50to51))
    {
      result= true;
      goto end;
    }
  }
  
end:
  delete table.triggers;
  table.triggers= NULL;
  free_root(&table.mem_root, MYF(0));
  DBUG_RETURN(result);
}


/**
  Parse trigger definition statements (CREATE TRIGGER).

  @param [in] thd                  thread handle

  @return Operation status.
    @retval true   Failure
    @retval false  Success
*/

bool Table_trigger_dispatcher::parse_triggers(THD *thd)
{
  List_iterator<Trigger> it(m_triggers);

  while (true)
  {
    Trigger *t= it++;

    if (!t)
      break;

    bool result= t->parse_trigger_body(thd, trigger_table);

    if (result || t->has_parse_error())
    {
      /*
        Remember the parse error message
        (only the 1st error message will be remembered).
      */

      set_parse_error_message(t->get_parse_error_message());

      if (result)
      {
        /*
          The method Trigger::parse_trigger_body() returns true when the
          trigger definition is unparseable, e.g. instead of SQL-statement
          'CREATE TRIGGER trg1 ...'  the trigger definition contains
          the string 'bla-bla-bla'. In this case we remove the trigger object
          from the list.

          TODO: XXX: FIXME: that seems to be not the case.
          result is true when parse_trigger_body() returns true.
          And parse_trigger_body() returns true when OOM happens,
          not when the parsing failed.
        */

        delete t;
        it.remove();
      }

      continue;
    }

    DBUG_ASSERT(!t->has_parse_error());

    sp_head *sp= t->get_sp();

    if (sp)
      sp->m_trg_list= this;
  }

  return false;
}


/**
  Execute trigger for given (event, time) pair.

  The operation executes trigger for the specified event (insert, update,
  delete) and time (after, before) if it is set.

  @param thd
  @param event
  @param action_time
  @param old_row_is_record1

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::process_triggers(THD *thd,
                                                trg_event_type event,
                                                trg_action_time_type action_time,
                                                bool old_row_is_record1)
{
  Trigger *trigger= get_trigger(event, action_time);

  if (check_for_broken_triggers())
    return true;

  if (trigger == NULL)
    return false;

  if (old_row_is_record1)
  {
    m_old_field= m_record1_field;
    m_new_field= trigger_table->field;
  }
  else
  {
    m_new_field= m_record1_field;
    m_old_field= trigger_table->field;
  }
  /*
    This trigger must have been processed by the pre-locking
    algorithm.
  */
  DBUG_ASSERT(trigger_table->pos_in_table_list->trg_event_map &
              static_cast<uint>(1 << static_cast<int>(event)));

  return trigger->execute(thd);
}


/**
  Add triggers for table to the set of routines used by statement.
  Add tables used by them to statement table list. Do the same for
  routines used by triggers.

  @param thd             Thread context.
  @param prelocking_ctx  Prelocking context of the statement.
  @param table_list      Table list element for table with trigger.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::add_tables_and_routines_for_triggers(
  THD *thd,
  Query_tables_list *prelocking_ctx,
  TABLE_LIST *table_list)
{
  DBUG_ASSERT(static_cast<int>(table_list->lock_type) >=
              static_cast<int>(TL_WRITE_ALLOW_WRITE));

  for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
  {
    if (table_list->trg_event_map &
        static_cast<uint8>(1 << static_cast<int>(i)))
    {
      for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
      {
        /* We can have only one trigger per action type currently */

        trg_event_type trg_event= (trg_event_type) i;
        trg_action_time_type trg_action_time = (trg_action_time_type) j;

        Trigger *trigger=
          table_list->table->triggers->get_trigger(trg_event,
                                                   trg_action_time);

        if (trigger)
          trigger->add_tables_and_routines(thd, prelocking_ctx, table_list);
      }
    }
  }
  return FALSE;
}


/**
  Mark all trigger fields as "temporary nullable" and remember the current
  THD::count_cuted_fields value.

  @param thd Thread context.
*/
void Table_trigger_dispatcher::enable_fields_temporary_nullability(THD* thd)
{
  for (Field** next_field= trigger_table->field; *next_field; ++next_field)
  {
    (*next_field)->set_tmp_nullable();
    (*next_field)->set_count_cuted_fields(thd->count_cuted_fields);

    /*
      For statement LOAD INFILE we set field values during parsing of data file
      and later run fill_record_n_invoke_before_triggers() to invoke table's
      triggers. fill_record_n_invoke_before_triggers() calls this method
      to enable temporary nullability before running trigger's instructions
      Since for the case of handling statement LOAD INFILE the null value of
      fields have been already set we don't have to reset these ones here.
      In case of handling statements INSERT/REPLACE/INSERT SELECT/
      REPLACE SELECT we set field's values inside method fill_record
      that is called from fill_record_n_invoke_before_triggers()
      after the method enable_fields_temporary_nullability has been executed.
    */
    if (thd->lex->sql_command != SQLCOM_LOAD)
      (*next_field)->reset_tmp_null();
  }
}


/**
  Reset "temporary nullable" flag from trigger fields.
*/

void Table_trigger_dispatcher::disable_fields_temporary_nullability()
{
  for (Field** next_field= trigger_table->field; *next_field; ++next_field)
    (*next_field)->reset_tmp_nullable();
}


/**
  Mark fields of subject table which we read/set in its triggers
  as such.

  This method marks fields of subject table which are read/set in its
  triggers as such (by properly updating TABLE::read_set/write_set)
  and thus informs handler that values for these fields should be
  retrieved/stored during execution of statement.

  @param event  Type of event triggers for which we are going to inspect
*/

void Table_trigger_dispatcher::mark_fields_used(trg_event_type event)
{
  for (int i= 0; i < (int) TRG_ACTION_MAX; ++i)
  {
    Trigger *trigger= get_trigger(event, (trg_action_time_type) i);

    if (!trigger)
      continue;

    trigger->mark_field_used(trigger_table);
  }

  trigger_table->file->column_bitmaps_signal();
}
