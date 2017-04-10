#include "mysql/psi/psi_table.h"
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
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "psi_base.h"
#include "my_psi_config.h"
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
typedef unsigned int PSI_stage_key;
typedef unsigned int PSI_statement_key;
typedef unsigned int PSI_socket_key;
struct PSI_placeholder
{
  int m_placeholder;
};
struct TABLE_SHARE;
struct PSI_table_locker;
typedef struct PSI_table_locker PSI_table_locker;
enum PSI_table_io_operation
{
  PSI_TABLE_FETCH_ROW = 0,
  PSI_TABLE_WRITE_ROW = 1,
  PSI_TABLE_UPDATE_ROW = 2,
  PSI_TABLE_DELETE_ROW = 3
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
struct PSI_table_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_table_bootstrap PSI_table_bootstrap;
struct PSI_table_share;
typedef struct PSI_table_share PSI_table_share;
struct PSI_table;
typedef struct PSI_table PSI_table;
enum PSI_table_lock_operation
{
  PSI_TABLE_LOCK = 0,
  PSI_TABLE_EXTERNAL_LOCK = 1
};
typedef enum PSI_table_lock_operation PSI_table_lock_operation;
typedef struct PSI_table_share *(*get_table_share_v1_t)(
  bool temporary, struct TABLE_SHARE *share);
typedef void (*release_table_share_v1_t)(struct PSI_table_share *share);
typedef void (*drop_table_share_v1_t)(bool temporary,
                                      const char *schema_name,
                                      int schema_name_length,
                                      const char *table_name,
                                      int table_name_length);
typedef struct PSI_table *(*open_table_v1_t)(struct PSI_table_share *share,
                                             const void *identity);
typedef void (*unbind_table_v1_t)(struct PSI_table *table);
typedef PSI_table *(*rebind_table_v1_t)(PSI_table_share *share,
                                        const void *identity,
                                        PSI_table *table);
typedef void (*close_table_v1_t)(struct TABLE_SHARE *server_share,
                                 struct PSI_table *table);
typedef struct PSI_table_locker *(*start_table_io_wait_v1_t)(
  struct PSI_table_locker_state *state,
  struct PSI_table *table,
  enum PSI_table_io_operation op,
  uint index,
  const char *src_file,
  uint src_line);
typedef void (*end_table_io_wait_v1_t)(struct PSI_table_locker *locker,
                                       ulonglong numrows);
typedef struct PSI_table_locker *(*start_table_lock_wait_v1_t)(
  struct PSI_table_locker_state *state,
  struct PSI_table *table,
  enum PSI_table_lock_operation op,
  ulong flags,
  const char *src_file,
  uint src_line);
typedef void (*end_table_lock_wait_v1_t)(struct PSI_table_locker *locker);
typedef void (*unlock_table_v1_t)(struct PSI_table *table);
struct PSI_table_service_v1
{
  get_table_share_v1_t get_table_share;
  release_table_share_v1_t release_table_share;
  drop_table_share_v1_t drop_table_share;
  open_table_v1_t open_table;
  unbind_table_v1_t unbind_table;
  rebind_table_v1_t rebind_table;
  close_table_v1_t close_table;
  start_table_io_wait_v1_t start_table_io_wait;
  end_table_io_wait_v1_t end_table_io_wait;
  start_table_lock_wait_v1_t start_table_lock_wait;
  end_table_lock_wait_v1_t end_table_lock_wait;
  unlock_table_v1_t unlock_table;
};
typedef struct PSI_table_service_v1 PSI_table_service_t;
extern PSI_table_service_t *psi_table_service;
