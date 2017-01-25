/*
 Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights
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


#include <ndb_init.h>
#include "CharsetMap.hpp"

#include <node.h>
#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeCFunctionCall.h"
#include "NativeMethodCall.h"




void CharsetMap_init_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  CharsetMap::init();
  args.GetReturnValue().SetNull();
}


void CharsetMap_unload_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  CharsetMap::unload();
  args.GetReturnValue().SetNull();
}


void Ndb_util_initOnLoad(v8::Handle<v8::Object> target) {
  v8::Local<v8::FunctionTemplate> JSCharsetMap;

  DEFINE_JS_FUNCTION(target, "CharsetMap_init", CharsetMap_init_wrapper);
  DEFINE_JS_FUNCTION(target, "CharsetMap_unload", CharsetMap_unload_wrapper);
}

