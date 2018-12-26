/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_global.h"
#include "xpl_performance_schema.h"
#include "replication.h"
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if !defined(_WIN32)
#include <sys/utsname.h>
#endif

PSI_thread_key KEY_thread_x_worker = PSI_NOT_INSTRUMENTED;
PSI_thread_key KEY_thread_x_acceptor = PSI_NOT_INSTRUMENTED;

PSI_mutex_key KEY_mutex_x_client_session = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_obuffer = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_lock_list_access = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_worker_pending = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_scheduler_dynamic_thread_exit = PSI_NOT_INSTRUMENTED;
PSI_mutex_key KEY_mutex_x_queue = PSI_NOT_INSTRUMENTED;

PSI_cond_key KEY_cond_x_scheduler_dynamic_worker_pending = PSI_NOT_INSTRUMENTED;
PSI_cond_key KEY_cond_x_scheduler_dynamic_thread_exit = PSI_NOT_INSTRUMENTED;
PSI_cond_key KEY_cond_x_queue = PSI_NOT_INSTRUMENTED;

PSI_rwlock_key KEY_rwlock_x_client_list_clients = PSI_NOT_INSTRUMENTED;

PSI_memory_key KEY_memory_x_recv_buffer = PSI_NOT_INSTRUMENTED;
PSI_memory_key KEY_memory_x_send_buffer = PSI_NOT_INSTRUMENTED;

PSI_socket_key KEY_socket_x_tcpip = PSI_NOT_INSTRUMENTED;
PSI_socket_key KEY_socket_x_unix = PSI_NOT_INSTRUMENTED;
PSI_socket_key KEY_socket_x_client_connection = PSI_NOT_INSTRUMENTED;

const char  *my_localhost;
bool volatile abort_loop;

int ip_to_hostname(struct sockaddr_storage *ip_storage,
                const char *ip_string,
                char **hostname,
                uint *connect_errors)
{
  DBUG_ASSERT(0);
  return 1;
}

int register_server_state_observer(Server_state_observer *observer, void *p)
{
  return 0;
}

int unregister_server_state_observer(Server_state_observer *observer, void *p)
{
  return 0;
}

extern "C"
void ssl_wrapper_version(Vio *vio, char *version, const size_t version_size)
{
}

extern "C"
void ssl_wrapper_cipher(Vio *vio, char *cipher, const size_t cipher_size)
{
}

extern "C"
long ssl_wrapper_cipher_list(Vio *vio, const char **clipher_list, const size_t maximun_num_of_elements)
{
  return 0;
}

extern "C"
long ssl_wrapper_verify_depth(Vio *vio)
{
  return 0;
}

extern "C"
long ssl_wrapper_verify_mode(Vio *vio)
{
  return 0;
}

extern "C"
void ssl_wrapper_get_peer_certificate_issuer(Vio *vio, char *issuer, const size_t issuer_size)
{
}

extern "C"
void ssl_wrapper_get_peer_certificate_subject(Vio *vio, char *subject, const size_t subject_size)
{
}

extern "C"
long ssl_wrapper_get_verify_result_and_cert(Vio *vio)
{
  return 0;
}

extern "C"
long ssl_wrapper_ctx_verify_depth(struct st_VioSSLFd *vio_ssl)
{
  return 0;
}

extern "C"
long ssl_wrapper_ctx_verify_mode(struct st_VioSSLFd *vio_ssl)
{
  return 0;
}

extern "C"
void  ssl_wrapper_ctx_server_not_after(struct st_VioSSLFd *vio_ssl, char *no_after, const size_t no_after_size)
{
}

extern "C"
void ssl_wrapper_ctx_server_not_before(struct st_VioSSLFd *vio_ssl, char *no_before, const size_t no_before_size)
{
}

extern "C"
void ssl_wrapper_thread_cleanup()
{
}

extern "C"
long ssl_wrapper_sess_accept(struct st_VioSSLFd *vio_ssl)
{
  return 0;
}

extern "C"
long ssl_wrapper_sess_accept_good(struct st_VioSSLFd *vio_ssl)
{
  return 0;
}
