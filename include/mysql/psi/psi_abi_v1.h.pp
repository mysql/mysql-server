#include "mysql/psi/psi.h"
#include "psi_base.h"
#include "psi_memory.h"
#include "psi_base.h"
struct PSI_thread;
typedef unsigned int PSI_memory_key;
struct PSI_memory_info_v1
{
  PSI_memory_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_memory_info_v1 PSI_memory_info_v1;
typedef void (*register_memory_v1_t)
  (const char *category, struct PSI_memory_info_v1 *info, int count);
typedef PSI_memory_key (*memory_alloc_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread ** owner);
typedef PSI_memory_key (*memory_realloc_v1_t)
  (PSI_memory_key key, size_t old_size, size_t new_size, struct PSI_thread ** owner);
typedef PSI_memory_key (*memory_claim_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread ** owner);
typedef void (*memory_free_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread * owner);
typedef struct PSI_memory_info_v1 PSI_memory_info;
C_MODE_START
struct MDL_key;
typedef struct MDL_key MDL_key;
typedef int opaque_mdl_type;
typedef int opaque_mdl_duration;
typedef int opaque_mdl_status;
typedef int opaque_vio_type;
struct TABLE_SHARE;
struct sql_digest_storage;
  struct opaque_THD
  {
    int dummy;
  };
  typedef struct opaque_THD THD;
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
struct PSI_prepared_stmt;
typedef struct PSI_prepared_stmt PSI_prepared_stmt;
struct PSI_table_locker;
typedef struct PSI_table_locker PSI_table_locker;
struct PSI_statement_locker;
typedef struct PSI_statement_locker PSI_statement_locker;
struct PSI_transaction_locker;
typedef struct PSI_transaction_locker PSI_transaction_locker;
struct PSI_idle_locker;
typedef struct PSI_idle_locker PSI_idle_locker;
struct PSI_digest_locker;
typedef struct PSI_digest_locker PSI_digest_locker;
struct PSI_sp_share;
typedef struct PSI_sp_share PSI_sp_share;
struct PSI_sp_locker;
typedef struct PSI_sp_locker PSI_sp_locker;
struct PSI_metadata_lock;
typedef struct PSI_metadata_lock PSI_metadata_lock;
struct PSI_stage_progress
{
  ulonglong m_work_completed;
  ulonglong m_work_estimated;
};
typedef struct PSI_stage_progress PSI_stage_progress;
enum PSI_table_io_operation
{
  PSI_TABLE_FETCH_ROW= 0,
  PSI_TABLE_WRITE_ROW= 1,
  PSI_TABLE_UPDATE_ROW= 2,
  PSI_TABLE_DELETE_ROW= 3
};
typedef enum PSI_table_io_operation PSI_table_io_operation;
struct PSI_table_locker_state
{
  uint m_flags;
  enum PSI_table_io_operation m_io_operation;
  struct PSI_table *m_table;
  struct PSI_table_share *m_table_share;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
  uint m_index;
};
typedef struct PSI_table_locker_state PSI_table_locker_state;
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
struct PSI_metadata_locker;
typedef struct PSI_metadata_locker PSI_metadata_locker;
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
  PSI_RWLOCK_TRYWRITELOCK= 3,
  PSI_RWLOCK_SHAREDLOCK= 4,
  PSI_RWLOCK_SHAREDEXCLUSIVELOCK= 5,
  PSI_RWLOCK_EXCLUSIVELOCK= 6,
  PSI_RWLOCK_TRYSHAREDLOCK= 7,
  PSI_RWLOCK_TRYSHAREDEXCLUSIVELOCK= 8,
  PSI_RWLOCK_TRYEXCLUSIVELOCK= 9
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
struct PSI_mutex_info_v1
{
  PSI_mutex_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_mutex_info_v1 PSI_mutex_info_v1;
struct PSI_rwlock_info_v1
{
  PSI_rwlock_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info_v1;
struct PSI_cond_info_v1
{
  PSI_cond_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_cond_info_v1 PSI_cond_info_v1;
struct PSI_thread_info_v1
{
  PSI_thread_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_thread_info_v1 PSI_thread_info_v1;
struct PSI_file_info_v1
{
  PSI_file_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_file_info_v1 PSI_file_info_v1;
struct PSI_stage_info_v1
{
  PSI_stage_key m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_stage_info_v1 PSI_stage_info_v1;
struct PSI_statement_info_v1
{
  PSI_statement_key m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_statement_info_v1 PSI_statement_info_v1;
struct PSI_socket_info_v1
{
  PSI_socket_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_socket_info_v1 PSI_socket_info_v1;
struct PSI_idle_locker_state_v1
{
  uint m_flags;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state_v1;
struct PSI_mutex_locker_state_v1
{
  uint m_flags;
  enum PSI_mutex_operation m_operation;
  struct PSI_mutex *m_mutex;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state_v1;
struct PSI_rwlock_locker_state_v1
{
  uint m_flags;
  enum PSI_rwlock_operation m_operation;
  struct PSI_rwlock *m_rwlock;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state_v1;
struct PSI_cond_locker_state_v1
{
  uint m_flags;
  enum PSI_cond_operation m_operation;
  struct PSI_cond *m_cond;
  struct PSI_mutex *m_mutex;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state_v1;
struct PSI_file_locker_state_v1
{
  uint m_flags;
  enum PSI_file_operation m_operation;
  struct PSI_file *m_file;
  const char *m_name;
  void *m_class;
  struct PSI_thread *m_thread;
  size_t m_number_of_bytes;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state_v1;
struct PSI_metadata_locker_state_v1
{
  uint m_flags;
  struct PSI_metadata_lock *m_metadata_lock;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state_v1;
struct PSI_statement_locker_state_v1
{
  my_bool m_discarded;
  my_bool m_in_prepare;
  uchar m_no_index_used;
  uchar m_no_good_index_used;
  uint m_flags;
  void *m_class;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_statement;
  ulonglong m_lock_time;
  ulonglong m_rows_sent;
  ulonglong m_rows_examined;
  ulong m_created_tmp_disk_tables;
  ulong m_created_tmp_tables;
  ulong m_select_full_join;
  ulong m_select_full_range_join;
  ulong m_select_range;
  ulong m_select_range_check;
  ulong m_select_scan;
  ulong m_sort_merge_passes;
  ulong m_sort_range;
  ulong m_sort_rows;
  ulong m_sort_scan;
  const struct sql_digest_storage *m_digest;
  char m_schema_name[(64 * 3)];
  uint m_schema_name_length;
  uint m_cs_number;
  PSI_sp_share *m_parent_sp_share;
  PSI_prepared_stmt *m_parent_prepared_stmt;
};
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state_v1;
struct PSI_transaction_locker_state_v1
{
  uint m_flags;
  void *m_class;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_transaction;
  my_bool m_read_only;
  my_bool m_autocommit;
  ulong m_statement_count;
  ulong m_savepoint_count;
  ulong m_rollback_to_savepoint_count;
  ulong m_release_savepoint_count;
};
typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state_v1;
struct PSI_socket_locker_state_v1
{
  uint m_flags;
  struct PSI_socket *m_socket;
  struct PSI_thread *m_thread;
  size_t m_number_of_bytes;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_socket_operation m_operation;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state_v1;
struct PSI_sp_locker_state_v1
{
  uint m_flags;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  PSI_sp_share* m_sp_share;
};
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state_v1;
typedef void (*register_mutex_v1_t)
  (const char *category, struct PSI_mutex_info_v1 *info, int count);
typedef void (*register_rwlock_v1_t)
  (const char *category, struct PSI_rwlock_info_v1 *info, int count);
typedef void (*register_cond_v1_t)
  (const char *category, struct PSI_cond_info_v1 *info, int count);
typedef void (*register_thread_v1_t)
  (const char *category, struct PSI_thread_info_v1 *info, int count);
typedef void (*register_file_v1_t)
  (const char *category, struct PSI_file_info_v1 *info, int count);
typedef void (*register_stage_v1_t)
  (const char *category, struct PSI_stage_info_v1 **info, int count);
typedef void (*register_statement_v1_t)
  (const char *category, struct PSI_statement_info_v1 *info, int count);
typedef void (*register_socket_v1_t)
  (const char *category, struct PSI_socket_info_v1 *info, int count);
typedef struct PSI_mutex* (*init_mutex_v1_t)
  (PSI_mutex_key key, const void *identity);
typedef void (*destroy_mutex_v1_t)(struct PSI_mutex *mutex);
typedef struct PSI_rwlock* (*init_rwlock_v1_t)
  (PSI_rwlock_key key, const void *identity);
typedef void (*destroy_rwlock_v1_t)(struct PSI_rwlock *rwlock);
typedef struct PSI_cond* (*init_cond_v1_t)
  (PSI_cond_key key, const void *identity);
typedef void (*destroy_cond_v1_t)(struct PSI_cond *cond);
typedef struct PSI_socket* (*init_socket_v1_t)
  (PSI_socket_key key, const my_socket *fd,
  const struct sockaddr *addr, socklen_t addr_len);
typedef void (*destroy_socket_v1_t)(struct PSI_socket *socket);
typedef struct PSI_table_share* (*get_table_share_v1_t)
  (my_bool temporary, struct TABLE_SHARE *share);
typedef void (*release_table_share_v1_t)(struct PSI_table_share *share);
typedef void (*drop_table_share_v1_t)
  (my_bool temporary, const char *schema_name, int schema_name_length,
   const char *table_name, int table_name_length);
typedef struct PSI_table* (*open_table_v1_t)
  (struct PSI_table_share *share, const void *identity);
typedef void (*unbind_table_v1_t)
  (struct PSI_table *table);
typedef PSI_table* (*rebind_table_v1_t)
  (PSI_table_share *share, const void *identity, PSI_table *table);
typedef void (*close_table_v1_t)(struct TABLE_SHARE *server_share,
                                 struct PSI_table *table);
typedef void (*create_file_v1_t)(PSI_file_key key, const char *name,
                                 File file);
typedef int (*spawn_thread_v1_t)(PSI_thread_key key,
                                 my_thread_handle *thread,
                                 const my_thread_attr_t *attr,
                                 void *(*start_routine)(void*), void *arg);
typedef struct PSI_thread* (*new_thread_v1_t)
  (PSI_thread_key key, const void *identity, ulonglong thread_id);
typedef void (*set_thread_THD_v1_t)(struct PSI_thread *thread,
                                    THD *thd);
typedef void (*set_thread_id_v1_t)(struct PSI_thread *thread,
                                   ulonglong id);
typedef void (*set_thread_os_id_v1_t)(struct PSI_thread *thread);
typedef struct PSI_thread* (*get_thread_v1_t)(void);
typedef void (*set_thread_user_v1_t)(const char *user, int user_len);
typedef void (*set_thread_account_v1_t)(const char *user, int user_len,
                                        const char *host, int host_len);
typedef void (*set_thread_db_v1_t)(const char* db, int db_len);
typedef void (*set_thread_command_v1_t)(int command);
typedef void (*set_connection_type_v1_t)(opaque_vio_type conn_type);
typedef void (*set_thread_start_time_v1_t)(time_t start_time);
typedef void (*set_thread_state_v1_t)(const char* state);
typedef void (*set_thread_info_v1_t)(const char* info, uint info_len);
typedef void (*set_thread_v1_t)(struct PSI_thread *thread);
typedef void (*delete_current_thread_v1_t)(void);
typedef void (*delete_thread_v1_t)(struct PSI_thread *thread);
typedef struct PSI_file_locker* (*get_thread_file_name_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   PSI_file_key key, enum PSI_file_operation op, const char *name,
   const void *identity);
typedef struct PSI_file_locker* (*get_thread_file_stream_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   struct PSI_file *file, enum PSI_file_operation op);
typedef struct PSI_file_locker* (*get_thread_file_descriptor_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   File file, enum PSI_file_operation op);
typedef void (*unlock_mutex_v1_t)
  (struct PSI_mutex *mutex);
typedef void (*unlock_rwlock_v1_t)
  (struct PSI_rwlock *rwlock);
typedef void (*signal_cond_v1_t)
  (struct PSI_cond *cond);
typedef void (*broadcast_cond_v1_t)
  (struct PSI_cond *cond);
typedef struct PSI_idle_locker* (*start_idle_wait_v1_t)
  (struct PSI_idle_locker_state_v1 *state, const char *src_file, uint src_line);
typedef void (*end_idle_wait_v1_t)
  (struct PSI_idle_locker *locker);
typedef struct PSI_mutex_locker* (*start_mutex_wait_v1_t)
  (struct PSI_mutex_locker_state_v1 *state,
   struct PSI_mutex *mutex,
   enum PSI_mutex_operation op,
   const char *src_file, uint src_line);
typedef void (*end_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, int rc);
typedef struct PSI_rwlock_locker* (*start_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker_state_v1 *state,
   struct PSI_rwlock *rwlock,
   enum PSI_rwlock_operation op,
   const char *src_file, uint src_line);
typedef void (*end_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);
typedef struct PSI_rwlock_locker* (*start_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker_state_v1 *state,
   struct PSI_rwlock *rwlock,
   enum PSI_rwlock_operation op,
   const char *src_file, uint src_line);
typedef void (*end_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);
typedef struct PSI_cond_locker* (*start_cond_wait_v1_t)
  (struct PSI_cond_locker_state_v1 *state,
   struct PSI_cond *cond,
   struct PSI_mutex *mutex,
   enum PSI_cond_operation op,
   const char *src_file, uint src_line);
typedef void (*end_cond_wait_v1_t)
  (struct PSI_cond_locker *locker, int rc);
typedef struct PSI_table_locker* (*start_table_io_wait_v1_t)
  (struct PSI_table_locker_state *state,
   struct PSI_table *table,
   enum PSI_table_io_operation op,
   uint index,
   const char *src_file, uint src_line);
typedef void (*end_table_io_wait_v1_t)
  (struct PSI_table_locker *locker,
   ulonglong numrows);
typedef struct PSI_table_locker* (*start_table_lock_wait_v1_t)
  (struct PSI_table_locker_state *state,
   struct PSI_table *table,
   enum PSI_table_lock_operation op,
   ulong flags,
   const char *src_file, uint src_line);
typedef void (*end_table_lock_wait_v1_t)(struct PSI_table_locker *locker);
typedef void (*unlock_table_v1_t)(struct PSI_table *table);
typedef void (*start_file_open_wait_v1_t)
  (struct PSI_file_locker *locker, const char *src_file, uint src_line);
typedef struct PSI_file* (*end_file_open_wait_v1_t)
  (struct PSI_file_locker *locker, void *result);
typedef void (*end_file_open_wait_and_bind_to_descriptor_v1_t)
  (struct PSI_file_locker *locker, File file);
typedef void (*end_temp_file_open_wait_and_bind_to_descriptor_v1_t)
  (struct PSI_file_locker *locker, File file, const char *filename);
typedef void (*start_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count,
   const char *src_file, uint src_line);
typedef void (*end_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count);
typedef void (*start_file_close_wait_v1_t)
  (struct PSI_file_locker *locker, const char *src_file, uint src_line);
typedef void (*end_file_close_wait_v1_t)
  (struct PSI_file_locker *locker, int rc);
typedef PSI_stage_progress* (*start_stage_v1_t)
  (PSI_stage_key key, const char *src_file, int src_line);
typedef PSI_stage_progress* (*get_current_stage_progress_v1_t)(void);
typedef void (*end_stage_v1_t) (void);
typedef struct PSI_statement_locker* (*get_thread_statement_locker_v1_t)
  (struct PSI_statement_locker_state_v1 *state,
   PSI_statement_key key, const void *charset, PSI_sp_share *sp_share);
typedef struct PSI_statement_locker* (*refine_statement_v1_t)
  (struct PSI_statement_locker *locker,
   PSI_statement_key key);
typedef void (*start_statement_v1_t)
  (struct PSI_statement_locker *locker,
   const char *db, uint db_length,
   const char *src_file, uint src_line);
typedef void (*set_statement_text_v1_t)
  (struct PSI_statement_locker *locker,
   const char *text, uint text_len);
typedef void (*set_statement_lock_time_t)
  (struct PSI_statement_locker *locker, ulonglong lock_time);
typedef void (*set_statement_rows_sent_t)
  (struct PSI_statement_locker *locker, ulonglong count);
typedef void (*set_statement_rows_examined_t)
  (struct PSI_statement_locker *locker, ulonglong count);
typedef void (*inc_statement_created_tmp_disk_tables_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_created_tmp_tables_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_full_join_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_full_range_join_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_range_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_range_check_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_scan_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_sort_merge_passes_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_sort_range_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_sort_rows_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_sort_scan_t)
  (struct PSI_statement_locker *locker, ulong count);
typedef void (*set_statement_no_index_used_t)
  (struct PSI_statement_locker *locker);
typedef void (*set_statement_no_good_index_used_t)
  (struct PSI_statement_locker *locker);
typedef void (*end_statement_v1_t)
  (struct PSI_statement_locker *locker, void *stmt_da);
typedef struct PSI_transaction_locker* (*get_thread_transaction_locker_v1_t)
  (struct PSI_transaction_locker_state_v1 *state, const void *xid,
   const ulonglong *trxid, int isolation_level, my_bool read_only,
   my_bool autocommit);
typedef void (*start_transaction_v1_t)
  (struct PSI_transaction_locker *locker,
   const char *src_file, uint src_line);
typedef void (*set_transaction_xid_v1_t)
  (struct PSI_transaction_locker *locker,
   const void *xid, int xa_state);
typedef void (*set_transaction_xa_state_v1_t)
  (struct PSI_transaction_locker *locker,
   int xa_state);
typedef void (*set_transaction_gtid_v1_t)
  (struct PSI_transaction_locker *locker,
   const void *sid, const void *gtid_spec);
typedef void (*set_transaction_trxid_v1_t)
  (struct PSI_transaction_locker *locker,
   const ulonglong *trxid);
typedef void (*inc_transaction_savepoints_v1_t)
  (struct PSI_transaction_locker *locker, ulong count);
typedef void (*inc_transaction_rollback_to_savepoint_v1_t)
  (struct PSI_transaction_locker *locker, ulong count);
typedef void (*inc_transaction_release_savepoint_v1_t)
  (struct PSI_transaction_locker *locker, ulong count);
typedef void (*end_transaction_v1_t)
  (struct PSI_transaction_locker *locker,
   my_bool commit);
typedef struct PSI_socket_locker* (*start_socket_wait_v1_t)
  (struct PSI_socket_locker_state_v1 *state,
   struct PSI_socket *socket,
   enum PSI_socket_operation op,
   size_t count,
   const char *src_file, uint src_line);
typedef void (*end_socket_wait_v1_t)
  (struct PSI_socket_locker *locker, size_t count);
typedef void (*set_socket_state_v1_t)(struct PSI_socket *socket,
                                      enum PSI_socket_state state);
typedef void (*set_socket_info_v1_t)(struct PSI_socket *socket,
                                     const my_socket *fd,
                                     const struct sockaddr *addr,
                                     socklen_t addr_len);
typedef void (*set_socket_thread_owner_v1_t)(struct PSI_socket *socket);
typedef PSI_prepared_stmt* (*create_prepared_stmt_v1_t)
  (void *identity, uint stmt_id, PSI_statement_locker *locker,
   const char *stmt_name, size_t stmt_name_length,
   const char *name, size_t length);
typedef void (*destroy_prepared_stmt_v1_t)
  (PSI_prepared_stmt *prepared_stmt);
typedef void (*reprepare_prepared_stmt_v1_t)
  (PSI_prepared_stmt *prepared_stmt);
typedef void (*execute_prepared_stmt_v1_t)
  (PSI_statement_locker *locker, PSI_prepared_stmt* prepared_stmt);
typedef struct PSI_digest_locker * (*digest_start_v1_t)
  (struct PSI_statement_locker *locker);
typedef void (*digest_end_v1_t)
  (struct PSI_digest_locker *locker, const struct sql_digest_storage *digest);
typedef PSI_sp_locker* (*start_sp_v1_t)
  (struct PSI_sp_locker_state_v1 *state, struct PSI_sp_share* sp_share);
typedef void (*end_sp_v1_t)
  (struct PSI_sp_locker *locker);
typedef void (*drop_sp_v1_t)
  (uint object_type,
   const char *schema_name, uint schema_name_length,
   const char *object_name, uint object_name_length);
typedef struct PSI_sp_share* (*get_sp_share_v1_t)
  (uint object_type,
   const char *schema_name, uint schema_name_length,
   const char *object_name, uint object_name_length);
typedef void (*release_sp_share_v1_t)(struct PSI_sp_share *share);
typedef PSI_metadata_lock* (*create_metadata_lock_v1_t)
  (void *identity,
   const MDL_key *key,
   opaque_mdl_type mdl_type,
   opaque_mdl_duration mdl_duration,
   opaque_mdl_status mdl_status,
   const char *src_file,
   uint src_line);
typedef void (*set_metadata_lock_status_v1_t)(PSI_metadata_lock *lock,
                                              opaque_mdl_status mdl_status);
typedef void (*destroy_metadata_lock_v1_t)(PSI_metadata_lock *lock);
typedef struct PSI_metadata_locker* (*start_metadata_wait_v1_t)
  (struct PSI_metadata_locker_state_v1 *state,
   struct PSI_metadata_lock *mdl,
   const char *src_file, uint src_line);
typedef void (*end_metadata_wait_v1_t)
  (struct PSI_metadata_locker *locker, int rc);
typedef int (*set_thread_connect_attrs_v1_t)(const char *buffer, uint length,
                                             const void *from_cs);
struct PSI_v1
{
  register_mutex_v1_t register_mutex;
  register_rwlock_v1_t register_rwlock;
  register_cond_v1_t register_cond;
  register_thread_v1_t register_thread;
  register_file_v1_t register_file;
  register_stage_v1_t register_stage;
  register_statement_v1_t register_statement;
  register_socket_v1_t register_socket;
  init_mutex_v1_t init_mutex;
  destroy_mutex_v1_t destroy_mutex;
  init_rwlock_v1_t init_rwlock;
  destroy_rwlock_v1_t destroy_rwlock;
  init_cond_v1_t init_cond;
  destroy_cond_v1_t destroy_cond;
  init_socket_v1_t init_socket;
  destroy_socket_v1_t destroy_socket;
  get_table_share_v1_t get_table_share;
  release_table_share_v1_t release_table_share;
  drop_table_share_v1_t drop_table_share;
  open_table_v1_t open_table;
  unbind_table_v1_t unbind_table;
  rebind_table_v1_t rebind_table;
  close_table_v1_t close_table;
  create_file_v1_t create_file;
  spawn_thread_v1_t spawn_thread;
  new_thread_v1_t new_thread;
  set_thread_id_v1_t set_thread_id;
  set_thread_THD_v1_t set_thread_THD;
  set_thread_os_id_v1_t set_thread_os_id;
  get_thread_v1_t get_thread;
  set_thread_user_v1_t set_thread_user;
  set_thread_account_v1_t set_thread_account;
  set_thread_db_v1_t set_thread_db;
  set_thread_command_v1_t set_thread_command;
  set_connection_type_v1_t set_connection_type;
  set_thread_start_time_v1_t set_thread_start_time;
  set_thread_state_v1_t set_thread_state;
  set_thread_info_v1_t set_thread_info;
  set_thread_v1_t set_thread;
  delete_current_thread_v1_t delete_current_thread;
  delete_thread_v1_t delete_thread;
  get_thread_file_name_locker_v1_t get_thread_file_name_locker;
  get_thread_file_stream_locker_v1_t get_thread_file_stream_locker;
  get_thread_file_descriptor_locker_v1_t get_thread_file_descriptor_locker;
  unlock_mutex_v1_t unlock_mutex;
  unlock_rwlock_v1_t unlock_rwlock;
  signal_cond_v1_t signal_cond;
  broadcast_cond_v1_t broadcast_cond;
  start_idle_wait_v1_t start_idle_wait;
  end_idle_wait_v1_t end_idle_wait;
  start_mutex_wait_v1_t start_mutex_wait;
  end_mutex_wait_v1_t end_mutex_wait;
  start_rwlock_rdwait_v1_t start_rwlock_rdwait;
  end_rwlock_rdwait_v1_t end_rwlock_rdwait;
  start_rwlock_wrwait_v1_t start_rwlock_wrwait;
  end_rwlock_wrwait_v1_t end_rwlock_wrwait;
  start_cond_wait_v1_t start_cond_wait;
  end_cond_wait_v1_t end_cond_wait;
  start_table_io_wait_v1_t start_table_io_wait;
  end_table_io_wait_v1_t end_table_io_wait;
  start_table_lock_wait_v1_t start_table_lock_wait;
  end_table_lock_wait_v1_t end_table_lock_wait;
  start_file_open_wait_v1_t start_file_open_wait;
  end_file_open_wait_v1_t end_file_open_wait;
  end_file_open_wait_and_bind_to_descriptor_v1_t
    end_file_open_wait_and_bind_to_descriptor;
  end_temp_file_open_wait_and_bind_to_descriptor_v1_t
    end_temp_file_open_wait_and_bind_to_descriptor;
  start_file_wait_v1_t start_file_wait;
  end_file_wait_v1_t end_file_wait;
  start_file_close_wait_v1_t start_file_close_wait;
  end_file_close_wait_v1_t end_file_close_wait;
  start_stage_v1_t start_stage;
  get_current_stage_progress_v1_t get_current_stage_progress;
  end_stage_v1_t end_stage;
  get_thread_statement_locker_v1_t get_thread_statement_locker;
  refine_statement_v1_t refine_statement;
  start_statement_v1_t start_statement;
  set_statement_text_v1_t set_statement_text;
  set_statement_lock_time_t set_statement_lock_time;
  set_statement_rows_sent_t set_statement_rows_sent;
  set_statement_rows_examined_t set_statement_rows_examined;
  inc_statement_created_tmp_disk_tables_t inc_statement_created_tmp_disk_tables;
  inc_statement_created_tmp_tables_t inc_statement_created_tmp_tables;
  inc_statement_select_full_join_t inc_statement_select_full_join;
  inc_statement_select_full_range_join_t inc_statement_select_full_range_join;
  inc_statement_select_range_t inc_statement_select_range;
  inc_statement_select_range_check_t inc_statement_select_range_check;
  inc_statement_select_scan_t inc_statement_select_scan;
  inc_statement_sort_merge_passes_t inc_statement_sort_merge_passes;
  inc_statement_sort_range_t inc_statement_sort_range;
  inc_statement_sort_rows_t inc_statement_sort_rows;
  inc_statement_sort_scan_t inc_statement_sort_scan;
  set_statement_no_index_used_t set_statement_no_index_used;
  set_statement_no_good_index_used_t set_statement_no_good_index_used;
  end_statement_v1_t end_statement;
  get_thread_transaction_locker_v1_t get_thread_transaction_locker;
  start_transaction_v1_t start_transaction;
  set_transaction_xid_v1_t set_transaction_xid;
  set_transaction_xa_state_v1_t set_transaction_xa_state;
  set_transaction_gtid_v1_t set_transaction_gtid;
  set_transaction_trxid_v1_t set_transaction_trxid;
  inc_transaction_savepoints_v1_t inc_transaction_savepoints;
  inc_transaction_rollback_to_savepoint_v1_t inc_transaction_rollback_to_savepoint;
  inc_transaction_release_savepoint_v1_t inc_transaction_release_savepoint;
  end_transaction_v1_t end_transaction;
  start_socket_wait_v1_t start_socket_wait;
  end_socket_wait_v1_t end_socket_wait;
  set_socket_state_v1_t set_socket_state;
  set_socket_info_v1_t set_socket_info;
  set_socket_thread_owner_v1_t set_socket_thread_owner;
  create_prepared_stmt_v1_t create_prepared_stmt;
  destroy_prepared_stmt_v1_t destroy_prepared_stmt;
  reprepare_prepared_stmt_v1_t reprepare_prepared_stmt;
  execute_prepared_stmt_v1_t execute_prepared_stmt;
  digest_start_v1_t digest_start;
  digest_end_v1_t digest_end;
  set_thread_connect_attrs_v1_t set_thread_connect_attrs;
  start_sp_v1_t start_sp;
  end_sp_v1_t end_sp;
  drop_sp_v1_t drop_sp;
  get_sp_share_v1_t get_sp_share;
  release_sp_share_v1_t release_sp_share;
  register_memory_v1_t register_memory;
  memory_alloc_v1_t memory_alloc;
  memory_realloc_v1_t memory_realloc;
  memory_claim_v1_t memory_claim;
  memory_free_v1_t memory_free;
  unlock_table_v1_t unlock_table;
  create_metadata_lock_v1_t create_metadata_lock;
  set_metadata_lock_status_v1_t set_metadata_lock_status;
  destroy_metadata_lock_v1_t destroy_metadata_lock;
  start_metadata_wait_v1_t start_metadata_wait;
  end_metadata_wait_v1_t end_metadata_wait;
};
typedef struct PSI_v1 PSI;
typedef struct PSI_mutex_info_v1 PSI_mutex_info;
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info;
typedef struct PSI_cond_info_v1 PSI_cond_info;
typedef struct PSI_thread_info_v1 PSI_thread_info;
typedef struct PSI_file_info_v1 PSI_file_info;
typedef struct PSI_stage_info_v1 PSI_stage_info;
typedef struct PSI_statement_info_v1 PSI_statement_info;
typedef struct PSI_transaction_info_v1 PSI_transaction_info;
typedef struct PSI_socket_info_v1 PSI_socket_info;
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state;
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state;
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state;
typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state;
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state;
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state;
typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
