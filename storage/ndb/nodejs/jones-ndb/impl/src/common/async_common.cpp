/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <stdio.h>

#include "AsyncMethodCall.h"
#include "JsValueAccess.h"
#include "adapter_global.h"

void report_error(TryCatch *err) {
  Local<Message> message = err->Message();
  Isolate *isolate = message->GetIsolate();
  String::Utf8Value exception(isolate, err->Exception());
  String::Utf8Value stack(isolate, GetStackTrace(isolate, err));
  String::Utf8Value msg(isolate, message->Get());
  String::Utf8Value file(isolate, message->GetScriptResourceName());
  int line = 0;
  Maybe<int> err_line = message->GetLineNumber(isolate->GetCurrentContext());
  if (err_line.IsJust()) line = err_line.ToChecked();

  fprintf(stderr, "%s [%s]\n", *exception, *msg);
  fprintf(stderr, "%s: line %d\n", *file, line);
  if (stack.length() > 0) fprintf(stderr, "%s\n", *stack);
}

void work_thd_run(uv_work_t *req) {
  AsyncCall *m = (AsyncCall *)req->data;

  m->run();
  m->handleErrors();
}

void main_thd_complete_async_call(AsyncCall *m) {
  v8::Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  m->doAsyncCallback(isolate->GetCurrentContext()->Global());

  /* exceptions */
  if (try_catch.HasCaught()) {
    report_error(&try_catch);
  }

  /* cleanup */
  delete m;
}

void main_thd_complete(uv_work_t *req, int) {
  AsyncCall *m = (AsyncCall *)req->data;
  main_thd_complete_async_call(m);
  delete req;
}
