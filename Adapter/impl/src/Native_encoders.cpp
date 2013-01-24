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
#include <node_buffer.h>
#include "adapter_global.h"
#include "unified_debug.h"
#include "js_wrapper_macros.h"

using namespace v8;


/* readDouble(buffer, offset) 
*/
Handle<Value> readDouble(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  REQUIRE_ARGS_LENGTH(2);
  char * data = node::Buffer::Data(args[0]->ToObject());
  size_t offset = args[1]->Uint32Value();
  double * dval = (double *) (data + offset);
  Local<Number> val = Number::New(*dval);
  
  return scope.Close(val);
}

/* writeDouble(value, buffer, offset) 
*/
Handle<Value> writeDouble(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  REQUIRE_ARGS_LENGTH(3);
  
  char * data = node::Buffer::Data(args[1]->ToObject());
  size_t offset = args[2]->Uint32Value();
  double * dval = (double *) (data + offset);
  
  *dval = args[0]->NumberValue();
  return scope.Close(Null());
}


/* readFloat(buffer, offset) 
*/
Handle<Value> readFloat(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  REQUIRE_ARGS_LENGTH(2);
  char * data = node::Buffer::Data(args[0]->ToObject());
  size_t offset = args[1]->Uint32Value();
  float * fval = (float *) (data + offset);
  double dval = *fval;  
  Local<Number> val = Number::New(dval);
  return scope.Close(val);
}


/* writeFloat(value, buffer, offset)
*/
Handle<Value> writeFloat(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  REQUIRE_ARGS_LENGTH(3);
  
  double dval = args[0]->NumberValue();
  char * data = node::Buffer::Data(args[1]->ToObject());
  size_t offset = args[2]->Uint32Value();
  float * fval = (float *) (data + offset);
  *fval = dval;

  return scope.Close(Null());
}


void Native_encoders_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "readDouble",  readDouble);
  DEFINE_JS_FUNCTION(target, "writeDouble", writeDouble);
  DEFINE_JS_FUNCTION(target, "readFloat",   readFloat);
  DEFINE_JS_FUNCTION(target, "writeFloat",  writeFloat);
}
