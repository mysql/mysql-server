/* Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.
   
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


#ifndef CLIENT_SETTINGS_INCLUDED
#define CLIENT_SETTINGS_INCLUDED
#else
#error You have already included an client_settings.h and it should not be included twice
#endif /* CLIENT_SETTINGS_INCLUDED */

#include <sql_common.h>

/*
 Note: CLIENT_CAPABILITIES is also defined in libmysql/client_settings.h.
 When adding capabilities here, consider if they should be also added to
 the libmysql version.
*/
#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | \
                             CLIENT_LONG_FLAG |     \
                             CLIENT_TRANSACTIONS |  \
                             CLIENT_PROTOCOL_41 |   \
                             CLIENT_RESERVED2 | \
                             CLIENT_PLUGIN_AUTH | \
                             CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA | \
                             CLIENT_CONNECT_ATTRS | \
                             CLIENT_SESSION_TRACK | \
                             CLIENT_DEPRECATE_EOF)

#define read_user_name(A) {}

#define mysql_server_init(a,b,c) mysql_client_plugin_init()
#define mysql_server_end()       mysql_client_plugin_deinit()

#ifdef HAVE_REPLICATION
C_MODE_START
void slave_io_thread_detach_vio();
C_MODE_END
#else
#define slave_io_thread_detach_vio()
#endif

