#include "mysql/psi/psi_table.h"
#include "my_global.h"
#include "my_macros.h"
#include "my_psi_config.h"
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
typedef struct PSI_placeholder PSI_table_service_t;
extern MYSQL_PLUGIN_IMPORT PSI_table_service_t *psi_table_service;
