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


/***** 
 *
 *
 *   dlopen() a file and return any errors from the loader.
 * 
 *   We do this because Node.js loses the error messages.
 *   This is implemented in a way that minimizes the load-time dependencies 
 *   of this module itself.
 *   
 ****/


#include <dlfcn.h>

#include "adapter_global.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"

using namespace v8;

Handle<Value> dlopen_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);

  v8::String::AsciiValue pathname(args[0]);
  Local<String> result;
  
  if(dlopen(*pathname, RTLD_LAZY) == NULL) {
    result = String::New(dlerror());
  }
  else {
    result = String::New("OK");
  }
  
  return scope.Close(result);
}


void dlopen_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "debug_dlopen", dlopen_wrapper);
}

V8BINDER_LOADABLE_MODULE(debug_dlopen, dlopen_initOnLoad)

