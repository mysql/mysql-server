/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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


#include "mysql/service_command.h"

#ifdef __cplusplus
extern "C" {
#endif

  int command_service_run_command(MYSQL_SESSION session,
                                enum enum_server_command command,
                                const union COM_DATA * data,
                                const CHARSET_INFO * client_cs,
                                const struct st_command_service_cbs * callbacks,
                                enum cs_text_or_binary text_or_binary,
                                void * service_callbacks_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

#ifdef __cplusplus
}
#endif
