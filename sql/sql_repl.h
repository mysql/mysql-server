#ifndef SQL_REPL_H
#define SQL_REPL_H

#include "slave.h"

extern char* master_host;
extern my_string opt_bin_logname, master_info_file;
extern uint32 server_id;
extern bool server_id_supplied;
extern I_List<i_string> binlog_do_db, binlog_ignore_db;

File open_binlog(IO_CACHE *log, const char *log_file_name,
	      const char **errmsg);

int start_slave(THD* thd = 0, bool net_report = 1);
int stop_slave(THD* thd = 0, bool net_report = 1);
int change_master(THD* thd);
void reset_slave();
void reset_master();
int purge_master_logs(THD* thd, const char* to_log);
bool log_in_use(const char* log_name);
void adjust_linfo_offsets(my_off_t purge_offset);
int show_binlogs(THD* thd);
extern int init_master_info(MASTER_INFO* mi);
void kill_zombie_dump_threads(uint32 slave_server_id);

#endif
