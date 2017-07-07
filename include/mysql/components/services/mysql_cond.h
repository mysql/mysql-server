/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_MYSQL_COND_H
#define COMPONENTS_SERVICES_MYSQL_COND_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_mutex_bits.h>
#include <mysql/components/services/mysql_cond_service.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_cond_v1);

#define mysql_cond_register(P1, P2, P3) \
  mysql_service_mysql_cond_v1->register_info(P1, P2, P3)

#define mysql_cond_init(K, C) mysql_cond_init_with_src(K, C, __FILE__, __LINE__)
#define mysql_cond_init_with_src(K, C, F, L) \
  mysql_service_mysql_cond_v1->init(K, C, F, L)

#define mysql_cond_destroy(C) mysql_cond_destroy_with_src(C, __FILE__, __LINE__)
#define mysql_cond_destroy_with_src(C, F, L) \
  mysql_service_mysql_cond_v1->destroy(C, F, L)

#define mysql_cond_wait(C, M) mysql_cond_wait_with_src(C, M, __FILE__, __LINE__)
#define mysql_cond_wait_with_src(C, M, F, L) \
  mysql_service_mysql_cond_v1->wait(C, M, F, L)

#define mysql_cond_timedwait(C, M, T) \
  mysql_cond_timedwait_with_src(C, M, T, __FILE__, __LINE__)
#define mysql_cond_timedwait_with_src(C, M, T, F, L) \
  mysql_service_mysql_cond_v1->timedwait(C, M, T, F, L)

#define mysql_cond_signal(C) mysql_cond_signal_with_src(C, __FILE__, __LINE__)
#define mysql_cond_signal_with_src(C, F, L) mysql_service_mysql_cond_v1->signal(C, F, L)

#define mysql_cond_broadcast(C) \
  mysql_cond_broadcast_with_src(C, __FILE__, __LINE__)
#define mysql_cond_broadcast_with_src(C, F, L) \
  mysql_service_mysql_cond_v1->broadcast(C, F, L)

#endif /* COMPONENTS_SERVICES_MYSQL_COND_H */
