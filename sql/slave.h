#ifndef SLAVE_H
#define SLAVE_H

#define SLAVE_NET_TIMEOUT  3600

extern ulong slave_net_timeout;

typedef struct st_master_info
{
  char log_file_name[FN_REFLEN];
  ulonglong pos,pending;
  File fd; // we keep the file open, so we need to remember the file pointer
  IO_CACHE file;
  // the variables below are needed because we can change masters on the fly
  char host[HOSTNAME_LENGTH+1];
  char user[USERNAME_LENGTH+1];
  char password[HASH_PASSWORD_LENGTH+1];
  uint port;
  uint connect_retry;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  bool inited;
  
  st_master_info():pending(0),fd(-1),inited(0)
  {
    host[0] = 0; user[0] = 0; password[0] = 0;
    pthread_mutex_init(&lock, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&cond, NULL);
  }

  ~st_master_info()
  {
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
  }
  inline void inc_pending(ulonglong val)
  {
    pending += val;
  }
  inline void inc_pos(ulonglong val)
  {
    pthread_mutex_lock(&lock);
    pos += val + pending;
    pending = 0;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
  }
  // thread safe read of position - not needed if we are in the slave thread,
  // but required otherwise
  inline void read_pos(ulonglong& var)
  {
    pthread_mutex_lock(&lock);
    var = pos;
    pthread_mutex_unlock(&lock);
  }

  int wait_for_pos(THD* thd, String* log_name, ulonglong log_pos);
} MASTER_INFO;

typedef struct st_table_rule_ent
{
  char* db;
  char* tbl_name;
  uint key_len;
} TABLE_RULE_ENT;

#define TABLE_RULE_HASH_SIZE   16
#define TABLE_RULE_ARR_SIZE   16

int flush_master_info(MASTER_INFO* mi);

int mysql_table_dump(THD* thd, char* db, char* tbl_name, int fd = -1);
// if fd is -1, dump to NET
int fetch_nx_table(THD* thd, MASTER_INFO* mi);
// retrieve non-exitent table from master
// the caller must set thd->last_nx_table and thd->last_nx_db first
int show_master_info(THD* thd);
int show_binlog_info(THD* thd);

int tables_ok(THD* thd, TABLE_LIST* tables);
// see if the query uses any tables that should not be replicated

int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list );
// check to see if the database is ok to operate on with respect to the
// do and ignore lists - used in replication

int add_table_rule(HASH* h, const char* table_spec);
int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec);
void init_table_rule_hash(HASH* h, bool* h_inited);
void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited);

void end_slave(); // clean up
int init_master_info(MASTER_INFO* mi);
void end_master_info(MASTER_INFO* mi);
extern bool opt_log_slave_updates ;
pthread_handler_decl(handle_slave,arg);
extern bool volatile abort_loop, abort_slave, slave_running;
extern uint32 slave_skip_counter;
// needed for problems when slave stops and
// we want to restart it skipping one or more events in the master log that
// have caused errors, and have been manually applied by DBA already

extern pthread_t slave_real_id;
extern THD* slave_thd;
extern MASTER_INFO glob_mi;
extern HASH replicate_do_table, replicate_ignore_table;
extern DYNAMIC_ARRAY  replicate_wild_do_table, replicate_wild_ignore_table;
extern bool do_table_inited, ignore_table_inited,
	    wild_do_table_inited, wild_ignore_table_inited;
extern bool table_rules_on;

#ifndef DBUG_OFF
extern int disconnect_slave_event_count, abort_slave_event_count ;
#endif

// the master variables are defaults read from my.cnf or command line
extern uint master_port, master_connect_retry;
extern my_string master_user, master_password, master_host,
  master_info_file;

extern I_List<i_string> replicate_do_db, replicate_ignore_db;
extern I_List<i_string_pair> replicate_rewrite_db;
extern I_List<THD> threads;

#endif


