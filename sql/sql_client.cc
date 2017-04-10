/* Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  This files defines some MySQL C API functions that are server specific
*/

#include <stddef.h>
#include <sys/types.h>
#include <algorithm>

#include "mysql_com.h"
#include "mysqld.h"                             // global_system_variables
#include "system_variables.h"

using std::min;
using std::max;

/*
  Function called by my_net_init() to set some check variables
*/

extern "C" {
void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) global_system_variables.net_buffer_length;

  my_net_set_read_timeout(net, (uint)global_system_variables.net_read_timeout);
  my_net_set_write_timeout(net,
                           (uint)global_system_variables.net_write_timeout);

  net->retry_count=  (uint) global_system_variables.net_retry_count;
  net->max_packet_size= max<size_t>(global_system_variables.net_buffer_length,
                                    global_system_variables.max_allowed_packet);
}
}
