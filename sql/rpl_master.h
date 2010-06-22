#ifndef RPL_MASTER_H_INCLUDED
/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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


#define RPL_MASTER_H_INCLUDED
extern bool server_id_supplied;
extern int max_binlog_dump_events;
extern my_bool opt_sporadic_binlog_dump_fail;
extern my_bool opt_show_slave_auth_info;

typedef struct st_slave_info
{
  uint32 server_id;
  uint32 rpl_recovery_rank, master_id;
  char host[HOSTNAME_LENGTH+1];
  char user[USERNAME_LENGTH+1];
  char password[MAX_PASSWORD_LENGTH+1];
  uint16 port;
  THD* thd;
} SLAVE_INFO;

String *get_slave_uuid(THD *thd, String *value);
bool mysql_show_binlog_events(THD* thd);
bool show_binlogs(THD* thd);
void kill_zombie_dump_threads(String *slave_uuid);
void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos, ushort flags);
int reset_master(THD* thd);
#endif /* RPL_MASTER_H_INCLUDED */
