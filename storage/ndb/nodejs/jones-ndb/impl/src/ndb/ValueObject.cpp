/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <node.h>
#include <NdbApi.hpp>

#include "JsValueAccess.h"
#include "JsWrapper.h"
#include "NdbRecordObject.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

using v8::Function;

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
  arguments the Record, the field names, and the prototype for domain objects.
  It returns a constructor that can be used to create VOs (a VOC).  The VOC
  itself takes two arguments: the buffer containing in-row data that has been
  read, and an array of individual buffers for BLOB columns.

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

#define JSCONTEXT(x) x.GetIsolate()->GetCurrentContext()

Envelope columnHandlerSetEnvelope("ColumnHandlerSet");

// An Envelope that wraps Envelopes for passing them in mapData:
Envelope envelopeEnvelope("Envelope");

/* Generic getter for all NdbRecordObjects
 */
void nroGetter(Local<String>, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  Envelope *env = static_cast<Envelope *>(
      info.Holder()->GetAlignedPointerFromInternalField(0));
  assert(env->isVO);
  NdbRecordObject *nro = static_cast<NdbRecordObject *>(
      info.Holder()->GetAlignedPointerFromInternalField(1));
  int nField = GetInt32Value(info.GetIsolate(), info.Data());
  DEBUG_PRINT_DETAIL("_GET_ NdbRecordObject field %d", nField);
  info.GetReturnValue().Set(nro->getField(nField));
}

/* Generic setter for all NdbRecordObjects
 */
void nroSetter(Local<String>, Local<Value> value, const SetterInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  Envelope *env = static_cast<Envelope *>(
      info.Holder()->GetAlignedPointerFromInternalField(0));
  assert(env->isVO);
  NdbRecordObject *nro = static_cast<NdbRecordObject *>(
      info.Holder()->GetAlignedPointerFromInternalField(1));
  int nField = GetInt32Value(info.GetIsolate(), info.Data());
  DEBUG_PRINT_DETAIL("+SET+ NdbRecordObject field %d", nField);
  nro->setField(nField, value);
}

void nroGetFieldByNumber(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);

  NdbRecordObject *nro = static_cast<NdbRecordObject *>(
      ArgToObject(args, 0)->GetAlignedPointerFromInternalField(1));
  int nField = GetInt32Arg(args, 1);
  args.GetReturnValue().Set(nro->getField(nField));
}

/* Generic constructor wrapper.
 * args[0]: row buffer
 * args[1]: array of blob & text column values
 * args.Data(): mapData holding the record, ColumnHandlers, and Envelope
 */
void nroConstructor(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  EscapableHandleScope scope(args.GetIsolate());

  /* Unwrap record from mapData */
  Local<Object> mapData = ToObject(args, args.Data());
  const Record *record =
      unwrapPointer<const Record *>(ElementToObject(mapData, 0));

  /* Unwrap Column Handlers from mapData */
  ColumnHandlerSet *handlers =
      unwrapPointer<ColumnHandlerSet *>(ElementToObject(mapData, 1));

  /* Unwrap the Envelope */
  Envelope *nroEnvelope =
      unwrapPointer<Envelope *>(ElementToObject(mapData, 2));

  /* Build NdbRecordObject */
  NdbRecordObject *nro = new NdbRecordObject(record, handlers, args);

  /* Wrap for JavaScript */
  Local<Value> jsRecordObject = nroEnvelope->wrap(nro);
  nroEnvelope->freeFromGC(nro, jsRecordObject);

  /* Set Prototype */
  Local<Value> prototype = Get(mapData, 3);
  if (!prototype->IsNull()) {
    ToObject(args, jsRecordObject)
        ->SetPrototype(JSCONTEXT(args), prototype)
        .ToChecked();
  }
  args.GetReturnValue().Set(scope.Escape(jsRecordObject));
}

/* arg0: Record constructed over the appropriate column list
   arg1: Array of field names
   arg2: DOC Prototype

   Returns: a constructor function that can be used to create native-backed
   objects
*/
void getValueObjectConstructor(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  EscapableHandleScope scope(args.GetIsolate());
  Local<FunctionTemplate> ft = FunctionTemplate::New(args.GetIsolate());
  ft->InstanceTemplate()->SetInternalFieldCount(2);

  /* Initialize the mapData */
  Local<Object> mapData = Object::New(args.GetIsolate());

  /* Store the record in the mapData at 0 */
  SetProp(mapData, 0, args[0]);

  /* Build the ColumnHandlers and store them in the mapData at 1 */
  const Record *record = unwrapPointer<const Record *>(ArgToObject(args, 0));
  const uint32_t ncol = record->getNoOfColumns();
  ColumnHandlerSet *columnHandlers = new ColumnHandlerSet(ncol);
  for (unsigned int i = 0; i < ncol; i++) {
    const NdbDictionary::Column *col = record->getColumn(i);
    uint32_t offset = record->getColumnOffset(i);
    ColumnHandler *handler = columnHandlers->getHandler(i);
    handler->init(args.GetIsolate(), col, offset);
  }
  Local<Value> jsHandlerSet = columnHandlerSetEnvelope.wrap(columnHandlers);
  SetProp(mapData, 1, jsHandlerSet);

  /* Create an Envelope and wrap it */
  Envelope *nroEnvelope = new Envelope("NdbRecordObject");
  nroEnvelope->isVO = true;
  SetProp(mapData, 2, envelopeEnvelope.wrap(nroEnvelope));

  /* Set the Prototype in the mapData */
  SetProp(mapData, 3, args[2]);

  /* Create accessors for the mapped fields in the instance template.
     AccessorInfo.Data() for the accessor will hold the field number.
  */
  Local<Object> jsFields = ArgToObject(args, 1);
  for (unsigned int i = 0; i < ncol; i++) {
    Local<Value> fieldNumber = v8::Number::New(args.GetIsolate(), i);
    Local<String> fieldName = ElementToString(jsFields, i);
    nroEnvelope->addAccessor(fieldName, nroGetter, nroSetter, fieldNumber);
  }

  /* The generic constructor is the CallHandler */
  ft->SetCallHandler(nroConstructor, mapData);
  DEBUG_PRINT("Template fields: %d",
              ft->InstanceTemplate()->InternalFieldCount());

  Local<Function> f = ft->GetFunction(JSCONTEXT(args)).ToLocalChecked();
  args.GetReturnValue().Set(scope.Escape(f));
}

void isValueObject(const Arguments &args) {
  bool answer = false;
  Local<Value> v = args[0];

  if (v->IsObject()) {
    Local<Object> o = ToObject(args, v);
    if (o->InternalFieldCount() == 2) {
      Envelope *n = (Envelope *)o->GetAlignedPointerFromInternalField(0);
      answer = n->isVO;
    }
  }

  args.GetReturnValue().Set(answer);
}

void getValueObjectWriteCount(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  NdbRecordObject *nro = unwrapPointer<NdbRecordObject *>(ArgToObject(args, 0));
  args.GetReturnValue().Set(nro->getWriteCount());
}

void prepareForUpdate(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  NdbRecordObject *nro = unwrapPointer<NdbRecordObject *>(ArgToObject(args, 0));
  args.GetReturnValue().Set(scope.Escape(nro->prepare()));
}

void ValueObject_initOnLoad(Local<Object> target) {
  DEFINE_JS_FUNCTION(target, "getValueObjectConstructor",
                     getValueObjectConstructor);
  DEFINE_JS_FUNCTION(target, "isValueObject", isValueObject);
  DEFINE_JS_FUNCTION(target, "getValueObjectWriteCount",
                     getValueObjectWriteCount);
  DEFINE_JS_FUNCTION(target, "prepareForUpdate", prepareForUpdate);
  DEFINE_JS_FUNCTION(target, "getValueObjectFieldByNumber",
                     nroGetFieldByNumber);
}
