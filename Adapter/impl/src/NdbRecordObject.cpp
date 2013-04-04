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
#include "unified_debug.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"
#include "ColumnProxy.h"


/************************************************************
          Value Object, a.k.a "VO" or "Native-Backed Object"
          
 A VO consists of:
    a Buffer holding data read from NDB
    an NdbRecord (wrapped by a Record) describing the layout of the buffer 
    the NdbRecordObject, which provides a JavaScript shim for the values 
      stored in the buffer 

 The ValueObject defines setters and getters for the mapped fields and
 directs them to ColumnProxies. 

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


/* TODO: 
   VO needs a persistent handle on its buffer 
   When VO is made weak, it disposes buffer
   
   VOC needs a persistent handle on its Record
   If VOC is ever made weak (though we don't expect this) 
   it will dispose this handle
*/

Handle<String> K_toDB, K_fromDB;

void gcWeakRefCallback(Persistent<Value>, void*);

class ColumnHandlerSet {
public:
  ColumnHandlerSet(int _size) : size(_size), handlers(new ColumnHandler[size]) { }
  ~ColumnHandlerSet() { delete[] handlers; }  
  ColumnHandler * getHandler(int i) { assert(i < size); return & handlers[i]; }
private:
  int size;
  ColumnHandler * const handlers;
};

Envelope columnHandlerSetEnvelope("ColumnHandlerSet");


class NdbRecordObject {
public:
  NdbRecordObject(Record *, ColumnHandlerSet *, Handle<Value>);
  ~NdbRecordObject();
  
  Handle<Value> getField(int);
  void setField(int nField, Handle<Value> value);
  Handle<Value> prepare();

private:
  Record * record;
  char * buffer;
  ColumnHandlerSet * handlers;
  Persistent<Value> persistentBufferHandle;
  const unsigned int & ncol;
  ColumnProxy * const proxy;
  uint8_t row_mask[4];
  void maskIn(unsigned int nField);
  bool isMaskedIn(unsigned int nField);
};


NdbRecordObject::NdbRecordObject(Record *_record, 
                                 ColumnHandlerSet * _handlers,
                                 Handle<Value> jsBuffer) : 
  record(_record), 
  handlers(_handlers),
  ncol(record->getNoOfColumns()),
  proxy(new ColumnProxy[record->getNoOfColumns()])
{
  DEBUG_MARKER(UDEB_DEBUG);

  /* Retain a handler on the buffer for our whole lifetime */
  persistentBufferHandle = Persistent<Value>::New(jsBuffer);
  buffer = node::Buffer::Data(jsBuffer->ToObject());  
  // You could assert here that buffer size == record buffer size

  /* Initialize the list of masked-in columns */
  row_mask[3] = row_mask[2] = row_mask[1] = row_mask[0] = 0;
  
  /* Attach the column proxies to their handlers */
  for(unsigned int i = 0 ; i < ncol ; i++)
    proxy[i].setHandler(handlers->getHandler(i));
}


NdbRecordObject::~NdbRecordObject() {
  DEBUG_MARKER(UDEB_DEBUG);
  if(! persistentBufferHandle.IsEmpty()) 
    persistentBufferHandle.Dispose();
  delete[] proxy;
}


Handle<Value> NdbRecordObject::getField(int nField) {
  if(record->isNull(nField, buffer))
    return Null();
  else
    return proxy[nField].get(buffer);
}


inline void NdbRecordObject::setField(int nField, Handle<Value> value) {
  maskIn(nField); 
  proxy[nField].set(value);
}


Handle<Value> NdbRecordObject::prepare() {
  HandleScope scope;
  Handle<Value> writeStatus;
  Handle<Value> savedError = Undefined();
  for(unsigned int i = 0 ; i < ncol ; i++) {
    if(isMaskedIn(i)) {
      if(proxy[i].isNull) {
        record->setNull(i, buffer);
      }
      else {
        writeStatus = proxy[i].write(buffer);
        if(! writeStatus->IsUndefined()) savedError = writeStatus;
      }
    }
  }
  return scope.Close(savedError);
}


inline void NdbRecordObject::maskIn(unsigned int nField) {
  assert(nField < ncol);
  row_mask[nField >> 3] |= (1 << (nField & 7));
}

  
inline bool NdbRecordObject::isMaskedIn(unsigned int nField) {
  assert(nField < ncol);
  return (row_mask[nField >> 3] & (1<<(nField & 7)));
}

Envelope nroEnvelope("NdbRecordObject");


/************************************************************************/

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
 * arg0: buffer
*/
Handle<Value> nroConstructor(const Arguments &args) {
  HandleScope scope;

  if(args.IsConstructCall()) {
    /* Unwrap record from mapData */
    Local<Object> mapData = args.Data()->ToObject();
    Record * record = 
      unwrapPointer<Record *>(mapData->Get(0)->ToObject());

    /* Unwrap Column Handlers from mapData */
    ColumnHandlerSet * handlers = 
      unwrapPointer<ColumnHandlerSet *>(mapData->Get(1)->ToObject());

    /* Build NdbRecordObject */
    NdbRecordObject * nro = new NdbRecordObject(record, handlers, args[0]);

    // TODO: Expose JS wrapper for NdbRecordObject::prepare()

    /* Wrap for JavaScript */
    wrapPointerInObject<NdbRecordObject *>(nro, nroEnvelope, args.This());
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
Handle<Value> NroConstructorBuilder(const Arguments &args) {
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
  Record * record = unwrapPointer<Record *>(args[0]->ToObject());
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

  /* Create accessors for the mapped fields in the instance template
     AccessorInfo.Data() for the accessor will hold the field number 
  */
  Local<Object> jsFields = args[1]->ToObject();
  for(unsigned int i = 0 ; i < ncol; i++) {
    Handle<String> fieldName = jsFields->Get(i)->ToString();
    inst->SetAccessor(fieldName, nroGetter, nroSetter, Number::New(i));
  }

  /* The generic constructor is the CallHandler */
  ft->SetCallHandler(nroConstructor, Persistent<Object>::New(mapData));

  return scope.Close(ft->GetFunction());
}


void NdbRecordObject_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "getValueObjectConstructor",  NroConstructorBuilder);
}
