/*
   Copyright (c) 2013, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "channel_info.h"

#include "my_stacktrace.h"              // my_safe_snprintf
#include "sql_class.h"                  // THD


THD* Channel_info::create_thd()
{
  DBUG_EXECUTE_IF("simulate_resource_failure", return NULL;);

  Vio* vio_tmp= create_and_init_vio();
  if (vio_tmp == NULL)
    return NULL;

  THD* thd= new (std::nothrow) THD;
  if (thd == NULL)
  {
    vio_delete(vio_tmp);
    return NULL;
  }

  thd->get_protocol_classic()->init_net(vio_tmp);

  return thd;
}


void Channel_info::send_error_and_close_channel(uint errorcode,
                                                int error,
                                                bool senderror)
{
  DBUG_ASSERT(errorcode != 0);
  if (!errorcode)
    return;

  char error_message_buff[MYSQL_ERRMSG_SIZE];

  if (senderror)
  {
    NET net_tmp;
    Vio *vio_tmp= create_and_init_vio();

    if (vio_tmp && !my_net_init(&net_tmp, vio_tmp))
    {
      if (error)
        my_snprintf(error_message_buff, sizeof(error_message_buff),
                    ER_DEFAULT(errorcode), error);
      net_send_error(&net_tmp, errorcode, error ? error_message_buff:
                     ER_DEFAULT(errorcode));
      net_end(&net_tmp);
    }
    if (vio_tmp != NULL)
    {
      vio_tmp->inactive= TRUE; // channel is already closed.
      vio_delete(vio_tmp);
    }
  }
  else // fatal error like out of memory.
  {
    if (error)
      my_safe_snprintf(error_message_buff, sizeof(error_message_buff),
                       ER_DEFAULT(errorcode), error);
    my_safe_printf_stderr("[Warning] %s\n",
                          error ? error_message_buff : ER_DEFAULT(errorcode));
  }
}
