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

#include <node.h>
#include <node_buffer.h>
#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"
#include "NdbRecordObject.h"


/************************************************************
          Value Object, a.k.a "VO" or "Native-Backed Object"
          
 A VO consists of:
    * a Buffer holding data read from NDB
    * an NdbRecord (wrapped by a Record) describing the layout of the buffer
    * the NdbRecordObject
      Holds both the buffer and the record
      Maintains a list of columns to be written back, in READ-MODIFY-UPDATE
      Manages NULL values itself
      Delegates management of non-NULL values to its Column Proxies
    * Mutable per-instance Column Proxies
      Proxy the JavaScript value of a column
      read it from the buffer if it has not yet been read
      write it back to the buffer when requested
    * Immutable per-class Column Handlers
      Encode and decode values for column based on record layout
      
 
 The ValueObject defines setters and getters for the mapped fields and
 directs them to the NdbRecordObject

 Rough Call Flow
 ---------------
 A user supplies a mapping for a table.  The TableMetadata is fetched, and 
 used to resolve the mapping and create a DBTableHandler (dbt).  The dbt
 can then be used to build a JavaScript constructor for VOs. 
 
 Step 1: call getRecordForMapping(), implemented in DBDictionaryImpl.cpp. 
  This takes as arguments some parts of the DBTableHandler, and returns 
  a Record over the set of mapped columns. 

 Step 2: call getValueObjectConstructor(), implemented here.  This takes as 
  arguments the Record, the field names, and any needed typeConverters.  
  It returns a constructor that can be used to create VOs (a VOC).  The VOC 
  itself takes one argument, the buffer containing data that has been read.

 Step 3: we want an instantiated VO both to have the properties defined in
  the mapping and to have the behaviors of the user's Domain Object (DO).  
  So, after obtaining the VOC in JavaScript, we apply the user's prototype 
  to it, like this:
    VOC.prototype = DOC.prototype; 

 These steps are all currently are performed in NdbOperation.js 
 
 Application: 
   * a row is read from the database into buffer op.buffers.row 
   * The operation's read valule is set to a newly constructed VO:
     op.result.value = new VOC(op.buffers.row);
   * The user's constructor is called on the new value:
     DOC.call(op.result.value);
 


*************************************************************/

Envelope columnHandlerSetEnvelope("ColumnHandlerSet");
Envelope nroEnvelope("NdbRecordObject");


/* Generic getter for all NdbRecordObjects
*/
Handle<Value> nroGetter(Local<String>, const AccessorInfo & info) 
{
  NdbRecordObject * nro = 
    static_cast<NdbRecordObject *>(info.Holder()->GetPointerFromInternalField(1));
  int nField = info.Data()->Int32Value();
  return nro->getField(nField);
}                    


/* Generic setter for all NdbRecordObjects
*/
void nroSetter(Local<String>, Local<Value> value, const AccessorInfo& info) 
{
  NdbRecordObject * nro = 
    static_cast<NdbRecordObject *>(info.Holder()->GetPointerFromInternalField(1));
  int nField = info.Data()->Int32Value();
  nro->setField(nField, value);
}


/* Generic constructor wrapper.
 * args[0]: row buffer
 * args[1]: array of blob & text column values
 * args.Data(): mapData holding the record and ColumnHandlers
 * args.This(): VO built from the mapping-specific InstanceTemplate
*/
Handle<Value> nroConstructor(const Arguments &args) {
  HandleScope scope;

  if(args.IsConstructCall()) {
    /* Unwrap record from mapData */
    Local<Object> mapData = args.Data()->ToObject();
    const Record * record = 
      unwrapPointer<const Record *>(mapData->Get(0)->ToObject());

    /* Unwrap Column Handlers from mapData */
    ColumnHandlerSet * handlers = 
      unwrapPointer<ColumnHandlerSet *>(mapData->Get(1)->ToObject());

    /* Build NdbRecordObject */
    NdbRecordObject * nro = new NdbRecordObject(record, handlers, args[0], args[1]);

    /* Wrap for JavaScript */
    wrapPointerInObject<NdbRecordObject *>(nro, nroEnvelope, args.This());
    freeFromGC(nro, args.This());
  }
  else {
    ThrowException(Exception::Error(String::New("must be a called as constructor")));
  }
  return args.This();
}


/* arg0: Record constructed over the appropriate column list
   arg1: Array of field names
   arg2: Array of typeConverters 

   Returns: a constructor function that can be used to create native-backed 
   objects
*/
Handle<Value> getValueObjectConstructor(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  Local<FunctionTemplate> ft = FunctionTemplate::New();
  Local<ObjectTemplate> inst = ft->InstanceTemplate();
  inst->SetInternalFieldCount(2);

  /* Initialize the mapData */
  Local<Object> mapData = Object::New();
  
  /* Store the record in the mapData at 0 */
  mapData->Set(0, args[0]);

  /* Build the ColumnHandlers and store them in the mapData at 1 */
  const Record * record = unwrapPointer<const Record *>(args[0]->ToObject());
  const uint32_t ncol = record->getNoOfColumns();
  ColumnHandlerSet *columnHandlers = new ColumnHandlerSet(ncol);
  for(unsigned int i = 0 ; i < ncol ; i++) {
    const NdbDictionary::Column * col = record->getColumn(i);
    size_t offset = record->getColumnOffset(i);
    ColumnHandler * handler = columnHandlers->getHandler(i);
    handler->init(col, offset, args[2]->ToObject()->Get(i));
  }
  Local<Object> jsHandlerSet = columnHandlerSetEnvelope.newWrapper();
  wrapPointerInObject<ColumnHandlerSet *>(columnHandlers, 
                                          columnHandlerSetEnvelope,
                                          jsHandlerSet);
  mapData->Set(1, jsHandlerSet);

  /* Create accessors for the mapped fields in the instance template.
     AccessorInfo.Data() for the accessor will hold the field number.
  */
  Local<Object> jsFields = args[1]->ToObject();
  for(unsigned int i = 0 ; i < ncol; i++) {
    Handle<String> fieldName = jsFields->Get(i)->ToString();
    inst->SetAccessor(fieldName, nroGetter, nroSetter, Number::New(i),
                      DEFAULT, DontDelete);
  }

  /* The generic constructor is the CallHandler */
  ft->SetCallHandler(nroConstructor, Persistent<Object>::New(mapData));

  return scope.Close(ft->GetFunction());
}


Handle<Value> isValueObject(const Arguments &args) {
  HandleScope scope;
  bool answer = false;  
  Handle<Value> v = args[0];

  if(v->IsObject()) {
    Local<Object> o = v->ToObject();
    if(o->InternalFieldCount() == 2) {
      Envelope * n = (Envelope *) o->GetPointerFromInternalField(0);
      if(n == & nroEnvelope) {
        answer = true;
      }
    }
  }

  return scope.Close(Boolean::New(answer));
}


Handle<Value> getValueObjectWriteCount(const Arguments &args) {
  HandleScope scope;
  NdbRecordObject * nro = unwrapPointer<NdbRecordObject *>(args[0]->ToObject());
  return scope.Close(Number::New(nro->getWriteCount()));
}


Handle<Value> prepareForUpdate(const Arguments &args) {
  HandleScope scope;
  NdbRecordObject * nro = unwrapPointer<NdbRecordObject *>(args[0]->ToObject());
  return scope.Close(nro->prepare());
}


void ValueObject_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "getValueObjectConstructor", getValueObjectConstructor);
  DEFINE_JS_FUNCTION(target, "isValueObject", isValueObject);
  DEFINE_JS_FUNCTION(target, "getValueObjectWriteCount", getValueObjectWriteCount);
  DEFINE_JS_FUNCTION(target, "prepareForUpdate", prepareForUpdate);
}
