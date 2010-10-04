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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


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

void init_slave_list();
void end_slave_list();
int register_slave(THD* thd, uchar* packet, uint packet_length);
void unregister_slave(THD* thd, bool only_mine, bool need_mutex);
bool show_slave_hosts(THD* thd);

String *get_slave_uuid(THD *thd, String *value);
bool mysql_show_binlog_events(THD* thd);
bool show_binlogs(THD* thd);
void kill_zombie_dump_threads(String *slave_uuid);
void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos, ushort flags);
int reset_master(THD* thd);
#endif /* RPL_MASTER_H_INCLUDED */
