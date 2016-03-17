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

#ifndef DD_EVENT_INCLUDED
#define DD_EVENT_INCLUDED

#include "my_global.h"

#include "my_time.h"             // interval_type
#include "dd/types/event.h"      // dd::Event::enum_event_status

#include <string>

class Event_parse_data;
class sp_head;
class THD;

using sql_mode_t= ulonglong;

namespace dd
{
namespace cache
{
  class Dictionary_client;
}

/**
   Convert new DD Event::enum_event_status to status type used in
   server code.

   @param event_status status of type enum_event_status.

   @returns an int indicating status of event used in server code.
*/
int get_old_status(Event::enum_event_status event_status);

/**
   Convert new DD Event::enum_on_completion to completion type used in
   server code.

   @param on_completion on_completion of type enum_on_completion.

   @returns an int indicating on_completion of event used in server code.
*/
int get_old_on_completion(Event::enum_on_completion on_completion);

/**
   Convert new DD interval_field type to type interval_type used in
   server code.

   @param interval_field interval_field of type enum_interval_field.

   @returns an value of type interval type indicating interval type values
            used in server code.
*/
interval_type get_old_interval_type(Event::enum_interval_field interval_field);

/**
   Check if an event exists under a schema.

   @param       dd_client   Dictionary client
   @param       schema_name Schema name of the event object name.
   @param       name        The event name to search for.
   @param [out] exists      Value set to true if the object is found else false.

   @retval      true         Failure (error has been reported).
   @retval      false        Success.
*/
bool event_exists(dd::cache::Dictionary_client *dd_client,
                  const std::string &schema_name,
                  const std::string &name,
                  bool *exists);

/**
   Create an event object and commit it to DD Table Events.

   @param thd            Thread handle
   @param schema_name    Database name
   @param event_name     Event name
   @param event_data     Event information obtained from parser.
   @param sp             SP head

   @retval true  Event creation failed.
   @retval false Event creation succeeded.
*/
bool create_event(THD *thd, const std::string &schema_name,
                  const std::string &event_name, Event_parse_data *event_data,
                  sp_head *sp);

/**
  Create or update a event object and commit it to
  DD Table Events.

  @param thd            Thread handle
  @param event          Event to update.
  @param event_data     Event information obtained from parser.
  @param sp             SP head
  @param new_db_name    Updated db name.
  @param new_event_name Updated Event name.

  @retval true  Event updation failed.
  @retval false Event updation succeeded.
*/
bool update_event(THD *thd, const Event *event,
                  Event_parse_data *event_data,
                  sp_head *sp, const std::string &new_db_name,
                  const std::string &new_event_name);

/**
  Update time related fields of Event object.

  @param thd               Thread handle
  @param event             Event to update.
  @param last_executed     Time the event was last executed.
  @param status            Event status.

  @retval true  true if update failed.
  @retval false false if update succeeded.
*/
bool update_event_time_and_status(THD *thd, const Event *event,
                                  my_time_t last_executed,
                                  ulonglong status);

/**
  Drop an Event from event metadata table.

  @param thd            Thread handle.
  @param event          Event to be droppped.

  @retval true if event drop failed.
  @retval false if event drop succeeded.
*/
bool drop_event(THD *thd, const Event *event);
} // namespace dd
#endif // DD_EVENT_INCLUDED
