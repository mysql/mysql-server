/*
 Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 
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
#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"

#include "adapter_global.h"
#include "TransactionImpl.h"
#include "QueryOperation.h"

#include "JsWrapper.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"

#define SET_KEY(X,Y) X.Set(isolate, NewUtf8String(isolate, Y))
#define GET_KEY(X) X.Get(isolate)

#define MAX_KEY_PARTS 8

Eternal<String>    /* keys of NdbProjection */
  K_next,
  K_root,
  K_keyFields,
  K_joinTo,
  K_serial,
  K_parent,
  K_tableHandler,
  K_rowRecord,
  K_indexHandler,
  K_keyRecord,
  K_isPrimaryKey,
  K_relatedField,
  K_dbTable,
  K_dbIndex,
  K_level,
  K_data,
  K_tag;


V8WrapperFn queryPrepareAndExecute,
            querySetTransactionImpl,
            queryFetchAllResults,
            queryGetResult,
            queryClose;


class QueryOperationEnvelopeClass : public Envelope {
public:
  QueryOperationEnvelopeClass() : Envelope("QueryOperation") {
    addMethod("prepareAndExecute", queryPrepareAndExecute);
    addMethod("setTransactionImpl", querySetTransactionImpl);
    addMethod("fetchAllResults", queryFetchAllResults);
    addMethod("getResult", queryGetResult);
    addMethod("close", queryClose);
  }
};

QueryOperationEnvelopeClass QueryOperationEnvelope;

Local<Value> QueryOperation_Wrapper(QueryOperation *queryOp) {
  Local<Value> jsobj = QueryOperationEnvelope.wrap(queryOp);
  QueryOperationEnvelope.freeFromGC(queryOp, jsobj);
  return jsobj;
}


void setRowBuffers(Isolate * isolate,
                   QueryOperation *queryOp,
                   Local<Object> spec,
                   int parentId) {
  DEBUG_ENTER();
  Record * record = 0;
  int level = GetInt32Property(isolate, spec, GET_KEY(K_serial));
  if(Get(isolate, spec, GET_KEY(K_rowRecord))->IsObject()) {
    record = unwrapPointer<Record *>(ElementToObject(spec, GET_KEY(K_rowRecord)));
  }
  assert(record);
  queryOp->createRowBuffer(level, record, parentId);

  if(Get(isolate, spec, GET_KEY(K_relatedField))->IsNull()) {
    queryOp->levelIsJoinTable(level);
  }
}


const NdbQueryOperationDef * createTopLevelQuery(Isolate * isolate,
                                                 QueryOperation *queryOp,
                                                 Local<Object> spec,
                                                 Local<Object> keyBuffer) {
  DEBUG_MARKER(UDEB_DETAIL);
  NdbQueryBuilder *builder = queryOp->getBuilder();

  /* Pull values out of the JavaScript object */
  Local<Value> v;
  const Record * keyRecord = 0;
  const NdbDictionary::Table * table = 0;
  const NdbDictionary::Index * index = 0;

  v = Get(isolate, spec, GET_KEY(K_keyRecord));
  if(v->IsObject()) {
    keyRecord = unwrapPointer<const Record *>(ToObject(isolate, v));
  };
  v = Get(spec, GET_KEY(K_tableHandler));
  if(v->IsObject()) {
    v = Get(ToObject(isolate, v), GET_KEY(K_dbTable));
    if(v->IsObject()) {
      table = unwrapPointer<const NdbDictionary::Table *>(ToObject(isolate, v));
    }
  }
  bool isPrimaryKey = GetBoolProperty(spec, GET_KEY(K_isPrimaryKey));
  const char * key_buffer = GetBufferData(keyBuffer);
  if(! isPrimaryKey) {
    v = Get(spec, GET_KEY(K_indexHandler));
    if(v->IsObject()) {
      v = Get(ToObject(isolate, v), GET_KEY(K_dbIndex));
      if(v->IsObject()) {
        index = unwrapPointer<const NdbDictionary::Index *> (ToObject(isolate, v));
      }
    }
    assert(index);
  }

  /* Build the key */
  int nKeyParts = keyRecord->getNoOfColumns();
  assert(nKeyParts <= MAX_KEY_PARTS);
  const NdbQueryOperand * key_parts[MAX_KEY_PARTS + 1];

  DEBUG_PRINT("Creating root QueryOperationDef for table: %s", table->getName());
  for(int i = 0; i < nKeyParts ; i++) {
    uint32_t offset = keyRecord->getColumnOffset(i);
    uint32_t length = keyRecord->getValueLength(i, key_buffer + offset);
    offset += keyRecord->getValueOffset(i);  // accounts for length bytes
    key_parts[i] = builder->constValue(key_buffer + offset, length);
    DEBUG_PRINT_DETAIL("Key part %d: %s", i, keyRecord->getColumn(i)->getName());
  }
  key_parts[nKeyParts] = 0;

  return queryOp->defineOperation(index, table, key_parts);
}

const NdbQueryOperationDef * createNextLevel(Isolate * isolate,
                                             QueryOperation *queryOp,
                                             Local<Object> spec,
                                             const NdbQueryOperationDef * parent) {
  DEBUG_MARKER(UDEB_DEBUG);
  NdbQueryBuilder *builder = queryOp->getBuilder();

  /* Pull values out of the JavaScript object */
  Local<Value> v;
  const NdbDictionary::Table * table = 0;
  const NdbDictionary::Index * index = 0;
  int depth = GetInt32Property(spec, GET_KEY(K_serial));

  v = Get(spec, GET_KEY(K_tableHandler));
  if(v->IsObject()) {
    v = Get(ToObject(isolate, v), GET_KEY(K_dbTable));
    if(v->IsObject()) {
      table = unwrapPointer<const NdbDictionary::Table *>(ToObject(isolate, v));
    }
  }
  bool isPrimaryKey = GetBoolProperty(isolate, spec, GET_KEY(K_isPrimaryKey));

  DEBUG_PRINT("Creating QueryOperationDef at level %d for table: %s",
              depth, table->getName());

  if(! isPrimaryKey) {
    v = Get(spec, GET_KEY(K_indexHandler));
    if(v->IsObject()) {
      v = Get(isolate, ToObject(isolate, v), GET_KEY(K_dbIndex));
      if(v->IsObject()) {
        index = unwrapPointer<const NdbDictionary::Index *>(ToObject(isolate, v));
      }
    }
    assert(index);
  }

  /* Build the key */
   Local<Object> joinColumns = ElementToObject(spec, GET_KEY(K_joinTo));
  int nKeyParts = Array::Cast(*joinColumns)->Length();
  assert(nKeyParts <= MAX_KEY_PARTS);
  const NdbQueryOperand * key_parts[MAX_KEY_PARTS + 1];

  for(int i = 0 ; i < nKeyParts ; i++) {
    String::Utf8Value column_name(isolate, Get(joinColumns, i));
    key_parts[i] = builder->linkedValue(parent, *column_name);
    DEBUG_PRINT_DETAIL("Key part %d: %s", i, *column_name);
  }
  key_parts[nKeyParts] = 0;

  return queryOp->defineOperation(index, table, key_parts);
}

/* JS QueryOperation.create(ndbRootProjection, keyBuffer, depth)
*/
void createQueryOperation(const Arguments & args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(4);
  Isolate * isolate = args.GetIsolate();

  int size = GetInt32Arg(args, 2);
  SessionImpl * sessionImpl = unwrapPointer<SessionImpl *>(ArgToObject(args, 3));

  int currentId = 0;
  int parentId;
  const NdbQueryOperationDef * current;
  const NdbQueryOperationDef ** all = new const NdbQueryOperationDef * [size];
  QueryOperation * queryOperation = new QueryOperation(size);

  Local<Value> v;
  Local<Object> spec = ArgToObject(args, 0);
  Local<Object> parentSpec;

  setRowBuffers(isolate, queryOperation, spec, 0);
  current = createTopLevelQuery(isolate, queryOperation, spec, ArgToObject(args, 1));

  while(! (v = Get(spec, GET_KEY(K_next)))->IsUndefined()) {
    all[currentId++] = current;
    spec = ToObject(args, v);
    parentSpec = ElementToObject(spec, GET_KEY(K_parent));
    parentId = GetInt32Property(parentSpec, GET_KEY(K_serial));
    current = createNextLevel(isolate, queryOperation, spec, all[parentId]);
    assert(current->getOpNo() == GetUint32Property(spec, GET_KEY(K_serial)));
    setRowBuffers(isolate, queryOperation, spec, parentId);
  }
  queryOperation->prepare(all[0], sessionImpl);
  delete[] all;
  args.GetReturnValue().Set(QueryOperation_Wrapper(queryOperation));
}

void querySetTransactionImpl(const Arguments &args) {
  REQUIRE_ARGS_LENGTH(1);

  typedef NativeVoidMethodCall_1_<QueryOperation, TransactionImpl *> MCALL;
  MCALL mcall(& QueryOperation::setTransactionImpl, args);
  mcall.run();
  
  args.GetReturnValue().SetUndefined();
}

// void prepareAndExecute() 
// ASYNC
void queryPrepareAndExecute(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeMethodCall_0_<int, QueryOperation> MCALL;
  MCALL * mcallptr = new MCALL(& QueryOperation::prepareAndExecute, args);
  mcallptr->errorHandler = getNdbErrorIfLessThanZero;
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

// fetchAllResults()
// ASYNC
void queryFetchAllResults(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeMethodCall_0_<int, QueryOperation> MCALL;
  MCALL * mcallptr = new MCALL(& QueryOperation::fetchAllResults, args);
  mcallptr->errorHandler = getNdbErrorIfLessThanZero;
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

void freeQueryResultAtGC(char *data, void *hint) {
  (void) hint;   // unused
  free(data);
}

void doNotFreeQueryResultAtGC(char *data, void *hint) {
  (void) hint;
  (void) data;
}

// getResult(id, objectWrapper):  IMMEDIATE
void queryGetResult(const Arguments & args) {
  REQUIRE_ARGS_LENGTH(2);
  v8::Isolate * isolate = args.GetIsolate();

  QueryOperation * op = unwrapPointer<QueryOperation *>(args.Holder());
  uint32_t id = GetUint32Arg(args, 0);
  Local<Object> wrapper = ArgToObject(args, 1);

  QueryResultHeader * header = op->getResult(id);

  if(header) {
    if(header->data) {
      SetProp(wrapper, GET_KEY(K_data), NewJsBuffer(isolate, header->data,
              op->getResultRowSize(header->sector), doNotFreeQueryResultAtGC));
    } else {
      SetProp(wrapper, GET_KEY(K_data), Null(isolate));
    }
    SetProp(wrapper, GET_KEY(K_level), v8::Uint32::New(isolate, header->sector));
    SetProp(wrapper, GET_KEY(K_tag),   v8::Uint32::New(isolate, header->tag));
    args.GetReturnValue().Set(true);
  } else {
    args.GetReturnValue().Set(false);
  }
}

// void close()
// ASYNC
void queryClose(const Arguments & args) {
  typedef NativeVoidMethodCall_0_<QueryOperation> NCALL;
  NCALL * ncallptr = new NCALL(& QueryOperation::close, args);
  ncallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

void QueryOperation_initOnLoad(Local<Object> target) {
  Isolate * isolate = target->GetIsolate();
  Local<Object> ibObj = Object::New(isolate);
  SetProp(target, "QueryOperation", ibObj);
  DEFINE_JS_FUNCTION(ibObj, "create", createQueryOperation);

  SET_KEY(K_next, "next");
  SET_KEY(K_root, "root");
  SET_KEY(K_keyFields, "keyFields");
  SET_KEY(K_joinTo, "joinTo");
  SET_KEY(K_serial, "serial");
  SET_KEY(K_parent, "parent");
  SET_KEY(K_tableHandler, "tableHandler");
  SET_KEY(K_rowRecord, "rowRecord"),
  SET_KEY(K_indexHandler, "indexHandler");
  SET_KEY(K_keyRecord, "keyRecord");
  SET_KEY(K_isPrimaryKey, "isPrimaryKey");
  SET_KEY(K_relatedField, "relatedField");

  SET_KEY(K_dbTable, "dbTable");
  SET_KEY(K_dbIndex, "dbIndex");

  SET_KEY(K_level, "level");
  SET_KEY(K_data, "data");
  SET_KEY(K_tag, "tag");
}

