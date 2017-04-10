#include "mysql/psi/psi_mutex.h"
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
struct PSI_mutex;
typedef struct PSI_mutex PSI_mutex;
struct PSI_mutex_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_mutex_bootstrap PSI_mutex_bootstrap;
struct PSI_mutex_locker;
typedef struct PSI_mutex_locker PSI_mutex_locker;
enum PSI_mutex_operation
{
  PSI_MUTEX_LOCK = 0,
  PSI_MUTEX_TRYLOCK = 1
};
typedef enum PSI_mutex_operation PSI_mutex_operation;
struct PSI_mutex_info_v1
{
  PSI_mutex_key *m_key;
  const char *m_name;
  int m_flags;
  int m_volatility;
};
typedef struct PSI_mutex_info_v1 PSI_mutex_info_v1;
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
typedef void (*register_mutex_v1_t)(const char *category,
                                    struct PSI_mutex_info_v1 *info,
                                    int count);
typedef struct PSI_mutex *(*init_mutex_v1_t)(PSI_mutex_key key,
                                             const void *identity);
typedef void (*destroy_mutex_v1_t)(struct PSI_mutex *mutex);
typedef void (*unlock_mutex_v1_t)(struct PSI_mutex *mutex);
typedef struct PSI_mutex_locker *(*start_mutex_wait_v1_t)(
  struct PSI_mutex_locker_state_v1 *state,
  struct PSI_mutex *mutex,
  enum PSI_mutex_operation op,
  const char *src_file,
  uint src_line);
typedef void (*end_mutex_wait_v1_t)(struct PSI_mutex_locker *locker, int rc);
struct PSI_mutex_service_v1
{
  register_mutex_v1_t register_mutex;
  init_mutex_v1_t init_mutex;
  destroy_mutex_v1_t destroy_mutex;
  start_mutex_wait_v1_t start_mutex_wait;
  end_mutex_wait_v1_t end_mutex_wait;
  unlock_mutex_v1_t unlock_mutex;
};
typedef struct PSI_mutex_service_v1 PSI_mutex_service_t;
typedef struct PSI_mutex_info_v1 PSI_mutex_info;
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state;
extern PSI_mutex_service_t *psi_mutex_service;
