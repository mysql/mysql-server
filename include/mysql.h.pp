typedef char my_bool;
typedef int my_socket;
#include "mysql_version.h"
#include "mysql_com.h"
#include "binary_log_types.h"
typedef enum enum_field_types {
  MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
  MYSQL_TYPE_SHORT, MYSQL_TYPE_LONG,
  MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE,
  MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP,
  MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
  MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
  MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
  MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
  MYSQL_TYPE_BIT,
  MYSQL_TYPE_TIMESTAMP2,
  MYSQL_TYPE_DATETIME2,
  MYSQL_TYPE_TIME2,
  MYSQL_TYPE_JSON=245,
  MYSQL_TYPE_NEWDECIMAL=246,
  MYSQL_TYPE_ENUM=247,
  MYSQL_TYPE_SET=248,
  MYSQL_TYPE_TINY_BLOB=249,
  MYSQL_TYPE_MEDIUM_BLOB=250,
  MYSQL_TYPE_LONG_BLOB=251,
  MYSQL_TYPE_BLOB=252,
  MYSQL_TYPE_VAR_STRING=253,
  MYSQL_TYPE_STRING=254,
  MYSQL_TYPE_GEOMETRY=255
} enum_field_types;
#include "my_command.h"
enum enum_server_command
{
  COM_SLEEP,
  COM_QUIT,
  COM_INIT_DB,
  COM_QUERY,
  COM_FIELD_LIST,
  COM_CREATE_DB,
  COM_DROP_DB,
  COM_REFRESH,
  COM_DEPRECATED_1,
  COM_STATISTICS,
  COM_PROCESS_INFO,
  COM_CONNECT,
  COM_PROCESS_KILL,
  COM_DEBUG,
  COM_PING,
  COM_TIME,
  COM_DELAYED_INSERT,
  COM_CHANGE_USER,
  COM_BINLOG_DUMP,
  COM_TABLE_DUMP,
  COM_CONNECT_OUT,
  COM_REGISTER_SLAVE,
  COM_STMT_PREPARE,
  COM_STMT_EXECUTE,
  COM_STMT_SEND_LONG_DATA,
  COM_STMT_CLOSE,
  COM_STMT_RESET,
  COM_SET_OPTION,
  COM_STMT_FETCH,
  COM_DAEMON,
  COM_BINLOG_DUMP_GTID,
  COM_RESET_CONNECTION,
  COM_END
};
struct st_vio;
typedef struct st_vio Vio;
typedef struct st_net {
  Vio *vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  my_socket fd;
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout, read_timeout, retry_count;
  int fcntl;
  unsigned int *return_status;
  unsigned char reading_or_writing;
  char save_char;
  my_bool unused1;
  my_bool unused2;
  my_bool compress;
  my_bool unused3;
  unsigned char *unused;
  unsigned int last_errno;
  unsigned char error;
  my_bool unused4;
  my_bool unused5;
  char last_error[512];
  char sqlstate[5 +1];
  void *extension;
} NET;
enum mysql_enum_shutdown_level {
  SHUTDOWN_DEFAULT = 0,
  SHUTDOWN_WAIT_CONNECTIONS= (unsigned char)(1 << 0),
  SHUTDOWN_WAIT_TRANSACTIONS= (unsigned char)(1 << 1),
  SHUTDOWN_WAIT_UPDATES= (unsigned char)(1 << 3),
  SHUTDOWN_WAIT_ALL_BUFFERS= ((unsigned char)(1 << 3) << 1),
  SHUTDOWN_WAIT_CRITICAL_BUFFERS= ((unsigned char)(1 << 3) << 1) + 1,
  KILL_QUERY= 254,
  KILL_CONNECTION= 255
};
enum enum_cursor_type
{
  CURSOR_TYPE_NO_CURSOR= 0,
  CURSOR_TYPE_READ_ONLY= 1,
  CURSOR_TYPE_FOR_UPDATE= 2,
  CURSOR_TYPE_SCROLLABLE= 4
};
enum enum_mysql_set_option
{
  MYSQL_OPTION_MULTI_STATEMENTS_ON,
  MYSQL_OPTION_MULTI_STATEMENTS_OFF
};
enum enum_session_state_type
{
  SESSION_TRACK_SYSTEM_VARIABLES,
  SESSION_TRACK_SCHEMA,
  SESSION_TRACK_STATE_CHANGE,
  SESSION_TRACK_GTIDS,
  SESSION_TRACK_TRANSACTION_CHARACTERISTICS,
  SESSION_TRACK_TRANSACTION_STATE
};
my_bool my_net_init(NET *net, Vio* vio);
void my_net_local_init(NET *net);
void net_end(NET *net);
void net_clear(NET *net, my_bool check_buffer);
void net_claim_memory_ownership(NET *net);
my_bool net_realloc(NET *net, size_t length);
my_bool net_flush(NET *net);
my_bool my_net_write(NET *net,const unsigned char *packet, size_t len);
my_bool net_write_command(NET *net,unsigned char command,
     const unsigned char *header, size_t head_len,
     const unsigned char *packet, size_t len);
my_bool net_write_packet(NET *net, const unsigned char *packet, size_t length);
unsigned long my_net_read(NET *net);
struct rand_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};
enum Item_result {INVALID_RESULT=-1,
                  STRING_RESULT=0, REAL_RESULT, INT_RESULT, ROW_RESULT,
                  DECIMAL_RESULT};
typedef struct st_udf_args
{
  unsigned int arg_count;
  enum Item_result *arg_type;
  char **args;
  unsigned long *lengths;
  char *maybe_null;
  char **attributes;
  unsigned long *attribute_lengths;
  void *extension;
} UDF_ARGS;
typedef struct st_udf_init
{
  my_bool maybe_null;
  unsigned int decimals;
  unsigned long max_length;
  char *ptr;
  my_bool const_item;
  void *extension;
} UDF_INIT;
void randominit(struct rand_struct *, unsigned long seed1,
                unsigned long seed2);
double my_rnd(struct rand_struct *);
void create_random_string(char *to, unsigned int length, struct rand_struct *rand_st);
void hash_password(unsigned long *to, const char *password, unsigned int password_len);
void make_scrambled_password_323(char *to, const char *password);
void scramble_323(char *to, const char *message, const char *password);
my_bool check_scramble_323(const unsigned char *reply, const char *message,
                           unsigned long *salt);
void get_salt_from_password_323(unsigned long *res, const char *password);
void make_password_from_salt_323(char *to, const unsigned long *salt);
void make_scrambled_password(char *to, const char *password);
void scramble(char *to, const char *message, const char *password);
my_bool check_scramble(const unsigned char *reply, const char *message,
                       const unsigned char *hash_stage2);
void get_salt_from_password(unsigned char *res, const char *password);
void make_password_from_salt(char *to, const unsigned char *hash_stage2);
char *octet2hex(char *to, const char *str, unsigned int len);
char *get_tty_password(const char *opt_message);
const char *mysql_errno_to_sqlstate(unsigned int mysql_errno);
my_bool my_thread_init(void);
void my_thread_end(void);
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
#include "my_list.h"
typedef struct st_list {
  struct st_list *prev,*next;
  void *data;
} LIST;
typedef int (*list_walk_action)(void *,void *);
extern LIST *list_add(LIST *root,LIST *element);
extern LIST *list_delete(LIST *root,LIST *element);
extern LIST *list_cons(void *data,LIST *root);
extern LIST *list_reverse(LIST *root);
extern void list_free(LIST *root,unsigned int free_data);
extern unsigned int list_length(LIST *);
extern int list_walk(LIST *,list_walk_action action,unsigned char * argument);
#include "mysql/client_plugin.h"
struct st_mysql_client_plugin
{
  int type; unsigned int interface_version; const char *name; const char *author; const char *desc; unsigned int version[3]; const char *license; void *mysql_api; int (*init)(char *, size_t, int, va_list); int (*deinit)(void); int (*options)(const char *option, const void *);
};
struct st_mysql;
#include <mysql/plugin_auth_common.h>
typedef struct st_plugin_vio_info
{
  enum { MYSQL_VIO_INVALID, MYSQL_VIO_TCP, MYSQL_VIO_SOCKET,
         MYSQL_VIO_PIPE, MYSQL_VIO_MEMORY } protocol;
  int socket;
} MYSQL_PLUGIN_VIO_INFO;
typedef struct st_plugin_vio
{
  int (*read_packet)(struct st_plugin_vio *vio,
                     unsigned char **buf);
  int (*write_packet)(struct st_plugin_vio *vio,
                      const unsigned char *packet,
                      int packet_len);
  void (*info)(struct st_plugin_vio *vio, struct st_plugin_vio_info *info);
} MYSQL_PLUGIN_VIO;
struct st_mysql_client_plugin_AUTHENTICATION
{
  int type; unsigned int interface_version; const char *name; const char *author; const char *desc; unsigned int version[3]; const char *license; void *mysql_api; int (*init)(char *, size_t, int, va_list); int (*deinit)(void); int (*options)(const char *option, const void *);
  int (*authenticate_user)(MYSQL_PLUGIN_VIO *vio, struct st_mysql *mysql);
};
struct st_mysql_client_plugin *
mysql_load_plugin(struct st_mysql *mysql, const char *name, int type,
                  int argc, ...);
struct st_mysql_client_plugin *
mysql_load_plugin_v(struct st_mysql *mysql, const char *name, int type,
                    int argc, va_list args);
struct st_mysql_client_plugin *
mysql_client_find_plugin(struct st_mysql *mysql, const char *name, int type);
struct st_mysql_client_plugin *
mysql_client_register_plugin(struct st_mysql *mysql,
                             struct st_mysql_client_plugin *plugin);
int mysql_plugin_options(struct st_mysql_client_plugin *plugin,
                         const char *option, const void *value);
#include "typelib.h"
#include "my_alloc.h"
#include <mysql/psi/psi_memory.h>
#include "my_global.h"
#include "my_config.h"
#include <stdio.h>
