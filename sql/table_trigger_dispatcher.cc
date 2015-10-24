/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "table_trigger_dispatcher.h"

#include "auth_common.h"            // check_global_access
#include "sp_head.h"                // sp_head
#include "sql_parse.h"              // create_default_definer
#include "sql_show.h"               // append_definer
#include "sql_table.h"              // check_n_cut_mysql50_prefix

#include "trigger_loader.h"
#include "trigger_chain.h"
#include "trigger.h"

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  Create an instance of Table_trigger_dispatcher for the given subject table.

  @param subject_table  valid (not fake!) TABLE-object representing
                        the subject table

  @return a pointer to a new Table_trigger_dispatcher instance.
*/
Table_trigger_dispatcher *Table_trigger_dispatcher::create(TABLE *subject_table)
{
  return
    new (&subject_table->mem_root) Table_trigger_dispatcher(subject_table);
}


/**
  Private form of Table_trigger_dispatcher constructor. In order to construct an
  instance of Table_trigger_dispatcher with a valid pointer to the subject
  table, use Table_trigger_dispatcher::create().
*/
Table_trigger_dispatcher::Table_trigger_dispatcher(TABLE *subject_table)
 :m_subject_table(subject_table),
  m_unparseable_triggers(NULL),
  m_record1_field(NULL),
  m_new_field(NULL),
  m_old_field(NULL),
  m_has_unparseable_trigger(false)
{
  memset(m_trigger_map, 0, sizeof(m_trigger_map));
  m_parse_error_message[0]= 0;
  m_db_name.str= subject_table->s->db.str;
  m_db_name.length= subject_table->s->db.length;
  m_subject_table_name.str= subject_table->s->table_name.str;
  m_subject_table_name.length= subject_table->s->table_name.length;
}


/**
  Create a Table_trigger_dispatcher instance, which should serve the subject
  table specified by database / table name. This form should be used when
  Table_trigger_dispatcher is created temporary and there is no initialized
  TABLE-object for the subject table.
*/
Table_trigger_dispatcher::Table_trigger_dispatcher(const char *db_name,
                                                   const char *table_name)
 :m_subject_table(NULL),
  m_unparseable_triggers(NULL),
  m_record1_field(NULL),
  m_new_field(NULL),
  m_old_field(NULL),
  m_has_unparseable_trigger(false)
{
  memset(m_trigger_map, 0, sizeof(m_trigger_map));
  m_parse_error_message[0]= 0;

  init_sql_alloc(key_memory_Table_trigger_dispatcher,
                 &m_mem_root, 8192, 0);

  lex_string_copy(get_mem_root(), &m_db_name, db_name);
  lex_string_copy(get_mem_root(), &m_subject_table_name, table_name);
}


Table_trigger_dispatcher::~Table_trigger_dispatcher()
{
  // Destroy all Trigger objects in m_triggers.

  m_triggers.delete_elements();

  // Destroy fields.

  if (m_record1_field)
  {
    for (Field **fld_ptr= m_record1_field; *fld_ptr; fld_ptr++)
      delete *fld_ptr;
  }

  // Destroy trigger chains.

  for (int i= 0; i < (int) TRG_ACTION_MAX; ++i)
    for (int j= 0; j < (int) TRG_ACTION_MAX; ++j)
      delete m_trigger_map[i][j];

  delete m_unparseable_triggers;

  // Destroy memory root if it was allocated.

  if (!m_subject_table)
    free_root(&m_mem_root, MYF(0));
}


/**
  Create trigger for table.

  @param      thd   thread context
  @param[out] binlog_create_trigger_stmt
                    well-formed CREATE TRIGGER statement for putting into binlog
                    (after successful execution)

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

bool Table_trigger_dispatcher::create_trigger(
  THD *thd, String *binlog_create_trigger_stmt)
{
  LEX *lex= thd->lex;

  // If this table has broken triggers, CREATE TRIGGER is not allowed.

  if (check_for_broken_triggers())
    return true;

  // Check that the new trigger is in the same schema as the base table.

  if (my_strcasecmp(table_alias_charset, m_db_name.str, lex->spname->m_db.str))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return true;
  }

  // Check that the trigger does not exist.

  {
    char trn_path_buffer[FN_REFLEN];

    LEX_STRING trn_path =
      Trigger_loader::build_trn_path(trn_path_buffer, FN_REFLEN,
                                     thd->lex->spname->m_db.str,
                                     thd->lex->spname->m_name.str);

    if (!trn_path.str)
      return true;

    if (!Trigger_loader::check_trn_exists(trn_path))
    {
      my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
      return true;
    }
  }

  // Make sure DEFINER clause is specified.

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

      NOTE: here we allocate lex->definer on THD->mem_root. Later it will be
      copied into the base table mem-root to be used inside Trigger.
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
      (strcmp(lex->definer->user.str,
              thd->security_context()->priv_user().str) ||
       my_strcasecmp(system_charset_info,
                     lex->definer->host.str,
                     thd->security_context()->priv_host().str)))
  {
    if (check_global_access(thd, SUPER_ACL))
    {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
      return true;
    }
  }

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

    if (thd->get_stmt_da()->is_error())
      return true;
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  /*
    Check if all references to fields in OLD/NEW-rows in this trigger are valid.

    NOTE: Setting m_old_field / m_new_field here is required because of
    Item_trigger_field::fix_fields() later.

    NOTE: We do it here more from ease of use standpoint. We still have to
    do some checks on each execution. E.g. we can catch privilege changes
    only during execution. Also in near future, when we will allow access
    to other tables from trigger we won't be able to catch changes in other
    tables...

    Since we don't plan to access to contents of the fields it does not
    matter that we choose for both OLD and NEW values the same versions
    of Field objects here.
   */

  DBUG_ASSERT(m_subject_table);

  m_old_field= m_subject_table->field;
  m_new_field= m_subject_table->field;

  if (lex->sphead->setup_trigger_fields(thd, get_trigger_field_support(),
                                        NULL, true))
    return true;

  m_old_field= NULL;
  m_new_field= NULL;

  // Create ne trigger.

  Trigger *t= Trigger::create_from_parser(thd,
                                          m_subject_table,
                                          binlog_create_trigger_stmt);

  if (!t)
    return true;

  // Create trigger chain.

  Trigger_chain *tc= create_trigger_chain(t->get_event(),
                                          t->get_action_time());

  if (!tc)
  {
    delete t; // that's not needed, but it's good manner.
    return true;
  }

  // Add the newly created trigger to the chain.

  if (tc->add_trigger(get_mem_root(), t,
                      lex->sphead->m_trg_chistics.ordering_clause,
                      lex->sphead->m_trg_chistics.anchor_trigger_name))
  {
    delete t; // that's not needed, but it's good manner.
    return true;
  }

  /*
    NOTE: no need to renumerate_triggers() as this is only for storing a new
    trigger in Data Dictionary.
  */

  /*
    Now it's necessary to prepare a list of all triggers assigned to a table.
    The current state of m_triggers might be not appropriate because a new
    trigger was added to a trigger chain lately.
  */

  return rebuild_trigger_list() ||
         Trigger_loader::store_trigger(m_db_name, m_subject_table_name,
                                       get_mem_root(), t, &m_triggers);
}


/**
  Drop trigger for table.

  @param thd                  thread context
  @param trigger_name         name of the trigger to drop
  @param [out] trigger_found  out-flag to determine if the trigger found

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::drop_trigger(THD *thd,
                                            const LEX_STRING &trigger_name,
                                            bool *trigger_found)
{
  if (rebuild_trigger_list() ||
      Trigger_loader::drop_trigger(m_db_name,
                                   m_subject_table_name,
                                   trigger_name,
                                   get_mem_root(),
                                   &m_triggers,
                                   trigger_found))
    return true;

  if (*trigger_found)
    return false;

  DBUG_ASSERT (!*trigger_found);
  my_message(ER_TRG_DOES_NOT_EXIST, ER(ER_TRG_DOES_NOT_EXIST), MYF(0));

  return true;
}


/**
  Prepare array of Field objects referencing to TABLE::record[1] instead
  of record[0] (they will represent OLD.* row values in ON UPDATE trigger
  and in ON DELETE trigger which will be called during REPLACE execution).

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::prepare_record1_accessors()
{
  Field **fld, **old_fld;

  DBUG_ASSERT(m_subject_table);

  m_record1_field= (Field **) alloc_root(
    get_mem_root(),
    (m_subject_table->s->fields + 1) * sizeof (Field*));

  if (!m_record1_field)
    return true;

  for (fld= m_subject_table->field, old_fld= m_record1_field;
       *fld; fld++, old_fld++)
  {
    /*
      QQ: it is supposed that it is ok to use this function for field
      cloning...
    */
    *old_fld= (*fld)->new_field(get_mem_root(), m_subject_table,
                                m_subject_table == (*fld)->table);

    if (!(*old_fld))
      return true;

    (*old_fld)->move_field_offset(
      (my_ptrdiff_t)(m_subject_table->record[1] - m_subject_table->record[0]));
  }

  *old_fld= 0;

  return false;
}


/**
  Load triggers for the table specified by the db_name and table_name pair.

  @note The table object passed to this function can be fake. This is usually
  happens when names_only is set. This is the case when triggers should be
  loaded just to get their names (in order to get a trigger name, the trigger
  definition has to be parsed; trigger names are not stored in TRG files).

  @note If table object is fake, only its memory root can be used.

  @param thd          current thread context
  @param db_name      table's database name
  @param table_name   table's name
  @param table        pointer to table object
  @param names_only   stop after loading trigger names

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::check_n_load(THD *thd, bool names_only)
{
  // Load triggers from Data Dictionary.

  if (Trigger_loader::load_triggers(thd,
                                    get_mem_root(),
                                    m_db_name.str,
                                    m_subject_table_name.str,
                                    &m_triggers))
  {
    return true;
  }

  // Parse trigger definitions (names are known after this point).

  parse_triggers(thd);

  // Create m_unparseable_triggers if needed.

  if (m_has_unparseable_trigger)
  {
    m_unparseable_triggers= new (get_mem_root()) Trigger_chain();

    if (!m_unparseable_triggers)
      return true;
  }

  // Create trigger chains and assigns triggers to chains.

  {
    List_iterator_fast<Trigger> it(m_triggers);
    Trigger *t;

    while ((t= it++))
    {
      Trigger_chain *tc= t->has_parse_error() ?
                         m_unparseable_triggers :
                         create_trigger_chain(t->get_event(),
                                              t->get_action_time());

      if (!tc || tc->add_trigger(get_mem_root(), t))
        return true;
    }
  }

  // Update action_order trigger attribute.

  for (int i= 0; i < (int) TRG_EVENT_MAX; i++)
  {
    for (int j= 0; j < (int) TRG_ACTION_MAX; j++)
    {
      Trigger_chain *tc= get_triggers(i, j);

      if (tc)
        tc->renumerate_triggers();
    }
  }

  if (!names_only) // if we're doing complete trigger loading
  {
    // Prepare fields for the OLD-row.

    if (has_update_triggers() || has_delete_triggers())
    {
      if (prepare_record1_accessors())
        return true;
    }

    /*
      Bind Item_trigger_field in the trigger's SP-head to this
      Table_trigger_dispatcher object.
    */

    List_iterator_fast<Trigger> it(m_triggers);
    Trigger *t;

    while ((t= it++))
    {
      sp_head *sp= t->get_sp();

      if (!sp)
        continue;

      sp->setup_trigger_fields(thd, get_trigger_field_support(),
                               t->get_subject_table_grant(), false);
    }
  }

  return false;
}


/**
  Make sure there is a chain for the specified event and action time.

  @return A pointer to newly created Trigger_chain object,
  NULL in case of OOM error.
*/

Trigger_chain *Table_trigger_dispatcher::create_trigger_chain(
  enum_trigger_event_type event,
  enum_trigger_action_time_type action_time)
{
  DBUG_ASSERT(event != TRG_EVENT_MAX);
  DBUG_ASSERT(action_time != TRG_ACTION_MAX);

  Trigger_chain *tc= get_triggers(event, action_time);

  if (tc)
    return tc;

  tc= new (get_mem_root()) Trigger_chain();

  if (tc)
    m_trigger_map[event][action_time]= tc;

  return tc;
}


/**
  Rebuild trigger list so that triggers there are listed in the proper order.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::rebuild_trigger_list()
{
  m_triggers.empty();

  // Add triggers from regular chains.

  for (int i= 0; i < (int) TRG_EVENT_MAX; ++i)
  {
    for (int j= 0; j < (int) TRG_ACTION_MAX; ++j)
    {
      Trigger_chain *tc= get_triggers(i, j);

      if (!tc)
        continue;

      List_iterator<Trigger> it(tc->get_trigger_list());
      Trigger *t;

      while ((t= it++))
      {
        if (m_triggers.push_back(t, get_mem_root()))
          return true;
      }
    }
  }

  // Add unparseable triggers.

  if (m_unparseable_triggers)
  {
    List_iterator<Trigger> it(m_unparseable_triggers->get_trigger_list());
    Trigger *t;

    while ((t= it++))
    {
      if (m_triggers.push_back(t, get_mem_root()))
        return true;
    }
  }

  return false;
}


/**
  Get trigger object by trigger name.

  @param [in] trigger_name  trigger name

  @return a pointer to Trigger object, NULL if the trigger not found.
*/

Trigger *Table_trigger_dispatcher::find_trigger(const LEX_STRING &trigger_name)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
  {
    if (my_strcasecmp(table_alias_charset,
                      t->get_trigger_name().str,
                      trigger_name.str) == 0)
    {
      return t;
    }
  }

  return NULL;
}


/**
  Update .TRG file after renaming triggers' subject table
  (change name of table in triggers' definitions).

  @param thd                 Thread context
  @param old_db_name         Old database of subject table
  @param new_db_name         New database of subject table
  @param old_table_name_str  Old subject table's name
  @param new_table_name_str  New subject table's name

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool Table_trigger_dispatcher::rename_subject_table(
  THD *thd,
  const char *old_db_name,
  const char *new_db_name,
  const char *old_table_name_str,
  const char *new_table_name_str,
  bool upgrading50to51)
{
  LEX_STRING old_table_name;
  lex_string_set(&old_table_name, old_table_name_str);

  LEX_STRING new_table_name;
  lex_string_set(&new_table_name, new_table_name_str);

  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
    t->rename_subject_table(thd, new_table_name);

  if (thd->is_fatal_error)
    return true; /* OOM */

  return Trigger_loader::rename_subject_table(get_mem_root(),
                                              &m_triggers,
                                              old_db_name, &old_table_name,
                                              new_db_name, &new_table_name,
                                              upgrading50to51);
}


/**
  Parse trigger definition statements (CREATE TRIGGER).

  @param [in] thd   Thread context
*/

void Table_trigger_dispatcher::parse_triggers(THD *thd)
{
  List_iterator<Trigger> it(m_triggers);

  while (true)
  {
    Trigger *t= it++;

    if (!t)
      break;

    bool fatal_parse_error= t->parse(thd);

    /*
      There are two kinds of parse errors here:

        - "soft errors" -- these are the errors when we were able to parse out
          the trigger name and the base table name. However there was some
          parse error that prevents the trigger from being executed.

          This kind of errors is designated by:
            - fatal_parse_error is false
            - Trigger::has_parse_error() returns true

          In case of these errors we put the failed trigger into the list so
          that it will be shown in the informational statements (queries from
          INFORMATION_SCHEMA and so on), but it's forbidden to execute such
          triggers.

        - "fatal errors" -- there are the errors when we were unable to get
          even basic information about the trigger (or out-of-memory error
          happens). The trigger is in completely useless state.

          This kind of errors is signalled by fatal_parse_error being true.

          In case of these errors we just remember the error message and delete
          the trigger instance (do not put it into the list).
    */

    if (fatal_parse_error || t->has_parse_error())
    {
      DBUG_ASSERT(!t->get_sp()); // SP must be NULL.

      if (t->has_parse_error())
        set_parse_error_message(t->get_parse_error_message());

      if (fatal_parse_error)
      {
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

bool Table_trigger_dispatcher::process_triggers(
  THD *thd,
  enum_trigger_event_type event,
  enum_trigger_action_time_type action_time,
  bool old_row_is_record1)
{
  if (check_for_broken_triggers())
    return true;

  Trigger_chain *tc= get_triggers(event, action_time);

  if (!tc)
    return false;

  DBUG_ASSERT(m_subject_table);

  if (old_row_is_record1)
  {
    m_old_field= m_record1_field;
    m_new_field= m_subject_table->field;
  }
  else
  {
    m_new_field= m_record1_field;
    m_old_field= m_subject_table->field;
  }
  /*
    This trigger must have been processed by the pre-locking
    algorithm.
  */
  DBUG_ASSERT(m_subject_table->pos_in_table_list->trg_event_map &
              static_cast<uint>(1 << static_cast<int>(event)));

  bool rc= tc->execute_triggers(thd);

  m_new_field= NULL;
  m_old_field= NULL;

  return rc;
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

  for (int i= 0; i < (int) TRG_EVENT_MAX; ++i)
  {
    if (table_list->trg_event_map &
        static_cast<uint8>(1 << i))
    {
      for (int j= 0; j < (int) TRG_ACTION_MAX; ++j)
      {
        Trigger_chain *tc= table_list->table->triggers->get_triggers(i, j);

        if (tc)
          tc->add_tables_and_routines(thd, prelocking_ctx, table_list);
      }
    }
  }
  return false;
}


/**
  Mark all trigger fields as "temporary nullable" and remember the current
  THD::count_cuted_fields value.

  @param thd Thread context.
*/

void Table_trigger_dispatcher::enable_fields_temporary_nullability(THD *thd)
{
  DBUG_ASSERT(m_subject_table);

  for (Field **next_field= m_subject_table->field; *next_field; ++next_field)
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
  DBUG_ASSERT(m_subject_table);

  for (Field **next_field= m_subject_table->field; *next_field; ++next_field)
    (*next_field)->reset_tmp_nullable();
}


/**
  Iterate along triggers and print necessary upgrade warnings.

  Now it prints the warning about missing 'CREATED' attribute.

  @param thd Thread context.
*/

void Table_trigger_dispatcher::print_upgrade_warnings(THD *thd)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
    t->print_upgrade_warning(thd);
}


/**
  Mark fields of subject table which we read/set in its triggers
  as such.

  This method marks fields of subject table which are read/set in its
  triggers as such (by properly updating TABLE::read_set/write_set)
  and thus informs handler that values for these fields should be
  retrieved/stored during execution of statement.

  @param event  Type of event triggers for which we are going to inspect

  @returns false if success, true if error
*/

bool Table_trigger_dispatcher::mark_fields(enum_trigger_event_type event)
{
  if (check_for_broken_triggers())
    return true;

  DBUG_ASSERT(m_subject_table);

  for (int i= 0; i < (int) TRG_ACTION_MAX; ++i)
  {
    Trigger_chain *tc= get_triggers(event, i);

    if (!tc)
      continue;

    tc->mark_fields(m_subject_table);
  }

  m_subject_table->file->column_bitmaps_signal();
  return false;
}
