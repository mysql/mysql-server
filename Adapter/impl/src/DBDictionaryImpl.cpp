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

#include <v8.h>

#include "adapter_global.h"
#include "NdbApi.hpp"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "DBSessionImpl.h"
#include "Record.h"
#include "NdbWrappers.h"

using namespace v8;


/**** Dictionary implementation
 *
 * getTable(), listIndexes(), and listTables() should run in a uv background 
 * thread, as they may require network waits.
 *
 * Looking at NdbDictionaryImpl.cpp, any method that calls into 
 * NdbDictInterface (m_receiver) might block.
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
Envelope NdbDictionaryImplEnv("NdbDictionaryImpl");
Envelope NdbDictTableEnv("NdbDictionary::Table");
Envelope NdbDictColumnEnv("NdbDictionary::Column");
Envelope NdbDictIndexEnv("NdbDictionary::Index");

Handle<Value> getColumnType(const NdbDictionary::Column *);
Handle<Value> getIntColumnUnsigned(const NdbDictionary::Column *);


/*** DBDictionary.listTables()
  **
   **/
class ListTablesCall : public NativeCFunctionCall_2_<int, ndb_session *, const char *> 
{
private:
  NdbDictionary::Dictionary::List list;

public:
  /* Constructor */
  ListTablesCall(const Arguments &args) :
    NativeCFunctionCall_2_<int, ndb_session *, const char *>(NULL, args),
    list() 
  {
    envelope = & NdbDictionaryImplEnv;
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run() {
    NdbDictionary::Dictionary * dict = arg0->dict;
    return_val = dict->listObjects(list, NdbDictionary::Object::UserTable);
  }

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);
};


void ListTablesCall::doAsyncCallback(Local<Object> ctx) {
  DEBUG_MARKER(UDEB_DETAIL);
  Handle<Value> cb_args[2];
  
  DEBUG_PRINT("RETURN VAL: %d", return_val);
  if(return_val == -1) {
    cb_args[0] = String::New(arg0->dict->getNdbError().message);
    cb_args[1] = Null();
  }
  else {
    cb_args[0] = Null(); // no error
    /* ListObjects has returned tables in all databases; 
       we need to filter here on database name. */
    int * stack = new int[list.count];
    unsigned int nmatch = 0;
    for(unsigned i = 0; i < list.count ; i++) {
      if(strcmp(arg1, list.elements[i].database) == 0) {
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
   arg0: DBDictionaryImpl
   arg1: database name
   arg2: user_callback
*/
Handle<Value> listTables(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  // FIXME: This throws an assertion that never gets caught ...
  REQUIRE_ARGS_LENGTH(3);
  
  ListTablesCall * ncallptr = new ListTablesCall(args);

  DEBUG_PRINT("listTables in database: %s", ncallptr->arg1);
  ncallptr->runAsync();

  return scope.Close(JS_VOID_RETURN);
}


/*** DBDictionary.getTable()
  **
   **/
class GetTableCall : public NativeCFunctionCall_3_<int, ndb_session *, 
                                                   const char *, const char *> 
{
private:
  const NdbDictionary::Table * ndb_table;
  const NdbDictionary::Index ** indexes;
  int n_index;
  
  Handle<Object> buildDBIndex_PK();
  Handle<Object> buildDBIndex(const NdbDictionary::Index *);
  Handle<Object> buildDBColumn(const NdbDictionary::Column *);

public:
  /* Constructor */
  GetTableCall(const Arguments &args) : 
    NativeCFunctionCall_3_<int, ndb_session *, const char *, const char *>(NULL, args),
    ndb_table(0), indexes(0), n_index(0)
  {
    envelope = & NdbDictionaryImplEnv;
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run();

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);  
};


void GetTableCall::run() {
  DEBUG_PRINT("GetTableCall::run() [%s.%s]", arg1, arg2);
  NdbDictionary::Dictionary * dict = arg0->dict;
  NdbDictionary::Dictionary::List idx_list;
  
  // TODO: Set database name? 
  ndb_table = dict->getTable(arg2);
  if(ndb_table) {
    return_val = dict->listIndexes(idx_list, *ndb_table);
    if(return_val != -1) {
      n_index = idx_list.count;
      indexes = new const NdbDictionary::Index *[idx_list.count];
      for(unsigned int i = 0 ; i < idx_list.count ; i++) {
        indexes[i] = dict->getIndex(idx_list.elements[i].name, arg2);
      }
    }
  }
  else
    return_val = -1;
}


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
    
    // columns
    Local<Array> columns = Array::New(ndb_table->getNoOfColumns());
    for(int i = 0 ; i < ndb_table->getNoOfColumns() ; i++) {
      Handle<Object> col = buildDBColumn(ndb_table->getColumn(i));
      columns->Set(i, col);
    }
    table->Set(String::NewSymbol("columns"), columns);

    // indexes (primary key & secondary) 
    Local<Array> js_indexes = Array::New(n_index + 1);
    js_indexes->Set(0, buildDBIndex_PK());                // primary key
    for(int i = 0 ; i < n_index ; i++) {                  // secondary indexes
      js_indexes->Set(i+1, buildDBIndex(indexes[i]));
    }    
    table->Set(String::NewSymbol("indexes"), js_indexes, ReadOnly);

    // partitionKey
    
  
    // Table Record (implementation artifact; not part of spec)
    DEBUG_PRINT("Creating Table Record");
    Record * rec = new Record(arg0->dict, ndb_table->getNoOfColumns());
    for(int i = 0 ; i < ndb_table->getNoOfColumns() ; i++) {
      rec->addColumn(ndb_table->getColumn(i));      
    }
    rec->completeTableRecord(ndb_table);

    table->Set(String::NewSymbol("record"), Record_Wrapper(rec));    
    
    // User Callback
    cb_args[1] = table;
  }
  else {
    cb_args[0] = String::New(arg0->dict->getNdbError().message);
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
  Record * pk_record = new Record(arg0->dict, ncol);
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
  Record * idx_record = new Record(arg0->dict, ncol);
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

  obj->Set(String::NewSymbol("isPrimaryKey"),
           Boolean::New(col->getPrimaryKey()),
           ReadOnly);

  obj->Set(String::NewSymbol("columnSpace"),
           v8::Int32::New(col->getSizeInBytes()),
           ReadOnly);

  /* Implementation-specific properties */
  
  obj->Set(String::NewSymbol("ndbTypeId"),
           v8::Int32::New(static_cast<int>(col->getType())),
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
   arg0: DBDictionaryImpl
   arg1: database name
   arg2: table name
   arg3: user_callback
*/
Handle<Value> getTable(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(4);
  GetTableCall * ncallptr = new GetTableCall(args);
  ncallptr->runAsync();
  return JS_VOID_RETURN;
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
  };

  const char * name = typenames[col->getType()];

  if(*name == 0) {   /* One of the undefined types */
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


void DBDictionaryImpl_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  Persistent<Object> dbdict_obj = Persistent<Object>(Object::New());

  DEFINE_JS_FUNCTION(dbdict_obj, "listTables", listTables);
  DEFINE_JS_FUNCTION(dbdict_obj, "getTable", getTable);

  target->Set(Persistent<String>(String::NewSymbol("DBDictionary")), dbdict_obj);
}

