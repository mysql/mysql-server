/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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


#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"
#include "JsValueAccess.h"


enum {
  BOUND_LOW_KEY = 0,
  BOUND_LOW_KEY_COUNT,
  BOUND_LOW_INCLUSIVE,
  BOUND_HIGH_KEY,
  BOUND_HIGH_KEY_COUNT,
  BOUND_HIGH_INCLUSIVE,
  BOUND_RANGE_NO
};

Envelope IndexBoundEnvelope("IndexBound");

void debug_print_bound(NdbIndexScanOperation::IndexBound * bound) {
  DEBUG_PRINT("Range %d: %s-%d-part-%s -> %d-part-%s-%s",
              bound->range_no,
              bound->low_inclusive ? "[inc" : "(exc",
              bound->low_key_count,
              bound->low_key ? "value" : "NULL",
              bound->high_key_count,
              bound->high_key ? "value" : "NULL",
              bound->high_inclusive ? "inc]" : "exc)");
}

void newIndexBound(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());

  NdbIndexScanOperation::IndexBound * bound = 
    new NdbIndexScanOperation::IndexBound;
  Local<Value> jsBound = IndexBoundEnvelope.wrap(bound);

  const Local<Object> spec = ArgToObject(args, 0);
  Local<Value> v;
  Local<Object> o;

  bound->low_key = 0;
  v = Get(spec, BOUND_LOW_KEY);
  if(v->IsNull()) {
    bound->low_key = 0;
  } else {
    o = ToObject(args, v);
    bound->low_key = GetBufferData(o);
  }

  bound->low_key_count = 0;
  v = Get(spec, BOUND_LOW_KEY_COUNT);
  if(! v->IsNull()) {
    bound->low_key_count = GetInt32Value(args, v);
  }
  
  bound->low_inclusive = false;
  v = Get(spec, BOUND_LOW_INCLUSIVE);
  if(! v->IsNull()) {
    bound->low_inclusive = GetBoolValue(args, v);
  }
  
  bound->high_key = 0;
  v = Get(spec, BOUND_HIGH_KEY);
  if(v->IsNull()) {
    bound->high_key = 0;
  } else {
    o = ToObject(args, v);
    bound->high_key = GetBufferData(o);
  }
  
  bound->high_key_count = 0;
  v = Get(spec, BOUND_HIGH_KEY_COUNT);
  if(! v->IsNull()) {
    bound->high_key_count = GetInt32Value(args, v);
  }
  
  bound->high_inclusive = false;
  v = Get(spec, BOUND_HIGH_INCLUSIVE);
  if(! v->IsNull()) {
    bound->high_inclusive = GetBoolValue(args, v);
  }
  
  bound->range_no = 0;
  v = Get(spec, BOUND_RANGE_NO);
  if(! v->IsNull()) {
    bound->range_no = GetInt32Value(args, v);
  }

  debug_print_bound(bound);

  args.GetReturnValue().Set(scope.Escape(jsBound));
}


void IndexBound_initOnLoad(Local<Object> target) {
  Local<Object> ibObj = Object::New(Isolate::GetCurrent());
  SetProp(target, "IndexBound", ibObj);

  DEFINE_JS_FUNCTION(ibObj, "create", newIndexBound);

  Local<Object> BoundHelper = Object::New(Isolate::GetCurrent());
  SetProp(ibObj, "helper", BoundHelper);

  DEFINE_JS_INT(BoundHelper, "low_key", BOUND_LOW_KEY);
  DEFINE_JS_INT(BoundHelper, "low_key_count", BOUND_LOW_KEY_COUNT);
  DEFINE_JS_INT(BoundHelper, "low_inclusive", BOUND_LOW_INCLUSIVE);
  DEFINE_JS_INT(BoundHelper, "high_key", BOUND_HIGH_KEY);
  DEFINE_JS_INT(BoundHelper, "high_key_count", BOUND_HIGH_KEY_COUNT);
  DEFINE_JS_INT(BoundHelper, "high_inclusive", BOUND_HIGH_INCLUSIVE);
  DEFINE_JS_INT(BoundHelper, "range_no", BOUND_RANGE_NO);
}

