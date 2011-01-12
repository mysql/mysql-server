/* Copyright (C) 2003 MySQL AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifndef CLIENT_SETTINGS_INCLUDED
#define CLIENT_SETTINGS_INCLUDED
#else
#error You have already included an client_settings.h and it should not be included twice
#endif /* CLIENT_SETTINGS_INCLUDED */

#include <thr_alarm.h>
#include <sql_common.h>

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_SECURE_CONNECTION | CLIENT_TRANSACTIONS | \
			     CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION)

#define read_user_name(A) {}
#undef HAVE_SMEM
#undef _CUSTOMCONFIG_

#define mysql_server_init(a,b,c) mysql_client_plugin_init()
#define mysql_server_end()       mysql_client_plugin_deinit()

#ifdef HAVE_REPLICATION
C_MODE_START
void slave_io_thread_detach_vio();
C_MODE_END
#else
#define slave_io_thread_detach_vio()
#endif

