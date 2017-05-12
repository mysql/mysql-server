#include "mysql/psi/psi_mdl.h"
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
struct PSI_mdl_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_mdl_bootstrap PSI_mdl_bootstrap;
struct MDL_key;
typedef int opaque_mdl_type;
typedef int opaque_mdl_duration;
typedef int opaque_mdl_status;
struct PSI_metadata_lock;
typedef struct PSI_metadata_lock PSI_metadata_lock;
struct PSI_metadata_locker;
typedef struct PSI_metadata_locker PSI_metadata_locker;
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
typedef PSI_metadata_lock *(*create_metadata_lock_v1_t)(
  void *identity,
  const struct MDL_key *key,
  opaque_mdl_type mdl_type,
  opaque_mdl_duration mdl_duration,
  opaque_mdl_status mdl_status,
  const char *src_file,
  uint src_line);
typedef void (*set_metadata_lock_status_v1_t)(PSI_metadata_lock *lock,
                                              opaque_mdl_status mdl_status);
typedef void (*destroy_metadata_lock_v1_t)(PSI_metadata_lock *lock);
typedef struct PSI_metadata_locker *(*start_metadata_wait_v1_t)(
  struct PSI_metadata_locker_state_v1 *state,
  struct PSI_metadata_lock *mdl,
  const char *src_file,
  uint src_line);
typedef void (*end_metadata_wait_v1_t)(struct PSI_metadata_locker *locker,
                                       int rc);
struct PSI_mdl_service_v1
{
  create_metadata_lock_v1_t create_metadata_lock;
  set_metadata_lock_status_v1_t set_metadata_lock_status;
  destroy_metadata_lock_v1_t destroy_metadata_lock;
  start_metadata_wait_v1_t start_metadata_wait;
  end_metadata_wait_v1_t end_metadata_wait;
};
typedef struct PSI_mdl_service_v1 PSI_mdl_service_t;
typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state;
extern PSI_mdl_service_t *psi_mdl_service;
