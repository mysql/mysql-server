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
} *srv_session_service;
int srv_session_init_thread(const void *plugin);
void srv_session_deinit_thread();
MYSQL_SESSION srv_session_open(srv_session_error_cb error_cb, void *plugin_ctx);
int srv_session_detach(MYSQL_SESSION session);
int srv_session_close(MYSQL_SESSION session);
int srv_session_server_is_available();
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
  my_bool neg;
  enum enum_mysql_timestamp_type time_type;
} MYSQL_TIME;
#include "decimal.h"
C_MODE_START
typedef enum
{TRUNCATE=0, HALF_EVEN, HALF_UP, CEILING, FLOOR}
  decimal_round_mode;
typedef int32 decimal_digit_t;
typedef struct st_decimal_t {
  int intg, frac, len;
  my_bool sign;
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
  size_t (*my_vsnprintf_type)(char *, size_t, const char*, va_list);
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
    __attribute__((format(printf, 3, 4)));
} *my_plugin_log_service;
int my_plugin_log_message(MYSQL_PLUGIN *plugin, enum plugin_log_level level,
                          const char *format, ...)
  __attribute__((format(printf, 3, 4)));
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
#include "mysql/psi/psi_memory.h"
#include "my_global.h"
#include "my_config.h"
#include <stdio.h>
