/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MYSQL_PSI_SOCKET_H
#define MYSQL_PSI_SOCKET_H

/**
  @file include/mysql/psi/psi_socket.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_socket Socket Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_global.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_SOCKET_VERSION_1
  Performance Schema Socket Interface number for version 1.
  This version is supported.
*/
#define PSI_SOCKET_VERSION_1 1

/**
  @def PSI_SOCKET_VERSION_2
  Performance Schema Socket Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_SOCKET_VERSION_2 2

/**
  @def PSI_CURRENT_SOCKET_VERSION
  Performance Schema Socket Interface number for the most recent version.
  The most current version is @c PSI_SOCKET_VERSION_1
*/
#define PSI_CURRENT_SOCKET_VERSION 1

#ifndef USE_PSI_SOCKET_2
#ifndef USE_PSI_SOCKET_1
#define USE_PSI_SOCKET_1
#endif /* USE_PSI_SOCKET_1 */
#endif /* USE_PSI_SOCKET_2 */

#ifdef USE_PSI_SOCKET_1
#define HAVE_PSI_SOCKET_1
#endif /* USE_PSI_SOCKET_1 */

#ifdef USE_PSI_SOCKET_2
#define HAVE_PSI_SOCKET_2
#endif /* USE_PSI_SOCKET_2 */

/**
  Interface for an instrumented socket descriptor.
  This is an opaque structure.
*/
struct PSI_socket;
typedef struct PSI_socket PSI_socket;

/** Entry point for the performance schema interface. */
struct PSI_socket_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_SOCKET_VERSION_1
    @sa PSI_SOCKET_VERSION_2
    @sa PSI_CURRENT_SOCKET_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_socket_bootstrap PSI_socket_bootstrap;

#ifdef HAVE_PSI_SOCKET_1

/**
  Interface for an instrumented socket operation.
  This is an opaque structure.
*/
struct PSI_socket_locker;
typedef struct PSI_socket_locker PSI_socket_locker;

/** State of an instrumented socket. */
enum PSI_socket_state
{
  /** Idle, waiting for the next command. */
  PSI_SOCKET_STATE_IDLE= 1,
  /** Active, executing a command. */
  PSI_SOCKET_STATE_ACTIVE= 2
};
typedef enum PSI_socket_state PSI_socket_state;

/** Operation performed on an instrumented socket. */
enum PSI_socket_operation
{
  /** Socket creation, as in @c socket() or @c socketpair(). */
  PSI_SOCKET_CREATE= 0,
  /** Socket connection, as in @c connect(), @c listen() and @c accept(). */
  PSI_SOCKET_CONNECT= 1,
  /** Socket bind, as in @c bind(), @c getsockname() and @c getpeername(). */
  PSI_SOCKET_BIND= 2,
  /** Socket close, as in @c shutdown(). */
  PSI_SOCKET_CLOSE= 3,
  /** Socket send, @c send(). */
  PSI_SOCKET_SEND= 4,
  /** Socket receive, @c recv(). */
  PSI_SOCKET_RECV= 5,
  /** Socket send, @c sendto(). */
  PSI_SOCKET_SENDTO= 6,
  /** Socket receive, @c recvfrom). */
  PSI_SOCKET_RECVFROM= 7,
  /** Socket send, @c sendmsg(). */
  PSI_SOCKET_SENDMSG= 8,
  /** Socket receive, @c recvmsg(). */
  PSI_SOCKET_RECVMSG= 9,
  /** Socket seek, such as @c fseek() or @c seek(). */
  PSI_SOCKET_SEEK= 10,
  /** Socket options, as in @c getsockopt() and @c setsockopt(). */
  PSI_SOCKET_OPT= 11,
  /** Socket status, as in @c sockatmark() and @c isfdtype(). */
  PSI_SOCKET_STAT= 12,
  /** Socket shutdown, as in @c shutdown(). */
  PSI_SOCKET_SHUTDOWN= 13,
  /** Socket select, as in @c select() and @c poll(). */
  PSI_SOCKET_SELECT= 14
};
typedef enum PSI_socket_operation PSI_socket_operation;

/**
  Socket instrument information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented socket.
*/
struct PSI_socket_info_v1
{
  /**
    Pointer to the key assigned to the registered socket.
  */
  PSI_socket_key *m_key;
  /**
    The name of the socket instrument to register.
  */
  const char *m_name;
  /**
    The flags of the socket instrument to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};
typedef struct PSI_socket_info_v1 PSI_socket_info_v1;

/**
  State data storage for @c start_socket_wait_v1_t.
  This structure provide temporary storage to a socket locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_socket_wait_v1_t
*/
struct PSI_socket_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current socket. */
  struct PSI_socket *m_socket;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Operation number of bytes. */
  size_t m_number_of_bytes;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Current operation. */
  enum PSI_socket_operation m_operation;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state_v1;

/**
  Socket registration API.
  @param category a category name (typically a plugin name)
  @param info an array of socket info to register
  @param count the size of the info array
*/
typedef void (*register_socket_v1_t)
  (const char *category, struct PSI_socket_info_v1 *info, int count);

/**
  Socket instrumentation initialisation API.
  @param key the registered socket key
  @param fd socket file descriptor
  @param addr the socket ip address
  @param addr_len length of socket ip address
  @return an instrumented socket
*/
typedef struct PSI_socket* (*init_socket_v1_t)
  (PSI_socket_key key, const my_socket *fd,
  const struct sockaddr *addr, socklen_t addr_len);

/**
  socket instrumentation destruction API.
  @param socket the socket to destroy
*/
typedef void (*destroy_socket_v1_t)(struct PSI_socket *socket);

/**
  Record a socket instrumentation start event.
  @param state data storage for the locker
  @param socket the instrumented socket
  @param op socket operation to be performed
  @param count the number of bytes requested, or 0 if not applicable
  @param src_file the source file name
  @param src_line the source line number
  @return a socket locker, or NULL
*/
typedef struct PSI_socket_locker* (*start_socket_wait_v1_t)
  (struct PSI_socket_locker_state_v1 *state,
   struct PSI_socket *socket,
   enum PSI_socket_operation op,
   size_t count,
   const char *src_file, uint src_line);

/**
  Record a socket instrumentation end event.
  Note that for socket close operations, the instrumented socket handle
  associated with the socket (which was provided to obtain a locker)
  is invalid after this call.
  @param locker a socket locker for the running thread
  @param count the number of bytes actually used in the operation,
  or 0 if not applicable, or -1 if the operation failed
  @sa get_thread_socket_locker
*/
typedef void (*end_socket_wait_v1_t)
  (struct PSI_socket_locker *locker, size_t count);

/**
  Set the socket state for an instrumented socket.
  @param socket the instrumented socket
  @param state socket state
*/
typedef void (*set_socket_state_v1_t)(struct PSI_socket *socket,
                                      enum PSI_socket_state state);

/**
  Set the socket info for an instrumented socket.
  @param socket the instrumented socket
  @param fd the socket descriptor
  @param addr the socket ip address
  @param addr_len length of socket ip address
*/
typedef void (*set_socket_info_v1_t)(struct PSI_socket *socket,
                                     const my_socket *fd,
                                     const struct sockaddr *addr,
                                     socklen_t addr_len);

/**
  Bind a socket to the thread that owns it.
  @param socket instrumented socket
*/
typedef void (*set_socket_thread_owner_v1_t)(struct PSI_socket *socket);

/**
  Performance Schema Socket Interface, version 1.
  @since PSI_SOCKET_VERSION_1
*/
struct PSI_socket_service_v1
{
  /** @sa register_socket_v1_t. */
  register_socket_v1_t register_socket;
  /** @sa init_socket_v1_t. */
  init_socket_v1_t init_socket;
  /** @sa destroy_socket_v1_t. */
  destroy_socket_v1_t destroy_socket;
  /** @sa start_socket_wait_v1_t. */
  start_socket_wait_v1_t start_socket_wait;
  /** @sa end_socket_wait_v1_t. */
  end_socket_wait_v1_t end_socket_wait;
  /** @sa set_socket_state_v1_t. */
  set_socket_state_v1_t set_socket_state;
  /** @sa set_socket_info_v1_t. */
  set_socket_info_v1_t set_socket_info;
  /** @sa set_socket_thread_owner_v1_t. */
  set_socket_thread_owner_v1_t set_socket_thread_owner;
};

#endif /* HAVE_PSI_SOCKET_1 */

/* Export the required version */
#ifdef USE_PSI_SOCKET_1
typedef struct PSI_socket_service_v1 PSI_socket_service_t;
typedef struct PSI_socket_info_v1 PSI_socket_info;
typedef struct PSI_socket_locker_state_v1 PSI_socket_locker_state;
#else
typedef struct PSI_placeholder PSI_socket_service_t;
typedef struct PSI_placeholder PSI_socket_info;
typedef struct PSI_placeholder PSI_socket_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_socket_service_t *psi_socket_service;

/** @} (end of group psi_abi_socket) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_SOCKET_H */

