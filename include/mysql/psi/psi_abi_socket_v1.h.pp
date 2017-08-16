#include "mysql/psi/psi_socket.h"
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
#include "my_io.h"
static inline int is_directory_separator(char c)
{
  return c == '/';
}
typedef int File;
typedef mode_t MY_MODE;
typedef socklen_t socket_len_t;
typedef int my_socket;
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "mysql/components/services/psi_socket_bits.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
typedef unsigned int PSI_socket_key;
struct PSI_socket;
typedef struct PSI_socket PSI_socket;
struct PSI_socket_locker;
typedef struct PSI_socket_locker PSI_socket_locker;
enum PSI_socket_state
{
  PSI_SOCKET_STATE_IDLE = 1,
  PSI_SOCKET_STATE_ACTIVE = 2
};
typedef enum PSI_socket_state PSI_socket_state;
enum PSI_socket_operation
{
  PSI_SOCKET_CREATE = 0,
  PSI_SOCKET_CONNECT = 1,
  PSI_SOCKET_BIND = 2,
  PSI_SOCKET_CLOSE = 3,
  PSI_SOCKET_SEND = 4,
  PSI_SOCKET_RECV = 5,
  PSI_SOCKET_SENDTO = 6,
  PSI_SOCKET_RECVFROM = 7,
  PSI_SOCKET_SENDMSG = 8,
  PSI_SOCKET_RECVMSG = 9,
  PSI_SOCKET_SEEK = 10,
  PSI_SOCKET_OPT = 11,
  PSI_SOCKET_STAT = 12,
  PSI_SOCKET_SHUTDOWN = 13,
  PSI_SOCKET_SELECT = 14
};
typedef enum PSI_socket_operation PSI_socket_operation;
struct PSI_socket_info_v1
{
  PSI_socket_key *m_key;
  const char *m_name;
  uint m_flags;
  int m_volatility;
  const char *m_documentation;
};
typedef struct PSI_socket_info_v1 PSI_socket_info_v1;
struct PSI_socket_locker_state_v1
{
  uint m_flags;
  struct PSI_socket *m_socket;
  struct PSI_thread *m_thread;
  size_t m_number_of_bytes;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  enum PSI_socket_operation m_operation;
  const char *m_src_file;
  int m_src_line;
  void *m_wait;
};
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state_v1;
typedef void (*register_socket_v1_t)(const char *category,
                                     struct PSI_socket_info_v1 *info,
                                     int count);
typedef struct PSI_socket *(*init_socket_v1_t)(PSI_socket_key key,
                                               const my_socket *fd,
                                               const struct sockaddr *addr,
                                               socklen_t addr_len);
typedef void (*destroy_socket_v1_t)(struct PSI_socket *socket);
typedef struct PSI_socket_locker *(*start_socket_wait_v1_t)(
  struct PSI_socket_locker_state_v1 *state,
  struct PSI_socket *socket,
  enum PSI_socket_operation op,
  size_t count,
  const char *src_file,
  uint src_line);
typedef void (*end_socket_wait_v1_t)(struct PSI_socket_locker *locker,
                                     size_t count);
typedef void (*set_socket_state_v1_t)(struct PSI_socket *socket,
                                      enum PSI_socket_state state);
typedef void (*set_socket_info_v1_t)(struct PSI_socket *socket,
                                     const my_socket *fd,
                                     const struct sockaddr *addr,
                                     socklen_t addr_len);
typedef void (*set_socket_thread_owner_v1_t)(struct PSI_socket *socket);
typedef struct PSI_socket_info_v1 PSI_socket_info;
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state;
struct PSI_socket_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_socket_bootstrap PSI_socket_bootstrap;
struct PSI_socket_service_v1
{
  register_socket_v1_t register_socket;
  init_socket_v1_t init_socket;
  destroy_socket_v1_t destroy_socket;
  start_socket_wait_v1_t start_socket_wait;
  end_socket_wait_v1_t end_socket_wait;
  set_socket_state_v1_t set_socket_state;
  set_socket_info_v1_t set_socket_info;
  set_socket_thread_owner_v1_t set_socket_thread_owner;
};
typedef struct PSI_socket_service_v1 PSI_socket_service_t;
extern PSI_socket_service_t *psi_socket_service;
