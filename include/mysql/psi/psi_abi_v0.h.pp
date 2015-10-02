#include "mysql/psi/psi.h"
#include "psi_base.h"
#include "psi_memory.h"
#include "psi_base.h"
struct PSI_thread;
typedef unsigned int PSI_memory_key;
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
struct PSI_none
{
  int opaque;
};
typedef struct PSI_none PSI;
struct PSI_stage_info_none
{
  unsigned int m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_stage_info_none PSI_stage_info;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
