#include "mysql/psi/psi.h"
C_MODE_START
struct TABLE_SHARE;
struct PSI_mutex;
struct PSI_rwlock;
struct PSI_cond;
struct PSI_table_share;
struct PSI_table;
struct PSI_thread;
struct PSI_file;
struct PSI_table_locker;
struct PSI_bootstrap
{
  void* (*get_interface)(int version);
};
struct PSI_mutex_locker;
struct PSI_rwlock_locker;
struct PSI_cond_locker;
struct PSI_file_locker;
enum PSI_mutex_operation
{
  PSI_MUTEX_LOCK= 0,
  PSI_MUTEX_TRYLOCK= 1
};
enum PSI_rwlock_operation
{
  PSI_RWLOCK_READLOCK= 0,
  PSI_RWLOCK_WRITELOCK= 1,
  PSI_RWLOCK_TRYREADLOCK= 2,
  PSI_RWLOCK_TRYWRITELOCK= 3
};
enum PSI_cond_operation
{
  PSI_COND_WAIT= 0,
  PSI_COND_TIMEDWAIT= 1
};
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
enum PSI_table_io_operation
{
  PSI_TABLE_FETCH_ROW= 0,
  PSI_TABLE_WRITE_ROW= 1,
  PSI_TABLE_UPDATE_ROW= 2,
  PSI_TABLE_DELETE_ROW= 3
};
enum PSI_table_lock_operation
{
  PSI_TABLE_LOCK= 0,
  PSI_TABLE_EXTERNAL_LOCK= 1
};
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
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
typedef struct PSI_v2 PSI;
typedef struct PSI_mutex_info_v2 PSI_mutex_info;
typedef struct PSI_rwlock_info_v2 PSI_rwlock_info;
typedef struct PSI_cond_info_v2 PSI_cond_info;
typedef struct PSI_thread_info_v2 PSI_thread_info;
typedef struct PSI_file_info_v2 PSI_file_info;
typedef struct PSI_mutex_locker_state_v2 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v2 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v2 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v2 PSI_file_locker_state;
typedef struct PSI_table_locker_state_v2 PSI_table_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
