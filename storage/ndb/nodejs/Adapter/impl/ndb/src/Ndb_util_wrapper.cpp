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

#include <node.h>

#include <ndb_init.h>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeCFunctionCall.h"
#include "NativeMethodCall.h"

#include "ndb_util/CharsetMap.hpp"

using namespace v8;


Envelope CharsetMapEnv("CharsetMap");

Handle<Value> CharsetMap_init_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  CharsetMap::init();
  return Null();
}


Handle<Value> CharsetMap_unload_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  CharsetMap::unload();
  return Null();
}


Handle<Value> CharsetMap_new_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(0);

  CharsetMap * c = new CharsetMap();

  wrapPointerInObject(c, CharsetMapEnv, args.This());
  return args.This();
}


Handle<Value>  CharsetMap_getName(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<const char *, const CharsetMap, int> NCALL;

  NCALL ncall(& CharsetMap::getName, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}


Handle<Value>  CharsetMap_getMysqlName(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<const char *, const CharsetMap, int> NCALL;

  NCALL ncall(& CharsetMap::getMysqlName, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}


Handle<Value>  CharsetMap_getCharsetNumber(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<int, const CharsetMap, const char *> NCALL;

  NCALL ncall(& CharsetMap::getCharsetNumber, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}


Handle<Value>  CharsetMap_getUTF8CharsetNumber(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(0);

  typedef NativeConstMethodCall_0_<int, const CharsetMap> NCALL;

  NCALL ncall(& CharsetMap::getUTF8CharsetNumber, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}


Handle<Value>  CharsetMap_getUTF16CharsetNumber(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(0);

  typedef NativeConstMethodCall_0_<int, const CharsetMap> NCALL;

  NCALL ncall(& CharsetMap::getUTF16CharsetNumber, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> CharsetMap_isMultibyte(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<const bool *, const CharsetMap, int> NCALL;

  NCALL ncall(& CharsetMap::isMultibyte, args);
  ncall.run();

  return scope.Close(ncall.jsReturnVal());
}

void Ndb_util_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  Local<FunctionTemplate> JSCharsetMap;

  DEFINE_JS_FUNCTION(target, "CharsetMap_init", CharsetMap_init_wrapper);
  DEFINE_JS_FUNCTION(target, "CharsetMap_unload", CharsetMap_unload_wrapper);

  DEFINE_JS_CLASS(JSCharsetMap, "CharsetMap", CharsetMap_new_wrapper);
  DEFINE_JS_METHOD(JSCharsetMap, "getName", CharsetMap_getName);
  DEFINE_JS_METHOD(JSCharsetMap, "getMysqlName", CharsetMap_getMysqlName);
  DEFINE_JS_METHOD(JSCharsetMap, "getCharsetNumber", CharsetMap_getCharsetNumber);
  DEFINE_JS_METHOD(JSCharsetMap, "getUTF8CharsetNumber", CharsetMap_getUTF8CharsetNumber);
  DEFINE_JS_METHOD(JSCharsetMap, "getUTF16CharsetNumber", CharsetMap_getUTF16CharsetNumber);
  DEFINE_JS_METHOD(JSCharsetMap, "isMultibyte", CharsetMap_isMultibyte);
  DEFINE_JS_CONSTRUCTOR(target, "CharsetMap", JSCharsetMap);
}

