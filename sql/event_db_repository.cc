/*
   Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "event_db_repository.h"

#include "dd/dd_event.h"
#include "dd/cache/dictionary_client.h" // fetch_schema_components
#include "dd/iterator.h"
#include "dd/dd_schema.h"

#include "sql_base.h"                   // close_thread_tables
#include "key.h"                        // key_copy
#include "sql_db.h"                     // get_default_db_collation
#include "sql_time.h"                   // interval_type_to_name
#include "tztime.h"                     // struct Time_zone
#include "auth_common.h"                // SUPER_ACL
#include "sp_head.h"
#include "event_data_objects.h"
#include "event_parse_data.h"
#include "events.h"
#include "sql_show.h"
#include "log.h"
#include "lock.h"                       // lock_object_name
#include "derror.h"
#include "transaction.h"

/**
  @addtogroup Event_Scheduler
  @{
*/


/**
  Fetch the events which are defined under the schema
  using the Data Dictionary API fetch_schema_components
  and fill the Information Schema Events table.

  The DD API fetch_schema_components does a index scan of the underlying
  meta table mysql.events.

  @param       thd          THD context.
  @param       schema_table The I_S.EVENTS table
  @param       db           For which schema to do an index scan.

  @returns     false on success and true on error.
*/

static bool index_read_for_db_for_i_s(THD *thd, TABLE *schema_table,
                                      const char *db)
{
  DBUG_ENTER("index_read_for_db_for_i_s");

  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= nullptr;

  // Acquire schema object by name.
  if (mdl_locker.ensure_locked(db))
    DBUG_RETURN(true);

  if (thd->dd_client()->acquire<dd::Schema>(db, &sch_obj))
  {
    /*
      Ignore any error so that information schema display a empty row.
      Clear any diagnostics area set.
    */
    thd->clear_error();
    DBUG_RETURN(false);
  }

  if (sch_obj == nullptr)
    DBUG_RETURN(false);

  std::vector<const dd::Event*> events;
  if (thd->dd_client()->fetch_schema_components(sch_obj, &events))
  {
    /*
      Ignore any error so that information schema displays a empty row.
      Clear any diagnostics area set.
    */
    thd->clear_error();
    DBUG_RETURN(false);
  }

  for (const dd::Event *event_obj : events)
  {
    /*
      Fill meta information from the DD Event object
      to Information Schema Events Table.
    */
    bool res= copy_event_to_schema_table(thd, schema_table, *event_obj, db);
    if (res)
    {
      /*
        If no meta information can't be loaded to schema table
        proceed to fetch information for other events.
        Clear any diagnostics area set.
      */
      thd->clear_error();
    }
  }

  delete_container_pointers(events);

  DBUG_RETURN(false);
}


/**
  Fetch all events and fill the Information Schema Events table.
  The DD API fetch_all_schema_events does a table scan of the
  underlying meta table mysql.events.

  @param thd          THD context.
  @param schema_table The I_S.EVENTS in memory table

  @returns  false on success and true on error.
*/

static bool table_scan_all_for_i_s(THD *thd, TABLE *schema_table)
{
  DBUG_ENTER("table_scan_all_for_i_s");

  // Fetch all Schemas
  std::vector<const dd::Schema*> schemas;
  if (thd->dd_client()->fetch_global_components(&schemas))
    DBUG_RETURN(true);

  for (const dd::Schema *schema_obj : schemas)
  {
    // Fetch all DD Event Objects.
    std::vector<const dd::Event*> events;
    if (thd->dd_client()->fetch_schema_components(schema_obj, &events))
      DBUG_RETURN(true);

    for (const dd::Event *event_obj : events)
    {
      /*
        Fill meta information from the DD Event object
        to Information Schema Events Table.
      */
      bool res= copy_event_to_schema_table(thd, schema_table, *event_obj,
                                           schema_obj->name().c_str());
      if (res)
      {
        /*
          If no meta information can't be loaded to schema table
          proceed to fetch information for other events.
          Clear any diagnostics area set.
        */
        thd->clear_error();
      }
    }

    delete_container_pointers(events);
  }

  delete_container_pointers(schemas);

  DBUG_RETURN(false);
}


/**
   Fills I_S.EVENTS with Events information obtained from Data Dictionary.
   Also used by SHOW EVENTS.

   @retval false  success
   @retval true   error
*/

bool
Event_db_repository::fill_schema_events(THD *thd, TABLE_LIST *i_s_table,
                                        const char *db)
{
  bool res= false;

  DBUG_ENTER("Event_db_repository::fill_schema_events");
  DBUG_PRINT("info",("db=%s", db != nullptr? db:"(null)"));

  if (db)
    res= index_read_for_db_for_i_s(thd, i_s_table->table, db);
  else
    res= table_scan_all_for_i_s(thd, i_s_table->table);

  DBUG_PRINT("info", ("Return code=%d", res));
  DBUG_RETURN(res);
}


/**
  Creates an event object and persist to Data Dictionary.

  @pre All semantic checks must be performed outside.

  @param[in,out] thd                   THD
  @param[in]     parse_data            Parsed event definition
  @param[in]     create_if_not         true if IF NOT EXISTS clause was provided
                                       to CREATE EVENT statement
  @param[out]    event_already_exists  When method is completed successfully
                                       set to true if event already exists else
                                       set to false
  @retval false  Success
  @retval true   Error
*/

bool
Event_db_repository::create_event(THD *thd, Event_parse_data *parse_data,
                                  bool create_if_not,
                                  bool *event_already_exists)
{
  DBUG_ENTER("Event_db_repository::create_event");
  sp_head *sp= thd->lex->sphead;
  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);

  DBUG_ASSERT(sp);

  if (dd::event_exists(thd->dd_client(), parse_data->dbname.str,
                       parse_data->name.str, event_already_exists))
    DBUG_RETURN(true);

  if (*event_already_exists)
  {
    if (create_if_not)
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_EVENT_ALREADY_EXISTS,
                          ER_THD(thd, ER_EVENT_ALREADY_EXISTS),
                          parse_data->name.str);
      DBUG_RETURN(false);
    }
    my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), parse_data->name.str);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(dd::create_event(thd, parse_data->dbname.str,
                               parse_data->name.str, parse_data, sp));
}


/**
  Used to execute ALTER EVENT. Pendant to Events::update_event().

  @param[in]      thd         THD context
  @param[in]      parse_data  parsed event definition
  @param[in]      new_dbname  not NULL if ALTER EVENT RENAME
                              points at a new database name
  @param[in]      new_name    not NULL if ALTER EVENT RENAME
                              points at a new event name

  @pre All semantic checks are performed outside this function.

  @retval false Success
  @retval true Error (reported)
*/

bool
Event_db_repository::update_event(THD *thd, Event_parse_data *parse_data,
                                  LEX_STRING *new_dbname, LEX_STRING *new_name)
{
  DBUG_ENTER("Event_db_repository::update_event");
  sp_head *sp= thd->lex->sphead;
  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);

  /* None or both must be set */
  DBUG_ASSERT((new_dbname && new_name) || new_dbname == new_name);

  DBUG_PRINT("info", ("dbname: %s", parse_data->dbname.str));
  DBUG_PRINT("info", ("name: %s", parse_data->name.str));
  DBUG_PRINT("info", ("user: %s", parse_data->definer.str));

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /* first look whether we overwrite */
  if (new_name)
  {
    DBUG_PRINT("info", ("rename to: %s@%s", new_dbname->str, new_name->str));

    bool exists= false;
    if (dd::event_exists(thd->dd_client(), new_dbname->str,
                         new_name->str, &exists))
      DBUG_RETURN(true);

    if (exists)
    {
      my_error(ER_EVENT_ALREADY_EXISTS, MYF(0), new_name->str);
      DBUG_RETURN(true);
    }
  }

  const dd::Event *event= nullptr;
  if (thd->dd_client()->acquire<dd::Event>(parse_data->dbname.str,
                                           parse_data->name.str, &event))
    DBUG_RETURN(true);

  if (event == nullptr)
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), parse_data->name.str);
    DBUG_RETURN(true);
  }

  // Update Event in the data dictionary with altered event object attributes.
  if (dd::update_event(thd, event, parse_data, sp,
                       new_dbname != nullptr ? new_dbname->str : "",
                       new_name != nullptr ? new_name->str : ""))
  {
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


/**
  Delete event.

  @param[in]     thd            THD context
  @param[in]     db             Database name
  @param[in]     name           Event name
  @param[in]     drop_if_exists DROP IF EXISTS clause was specified.
                                If set, and the event does not exist,
                                the error is downgraded to a warning.

  @retval false success
  @retval true error (reported)
*/

bool
Event_db_repository::drop_event(THD *thd, LEX_STRING db, LEX_STRING name,
                                bool drop_if_exists)
{
  DBUG_ENTER("Event_db_repository::drop_event");
  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);
  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for CREATE EVENT command.
    When we are going out of the function scope, the original binary
    format state will be restored.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  DBUG_PRINT("enter", ("%s@%s", db.str, name.str));

  const dd::Event *event_ptr= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire<dd::Event>(db.str, name.str, &event_ptr))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (event_ptr != nullptr)
    DBUG_RETURN(dd::drop_event(thd, event_ptr));

  // Event not found
  if (!drop_if_exists)
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    DBUG_RETURN(true);
  }

  push_warning_printf(thd, Sql_condition::SL_NOTE,
                      ER_SP_DOES_NOT_EXIST, ER_THD(thd, ER_SP_DOES_NOT_EXIST),
                      "Event", name.str);
  DBUG_RETURN(false);
}


/**
  Drops all events in the selected database.

  @param      thd     THD context
  @param      schema  The database under which events are to be dropped.

  @returns true on error, false on success.
*/

bool
Event_db_repository::drop_schema_events(THD *thd, LEX_STRING schema)
{
  DBUG_ENTER("Event_db_repository::drop_schema_events");
  DBUG_PRINT("enter", ("schema=%s", schema.str));

  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= nullptr;

  if (thd->dd_client()->acquire<dd::Schema>(schema.str, &sch_obj))
    DBUG_RETURN(true);
  if (sch_obj == nullptr)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema.str);
    DBUG_RETURN(true);
  }

  std::vector<const dd::Event*> events;
  if (thd->dd_client()->fetch_schema_components(sch_obj, &events))
    DBUG_RETURN(true);

  for (const dd::Event *event_obj : events)
  {
     /*
       TODO: This extra acquire is required for now as Dictionary_client::drop()
       requires the object to be present in the DD cache. Since fetch_schema_components()
       bypasses the cache, the object is not there.
       Remove this code once either fetch_schema_components() uses the cache or
       Dictionary_client::drop() works with uncached objects.
    */
    const dd::Event *event_obj2;
    if (thd->dd_client()->acquire<dd::Event>(schema.str, event_obj->name(), &event_obj2))
      DBUG_RETURN(true);

    if (dd::drop_event(thd, event_obj2))
    {
      my_error(ER_SP_DROP_FAILED, MYF(0),
               "Drop failed for Event: %s", event_obj->name().c_str());
      DBUG_RETURN(true);
    }
  }

  delete_container_pointers(events);

  DBUG_RETURN(false);
}


/**
  Looks for a named event in the Data Dictionary and load it.

  @pre The given thread does not have open tables.

  @retval false  success
  @retval true   error
*/

bool
Event_db_repository::load_named_event(THD *thd, LEX_STRING dbname,
                                      LEX_STRING name, Event_basic *etn)
{
  const dd::Event *event_obj= nullptr;

  DBUG_ENTER("Event_db_repository::load_named_event");
  DBUG_PRINT("enter",("thd: %p  name: %*s", thd,
                      (int) name.length, name.str));

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (thd->dd_client()->acquire<dd::Event>(dbname.str, name.str, &event_obj))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (event_obj == nullptr)
  {
    my_error(ER_EVENT_DOES_NOT_EXIST, MYF(0), name.str);
    DBUG_RETURN(true);
  }

  if (etn->fill_event_info(thd, *event_obj, dbname.str))
  {
    my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "events");
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/**
   Update the event in Data Dictionary with changed status
   and/or last execution time.
*/

bool
Event_db_repository::
update_timing_fields_for_event(THD *thd, LEX_STRING event_db_name,
                               LEX_STRING event_name, my_time_t last_executed,
                               ulonglong status)
{
  DBUG_ENTER("Event_db_repository::update_timing_fields_for_event");
  // Turn off autocommit.
  Disable_autocommit_guard autocommit_guard(thd);

  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for CREATE EVENT command.
    When we are going out of the function scope, the original binary
    format state will be restored.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  DBUG_ASSERT(thd->security_context()->check_access(SUPER_ACL));

  const dd::Event *event_ptr= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire<dd::Event>(event_db_name.str, event_name.str, &event_ptr))
    DBUG_RETURN(true);
  if (event_ptr == nullptr)
    DBUG_RETURN(true);

  bool res= dd::update_event_time_and_status(thd, event_ptr, last_executed,
                                        status);

  DBUG_RETURN(res);
}

/**
  @} (End of group Event_Scheduler)
*/
// XXX:
