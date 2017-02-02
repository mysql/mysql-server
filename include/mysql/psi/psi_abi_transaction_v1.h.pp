#include "mysql/psi/psi_transaction.h"
#include "my_global.h"
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
typedef char my_bool;
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
struct PSI_transaction_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_transaction_bootstrap PSI_transaction_bootstrap;
struct PSI_transaction_locker;
typedef struct PSI_transaction_locker PSI_transaction_locker;
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
typedef struct PSI_transaction_locker *(*get_thread_transaction_locker_v1_t)(
  struct PSI_transaction_locker_state_v1 *state,
  const void *xid,
  const ulonglong *trxid,
  int isolation_level,
  my_bool read_only,
  my_bool autocommit);
typedef void (*start_transaction_v1_t)(struct PSI_transaction_locker *locker,
                                       const char *src_file,
                                       uint src_line);
typedef void (*set_transaction_xid_v1_t)(struct PSI_transaction_locker *locker,
                                         const void *xid,
                                         int xa_state);
typedef void (*set_transaction_xa_state_v1_t)(
  struct PSI_transaction_locker *locker, int xa_state);
typedef void (*set_transaction_gtid_v1_t)(struct PSI_transaction_locker *locker,
                                          const void *sid,
                                          const void *gtid_spec);
typedef void (*set_transaction_trxid_v1_t)(
  struct PSI_transaction_locker *locker, const ulonglong *trxid);
typedef void (*inc_transaction_savepoints_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);
typedef void (*inc_transaction_rollback_to_savepoint_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);
typedef void (*inc_transaction_release_savepoint_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);
typedef void (*end_transaction_v1_t)(struct PSI_transaction_locker *locker,
                                     my_bool commit);
struct PSI_transaction_service_v1
{
  get_thread_transaction_locker_v1_t get_thread_transaction_locker;
  start_transaction_v1_t start_transaction;
  set_transaction_xid_v1_t set_transaction_xid;
  set_transaction_xa_state_v1_t set_transaction_xa_state;
  set_transaction_gtid_v1_t set_transaction_gtid;
  set_transaction_trxid_v1_t set_transaction_trxid;
  inc_transaction_savepoints_v1_t inc_transaction_savepoints;
  inc_transaction_rollback_to_savepoint_v1_t
    inc_transaction_rollback_to_savepoint;
  inc_transaction_release_savepoint_v1_t inc_transaction_release_savepoint;
  end_transaction_v1_t end_transaction;
};
typedef struct PSI_transaction_service_v1 PSI_transaction_service_t;
typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state;
extern PSI_transaction_service_t *psi_transaction_service;
