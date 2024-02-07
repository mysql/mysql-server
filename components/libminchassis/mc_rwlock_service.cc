/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_rwlock_service.h>
#include "component_common.h"

#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#endif

void impl_min_chassis_rwlock_register(const char *, PSI_rwlock_info *, int) {
  return;
}

int impl_min_chassis_rwlock_init(PSI_rwlock_key, mysql_rwlock_t *that,
                                 const char *, unsigned int) {
  that->m_psi = nullptr;
#ifdef _WIN32
  InitializeSRWLock(&that->m_rwlock.srwlock);
  that->m_rwlock.have_exclusive_srwlock = false;
  return 0;
#else
  return pthread_rwlock_init(&that->m_rwlock, nullptr);
#endif
}

int impl_min_chassis_prlock_init(PSI_rwlock_key, mysql_prlock_t *, const char *,
                                 unsigned int) {
  /*
    prlock is not supported in minimal chassis implementations as it is not
    required. And also implementation of prlock would create a dependency on
    server code.
    To avoid accidental usage of this method, we do assert.
  */
  assert(false);
  return 0;
}

int impl_min_chassis_rwlock_destroy(mysql_rwlock_t *that [[maybe_unused]],
                                    const char *, unsigned int) {
#ifdef _WIN32
  return 0; /* no destroy function */
#else
  return pthread_rwlock_destroy(&that->m_rwlock);
#endif
}

int impl_min_chassis_prlock_destroy(mysql_prlock_t *, const char *,
                                    unsigned int) {
  /*
    prlock is not supported in minimal chassis implementations as it is not
    required. And also implementation of prlock would create a dependency on
    server code.
    To avoid accidental usage of this method, we do assert.
  */
  assert(false);
  return 0;
}

int impl_min_chassis_rwlock_rdlock(mysql_rwlock_t *that, const char *,
                                   unsigned int) {
#ifdef _WIN32
  AcquireSRWLockShared(&that->m_rwlock.srwlock);
  return 0;
#else
  return pthread_rwlock_rdlock(&that->m_rwlock);
#endif
}

int impl_min_chassis_prlock_rdlock(mysql_prlock_t *, const char *,
                                   unsigned int) {
  /*
    prlock is not supported in minimal chassis implementations as it is not
    required. And also implementation of prlock would create a dependency on
    server code.
    To avoid accidental usage of this method, we do assert.
  */
  assert(false);
  return 0;
}

int impl_min_chassis_rwlock_wrlock(mysql_rwlock_t *that, const char *,
                                   unsigned int) {
#ifdef _WIN32
  AcquireSRWLockExclusive(&that->m_rwlock.srwlock);
  that->m_rwlock.have_exclusive_srwlock = true;
  return 0;
#else
  return pthread_rwlock_wrlock(&that->m_rwlock);
#endif
}

int impl_min_chassis_prlock_wrlock(mysql_prlock_t *, const char *,
                                   unsigned int) {
  /*
    prlock is not supported in minimal chassis implementations as it is not
    required. And also implementation of prlock would create a dependency on
    server code.
    To avoid accidental usage of this method, we do assert.
  */
  assert(false);
  return 0;
}

int impl_min_chassis_rwlock_tryrdlock(mysql_rwlock_t *that, const char *,
                                      unsigned int) {
#ifdef _WIN32
  if (!TryAcquireSRWLockShared(&that->m_rwlock.srwlock)) return EBUSY;
  return 0;
#else
  return pthread_rwlock_tryrdlock(&that->m_rwlock);
#endif
  return 0;
}

int impl_min_chassis_rwlock_trywrlock(mysql_rwlock_t *that, const char *,
                                      unsigned int) {
#ifdef _WIN32
  if (!TryAcquireSRWLockExclusive(&that->m_rwlock.srwlock)) return EBUSY;
  that->m_rwlock.have_exclusive_srwlock = true;
  return 0;
#else
  return pthread_rwlock_trywrlock(&that->m_rwlock);
#endif
  return 0;
}

int impl_min_chassis_rwlock_unlock(mysql_rwlock_t *that, const char *,
                                   unsigned int) {
#ifdef _WIN32
  if (that->m_rwlock.have_exclusive_srwlock) {
    that->m_rwlock.have_exclusive_srwlock = false;
    ReleaseSRWLockExclusive(&that->m_rwlock.srwlock);
  } else
    ReleaseSRWLockShared(&that->m_rwlock.srwlock);
  return 0;
#else
  return pthread_rwlock_unlock(&that->m_rwlock);
#endif
}

int impl_min_chassis_prlock_unlock(mysql_prlock_t *, const char *,
                                   unsigned int) {
  /*
    prlock is not supported in minimal chassis implementations as it is not
    required. And also implementation of prlock would create a dependency on
    server code.
    To avoid accidental usage of this method, we do assert.
  */
  assert(false);
  return 0;
}

extern SERVICE_TYPE(mysql_rwlock_v1)
    SERVICE_IMPLEMENTATION(mysql_minimal_chassis, mysql_rwlock_v1);

SERVICE_TYPE(mysql_rwlock_v1)
SERVICE_IMPLEMENTATION(mysql_minimal_chassis, mysql_rwlock_v1) = {
    impl_min_chassis_rwlock_register,  impl_min_chassis_rwlock_init,
    impl_min_chassis_prlock_init,      impl_min_chassis_rwlock_destroy,
    impl_min_chassis_prlock_destroy,   impl_min_chassis_rwlock_rdlock,
    impl_min_chassis_prlock_rdlock,    impl_min_chassis_rwlock_wrlock,
    impl_min_chassis_prlock_wrlock,    impl_min_chassis_rwlock_tryrdlock,
    impl_min_chassis_rwlock_trywrlock, impl_min_chassis_rwlock_unlock,
    impl_min_chassis_prlock_unlock};
