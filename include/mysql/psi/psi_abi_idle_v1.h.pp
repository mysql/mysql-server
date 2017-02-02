#include "mysql/psi/psi_idle.h"
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
struct PSI_idle_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_idle_bootstrap PSI_idle_bootstrap;
struct PSI_idle_locker;
typedef struct PSI_idle_locker PSI_idle_locker;
struct PSI_idle_locker_state_v1
{
  uint m_flags;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_wait;
};
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state_v1;
typedef struct PSI_idle_locker *(*start_idle_wait_v1_t)(
  struct PSI_idle_locker_state_v1 *state, const char *src_file, uint src_line);
typedef void (*end_idle_wait_v1_t)(struct PSI_idle_locker *locker);
struct PSI_idle_service_v1
{
  start_idle_wait_v1_t start_idle_wait;
  end_idle_wait_v1_t end_idle_wait;
};
typedef struct PSI_idle_service_v1 PSI_idle_service_t;
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state;
extern PSI_idle_service_t *psi_idle_service;
