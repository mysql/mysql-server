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

void gcWeakRefCallback(Persistent<Value>, void*);


class NdbRecordObject {
public:
  NdbRecordObject(Record *, char *);
  ~NdbRecordObject();

  Handle<Value> getField(int nField) {
    if(record->isNull(nField, buffer))
      return Null();
    else
      return proxy[nField].get(record->getColumn(nField), buffer, 
                               record->getColumnOffset(nField));
  };
  
  void setField(int nField, Handle<Value> value) {
    maskIn(nField);
    proxy[nField].set(value);
  };

  Handle<Value> prepare();

private:
  Record * record;
  char * buffer;
  const unsigned int & ncol;
  ColumnProxy * const proxy;
  uint8_t row_mask[4];

  void maskIn(unsigned int nField) {
    row_mask[nField >> 3] |= (1 << (nField & 7));
  }
  
  bool isMaskedIn(unsigned int nField) {
    return (row_mask[nField >> 3] & (1<<(nField & 7)));
  }
};


NdbRecordObject::NdbRecordObject(Record *_record, char *_buffer) : 
  record(_record), 
  buffer(_buffer),
  ncol(record->getNoOfColumns()),
  proxy(new ColumnProxy[record->getNoOfColumns()])
{
  row_mask[3] = row_mask[2] = row_mask[1] = row_mask[0] = 0;
  for(unsigned int i = 0; i < ncol ; i++) {
    proxy[i].setColumn(record->getColumn(i));
  }
}


NdbRecordObject::~NdbRecordObject() {
  delete[] proxy;
}


Handle<Value> NdbRecordObject::prepare() {
  HandleScope scope;
  Handle<Value> savedError = Undefined();
  Handle<Value> error;
  for(unsigned int i = 0 ; i < ncol ; i++) {
    if(isMaskedIn(i)) {
      error = proxy[i].write(record, i, buffer);
      if(! error->IsUndefined()) savedError = error;
    }
  }
  return scope.Close(savedError);
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
    // You could assert here that buffer size == record buffer size
    Handle<Object> thisNro = args.This();
    // Do we need to keep a persistent handle to the buffer?
    char * buffer = node::Buffer::Data(args[0]->ToObject());
    Local<Object> mapData = args.Data()->ToObject();
    
    Record * record = unwrapPointer<Record *>(mapData->Get(0)->ToObject());
    NdbRecordObject * nro = new NdbRecordObject(record, buffer);
    thisNro->SetPointerInInternalField(0, & nroEnvelope); 
    thisNro->SetPointerInInternalField(1, nro);

    // TODO: Apply the TypeConverters from mapData

    // TODO: Expose JS wrapper for NdbRecordObject::prepare()
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
  inst->SetInternalFieldCount(3);

  // Store the record in the mapData
  Local<Object> mapData = Object::New();
  mapData->Set(0, Persistent<Value>::New(args[0]));
  Record * record = unwrapPointer<Record *>(args[0]->ToObject());

  // Create accessors for the mapped fields in the instance template
  Local<Object> vFields = args[1]->ToObject();
  for(unsigned int i = 0 ; i < record->getNoOfColumns() ; i++) {
    char fieldbuf[100];
    vFields->Get(i)->ToString()->WriteAscii(fieldbuf);
    DEBUG_PRINT("Accessor for %s", fieldbuf);
    inst->SetAccessor(vFields->Get(i)->ToString(), 
                      nroGetter, nroSetter, 
                      Number::New(i));
  }

  // TODO: Store TypeConverters in the mapData
  
  // The generic constructor is the CallHandler
  ft->SetCallHandler(nroConstructor, Persistent<Object>::New(mapData));

  return scope.Close(ft->GetFunction());
}


void NdbRecordObject_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "getValueObjectConstructor",  NroConstructorBuilder);
}
