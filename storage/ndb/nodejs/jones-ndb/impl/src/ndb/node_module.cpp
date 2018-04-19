/*
 Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <node.h>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "JsConverter.h"

using namespace v8;

typedef void LOADER_FUNCTION(Handle<Object>);

extern LOADER_FUNCTION Ndb_init_initOnLoad;
extern LOADER_FUNCTION Ndb_util_initOnLoad;
extern LOADER_FUNCTION Ndb_cluster_connection_initOnLoad;
extern LOADER_FUNCTION NdbTransaction_initOnLoad;
extern LOADER_FUNCTION DBDictionaryImpl_initOnLoad;
extern LOADER_FUNCTION DBOperationHelper_initOnLoad;
extern LOADER_FUNCTION udebug_initOnLoad;
extern LOADER_FUNCTION AsyncNdbContext_initOnLoad;
extern LOADER_FUNCTION NdbWrapper_initOnLoad;
extern LOADER_FUNCTION NdbTypeEncoders_initOnLoad;
extern LOADER_FUNCTION ValueObject_initOnLoad;
extern LOADER_FUNCTION IndexBound_initOnLoad;
extern LOADER_FUNCTION NdbInterpretedCode_initOnLoad;
extern LOADER_FUNCTION NdbScanFilter_initOnLoad;
extern LOADER_FUNCTION ScanHelper_initOnLoad;
extern LOADER_FUNCTION SessionImpl_initOnLoad;
extern LOADER_FUNCTION QueryOperation_initOnLoad;

void init_ndbapi(Handle<Object> target) {
  Ndb_cluster_connection_initOnLoad(target);
  Ndb_init_initOnLoad(target);
  NdbTransaction_initOnLoad(target);
  NdbInterpretedCode_initOnLoad(target);
  NdbScanFilter_initOnLoad(target);
}


void init_impl(Handle<Object> target) {
  DBDictionaryImpl_initOnLoad(target);
  DBOperationHelper_initOnLoad(target);
  AsyncNdbContext_initOnLoad(target);
  NdbWrapper_initOnLoad(target);
  ValueObject_initOnLoad(target);
  IndexBound_initOnLoad(target);
  ScanHelper_initOnLoad(target);
  SessionImpl_initOnLoad(target);
  QueryOperation_initOnLoad(target);
}


void initModule(Handle<Object> target) {
  EscapableHandleScope scope(v8::Isolate::GetCurrent());      // Keep this
  Local<Object> ndb_obj    = Object::New(v8::Isolate::GetCurrent());
  Local<Object> ndbapi_obj = Object::New(v8::Isolate::GetCurrent());
  Local<Object> impl_obj   = Object::New(v8::Isolate::GetCurrent());
  Local<Object> util_obj   = Object::New(v8::Isolate::GetCurrent());
  Local<Object> debug_obj  = Object::New(v8::Isolate::GetCurrent());
  
  init_ndbapi(ndbapi_obj);
  init_impl(impl_obj);
  Ndb_util_initOnLoad(util_obj);
  NdbTypeEncoders_initOnLoad(impl_obj);
  udebug_initOnLoad(debug_obj);
  
  target->Set(NEW_SYMBOL("debug"), debug_obj);
  target->Set(NEW_SYMBOL("ndb"), ndb_obj);

  ndb_obj->Set(NEW_SYMBOL("ndbapi"), ndbapi_obj);
  ndb_obj->Set(NEW_SYMBOL("impl"), impl_obj);
  ndb_obj->Set(NEW_SYMBOL("util"), util_obj);
}

NODE_MODULE(ndb_adapter, initModule)

