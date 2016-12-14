/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GROUP_REPLICATION_AUTO_INCREMENT
#define GROUP_REPLICATION_AUTO_INCREMENT

#include "plugin_server_include.h"

#define SERVER_DEFAULT_AUTO_INCREMENT 1
#define SERVER_DEFAULT_AUTO_OFFSET 1

/*
  @class Plugin_group_replication_auto_increment

  This class will be used to configure auto_increment variables
  (auto_increment_increment and auto_increment_offset)
 */
class Plugin_group_replication_auto_increment
{
public:
  /**
    Plugin_group_replication_auto_increment constructor

    Set auto_increment_increment and auto_increment_offset in the server
  */
  Plugin_group_replication_auto_increment();

  /**
    Set auto_increment_increment and auto_increment_offset

    @param increment the interval between successive column values
    @param offset    the starting point for the AUTO_INCREMENT column value
  */

  void set_auto_increment_variables(ulong increment, ulong offset);

  /**
    Reset auto_increment_increment and auto_increment_offset,
    if modified by this plugin in set function
  */

  void reset_auto_increment_variables();

private:
  ulong group_replication_auto_increment;
  ulong group_replication_auto_offset;
};

#endif /* GROUP_REPLICATION_AUTO_INCREMENT */
