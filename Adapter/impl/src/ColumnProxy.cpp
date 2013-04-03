/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

#include "unified_debug.h"
#include "ColumnProxy.h"

using namespace v8;

Handle<String> K_toDB, K_fromDB;

void ColumnProxy_initOnLoad(Handle<Object>) {
  HandleScope scope;
  K_toDB = Persistent<String>::New(String::NewSymbol("toDB"));
  K_fromDB = Persistent<String>::New(String::NewSymbol("fromDB"));
}


void ColumnProxy::setTypeConverter(Handle<Object> _typeConverter)
{
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);

  typeConverter = Persistent<Object>::New(_typeConverter);
  hasWriteConverter =
    (typeConverter->Has(K_toDB) && typeConverter->Get(K_toDB)->IsFunction());
 
  hasReadConverter =
    (typeConverter->Has(K_fromDB) && typeConverter->Get(K_fromDB)->IsFunction());
}


ColumnProxy::~ColumnProxy() {
  typeConverter.Dispose();
  if(! jsValue.IsEmpty())
    jsValue.Dispose();
}


Handle<Value> ColumnProxy::get(const NdbDictionary::Column *col,
                               char *buffer, size_t offset) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> val;
  
  if(! isLoaded) {
    val = encoder->read(col, buffer, offset);

    /* Apply the typeConverter */
    if(hasReadConverter) {
      Function * converter = Function::Cast(* typeConverter->Get(K_fromDB));
      Handle<Value> arguments[1];
      arguments[0] = val;
      
      val = converter->Call(typeConverter, 1, arguments);
    }

    jsValue = Persistent<Value>::New(val);
    isLoaded = true;
  }
  return scope.Close(jsValue);
}


void ColumnProxy::set(Handle<Value> newValue) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> val = newValue;
  
  /* Drop our claim on the old value */
  if(! jsValue.IsEmpty()) {
    jsValue.Dispose();
  }
  
  isDirty = true;

  /* Apply the typeConverter */
  if(hasWriteConverter) {
    Function * converter = Function::Cast(* typeConverter->Get(K_toDB));
    Handle<Value> arguments[1];
    arguments[0] = newValue;
    
    val = converter->Call(typeConverter, 1, arguments);
  }
  
  jsValue = Persistent<Value>::New(val);
}


Handle<Value> ColumnProxy::write(Record *record, int col_idx, char *buffer) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  const NdbDictionary::Column * col = record->getColumn(col_idx);
  size_t offset = record->getColumnOffset(col_idx);
  Handle<Value> rval;

  if(isDirty || (jsValue->IsObject() && jsValue->ToObject()->IsDirty())) {
    if(jsValue->IsNull()) 
      record->setNull(col_idx, buffer);
    else 
      rval = encoder->write(col, jsValue, buffer, offset);
  }
  isDirty = false;
  
  return scope.Close(rval);
}

