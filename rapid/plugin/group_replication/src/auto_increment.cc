/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/auto_increment.h"

#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_log.h"


Plugin_group_replication_auto_increment::
Plugin_group_replication_auto_increment()
  : group_replication_auto_increment(0),
    group_replication_auto_offset(0)
{
}

void
Plugin_group_replication_auto_increment::
reset_auto_increment_variables()
{
  /* get server auto_increment variables value */
  ulong current_server_increment= get_auto_increment_increment();
  ulong current_server_offset= get_auto_increment_offset();

  /*
    Verify whether auto_increment variables were modified by user
    or by group_replication, by checking their last saved values in
    group_replication_auto_increment_increment and
    group_replication_auto_increment_offset
  */
  if (local_member_info != NULL && !local_member_info->in_primary_mode() &&
      group_replication_auto_increment == current_server_increment &&
      group_replication_auto_offset == current_server_offset)
  {
    /* set to default values i.e. 1 */
    set_auto_increment_increment(SERVER_DEFAULT_AUTO_INCREMENT);
    set_auto_increment_offset(SERVER_DEFAULT_AUTO_OFFSET);

    log_message(MY_INFORMATION_LEVEL,
                "auto_increment_increment is reset to %lu",
                SERVER_DEFAULT_AUTO_INCREMENT);

    log_message(MY_INFORMATION_LEVEL,
                "auto_increment_offset is reset to %lu",
                SERVER_DEFAULT_AUTO_OFFSET);
  }
}

void
Plugin_group_replication_auto_increment::
set_auto_increment_variables(ulong increment, ulong offset)
{
  /* get server auto_increment variables value */
  ulong current_server_increment= get_auto_increment_increment();
  ulong current_server_offset= get_auto_increment_offset();

  if (local_member_info != NULL && !local_member_info->in_primary_mode() &&
      current_server_increment == 1 &&
      current_server_offset == 1)
  {
    /* set server auto_increment variables */
    set_auto_increment_increment(increment);
    set_auto_increment_offset(offset);

    /*
      store auto_increment variables in local variables to verify later
      in destructor if auto_increment variables were modified by user.
    */
    group_replication_auto_increment= increment;
    group_replication_auto_offset= offset;

    log_message(MY_INFORMATION_LEVEL,
                "auto_increment_increment is set to %lu",
                increment);

    log_message(MY_INFORMATION_LEVEL,
                "auto_increment_offset is set to %lu",
                offset);
  }
}
