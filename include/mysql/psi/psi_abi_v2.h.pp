#include "mysql/psi/psi.h"
C_MODE_START
struct TABLE_SHARE;
struct OPAQUE_LEX_YYSTYPE;
struct PSI_mutex;
typedef struct PSI_mutex PSI_mutex;
struct PSI_rwlock;
typedef struct PSI_rwlock PSI_rwlock;
struct PSI_cond;
typedef struct PSI_cond PSI_cond;
struct PSI_table_share;
typedef struct PSI_table_share PSI_table_share;
struct PSI_table;
typedef struct PSI_table PSI_table;
struct PSI_thread;
typedef struct PSI_thread PSI_thread;
struct PSI_file;
typedef struct PSI_file PSI_file;
struct PSI_socket;
typedef struct PSI_socket PSI_socket;
struct PSI_table_locker;
typedef struct PSI_table_locker PSI_table_locker;
struct PSI_statement_locker;
typedef struct PSI_statement_locker PSI_statement_locker;
struct PSI_idle_locker;
typedef struct PSI_idle_locker PSI_idle_locker;
struct PSI_digest_locker;
typedef struct PSI_digest_locker PSI_digest_locker;
struct PSI_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_bootstrap PSI_bootstrap;
struct PSI_mutex_locker;
typedef struct PSI_mutex_locker PSI_mutex_locker;
struct PSI_rwlock_locker;
typedef struct PSI_rwlock_locker PSI_rwlock_locker;
struct PSI_cond_locker;
typedef struct PSI_cond_locker PSI_cond_locker;
struct PSI_file_locker;
typedef struct PSI_file_locker PSI_file_locker;
struct PSI_socket_locker;
typedef struct PSI_socket_locker PSI_socket_locker;
enum PSI_mutex_operation
{
  PSI_MUTEX_LOCK= 0,
  PSI_MUTEX_TRYLOCK= 1
};
typedef enum PSI_mutex_operation PSI_mutex_operation;
enum PSI_rwlock_operation
{
  PSI_RWLOCK_READLOCK= 0,
  PSI_RWLOCK_WRITELOCK= 1,
  PSI_RWLOCK_TRYREADLOCK= 2,
  PSI_RWLOCK_TRYWRITELOCK= 3
};
typedef enum PSI_rwlock_operation PSI_rwlock_operation;
enum PSI_cond_operation
{
  PSI_COND_WAIT= 0,
  PSI_COND_TIMEDWAIT= 1
};
typedef enum PSI_cond_operation PSI_cond_operation;
enum PSI_file_operation
{
  PSI_FILE_CREATE= 0,
  PSI_FILE_CREATE_TMP= 1,
  PSI_FILE_OPEN= 2,
  PSI_FILE_STREAM_OPEN= 3,
  PSI_FILE_CLOSE= 4,
  PSI_FILE_STREAM_CLOSE= 5,
  PSI_FILE_READ= 6,
  PSI_FILE_WRITE= 7,
  PSI_FILE_SEEK= 8,
  PSI_FILE_TELL= 9,
  PSI_FILE_FLUSH= 10,
  PSI_FILE_STAT= 11,
  PSI_FILE_FSTAT= 12,
  PSI_FILE_CHSIZE= 13,
  PSI_FILE_DELETE= 14,
  PSI_FILE_RENAME= 15,
  PSI_FILE_SYNC= 16
};
typedef enum PSI_file_operation PSI_file_operation;
enum PSI_table_io_operation
{
  PSI_TABLE_FETCH_ROW= 0,
  PSI_TABLE_WRITE_ROW= 1,
  PSI_TABLE_UPDATE_ROW= 2,
  PSI_TABLE_DELETE_ROW= 3
};
typedef enum PSI_table_io_operation PSI_table_io_operation;
enum PSI_table_lock_operation
{
  PSI_TABLE_LOCK= 0,
  PSI_TABLE_EXTERNAL_LOCK= 1
};
typedef enum PSI_table_lock_operation PSI_table_lock_operation;
enum PSI_socket_state
{
  PSI_SOCKET_STATE_IDLE= 1,
  PSI_SOCKET_STATE_ACTIVE= 2
};
typedef enum PSI_socket_state PSI_socket_state;
enum PSI_socket_operation
{
  PSI_SOCKET_CREATE= 0,
  PSI_SOCKET_CONNECT= 1,
  PSI_SOCKET_BIND= 2,
  PSI_SOCKET_CLOSE= 3,
  PSI_SOCKET_SEND= 4,
  PSI_SOCKET_RECV= 5,
  PSI_SOCKET_SENDTO= 6,
  PSI_SOCKET_RECVFROM= 7,
  PSI_SOCKET_SENDMSG= 8,
  PSI_SOCKET_RECVMSG= 9,
  PSI_SOCKET_SEEK= 10,
  PSI_SOCKET_OPT= 11,
  PSI_SOCKET_STAT= 12,
  PSI_SOCKET_SHUTDOWN= 13,
  PSI_SOCKET_SELECT= 14
};
typedef enum PSI_socket_operation PSI_socket_operation;
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
typedef unsigned int PSI_stage_key;
typedef unsigned int PSI_statement_key;
typedef unsigned int PSI_socket_key;
struct PSI_v2
{
  int placeholder;
};
struct PSI_mutex_info_v2
{
  int placeholder;
};
struct PSI_rwlock_info_v2
{
  int placeholder;
};
struct PSI_cond_info_v2
{
  int placeholder;
};
struct PSI_thread_info_v2
{
  int placeholder;
};
struct PSI_file_info_v2
{
  int placeholder;
};
struct PSI_stage_info_v2
{
  int placeholder;
};
struct PSI_statement_info_v2
{
  int placeholder;
};
struct PSI_idle_locker_state_v2
{
  int placeholder;
};
struct PSI_mutex_locker_state_v2
{
  int placeholder;
};
struct PSI_rwlock_locker_state_v2
{
  int placeholder;
};
struct PSI_cond_locker_state_v2
{
  int placeholder;
};
struct PSI_file_locker_state_v2
{
  int placeholder;
};
struct PSI_table_locker_state_v2
{
  int placeholder;
};
struct PSI_statement_locker_state_v2
{
  int placeholder;
};
struct PSI_socket_locker_state_v2
{
  int placeholder;
};
typedef struct PSI_v2 PSI;
typedef struct PSI_mutex_info_v2 PSI_mutex_info;
typedef struct PSI_rwlock_info_v2 PSI_rwlock_info;
typedef struct PSI_cond_info_v2 PSI_cond_info;
typedef struct PSI_thread_info_v2 PSI_thread_info;
typedef struct PSI_file_info_v2 PSI_file_info;
typedef struct PSI_stage_info_v2 PSI_stage_info;
typedef struct PSI_statement_info_v2 PSI_statement_info;
typedef struct PSI_socket_info_v2 PSI_socket_info;
typedef struct PSI_idle_locker_state_v2 PSI_idle_locker_state;
typedef struct PSI_mutex_locker_state_v2 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v2 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v2 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v2 PSI_file_locker_state;
typedef struct PSI_table_locker_state_v2 PSI_table_locker_state;
typedef struct PSI_statement_locker_state_v2 PSI_statement_locker_state;
typedef struct PSI_socket_locker_state_v2 PSI_socket_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
