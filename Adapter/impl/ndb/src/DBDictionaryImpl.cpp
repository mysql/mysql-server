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

#include "Ndbapi.hpp"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "DBSessionImpl.h"

using namespace v8;

Envelope NdbDictionaryImplEnv("NdbDictionaryImpl");

Handle<Object> buildDBColumn(const NdbDictionary::Column *);
Handle<Object> buildDBIndex_PK(const NdbDictionary::Table *);
Handle<Object> buildDBIndex(const NdbDictionary::Index *);


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
*/

struct DBDictImpl {
  ndb_session *sess;
};

/*** NewDBDictionaryImpl implements DBDictionary.create()
  ** Called from DBSession.getDataDictionary() 
   **/
Handle<Value> NewDBDictionaryImpl(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  Local<Object> dict_obj = NdbDictionaryImplEnv.newWrapper();
 
  JsValueConverter<ndb_session *> arg0(args[0]);
  ndb_session * my_sess = arg0.toC();
  
  DBDictImpl *self = new DBDictImpl;
  self->sess = arg0.toC();

  wrapPointerInObject(self, NdbDictionaryImplEnv, dict_obj);
  return dict_obj;
}


/*** DBDictionary.listTables()
  **
   **/
class ListTablesCall : public NativeCFunctionCall_2_<int, DBDictImpl *, const char *> 
{
private:
  NdbDictionary::Dictionary::List list;

public:
  /* Constructor */
  ListTablesCall(const Arguments &args) : 
    NativeCFunctionCall_2_<int, DBDictImpl *, const char *>(args), 
    list() {  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run() {
    DEBUG_MARKER(UDEB_DEBUG);
    NdbDictionary::Dictionary * dict = arg0->sess->dict;
    return_val = dict->listObjects(list, NdbDictionary::Object::UserTable);
  }

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);
};


void ListTablesCall::doAsyncCallback(Local<Object> ctx) {
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> cb_args[2];
  
  DEBUG_PRINT("RETURN VAL: %d", return_val);
  if(return_val == -1) {
    cb_args[0] = String::New(arg0->sess->dict->getNdbError().message);
    cb_args[1] = Null();
  }
  else {
    cb_args[0] = Null(); // no error
    /* ListObjects has returned tables in all databases; 
       we need to filter here on database name. */
    int stack[list.count];
    int nmatch = 0;
    for(unsigned i = 0; i < list.count ; i++) {
      if(strcmp(arg1, list.elements[i].database) == 0) {
        stack[nmatch++] = i;
      }
    }
    DEBUG_PRINT("arg1/nmatch/list.count: %s/%d/%d", arg1,nmatch,list.count);

    Local<Array> cb_list = Array::New(nmatch);
    for(unsigned i = 0; i < nmatch ; i++) {
      cb_list->Set(i, String::New(list.elements[stack[i]].name));
    }
    cb_args[1] = cb_list;
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
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  // FIXME: This throws an assertion that never gets caught ...
  REQUIRE_ARGS_LENGTH(3);
  
  ListTablesCall * ncallptr = new ListTablesCall(args);  

  DEBUG_PRINT("arg1: %s", ncallptr->arg1);
  ncallptr->runAsync();

  return scope.Close(JS_VOID_RETURN);
}


/*** DBDictionary.getTable()
  **
   **/
class GetTableCall : public NativeCFunctionCall_3_<int, DBDictImpl *, 
                                                   const char *, const char *> 
{
private:
  const NdbDictionary::Table * ndb_table;
  const NdbDictionary::Index ** indexes;
  int n_index;

public:
  /* Constructor */
  GetTableCall(const Arguments &args) : 
    NativeCFunctionCall_3_<int, DBDictImpl *, const char *, const char *>(args), 
    ndb_table(0), indexes(0), n_index(0) {  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run();

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx);  
};

void GetTableCall::run() {
  DEBUG_MARKER(UDEB_DEBUG);
  NdbDictionary::Dictionary * dict = arg0->sess->dict;
  NdbDictionary::Dictionary::List idx_list;
  
  // TODO: Set database name? 
  ndb_table = dict->getTable(arg2);
  if(ndb_table) {
    return_val = dict->listIndexes(idx_list, *ndb_table);
    if(return_val != -1) {
      n_index = idx_list.count;
      indexes = new const NdbDictionary::Index *[idx_list.count];
      for(int i = 0 ; i < idx_list.count ; i++) {
        indexes[i] = dict->getIndex(idx_list.elements[i].name, arg2);
      }
    }
  }
  else
    return_val = -1;
}


void GetTableCall::doAsyncCallback(Local<Object> ctx) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;  

  /* User callback arguments */
  Handle<Value> cb_args[2];
  cb_args[0] = Null();
  cb_args[1] = Null();
  
  /* DBTable = {
      database      : ""    ,  // Database name
      name          : ""    ,  // Table Name
      columns       : []    ,  // an array of DBColumn objects
      primaryKey    : {}    ,  // a DBIndex object
      secondaryIndexes: []  ,  // an array of DBIndex objects 
      userData      : ""       // Data stored in the DBTable by the ORM layer
    }
  */    
  if(ndb_table && ! return_val) {

    Local<Object> table = Object::New();

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

    // primaryKey
    table->Set(String::NewSymbol("primaryKey"), 
               buildDBIndex_PK(ndb_table),
               ReadOnly);

    // secondaryIndexes
    Local<Array> secondaryIndexes = Array::New(n_index);
    for(int i = 0 ; i < n_index ; i++) {
      secondaryIndexes->Set(i, buildDBIndex(indexes[i]));
    }    
    table->Set(String::NewSymbol("secondaryIndexes"), secondaryIndexes, ReadOnly);
    
    // User Callback
    cb_args[1] = table;
  }
  else {
    cb_args[0] = String::New(arg0->sess->dict->getNdbError().message);
  }
  
  callback->Call(ctx, 2, cb_args);
}


/* getTable() method call
   ASYNC
   arg0: DBDictionaryImpl
   arg1: database name
   arg2: table name
   arg3: user_callback
*/
Handle<Value> getTable(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(4);
  GetTableCall * ncallptr = new GetTableCall(args);
  ncallptr->runAsync();
  return JS_VOID_RETURN;
}


Handle<Value> getColumnType(const NdbDictionary::Column * col) {
  HandleScope scope;

  /* Based on ndb_constants.h */
  const char * typenames[NDB_TYPE_MAX] = {
    "",
    "TINYINT",        // TINY INT
    "TINYINT",        // TINY UNSIGNED
    "SMALLINT",       // SMALL INT
    "SMALLINT",       // SMALL UNSIGNED
    "MEDIUMINT",      // MEDIIUM INT
    "MEDIUMINT",      // MEDIUM UNSIGNED
    "INT",            // INT
    "INT",            // UNSIGNED
    "BIGINT",         // BIGINT
    "BIGINT",         // BIG UNSIGNED
    "FLOAT",
    "DOUBLE",    
    "",               // OLDDECIMAL
    "CHAR",
    "VARCHAR",
    "BINARY",
    "VARBINARY",
    "DATETIME",
    "DATE",
    "BLOB",
    "BLOB",           // TEXT
    "BIT",
    "VARCHAR",        // LONGVARCHAR
    "VARBINARY",      // LONGVARBINARY
    "TIME",
    "YEAR",
    "TIMESTAMP",
    "",               // OLDDECIMAL UNSIGNED
    "DECIMAL",        // DECIMAL
    "DECIMAL"         // DECIMAL UNSIGNED 
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

 
Handle<Object> buildDBColumn(const NdbDictionary::Column *col) {
  HandleScope scope;
  
  Local<Object> obj = Object::New();
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
           Integer::New(col->getColumnNo()),
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

   // todo: columnspace
  
  /* Optional Properties, depending on columnType */

  /* Group A: Numeric */
  if(is_int || is_dec) {
    obj->Set(String::NewSymbol("isUnsigned"),
             getIntColumnUnsigned(col),
             ReadOnly);
  }
  
  if(is_int) {    
    obj->Set(String::NewSymbol("intSize"),
             Integer::New(col->getSizeInBytes()),
             ReadOnly);
  }
  
  if(is_dec) {
    obj->Set(String::NewSymbol("scale"),
             Integer::New(col->getScale()),
             ReadOnly);
    
    obj->Set(String::NewSymbol("precision"),
             Integer::New(col->getPrecision()),
             ReadOnly);
  }
  
  /* Group B: Non-numeric */
  if(is_binary || is_char) {  
    obj->Set(String::NewSymbol("length"),
             Integer::New(col->getLength()),
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


/* DBIndex = {
  name          : ""    ,  // External or Internal index name (see clusterj-core store/Index.java)
  isPrimaryKey  : false ,  // bool
  isUnique      : false ,  // bool
  isOrdered     : false ,  // bool; can scan if true
  columns       : []    ,  // an array of DBColumn objects
  userData      : ""       // Data stored in the DBIndex by the ORM layer
};
*/
Handle<Object> buildDBIndex_PK(const NdbDictionary::Table *ndb_table) {
  HandleScope scope;
  
  Local<Object> obj = Object::New();

  obj->Set(String::NewSymbol("name"), Null(), ReadOnly);
  obj->Set(String::NewSymbol("isPrimaryKey"), Boolean::New(true), ReadOnly);
  obj->Set(String::NewSymbol("isUnique"),     Boolean::New(true), ReadOnly);
  obj->Set(String::NewSymbol("isOrdered"),    Boolean::New(false), ReadOnly);
  
  int ncol = ndb_table->getNoOfPrimaryKeys();
  Local<Array> idx_columns = Array::New(ncol);
  obj->Set(String::NewSymbol("columns"), idx_columns);
  for(int i = 0 ; i < ncol ; i++) {
    const char * col_name = ndb_table->getPrimaryKey(i);
    idx_columns->Set(i, String::New(col_name));
  }
  return scope.Close(obj);
}


Handle<Object> buildDBIndex(const NdbDictionary::Index *idx) {
  HandleScope scope;
  
  Local<Object> obj = Object::New();

  obj->Set(String::NewSymbol("name"), String::New(idx->getName()));
  obj->Set(String::NewSymbol("isPrimaryKey"), Boolean::New(false), ReadOnly);
  obj->Set(String::NewSymbol("isUnique"),     
           Boolean::New(idx->getType() == NdbDictionary::Index::UniqueHashIndex),
           ReadOnly);
  obj->Set(String::NewSymbol("isOrdered"),
           Boolean::New(idx->getType() == NdbDictionary::Index::OrderedIndex),
           ReadOnly);
  
  int ncol = idx->getNoOfColumns();
  Local<Array> idx_columns = Array::New(ncol);
  obj->Set(String::NewSymbol("columns"), idx_columns);
  for(int i = 0 ; i < ncol ; i++) {
    idx_columns->Set(i, Integer::New(idx->getColumn(i)->getColumnNo()));
  }
  return scope.Close(obj);
}


void DBDictionaryImpl_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  Persistent<Object> dbdict_obj = Persistent<Object>(Object::New());

  DEFINE_JS_FUNCTION(dbdict_obj, "create", NewDBDictionaryImpl);
  DEFINE_JS_FUNCTION(dbdict_obj, "listTables", listTables);
  DEFINE_JS_FUNCTION(dbdict_obj, "getTable", getTable);

  target->Set(Persistent<String>(String::NewSymbol("DBDictionary")), dbdict_obj);
}


