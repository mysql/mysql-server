/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; version 2 of the
License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef MYSQL_SOCKET_H
#define MYSQL_SOCKET_H

/* For strlen() */
#include <string.h>
/* For MY_STAT */
#include <my_dir.h>
/* For my_chsize */
#include <my_sys.h>
/* For socket api */
//#include <sys/socket.h>
#include <netinet/in.h>
/**
  @file mysql/psi/mysql_socket.h
[...]
*/

#include "mysql/psi/psi.h"

/**
  @defgroup Socket_instrumentation Socket Instrumentation
  @ingroup Instrumentation_interface
  @{
*/

/**
  @def MYSQL_SOCKET_WAIT_VARIABLES
  Instrumentation helper for socket waits.
  This instrumentation declares local variables.
  Do not use a ';' after this macro
  @param LOCKER the locker
  @param STATE the locker state
  @sa MYSQL_START_SOCKET_WAIT.
  @sa MYSQL_END_SOCKET_WAIT.
*/
#ifdef HAVE_PSI_INTERFACE
  #define MYSQL_SOCKET_WAIT_VARIABLES(LOCKER, STATE) \
    struct PSI_socket_locker* LOCKER; \
    PSI_socket_locker_state STATE;
#else
  #define MYSQL_SOCKET_WAIT_VARIABLES(LOCKER, STATE)
#endif

/**
  @def MYSQL_START_SOCKET_WAIT
  Instrumentation helper for socket waits.
  This instrumentation marks the start of a wait event.
  @param LOCKER the locker
  @param STATE the locker state
  @param PSI The instrumented socket
  @param OP The socket operation to be performed
  @param FLAGS Per socket operation flags.
  @param COUNT Bytes written or -1
  @sa MYSQL_END_SOCKET_WAIT.
*/
#ifdef HAVE_PSI_INTERFACE
  #define MYSQL_START_SOCKET_WAIT(LOCKER, STATE, PSI, OP, COUNT) \
    LOCKER= inline_mysql_start_socket_wait(STATE, PSI, OP, COUNT,\
                                           __FILE__, __LINE__)
#else
  #define MYSQL_START_SOCKET_WAIT(LOCKER, STATE, PSI, OP, COUNT) \
    do {} while (0)
#endif

/**
  @def MYSQL_END_SOCKET_WAIT
  Instrumentation helper for socket waits.
  This instrumentation marks the end of a wait event.
  @sa MYSQL_START_SOCKET_WAIT.
*/
#ifdef HAVE_PSI_INTERFACE
  #define MYSQL_END_SOCKET_WAIT(LOCKER, COUNT) \
    inline_mysql_end_socket_wait(LOCKER, COUNT)
#else
  #define MYSQL_END_SOCKET_WAIT(LOCKER, COUNT) \
    do {} while (0)
#endif



#ifdef HAVE_PSI_INTERFACE
/**
  Instrumentation calls for MYSQL_START_SOCKET_WAIT.
  @sa MYSQL_START_SOCKET_WAIT.
*/
static inline struct PSI_socket_locker*
inline_mysql_start_socket_wait(PSI_socket_locker_state *state,
                               struct PSI_socket *psi, enum PSI_socket_operation op,
                               size_t count,
                               const char *src_file, int src_line)
{
  struct PSI_socket_locker *locker= NULL;

  if (likely(PSI_server && psi))
  {
    locker= PSI_server->get_thread_socket_locker(state, psi, op);
    if (likely(locker != NULL))
      PSI_server->start_socket_wait(locker, count, src_file, src_line);
  }
  return locker;
}

/**
  Instrumentation calls for MYSQL_END_SOCKET_WAIT.
  @sa MYSQL_END_SOCKET_WAIT.
*/
static inline void
inline_mysql_end_socket_wait(struct PSI_socket_locker *locker, size_t count)
{
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, count);
}
#endif


#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_socket(K, D, T, P) \
    inline_mysql_socket_socket(K, D, T, P)
#else
  #define mysql_socket_socket(K, D, T, P) \
    inline_mysql_socket_socket(D, T, P)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_socketpair(K, D, T, P, FDS) \
    inline_mysql_socket_socketpair(K, D, T, P, FDS)
#else
  #define mysql_socket_socketpair(K, D, T, P, FDS) \
    inline_mysql_socket_socketpair(D, T, P, FDS)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_bind(FD, A, L) \
    inline_mysql_socket_bind(__FILE__, __LINE__, FD, A, L)
#else
  #define mysql_socket_bind(FD, A, L) \
    inline_mysql_socket_bind(FD, A, L)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_getsockname(FD, A, LP) \
    inline_mysql_socket_getsockname(__FILE__, __LINE__, FD, A, LP)
#else
  #define mysql_socket_getsockname(FD, A, LP) \
    inline_mysql_socket_getsockname(FD, A, LP)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_connect(FD, A, L) \
    inline_mysql_socket_connect(__FILE__, __LINE__, FD, A, L)
#else
  #define mysql_socket_connect(FD, A, L) \
    inline_mysql_socket_connect(FD, A, L)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_getpeername(FD, A, LP) \
    inline_mysql_socket_getpeername(__FILE__, __LINE__, FD, A, LP)
#else
  #define mysql_socket_getpeername(FD, A, LP) \
    inline_mysql_socket_getpeername(FD, A, LP)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_send(FD, B, N, FL) \
    inline_mysql_socket_send(__FILE__, __LINE__, FD, B, N, FL)
#else
  #define mysql_socket_send(FD, B, N, FL) \
    inline_mysql_socket_send(FD, B, N, FL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_recv(FD, B, N, FL) \
    inline_mysql_socket_recv(__FILE__, __LINE__, FD, B, N, FL)
#else
  #define mysql_socket_recv(FD, B, N, FL) \
    inline_mysql_socket_recv(FD, B, N, FL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_sendto(FD, B, N, FL, A, L) \
    inline_mysql_socket_sendto(__FILE__, __LINE__, FD, B, N, FL, A, L)
#else
  #define mysql_socket_sendto(FD, B, N, FL, A, L) \
    inline_mysql_socket_sendto(FD, B, N, FL, A, L)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_recvfrom(FD, B, N, FL, A, LP) \
    inline_mysql_socket_recvfrom(__FILE__, __LINE__, FD, B, N, FL, A, LP)
#else
  #define mysql_socket_recvfrom(FD, B, N, FL, A, LP) \
    inline_mysql_socket_recvfrom(FD, B, N, FL, A, LP)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_sendmsg(FD, M, FL) \
    inline_mysql_socket_sendmsg(__FILE__, __LINE__, FD, M, FL)
#else
  #define mysql_socket_sendmsg(FD, M, FL) \
    inline_mysql_socket_sendmsg(FD, M, FL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_recvmsg(FD, M, FL) \
    inline_mysql_socket_recvmsg(__FILE__, __LINE__, FD, M, FL)
#else
  #define_mysql_socket_recvmsg(FD, M, FL) \
    inline_mysql_socket_recvmsg(FD, M, FL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_getsockopt(FD, LV, ON, OP, OL) \
    inline_mysql_socket_getsockopt(__FILE__, __LINE__, FD, LV, ON, OP, OL)
#else
  #define mysql_socket_getsockopt(FD, LV, ON, OP, OL) \
    inline_mysql_socket_getsockopt(FD, LV, ON, OP, OL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_setsockopt(FD, LV, ON, OP, OL) \
    inline_mysql_socket_setsockopt(__FILE__, __LINE__, FD, LV, ON, OP, OL)
#else
  #define mysql_socket_setsockopt(FD, LV, ON, OP, OL) \
    inline_mysql_socket_setsockopt(FD, LV, ON, OP, OL)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_listen(FD, N) \
    inline_mysql_socket_listen(__FILE__, __LINE__, FD, N)
#else
  #define mysql_socket_listen(FD, N) \
    inline_mysql_socket_listen(FD, N)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_accept(K, FD, A, LP) \
    inline_mysql_socket_accept(K, __FILE__, __LINE__, FD, A, LP)
#else
  #define mysql_socket_accept(FD, A, LP) \
    inline_mysql_socket_accept(FD, A, LP)
#endif

#if 0

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_accept4(K, FD, A, LP, FL) \
    inline_mysql_socket_accept4(K, __FILE__, __LINE__, FD, A, LP, FL)
#else
  #define mysql_socket_accept4(FD, A, LP, FL) \
    inline_mysql_socket_accept4(FD, A, LP, FL)
#endif

#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_close(FD) \
    inline_mysql_socket_close(__FILE__, __LINE__, FD)
#else
  #define mysql_socket_close(FD) \
    inline_mysql_socket_close(FD)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_shutdown(FD, H) \
    inline_mysql_socket_shutdown(__FILE__, __LINE__, FD, H)
#else
  #define mysql_socket_shutdown(FD, H) \
    inline_mysql_socket_shutdown(FD, H)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_sockatmark(FD) \
    inline_mysql_socket_sockatmark(__FILE__, __LINE__, FD)
#else
  #define mysql_socket_sockatmark(FD) \
    inline_mysql_socket_sockatmark(FD)
#endif

#ifdef HAVE_PSI_INTERFACE
  #define mysql_socket_isfdtype(FD, FT) \
    inline_mysql_socket_isfdtype(__FILE__, __LINE__, FD, FT)
#else
  #define mysql_socket_isfdtype(FD, FT) \
    inline_mysql_socket_isfdtype(FD, FT)
#endif

struct st_mysql_socket
{
  /** The real socket identifier. */
  my_socket fd;

  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c MYSQL_FILE interface.
  */

  struct PSI_socket *m_psi;
};

/**
  Type of an instrumented socket.
  @c MYSQL_SOCKET is a replacement for @c my_socket.
  @sa mysql_file_open
*/

typedef struct st_mysql_socket MYSQL_SOCKET;

#define mysql_socket_getfd(FD) ((FD).fd)

/** mysql_socket_socket */

static inline MYSQL_SOCKET
inline_mysql_socket_socket
(
#ifdef HAVE_PSI_INTERFACE
  PSI_socket_key key,
#endif
  int domain, int type, int protocol)
{
  MYSQL_SOCKET mysql_socket = {0, NULL};
#ifdef HAVE_PSI_INTERFACE
  mysql_socket.m_psi = PSI_server ? PSI_server->init_socket(key, &mysql_socket.fd)
                                  : NULL;
#endif
  mysql_socket.fd= socket(domain, type, protocol);

#ifdef HAVE_PSI_INTERFACE
  if (likely(mysql_socket.m_psi != NULL))
    PSI_server->set_socket_descriptor(mysql_socket.m_psi, mysql_socket.fd);
#endif
  return mysql_socket;
}

/** mysql_socket_socketpair */

static inline int
inline_mysql_socket_socketpair
(
#ifdef HAVE_PSI_INTERFACE
  PSI_socket_key key,
#endif
  int domain, int type, int protocol, MYSQL_SOCKET mysql_socket[2])
{
  int result= 0;
  int fds[2]= {0, 0};

  mysql_socket[0].m_psi= PSI_server ? PSI_server->init_socket(key, &mysql_socket[0].fd)
                                    : NULL;
  mysql_socket[1].m_psi= PSI_server ? PSI_server->init_socket(key, &mysql_socket[1].fd)
                                    : NULL;

  result= socketpair(domain, type, protocol, fds);

  mysql_socket[0].fd = fds[0];
  mysql_socket[1].fd = fds[1];

#ifdef HAVE_PSI_INTERFACE
  if (likely(mysql_socket[0].m_psi != NULL && mysql_socket[1].m_psi != NULL))
  {
    PSI_server->set_socket_descriptor(mysql_socket[0].m_psi, fds[0]);
    PSI_server->set_socket_descriptor(mysql_socket[1].m_psi, fds[1]);
  }
#endif
  return result;
}

/** mysql_socket_bind */

static inline int
inline_mysql_socket_bind
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_SOCKET mysql_socket, const struct sockaddr *addr, socklen_t len)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_BIND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= bind(mysql_socket.fd, addr, len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL
      && result == 0))
    PSI_server->set_socket_address(mysql_socket.m_psi, addr);

  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_getsockname */

static inline int
inline_mysql_socket_getsockname
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, __SOCKADDR_ARG addr, socklen_t *len)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_BIND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= getsockname(mysql_socket.fd, addr, len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_connect */

static inline int
inline_mysql_socket_connect
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, __CONST_SOCKADDR_ARG addr, socklen_t len)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_CONNECT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= connect(mysql_socket.fd, addr, len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_getpeername */

static inline int
inline_mysql_socket_getpeername
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, __SOCKADDR_ARG addr, socklen_t *len)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_BIND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= getpeername(mysql_socket.fd, addr, len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_send */

static inline ssize_t
inline_mysql_socket_send
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, const void *buf, size_t n, int flags)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_SEND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= send(mysql_socket.fd, buf, n, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_written);
  }
#endif
  return result;
}

/** mysql_socket_recv */

static inline ssize_t
inline_mysql_socket_recv
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket,  void *buf, size_t n, int flags)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_RECV);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= recv(mysql_socket.fd, buf, n, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_read = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_read);
  }
#endif
  return result;
}

/** mysql_socket_sendto */

static inline ssize_t
inline_mysql_socket_sendto
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, const void *buf, size_t n, int flags, __CONST_SOCKADDR_ARG addr, socklen_t addr_len)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_SEND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= sendto(mysql_socket.fd, buf, n, flags, addr, addr_len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_written);
  }
#endif
  return result;
}

/** mysql_socket_recvfrom */

static inline ssize_t
inline_mysql_socket_recvfrom
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, void *buf, size_t n, int flags, __SOCKADDR_ARG addr, socklen_t *addr_len)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_RECV);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= recvfrom(mysql_socket.fd, buf, n, flags, addr, addr_len);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_read = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_read);
  }
#endif
  return result;
}

/** mysql_socket_sendmsg */

static inline ssize_t
inline_mysql_socket_sendmsg
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, const struct msghdr *message, int flags)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_SEND);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= sendmsg(mysql_socket.fd, message, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_written);
  }
#endif
  return result;
}

/** mysql_socket_recvmsg */

static inline ssize_t
inline_mysql_socket_recvmsg
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, struct msghdr *message, int flags)
{
  ssize_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_RECV);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= recvmsg(mysql_socket.fd, message, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written = (result > -1) ? result : 0;
    PSI_server->end_socket_wait(locker, bytes_written);
  }
#endif
  return result;
}

/** mysql_socket_getsockopt */

static inline int
inline_mysql_socket_getsockopt
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, int level, int optname, void *optval, socklen_t *optlen)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_OPT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= getsockopt(mysql_socket.fd, level, optname, optval, optlen);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_setsockopt */

static inline int
inline_mysql_socket_setsockopt
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, int level, int optname, const void *optval, socklen_t optlen)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_OPT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= setsockopt(mysql_socket.fd, level, optname, optval, optlen);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_listen */

static inline int
inline_mysql_socket_listen
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, int n)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_CONNECT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= listen(mysql_socket.fd, n);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_accept */

static inline MYSQL_SOCKET
inline_mysql_socket_accept
(
#ifdef HAVE_PSI_INTERFACE
  PSI_socket_key key, const char *src_file, uint src_line,
#endif
  MYSQL_SOCKET socket_listen, struct sockaddr *addr, socklen_t *addr_len)
{
  MYSQL_SOCKET socket_accept = {0, NULL};
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  socket_accept.m_psi = PSI_server ? PSI_server->init_socket(key, &socket_accept.fd)
                                   : NULL;
  if (likely(PSI_server != NULL && socket_accept.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, socket_accept.m_psi, PSI_SOCKET_CONNECT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  socket_accept.fd= accept(socket_listen.fd, addr, addr_len);

#ifdef HAVE_PSI_INTERFACE
  /** Set socket address info */
  if (likely(PSI_server != NULL && socket_accept.m_psi != NULL
      && socket_accept.fd > -1))
    PSI_server->set_socket_info(socket_accept.m_psi, socket_accept.fd, addr);

  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return socket_accept;
}

/** mysql_socket_accept4 */
#if 0

static inline MYSQL_SOCKET
inline_mysql_socket_accept4
(
#ifdef HAVE_PSI_INTERFACE
  PSI_socket_key key, const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET socket_listen, __SOCKADDR_ARG addr, socklen_t *addr_len, int flags)
{
  MYSQL_SOCKET socket_accept = {0, NULL};
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  socket_accept.m_psi = PSI_server ? PSI_server->init_socket(key, &socket_accept.fd)
                                   : NULL;
  if (likely(PSI_server != NULL && socket_accept.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(socket_accept.m_psi, PSI_SOCKET_CONNECT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  socket_accept.fd= accept4(socket_listen.fd, addr, addr_len, flags);
#ifdef HAVE_PSI_INTERFACE
  /** Set socket address info */
  if (likely(PSI_server != NULL && socket_accept.m_psi != NULL
      && socket_accept.fd > -1))
    PSI_server->set_socket_info(socket_accept.m_psi, socket_accept.fd, addr);

  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return socket_accept;
}

#endif


/** mysql_socket_close */

static inline int
inline_mysql_socket_close
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_SOCKET mysql_socket)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_CLOSE);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= closesocket(mysql_socket.fd);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_shutdown */

static inline int
inline_mysql_socket_shutdown
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_SOCKET mysql_socket, int how)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_SHUTDOWN);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= shutdown(mysql_socket.fd, how);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_sockatmark */

static inline int
inline_mysql_socket_sockatmark
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_SOCKET mysql_socket)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_STAT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= sockatmark(mysql_socket.fd);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** mysql_socket_isfdtype */

static inline int
inline_mysql_socket_isfdtype
(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
 MYSQL_SOCKET mysql_socket, int fdtype)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_socket_locker *locker= NULL;
  PSI_socket_locker_state state;

  if (likely(PSI_server != NULL && mysql_socket.m_psi != NULL))
  {
    locker= PSI_server->get_thread_socket_locker(&state, mysql_socket.m_psi, PSI_SOCKET_STAT);
    if (likely(locker !=NULL))
      PSI_server->start_socket_wait(locker, (size_t)0, src_file, src_line);
  }
#endif
  result= isfdtype(mysql_socket.fd, fdtype);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_socket_wait(locker, (size_t)0);
#endif
  return result;
}

/** @} (end of group Socket_instrumentation) */

#endif

