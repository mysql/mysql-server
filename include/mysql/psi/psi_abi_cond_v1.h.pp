#include "mysql/psi/psi_cond.h"
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
struct PSI_cond;
typedef struct PSI_cond PSI_cond;
struct PSI_cond_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_cond_bootstrap PSI_cond_bootstrap;
struct PSI_cond_locker;
typedef struct PSI_cond_locker PSI_cond_locker;
enum PSI_cond_operation
{
  PSI_COND_WAIT = 0,
  PSI_COND_TIMEDWAIT = 1
};
typedef enum PSI_cond_operation PSI_cond_operation;
struct PSI_cond_info_v1
{
  PSI_cond_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_cond_info_v1 PSI_cond_info_v1;
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
typedef void (*register_cond_v1_t)(const char *category,
                                   struct PSI_cond_info_v1 *info,
                                   int count);
typedef struct PSI_cond *(*init_cond_v1_t)(PSI_cond_key key,
                                           const void *identity);
typedef void (*destroy_cond_v1_t)(struct PSI_cond *cond);
typedef void (*signal_cond_v1_t)(struct PSI_cond *cond);
typedef void (*broadcast_cond_v1_t)(struct PSI_cond *cond);
typedef struct PSI_cond_locker *(*start_cond_wait_v1_t)(
  struct PSI_cond_locker_state_v1 *state,
  struct PSI_cond *cond,
  struct PSI_mutex *mutex,
  enum PSI_cond_operation op,
  const char *src_file,
  uint src_line);
typedef void (*end_cond_wait_v1_t)(struct PSI_cond_locker *locker, int rc);
struct PSI_cond_service_v1
{
  register_cond_v1_t register_cond;
  init_cond_v1_t init_cond;
  destroy_cond_v1_t destroy_cond;
  signal_cond_v1_t signal_cond;
  broadcast_cond_v1_t broadcast_cond;
  start_cond_wait_v1_t start_cond_wait;
  end_cond_wait_v1_t end_cond_wait;
};
typedef struct PSI_cond_service_v1 PSI_cond_service_t;
typedef struct PSI_cond_info_v1 PSI_cond_info;
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state;
extern PSI_cond_service_t *psi_cond_service;
