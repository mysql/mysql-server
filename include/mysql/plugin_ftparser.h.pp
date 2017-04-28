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
#include "com_data.h"
typedef struct st_com_init_db_data
{
  const char *db_name;
  unsigned long length;
} COM_INIT_DB_DATA;
typedef struct st_com_refresh_data
{
  unsigned char options;
} COM_REFRESH_DATA;
typedef struct st_com_shutdown_data
{
  enum mysql_enum_shutdown_level level;
} COM_SHUTDOWN_DATA;
typedef struct st_com_kill_data
{
  unsigned long id;
} COM_KILL_DATA;
typedef struct st_com_set_option_data
{
  unsigned int opt_command;
} COM_SET_OPTION_DATA;
typedef struct st_com_stmt_execute_data
{
  unsigned long stmt_id;
  unsigned long flags;
  unsigned char *params;
  unsigned long params_length;
} COM_STMT_EXECUTE_DATA;
typedef struct st_com_stmt_fetch_data
{
  unsigned long stmt_id;
  unsigned long num_rows;
} COM_STMT_FETCH_DATA;
typedef struct st_com_stmt_send_long_data_data
{
  unsigned long stmt_id;
  unsigned int param_number;
  unsigned char *longdata;
  unsigned long length;
} COM_STMT_SEND_LONG_DATA_DATA;
typedef struct st_com_stmt_prepare_data
{
  const char *query;
  unsigned int length;
} COM_STMT_PREPARE_DATA;
typedef struct st_stmt_close_data
{
  unsigned int stmt_id;
} COM_STMT_CLOSE_DATA;
typedef struct st_com_stmt_reset_data
{
  unsigned int stmt_id;
} COM_STMT_RESET_DATA;
typedef struct st_com_query_data
{
  const char *query;
  unsigned int length;
} COM_QUERY_DATA;
typedef struct st_com_field_list_data
{
  unsigned char *table_name;
  unsigned int table_name_length;
  const unsigned char *query;
  unsigned int query_length;
} COM_FIELD_LIST_DATA;
union COM_DATA {
  COM_INIT_DB_DATA com_init_db;
  COM_REFRESH_DATA com_refresh;
  COM_SHUTDOWN_DATA com_shutdown;
  COM_KILL_DATA com_kill;
  COM_SET_OPTION_DATA com_set_option;
  COM_STMT_EXECUTE_DATA com_stmt_execute;
  COM_STMT_FETCH_DATA com_stmt_fetch;
  COM_STMT_SEND_LONG_DATA_DATA com_stmt_send_long_data;
  COM_STMT_PREPARE_DATA com_stmt_prepare;
  COM_STMT_CLOSE_DATA com_stmt_close;
  COM_STMT_RESET_DATA com_stmt_reset;
  COM_QUERY_DATA com_query;
  COM_FIELD_LIST_DATA com_field_list;
};
struct st_mysql_sqlshim
{
  int interface_version;
  my_bool (*shim_function)(void*, union COM_DATA*, enum enum_server_command, union COM_DATA*, enum enum_server_command*);
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
enum enum_ftparser_mode
{
  MYSQL_FTPARSER_SIMPLE_MODE= 0,
  MYSQL_FTPARSER_WITH_STOPWORDS= 1,
  MYSQL_FTPARSER_FULL_BOOLEAN_INFO= 2
};
enum enum_ft_token_type
{
  FT_TOKEN_EOF= 0,
  FT_TOKEN_WORD= 1,
  FT_TOKEN_LEFT_PAREN= 2,
  FT_TOKEN_RIGHT_PAREN= 3,
  FT_TOKEN_STOPWORD= 4
};
typedef struct st_mysql_ftparser_boolean_info
{
  enum enum_ft_token_type type;
  int yesno;
  int weight_adjust;
  char wasign;
  char trunc;
  int position;
  char prev;
  char *quot;
} MYSQL_FTPARSER_BOOLEAN_INFO;
typedef struct st_mysql_ftparser_param
{
  int (*mysql_parse)(struct st_mysql_ftparser_param *,
                     char *doc, int doc_len);
  int (*mysql_add_word)(struct st_mysql_ftparser_param *,
                        char *word, int word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  void *ftparser_state;
  void *mysql_ftparam;
  const struct charset_info_st *cs;
  char *doc;
  int length;
  int flags;
  enum enum_ftparser_mode mode;
} MYSQL_FTPARSER_PARAM;
struct st_mysql_ftparser
{
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
};
