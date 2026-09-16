/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file

  @brief Pool of Threads low level using poll implementation.
*/

/*
  Schedule threads with poll()/WSAPoll (Windows)

  This scheduler uses poll() to handle I/O and processing of requests.

  Added minor adjustments to enable poll on Windows (using the Windows
  library function WSAPoll. winsock2 was enhanced for Vista & Windows
  Server 2008 to include this poll() implementation for scalable socket
  event management, replacing select() which shows performance degradation
  when the number of connections increases.
*/

#include <my_config.h>
/* General includes */
#include <my_sys.h>
#include <stdio.h>    // Solaris header file bug.
#include <algorithm>  // Depends on FILE (on Solaris).
#ifndef _WIN32
#include <poll.h>
#endif
#include <sys/types.h>

#include "events.h"
#include "methods.h"
#include "mysql/plugin.h"    // thd_get_thread_id()
#include "template_utils.h"  // pointer_cast
#include "thread_pool.h"

#include "include/mysql/service_mysql_alloc.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "my_dbug.h"

/* Windows winsock2 library does not include posix function socketpair */
#ifdef AF_LOCAL
constexpr auto SOCKET_PAIR_DOMAIN = AF_LOCAL;
#else
#ifdef _WIN32
constexpr auto SOCKET_PAIR_DOMAIN = AF_INET;
#else
constexpr auto SOCKET_PAIR_DOMAIN = AF_UNIX;
#endif
#endif

constexpr auto TP_POLL_WAIT_FD_INDEX = 0;
constexpr auto TP_POLL_FIRST_REAL_FD = 1;
constexpr auto TP_POLL_MAX_FDS =
    (MAX_THREADS_PER_GROUP + TP_POLL_FIRST_REAL_FD);

#ifdef _WIN32
constexpr auto TP_POLL_WAIT_EVENTS = (POLLIN);
using POLLFD_TYPE = WSAPOLLFD;
#else
constexpr auto TP_POLL_WAIT_EVENTS = (POLLIN | POLLPRI);
using POLLFD_TYPE = pollfd;
#endif

enum client_state_t { CLIENT_ARMED, CLIENT_DISARMED };

struct tp_group_low_level_t {
  tp_group_t *tp_group;
  uint last_check;
  my_socket notify_fd;
  uint max_client_index;
  client_state_t armed[TP_POLL_MAX_FDS];
  POLLFD_TYPE poll_array[TP_POLL_MAX_FDS];
  tp_client_low_level_t *low_level_cntx[TP_POLL_MAX_FDS];
};

/**
  Check if the event API is safe

  @retval true always returns true.

  @note
  For poll, this method always returns true.
  This method is introduced for epoll where
  where EPOLL_CTL_MOD can miss events. This
  method is here to avoid #ifdef in the generic
  code.
*/
bool safe_event_api_detect() { return true; }

#ifdef _WIN32
/** Wrapper around WSAPoll to make it available as poll() also on Windows. */
static decltype(auto) poll(auto a, auto b, auto c) {
  return WSAPoll(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b),
                 std::forward<decltype(c)>(c));
}

/**
  Prepare the server part for client connect

  @param ret_fd      Socket created for server part
  @param server_addr Server address of created socket

  @retval            true if failure, false if success
*/
static bool prepare_server_connect(SOCKET *ret_fd,
                                   struct sockaddr_in *server_addr) {
  struct sockaddr_in sock_addr;
  SOCKET fd;
  int error;
  int sock_addr_len = sizeof(struct sockaddr_in);

  /* Create a socket */
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) ==
      static_cast<SOCKET>(SOCKET_ERROR))
    return true;
  *ret_fd = fd;

  /* Prepare to bind to 127.0.0.1 using any port number */
  memset(&sock_addr, 0, sizeof(struct sockaddr_in));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = 0;
  sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  /* Bind using this constructed address struct */
  if ((bind(fd, (struct sockaddr *)&sock_addr, sizeof(struct sockaddr_in))) ==
      SOCKET_ERROR)
    return true;

  /* Get address struct to use for client part */
  if ((error = getsockname(fd, (struct sockaddr *)server_addr,
                           &sock_addr_len)) == SOCKET_ERROR)
    return true;

  /* Start listening on the socket created and bound to */
  if ((error = listen(fd, 1)) == SOCKET_ERROR) return true;
  /* Successful return */
  return false;
}

/**
  Connect to the prepared server on the client side

  @param ret_fd      Socket created for client part
  @param server_addr Server address

  @retval           true if failure, false if success
*/
static bool client_connect(SOCKET *ret_fd, struct sockaddr_in *server_addr) {
  SOCKET fd;
  int error;

  /* Create a socket */
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) ==
      static_cast<SOCKET>(SOCKET_ERROR))
    return true;
  *ret_fd = fd;

  /* Connect to the socket */
  if ((error = connect(fd, (struct sockaddr *)server_addr,
                       sizeof(struct sockaddr_in))) == SOCKET_ERROR)
    return true;
  /* Successful return */
  return false;
}

/**
  Finalise the connect on the server side

  @param fd         Server socket listening
  @param ret_fd     New socket created by accept

  @retval           true if failure, false if success
*/
static bool server_connect(SOCKET fd, SOCKET *ret_fd) {
  if ((*ret_fd = accept(fd, nullptr, nullptr)) ==
      static_cast<SOCKET>(SOCKET_ERROR))
    return true;
  return false;
}

/**
  Windows native implementation of socketpair

  @param fds        Socket pair created by this function

  @retval           true if failure, false if success
*/
static bool windows_socket_pair(SOCKET fds[2]) {
  SOCKET server_first_fd = INVALID_SOCKET;
  SOCKET server_fd = INVALID_SOCKET;
  SOCKET client_fd = INVALID_SOCKET;
  struct sockaddr_in server_addr;
  DWORD error;

  memset(&server_addr, 0, sizeof(struct sockaddr_in));

  if (unlikely(prepare_server_connect(&server_first_fd, &server_addr) ||
               client_connect(&client_fd, &server_addr) ||
               server_connect(server_first_fd, &server_fd))) {
    error = WSAGetLastError();
    if (server_first_fd != INVALID_SOCKET) closesocket(server_first_fd);
    if (server_fd != INVALID_SOCKET) closesocket(server_fd);
    if (client_fd != INVALID_SOCKET) closesocket(client_fd);
    WSASetLastError(error);
    return true;
  }
  fds[0] = client_fd;
  fds[1] = server_fd;
  /* Successful return */
  return false;
}

/** Wrapper which skips a lot of unnecessary parameters we're not
    interested in. */
static decltype(auto) socketpair(auto, auto, auto, auto fds) {
  return windows_socket_pair(std::forward<decltype(fds)>(fds));
}
#endif

tp_group_low_level_t *tp_group_low_level_init(tp_group_t *tp_group) {
  tp_group_low_level_t *group_cntx;
  my_socket poll_group_fds[2];

  if (!(group_cntx = (tp_group_low_level_t *)my_malloc(
            PSI_INSTRUMENT_ME, sizeof(tp_group_low_level_t), MYF(MY_ZEROFILL))))
    return nullptr;

  group_cntx->notify_fd = TP_INVALID_SOCKET;
  for (uint i = 0; i < TP_POLL_MAX_FDS; i++) {
    group_cntx->armed[i] = CLIENT_DISARMED;
    group_cntx->poll_array[i].fd = TP_INVALID_SOCKET;
  }

  group_cntx->tp_group = tp_group;
  /* Needed for for threads to notify a waiter that the FD list changed */
  if (socketpair(SOCKET_PAIR_DOMAIN, SOCK_STREAM, 0, poll_group_fds) == 0) {
    group_cntx->notify_fd = poll_group_fds[0];
    group_cntx->armed[TP_POLL_WAIT_FD_INDEX] = CLIENT_ARMED;
    group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].fd = poll_group_fds[1];
    group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].events = TP_POLL_WAIT_EVENTS;
  } else {
    LogErr(WARNING_LEVEL, ER_THREAD_POOL_SOCKETPAIR_FAILED, socket_errno);
    my_free(group_cntx);
    group_cntx = nullptr;
  }
  return group_cntx;
}

void tp_group_low_level_end(tp_group_low_level_t *group_cntx) {
  if (group_cntx->notify_fd != TP_INVALID_SOCKET)
    closesocket(group_cntx->notify_fd);
  if (group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].fd != TP_INVALID_SOCKET)
    closesocket(group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].fd);

  my_free(group_cntx);
}

/**
  Get group data given low level context.

  @param group_cntx       Low level group context

  @retval                 The thread group data
*/
tp_group_t *tp_group_low_level_get_tp_group(tp_group_low_level_t *group_cntx) {
  return group_cntx->tp_group;
}

/**
  Send wake flag to waiters

  @param group_cntx       Low level group context
  @param flag             No flag or KILL_FLAG

  @retval                 true if failure, false if success
*/
bool tp_group_low_level_waiter_wake(tp_group_low_level_t *group_cntx,
                                    char flag) {
  int res;
  DBUG_TRACE;

  /* Write a byte to wake up the waiter thread */
  res = send(group_cntx->notify_fd, &flag, 1, 0);

  return res == 1 ? false : true;
}

/**
  Flush wait context.

  @param group_cntx       Low level group context
*/
void tp_group_low_level_waiter_flush(tp_group_low_level_t *group_cntx
                                     [[maybe_unused]]) {}

/**
  Put ready clients into event list

  @param             group_cntx         Low level group context
  @param             clients_valid      Last valid client context index
  @param             events_copied      Number of events copied to next_events
  @param             next_events        Array of client contexts
  @param             max_events         Maximum number of events collected

  @note
    This method is called after a successful return from poll/WSAPoll to
    discover a number of sockets that have data received. Each socket here
    represents a MySQL connection.

    We do however limit the maximum number of events collected from one poll
    call to either 1 or 32 in this call. This means that there might be events
    ready here that we don't collect. To ensure that connections aren't treated
    in an unfair manner we've introduced the last_check variable on the low
    level group context. This keeps track of which connection was the last
    connection we checked when here last time. We will use this to continue
    the next round of checks from the same place.

    We will also ignore any events on sockets that aren't armed for the moment.
    Given that we don't synchronize arm/disarm perfectly this could
    happen in rare cases.

    When we discover an event to process, we immediately disarm it to avoid
    having to bother about this connection while it's being processed. The
    final step is to put it into the next_events array which is used to
    communicate with the higher levels of the thread pool code.
*/
static void put_ready_clients_into_event_list(
    tp_group_low_level_t *group_cntx, uint clients_valid,
    tp_client_low_level_t **next_events, int *events_copied, int max_events,
    Time_pt time_of_events) {
  uint client_index;

  /* Put all clients that are ready into the "ready" list */
  for (uint i = 0; i < clients_valid; i++) {
    if (group_cntx->last_check > clients_valid)
      group_cntx->last_check = TP_POLL_FIRST_REAL_FD;
    client_index = (group_cntx->last_check & MAX_THREADS_PER_GROUP_MASK);
    group_cntx->last_check++;
    if (client_index >= TP_POLL_FIRST_REAL_FD) {
      if ((group_cntx->armed[client_index] == CLIENT_ARMED) &&
          // poll on Windows returns POLLNVAL in revents for ignored fd values
          (group_cntx->poll_array[client_index].revents > 0) &&
          (group_cntx->poll_array[client_index].fd != TP_INVALID_SOCKET)) {
        tp_client_low_level_disarm(group_cntx->low_level_cntx[client_index]);
        group_cntx->low_level_cntx[client_index]->time_of_event_arrival =
            time_of_events;
        insert_next_events(next_events, events_copied,
                           group_cntx->low_level_cntx[client_index]);
        if (*events_copied >= max_events) return;
      }
    }
  }
}

/**
  Find last valid file descriptor

  @param       group_cntx       Low level group context

  @retval      Number of valid sockets >= 1 (always a socketpair socket)
*/
static uint find_last_valid_fd(tp_group_low_level_t *group_cntx) {
  int i;

  for (i = group_cntx->max_client_index; i > 0; i--) {
    if (group_cntx->low_level_cntx[i] != nullptr) break;
  }
  return i + 1;
}

/**
  Set up the poll_array used in poll call

  @param        group_cntx       Low level group context
  @param        clients_valid    Number of clients valid
*/
static void set_up_poll_array(tp_group_low_level_t *group_cntx,
                              uint *clients_valid) {
  uint i;

  *clients_valid = find_last_valid_fd(group_cntx);

  /* prepare poll_array for poll() call */
  for (i = *clients_valid - 1; i > 0; i--) {
    if ((group_cntx->armed[i] == CLIENT_ARMED) &&
        (group_cntx->low_level_cntx[i] != nullptr)) {
      group_cntx->poll_array[i].fd = group_cntx->low_level_cntx[i]->fd;
      group_cntx->poll_array[i].events = TP_POLL_WAIT_EVENTS;
      group_cntx->poll_array[i].revents = 0;
    } else {
      group_cntx->poll_array[i].fd = TP_INVALID_SOCKET;
      group_cntx->poll_array[i].events = 0;
      group_cntx->poll_array[i].revents = 0;
    }
  }
  group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].revents = 0;
}

/**
  Check if we received a flush event

  @param         group_cntx        Low level group context
  @param         waiter_flush_flag Set to true if flush event received
  @param         events_total      Decrease number of query events if flush
                                   event received
*/
static void check_flush_event(tp_group_low_level_t *group_cntx,
                              bool *waiter_flush_flag, int *events_total) {
  *waiter_flush_flag = false;
  if (group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].revents != 0) {
    *waiter_flush_flag = true;
    (*events_total)--;
    assert(*events_total >= 0);
  }
}

/**
    Check for expected errors from poll().
 */
void handle_poll_wait_error() {
#ifdef _WIN32
  assert(socket_errno == WSAENOBUFS || socket_errno == WSAENOTSOCK);
#else
  assert(socket_errno == EINTR || socket_errno == EAGAIN);
#endif /* _WIN32 */
  LogErr(INFORMATION_LEVEL, ER_THREAD_POOL_POLL_WAIT_ERROR, socket_errno);
}

int tp_group_low_level_wait_for_events(tp_group_low_level_t *group_cntx,
                                       tp_group_t *my_tp_group,
                                       tp_thread_t *my_thread_data,
                                       tp_client_low_level_t **next_events,
                                       bool wait) {
  int events_total;
  int max_events;
  int events_copied;
  uint clients_valid;
  int timeout = wait ? 1000 : 0;
  int loop = wait ? 1000 : 1;
  bool waiter_flush_flag;
  DBUG_TRACE;
  DBUG_PRINT("tp_enter",
             ("Thread group id = %d, wait = %d", my_tp_group->group_idx, wait));

  while (continue_wait_events(my_tp_group, my_thread_data, loop)) {
    loop--;
    set_up_poll_array(group_cntx, &clients_valid);
    /*Wait for file descriptors to have something of interest */
    DBUG_PRINT("tp",
               ("poll(..) on Thread group id %d", my_tp_group->group_idx));
    events_total = poll(group_cntx->poll_array, (int)clients_valid, timeout);
    if (is_tp_shutdown()) break;
    if (events_total < 0) {
      handle_poll_wait_error();
      continue;
    }

    Time_pt time_of_events = tp_now();
    check_flush_event(group_cntx, &waiter_flush_flag, &events_total);
    clients_valid--; /* Ignore socketpair fd */
    /**
      Put the events on MySQL connections onto the next_events array.
      We get the number of events copied in the variable events_copied.
      We will maximum get 32 events per poll call.
    */
    max_events = std::min(events_total, MAX_EVENTS_PER_WAIT_CALL);
    events_copied = 0;
    put_ready_clients_into_event_list(group_cntx, clients_valid, next_events,
                                      &events_copied, max_events,
                                      time_of_events);
    /**
      This code takes care of reading the flush events. This could
      contain some notifications of killed connections. Thus the value
      of events_copied might decrease here.
    */
    handle_waiter_flush(group_cntx, next_events, &events_copied,
                        group_cntx->poll_array[TP_POLL_WAIT_FD_INDEX].fd,
                        waiter_flush_flag);

    if (can_process_direct(my_tp_group, events_copied, true)) {
      /**
        We execute immediately and continue to serve as the waiter thread
        while doing so, if this statement execution becomes stalled some
        other thread in the group might take over this role before we
        return from this call.
      */
      my_thread_data->client_low_level_cntx = next_events[0];
      process_direct_query(my_thread_data, my_tp_group);
      continue;
    } else if (events_copied > 0) {
      return events_copied;
    }
    /**
      We didn't have any events on MySQL connections left to process
      after removing flush events and possible KILLed connections.
      We start a new poll immediately
    */
  }
  return 0;
}

tp_client_low_level_t *tp_client_low_level_init(
    THD *thd, tp_group_low_level_t *group_cntx) {
  tp_client_low_level_t *client_cntx = nullptr;
  uint i;
  tp_group_t *my_tp_group = group_cntx->tp_group;
  DBUG_TRACE;

  /* Allocate client context first assuming success */
  if (!(client_cntx = (tp_client_low_level_t *)my_malloc(
            PSI_INSTRUMENT_ME, sizeof(struct tp_client_low_level_t),
            MYF(MY_ZEROFILL))))
    return nullptr;

  client_cntx->expiry_timpt = Time_pt::max();
  client_cntx->connection_id = thd_get_thread_id(pointer_cast<MYSQL_THD>(thd));
  /* Don't start at 0 as the first few sockets are internal only */
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  for (i = TP_POLL_FIRST_REAL_FD; i < TP_POLL_MAX_FDS; i++) {
    /* Find first non-used client index */
    if (group_cntx->low_level_cntx[i] == nullptr) {
      client_cntx->thd = thd;
      client_cntx->group = group_cntx;
      client_cntx->index = i;
      client_cntx->fd = thd_get_fd(thd);

      /*
        The poll_array data and armed data is initialized at start and at
        low level end of a group context. So no need to initialize
        it again here.
      */

      group_cntx->low_level_cntx[i] = client_cntx;
      /* Update new max_client_index */
      group_cntx->max_client_index = std::max(group_cntx->max_client_index, i);
      mysql_mutex_unlock(&my_tp_group->LOCK_group);
      return client_cntx;
    }
  }
  mysql_mutex_unlock(&my_tp_group->LOCK_group);
  my_free(client_cntx);
  return nullptr;
}

bool tp_client_low_level_arm(tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  client_cntx->group->armed[client_cntx->index] = CLIENT_ARMED;
  if (tp_group_low_level_waiter_wake(client_cntx->group, 0)) {
    DBUG_PRINT("tp", ("poll arm failed: error %d", socket_errno));
    return true;
  }
  return false;
}

bool tp_client_low_level_rearm(tp_client_low_level_t *client_cntx,
                               tp_group_t *my_tp_group) {
  DBUG_TRACE;

  mysql_mutex_lock(&my_tp_group->LOCK_group);
  if (client_cntx->connection_killed_state != TP_CONNECTION_NOT_KILLED) {
    /*
      Given that we call rearm without protection, in the poll case
      it's important to not set the ARMED flag since we read this
      without protection in the waiting_thread before calling poll.
      After setting the KILL flag it is fully possible that we free the
      client context memory object when the waiting_thread tries to read
      the client context given that the connection is armed. Thus we have
      to update this with mutex protection and also checking that the
      connection isn't being killed.

      The epoll implementation lacks this problem as it's rearmed by
      making a system call to update the network subsystem data and
      then we rely on the OS to protect it's data.
    */
    DBUG_PRINT("tp", ("Connection already killed"));
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    return true;
  }
  DBUG_PRINT("tp", ("Arm client context %p in Thread group id %d", client_cntx,
                    (int)my_tp_group->group_idx));
  client_cntx->group->armed[client_cntx->index] = CLIENT_ARMED;
  mysql_mutex_unlock(&my_tp_group->LOCK_group);
  if (tp_group_low_level_waiter_wake(client_cntx->group, 0)) {
    DBUG_PRINT("tp", ("poll arm failed: error %d", socket_errno));
    return true;
  }
  DBUG_PRINT("tp", ("Arm successful"));
  return false;
}

bool tp_client_low_level_disarm(tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  client_cntx->group->armed[client_cntx->index] = CLIENT_DISARMED;
  /*
    NOTE: not calling tp_group_low_level_waiter_wake()
    There are two paths that reach this.
      1) from the tp_group_low_level_wait_for_events()
           (no need to wakeup myself).
      2) from tp_client_low_level_end()
           (wakeup call added for this case)
  */
  return false;
}

void tp_client_low_level_end(tp_client_low_level_t *client_cntx) {
  uint client_index;
  uint i;
  tp_group_low_level_t *group_cntx;
  tp_group_t *my_tp_group;
  POLLFD_TYPE *poll_client;
  DBUG_TRACE;

  tp_client_low_level_disarm(client_cntx);

  client_index = client_cntx->index;
  group_cntx = client_cntx->group;
  my_tp_group = group_cntx->tp_group;

  poll_client = &(group_cntx->poll_array[client_index]);

  poll_client->fd = TP_INVALID_SOCKET;
  poll_client->events = 0;
  poll_client->revents = 0;
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  group_cntx->low_level_cntx[client_index] = nullptr;
  if (client_index == group_cntx->max_client_index) {
    /* Find new maximum */
    for (i = group_cntx->max_client_index; i > 0; i--) {
      if (group_cntx->low_level_cntx[i] != nullptr) {
        /* Set new max_client_index */
        group_cntx->max_client_index = i;
        break;
      }
    }
  }
  mysql_mutex_unlock(&my_tp_group->LOCK_group);

  tp_group_low_level_waiter_wake(group_cntx, 0);
  my_free(client_cntx);
}
