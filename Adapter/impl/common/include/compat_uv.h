/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

/* Compatibility wrappers for changing APIs in libuv 
*/

#if (UV_VERSION_MAJOR == 0  && UV_VERSION_MINOR < 8) 
/*
  Work around the lack of uv_mutex_ and uv_pthread_ in Node.JS 0.6
*/
#ifdef _WIN32
#error "Building mysql-js with old libuv is not supported on Windows."
#endif

#include <pthread.h>

#define FORCE_UV_LEGACY_COMPAT 

#define uv_thread_t pthread_t
#define uv_mutex_t pthread_mutex_t
#define uv_rwlock_t pthread_rwlock_t

#define uv_mutex_init(X) pthread_mutex_init(X, NULL)
#define uv_mutex_lock(X) pthread_mutex_lock(X)
#define uv_mutex_unlock(X) pthread_mutex_unlock(X)
#define uv_mutex_destroy(X) pthread_mutex_destroy(X) 

#define uv_rwlock_init(X) pthread_rwlock_init(X, NULL)
#define uv_rwlock_rdlock(X) pthread_rwlock_rdlock(X)
#define uv_rwlock_tryrdlock(X) pthread_rwlock_tryrdlock(X)
#define uv_rwlock_rdunlock(X) pthread_rwlock_unlock(X)
#define uv_rwlock_wrlock(X) pthread_rwlock_wrlock(X)
#define uv_rwlock_trywrlock(X) pthread_rwlock_trywrlock(X)
#define uv_rwlock_wrunlock(X) pthread_rwlock_unlock(X)

#define uv_thread_create(A, B, C) pthread_create(A, NULL, B, C)
#define uv_thread_join(A) pthread_join(* A, NULL)

#endif


#if(UV_VERSION_MAJOR == 0 && UV_VERSION_MINOR < 9)
/* 
  uv_queue_work changed in Node.JS 0.9
*/
#define OLDER_UV_AFTER_WORK_CB

#endif

