/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_UPGRADE__EVENT_H_INCLUDED
#define DD_UPGRADE__EVENT_H_INCLUDED

#include "sql/dd/string_type.h"

class THD;

namespace dd {
namespace upgrade_57 {

/**
  Migrate all events from mysql.event to mysql.events table.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_events_to_dd(THD *thd);

/**
  Helper function to create a stored procedure from an event body.

  @param[in]  thd             Thread handle.
  @param[in]  name            Name of the event.
  @param[in]  name_len        Length of the name of the event.
  @param[in]  body            Body of the event.
  @param[in]  body_len        Length of the body of the event.
  @param[out] sp_sql          Stored procedure SQL string.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool build_event_sp(THD *thd, const char *name, size_t name_len,
                    const char *body, size_t body_len, dd::String_type *sp_sql);

}  // namespace upgrade_57
}  // namespace dd

#endif  // DD_UPGRADE__EVENT_H_INCLUDED
