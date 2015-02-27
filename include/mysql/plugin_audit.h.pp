#include "plugin.h"
typedef void * MYSQL_PLUGIN;
struct st_mysql_xid {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
};
typedef struct st_mysql_xid MYSQL_XID;
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL,
  SHOW_INT,
  SHOW_LONG,
  SHOW_LONGLONG,
  SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_DOUBLE
};
enum enum_mysql_show_scope
{
  SHOW_SCOPE_UNDEF,
  SHOW_SCOPE_GLOBAL
};
struct st_mysql_show_var
{
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
  enum enum_mysql_show_scope scope;
};
typedef int (*mysql_show_var_func)(void*, struct st_mysql_show_var*, char *);
struct st_mysql_sys_var;
struct st_mysql_value;
typedef int (*mysql_var_check_func)(void* thd,
                                    struct st_mysql_sys_var *var,
                                    void *save, struct st_mysql_value *value);
typedef void (*mysql_var_update_func)(void* thd,
                                      struct st_mysql_sys_var *var,
                                      void *var_ptr, const void *save);
struct st_mysql_plugin
{
  int type;
  void *info;
  const char *name;
  const char *author;
  const char *descr;
  int license;
  int (*init)(MYSQL_PLUGIN);
  int (*deinit)(MYSQL_PLUGIN);
  unsigned int version;
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  void * __reserved1;
  unsigned long flags;
};
struct st_mysql_daemon
{
  int interface_version;
};
struct st_mysql_information_schema
{
  int interface_version;
};
struct st_mysql_storage_engine
{
  int interface_version;
};
struct handlerton;
 struct Mysql_replication {
   int interface_version;
 };
struct st_mysql_value
{
  int (*value_type)(struct st_mysql_value *);
  const char *(*val_str)(struct st_mysql_value *, char *buffer, int *length);
  int (*val_real)(struct st_mysql_value *, double *realbuf);
  int (*val_int)(struct st_mysql_value *, long long *intbuf);
  int (*is_unsigned)(struct st_mysql_value *);
};
int thd_in_lock_tables(const void* thd);
int thd_tablespace_op(const void* thd);
long long thd_test_options(const void* thd, long long test_options);
int thd_sql_command(const void* thd);
const char *set_thd_proc_info(void* thd, const char *info,
                              const char *calling_func,
                              const char *calling_file,
                              const unsigned int calling_line);
void **thd_ha_data(const void* thd, const struct handlerton *hton);
void thd_storage_lock_wait(void* thd, long long value);
int thd_tx_isolation(const void* thd);
int thd_tx_is_read_only(const void* thd);
void* thd_tx_arbitrate(void* requestor, void* holder);
int thd_tx_priority(const void* thd);
int thd_tx_is_dd_trx(const void* thd);
char *thd_security_context(void* thd, char *buffer, size_t length,
                           size_t max_query_len);
void thd_inc_row_count(void* thd);
int thd_allow_batch(void* thd);
void thd_mark_transaction_to_rollback(void* thd, int all);
int mysql_tmpfile(const char *prefix);
int thd_killed(const void* thd);
void thd_set_kill_status(const void* thd);
void thd_binlog_pos(const void* thd,
                    const char **file_var,
                    unsigned long long *pos_var);
unsigned long thd_get_thread_id(const void* thd);
void thd_get_xid(const void* thd, MYSQL_XID *xid);
void mysql_query_cache_invalidate4(void* thd,
                                   const char *key, unsigned int key_length,
                                   int using_trx);
void *thd_get_ha_data(const void* thd, const struct handlerton *hton);
void thd_set_ha_data(void* thd, const struct handlerton *hton,
                     const void *ha_data);
#include "mysql/mysql_lex_string.h"
struct st_mysql_lex_string
{
  char *str;
  size_t length;
};
typedef struct st_mysql_lex_string MYSQL_LEX_STRING;
struct mysql_event_general
{
  unsigned int event_subclass;
  int general_error_code;
  unsigned long general_thread_id;
  const char *general_user;
  unsigned int general_user_length;
  const char *general_command;
  unsigned int general_command_length;
  const char *general_query;
  unsigned int general_query_length;
  struct charset_info_st *general_charset;
  unsigned long long general_time;
  unsigned long long general_rows;
  MYSQL_LEX_STRING general_host;
  MYSQL_LEX_STRING general_sql_command;
  MYSQL_LEX_STRING general_external_user;
  MYSQL_LEX_STRING general_ip;
};
struct mysql_event_connection
{
  unsigned int event_subclass;
  int status;
  unsigned long thread_id;
  const char *user;
  unsigned int user_length;
  const char *priv_user;
  unsigned int priv_user_length;
  const char *external_user;
  unsigned int external_user_length;
  const char *proxy_user;
  unsigned int proxy_user_length;
  const char *host;
  unsigned int host_length;
  const char *ip;
  unsigned int ip_length;
  const char *database;
  unsigned int database_length;
};
struct st_mysql_audit
{
  int interface_version;
  void (*release_thd)(void*);
  void (*event_notify)(void*, unsigned int, const void *);
  unsigned long class_mask[1];
};
