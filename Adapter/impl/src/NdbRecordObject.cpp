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
#include "Record.h"
#include "NdbTypeEncoders.h"

void gcWeakRefCallback(Persistent<Value>, void*);


/* QUESTION: who is responsible for creating the Node Buffer?
   If MappedNdbRecord is ...
   If the caller is ... 
*/
class MappedNdbRecord {
public:
  MappedNdbRecord(Record *);
  ~MappedNdbRecord();

  Handle<Value> getBuffer();
  Handle<Value> getField(Local<String>, const AccessorInfo &);
  void setField(Local<String>, Local<Value>, const AccessorInfo&);

private:
  Record * record;
  int ncol;
  node::Buffer * buffer;
  Handle<Object> jsBuffer;
  NdbTypeEncoder * encoders[];        // TypeEncoders for each column
  Handle<Value> cachedConversions[];  // cached of converted values
};  

MappedNdbRecord::MappedNdbRecord(Record *rec) : 
  record(rec) 
{
  ncol = record->getNoOfColumns();
  buffer = node::Buffer::New(record->getBufferSize());
  //jsBuffer = createJsBuffer(..);  // implemented in DBDictionaryImpl
}


MappedNdbRecord::~MappedNdbRecord() {
  // Free the buffer
  // Free the encoders array
  // Free the cached conversions
  


}


//Handle<Value> MappedNdbRecord::getField(Local<String> key,
//                                        const AccessorInfo & info) {
//  /* The column number is stored in info.Data() */
//
//}                                        

/* arg0: Ndb 
   arg1: NdbDictionary::Table
   arg2: resolvedMapping 
     which contains an array "fields"
     where each element has "columnName" and "fieldName" properties
   
   Returns: a constructor function that can be used to create native-backed 
   objects
*/
Handle<Value> NroBuilder(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  //Ndb * ndb =  unwrapPointer<Ndb *>(args[0]->ToObject());
  //Handle<Object> jsDbTable = args[1]->ToObject();
  //NdbDictionary::Table * table = unwrapPointer<NdbDictionary::Table *>(jsDbTable);
  
  return scope.Close(Null()); //fixme
}


void NdbRecordObject_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "NdbRecordObjectBuilder",  NroBuilder);
}
