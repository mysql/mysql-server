#ifndef SQL_REPL_H
#define SQL_REPL_H

#include "slave.h"

typedef struct st_slave_info
{
  uint32 server_id;
  uint32 rpl_recovery_rank, master_id;
  char host[HOSTNAME_LENGTH+1];
  char user[USERNAME_LENGTH+1];
  char password[HASH_PASSWORD_LENGTH+1];
  uint16 port;
  THD* thd;
} SLAVE_INFO;

extern bool opt_show_slave_auth_info, opt_old_rpl_compat;
extern char* master_host;
extern my_string opt_bin_logname, master_info_file;
extern uint32 server_id;
extern bool server_id_supplied;
extern I_List<i_string> binlog_do_db, binlog_ignore_db;

#ifndef DBUG_OFF
extern int max_binlog_dump_events;
extern bool opt_sporadic_binlog_dump_fail;
#endif

#define KICK_SLAVE(thd) thd->awake(0 /* do not prepare to die*/);

File open_binlog(IO_CACHE *log, const char *log_file_name,
	      const char **errmsg);

int start_slave(THD* thd, MASTER_INFO* mi, bool net_report);
int stop_slave(THD* thd, MASTER_INFO* mi, bool net_report);
int change_master(THD* thd, MASTER_INFO* mi);
int show_binlog_events(THD* thd);
int cmp_master_pos(const char* log_file_name1, ulonglong log_pos1,
		   const char* log_file_name2, ulonglong log_pos2);
int reset_slave(MASTER_INFO* mi);
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
  sql_exchange* ex;
  List <Item> *fields;
  enum enum_duplicates handle_dup;
  char* db;
  char* table_name;
  bool wrote_create_file;
  my_off_t last_pos_in_file;
} LOAD_FILE_INFO;

int log_loaded_block(IO_CACHE* file);

#endif

