/*
* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of the
* License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301  USA
*/

#include "xpl_performance_schema.h"
#include "ngs/memory.h"


#ifdef HAVE_PSI_INTERFACE

PSI_thread_key KEY_thread_x_acceptor = PSI_NOT_INSTRUMENTED;
PSI_thread_key KEY_thread_x_worker = PSI_NOT_INSTRUMENTED;

static PSI_thread_info all_x_threads[] = {
  { &KEY_thread_x_acceptor, "acceptor_network", 0 },
  { &KEY_thread_x_worker,   "worker", 0 },
};

PSI_mutex_key KEY_mutex_x_lock_list_access = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_worker_pending = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_thread_exit = PSI_NOT_INSTRUMENTED;

static PSI_mutex_info all_x_mutexes[] = {
  { &KEY_mutex_x_lock_list_access, "lock_list_access", 0 },
  { &KEY_mutex_x_scheduler_dynamic_worker_pending, "scheduler_dynamic_worker_pending", 0 },
  { &KEY_mutex_x_scheduler_dynamic_thread_exit, "scheduler_dynamic_thread_exit", 0 },
};

PSI_cond_key KEY_cond_x_scheduler_dynamic_worker_pending = PSI_NOT_INSTRUMENTED;
PSI_cond_key KEY_cond_x_scheduler_dynamic_thread_exit = PSI_NOT_INSTRUMENTED;

static PSI_cond_info all_x_conds[] = {
  { &KEY_cond_x_scheduler_dynamic_worker_pending, "scheduler_dynamic_worker_pending", 0 },
  { &KEY_cond_x_scheduler_dynamic_thread_exit, "scheduler_dynamic_thread_exit", 0 },
};


PSI_rwlock_key KEY_rwlock_x_client_list_clients = PSI_NOT_INSTRUMENTED;

static PSI_rwlock_info all_x_rwlocks[] = {
  { &KEY_rwlock_x_client_list_clients, "client_list_clients", 0 },
};


PSI_socket_key KEY_socket_x_tcpip = PSI_NOT_INSTRUMENTED;
PSI_socket_key KEY_socket_x_unix = PSI_NOT_INSTRUMENTED;
PSI_socket_key KEY_socket_x_client_connection = PSI_NOT_INSTRUMENTED;

#ifdef HAVE_PSI_SOCKET_INTERFACE

static PSI_socket_info all_x_sockets[] =
{
  { &KEY_socket_x_tcpip, "tcpip_socket", 0 },
  { &KEY_socket_x_unix, "unix_socket", 0 },
  { &KEY_socket_x_client_connection, "client_connection", 0 },

};

#endif // HAVE_PSI_SOCKET_INTERFACE

PSI_memory_key KEY_memory_x_objects = PSI_NOT_INSTRUMENTED;
PSI_memory_key KEY_memory_x_recv_buffer = PSI_NOT_INSTRUMENTED;
PSI_memory_key KEY_memory_x_send_buffer = PSI_NOT_INSTRUMENTED;

static PSI_memory_info all_x_memory[] =
{
    { &KEY_memory_x_objects, "objects", PSI_FLAG_GLOBAL },
    { &KEY_memory_x_recv_buffer, "recv_buffer", PSI_FLAG_GLOBAL },
    { &KEY_memory_x_send_buffer, "send_buffer", PSI_FLAG_GLOBAL },
};

#endif // HAVE_PSI_INTERFACE


void xpl_init_performance_schema()
{
#ifdef HAVE_PSI_INTERFACE

  const char * const category = "mysqlx";

  mysql_thread_register(category, all_x_threads, array_elements(all_x_threads));
  mysql_mutex_register(category, all_x_mutexes, array_elements(all_x_mutexes));
  mysql_cond_register(category, all_x_conds, array_elements(all_x_conds));
  mysql_rwlock_register(category, all_x_rwlocks, array_elements(all_x_rwlocks));
  mysql_socket_register(category, all_x_sockets, array_elements(all_x_sockets));
  mysql_memory_register(category, all_x_memory, array_elements(all_x_memory));

  ngs::x_psf_objects_key = KEY_memory_x_objects;

#endif // HAVE_PSI_INTERFACE
}
