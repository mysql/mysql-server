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

#include <v8.h>
#include "node.h"

#include "NativeMethodCall.h"
#include "async_common.h"
#include "unified_debug.h"

void work_thd_run(uv_work_t *req) {
  DEBUG_ENTER();
  AsyncMethodCall *m = (AsyncMethodCall *) req->data;
  
  m->run();
  DEBUG_TRACE();
}


void main_thd_complete(uv_work_t *req) {
  DEBUG_ENTER();
  v8::HandleScope scope;
  v8::TryCatch try_catch;
  
  AsyncMethodCall *m = (AsyncMethodCall *) req->data;

  m->doAsyncCallback(v8::Context::GetCurrent()->Global());
  
  /* cleanup */
  m->callback.Dispose();
  delete m;
  delete req;
  
  /* exceptions */
  //if(try_catch.HasCaught())
  //  FatalException(try_catch);
  DEBUG_TRACE();
}

