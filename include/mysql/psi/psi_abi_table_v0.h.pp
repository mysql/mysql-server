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
