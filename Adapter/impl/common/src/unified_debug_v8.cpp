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

#include "js_wrapper_macros.h"
#include "node.h"

#include "unified_debug.h"

using namespace v8;

// udeb_switch
// udeb_print
// udeb_select
// udeb_add_drop
// udeb_destination

Handle<Value> js_udeb_switch(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  
  int i = args[0]->ToInt32()->Value();
  udeb_switch(i);
  
  return scope.Close(Null());
}


Handle<Value> js_udeb_print(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(3);

  if(uni_dbg()) {
    String::AsciiValue source_file(args[0]);
    int level = args[1]->ToInt32()->Value();
    String::AsciiValue message(args[2]);
    
    udeb_print(*source_file, level, *message);
  }
  return scope.Close(Null());
}


Handle<Value> js_udeb_select(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);

  int i = args[0]->ToInt32()->Value();

  switch(i) {
    case 0:
    case 3:
    case 4:
    case 5:
      udeb_select(NULL, i);
    default:
      break;
  }
  
  return scope.Close(Null());
}


Handle<Value> js_udeb_add_drop(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);

  String::AsciiValue source_file(args[0]);
  int i = args[1]->ToInt32()->Value();

  assert((i == 1) || (i == 2));
  udeb_select(*source_file, i);

  return scope.Close(Null());
}


Handle<Value> js_udeb_destination(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  String::AsciiValue log_file(args[0]);
  
  unified_debug_destination(*log_file);
    
  return scope.Close(Null());
}


void initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "udeb_switch"     , js_udeb_switch);
  DEFINE_JS_FUNCTION(target, "udeb_print"      , js_udeb_print );
  DEFINE_JS_FUNCTION(target, "udeb_select"     , js_udeb_select );
  DEFINE_JS_FUNCTION(target, "udeb_add_drop"   , js_udeb_add_drop );
  DEFINE_JS_FUNCTION(target, "udeb_destination", js_udeb_destination);
  NODE_DEFINE_CONSTANT(target, UDEB_INFO);
  NODE_DEFINE_CONSTANT(target, UDEB_DEBUG);
  NODE_DEFINE_CONSTANT(target, UDEB_DETAIL);
}

NODE_MODULE(unified_debug_impl, initOnLoad)

