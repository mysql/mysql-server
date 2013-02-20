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

/* Compatibility for older libuv 
   
  THIS FILE SHOULD BE REMOVED BY 2014, OR WHEN NODE 1.0 IS RELEASED.
  It allows compatibility with Node 0.6 on unix platforms.
*/

#if (UV_VERSION_MAJOR == 0  && UV_VERSION_MINOR < 8) 

#ifdef _WIN32
#error "Building mysql-js with old libuv is not supported on Windows."
#endif

#include <pthread.h>

#define FORCE_UV_LEGACY_COMPAT 

#define uv_thread_t pthread_t
#define uv_mutex_t pthread_mutex_t

#define uv_mutex_init(X) pthread_mutex_init(X, NULL)
#define uv_mutex_lock(X) pthread_mutex_lock(X)
#define uv_mutex_unlock(X) pthread_mutex_unlock(X)
#define uv_mutex_destroy(X) pthread_mutex_destroy(X) 

#define uv_thread_create(A, B, C) pthread_create(A, NULL, B, C)
#define uv_thread_join(A) pthread_join(* A, NULL)

#endif
