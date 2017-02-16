extern struct my_snprintf_service_st {
  size_t (*my_snprintf_type)(char*, size_t, const char*, ...);
  size_t (*my_vsnprintf_type)(char *, size_t, const char*, va_list);
} *my_snprintf_service;
size_t my_snprintf(char* to, size_t n, const char* fmt, ...);
size_t my_vsnprintf(char *to, size_t n, const char* fmt, va_list ap);
struct st_mysql_lex_string
{
  char *str;
  size_t length;
};
typedef struct st_mysql_lex_string MYSQL_LEX_STRING;
extern struct thd_alloc_service_st {
  void *(*thd_alloc_func)(void*, unsigned int);
  void *(*thd_calloc_func)(void*, unsigned int);
  char *(*thd_strdup_func)(void*, const char *);
  char *(*thd_strmake_func)(void*, const char *, unsigned int);
  void *(*thd_memdup_func)(void*, const void*, unsigned int);
  MYSQL_LEX_STRING *(*thd_make_lex_string_func)(void*, MYSQL_LEX_STRING *,
                                        const char *, unsigned int, int);
} *thd_alloc_service;
void *thd_alloc(void* thd, unsigned int size);
void *thd_calloc(void* thd, unsigned int size);
char *thd_strdup(void* thd, const char *str);
char *thd_strmake(void* thd, const char *str, unsigned int size);
void *thd_memdup(void* thd, const void* str, unsigned int size);
MYSQL_LEX_STRING *thd_make_lex_string(void* thd, MYSQL_LEX_STRING *lex_str,
                                      const char *str, unsigned int size,
                                      int allocate_lex_string);
typedef enum _thd_wait_type_e {
  THD_WAIT_SLEEP= 1,
  THD_WAIT_DISKIO= 2,
  THD_WAIT_ROW_LOCK= 3,
  THD_WAIT_GLOBAL_LOCK= 4,
  THD_WAIT_META_DATA_LOCK= 5,
  THD_WAIT_TABLE_LOCK= 6,
  THD_WAIT_USER_LOCK= 7,
  THD_WAIT_BINLOG= 8,
  THD_WAIT_GROUP_COMMIT= 9,
  THD_WAIT_SYNC= 10,
  THD_WAIT_LAST= 11
} thd_wait_type;
extern struct thd_wait_service_st {
  void (*thd_wait_begin_func)(void*, int);
  void (*thd_wait_end_func)(void*);
} *thd_wait_service;
void thd_wait_begin(void* thd, int wait_type);
void thd_wait_end(void* thd);
struct scheduler_functions;
extern struct my_thread_scheduler_service {
  int (*set)(struct scheduler_functions *scheduler);
  int (*reset)();
} *my_thread_scheduler_service;
int my_thread_scheduler_set(struct scheduler_functions *scheduler);
int my_thread_scheduler_reset();
extern struct progress_report_service_st {
  void (*thd_progress_init_func)(void* thd, unsigned int max_stage);
  void (*thd_progress_report_func)(void* thd,
                                   unsigned long long progress,
                                   unsigned long long max_progress);
  void (*thd_progress_next_stage_func)(void* thd);
  void (*thd_progress_end_func)(void* thd);
  const char *(*set_thd_proc_info_func)(void*, const char *info,
                                        const char *func,
                                        const char *file,
                                        unsigned int line);
} *progress_report_service;
void thd_progress_init(void* thd, unsigned int max_stage);
void thd_progress_report(void* thd,
                         unsigned long long progress,
                         unsigned long long max_progress);
void thd_progress_next_stage(void* thd);
void thd_progress_end(void* thd);
const char *set_thd_proc_info(void*, const char * info, const char *func,
                              const char *file, unsigned int line);
extern void (*debug_sync_C_callback_ptr)(void*, const char *, size_t);
enum thd_kill_levels {
  THD_IS_NOT_KILLED=0,
  THD_ABORT_SOFTLY=50,
  THD_ABORT_ASAP=100,
};
extern struct kill_statement_service_st {
  enum thd_kill_levels (*thd_kill_level_func)(const void*);
} *thd_kill_statement_service;
enum thd_kill_levels thd_kill_level(const void*);
typedef struct logger_handle_st LOGGER_HANDLE;
extern struct logger_service_st {
  void (*logger_init_mutexes)();
  LOGGER_HANDLE* (*open)(const char *path,
                         unsigned long long size_limit,
                         unsigned int rotations);
  int (*close)(LOGGER_HANDLE *log);
  int (*vprintf)(LOGGER_HANDLE *log, const char *fmt, va_list argptr);
  int (*printf)(LOGGER_HANDLE *log, const char *fmt, ...);
  int (*write)(LOGGER_HANDLE *log, const char *buffer, size_t size);
  int (*rotate)(LOGGER_HANDLE *log);
} *logger_service;
  void logger_init_mutexes();
  LOGGER_HANDLE *logger_open(const char *path,
                             unsigned long long size_limit,
                             unsigned int rotations);
  int logger_close(LOGGER_HANDLE *log);
  int logger_vprintf(LOGGER_HANDLE *log, const char *fmt, va_list argptr);
  int logger_printf(LOGGER_HANDLE *log, const char *fmt, ...);
  int logger_write(LOGGER_HANDLE *log, const char *buffer, size_t size);
  int logger_rotate(LOGGER_HANDLE *log);
struct st_mysql_xid {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
};
typedef struct st_mysql_xid MYSQL_XID;
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_UINT, SHOW_ULONG,
  SHOW_ULONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_DOUBLE,
  SHOW_SINT, SHOW_SLONG, SHOW_SLONGLONG,
  SHOW_always_last
};
struct st_mysql_show_var {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
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
  int (*init)(void *);
  int (*deinit)(void *);
  unsigned int version;
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  void * __reserved1;
  unsigned long flags;
};
struct st_maria_plugin
{
  int type;
  void *info;
  const char *name;
  const char *author;
  const char *descr;
  int license;
  int (*init)(void *);
  int (*deinit)(void *);
  unsigned int version;
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  const char *version_info;
  unsigned int maturity;
};
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
  char prev;
  char *quot;
} MYSQL_FTPARSER_BOOLEAN_INFO;
typedef struct st_mysql_ftparser_param
{
  int (*mysql_parse)(struct st_mysql_ftparser_param *,
                     const char *doc, int doc_len);
  int (*mysql_add_word)(struct st_mysql_ftparser_param *,
                        const char *word, int word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  void *ftparser_state;
  void *mysql_ftparam;
  const struct charset_info_st *cs;
  const char *doc;
  int length;
  unsigned int flags;
  enum enum_ftparser_mode mode;
} MYSQL_FTPARSER_PARAM;
struct st_mysql_ftparser
{
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
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
void **thd_ha_data(const void* thd, const struct handlerton *hton);
void thd_storage_lock_wait(void* thd, long long value);
int thd_tx_isolation(const void* thd);
char *thd_security_context(void* thd, char *buffer, unsigned int length,
                           unsigned int max_query_len);
void thd_inc_row_count(void* thd);
int mysql_tmpfile(const char *prefix);
unsigned long thd_get_thread_id(const void* thd);
void thd_get_xid(const void* thd, MYSQL_XID *xid);
void mysql_query_cache_invalidate4(void* thd,
                                   const char *key, unsigned int key_length,
                                   int using_trx);
void *thd_get_ha_data(const void* thd, const struct handlerton *hton);
void thd_set_ha_data(void* thd, const struct handlerton *hton,
                     const void *ha_data);
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
  unsigned long long query_id;
  const char *database;
  unsigned int database_length;
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
struct mysql_event_table
{
  unsigned int event_subclass;
  unsigned long thread_id;
  const char *user;
  const char *priv_user;
  const char *priv_host;
  const char *external_user;
  const char *proxy_user;
  const char *host;
  const char *ip;
  const char *database;
  unsigned int database_length;
  const char *table;
  unsigned int table_length;
  int read_only;
  const char *new_database;
  unsigned int new_database_length;
  const char *new_table;
  unsigned int new_table_length;
  unsigned long long query_id;
};
struct st_mysql_audit
{
  int interface_version;
  void (*release_thd)(void*);
  void (*event_notify)(void*, unsigned int, const void *);
  unsigned long class_mask[1];
};
