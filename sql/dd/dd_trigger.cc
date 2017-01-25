/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd_trigger.h"

#include <string.h>
#include <memory>
#include <string>                        // std::string

#include "dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "dd/dd.h"                       // dd::get_dictionary
#include "dd/dd_schema.h"                // dd::Schema_MDL_locker
#include "dd/dictionary.h"               // dd::Dictionary
#include "dd/types/schema.h"
#include "dd/types/table.h"              // dd::Table
#include "dd/types/trigger.h"            // dd::Trigger
#include "dd_table_share.h"              // dd_get_mysql_charset
#include "my_dbug.h"
#include "my_global.h"
#include "my_psi_config.h"
#include "my_sys.h"                      // my_error, resolve_collation
#include "mysql/psi/mysql_sp.h"          // MYSQL_DROP_SP
#include "mysql/psi/mysql_statement.h"
#include "mysqld_error.h"                // ER_UNKNOWN_COLLATION
#include "sql_class.h"                   // THD
#include "sql_lex.h"
#include "sql_list.h"                    // List
#include "sql_servers.h"
#include "system_variables.h"
#include "table.h"
#include "transaction.h"                 // trans_commit, trans_rollback
#include "trigger.h"                     // Trigger

namespace dd
{

/**
  Get DD API value of event type for a trigger.

  @param [in]  new_trigger       pointer to a Trigger object from sql-layer.

  @return Value of enumeration dd::Trigger::enum_event_type
*/

static inline Trigger::enum_event_type
get_dd_event_type(const ::Trigger *new_trigger)
{
  switch (new_trigger->get_event())
  {
  case TRG_EVENT_INSERT:
    return Trigger::enum_event_type::ET_INSERT;
  case TRG_EVENT_UPDATE:
    return Trigger::enum_event_type::ET_UPDATE;
  case TRG_EVENT_DELETE:
    return Trigger::enum_event_type::ET_DELETE;
  case TRG_EVENT_MAX:
    break;
  }
  /*
    Since a trigger event is supplied by parser and parser controls that
    a value for trigger event can take only values from the set
    (TRG_EVENT_INSERT, TRG_EVENT_UPDATE, TRG_EVENT_DELETE), it's allowable
    to add an assert to catch violation of this invariant just in case.
  */

  DBUG_ASSERT(false);

  return Trigger::enum_event_type::ET_INSERT;
}


/**
  Get DD API value of action timing for a trigger.

  @param [in]  new_trigger         pointer to a Trigger object from sql-layer.

  @return Value of enumeration Trigger::enum_action_timing
*/

static inline Trigger::enum_action_timing
get_dd_action_timing(const ::Trigger *new_trigger)
{
  switch (new_trigger->get_action_time())
  {
  case TRG_ACTION_BEFORE:
    return Trigger::enum_action_timing::AT_BEFORE;
  case TRG_ACTION_AFTER:
    return Trigger::enum_action_timing::AT_AFTER;
  case TRG_ACTION_MAX:
    break;
  }
  /*
    Since a trigger action time is supplied by parser and parser controls
    that a value for trigger action time can take only values from the set
    (TRG_ACTION_BEFORE, TRG_ACTION_AFTER), it's allowable to add an assert
    to catch violation of this invariant just in case.
  */
  DBUG_ASSERT(false);

  return Trigger::enum_action_timing::AT_BEFORE;
}


/**
  Fill in a dd::Trigger object based on a Trigger object supplied by sql-layer.

  @param [in]   thd               thread handle
  @param [in]   new_trigger       Trigger object supplied by sql-layer
  @param [out]  dd_trig_obj       dd::Trigger object to fill in

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

static bool fill_in_dd_trigger_object(THD *thd, const ::Trigger *new_trigger,
                                      Trigger *dd_trig_obj)
{
  dd_trig_obj->set_name(String_type(new_trigger->get_trigger_name().str,
                                    new_trigger->get_trigger_name().length));
  dd_trig_obj->set_definer(
    String_type(new_trigger->get_definer_user().str,
                new_trigger->get_definer_user().length),
    String_type(new_trigger->get_definer_host().str,
                new_trigger->get_definer_host().length));

  dd_trig_obj->set_event_type(get_dd_event_type(new_trigger));
  dd_trig_obj->set_action_timing(get_dd_action_timing(new_trigger));

  dd_trig_obj->set_action_statement(
    String_type(new_trigger->get_definition().str,
                new_trigger->get_definition().length));

  dd_trig_obj->set_action_statement_utf8(
    String_type(new_trigger->get_definition_utf8().str,
                new_trigger->get_definition_utf8().length));

  dd_trig_obj->set_sql_mode(new_trigger->get_sql_mode());

  const CHARSET_INFO *collation;

  if (resolve_charset(new_trigger->get_client_cs_name().str,
                      system_charset_info,
                      &collation))
  {
    // resolve_charset will not cause an error to be reported if the
    // collation was not found, so we must report error here.
    my_error(ER_UNKNOWN_COLLATION, MYF(0),
             new_trigger->get_client_cs_name().str);
    return true;
  }
  dd_trig_obj->set_client_collation_id(collation->number);

  if (resolve_collation(new_trigger->get_connection_cl_name().str,
                        system_charset_info,
                        &collation))
  {
    // resolve_charset will not cause an error to be reported if the
    // collation was not found, so we must report error here.
    my_error(ER_UNKNOWN_COLLATION, MYF(0),
             new_trigger->get_connection_cl_name().str);
    return true;
  }
  dd_trig_obj->set_connection_collation_id(collation->number);

  if (resolve_collation(new_trigger->get_db_cl_name().str, system_charset_info,
                        &collation))
  {
    // resolve_charset will not cause an error to be reported if the
    // collation was not found, so we must report error here.
    my_error(ER_UNKNOWN_COLLATION, MYF(0), new_trigger->get_db_cl_name().str);
    return true;
  }
  dd_trig_obj->set_schema_collation_id(collation->number);

  return false;
}


bool create_trigger(THD *thd, const ::Trigger *new_trigger,
                    enum_trigger_order_type ordering_clause,
                    const LEX_CSTRING &referenced_trigger_name)
{
  DBUG_ENTER("dd::create_trigger");

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  Table *new_table= nullptr;

  DBUG_EXECUTE_IF("create_trigger_fail", {
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      DBUG_RETURN(true);
    });

  if (dd_client->acquire_for_modification(new_trigger->get_db_name().str,
                                          new_trigger->get_subject_table_name().str,
                                          &new_table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (new_table == nullptr)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0),
             new_trigger->get_db_name().str,
             new_trigger->get_subject_table_name().str);
    DBUG_RETURN(true);
  }

  Trigger *dd_trig_obj;

  if (ordering_clause != TRG_ORDER_NONE)
  {
    const Trigger *referenced_trg=
        new_table->get_trigger(referenced_trigger_name.str);

    /*
      Checking for presence of a trigger referenced by FOLLOWS/PRECEDES clauses
      is done in Trigger_chain::add_trigger() that is called from
      Table_trigger_dispatcher::create_trigger() before storing trigger
      in the data dictionary. It means that call of dd::Trigger::get_trigger()
      for referenced trigger name must return NOT NULL pointer. Therefore
      it just added assert to check this invariant.
    */
    DBUG_ASSERT(referenced_trg != nullptr);
    if (ordering_clause == TRG_ORDER_FOLLOWS)
      dd_trig_obj=
        new_table->add_trigger_following(referenced_trg,
                                         get_dd_action_timing(new_trigger),
                                         get_dd_event_type(new_trigger));
    else
      dd_trig_obj=
        new_table->add_trigger_preceding(referenced_trg,
                                         get_dd_action_timing(new_trigger),
                                         get_dd_event_type(new_trigger));
  }
  else
    dd_trig_obj= new_table->add_trigger(get_dd_action_timing(new_trigger),
                                        get_dd_event_type(new_trigger));

  if (dd_trig_obj == nullptr)
    // NOTE: It's expected that an error is reported
    // by the dd::cache::Dictionary_client::add_trigger.
    DBUG_RETURN(true);

  if (fill_in_dd_trigger_object(thd, new_trigger, dd_trig_obj))
    DBUG_RETURN(true);

  /*
    Store the dd::Table object. All the trigger objects are stored
    in mysql.triggers. Errors will be reported by the dictionary subsystem.
  */
  if (dd_client->update(new_table))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/**
  Convert event type value from DD presentation to generic SQL presentation.

  @param [in] event_type  Event type value from the Data Dictionary

  @return Event type value as it's presented in generic SQL-layer
*/

static enum_trigger_event_type convert_event_type_from_dd(
    dd::Trigger::enum_event_type event_type)
{
  switch (event_type)
  {
  case dd::Trigger::enum_event_type::ET_INSERT:
    return TRG_EVENT_INSERT;
  case dd::Trigger::enum_event_type::ET_UPDATE:
    return TRG_EVENT_UPDATE;
  case dd::Trigger::enum_event_type::ET_DELETE:
    return TRG_EVENT_DELETE;
  };
  DBUG_ASSERT (false);
  return TRG_EVENT_MAX;
}


/**
  Convert action timing value from DD presentation to generic SQL presentation.

  @param [in] action_timing  Action timing value from the Data Dictionary

  @return Action timing value as it's presented in generic SQL-layer
*/

static enum_trigger_action_time_type convert_action_time_from_dd(
  dd::Trigger::enum_action_timing action_timing)
{
  switch (action_timing)
  {
  case dd::Trigger::enum_action_timing::AT_BEFORE:
    return TRG_ACTION_BEFORE;
  case dd::Trigger::enum_action_timing::AT_AFTER:
    return TRG_ACTION_AFTER;
  }
  DBUG_ASSERT (false);
  return TRG_ACTION_MAX;
}


bool load_triggers(THD *thd,
                   MEM_ROOT *mem_root,
                   const char *schema_name,
                   const char *table_name,
                   List<::Trigger> *triggers)
{
  DBUG_ENTER("dd::load_triggers");

  Schema_MDL_locker schema_mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const Table *table= nullptr;
  if (schema_mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (table == nullptr)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0),
             schema_name, table_name);
    DBUG_RETURN(true);
  }

  for (const auto &trigger : table->triggers())
  {
    LEX_CSTRING db_name_str= { schema_name, strlen(schema_name) };
    LEX_CSTRING subject_table_name= { table_name, strlen(table_name) };
    LEX_CSTRING definition, definition_utf8;

    if (make_lex_string_root(mem_root,
                             &definition,
                             trigger->action_statement().c_str(),
                             trigger->action_statement().length(),
                             false) == nullptr)
      DBUG_RETURN(true);

    if (make_lex_string_root(mem_root,
                             &definition_utf8,
                             trigger->action_statement_utf8().c_str(),
                             trigger->action_statement_utf8().length(),
                             false) == nullptr)
      DBUG_RETURN(true);

    LEX_CSTRING definer_user;
    if (make_lex_string_root(mem_root,
                             &definer_user,
                             trigger->definer_user().c_str(),
                             trigger->definer_user().length(),
                             false) == nullptr)
      DBUG_RETURN(true);

    LEX_CSTRING definer_host;
    if (make_lex_string_root(mem_root,
                             &definer_host,
                             trigger->definer_host().c_str(),
                             trigger->definer_host().length(),
                             false) == nullptr)
      DBUG_RETURN(true);

    const CHARSET_INFO *client_cs=
      dd_get_mysql_charset(trigger->client_collation_id());
    if (client_cs == nullptr)
      client_cs= thd->variables.character_set_client;

    const CHARSET_INFO *connection_cs=
      dd_get_mysql_charset(trigger->connection_collation_id());
    if (connection_cs == nullptr)
      connection_cs= thd->variables.collation_connection;

    const CHARSET_INFO *schema_cs=
      dd_get_mysql_charset(trigger->schema_collation_id());
    if (schema_cs == nullptr)
      schema_cs= thd->variables.collation_database;

    LEX_CSTRING client_cs_name, connection_cl_name, db_cl_name, trigger_name;
    if (make_lex_string_root(mem_root,
                             &client_cs_name,
                             client_cs->csname,
                             strlen(client_cs->csname), false) == nullptr ||
        make_lex_string_root(mem_root,
                             &connection_cl_name,
                             connection_cs->name,
                             strlen(connection_cs->name), false) == nullptr ||
        make_lex_string_root(mem_root,
                             &db_cl_name,
                             schema_cs->name,
                             strlen(schema_cs->name), false) == nullptr ||
        make_lex_string_root(mem_root,
                             &trigger_name,
                             trigger->name().c_str(),
                             trigger->name().length(), false) == nullptr)
      DBUG_RETURN(true);

    ::Trigger *trigger_to_add=
      ::Trigger::create_from_dd(
        mem_root,
        trigger_name,
        db_name_str,
        subject_table_name,
        definition,
        definition_utf8,
        trigger->sql_mode(),
        definer_user,
        definer_host,
        client_cs_name,
        connection_cl_name,
        db_cl_name,
        convert_event_type_from_dd(trigger->event_type()),
        convert_action_time_from_dd(trigger->action_timing()),
        trigger->action_order(),
        trigger->created());

    if (trigger_to_add == nullptr)
      DBUG_RETURN(true);

    if (triggers->push_back(trigger_to_add, mem_root))
    {
      delete trigger_to_add;
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}


bool load_trigger_names(THD *thd,
                        MEM_ROOT *mem_root,
                        const char *schema_name,
                        const char *table_name,
                        List<LEX_CSTRING> *trigger_names)
{
  DBUG_ENTER("dd::load_trigger_names");

  Schema_MDL_locker schema_mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const Table *table= nullptr;
  if (schema_mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (table == nullptr)
  {
    DBUG_RETURN(false);
  }

  for (const auto &trigger : table->triggers())
  {
    LEX_CSTRING *trigger_name;

    trigger_name= make_lex_string_root(mem_root,
                                       static_cast<LEX_CSTRING *>(NULL),
                                       trigger->name().c_str(),
                                       trigger->name().length(), true);
    if (trigger_name == nullptr)
      DBUG_RETURN(true);

    if (trigger_names->push_back(trigger_name, mem_root))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


bool table_has_triggers(THD *thd, const char *schema_name,
                        const char *table_name, bool *table_has_trigger)
{
  DBUG_ENTER("table_has_triggers");

  Schema_MDL_locker schema_mdl_locker(thd);

  if (get_dictionary()->is_dd_table_name(schema_name, table_name))
  {
    *table_has_trigger= false;
    DBUG_RETURN(false);
  }

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const Table *table= nullptr;

  if (schema_mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  *table_has_trigger= (table != nullptr && table->has_trigger());

  DBUG_RETURN(false);
}


bool check_trigger_exists(THD *thd,
                          const char *schema_name,
                          const char *trigger_name,
                          bool *trigger_exists)
{
  Schema_MDL_locker mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const Schema *sch_obj= nullptr;
  if (mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire(schema_name, &sch_obj))
    return true;

  if (sch_obj == nullptr)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name);
    return true;
  }

  String_type table_name;
  if (dd_client->get_table_name_by_trigger_name(sch_obj->id(),
                                                trigger_name,
                                                &table_name))
    return true;

  *trigger_exists= (table_name == "") ? false : true;

  return false;
}


bool drop_trigger(THD *thd,
                  const char *schema_name,
                  const char *table_name,
                  const char *trigger_name,
                  bool *trigger_found)
{
  DBUG_ENTER("dd::drop_trigger");

  Schema_MDL_locker schema_mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  Table *new_table= nullptr;

  if (schema_mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire_for_modification(schema_name, table_name, &new_table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (new_table == nullptr)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0),
             schema_name, table_name);
    DBUG_RETURN(true);
  }

  const Trigger *dd_trig_obj= new_table->get_trigger(trigger_name);

  if (dd_trig_obj == nullptr)
  {
   *trigger_found= false;
   DBUG_RETURN(false);
  }
  new_table->drop_trigger(dd_trig_obj);

  /*
    Store the Table object. All the trigger objects are stored
    in mysql.triggers.
  */
  if (dd_client->update(new_table))
  {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  *trigger_found= true;
  DBUG_RETURN(false);
}


bool drop_all_triggers(THD *thd,
                       const char *schema_name,
                       const char *table_name,
                       List<::Trigger> *triggers)
{
  DBUG_ENTER("dd::drop_trigger");

  Schema_MDL_locker schema_mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  Table *new_table= nullptr;

  if (schema_mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire_for_modification(schema_name, table_name, &new_table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (new_table == nullptr)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0),
             schema_name, table_name);
    DBUG_RETURN(true);
  }

  List_iterator_fast<::Trigger> it(*triggers);
  ::Trigger *t;

  while ((t= it++))
  {
    LEX_CSTRING trigger_name= t->get_trigger_name();

    const Trigger *dd_trig_obj= new_table->get_trigger(trigger_name.str);

    if (dd_trig_obj == nullptr)
      continue;

    new_table->drop_trigger(dd_trig_obj);
#ifdef HAVE_PSI_SP_INTERFACE
    LEX_CSTRING db_name= t->get_db_name();
    /* Drop statistics for this stored program from performance schema. */
    MYSQL_DROP_SP(to_uint(enum_sp_type::TRIGGER),
                  db_name.str, db_name.length,
                  trigger_name.str, trigger_name.length);
#endif
  }

  /*
    Store the dd::Table object. All the trigger objects are removed from
    mysql.triggers.
  */
  if (dd_client->update(new_table))
  {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
}


bool get_table_name_for_trigger(THD *thd,
                                const char *schema_name,
                                const char *trigger_name,
                                String_type *table_name,
                                bool *trigger_found,
                                bool push_warning_if_not_exist)
{
  Schema_MDL_locker mdl_locker(thd);

  cache::Dictionary_client *dd_client= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const Schema *sch_obj= nullptr;
  if (mdl_locker.ensure_locked(schema_name) ||
      dd_client->acquire(schema_name, &sch_obj))
    return true;

  if (sch_obj == nullptr)
  {
    if (!push_warning_if_not_exist)
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), schema_name);
      return true;
    }
    *trigger_found= false;
    return false;
  }

  if (dd_client->get_table_name_by_trigger_name(sch_obj->id(),
                                                trigger_name,
                                                table_name))
    return true;

  *trigger_found= (*table_name == "") ? false : true;

  return false;
}

}  // namespace dd
