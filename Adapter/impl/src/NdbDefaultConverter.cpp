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

#include "DBSessionImpl.h"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "unified_debug.h"

#include "DataTypeHandler.h"

using namespace v8;

Envelope ThisEnvelope("NdbDefaultConverter");

/*  Converter.prototype = {
      "readFromString" : function(object, string) {},
      "writeToString"  : function(object, string) {},
      "readFromBuffer" : function(object, Buffer, offset, length) {},
      "writeToBuffer"  : function(object, Buffer, offset, length) {}
    };
*/



/* Construct the default converter.
   arg0 is a DBColumn (which wraps a const NdbDictionary::Column *)
*/
Handle<Value> conv_new(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<const NdbDictionary::Column *> arg0(args[0]);

  DataTypeHandler *dth = getDataTypeHandlerForColumn(arg0.toC());
    
  wrapPointerInObject(dth, ThisEnvelope, args.This());
  return args.This();
}


Handle<Value> conv_read(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(4);

}



//////////// INITIALIZATION

void NdbConverterImpl_initOnLoad(Handle<Object> target) {
 DEBUG_MARKER(UDEB_DETAIL);
  Local<FunctionTemplate> JSNdbConverterImpl;

  DEFINE_JS_CLASS(JSNdbDefaultConverter, "NdbDefaultConverter", conv_new);

  DEFINE_JS_METHOD(JSNdbDefaultConverter, "readFromBuffer", conv_read);
  DEFINE_JS_METHOD(JSNdbDefaultConverter, "writeToBuffer",  conv_write);
  
  DEFINE_JS_CONSTRUCTOR(target, "NdbDefaultConverter", JSNdbDefaultConverter);
}


