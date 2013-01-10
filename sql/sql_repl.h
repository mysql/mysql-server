/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_REPL_INCLUDED
#define SQL_REPL_INCLUDED

#include "rpl_filter.h"

#ifdef HAVE_REPLICATION
#include "slave.h"

typedef struct st_slave_info
{
  uint32 server_id;
  uint32 rpl_recovery_rank, master_id;
  char host[HOSTNAME_LENGTH*SYSTEM_CHARSET_MBMAXLEN+1];
  char user[USERNAME_LENGTH+1];
  char password[MAX_PASSWORD_LENGTH*SYSTEM_CHARSET_MBMAXLEN+1];
  uint16 port;
  THD* thd;
} SLAVE_INFO;

extern my_bool opt_show_slave_auth_info;
extern char *master_host, *master_info_file;
extern bool server_id_supplied;

extern int max_binlog_dump_events;
extern my_bool opt_sporadic_binlog_dump_fail;

int start_slave(THD* thd, Master_info* mi, bool net_report);
int stop_slave(THD* thd, Master_info* mi, bool net_report);
bool change_master(THD* thd, Master_info* mi);
bool mysql_show_binlog_events(THD* thd);
int reset_slave(THD *thd, Master_info* mi);
int reset_master(THD* thd);
bool purge_master_logs(THD* thd, const char* to_log);
bool purge_master_logs_before_date(THD* thd, time_t purge_time);
bool log_in_use(const char* log_name);
void adjust_linfo_offsets(my_off_t purge_offset);
bool show_binlogs(THD* thd);
extern int init_master_info(Master_info* mi);
void kill_zombie_dump_threads(uint32 slave_server_id);
int check_binlog_magic(IO_CACHE* log, const char** errmsg);

typedef struct st_load_file_info
{
  THD* thd;
  my_off_t last_pos_in_file;
  bool wrote_create_file, log_delayed;
} LOAD_FILE_INFO;

int log_loaded_block(IO_CACHE* file);
int init_replication_sys_vars();
void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos, ushort flags);

#endif /* HAVE_REPLICATION */

#endif /* SQL_REPL_INCLUDED */
