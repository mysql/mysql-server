/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd_event.h"

#include "event_parse_data.h"   // Event_parse_data
#include "log.h"                // sql_print_error
#include "sp_head.h"            // sp_head
#include "sql_db.h"             // get_default_db_collation
#include "sql_class.h"          // THD
#include "transaction.h"        // trans_commit
#include "tztime.h"             // Time_zone

#include "dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client


namespace dd {

int get_old_status(Event::enum_event_status event_status)
{
  switch(event_status)
  {
  case Event::ES_ENABLED:
    return Event_parse_data::ENABLED;
  case Event::ES_DISABLED:
    return Event_parse_data::DISABLED;
  case Event::ES_SLAVESIDE_DISABLED:
    return Event_parse_data::SLAVESIDE_DISABLED;
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return Event_parse_data::DISABLED;
  /* purecov: end deadcode */
}


/**
  Convert legacy event status to dd::Event::enum_event_status.

  @param  event_status  Legacy event_status

  @returns dd::Event::enum_event_status value corressponding to
           legacy event_status.
*/

static Event::enum_event_status get_enum_event_status(int event_status)
{
  switch(event_status)
  {
  case Event_parse_data::ENABLED:
    return Event::ES_ENABLED;
  case Event_parse_data::DISABLED:
    return Event::ES_DISABLED;
  case Event_parse_data::SLAVESIDE_DISABLED:
    return Event::ES_SLAVESIDE_DISABLED;
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return Event::ES_DISABLED;
  /* purecov: end deadcode */
}


int get_old_on_completion(Event::enum_on_completion on_completion)
{
  switch(on_completion)
  {
  case Event::OC_DROP:
    return Event_parse_data::ON_COMPLETION_DROP;
  case Event::OC_PRESERVE:
    return Event_parse_data::ON_COMPLETION_PRESERVE;
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return Event_parse_data::ON_COMPLETION_DROP;
  /* purecov: end deadcode */
}


/**
  Convert legacy event on completion behavior to dd::Event::enum_on_compeltion.

  @param  on_completion  Legacy on completion behaviour value

  @returns dd::Event::enum_on_compeltion corressponding to legacy
           event on completion value.
*/

static Event::enum_on_completion get_on_completion(int on_completion)
{
  switch(on_completion)
  {
  case Event_parse_data::ON_COMPLETION_DEFAULT:
  case Event_parse_data::ON_COMPLETION_DROP:
    return Event::OC_DROP;
  case Event_parse_data::ON_COMPLETION_PRESERVE:
    return Event::OC_PRESERVE;
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return Event::OC_DROP;
  /* purecov: end deadcode */
}


interval_type get_old_interval_type(Event::enum_interval_field interval_field)
{
  switch(interval_field)
  {
  case Event::IF_YEAR:
    return INTERVAL_YEAR;
  case Event::IF_QUARTER:
    return INTERVAL_QUARTER;
  case Event::IF_MONTH:
    return INTERVAL_MONTH;
  case Event::IF_WEEK:
    return INTERVAL_WEEK;
  case Event::IF_DAY:
    return INTERVAL_DAY;
  case Event::IF_HOUR:
    return INTERVAL_HOUR;
  case Event::IF_MINUTE:
    return INTERVAL_MINUTE;
  case Event::IF_SECOND:
    return INTERVAL_SECOND;
  case Event::IF_MICROSECOND:
    return INTERVAL_MICROSECOND;
  case Event::IF_YEAR_MONTH:
    return INTERVAL_YEAR_MONTH;
  case Event::IF_DAY_HOUR:
    return INTERVAL_DAY_HOUR;
  case Event::IF_DAY_MINUTE:
    return INTERVAL_DAY_MINUTE;
  case Event::IF_DAY_SECOND:
    return INTERVAL_DAY_SECOND;
  case Event::IF_HOUR_MINUTE:
    return INTERVAL_HOUR_MINUTE;
  case Event::IF_HOUR_SECOND:
    return INTERVAL_HOUR_SECOND;
  case Event::IF_MINUTE_SECOND:
    return INTERVAL_MINUTE_SECOND;
  case Event::IF_DAY_MICROSECOND:
    return INTERVAL_DAY_MICROSECOND;
  case Event::IF_HOUR_MICROSECOND:
    return INTERVAL_HOUR_MICROSECOND;
  case Event::IF_MINUTE_MICROSECOND:
    return INTERVAL_MINUTE_MICROSECOND;
  case Event::IF_SECOND_MICROSECOND:
    return INTERVAL_SECOND_MICROSECOND;
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return INTERVAL_YEAR;
  /* purecov: end deadcode */
}


/**
  Convert legacy interval type value to dd::Event::enum_interval_field.

  @param  interval_type_val  Interval type value.

  @returns dd::Event::enum_interval_field corressponding to legacy
           interval type value.
*/

static Event::enum_interval_field get_enum_interval_field(
  interval_type interval_type_val)
{
  switch(interval_type_val)
  {
  case INTERVAL_YEAR:
    return Event::IF_YEAR;
  case INTERVAL_QUARTER:
    return Event::IF_QUARTER;
  case INTERVAL_MONTH:
    return Event::IF_MONTH;
  case INTERVAL_WEEK:
    return Event::IF_WEEK;
  case INTERVAL_DAY:
    return Event::IF_DAY;
  case INTERVAL_HOUR:
    return Event::IF_HOUR;
  case INTERVAL_MINUTE:
    return Event::IF_MINUTE;
  case INTERVAL_SECOND:
    return Event::IF_SECOND;
  case INTERVAL_MICROSECOND:
    return Event::IF_MICROSECOND;
  case INTERVAL_YEAR_MONTH:
    return Event::IF_YEAR_MONTH;
  case INTERVAL_DAY_HOUR:
    return Event::IF_DAY_HOUR;
  case INTERVAL_DAY_MINUTE:
    return Event::IF_DAY_MINUTE;
  case INTERVAL_DAY_SECOND:
    return Event::IF_DAY_SECOND;
  case INTERVAL_HOUR_MINUTE:
    return Event::IF_HOUR_MINUTE;
  case INTERVAL_HOUR_SECOND:
    return Event::IF_HOUR_SECOND;
  case INTERVAL_MINUTE_SECOND:
    return Event::IF_MINUTE_SECOND;
  case INTERVAL_DAY_MICROSECOND:
    return Event::IF_DAY_MICROSECOND;
  case INTERVAL_HOUR_MICROSECOND:
    return Event::IF_HOUR_MICROSECOND;
  case INTERVAL_MINUTE_MICROSECOND:
    return Event::IF_MINUTE_MICROSECOND;
  case INTERVAL_SECOND_MICROSECOND:
    return Event::IF_SECOND_MICROSECOND;
  case INTERVAL_LAST:
    DBUG_ASSERT(false);
  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid Event status option");
  DBUG_ASSERT(false);

  return Event::IF_YEAR;
}


bool
event_exists(dd::cache::Dictionary_client *dd_client,
             const std::string &schema_name,
             const std::string &event_name,
             bool *exists)
{
  DBUG_ENTER("dd::event_exists");
  DBUG_ASSERT(exists);

  const dd::Event *event_ptr= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);
  if (dd_client->acquire<dd::Event>(schema_name, event_name, &event_ptr))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  *exists= event_ptr != nullptr;

  DBUG_RETURN(false);
}


/**
  Set Event attributes.

  @param    thd           THD context.
  @param    event         Pointer to Event Object.
  @param    event_name    Event name.
  @param    event_data    Parsed Event Data.
  @param    sp            Pointer to Stored Program Head.
  @param    is_update     true if existing Event objects attributes set
                          else false.
*/

static void set_event_attributes(THD *thd, Event *event,
                                 const std::string &event_name,
                                 Event_parse_data *event_data,
                                 sp_head *sp, bool is_update)
{
  // Set Event name and definer.
  event->set_name(event_name);
  event->set_definer(thd->lex->definer->user.str,
                         thd->lex->definer->host.str);

  // Set Event on completion and status.
  event->set_on_completion(get_on_completion(event_data->on_completion));
  if (!is_update || event_data->status_changed)
    event->set_event_status(get_enum_event_status(event_data->status));
  event->set_originator(event_data->originator);

  // Set Event definition attributes.
  if (event_data->body_changed)
  {
    event->set_sql_mode(thd->variables.sql_mode);
    event->set_definition_utf8(sp->m_body_utf8.str);
    event->set_definition(sp->m_body.str);
  }

  // Set Event scheduling attributes.
  if (event_data->expression)
  {
    const String *tz_name= thd->variables.time_zone->get_name();
    if (!is_update || !event_data->starts_null)
      event->set_time_zone(tz_name->ptr());

    event->set_interval_value_null(false);
    event->set_interval_value(event_data->expression);
    event->set_interval_field_null(false);
    event->set_interval_field(get_enum_interval_field(event_data->interval));

    event->set_execute_at_null(true);
    if (!event_data->starts_null)
    {
      event->set_starts_null(false);
      event->set_starts(event_data->starts);
    }
    else
      event->set_starts_null(true);

    if (!event_data->ends_null)
    {
      event->set_ends_null(false);
      event->set_ends(event_data->ends);
    }
    else
      event->set_ends_null(true);
  }
  else if (event_data->execute_at)
  {
    const String *tz_name= thd->variables.time_zone->get_name();
    event->set_time_zone(tz_name->ptr());
    event->set_interval_value_null(true);
    event->set_interval_field_null(true);
    event->set_starts_null(true);
    event->set_ends_null(true);
    event->set_execute_at_null(false);
    event->set_execute_at(event_data->execute_at);
  }
  else
    DBUG_ASSERT(is_update);

  if (event_data->comment.str != nullptr)
    event->set_comment(std::string(event_data->comment.str));

  // Set collation relate attributes.
  event->set_client_collation_id(
    thd->variables.character_set_client->number);
  event->set_connection_collation_id(
    thd->variables.collation_connection->number);

  const CHARSET_INFO *db_cl= nullptr;
  if (get_default_db_collation(thd, event_data->dbname.str, &db_cl))
  {
    DBUG_PRINT("error", ("get_default_db_collation failed."));
    // Obtain collation from thd and proceed.
    thd->clear_error();
  }

  db_cl= db_cl ? db_cl : thd->collation();
  event->set_schema_collation_id(db_cl->number);
}


bool create_event(THD *thd,
                  const std::string &schema_name,
                  const std::string &event_name,
                  Event_parse_data *event_data, sp_head *sp)
{
  DBUG_ENTER("dd::create_event");

  dd::cache::Dictionary_client *client= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  const dd::Schema *sch_obj= nullptr;

  // Acquire schema object.
  if (client->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }
  if (sch_obj == nullptr)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name.c_str());
    DBUG_RETURN(true);
  }

  std::unique_ptr<dd::Event> event(sch_obj->create_event(thd));

  // Set Event attributes.
  set_event_attributes(thd, event.get(), event_name, event_data, sp, false);

  if (client->store(event.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
}


bool update_event(THD *thd, const Event *event,
                  Event_parse_data *event_data, sp_head *sp,
                  const std::string &new_db_name,
                  const std::string &new_event_name)
{
  DBUG_ENTER("dd::update_event");
  DBUG_ASSERT(event != nullptr);

  // Clone the Event Object.
  std::unique_ptr<dd::Event> new_event(event->clone());

  // Check whether alter event was given dates that are in the past.
  if (event_data->check_dates(thd, static_cast<int>(new_event->on_completion())))
    DBUG_RETURN(true);

  // Update Schema Id if there is a dbname change.
  if (new_db_name != "")
  {
    const dd::Schema *to_sch_ptr;
    if (thd->dd_client()->acquire<dd::Schema>(new_db_name, &to_sch_ptr))
      DBUG_RETURN(true);

    if (to_sch_ptr == nullptr)
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), new_db_name.c_str());
      DBUG_RETURN(true);
    }

    new_event->set_schema_id(to_sch_ptr->id());
  }

  // Set the altered event attributes.
  set_event_attributes(thd, new_event.get(),
                       new_event_name != "" ? new_event_name : event->name(),
                       event_data, sp, true);

  if (thd->dd_client()->update(&event, new_event.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
}

bool update_event_time_and_status(THD *thd, const Event *event,
                                  my_time_t last_executed, ulonglong status)
{
  DBUG_ENTER("dd::update_event_time_fields");

  DBUG_ASSERT(event != nullptr);

  std::unique_ptr<dd::Event> new_event(event->clone());

  new_event->set_event_status_null(false);
  new_event->set_event_status(get_enum_event_status(status));
  new_event->set_last_executed_null(false);
  new_event->set_last_executed(last_executed);

  if (thd->dd_client()->update(&event, new_event.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
}


bool drop_event(THD *thd, const Event *event)
{
  DBUG_ENTER("dd::drop_event");

  DBUG_ASSERT(event != nullptr);

  Disable_gtid_state_update_guard disabler(thd);

  if (thd->dd_client()->drop(event))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) || trans_commit(thd));
}

} // namespace dd
