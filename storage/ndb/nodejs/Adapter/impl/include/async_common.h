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

#include "uv.h"
#include "compat_uv.h"

#ifdef __cplusplus
extern "C" {
#endif

  void work_thd_run(uv_work_t *);
  void main_thd_complete(uv_work_t *);
  void main_thd_complete_newapi(uv_work_t *, int);

#ifdef __cplusplus
}

  class AsyncCall;
  void main_thd_complete_async_call(AsyncCall *);

#endif

#ifdef OLDER_UV_AFTER_WORK_CB
#define ASYNC_COMMON_MAIN_THD_CALLBACK main_thd_complete
#else
#define ASYNC_COMMON_MAIN_THD_CALLBACK main_thd_complete_newapi
#endif
