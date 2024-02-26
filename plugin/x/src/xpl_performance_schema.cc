/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/xpl_performance_schema.h"

#include "plugin/x/src/ngs/memory.h"

#ifdef HAVE_PSI_INTERFACE

PSI_thread_key KEY_thread_x_acceptor;
PSI_thread_key KEY_thread_x_worker;

static PSI_thread_info all_x_threads[] = {
    {&KEY_thread_x_acceptor, "acceptor_network", "xpl_accept",
     PSI_FLAG_AUTO_SEQNUM, 0, PSI_DOCUMENT_ME},
    {&KEY_thread_x_worker, "worker", "xpl_worker",
     PSI_FLAG_USER | PSI_FLAG_AUTO_SEQNUM, 0, PSI_DOCUMENT_ME},
};

PSI_mutex_key KEY_mutex_x_lock_list_access;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_worker_pending;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_thread_exit;
PSI_mutex_key KEY_mutex_x_document_id_generate;
PSI_mutex_key KEY_mutex_x_notice_output_queue;
PSI_mutex_key KEY_mutex_x_client_session_exit;
PSI_mutex_key KEY_mutex_x_socket_events_timers;
PSI_mutex_key KEY_mutex_x_scheduler_post;
PSI_mutex_key KEY_mutex_x_server_client_exit;
PSI_mutex_key KEY_mutex_x_vio_shutdown;
PSI_mutex_key KEY_mutex_x_page_pool;
PSI_mutex_key KEY_mutex_x_listener_tcp_sync;
PSI_mutex_key KEY_mutex_x_listener_unix_socket_sync;
PSI_mutex_key KEY_mutex_x_broker_context_sync;
PSI_mutex_key KEY_mutex_x_server_state_sync;
PSI_mutex_key KEY_mutex_x_socket_acceptors_sync;

static PSI_mutex_info all_x_mutexes[] = {
    {&KEY_mutex_x_lock_list_access, "lock_list_access", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_scheduler_dynamic_worker_pending,
     "scheduler_dynamic_worker_pending", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_scheduler_dynamic_thread_exit,
     "scheduler_dynamic_thread_exit", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_document_id_generate, "document_id_generate", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_notice_output_queue, "notice_output_queue", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_client_session_exit, "client_session_exit", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_socket_events_timers, "socket_events_timers", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_scheduler_post, "scheduler_post", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_server_client_exit, "server_client_exit", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_vio_shutdown, "vio_shutdown", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_page_pool, "page_pool", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_mutex_x_listener_tcp_sync, "listener_tcp_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_listener_unix_socket_sync, "listener_unix_socket_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_broker_context_sync, "broker_context_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_server_state_sync, "server_state_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_mutex_x_socket_acceptors_sync, "socket_acceptors_sync", 0, 0,
     PSI_DOCUMENT_ME},
};

PSI_cond_key KEY_cond_x_scheduler_dynamic_worker_pending;
PSI_cond_key KEY_cond_x_scheduler_dynamic_thread_exit;
PSI_cond_key KEY_cond_x_listener_tcp_sync;
PSI_cond_key KEY_cond_x_listener_unix_socket_sync;
PSI_cond_key KEY_cond_x_broker_context_sync;
PSI_cond_key KEY_cond_x_server_state_sync;
PSI_cond_key KEY_cond_x_socket_acceptors_sync;

static PSI_cond_info all_x_conds[] = {
    {&KEY_cond_x_scheduler_dynamic_worker_pending,
     "scheduler_dynamic_worker_pending", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_cond_x_scheduler_dynamic_thread_exit, "scheduler_dynamic_thread_exit",
     0, 0, PSI_DOCUMENT_ME},
    {&KEY_cond_x_listener_tcp_sync, "listener_tcp_sync", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_cond_x_listener_unix_socket_sync, "listener_unix_socket_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_cond_x_broker_context_sync, "broker_context_sync", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_cond_x_server_state_sync, "server_state_sync", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_cond_x_socket_acceptors_sync, "socket_acceptors_sync", 0, 0,
     PSI_DOCUMENT_ME},
};

PSI_rwlock_key KEY_rwlock_x_client_list_clients;
PSI_rwlock_key KEY_rwlock_x_sha256_password_cache;
PSI_rwlock_key KEY_rwlock_x_xpl_server_instance;

static PSI_rwlock_info all_x_rwlocks[] = {
    {&KEY_rwlock_x_client_list_clients, "client_list_clients", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_rwlock_x_sha256_password_cache, "sha256_password_cache", 0, 0,
     PSI_DOCUMENT_ME},
    {&KEY_rwlock_x_xpl_server_instance, "xpl_server_instance", 0, 0,
     PSI_DOCUMENT_ME},
};

PSI_socket_key KEY_socket_x_tcpip;
PSI_socket_key KEY_socket_x_diagnostics;
PSI_socket_key KEY_socket_x_unix;
PSI_socket_key KEY_socket_x_client_connection;

#ifdef HAVE_PSI_SOCKET_INTERFACE

static PSI_socket_info all_x_sockets[] = {
    {&KEY_socket_x_tcpip, "tcpip_socket", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_socket_x_diagnostics, "diagnostics_socket", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_socket_x_unix, "unix_socket", 0, 0, PSI_DOCUMENT_ME},
    {&KEY_socket_x_client_connection, "client_connection", 0, 0,
     PSI_DOCUMENT_ME},
};

#endif  // HAVE_PSI_SOCKET_INTERFACE

PSI_memory_key KEY_memory_x_objects = PSI_NOT_INSTRUMENTED;
PSI_memory_key KEY_memory_x_recv_buffer = PSI_NOT_INSTRUMENTED;
PSI_memory_key KEY_memory_x_send_buffer = PSI_NOT_INSTRUMENTED;

static PSI_memory_info all_x_memory[] = {
    {&KEY_memory_x_objects, "objects", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&KEY_memory_x_recv_buffer, "recv_buffer", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&KEY_memory_x_send_buffer, "send_buffer", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
};

#endif  // HAVE_PSI_INTERFACE

namespace xpl {

void init_performance_schema() {
#ifdef HAVE_PSI_INTERFACE

  const char *const category = "mysqlx";

  mysql_thread_register(category, all_x_threads,
                        static_cast<int>(array_elements(all_x_threads)));
  mysql_mutex_register(category, all_x_mutexes,
                       static_cast<int>(array_elements(all_x_mutexes)));
  mysql_cond_register(category, all_x_conds,
                      static_cast<int>(array_elements(all_x_conds)));
  mysql_rwlock_register(category, all_x_rwlocks,
                        static_cast<int>(array_elements(all_x_rwlocks)));
  mysql_socket_register(category, all_x_sockets,
                        static_cast<int>(array_elements(all_x_sockets)));
  mysql_memory_register(category, all_x_memory,
                        static_cast<int>(array_elements(all_x_memory)));
#endif  // HAVE_PSI_INTERFACE
}

}  // namespace xpl
