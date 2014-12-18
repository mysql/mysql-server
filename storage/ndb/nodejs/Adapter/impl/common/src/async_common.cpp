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

#include <stdio.h>

#include "adapter_global.h"
#include "AsyncMethodCall.h"

using namespace v8;


void report_error(TryCatch * err) {
  HandleScope scope;
  String::Utf8Value exception(err->Exception());
  String::Utf8Value stack(err->StackTrace());
  Handle<Message> message = err->Message();
  fprintf(stderr, "%s\n", *exception);
  if(! message.IsEmpty()) {
    String::Utf8Value file(message->GetScriptResourceName());
    int line = message->GetLineNumber();
    fprintf(stderr, "%s:%d\n", *file, line);
  }
  if(stack.length() > 0) 
    fprintf(stderr, "%s\n", *stack);
}


void work_thd_run(uv_work_t *req) {
  AsyncCall *m = (AsyncCall *) req->data;

  m->run();
  m->handleErrors();
}


void main_thd_complete_async_call(AsyncCall *m) {
  v8::HandleScope scope;
  v8::TryCatch try_catch;
  try_catch.SetVerbose(true);

  m->doAsyncCallback(v8::Context::GetCurrent()->Global());

  /* exceptions */
  if(try_catch.HasCaught()) {
    report_error(& try_catch);
  }

  /* cleanup */
  delete m;
}


void main_thd_complete(uv_work_t *req UV_COMPLETE_EXTRA_ARG) {
  AsyncCall *m = (AsyncCall *) req->data;
  main_thd_complete_async_call(m);
  delete req;
}
