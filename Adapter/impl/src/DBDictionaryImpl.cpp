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

#include <string.h>

#include <node.h>
#include <NdbApi.hpp>

#include "adapter_global.h"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NdbWrappers.h"

using namespace v8;


/**** Dictionary implementation
 *
 * getTable(), listIndexes(), and listTables() should run in a uv background 
 * thread, as they may require network waits.
 *
 * Looking at NdbDictionaryImpl.cpp, any method that calls into 
 * NdbDictInterface (m_receiver) might block.  Any method that blocks 
 * will cause the ndb's WaitMetaRequestCount to increment.
 *
 * We assume that once a table has been fetched, all NdbDictionary::getColumn() 
 * calls are immediately served from the local dictionary cache.
 * 
 * After all background calls return, methods that create JavaScript objects 
 * can run in the main thread.
 * 
*/

/*
 * A note on getTable():
 *   In addition to the user-visible fields, the returned value wraps some 
 *   NdbDictionary objects.
 *   The TableMetadata wraps an NdbDictionary::Table
 *   The ColumnMetadata objects each wrap an NdbDictionary::Column
 *   The IndexMetadta objects for SECONDARY indexes wrap an NdbDictionary::Index,
 *    -- but IndexMetadta for PK does *not* wrap any native object!
*/
Envelope NdbDictTableEnv("const NdbDictionary::Table");
Envelope NdbDictColumnEnv("const NdbDictionary::Column");
Envelope NdbDictIndexEnv("const NdbDictionary::Index");

Handle<Value> getColumnType(const NdbDictionary::Column *);
Handle<Value> getIntColumnUnsigned(const NdbDictionary::Column *);
Handle<Value> getDefaultValue(const NdbDictionary::Column *);

Envelope * getNdbDictTableEnvelope() {
  return & NdbDictTableEnv;
}

/*** DBDictionary.listTables()
  **
   **/
class ListTablesCall : public NativeCFunctionCall_2_<int, Ndb *, const char *> 
{
private:
  NdbDictionary::Dictionary * dict;
  NdbDictionary::Dictionary::List list;

public:
  /* Constructor */
  ListTablesCall(const Arguments &args) :
    NativeCFunctionCall_2_<int, Ndb *, const char *>(NULL, args),
    list() 
  {
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run() {
    dict = arg0->getDictionary();
    return_val = dict->listObjects(list, NdbDictionary::Object::UserTable);
  }

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);
};


void ListTablesCall::doAsyncCallback(Local<Object> ctx) {
  DEBUG_MARKER(UDEB_DETAIL);
  Handle<Value> cb_args[2];
  const char * & dbName = arg1;
  
  DEBUG_PRINT("RETURN VAL: %d", return_val);
  if(return_val == -1) {
    cb_args[0] = NdbError_Wrapper(dict->getNdbError());
    cb_args[1] = Null();
  }
  else {
    cb_args[0] = Null(); // no error
    /* ListObjects has returned tables in all databases; 
       we need to filter here on database name. */
    int * stack = new int[list.count];
    unsigned int nmatch = 0;
    for(unsigned i = 0; i < list.count ; i++) {
      if(strcmp(dbName, list.elements[i].database) == 0) {
        stack[nmatch++] = i;
      }
    }
    DEBUG_PRINT("arg1/nmatch/list.count: %s/%d/%d", arg1,nmatch,list.count);

    Local<Array> cb_list = Array::New(nmatch);
    for(unsigned int i = 0; i < nmatch ; i++) {
      cb_list->Set(i, String::New(list.elements[stack[i]].name));
    }
    cb_args[1] = cb_list;
    delete[] stack;
  }
  callback->Call(ctx, 2, cb_args);
}


/* listTables() Method call
   ASYNC
   arg0: Ndb *
   arg1: database name
   arg2: user_callback
*/
Handle<Value> listTables(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);  
  REQUIRE_ARGS_LENGTH(3);
  
  ListTablesCall * ncallptr = new ListTablesCall(args);

  DEBUG_PRINT("listTables in database: %s", ncallptr->arg1);
  ncallptr->runAsync();

  return Undefined();
}


/*** DBDictionary.getTable()
  **
   **/
class GetTableCall : public NativeCFunctionCall_3_<int, Ndb *, 
                                                   const char *, const char *> 
{
private:
  const NdbDictionary::Table * ndb_table;
  Ndb * per_table_ndb;
  NdbDictionary::Dictionary * dict;
  NdbDictionary::Dictionary::List idx_list;
  const NdbError * ndbError;
  
  Handle<Object> buildDBIndex_PK();
  Handle<Object> buildDBIndex(const NdbDictionary::Index *);
  Handle<Object> buildDBColumn(const NdbDictionary::Column *);

public:
  /* Constructor */
  GetTableCall(const Arguments &args) : 
    NativeCFunctionCall_3_<int, Ndb *, const char *, const char *>(NULL, args),
    ndb_table(0), per_table_ndb(0), idx_list()
  {
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run();

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);  
};


void GetTableCall::run() {
  DEBUG_PRINT("GetTableCall::run() [%s.%s]", arg1, arg2);
  return_val = -1;
  /* Aliases: */
  Ndb * & ndb = arg0;
  const char * & dbName = arg1;
  const char * & tableName = arg2;
  
  if(strlen(dbName)) {
    arg0->setDatabaseName(dbName);
  }
  dict = ndb->getDictionary();
  ndb_table = dict->getTable(tableName);
  if(ndb_table) {
    /* Ndb object used to create NdbRecords and to cache auto-increment values */
    per_table_ndb = new Ndb(& ndb->get_ndb_cluster_connection());
    per_table_ndb->init();

    /* List the indexes */
    return_val = dict->listIndexes(idx_list, tableName);
  }
  if(return_val == 0) {
    /* Fetch the indexes now.  These calls may perform network IO, populating 
       the (connection) global and (Ndb) local dictionary caches.  Later,
       in the JavaScript main thread, we will call getIndex() again knowing
       that the caches are populated.
    */
    for(unsigned int i = 0 ; i < idx_list.count ; i++) { 
      const NdbDictionary::Index * idx = dict->getIndex(idx_list.elements[i].name, tableName);
      /* It is possible to get an index for a recently dropped table rather 
         than the desired table.  This is a known bug likely to be fixed later.
      */
      const char * idx_table_name = idx->getTable();
      const NdbDictionary::Table * idx_table = dict->getTable(idx_table_name);
      if(idx_table == 0 || idx_table->getObjectVersion() != ndb_table->getObjectVersion()) 
      {
        dict->invalidateIndex(idx);
        idx = dict->getIndex(idx_list.elements[i].name, tableName);
      }
    }
  }
  else {
    ndbError = & dict->getNdbError();
  }
}


/* doAsyncCallback() runs in the main thread.  We don't want it to block.
   TODO: verify whether any IO is done 
         by checking WaitMetaRequestCount at the start and end.
*/    
void GetTableCall::doAsyncCallback(Local<Object> ctx) {
  HandleScope scope;  
  DEBUG_PRINT("GetTableCall::doAsyncCallback: return_val %d", return_val);

  /* User callback arguments */
  Handle<Value> cb_args[2];
  cb_args[0] = Null();
  cb_args[1] = Null();
  
  /* TableMetadata = {
      database         : ""    ,  // Database name
      name             : ""    ,  // Table Name
      columns          : []    ,  // ordered array of DBColumn objects
      indexes          : []    ,  // array of DBIndex objects 
      partitionKey     : []    ,  // ordered array of column numbers in the partition key
    };
  */    
  if(ndb_table && ! return_val) {

    Local<Object> table = NdbDictTableEnv.newWrapper();
    wrapPointerInObject(ndb_table, NdbDictTableEnv, table);

    // database
    table->Set(String::NewSymbol("database"), String::New(arg1));
    
    // name
    table->Set(String::NewSymbol("name"), String::New(ndb_table->getName()));

    // partitionKey
    int nPartitionKeys = 0;
    Handle<Array> partitionKeys = Array::New();
    table->Set(String::NewSymbol("partitionKey"), partitionKeys);
    
    // columns
    Local<Array> columns = Array::New(ndb_table->getNoOfColumns());
    for(int i = 0 ; i < ndb_table->getNoOfColumns() ; i++) {
      const NdbDictionary::Column *ndb_col = ndb_table->getColumn(i);
      Handle<Object> col = buildDBColumn(ndb_col);
      columns->Set(i, col);
      if(ndb_col->getPartitionKey()) { /* partition key */
        partitionKeys->Set(nPartitionKeys++, String::New(ndb_col->getName()));
      }
    }
    table->Set(String::NewSymbol("columns"), columns);

    // indexes (primary key & secondary) 
    Local<Array> js_indexes = Array::New(idx_list.count + 1);
    js_indexes->Set(0, buildDBIndex_PK());                   // primary key
    for(unsigned int i = 0 ; i < idx_list.count ; i++) {   // secondary indexes
      const NdbDictionary::Index * idx =
        dict->getIndex(idx_list.elements[i].name, arg2);
      js_indexes->Set(i+1, buildDBIndex(idx));
    }    
    table->Set(String::NewSymbol("indexes"), js_indexes, ReadOnly);
  
    // Table Record (implementation artifact; not part of spec)
    DEBUG_PRINT("Creating Table Record");
    Record * rec = new Record(dict, ndb_table->getNoOfColumns());
    for(int i = 0 ; i < ndb_table->getNoOfColumns() ; i++) {
      rec->addColumn(ndb_table->getColumn(i));
    }
    rec->completeTableRecord(ndb_table);

    table->Set(String::NewSymbol("record"), Record_Wrapper(rec));    

    // Autoincrement Cache Impl (also not part of spec)
    if(per_table_ndb) {
      table->Set(String::NewSymbol("per_table_ndb"), Ndb_Wrapper(per_table_ndb));
    }
    
    // User Callback
    cb_args[1] = table;
  }
  else {
    cb_args[0] = NdbError_Wrapper(* ndbError);
  }
  
  callback->Call(ctx, 2, cb_args);
}


/* 
DBIndex = {
  name             : ""    ,  // Index name; undefined for PK 
  isPrimaryKey     : true  ,  // true for PK; otherwise undefined
  isUnique         : true  ,  // true or false
  isOrdered        : true  ,  // true or false; can scan if true
  columnNumbers    : []    ,  // an ordered array of column numbers
};
*/
Handle<Object> GetTableCall::buildDBIndex_PK() {
  HandleScope scope;
  
  Local<Object> obj = Object::New();

  obj->Set(String::NewSymbol("isPrimaryKey"), Boolean::New(true), ReadOnly);
  obj->Set(String::NewSymbol("isUnique"),     Boolean::New(true), ReadOnly);
  obj->Set(String::NewSymbol("isOrdered"),    Boolean::New(false), ReadOnly);

  /* Loop over the columns of the key. 
     Build the "columnNumbers" array and the "record" object, then set both.
  */  
  int ncol = ndb_table->getNoOfPrimaryKeys();
  DEBUG_PRINT("Creating Primary Key Record");
  Record * pk_record = new Record(dict, ncol);
  Local<Array> idx_columns = Array::New(ncol);
  for(int i = 0 ; i < ncol ; i++) {
    const char * col_name = ndb_table->getPrimaryKey(i);
    const NdbDictionary::Column * col = ndb_table->getColumn(col_name);
    pk_record->addColumn(col);
    idx_columns->Set(i, v8::Int32::New(col->getColumnNo()));
  }
  pk_record->completeTableRecord(ndb_table);

  obj->Set(String::NewSymbol("columnNumbers"), idx_columns);
  obj->Set(String::NewSymbol("record"), Record_Wrapper(pk_record), ReadOnly);
 
  return scope.Close(obj);
}


Handle<Object> GetTableCall::buildDBIndex(const NdbDictionary::Index *idx) {
  HandleScope scope;
  
  Local<Object> obj = NdbDictIndexEnv.newWrapper();
  wrapPointerInObject(idx, NdbDictIndexEnv, obj);

  obj->Set(String::NewSymbol("name"), String::New(idx->getName()));
  obj->Set(String::NewSymbol("isUnique"),     
           Boolean::New(idx->getType() == NdbDictionary::Index::UniqueHashIndex),
           ReadOnly);
  obj->Set(String::NewSymbol("isOrdered"),
           Boolean::New(idx->getType() == NdbDictionary::Index::OrderedIndex),
           ReadOnly);
  
  /* Loop over the columns of the key. 
     Build the "columns" array and the "record" object, then set both.
  */  
  int ncol = idx->getNoOfColumns();
  Local<Array> idx_columns = Array::New(ncol);
  DEBUG_PRINT("Creating Index Record (%s)", idx->getName());
  Record * idx_record = new Record(dict, ncol);
  for(int i = 0 ; i < ncol ; i++) {
    const char *colName = idx->getColumn(i)->getName();
    const NdbDictionary::Column *col = ndb_table->getColumn(colName);
    idx_columns->Set(i, v8::Int32::New(col->getColumnNo()));
    idx_record->addColumn(col);
  }
  idx_record->completeIndexRecord(idx);
  obj->Set(String::NewSymbol("record"), Record_Wrapper(idx_record), ReadOnly);
  obj->Set(String::NewSymbol("columnNumbers"), idx_columns);
  
  return scope.Close(obj);
}


Handle<Object> GetTableCall::buildDBColumn(const NdbDictionary::Column *col) {
  HandleScope scope;
  
  Local<Object> obj = NdbDictColumnEnv.newWrapper();
  wrapPointerInObject(col, NdbDictColumnEnv, obj);  
  
  NdbDictionary::Column::Type col_type = col->getType();
  bool is_int = (col_type <= NDB_TYPE_BIGUNSIGNED);
  bool is_dec = ((col_type == NDB_TYPE_DECIMAL) || (col_type == NDB_TYPE_DECIMALUNSIGNED));
  bool is_binary = ((col_type == NDB_TYPE_BLOB) || (col_type == NDB_TYPE_BINARY) 
                   || (col_type == NDB_TYPE_VARBINARY) || (col_type == NDB_TYPE_LONGVARBINARY));
  bool is_char = ((col_type == NDB_TYPE_CHAR) || (col_type == NDB_TYPE_TEXT)
                   || (col_type == NDB_TYPE_VARCHAR) || (col_type == NDB_TYPE_LONGVARCHAR));

  /* Required Properties */

  obj->Set(String::NewSymbol("name"),
           String::New(col->getName()),
           ReadOnly);
  
  obj->Set(String::NewSymbol("columnNumber"),
           v8::Int32::New(col->getColumnNo()),
           ReadOnly);
  
  obj->Set(String::NewSymbol("columnType"), 
           getColumnType(col), 
           ReadOnly);

  obj->Set(String::NewSymbol("isIntegral"),
           Boolean::New(is_int),
           ReadOnly);

  obj->Set(String::NewSymbol("isNullable"),
           Boolean::New(col->getNullable()),
           ReadOnly);

  obj->Set(String::NewSymbol("isInPrimaryKey"),
           Boolean::New(col->getPrimaryKey()),
           ReadOnly);

  obj->Set(String::NewSymbol("columnSpace"),
           v8::Int32::New(col->getSizeInBytes()),
           ReadOnly);           

  /* Implementation-specific properties */
  
  obj->Set(String::NewSymbol("ndbTypeId"),
           v8::Int32::New(static_cast<int>(col->getType())),
           ReadOnly);

  obj->Set(String::NewSymbol("ndbRawDefaultValue"),
           getDefaultValue(col), 
           ReadOnly);
  
  /* Optional Properties, depending on columnType */
  /* Group A: Numeric */
  if(is_int || is_dec) {
    obj->Set(String::NewSymbol("isUnsigned"),
             getIntColumnUnsigned(col),
             ReadOnly);
  }
  
  if(is_int) {    
    obj->Set(String::NewSymbol("intSize"),
             v8::Int32::New(col->getSizeInBytes()),
             ReadOnly);
  }
  
  if(is_dec) {
    obj->Set(String::NewSymbol("scale"),
             v8::Int32::New(col->getScale()),
             ReadOnly);
    
    obj->Set(String::NewSymbol("precision"),
             v8::Int32::New(col->getPrecision()),
             ReadOnly);
  }

  obj->Set(String::NewSymbol("isAutoincrement"),
           Boolean::New(col->getAutoIncrement()),
           ReadOnly);
   
  /* Group B: Non-numeric */
  if(is_binary || is_char) {  
    obj->Set(String::NewSymbol("length"),
             v8::Int32::New(col->getLength()),
             ReadOnly);

    obj->Set(String::NewSymbol("isBinary"),
             Boolean::New(is_binary),
             ReadOnly);
  
    if(is_char) {
      obj->Set(String::NewSymbol("charsetNumber"),
               Number::New(col->getCharsetNumber()),
               ReadOnly);
      // todo: charsetName
    }
  }
    
  return scope.Close(obj);
} 


/* getTable() method call
   ASYNC
   arg0: Ndb *
   arg1: database name
   arg2: table name
   arg3: user_callback
*/
Handle<Value> getTable(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(4);
  GetTableCall * ncallptr = new GetTableCall(args);
  ncallptr->runAsync();
  return Undefined();
}


Handle<Value> getColumnType(const NdbDictionary::Column * col) {
  HandleScope scope;

  /* Based on ndb_constants.h */
  const char * typenames[NDB_TYPE_MAX] = {
    "",               // 0
    "TINYINT",        // 1  TINY INT
    "TINYINT",        // 2  TINY UNSIGNED
    "SMALLINT",       // 3  SMALL INT
    "SMALLINT",       // 4  SMALL UNSIGNED
    "MEDIUMINT",      // 5  MEDIUM INT
    "MEDIUMINT",      // 6  MEDIUM UNSIGNED
    "INT",            // 7  INT
    "INT",            // 8  UNSIGNED
    "BIGINT",         // 9  BIGINT
    "BIGINT",         // 10 BIG UNSIGNED
    "FLOAT",          // 11
    "DOUBLE",         // 12
    "",               // OLDDECIMAL
    "CHAR",           // 14
    "VARCHAR",        // 15
    "BINARY",         // 16
    "VARBINARY",      // 17
    "DATETIME",       // 18
    "DATE",           // 19
    "BLOB",           // 20
    "TEXT",           // 21 TEXT
    "BIT",            // 22 
    "VARCHAR",        // 23 LONGVARCHAR
    "VARBINARY",      // 24 LONGVARBINARY
    "TIME",           // 25
    "YEAR",           // 26
    "TIMESTAMP",      // 27
    "",               // 28 OLDDECIMAL UNSIGNED
    "DECIMAL",        // 29 DECIMAL
    "DECIMAL"         // 30 DECIMAL UNSIGNED
#if NDB_TYPE_MAX > 31
    "TIME",          // 31 TIME2
    "DATETIME",      // 32 DATETIME2
    "TIMESTAMP",     // 33 TIMESTAMP2
#endif
  };

  const char * name = typenames[col->getType()];

  if(name == 0 || *name == 0) {   /* One of the undefined types */
    return scope.Close(Undefined());
  }

  Local<String> s = String::New(name);
  
  return scope.Close(s);
}


Handle<Value> getIntColumnUnsigned(const NdbDictionary::Column *col) {
  HandleScope scope;
     
  switch(col->getType()) {
    case NdbDictionary::Column::Unsigned:
    case NdbDictionary::Column::Bigunsigned:
    case NdbDictionary::Column::Smallunsigned:
    case NdbDictionary::Column::Tinyunsigned:
    case NdbDictionary::Column::Mediumunsigned:
      return scope.Close(Boolean::New(true));
    
    default:
      return scope.Close(Boolean::New(false));
  }
}

Handle<Value> createJsBuffer(node::Buffer *b, int len) {
  HandleScope scope;
  Local<Object> global = v8::Context::GetCurrent()->Global();
  Local<Function> bufferConstructor = 
    Local<Function>::Cast(global->Get(String::New("Buffer")));
  Handle<Value> args[3];
  args[0] = b->handle_;
  args[1] = Integer::New(len);
  args[2] = Integer::New(0);
  
  Local<Object> jsBuffer = bufferConstructor->NewInstance(3, args);

  return scope.Close(jsBuffer);
}


/* TODO: Probably we don't need the default value itself in JavaScript;
   merely a flag indicating that the column has a non-null default value
*/
Handle<Value> getDefaultValue(const NdbDictionary::Column *col) {
  HandleScope scope;
  Handle<Value> v;
  unsigned int defaultLen = 0;
  
  const void* dictDefaultBuff = col->getDefaultValue(& defaultLen);
  if(defaultLen) {
    node::Buffer *buf = node::Buffer::New((char *) dictDefaultBuff, defaultLen);
    v = createJsBuffer(buf, defaultLen);
  }
  else {
    v = v8::Null();
  }
  return scope.Close(v);
}


/* arg0: TableMetadata wrapping NdbDictionary::Table *
   arg1: Ndb *
   arg2: number of columns
   arg3: array of NdbDictionary::Column *
*/
Handle<Value> getRecordForMapping(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  NdbDictionary::Table *table = 
    unwrapPointer<NdbDictionary::Table *>(args[0]->ToObject());
  Ndb * ndb = unwrapPointer<Ndb *>(args[1]->ToObject());
  unsigned int nColumns = args[2]->Int32Value();
  Record * record = new Record(ndb->getDictionary(), nColumns);
  for(unsigned int i = 0 ; i < nColumns ; i++) {
    NdbDictionary::Column * col = 
      unwrapPointer<NdbDictionary::Column *>
        (args[3]->ToObject()->Get(i)->ToObject());
    record->addColumn(col);
  }
  record->completeTableRecord(table);

  return scope.Close(Record_Wrapper(record));
}


void DBDictionaryImpl_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  Persistent<Object> dbdict_obj = Persistent<Object>(Object::New());

  DEFINE_JS_FUNCTION(dbdict_obj, "listTables", listTables);
  DEFINE_JS_FUNCTION(dbdict_obj, "getTable", getTable);
  DEFINE_JS_FUNCTION(dbdict_obj, "getRecordForMapping", getRecordForMapping);

  target->Set(Persistent<String>(String::NewSymbol("DBDictionary")), dbdict_obj);
}

