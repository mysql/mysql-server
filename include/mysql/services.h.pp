#include <mysql/service_srv_session.h>
struct Srv_session;
typedef struct Srv_session* MYSQL_SESSION;
typedef void (*srv_session_error_cb)(void *ctx,
                                     unsigned int sql_errno,
                                     const char *err_msg);
extern struct srv_session_service_st
{
  int (*init_session_thread)(const void *plugin);
  void (*deinit_session_thread)();
  MYSQL_SESSION (*open_session)(srv_session_error_cb error_cb,
                                void *plugix_ctx);
  int (*detach_session)(MYSQL_SESSION session);
  int (*close_session)(MYSQL_SESSION session);
  int (*server_is_available)();
  int (*attach_session)(MYSQL_SESSION session, MYSQL_THD *ret_previous_thd);
} *srv_session_service;
int srv_session_init_thread(const void *plugin);
void srv_session_deinit_thread();
MYSQL_SESSION srv_session_open(srv_session_error_cb error_cb, void *plugin_ctx);
int srv_session_detach(MYSQL_SESSION session);
int srv_session_close(MYSQL_SESSION session);
int srv_session_server_is_available();
int srv_session_attach(MYSQL_SESSION session, MYSQL_THD *ret_previous_thd);
#include <mysql/service_srv_session_info.h>
#include "mysql/service_srv_session.h"
extern struct srv_session_info_service_st {
  MYSQL_THD (*get_thd)(MYSQL_SESSION session);
  my_thread_id (*get_session_id)(MYSQL_SESSION session);
  LEX_CSTRING (*get_current_db)(MYSQL_SESSION session);
  uint16_t (*get_client_port)(MYSQL_SESSION session);
  int (*set_client_port)(MYSQL_SESSION session, uint16_t port);
  int (*set_connection_type)(MYSQL_SESSION session, enum enum_vio_type type);
  int (*killed)(MYSQL_SESSION session);
  unsigned int (*session_count)();
  unsigned int (*thread_count)(const void *plugin);
} *srv_session_info_service;
MYSQL_THD srv_session_info_get_thd(MYSQL_SESSION session);
my_thread_id srv_session_info_get_session_id(MYSQL_SESSION session);
LEX_CSTRING srv_session_info_get_current_db(MYSQL_SESSION session);
uint16_t srv_session_info_get_client_port(MYSQL_SESSION session);
int srv_session_info_set_client_port(MYSQL_SESSION session, uint16_t port);
int srv_session_info_set_connection_type(MYSQL_SESSION session,
                                         enum enum_vio_type type);
int srv_session_info_killed(MYSQL_SESSION session);
unsigned int srv_session_info_session_count();
unsigned int srv_session_info_thread_count(const void *plugin);
#include <mysql/service_command.h>
#include "mysql/service_srv_session.h"
#include "mysql/com_data.h"
typedef struct st_com_init_db_data
{
  const char *db_name;
  unsigned long length;
} COM_INIT_DB_DATA;
typedef struct st_com_refresh_data
{
  unsigned char options;
} COM_REFRESH_DATA;
typedef struct st_com_kill_data
{
  unsigned long id;
} COM_KILL_DATA;
typedef struct st_com_set_option_data
{
  unsigned int opt_command;
} COM_SET_OPTION_DATA;
typedef struct st_ps_param
{
  unsigned char null_bit;
  enum enum_field_types type;
  unsigned char unsigned_type;
  const unsigned char *value;
  unsigned long length;
} PS_PARAM;
typedef struct st_com_stmt_execute_data
{
  unsigned long stmt_id;
  unsigned long open_cursor;
  PS_PARAM *parameters;
  unsigned long parameter_count;
  unsigned char has_new_types;
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
#include "mysql_time.h"
enum enum_mysql_timestamp_type
{
  MYSQL_TIMESTAMP_NONE= -2, MYSQL_TIMESTAMP_ERROR= -1,
  MYSQL_TIMESTAMP_DATE= 0, MYSQL_TIMESTAMP_DATETIME= 1, MYSQL_TIMESTAMP_TIME= 2
};
typedef struct st_mysql_time
{
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part;
  bool neg;
  enum enum_mysql_timestamp_type time_type;
} MYSQL_TIME;
#include "decimal.h"
#include "my_inttypes.h"
#include "my_config.h"
typedef unsigned char uchar;
typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef unsigned long long int ulonglong;
typedef long long int longlong;
typedef longlong int64;
typedef ulonglong uint64;
typedef unsigned long long my_ulonglong;
typedef intptr_t intptr;
typedef ulonglong my_off_t;
typedef ptrdiff_t my_ptrdiff_t;
typedef int myf;
#include "my_macros.h"
typedef enum
{TRUNCATE=0, HALF_EVEN, HALF_UP, CEILING, FLOOR}
  decimal_round_mode;
typedef int32 decimal_digit_t;
typedef struct st_decimal_t {
  int intg, frac, len;
  bool sign;
  decimal_digit_t *buf;
} decimal_t;
struct st_send_field
{
  const char *db_name;
  const char *table_name;
  const char *org_table_name;
  const char *col_name;
  const char *org_col_name;
  unsigned long length;
  unsigned int charsetnr;
  unsigned int flags;
  unsigned int decimals;
  enum_field_types type;
};
typedef int (*start_result_metadata_t)(void *ctx, uint num_cols, uint flags,
                                       const CHARSET_INFO *resultcs);
typedef int (*field_metadata_t)(void *ctx, struct st_send_field *field,
                                const CHARSET_INFO *charset);
typedef int (*end_result_metadata_t)(void *ctx, uint server_status,
                                     uint warn_count);
typedef int (*start_row_t)(void *ctx);
typedef int (*end_row_t)(void *ctx);
typedef void (*abort_row_t)(void *ctx);
typedef ulong (*get_client_capabilities_t)(void *ctx);
typedef int (*get_null_t)(void * ctx);
typedef int (*get_integer_t)(void * ctx, longlong value);
typedef int (*get_longlong_t)(void * ctx, longlong value, uint is_unsigned);
typedef int (*get_decimal_t)(void * ctx, const decimal_t * value);
typedef int (*get_double_t)(void * ctx, double value, uint32_t decimals);
typedef int (*get_date_t)(void * ctx, const MYSQL_TIME * value);
typedef int (*get_time_t)(void * ctx, const MYSQL_TIME * value, uint decimals);
typedef int (*get_datetime_t)(void * ctx, const MYSQL_TIME * value, uint decimals);
typedef int (*get_string_t)(void * ctx, const char * value, size_t length,
                            const CHARSET_INFO * valuecs);
typedef void (*handle_ok_t)(void * ctx,
                            uint server_status, uint statement_warn_count,
                            ulonglong affected_rows, ulonglong last_insert_id,
                            const char * message);
typedef void (*handle_error_t)(void * ctx, uint sql_errno, const char * err_msg,
                               const char * sqlstate);
typedef void (*shutdown_t)(void *ctx, int server_shutdown);
struct st_command_service_cbs
{
  start_result_metadata_t start_result_metadata;
  field_metadata_t field_metadata;
  end_result_metadata_t end_result_metadata;
  start_row_t start_row;
  end_row_t end_row;
  abort_row_t abort_row;
  get_client_capabilities_t get_client_capabilities;
  get_null_t get_null;
  get_integer_t get_integer;
  get_longlong_t get_longlong;
  get_decimal_t get_decimal;
  get_double_t get_double;
  get_date_t get_date;
  get_time_t get_time;
  get_datetime_t get_datetime;
  get_string_t get_string;
  handle_ok_t handle_ok;
  handle_error_t handle_error;
  shutdown_t shutdown;
};
enum cs_text_or_binary
{
  CS_TEXT_REPRESENTATION= 1,
  CS_BINARY_REPRESENTATION= 2,
};
extern struct command_service_st {
  int (*run_command)(MYSQL_SESSION session,
                     enum enum_server_command command,
                     const union COM_DATA * data,
                     const CHARSET_INFO * client_cs,
                     const struct st_command_service_cbs * callbacks,
                     enum cs_text_or_binary text_or_binary,
                     void * service_callbacks_ctx);
} *command_service;
int command_service_run_command(MYSQL_SESSION session,
                                enum enum_server_command command,
                                const union COM_DATA * data,
                                const CHARSET_INFO * client_cs,
                                const struct st_command_service_cbs * callbacks,
                                enum cs_text_or_binary text_or_binary,
                                void * service_callbacks_ctx);
#include <mysql/service_my_snprintf.h>
extern struct my_snprintf_service_st {
  size_t (*my_snprintf_type)(char*, size_t, const char*, ...);
  size_t (*my_vsnprintf_type)(char *to, size_t n, const char* fmt, va_list ap);
} *my_snprintf_service;
size_t my_snprintf(char* to, size_t n, const char* fmt, ...);
size_t my_vsnprintf(char *to, size_t n, const char* fmt, va_list ap);
#include <mysql/service_thd_alloc.h>
#include <mysql/mysql_lex_string.h>
struct st_mysql_lex_string
{
  char *str;
  size_t length;
};
typedef struct st_mysql_lex_string MYSQL_LEX_STRING;
struct st_mysql_const_lex_string
{
  const char *str;
  size_t length;
};
typedef struct st_mysql_const_lex_string MYSQL_LEX_CSTRING;
extern struct thd_alloc_service_st {
  void *(*thd_alloc_func)(void*, size_t);
  void *(*thd_calloc_func)(void*, size_t);
  char *(*thd_strdup_func)(void*, const char *);
  char *(*thd_strmake_func)(void*, const char *, size_t);
  void *(*thd_memdup_func)(void*, const void*, size_t);
  MYSQL_LEX_STRING *(*thd_make_lex_string_func)(void*, MYSQL_LEX_STRING *,
                                        const char *, size_t, int);
} *thd_alloc_service;
void *thd_alloc(void* thd, size_t size);
void *thd_calloc(void* thd, size_t size);
char *thd_strdup(void* thd, const char *str);
char *thd_strmake(void* thd, const char *str, size_t size);
void *thd_memdup(void* thd, const void* str, size_t size);
MYSQL_LEX_STRING *thd_make_lex_string(void* thd, MYSQL_LEX_STRING *lex_str,
                                      const char *str, size_t size,
                                      int allocate_lex_string);
#include <mysql/service_thd_wait.h>
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
#include <mysql/service_thread_scheduler.h>
struct Connection_handler_functions;
struct THD_event_functions;
extern struct my_thread_scheduler_service {
  int (*connection_handler_set)(struct Connection_handler_functions *,
                                struct THD_event_functions *);
  int (*connection_handler_reset)();
} *my_thread_scheduler_service;
int my_connection_handler_set(struct Connection_handler_functions *chf,
                              struct THD_event_functions *tef);
int my_connection_handler_reset();
#include <mysql/service_my_plugin_log.h>
enum plugin_log_level
{
  MY_ERROR_LEVEL,
  MY_WARNING_LEVEL,
  MY_INFORMATION_LEVEL
};
extern struct my_plugin_log_service
{
  int (*my_plugin_log_message)(MYSQL_PLUGIN *, enum plugin_log_level, const char *, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)));
} *my_plugin_log_service;
int my_plugin_log_message(MYSQL_PLUGIN *plugin, enum plugin_log_level level,
                          const char *format, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));
#include <mysql/service_mysql_string.h>
typedef void *mysql_string_iterator_handle;
typedef void *mysql_string_handle;
extern struct mysql_string_service_st {
  int (*mysql_string_convert_to_char_ptr_type)
       (mysql_string_handle, const char *, char *, unsigned int, int *);
  mysql_string_iterator_handle (*mysql_string_get_iterator_type)
                                (mysql_string_handle);
  int (*mysql_string_iterator_next_type)(mysql_string_iterator_handle);
  int (*mysql_string_iterator_isupper_type)(mysql_string_iterator_handle);
  int (*mysql_string_iterator_islower_type)(mysql_string_iterator_handle);
  int (*mysql_string_iterator_isdigit_type)(mysql_string_iterator_handle);
  mysql_string_handle (*mysql_string_to_lowercase_type)(mysql_string_handle);
  void (*mysql_string_free_type)(mysql_string_handle);
  void (*mysql_string_iterator_free_type)(mysql_string_iterator_handle);
} *mysql_string_service;
int mysql_string_convert_to_char_ptr(mysql_string_handle string_handle,
                                     const char *charset_name, char *buffer,
                                     unsigned int buffer_size, int *error);
mysql_string_iterator_handle mysql_string_get_iterator(mysql_string_handle
                                                       string_handle);
int mysql_string_iterator_next(mysql_string_iterator_handle iterator_handle);
int mysql_string_iterator_isupper(mysql_string_iterator_handle iterator_handle);
int mysql_string_iterator_islower(mysql_string_iterator_handle iterator_handle);
int mysql_string_iterator_isdigit(mysql_string_iterator_handle iterator_handle);
mysql_string_handle mysql_string_to_lowercase(mysql_string_handle
                                              string_handle);
void mysql_string_free(mysql_string_handle);
void mysql_string_iterator_free(mysql_string_iterator_handle);
#include <mysql/service_mysql_alloc.h>
#include "mysql/components/services/psi_memory_bits.h"
typedef unsigned int PSI_memory_key;
struct PSI_thread;
struct PSI_memory_info_v1
{
  PSI_memory_key *m_key;
  const char *m_name;
  unsigned int m_flags;
  int m_volatility;
  const char *m_documentation;
};
typedef struct PSI_memory_info_v1 PSI_memory_info_v1;
typedef void (*register_memory_v1_t)(const char *category,
                                     struct PSI_memory_info_v1 *info,
                                     int count);
typedef PSI_memory_key (*memory_alloc_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);
typedef PSI_memory_key (*memory_realloc_v1_t)(PSI_memory_key key,
                                              size_t old_size,
                                              size_t new_size,
                                              struct PSI_thread **owner);
typedef PSI_memory_key (*memory_claim_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);
typedef void (*memory_free_v1_t)(PSI_memory_key key,
                                 size_t size,
                                 struct PSI_thread *owner);
typedef struct PSI_memory_info_v1 PSI_memory_info;
typedef int myf_t;
typedef void * (*mysql_malloc_t)(PSI_memory_key key, size_t size, myf_t flags);
typedef void * (*mysql_realloc_t)(PSI_memory_key key, void *ptr, size_t size, myf_t flags);
typedef void (*mysql_claim_t)(const void *ptr);
typedef void (*mysql_free_t)(void *ptr);
typedef void * (*my_memdup_t)(PSI_memory_key key, const void *from, size_t length, myf_t flags);
typedef char * (*my_strdup_t)(PSI_memory_key key, const char *from, myf_t flags);
typedef char * (*my_strndup_t)(PSI_memory_key key, const char *from, size_t length, myf_t flags);
struct mysql_malloc_service_st
{
  mysql_malloc_t mysql_malloc;
  mysql_realloc_t mysql_realloc;
  mysql_claim_t mysql_claim;
  mysql_free_t mysql_free;
  my_memdup_t my_memdup;
  my_strdup_t my_strdup;
  my_strndup_t my_strndup;
};
extern struct mysql_malloc_service_st *mysql_malloc_service;
extern void * my_malloc(PSI_memory_key key, size_t size, myf_t flags);
extern void * my_realloc(PSI_memory_key key, void *ptr, size_t size, myf_t flags);
extern void my_claim(const void *ptr);
extern void my_free(void *ptr);
extern void * my_memdup(PSI_memory_key key, const void *from, size_t length, myf_t flags);
extern char * my_strdup(PSI_memory_key key, const char *from, myf_t flags);
extern char * my_strndup(PSI_memory_key key, const char *from, size_t length, myf_t flags);
#include <mysql/service_mysql_password_policy.h>
extern struct mysql_password_policy_service_st {
  int (*my_validate_password_policy_func)(const char *, unsigned int);
  int (*my_calculate_password_strength_func)(const char *, unsigned int);
} *mysql_password_policy_service;
int my_validate_password_policy(const char *, unsigned int);
int my_calculate_password_strength(const char *, unsigned int);
#include <mysql/service_parser.h>
#include <mysql/mysql_lex_string.h>
typedef void* MYSQL_ITEM;
typedef
int (*parse_node_visit_function)(MYSQL_ITEM item, unsigned char* arg);
typedef
int (*sql_condition_handler_function)(int sql_errno,
                                      const char* sqlstate,
                                      const char* msg,
                                      void *state);
struct st_my_thread_handle;
typedef void* (*mysql_current_session_t)();
typedef void* (*mysql_open_session_t)();
typedef void (*mysql_start_thread_t)(void* thd,
                                     void *(*callback_fun)(void*),
                                     void *arg,
                                     struct st_my_thread_handle *thread_handle);
typedef void (*mysql_join_thread_t)(struct st_my_thread_handle *thread_handle);
typedef void (*mysql_set_current_database_t)(void* thd, const MYSQL_LEX_STRING db);
typedef int (*mysql_parse_t)(void* thd, const MYSQL_LEX_STRING query,
                             unsigned char is_prepared,
                             sql_condition_handler_function handle_condition,
                             void *condition_handler_state);
typedef int (*mysql_get_statement_type_t)(void* thd);
typedef int (*mysql_get_statement_digest_t)(void* thd, unsigned char *digest);
typedef int (*mysql_get_number_params_t)(void* thd);
typedef int (*mysql_extract_prepared_params_t)(void* thd, int *positions);
typedef int (*mysql_visit_tree_t)(void* thd,
                                  parse_node_visit_function processor,
                                  unsigned char* arg);
typedef MYSQL_LEX_STRING (*mysql_item_string_t)(MYSQL_ITEM item);
typedef void (*mysql_free_string_t)(MYSQL_LEX_STRING string);
typedef MYSQL_LEX_STRING (*mysql_get_query_t)(void* thd);
typedef MYSQL_LEX_STRING (*mysql_get_normalized_query_t)(void* thd);
extern struct mysql_parser_service_st {
  mysql_current_session_t mysql_current_session;
  mysql_open_session_t mysql_open_session;
  mysql_start_thread_t mysql_start_thread;
  mysql_join_thread_t mysql_join_thread;
  mysql_set_current_database_t mysql_set_current_database;
  mysql_parse_t mysql_parse;
  mysql_get_statement_type_t mysql_get_statement_type;
  mysql_get_statement_digest_t mysql_get_statement_digest;
  mysql_get_number_params_t mysql_get_number_params;
  mysql_extract_prepared_params_t mysql_extract_prepared_params;
  mysql_visit_tree_t mysql_visit_tree;
  mysql_item_string_t mysql_item_string;
  mysql_free_string_t mysql_free_string;
  mysql_get_query_t mysql_get_query;
  mysql_get_normalized_query_t mysql_get_normalized_query;
} *mysql_parser_service;
typedef void *(*callback_function)(void*);
void* mysql_parser_current_session();
void* mysql_parser_open_session();
void mysql_parser_start_thread(void* thd, callback_function fun, void *arg,
                               struct st_my_thread_handle *thread_handle);
void mysql_parser_join_thread(struct st_my_thread_handle *thread_handle);
void mysql_parser_set_current_database(void* thd,
                                       const MYSQL_LEX_STRING db);
int mysql_parser_parse(void* thd, const MYSQL_LEX_STRING query,
                       unsigned char is_prepared,
                       sql_condition_handler_function handle_condition,
                       void *condition_handler_state);
int mysql_parser_get_statement_type(void* thd);
int mysql_parser_get_statement_digest(void* thd, unsigned char *digest);
int mysql_parser_get_number_params(void* thd);
int mysql_parser_extract_prepared_params(void* thd, int *positions);
int mysql_parser_visit_tree(void* thd, parse_node_visit_function processor,
                            unsigned char* arg);
MYSQL_LEX_STRING mysql_parser_item_string(MYSQL_ITEM item);
void mysql_parser_free_string(MYSQL_LEX_STRING string);
MYSQL_LEX_STRING mysql_parser_get_query(void* thd);
MYSQL_LEX_STRING mysql_parser_get_normalized_query(void* thd);
#include <mysql/service_rpl_transaction_ctx.h>
struct st_transaction_termination_ctx
{
  unsigned long m_thread_id;
  unsigned int m_flags;
  bool m_rollback_transaction;
  bool m_generated_gtid;
  int m_sidno;
  long long int m_gno;
};
typedef struct st_transaction_termination_ctx Transaction_termination_ctx;
extern struct rpl_transaction_ctx_service_st {
  int (*set_transaction_ctx)(Transaction_termination_ctx transaction_termination_ctx);
} *rpl_transaction_ctx_service;
int set_transaction_ctx(Transaction_termination_ctx transaction_termination_ctx);
#include <mysql/service_rpl_transaction_write_set.h>
struct st_trans_write_set
{
  unsigned int m_flags;
  unsigned long write_set_size;
  unsigned long long* write_set;
};
typedef struct st_trans_write_set Transaction_write_set;
extern struct transaction_write_set_service_st {
  Transaction_write_set* (*get_transaction_write_set)(unsigned long m_thread_id);
} *transaction_write_set_service;
Transaction_write_set* get_transaction_write_set(unsigned long m_thread_id);
#include <mysql/service_security_context.h>
#include "mysql/plugin.h"
#include "status_var.h"
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL,
  SHOW_INT,
  SHOW_LONG,
  SHOW_LONGLONG,
  SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_DOUBLE,
  SHOW_KEY_CACHE_LONG,
  SHOW_KEY_CACHE_LONGLONG,
  SHOW_LONG_STATUS,
  SHOW_DOUBLE_STATUS,
  SHOW_HAVE,
  SHOW_MY_BOOL,
  SHOW_HA_ROWS,
  SHOW_SYS,
  SHOW_LONG_NOFLUSH,
  SHOW_LONGLONG_STATUS,
  SHOW_LEX_STRING,
  SHOW_SIGNED_LONG
};
enum enum_mysql_show_scope
{
  SHOW_SCOPE_UNDEF,
  SHOW_SCOPE_GLOBAL,
  SHOW_SCOPE_SESSION,
  SHOW_SCOPE_ALL
};
struct st_mysql_show_var
{
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
  enum enum_mysql_show_scope scope;
};
typedef int (*mysql_show_var_func)(void*, struct st_mysql_show_var*, char *);
typedef void * MYSQL_PLUGIN;
struct st_mysql_xid {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
};
typedef struct st_mysql_xid MYSQL_XID;
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
  int (*check_uninstall)(MYSQL_PLUGIN);
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
void *thd_get_ha_data(const void* thd, const struct handlerton *hton);
void thd_set_ha_data(void* thd, const struct handlerton *hton,
                     const void *ha_data);
typedef char my_svc_bool;
extern struct security_context_service_st {
  my_svc_bool (*thd_get_security_context)(void*, void* *out_ctx);
  my_svc_bool (*thd_set_security_context)(void*, void* in_ctx);
  my_svc_bool (*security_context_create)(void* *out_ctx);
  my_svc_bool (*security_context_destroy)(void*);
  my_svc_bool (*security_context_copy)(void* in_ctx, void* *out_ctx);
  my_svc_bool (*security_context_lookup)(void* ctx,
                                         const char *user, const char *host,
                                         const char *ip, const char *db);
  my_svc_bool (*security_context_get_option)(void*, const char *name, void *inout_pvalue);
  my_svc_bool (*security_context_set_option)(void*, const char *name, void *pvalue);
} *security_context_service;
  my_svc_bool thd_get_security_context(void*, void* *out_ctx);
  my_svc_bool thd_set_security_context(void*, void* in_ctx);
  my_svc_bool security_context_create(void* *out_ctx);
  my_svc_bool security_context_destroy(void* ctx);
  my_svc_bool security_context_copy(void* in_ctx, void* *out_ctx);
  my_svc_bool security_context_lookup(void* ctx,
                                  const char *user, const char *host,
                                  const char *ip, const char *db);
  my_svc_bool security_context_get_option(void*, const char *name, void *inout_pvalue);
  my_svc_bool security_context_set_option(void*, const char *name, void *pvalue);
#include <mysql/service_locking.h>
enum enum_locking_service_lock_type
{ LOCKING_SERVICE_READ, LOCKING_SERVICE_WRITE };
typedef int (*mysql_acquire_locks_t)(void* opaque_thd,
                                     const char* lock_namespace,
                                     const char**lock_names,
                                     size_t lock_num,
                                     enum enum_locking_service_lock_type lock_type,
                                     unsigned long lock_timeout);
typedef int (*mysql_release_locks_t)(void* opaque_thd,
                                     const char* lock_namespace);
extern struct mysql_locking_service_st {
  mysql_acquire_locks_t mysql_acquire_locks;
  mysql_release_locks_t mysql_release_locks;
} *mysql_locking_service;
int mysql_acquire_locking_service_locks(void* opaque_thd,
                                        const char* lock_namespace,
                                        const char**lock_names,
                                        size_t lock_num,
                                        enum enum_locking_service_lock_type lock_type,
                                        unsigned long lock_timeout);
int mysql_release_locking_service_locks(void* opaque_thd,
                                        const char* lock_namespace);
#include <mysql/service_mysql_keyring.h>
extern struct mysql_keyring_service_st
{
  int (*my_key_store_func)(const char *, const char *, const char *,
                           const void *, size_t);
  int (*my_key_fetch_func)(const char *, char **, const char *, void **,
                           size_t *);
  int (*my_key_remove_func)(const char *, const char *);
  int (*my_key_generate_func)(const char *, const char *, const char *,
                              size_t);
} *mysql_keyring_service;
int my_key_store(const char *, const char *, const char *, const void *, size_t);
int my_key_fetch(const char *, char **, const char *, void **,
                 size_t *);
int my_key_remove(const char *, const char *);
int my_key_generate(const char *, const char *, const char *, size_t);
#include <mysql/service_plugin_registry.h>
#include <mysql/components/services/registry.h>
#include <mysql/components/service.h>
typedef int mysql_service_status_t;
#include <stdint.h>
