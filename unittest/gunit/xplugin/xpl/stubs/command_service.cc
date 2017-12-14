/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include "my_dbug.h"
#include "mysql/service_command.h"

int command_service_run_command(MYSQL_SESSION,
                              enum enum_server_command,
                              const union COM_DATA*,
                              const CHARSET_INFO*,
                              const struct st_command_service_cbs*,
                              enum cs_text_or_binary,
                              void*)
{
  DBUG_ASSERT(0);
  return 0;
}
