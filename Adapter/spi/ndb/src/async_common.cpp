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

#include "node.h"

#include "AsyncMethodWrapper.h"
#include "async_common.h"

void work_thd_run(uv_work_t *req) {
  AsyncMethodWrapper *w = (AsyncMethodWrapper *) req->data;
  
  w->main_thd_run();
}


void main_thd_complete(uw_work_t *req) {
  HandleScope scope;
  TryCatch try_catch;
  
  AsyncMethodWrapper *w = (AsyncMethodWrapper *) req->data;

  w->main_thd_complete(v8::Context::GetCurrent()->Global());
  
  /* cleanup */
  w->callback.Dispose();
  delete w;
  delete req;
  
  if(try_catch.HasCaught())
    v8::FatalException(try_catch);
}

