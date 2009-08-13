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


#include <thr_alarm.h>

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_SECURE_CONNECTION | CLIENT_TRANSACTIONS | \
			     CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION)

#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#define read_user_name(A) {}
#define mysql_rpl_query_type(A,B) MYSQL_RPL_ADMIN
#define mysql_master_send_query(A, B, C) 1
#define mysql_slave_send_query(A, B, C) 1
#define mysql_rpl_probe(mysql) 0
#undef HAVE_SMEM
#undef _CUSTOMCONFIG_

#define mysql_server_init(a,b,c) 0

#ifdef HAVE_REPLICATION
C_MODE_START
void slave_io_thread_detach_vio();
C_MODE_END
#else
#define slave_io_thread_detach_vio()
#endif

