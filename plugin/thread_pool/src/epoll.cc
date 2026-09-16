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

  @brief Pool of Threads low level using epoll implementation.

  Schedule threads with epoll(7) (GNU/Linux 2.6.2 and later)

  This scheduler uses epoll(7) to handle I/O event notification.

  Each epoll group has multiple threads that can wait on epoll and/or process
  the connection including: reading the request, processing the request, and
  sending the result set back to the client.

  Incoming requests are handled fully by an individual thread by having
  the file descriptor pulled out of the epoll instance through usage of
  the EPOLLONESHOT flag in epoll.
*/

#include "my_config.h"

#include <errno.h>
#include <limits.h>
#include <linux/version.h>  // Check for versions where CTL_MOD is safe
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <algorithm>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
/* General includes */
#include <mysql/components/services/log_builtins.h>
#include "include/mysql/service_mysql_alloc.h"
#include "my_sys.h"
#include "mysql/plugin.h"  // thd_get_thread_id()
#include "plugin/thread_pool/src/events.h"
#include "plugin/thread_pool/src/methods.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "template_utils.h"  // pointer_cast

/* Defines */

/*
  Hint for how many file descriptors will be maintained in epool.
  See epoll_create(2).
*/
static decltype(auto) EPOLL_CREATE_HINT() {
  return (get_max_connections() / thread_pool_size);
}
constexpr auto EPOLL_CREATE_HINT_MIN = 16;

PSI_memory_key key_memory_tp_group_low_level;
PSI_memory_key key_memory_tp_client_low_level;

/*
 In older Linux kernels, EPOLL_CTL_MOD has a race condition. Check the
 Linux kernel version we are compiling against to verify that it is safe.
*/
static_assert(LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0));

/*
  pool of threads low level GROUP implementation using epoll
*/

struct tp_group_low_level_t {
  my_socket fd;
  my_socket notify_fd;
  tp_client_low_level_t *wait_cntx;
  tp_group_t *tp_group;
};

tp_group_low_level_t *tp_group_low_level_init(tp_group_t *tp_group) {
  int hint;
  tp_group_low_level_t *group_cntx;
  my_socket poll_group_fds[2];

  group_cntx = (tp_group_low_level_t *)my_malloc(key_memory_tp_group_low_level,
                                                 sizeof(tp_group_low_level_t),
                                                 MYF(MY_ZEROFILL));

  if (!group_cntx) {
    /* No error printout attempted when out of memory */
    return nullptr;
  }

  /*
    We need to have link from low level group to the high level group.
    The low level group contains the epoll socket which we perform
    epoll_wait on, we need to hint the OS about how many sockets it
    will contain in a normal case.

    Part of the low level group we also need a socketpair, we handle
    this by using a low level client context which we call wait context.
  */
  group_cntx->tp_group = tp_group;
  hint = EPOLL_CREATE_HINT();
  hint = std::max(hint, EPOLL_CREATE_HINT_MIN);
  hint = std::min(hint, MAX_THREADS_PER_GROUP);
  group_cntx->fd = epoll_create(hint);

  if (group_cntx->fd != TP_INVALID_SOCKET) {
    group_cntx->wait_cntx = tp_client_low_level_init(nullptr, group_cntx);
    if (!group_cntx->wait_cntx) {
      LogErr(WARNING_LEVEL, ER_THREAD_POOL_ALLOC_FAILED,
             "tp_client_low_level_t");
      goto error;
    }

    /* Needed for for threads to notify a waiter to exit */
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, poll_group_fds) == 0) {
      group_cntx->notify_fd = poll_group_fds[0];
      group_cntx->wait_cntx->fd = poll_group_fds[1];
      if (tp_client_low_level_arm(group_cntx->wait_cntx)) {
        int error = socket_errno;
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_ARM_FAILED_WITH_ERRNO,
               error);
        goto error;
      }
      return group_cntx;
    } else {
      int error = socket_errno;
      LogErr(WARNING_LEVEL, ER_THREAD_POOL_SOCKETPAIR_FAILED, error);
      goto error;
    }
  } else {
    int error = socket_errno;
    LogErr(WARNING_LEVEL, ER_THREAD_POOL_CREATE_EPOLL_FAILED, error);
    my_free(group_cntx);
    return nullptr;
  }
  return group_cntx;

error:
  if (group_cntx->wait_cntx) tp_client_low_level_end(group_cntx->wait_cntx);
  closesocket(group_cntx->fd);
  my_free(group_cntx);
  return nullptr;
}

void tp_group_low_level_end(tp_group_low_level_t *group_cntx) {
  if (group_cntx->notify_fd != TP_INVALID_SOCKET)
    closesocket(group_cntx->notify_fd);
  if (group_cntx->wait_cntx->fd != TP_INVALID_SOCKET)
    closesocket(group_cntx->wait_cntx->fd);

  tp_client_low_level_end(group_cntx->wait_cntx);
  if (group_cntx->fd != TP_INVALID_SOCKET) closesocket(group_cntx->fd);

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
void tp_group_low_level_waiter_flush(tp_group_low_level_t *group_cntx) {
  if (tp_client_low_level_rearm(group_cntx->wait_cntx,
                                group_cntx->wait_cntx->group->tp_group)) {
    /*
      There is no remedy to a failure here. This means that we can't
      rearm the wait context, this is an important part of the
      workings of this thread group. If it doesn't work we can no
      longer KILL queries and also we would be unable to exit the
      server in a controlled fashion. The epoll rearm will retry once
      per second for 30 seconds before returning this error.
    */
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_LOW_LEVEL_REARM_FAILED);
    abort();
  }
}

/**
  Handle multiple events received from epoll_wait

  @param             group_cntx        Low level group context
  @param             next_events       Array of client contexts with query ready
  @param             events_total      Total number of events
  @param             event_list        Events received from epoll_wait

  @retval            Number of client contexts ready to execute >= 0

  @note
    This function is used to handle 0, 1 or many events (maximum 32) from
    epoll_wait. We'll fill the next_events array with the client contexts
    reported to be ready to execute. Events can also arrive on internal
    socket. We need to gather all events before we call the waiter flush
    method handling since there might be events in the event_list which
    should be ignored since the connection has been killed by an event
    which will be discovered by the waiter flush handling.
*/
static int handle_multiple_events(tp_group_low_level_t *group_cntx,
                                  tp_client_low_level_t **next_events,
                                  int events_total,
                                  struct epoll_event *event_list) {
  bool waiter_flush_flag = false;
  int added_events = 0;
  tp_client_low_level_t *client_cntx;
  DBUG_TRACE;

  /* Put all clients that are ready into the "ready" list */
  for (int i = 0; i < events_total; i++) {
    client_cntx = (tp_client_low_level_t *)event_list[i].data.ptr;

    /* We are being notified through notify_fd */
    if (client_cntx == group_cntx->wait_cntx)
      waiter_flush_flag = true;
    else {
      insert_next_events(next_events, &added_events, client_cntx);
    }
  }
  handle_waiter_flush(group_cntx, next_events, &added_events,
                      group_cntx->wait_cntx->fd, waiter_flush_flag);
  return added_events;
}

/**
  Handle a single event from epoll_wait by direct query execution

  @param           group_cntx          Low level group context
  @param           my_thread_data      Thread data
  @param           my_tp_group         Thread group data

  @note
    This function is called when there is only one event and it is
    possible for low level functions to execute immediately. The
    event could also be an event on the internal socket used for
    internal communication.
*/
static void handle_single_event(tp_group_low_level_t *group_cntx,
                                tp_thread_t *my_thread_data,
                                tp_group_t *my_tp_group) {
  assert(my_thread_data->client_low_level_cntx != nullptr);
  if (my_thread_data->client_low_level_cntx == group_cntx->wait_cntx) {
    waiter_flush(group_cntx, nullptr, nullptr, group_cntx->wait_cntx->fd);
    my_thread_data->client_low_level_cntx = nullptr;
  } else {
    process_direct_query(my_thread_data, my_tp_group);
  }
}

/**
    Check for expected errors from epoll_wait().
 */
void handle_epoll_wait_error() {
  assert(errno == EINTR);
  LogErr(WARNING_LEVEL, ER_THREAD_POOL_EPOLL_WAIT_ERROR, errno);
}

int tp_group_low_level_wait_for_events(tp_group_low_level_t *group_cntx,
                                       tp_group_t *my_tp_group,
                                       tp_thread_t *my_thread_data,
                                       tp_client_low_level_t **next_events,
                                       bool wait) {
  int events_total;
  int timeout = wait ? -1 : 0; /* -1 = Indefinite wait, 0 = no wait */
  int loop = wait ? 1000 : 1; /* 1 = single loop without wait, 1000=arbitrary */
  struct epoll_event event_list[MAX_EVENTS_PER_WAIT_CALL];
  DBUG_TRACE;
  DBUG_PRINT("tp_enter",
             ("Thread group id = %d, wait = %d", my_tp_group->group_idx, wait));

  while (continue_wait_events(my_tp_group, my_thread_data, loop)) {
    loop--;
    /* Wait for file descriptors to have something of interest */
    DBUG_PRINT("tp",
               ("epoll_wait in Thread group id %d", my_tp_group->group_idx));
    events_total = epoll_wait(group_cntx->fd, event_list,
                              MAX_EVENTS_PER_WAIT_CALL, timeout);
    if (is_tp_shutdown()) break;
    if (events_total < 0) {
      handle_epoll_wait_error();
      continue;
    }

    Time_pt time_of_events = tp_now();
    std::for_each_n(event_list, events_total, [&](const epoll_event &ev) {
      tp_client_low_level_t *client_cntx =
          pointer_cast<tp_client_low_level_t *>(ev.data.ptr);
      client_cntx->time_of_event_arrival = time_of_events;
    });

    if (can_process_direct(my_tp_group, events_total, wait)) {
      my_thread_data->client_low_level_cntx =
          pointer_cast<tp_client_low_level_t *>(event_list[0].data.ptr);
#ifndef NDEBUG
      event_list[0].data.ptr = nullptr;  // Avoid dangling
#endif                                   /* NDEBUG */
      handle_single_event(group_cntx, my_thread_data, my_tp_group);
    } else {
      assert(events_total <= MAX_EVENTS_PER_WAIT_CALL);

      /*
        When highly concurrent query processing mode is used, quickly check if
        more events are available. Events are concurrently processed by the
        multiple query worker threads.
      */
      if (events_total == 1 && in_highly_concurrent_query_processing_mode()) {
        int new_events = epoll_wait(group_cntx->fd, event_list + 1,
                                    MAX_EVENTS_PER_WAIT_CALL - 1, 0);
        if (new_events > 0) events_total += new_events;
        std::for_each_n(event_list + 1, new_events, [&](const epoll_event &ev) {
          tp_client_low_level_t *client_cntx =
              pointer_cast<tp_client_low_level_t *>(ev.data.ptr);
          client_cntx->time_of_event_arrival = time_of_events;
        });
      }

      return handle_multiple_events(group_cntx, next_events, events_total,
                                    &event_list[0]);
    }
  }
  return 0;
}

tp_client_low_level_t *tp_client_low_level_init(
    THD *thd, tp_group_low_level_t *group_cntx) {
  tp_client_low_level_t *client_cntx = nullptr;

  if (!(client_cntx = (tp_client_low_level_t *)my_malloc(
            key_memory_tp_client_low_level,
            sizeof(struct tp_client_low_level_t), MYF(MY_ZEROFILL))))
    return nullptr;

  client_cntx->thd = thd;
  client_cntx->group = group_cntx;
  if (thd != nullptr) {
    client_cntx->fd = thd_get_fd(thd);
    client_cntx->connection_id =
        thd_get_thread_id(pointer_cast<MYSQL_THD>(thd));
  } else {
    client_cntx->fd = TP_INVALID_SOCKET;
    client_cntx->connection_id = 0;
  }
  client_cntx->ev.data.ptr = client_cntx;
  client_cntx->epoll_fd = client_cntx->group->fd;
  client_cntx->expiry_timpt = Time_pt::max();
  return client_cntx;
}

/**
  Handle epoll_ctl call

  @param            client_cntx        Low level client context
  @param            op                 epoll_ctl operation (ADD/MOD)
  @param            arm                Setup for arm/disarm

  @retval           true epoll_ctl returned an error
  @retval           false epoll_ctl returned success
*/
static bool handle_epoll_ctl(tp_client_low_level_t *client_cntx,
                             tp_group_t *my_tp_group, int op, bool arm) {
  bool flag = false;

  if (op == EPOLL_CTL_MOD) {
    mysql_mutex_lock(&my_tp_group->LOCK_group);
    if (client_cntx->connection_killed_state == TP_CONNECTION_NOT_KILLED) {
      /*
        If the connection is about to be closed there is no need to
        rearm it, also rearming it now means we can potentially try
        to rearm a connection that is already closed and even operate
        on the wrong socket connection.
      */
      client_cntx->ev.events = EPOLLIN | EPOLLONESHOT;
      if (epoll_ctl(client_cntx->epoll_fd, EPOLL_CTL_MOD, client_cntx->fd,
                    &(client_cntx->ev)) < 0)
        flag = true;
    }
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
  } else {
    client_cntx->ev.events = arm ? EPOLLIN | EPOLLONESHOT : 0;
    if (epoll_ctl(client_cntx->epoll_fd, op, client_cntx->fd,
                  &(client_cntx->ev)) < 0)
      flag = true;
  }
  return flag;
}

bool tp_client_low_level_arm(tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;

  if (handle_epoll_ctl(client_cntx, nullptr, EPOLL_CTL_ADD, true)) {
    DBUG_PRINT("tp", ("epoll_ctl(...,EPOLL_CTL_ADD) arm failed: error %d",
                      socket_errno));
    return true;
  }
  return false;
}

bool tp_client_low_level_rearm(tp_client_low_level_t *client_cntx,
                               tp_group_t *my_tp_group) {
  uint loop = 0;
  DBUG_TRACE;
  assert(client_cntx);
  assert(my_tp_group);
loop:

  if (handle_epoll_ctl(client_cntx, my_tp_group, EPOLL_CTL_MOD, true)) {
    /*
      EBADF means the socket has gone away, we don't worry about this
      for normal connections, for notify sockets it's not ok. For normal
      connections it could simply mean that the connection has been closed
      by us or other side.
    */
    if (socket_errno != EBADF)
      DBUG_PRINT("tp", ("epoll_ctl(...,EPOLL_CTL_MOD) rearm failed: error %d",
                        socket_errno));
    if (my_tp_group->group_low_level_cntx->wait_cntx != client_cntx &&
        ++loop < 30) {
      /*
        We have no remedy for an error here, so we retry for 30 seconds
        before we decide to exit the MySQL Server.
      */
      sleep(1);
      goto loop;
    }
    return true;
  }
  return false;
}

bool tp_client_low_level_disarm(tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  assert(client_cntx);

  if (handle_epoll_ctl(client_cntx, nullptr, EPOLL_CTL_DEL, false)) {
    DBUG_PRINT("tp", ("epoll_ctl(...,EPOLL_CTL_DEL) disarm failed: error %d",
                      socket_errno));
    return true;
  }
  return false;
}

void tp_client_low_level_end(tp_client_low_level_t *client_cntx) {
  /*
    We don't need to delete as the kernel does that for us.
    We have always closed the socket before calling this and closing
    the socket also removes it from any epoll sockets.
    e.g. No need to call
    epoll_ctl(fd, EPOLL_CTL_DEL, tp_client_low_level_cntx->fd);
  */
  my_free(client_cntx);
}
