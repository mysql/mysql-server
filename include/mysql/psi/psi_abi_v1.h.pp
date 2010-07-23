#include "mysql/psi/psi.h"
C_MODE_START
struct PSI_mutex;
struct PSI_rwlock;
struct PSI_cond;
struct PSI_table_share;
struct PSI_table;
struct PSI_thread;
struct PSI_file;
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
struct PSI_table_locker;
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
struct PSI_mutex_info_v1
{
  PSI_mutex_key *m_key;
  const char *m_name;
  int m_flags;
};
struct PSI_rwlock_info_v1
{
  PSI_rwlock_key *m_key;
  const char *m_name;
  int m_flags;
};
struct PSI_cond_info_v1
{
  PSI_cond_key *m_key;
  const char *m_name;
  int m_flags;
};
struct PSI_thread_info_v1
{
  PSI_thread_key *m_key;
  const char *m_name;
  int m_flags;
};
struct PSI_file_info_v1
{
  PSI_file_key *m_key;
  const char *m_name;
  int m_flags;
};
struct PSI_mutex_locker_state_v1
{
  uint m_flags;
  struct PSI_mutex *m_mutex;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_mutex_operation m_operation;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
struct PSI_rwlock_locker_state_v1
{
  uint m_flags;
  struct PSI_rwlock *m_rwlock;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_rwlock_operation m_operation;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
struct PSI_cond_locker_state_v1
{
  uint m_flags;
  struct PSI_cond *m_cond;
  struct PSI_mutex *m_mutex;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_cond_operation m_operation;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
struct PSI_file_locker_state_v1
{
  uint m_flags;
  struct PSI_file *m_file;
  struct PSI_thread *m_thread;
  size_t m_number_of_bytes;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_file_operation m_operation;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
struct PSI_table_locker_state_v1
{
  uint m_flags;
  struct PSI_table *m_table;
  struct PSI_table_share *m_table_share;
  void *m_class;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  uint m_index;
  uint m_lock_index;
  const char* m_src_file;
  int m_src_line;
  void *m_wait;
};
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
typedef struct PSI_mutex* (*init_mutex_v1_t)
  (PSI_mutex_key key, const void *identity);
typedef void (*destroy_mutex_v1_t)(struct PSI_mutex *mutex);
typedef struct PSI_rwlock* (*init_rwlock_v1_t)
  (PSI_rwlock_key key, const void *identity);
typedef void (*destroy_rwlock_v1_t)(struct PSI_rwlock *rwlock);
typedef struct PSI_cond* (*init_cond_v1_t)
  (PSI_cond_key key, const void *identity);
typedef void (*destroy_cond_v1_t)(struct PSI_cond *cond);
typedef struct PSI_table_share* (*get_table_share_v1_t)
  (const char *schema_name, int schema_name_length, const char *table_name,
   int table_name_length, const void *identity);
typedef void (*release_table_share_v1_t)(struct PSI_table_share *share);
typedef struct PSI_table* (*open_table_v1_t)
  (struct PSI_table_share *share, const void *identity);
typedef void (*close_table_v1_t)(struct PSI_table *table);
typedef void (*create_file_v1_t)(PSI_file_key key, const char *name,
                                 File file);
typedef int (*spawn_thread_v1_t)(PSI_thread_key key,
                                 pthread_t *thread,
                                 const pthread_attr_t *attr,
                                 void *(*start_routine)(void*), void *arg);
typedef struct PSI_thread* (*new_thread_v1_t)
  (PSI_thread_key key, const void *identity, ulong thread_id);
typedef void (*set_thread_id_v1_t)(struct PSI_thread *thread,
                                   unsigned long id);
typedef struct PSI_thread* (*get_thread_v1_t)(void);
typedef void (*set_thread_v1_t)(struct PSI_thread *thread);
typedef void (*delete_current_thread_v1_t)(void);
typedef void (*delete_thread_v1_t)(struct PSI_thread *thread);
typedef struct PSI_mutex_locker* (*get_thread_mutex_locker_v1_t)
  (struct PSI_mutex_locker_state_v1 *state,
   struct PSI_mutex *mutex,
   enum PSI_mutex_operation op);
typedef struct PSI_rwlock_locker* (*get_thread_rwlock_locker_v1_t)
  (struct PSI_rwlock_locker_state_v1 *state,
   struct PSI_rwlock *rwlock,
   enum PSI_rwlock_operation op);
typedef struct PSI_cond_locker* (*get_thread_cond_locker_v1_t)
  (struct PSI_cond_locker_state_v1 *state,
   struct PSI_cond *cond, struct PSI_mutex *mutex,
   enum PSI_cond_operation op);
typedef struct PSI_table_locker* (*get_thread_table_locker_v1_t)
  (struct PSI_table_locker_state_v1 *state,
   struct PSI_table *table);
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
typedef void (*start_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, const char *src_file, uint src_line);
typedef void (*end_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, int rc);
typedef void (*start_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker *locker, const char *src_file, uint src_line);
typedef void (*end_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);
typedef void (*start_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker *locker, const char *src_file, uint src_line);
typedef void (*end_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);
typedef void (*start_cond_wait_v1_t)
  (struct PSI_cond_locker *locker, const char *src_file, uint src_line);
typedef void (*end_cond_wait_v1_t)
  (struct PSI_cond_locker *locker, int rc);
typedef void (*start_table_wait_v1_t)
  (struct PSI_table_locker *locker, const char *src_file, uint src_line);
typedef void (*end_table_wait_v1_t)(struct PSI_table_locker *locker);
typedef struct PSI_file* (*start_file_open_wait_v1_t)
  (struct PSI_file_locker *locker, const char *src_file, uint src_line);
typedef void (*end_file_open_wait_v1_t)(struct PSI_file_locker *locker);
typedef void (*end_file_open_wait_and_bind_to_descriptor_v1_t)
  (struct PSI_file_locker *locker, File file);
typedef void (*start_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count,
   const char *src_file, uint src_line);
typedef void (*end_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count);
struct PSI_v1
{
  register_mutex_v1_t register_mutex;
  register_rwlock_v1_t register_rwlock;
  register_cond_v1_t register_cond;
  register_thread_v1_t register_thread;
  register_file_v1_t register_file;
  init_mutex_v1_t init_mutex;
  destroy_mutex_v1_t destroy_mutex;
  init_rwlock_v1_t init_rwlock;
  destroy_rwlock_v1_t destroy_rwlock;
  init_cond_v1_t init_cond;
  destroy_cond_v1_t destroy_cond;
  get_table_share_v1_t get_table_share;
  release_table_share_v1_t release_table_share;
  open_table_v1_t open_table;
  close_table_v1_t close_table;
  create_file_v1_t create_file;
  spawn_thread_v1_t spawn_thread;
  new_thread_v1_t new_thread;
  set_thread_id_v1_t set_thread_id;
  get_thread_v1_t get_thread;
  set_thread_v1_t set_thread;
  delete_current_thread_v1_t delete_current_thread;
  delete_thread_v1_t delete_thread;
  get_thread_mutex_locker_v1_t get_thread_mutex_locker;
  get_thread_rwlock_locker_v1_t get_thread_rwlock_locker;
  get_thread_cond_locker_v1_t get_thread_cond_locker;
  get_thread_table_locker_v1_t get_thread_table_locker;
  get_thread_file_name_locker_v1_t get_thread_file_name_locker;
  get_thread_file_stream_locker_v1_t get_thread_file_stream_locker;
  get_thread_file_descriptor_locker_v1_t get_thread_file_descriptor_locker;
  unlock_mutex_v1_t unlock_mutex;
  unlock_rwlock_v1_t unlock_rwlock;
  signal_cond_v1_t signal_cond;
  broadcast_cond_v1_t broadcast_cond;
  start_mutex_wait_v1_t start_mutex_wait;
  end_mutex_wait_v1_t end_mutex_wait;
  start_rwlock_rdwait_v1_t start_rwlock_rdwait;
  end_rwlock_rdwait_v1_t end_rwlock_rdwait;
  start_rwlock_wrwait_v1_t start_rwlock_wrwait;
  end_rwlock_wrwait_v1_t end_rwlock_wrwait;
  start_cond_wait_v1_t start_cond_wait;
  end_cond_wait_v1_t end_cond_wait;
  start_table_wait_v1_t start_table_wait;
  end_table_wait_v1_t end_table_wait;
  start_file_open_wait_v1_t start_file_open_wait;
  end_file_open_wait_v1_t end_file_open_wait;
  end_file_open_wait_and_bind_to_descriptor_v1_t
    end_file_open_wait_and_bind_to_descriptor;
  start_file_wait_v1_t start_file_wait;
  end_file_wait_v1_t end_file_wait;
};
typedef struct PSI_v1 PSI;
typedef struct PSI_mutex_info_v1 PSI_mutex_info;
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info;
typedef struct PSI_cond_info_v1 PSI_cond_info;
typedef struct PSI_thread_info_v1 PSI_thread_info;
typedef struct PSI_file_info_v1 PSI_file_info;
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state;
typedef struct PSI_table_locker_state_v1 PSI_table_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
