/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB & Sasha

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "slave.h"

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

extern my_bool opt_show_slave_auth_info;
extern char *master_host, *master_info_file;
extern bool server_id_supplied;
extern I_List<i_string> binlog_do_db, binlog_ignore_db;

extern int max_binlog_dump_events;
extern my_bool opt_sporadic_binlog_dump_fail;

#define KICK_SLAVE(thd) { pthread_mutex_lock(&(thd)->LOCK_delete); (thd)->awake(0 /* do not prepare to die*/); pthread_mutex_unlock(&(thd)->LOCK_delete); }

File open_binlog(IO_CACHE *log, const char *log_file_name,
		 const char **errmsg);

int start_slave(THD* thd, MASTER_INFO* mi, bool net_report);
int stop_slave(THD* thd, MASTER_INFO* mi, bool net_report);
int change_master(THD* thd, MASTER_INFO* mi);
int show_binlog_events(THD* thd);
int cmp_master_pos(const char* log_file_name1, ulonglong log_pos1,
		   const char* log_file_name2, ulonglong log_pos2);
int reset_slave(THD *thd, MASTER_INFO* mi);
int reset_master(THD* thd);
int purge_master_logs(THD* thd, const char* to_log);
bool log_in_use(const char* log_name);
void adjust_linfo_offsets(my_off_t purge_offset);
int show_binlogs(THD* thd);
extern int init_master_info(MASTER_INFO* mi);
void kill_zombie_dump_threads(uint32 slave_server_id);
int check_binlog_magic(IO_CACHE* log, const char** errmsg);

typedef struct st_load_file_info
{
  THD* thd;
  my_off_t last_pos_in_file;
  sql_exchange* ex;
  List <Item> *fields;
  enum enum_duplicates handle_dup;
  char* db;
  char* table_name;
  bool wrote_create_file, log_delayed;
} LOAD_FILE_INFO;

int log_loaded_block(IO_CACHE* file);
