/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_UPGRADE__EVENT_H_INCLUDED
#define DD_UPGRADE__EVENT_H_INCLUDED

class THD;

namespace dd {
namespace upgrade {

/**
  Migrate all events from mysql.event to mysql.events table.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_events_to_dd(THD *thd);

} // namespace upgrade
} // namespace dd

#endif // DD_UPGRADE__EVENT_H_INCLUDED
