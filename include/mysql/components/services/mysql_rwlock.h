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

#ifndef COMPONENTS_SERVICES_MYSQL_RWLOCK_H
#define COMPONENTS_SERVICES_MYSQL_RWLOCK_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_rwlock_service.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_rwlock_v1);

#define mysql_rwlock_register(P1, P2, P3) \
  mysql_service_mysql_rwlock_v1->register_info(P1, P2, P3)

#define mysql_rwlock_init(K, T) \
  mysql_rwlock_init_with_src(K, T, __FILE__, __LINE__)
#define mysql_rwlock_init_with_src(K, T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_init(K, T, F, L)

#define mysql_prlock_init(K, T) \
  mysql_prlock_init_with_src(K, T, __FILE__, __LINE__)
#define mysql_prlock_init_with_src(K, T, F, L) \
  mysql_service_mysql_rwlock_v1->prlock_init(K, T, F, L)

#define mysql_rwlock_destroy(T) \
  mysql_rwlock_destroy_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_destroy_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_destroy(T, F, L)

#define mysql_prlock_destroy(T) \
  mysql_prlock_destroy_with_src(T, __FILE__, __LINE__)
#define mysql_prlock_destroy_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->prlock_destroy(T, F, L)

#define mysql_rwlock_rdlock(T) \
  mysql_rwlock_rdlock_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_rdlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_rdlock(T, F, L)

#define mysql_prlock_rdlock(T) \
  mysql_prlock_rdlock_with_src(T, __FILE__, __LINE__)
#define mysql_prlock_rdlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->prlock_rdlock(T, F, L)

#define mysql_rwlock_wrlock(T) \
  mysql_rwlock_wrlock_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_wrlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_wrlock(T, F, L)

#define mysql_prlock_wrlock(T) \
  mysql_prlock_wrlock_with_src(T, __FILE__, __LINE__)
#define mysql_prlock_wrlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->prlock_wrlock(T, F, L)

#define mysql_rwlock_tryrdlock(T) \
  mysql_rwlock_tryrdlock_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_tryrdlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_tryrdlock(T, F, L)

#define mysql_rwlock_trywrlock(T) \
  mysql_rwlock_trywrlock_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_trywrlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_trywrlock(T, F, L)

#define mysql_rwlock_unlock(T) \
  mysql_rwlock_unlock_with_src(T, __FILE__, __LINE__)
#define mysql_rwlock_unlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->rwlock_unlock(T, F, L)

#define mysql_prlock_unlock(T) \
  mysql_prlock_unlock_with_src(T, __FILE__, __LINE__)
#define mysql_prlock_unlock_with_src(T, F, L) \
  mysql_service_mysql_rwlock_v1->prlock_unlock(T, F, L)

#endif /* COMPONENTS_SERVICES_MYSQL_RWLOCK_H */
