/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef LOCKING_SERVICE_INCLUDED
#define LOCKING_SERVICE_INCLUDED

#include "my_global.h"
#include "mysql/service_locking.h" // enum_locking_service_lock_type

class THD;


/**
  Acquire locking service locks.

  @param opaque_thd      Thread handle. If NULL, current_thd will be used.
  @param lock_namespace  Namespace of the locks to acquire.
  @param lock_names      Array of names of the locks to acquire.
  @param lock_num        Number of elements in 'lock_names'.
  @param lock_type       Lock type to acquire. LOCKING_SERVICE_READ or _WRITE.
  @param lock_timeout    Number of seconds to wait before giving up.

  @retval 1              Acquisition failed, error has been reported.
  @retval 0              Acquisition successful, all locks acquired.

  @note both lock_namespace and lock_names are limited to 64 characters max.
  Names are compared using binary comparison.
*/
int acquire_locking_service_locks(MYSQL_THD opaque_thd, const char* lock_namespace,
                                  const char**lock_names, size_t lock_num,
                                  enum enum_locking_service_lock_type lock_type,
                                  unsigned long lock_timeout);

/**
  Release all lock service locks taken by the given connection
  in the given namespace.

  @param opaque_thd      Thread handle. If NULL, current_thd will be used.
  @param lock_namespace  Namespace of the locks to release.

  @retval 1              Release failed, error has been reported.
  @retval 0              Release successful, all locks acquired.
*/
int release_locking_service_locks(MYSQL_THD opaque_thd, const char* lock_namespace);

/**
  Release all locking service locks taken by the given connection
  in all namespaces.

  @param thd             Thread handle.
*/
void release_all_locking_service_locks(THD *thd);

#endif /* LOCKING_SERVICE_INCLUDED */
