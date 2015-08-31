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
MYSQL_SESSION srv_session_open(srv_session_error_cb cb, void *plugix_ctx);
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
#include "my_global.h"
#include "my_config.h"
#include <stdio.h>
