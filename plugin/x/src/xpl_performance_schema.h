/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_PERFORMANCE_SCHEMA_H_
#define _XPL_PERFORMANCE_SCHEMA_H_

#include <mysql/psi/mysql_memory.h>
#include <mysql/psi/mysql_rwlock.h>
#include <mysql/psi/mysql_socket.h>
#include <mysql/psi/mysql_thread.h>

#include "my_psi_config.h"
#include "my_sys.h"

#ifdef HAVE_PSI_INTERFACE

extern PSI_thread_key KEY_thread_x_worker;
extern PSI_thread_key KEY_thread_x_acceptor;

extern PSI_mutex_key KEY_mutex_x_lock_list_access;
extern PSI_mutex_key KEY_mutex_x_scheduler_dynamic_worker_pending;
extern PSI_mutex_key KEY_mutex_x_scheduler_dynamic_thread_exit;
extern PSI_mutex_key KEY_mutex_x_scheduler_dynamic_post;
extern PSI_mutex_key KEY_mutex_x_document_id_generate;

extern PSI_cond_key KEY_cond_x_scheduler_dynamic_worker_pending;
extern PSI_cond_key KEY_cond_x_scheduler_dynamic_thread_exit;

extern PSI_rwlock_key KEY_rwlock_x_client_list_clients;
extern PSI_rwlock_key KEY_rwlock_x_sha256_password_cache;

extern PSI_socket_key KEY_socket_x_tcpip;
extern PSI_socket_key KEY_socket_x_unix;
extern PSI_socket_key KEY_socket_x_client_connection;

extern PSI_memory_key KEY_memory_x_objects;
extern PSI_memory_key KEY_memory_x_recv_buffer;
extern PSI_memory_key KEY_memory_x_send_buffer;

#endif  // HAVE_PSI_INTERFACE

void xpl_init_performance_schema();

#endif  // _XPL_PERFORMANCE_SCHEMA_H_
