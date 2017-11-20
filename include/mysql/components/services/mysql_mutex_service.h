/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef COMPONENTS_SERVICES_MYSQL_MUTEX_SERVICE_H
#define COMPONENTS_SERVICES_MYSQL_MUTEX_SERVICE_H

#include <mysql/components/services/mysql_mutex_bits.h>
#include <mysql/components/services/psi_mutex_bits.h>
#include <mysql/components/service.h>

/**
  @defgroup psi_abi_mutex Mutex Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

typedef void (*mysql_mutex_register_t)(const char *category,
                                       PSI_mutex_info *info,
                                       int count);

typedef int (*mysql_mutex_init_t)(PSI_mutex_key key,
                                  mysql_mutex_t *that,
                                  const native_mutexattr_t *attr,
                                  const char *src_file,
                                  unsigned int src_line);

typedef int (*mysql_mutex_destroy_t)(mysql_mutex_t *that,
                                     const char *src_file,
                                     unsigned int src_line);

typedef int (*mysql_mutex_lock_t)(mysql_mutex_t *that,
                                  const char *src_file,
                                  unsigned int src_line);

typedef int (*mysql_mutex_trylock_t)(mysql_mutex_t *that,
                                     const char *src_file,
                                     unsigned int src_line);

typedef int (*mysql_mutex_unlock_t)(mysql_mutex_t *that,
                                    const char *src_file,
                                    unsigned int src_line);

BEGIN_SERVICE_DEFINITION(mysql_mutex_v1)
mysql_mutex_register_t register_info;
mysql_mutex_init_t init;
mysql_mutex_destroy_t destroy;
mysql_mutex_lock_t lock;
mysql_mutex_trylock_t trylock;
mysql_mutex_unlock_t unlock;
END_SERVICE_DEFINITION(mysql_mutex_v1)

/* Hide the real name from component source code, for versions. */
#define REQUIRES_MYSQL_MUTEX_SERVICE REQUIRES_SERVICE(mysql_mutex_v1)

/**
  Mutex service.
*/
typedef SERVICE_TYPE(mysql_mutex_v1) mysql_mutex_service_t;

/** @} (end of group psi_abi_mutex) */

#endif /* COMPONENTS_SERVICES_MYSQL_MUTEX_SERVICE_H */
