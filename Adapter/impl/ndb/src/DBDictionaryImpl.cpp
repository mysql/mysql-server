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


/**** Dictionary implementation
 *
 * getTable(), getIndexes(), and listTables() should run in a uv background 
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
  void doAsyncCallback(Local<Object> ctx) {
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
};



/* listTables() Method call
   ASYNC
   arg0: DBDictionaryImpl
   arg1: table name
   arg2: callback
   Callback will receive an int.
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


Handle<Value> getColumnType(NdbDictionary::Column * col) {
  HandleScope scope;

  /* Based on ndb_constants.h */
  const char * typenames[NDB_TYPE_MAX] = {
    "",
    "INT",            // TINY INT
    "INT",            // TINY UNSIGNED
    "INT",            // SMALL INT
    "INT",            // SMALL UNSIGNED
    "INT",            // MEDIIUM INT
    "INT",            // MEDIUM UNSIGNED
    "INT",            // INT
    "INT",            // UNSIGNED
    "INT",            // BIGINT
    "INT",            // BIG UNSIGNED
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


Handle<Value> getIntColumnSize(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;

  if(! do_test) return Null();
  
  return scope.Close(Integer::New(col->getSizeInBytes()));
}


Handle<Value> getIntColumnUnsigned(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;

  if(! do_test) return Null();
   
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


Handle<Value> getColumnNullable(NdbDictionary::Column *col) {
  HandleScope scope;
  
  return scope.Close(Boolean::New(col->getNullable()));
}


Handle<Value> getDecimalColumnScale(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();

  return scope.Close(Integer::New(col->getScale()));
}
 
 
Handle<Value> getDecimalColumnPrecision(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();
  
  return scope.Close(Integer::New(col->getPrecision()));
}                     


Handle<Value> getColumnLength(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();

  return scope.Close(Integer::New(col->getLength()));
}


Handle<Value> getColumnBinary(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();
  
  switch(col->getType()) {
    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Binary:
    case NdbDictionary::Column::Varbinary:
    case NdbDictionary::Column::Longvarbinary:
      return scope.Close(Boolean::New(true));
  
    default:
      return scope.Close(Boolean::New(false));
  }
}


Handle<Value> getColumnCharsetNumber(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();
    
  Local<Number> n = Number::New(col->getCharsetNumber());
  return scope.Close(n);
}


inline Handle<Value> getColumnNumber(NdbDictionary::Column *col) {
  HandleScope scope;

  return scope.Close(Integer::New(col->getColumnNo()));
}


inline Handle<Value> getColumnIsPrimaryKey(NdbDictionary::Column *col) {
  HandleScope scope;

  return scope.Close(Boolean::New(col->getPrimaryKey()));
}


inline Handle<Value> getColumnName(NdbDictionary::Column *col) {
  HandleScope scope;
  
  return scope.Close(String::New(col->getName()));
}


Handle<Object> buildDBColumn(NdbDictionary::Column *col) {
  HandleScope scope;
  
  Local<Object> obj = Object::New();
  NdbDictionary::Column::Type col_type = col->getType();
  bool is_int = (col_type <= NDB_TYPE_BIGUNSIGNED);
  bool is_dec = ((col_type == NDB_TYPE_DECIMAL) || (col_type == NDB_TYPE_DECIMALUNSIGNED));

  // TODO: set do_test in more places
  
  obj->Set(String::New("columnType"), 
           getColumnType(col), 
           ReadOnly);
  
  obj->Set(String::New("intSize"),
           getIntColumnSize(is_int, col),
           ReadOnly);

  obj->Set(String::New("isUnsigned"),
           getIntColumnUnsigned(is_int, col),
           ReadOnly);
  
  obj->Set(String::New("isNullable"),
           getColumnNullable(col),
           ReadOnly);
  
  obj->Set(String::New("scale"),
           getDecimalColumnScale(is_dec, col),
           ReadOnly);
  
  obj->Set(String::New("precision"),
           getDecimalColumnPrecision(is_dec, col),
           ReadOnly);
  
  obj->Set(String::New("length"),
           getColumnLength(true, col),
           ReadOnly);
  
  obj->Set(String::New("isBinary"),
           getColumnBinary(true, col),
           ReadOnly);
  
  obj->Set(String::New("charsetNumber"),
           getColumnCharsetNumber(true, col),
           ReadOnly);
  
  obj->Set(String::New("name"),
           getColumnName(col),
           ReadOnly);
  
  obj->Set(String::New("columnNumber"),
           getColumnNumber(col),
           ReadOnly);
  
  obj->Set(String::New("isPrimaryKey"),
           getColumnIsPrimaryKey(col),
           ReadOnly);

  return scope.Close(obj);
} 


void DBDictionaryImpl_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  Persistent<Object> dbdict_obj = Persistent<Object>(Object::New());

  DEFINE_JS_FUNCTION(dbdict_obj, "create", NewDBDictionaryImpl);
  DEFINE_JS_FUNCTION(dbdict_obj, "listTables", listTables);

  target->Set(Persistent<String>(String::NewSymbol("DBDictionary")), dbdict_obj);
}


