/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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

#include <string.h>
#include <uv.h>

#include <NdbApi.hpp>
#include <mysql.h>

#include "adapter_global.h"
#include "Record.h"
#include "NativeCFunctionCall.h"
#include "NdbWrappers.h"
#include "SessionImpl.h"
#include "EncoderCharset.h"
#include "js_wrapper_macros.h"
#include "JsValueAccess.h"

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
 *   The IndexMetadata objects for SECONDARY indexes wrap an NdbDictionary::Index,
 *    -- but IndexMetadta for PK does *not* wrap any native object!
 *   The ForeignKeyMetadata objects are literals and do *not* wrap any native object
*/
Envelope NdbDictTableEnv("const NdbDictionary::Table");
Envelope NdbDictColumnEnv("const NdbDictionary::Column");
Envelope NdbDictIndexEnv("const NdbDictionary::Index");

const char * getColumnType(const NdbDictionary::Column *);
bool getIntColumnUnsigned(const NdbDictionary::Column *);
Local<Value> getDefaultValue(v8::Isolate *, const NdbDictionary::Column *);

Envelope * getNdbDictTableEnvelope() {
  return & NdbDictTableEnv;
}

/** Dictionary calls run outside the main thread may end up in
    mysys error handling code, and therefore require a call to my_thread_init().
    We must assume that libuv and mysys have compatible thread abstractions.
    uv_thread_self() returns a uv_thread_t; we maintain a linked list
    containing the uv_thread_t IDs of threads that have been initialized.
*/
class ThdIdNode {
public:
  uv_thread_t id;
  ThdIdNode * next;
  ThdIdNode(uv_thread_t _id, ThdIdNode * _next) : id(_id), next(_next) {}
};

ThdIdNode * initializedThreadIds = 0;
uv_mutex_t threadListMutex;

void require_thread_specific_initialization() {
  uv_thread_t thd_id = uv_thread_self();
  bool isInitialized = false;
  ThdIdNode * list;

  uv_mutex_lock(& threadListMutex);
  {
    list = initializedThreadIds;

    while(list) {
      if(uv_thread_equal(& (list->id), & thd_id)) {
        isInitialized = true;
        break;
      }
      list = list->next;
    }

    if(! isInitialized) {
      my_thread_init();
      initializedThreadIds = new ThdIdNode(thd_id, initializedThreadIds);
    }
  }
  uv_mutex_unlock(& threadListMutex);
}

/*** DBDictionary.listTables()
  **
   **/
class ListTablesCall : public NativeCFunctionCall_2_<int, SessionImpl *, const char *> 
{
private:
  Ndb * ndb;
  NdbDictionary::Dictionary * dict;
  NdbDictionary::Dictionary::List list;

public:
  /* Constructor */
  ListTablesCall(const Arguments &args) :
    NativeCFunctionCall_2_<int, SessionImpl *, const char *>(NULL, args),
    list()
  {
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run() override {
    ndb = arg0->ndb;
    dict = ndb->getDictionary();
    return_val = dict->listObjects(list, NdbDictionary::Object::UserTable);
  }

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx) override;
};


void ListTablesCall::doAsyncCallback(Local<Object> recv) {
  DEBUG_MARKER(UDEB_DETAIL);
  Local<Value> cb_args[2];
  const char * & dbName = arg1;
  
  DEBUG_PRINT("RETURN VAL: %d", return_val);
  if(return_val == -1) {
    cb_args[0] = NdbError_Wrapper(dict->getNdbError());
    cb_args[1] = Null(isolate);
  }
  else {
    cb_args[0] = Null(isolate); // no error
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

    Local<Array> cb_list = Array::New(isolate, nmatch);
    for(unsigned int i = 0; i < nmatch ; i++) {
      SetProp(isolate, cb_list, i, list.elements[stack[i]].name);
    }
    cb_args[1] = cb_list;
    delete[] stack;
  }

  AsyncCall_Returning<int>::doAsyncCallback(recv, 2, cb_args);
}


/* listTables() Method call
   ASYNC
   arg0: SessionImpl *
   arg1: database name
   arg2: user_callback
*/
void listTables(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);  
  REQUIRE_ARGS_LENGTH(3);
  
  ListTablesCall * ncallptr = new ListTablesCall(args);

  DEBUG_PRINT("listTables in database: %s", ncallptr->arg1);
  ncallptr->runAsync();

  args.GetReturnValue().SetUndefined();
}


class DictionaryNameSplitter {
public:
  DictionaryNameSplitter()                      {}
  char part1[65];
  char part3[65];
  void splitName(const char * src);
  bool match(const char *db, const char *table);
};

/* match() returns true if parts 1 and 3 of split name match db.table 
*/
inline bool DictionaryNameSplitter::match(const char *db, const char *table) {
  return ((strncmp(db, part1, 65) == 0) && (strncmp(table, part3, 65) == 0));
}

/* Convert a name of the form <database>/<schema>/<table> to database and table 
   which are allocated by the caller and must each be able to hold 65 characters
*/
void DictionaryNameSplitter::splitName(const char * src) {
  char * dstp = part1;
  int max_len = 64; // maximum database name
  // copy first part of name to db
  while (*src != '/' && *src != 0 && max_len-- > 0) {
    *dstp++ = *src++;
  }
  src++;
  *dstp = 0;
  // skip second part of name /<schema>/
  max_len = 65; // maximum  schema name plus trailing /
  while (*src != '/' && max_len-- > 0) {
    ++src;
  }
  ++src;
  // copy third part of name to tbl
  max_len = 64; // maximum table name
  dstp = part3;
  while (*src != 0 && max_len-- > 0) {
    *dstp++ = *src++;
  }
  *dstp = 0;
  DEBUG_PRINT("splitName for %s => %s %s", src, part1, part3);
}


/*** DBDictionary.getTable()
  **
   **/
class GetTableCall : public NativeCFunctionCall_3_<int, SessionImpl *, 
                                                   const char *, const char *> 
{
private:
  const NdbDictionary::Table * ndb_table;
  Ndb * per_table_ndb;
  Ndb * ndb;                /* ndb from DBSesssionImpl */
  const char * dbName;      /* this is NativeCFunctionCall_3_  arg1 */
  const char * tableName;   /* this is NativeCFunctionCall_3_  arg2 */
  NdbDictionary::Dictionary * dict;
  NdbDictionary::Dictionary::List idx_list;
  NdbDictionary::Dictionary::List fk_list;
  const NdbError * ndbError;
  int fk_count;
  DictionaryNameSplitter splitter;

  Local<Object> buildDBIndex_PK();
  Local<Object> buildDBIndex(const NdbDictionary::Index *);
  Local<Object> buildDBForeignKey(const NdbDictionary::ForeignKey *);
  Local<Object> buildDBColumn(const NdbDictionary::Column *);
  bool splitNameMatchesDbAndTable(const char * name);

public:
  /* Constructor */
  GetTableCall(const Arguments &args) : 
    NativeCFunctionCall_3_<int, SessionImpl *, const char *, const char *>(NULL, args),
    ndb_table(0), per_table_ndb(0), idx_list(), fk_list(), fk_count(0)
  {
    ndb = arg0->ndb; 
    dbName = arg1;
    tableName = arg2;
  }
  
  /* UV_WORKER_THREAD part of listTables */
  void run() override;

  /* V8 main thread */
  void doAsyncCallback(Local<Object> ctx) override;
};


inline bool GetTableCall::splitNameMatchesDbAndTable(const char * name) {
  splitter.splitName(name);
  return splitter.match(dbName, tableName);
}

void GetTableCall::run() {
  DEBUG_PRINT("GetTableCall::run() [%s.%s]", arg1, arg2);
  require_thread_specific_initialization();
  return_val = -1;

  /* dbName is optional; if not present, set it from ndb database name */
  if(strlen(dbName)) {
    ndb->setDatabaseName(dbName);
  } else {
    dbName = ndb->getDatabaseName();
  }
  dict = ndb->getDictionary();
  ndb_table = dict->getTable(tableName);
  if(ndb_table) {
    /* Ndb object used to create NdbRecords and to cache auto-increment values */
    per_table_ndb = new Ndb(& ndb->get_ndb_cluster_connection());
    DEBUG_PRINT("per_table_ndb %s.%s %p\n", dbName, tableName, per_table_ndb);
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
    DEBUG_PRINT("listIndexes() returned %i", return_val);
    ndbError = & dict->getNdbError();
    return;
  }
  /* List the foreign keys and keep the list around for doAsyncCallback to create js objects
   * Currently there is no listForeignKeys so we use the more generic listDependentObjects
   * specifying the table metadata object.
   */
  return_val = dict->listDependentObjects(fk_list, *ndb_table);
  if (return_val == 0) {
    /* Fetch the foreign keys and associated parent tables now.
     * These calls may perform network IO, populating
     * the (connection) global and (Ndb) local dictionary caches.  Later,
     * in the JavaScript main thread, we will call getForeignKey() again knowing
     * that the caches are populated.
     * We only care about foreign keys where this table is the child table, not the parent table.
     */
    for(unsigned int i = 0 ; i < fk_list.count ; i++) {
      NdbDictionary::ForeignKey fk;
      if (fk_list.elements[i].type == NdbDictionary::Object::ForeignKey) {
        const char * fk_name = fk_list.elements[i].name;
        int fkGetCode = dict->getForeignKey(fk, fk_name);
        DEBUG_PRINT("getForeignKey for %s returned %i", fk_name, fkGetCode);
        // see if the foreign key child table is this table
        if(splitNameMatchesDbAndTable(fk.getChildTable())) {
          // the foreign key child table is this table; get the parent table
          ++fk_count;
          DEBUG_PRINT("Getting ParentTable");
          splitter.splitName(fk.getParentTable());
          ndb->setDatabaseName(splitter.part1);  // temp for next call
          const NdbDictionary::Table * parent_table = dict->getTable(splitter.part3);
          ndb->setDatabaseName(dbName);  // back to expected value
          DEBUG_PRINT("Parent table getTable returned %s", parent_table->getName());
        }
      }
    }
  }
  else {
    DEBUG_PRINT("listDependentObjects() returned %i", return_val);
    ndbError = & dict->getNdbError();
  }
}


/* doAsyncCallback() runs in the main thread.  We don't want it to block.
   TODO: verify whether any IO is done 
         by checking WaitMetaRequestCount at the start and end.
*/    
void GetTableCall::doAsyncCallback(Local<Object> recv) {
  const char *ndbTableName;
  EscapableHandleScope scope(isolate);
  DEBUG_PRINT("GetTableCall::doAsyncCallback: return_val %d", return_val);

  /* User callback arguments */
  Local<Value> cb_args[2];
  cb_args[0] = Null(isolate);
  cb_args[1] = Null(isolate);
  
  /* TableMetadata = {
      database         : ""    ,  // Database name
      name             : ""    ,  // Table Name
      columns          : []    ,  // ordered array of DBColumn objects
      indexes          : []    ,  // array of DBIndex objects 
      partitionKey     : []    ,  // ordered array of column numbers in the partition key
      sparseContainer  : null     // default column for sparse fields
    };
  */    
  if(ndb_table && ! return_val) {
    Local<Object> table = ToObject(isolate, NdbDictTableEnv.wrap(ndb_table));

    // database
    SetProp(isolate, table, "database", arg1);
    
    // name
    ndbTableName = ndb_table->getName();
    SetProp(isolate, table, "name", ndbTableName);

    // partitionKey
    int nPartitionKeys = 0;
    Local<Array> partitionKeys = Array::New(isolate);
    SetProp(isolate, table, "partitionKey", partitionKeys);

    // sparseContainer
    SetProp(isolate, table, "sparseContainer", Null(isolate));

    // columns
    Local<Array> columns = Array::New(isolate, ndb_table->getNoOfColumns());
    for(int i = 0 ; i < ndb_table->getNoOfColumns() ; i++) {
      const NdbDictionary::Column *ndb_col = ndb_table->getColumn(i);
      Local<Object> col = buildDBColumn(ndb_col);
      SetProp(isolate, columns, i, col);
      if(ndb_col->getPartitionKey()) { /* partition key */
        SetProp(isolate, partitionKeys, nPartitionKeys++, ndb_col->getName());
      }
      if(     ! strcmp(ndb_col->getName(), "SPARSE_FIELDS")
          && ( (! strncmp(getColumnType(ndb_col), "VARCHAR", 7)
                  && (getEncoderCharsetForColumn(ndb_col)->isUnicode))
              || (   ! strncmp(getColumnType(ndb_col), "VARBINARY", 9)
                  || ! strncmp(getColumnType(ndb_col), "JSON", 4))))
      {
        SetProp(isolate, table, "sparseContainer", ndb_col->getName());
      }
    }
    SetProp(isolate, table, "columns", columns);

    // indexes (primary key & secondary) 
    Local<Array> js_indexes = Array::New(isolate, idx_list.count + 1);
    SetProp(isolate, js_indexes, 0, buildDBIndex_PK());    // primary key
    for(unsigned int i = 0 ; i < idx_list.count ; i++) {   // secondary indexes
      const NdbDictionary::Index * idx =
        dict->getIndex(idx_list.elements[i].name, arg2);
      SetProp(isolate, js_indexes, i+1, buildDBIndex(idx));
    }    
    SET_RO_PROPERTY(table, SYMBOL(isolate, "indexes"), js_indexes);

    // foreign keys (only foreign keys for which this table is the child)
    // now create the javascript foreign key metadata objects for dictionary objects cached earlier
    Local<Array> js_fks = Array::New(isolate, fk_count);

    int fk_number = 0;
    for(unsigned int i = 0 ; i < fk_list.count ; i++) {
      NdbDictionary::ForeignKey fk;
      if (fk_list.elements[i].type == NdbDictionary::Object::ForeignKey) {
        const char * fk_name = fk_list.elements[i].name;
        int fkGetCode = dict->getForeignKey(fk, fk_name);
        DEBUG_PRINT("getForeignKey for %s returned %i", fk_name, fkGetCode);
        // see if the foreign key child table is this table
        if(splitNameMatchesDbAndTable(fk.getChildTable())) {
          // the foreign key child table is this table; build the fk object
          DEBUG_PRINT("Adding foreign key for %s at %i", fk.getName(), fk_number);
          SetProp(isolate, js_fks, fk_number++, buildDBForeignKey(&fk));
        }
      }
    }
    SET_RO_PROPERTY(table, SYMBOL(isolate, "foreignKeys"), js_fks);

    // Autoincrement Cache Impl (also not part of spec)
    if(per_table_ndb) {
      SetProp(isolate, table, "per_table_ndb", Ndb_Wrapper(per_table_ndb));
    }
    
    // User Callback
    cb_args[1] = table;
  }
  else {
    cb_args[0] = NdbError_Wrapper(* ndbError);
  }

  AsyncCall_Returning<int>::doAsyncCallback(recv, 2, cb_args);
}


/* 
DBIndex = {
  name             : ""    ,  // Index name
  isPrimaryKey     : true  ,  // true for PK; otherwise undefined
  isUnique         : true  ,  // true or false
  isOrdered        : true  ,  // true or false; can scan if true
  columnNumbers    : []    ,  // an ordered array of column numbers
};
*/
Local<Object> GetTableCall::buildDBIndex_PK() {
  EscapableHandleScope scope(isolate);
  
  Local<Object> obj = Object::New(isolate);

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "name"), NewUtf8String(isolate, "PRIMARY_KEY"));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isPrimaryKey"), Boolean::New(isolate, true));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isUnique"),     Boolean::New(isolate, true));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isOrdered"),    Boolean::New(isolate, false));

  /* Loop over the columns of the key. 
     Build the "columnNumbers" array and the "record" object, then set both.
  */  
  int ncol = ndb_table->getNoOfPrimaryKeys();
  DEBUG_PRINT("Creating Primary Key Record");
  Record * pk_record = new Record(dict, ncol);
  Local<Array> idx_columns = Array::New(isolate, ncol);
  for(int i = 0 ; i < ncol ; i++) {
    const char * col_name = ndb_table->getPrimaryKey(i);
    const NdbDictionary::Column * col = ndb_table->getColumn(col_name);
    pk_record->addColumn(col);
    SetProp(isolate, idx_columns, i, v8::Int32::New(isolate, col->getColumnNo()));
  }
  pk_record->completeTableRecord(ndb_table);

  SetProp(isolate, obj, "columnNumbers", idx_columns);
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "record"), Record_Wrapper(pk_record));

  return scope.Escape(obj);
}


Local<Object> GetTableCall::buildDBIndex(const NdbDictionary::Index *idx) {
  EscapableHandleScope scope(isolate);

  Local<Object> obj = NdbDictIndexEnv.newWrapper();
  wrapPointerInObject(idx, NdbDictIndexEnv, obj);

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "name"), NewUtf8String(isolate, idx->getName()));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isPrimaryKey"), Boolean::New(isolate, false));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isUnique"),
                Boolean::New(isolate, idx->getType() == NdbDictionary::Index::UniqueHashIndex));
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isOrdered"),
                Boolean::New(isolate, idx->getType() == NdbDictionary::Index::OrderedIndex));
  
  /* Loop over the columns of the key. 
     Build the "columns" array and the "record" object, then set both.
  */  
  int ncol = idx->getNoOfColumns();
  Local<Array> idx_columns = Array::New(isolate, ncol);
  DEBUG_PRINT("Creating Index Record (%s)", idx->getName());
  Record * idx_record = new Record(dict, ncol);
  for(int i = 0 ; i < ncol ; i++) {
    const char *colName = idx->getColumn(i)->getName();
    const NdbDictionary::Column *col = ndb_table->getColumn(colName);
    SetProp(isolate, idx_columns, i, col->getColumnNo());
    idx_record->addColumn(col);
  }
  idx_record->completeIndexRecord(idx);
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "record"), Record_Wrapper(idx_record));
  SetProp(isolate, obj, "columnNumbers", idx_columns);
  
  return scope.Escape(obj);
}

/*
 * ForeignKeyMetadata = {
  name             : ""    ,  // Constraint name
  columnNames      : null  ,  // an ordered array of column numbers
  targetTable      : ""    ,  // referenced table name
  targetDatabase   : ""    ,  // referenced database name
  targetColumnNames: null  ,  // an ordered array of target column names
};
*/
Local<Object> GetTableCall::buildDBForeignKey(const NdbDictionary::ForeignKey *fk) {
  EscapableHandleScope scope(isolate);
  DictionaryNameSplitter localSplitter;
  Local<Object> js_fk = Object::New(isolate);

  localSplitter.splitName(fk->getName());  // e.g. "12/20/fkname"
  SetProp(isolate, js_fk, "name", localSplitter.part3);

  // get child column names
  unsigned int childColumnCount = fk->getChildColumnCount();
  Local<Array> fk_child_column_names = Array::New(isolate, childColumnCount);
  for (unsigned i = 0; i < childColumnCount; ++i) {
    int columnNumber = fk->getChildColumnNo(i);
    const NdbDictionary::Column * column = ndb_table->getColumn(columnNumber);
    SetProp(isolate, fk_child_column_names, i, column->getName());
  }
  SetProp(isolate, js_fk, "columnNames", fk_child_column_names);

  // get parent table (which might be in a different database)
  const char * fk_parent_name = fk->getParentTable();
  localSplitter.splitName(fk_parent_name);
  const char * parent_db_name = localSplitter.part1;
  const char * parent_table_name = localSplitter.part3;
  SetProp(isolate, js_fk, "targetTable", parent_table_name);
  SetProp(isolate, js_fk, "targetDatabase", parent_db_name);
  ndb->setDatabaseName(parent_db_name);
  const NdbDictionary::Table * parent_table = dict->getTable(parent_table_name);
  ndb->setDatabaseName(dbName);

  // get parent column names
  unsigned int parentColumnCount = fk->getParentColumnCount();
  Local<Array> fk_parent_column_names = Array::New(isolate, parentColumnCount);
  for (unsigned i = 0; i < parentColumnCount; ++i) {
    int columnNumber = fk->getParentColumnNo(i);
    const NdbDictionary::Column * column = parent_table->getColumn(columnNumber);
    SetProp(isolate, fk_parent_column_names, i, column->getName());
  }
  SetProp(isolate, js_fk, "targetColumnNames", fk_parent_column_names);

  return scope.Escape(js_fk);
}

Local<Object> GetTableCall::buildDBColumn(const NdbDictionary::Column *col) {
  EscapableHandleScope scope(isolate);
  
  Local<Object> obj = ToObject(isolate, NdbDictColumnEnv.wrap(col));
  
  NdbDictionary::Column::Type col_type = col->getType();
  bool is_int = (col_type <= NDB_TYPE_BIGUNSIGNED);
  bool is_dec = ((col_type == NDB_TYPE_DECIMAL) || (col_type == NDB_TYPE_DECIMALUNSIGNED));
  bool is_binary = ((col_type == NDB_TYPE_BLOB) || (col_type == NDB_TYPE_BINARY) 
                   || (col_type == NDB_TYPE_VARBINARY) || (col_type == NDB_TYPE_LONGVARBINARY));
  bool is_char = ((col_type == NDB_TYPE_CHAR) || (col_type == NDB_TYPE_TEXT)
                   || (col_type == NDB_TYPE_VARCHAR) || (col_type == NDB_TYPE_LONGVARCHAR));
  bool is_lob =  ((col_type == NDB_TYPE_BLOB) || (col_type == NDB_TYPE_TEXT));

  /* Required Properties */

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "name"),
           NewUtf8String(isolate, col->getName()));
  
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "columnNumber"),
           v8::Int32::New(isolate, col->getColumnNo()));
  
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "columnType"),
           NewUtf8String(isolate, getColumnType(col)));

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isIntegral"),
           Boolean::New(isolate, is_int));

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isNullable"),
           Boolean::New(isolate, col->getNullable()));

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isInPrimaryKey"),
           Boolean::New(isolate, col->getPrimaryKey()));

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "columnSpace"),
           v8::Int32::New(isolate, col->getSizeInBytes()));

  /* Implementation-specific properties */
  
  SET_RO_PROPERTY(obj, SYMBOL(isolate, "ndbTypeId"),
           v8::Int32::New(isolate, static_cast<int>(col->getType())));

  SET_PROPERTY(obj, SYMBOL(isolate, "ndbRawDefaultValue"),
           getDefaultValue(isolate, col), v8::ReadOnly);

  if(is_lob) {
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "ndbInlineSize"),
            v8::Int32::New(isolate, col->getInlineSize()));

    SET_RO_PROPERTY(obj, SYMBOL(isolate, "ndbPartSize"),
            v8::Int32::New(isolate, col->getPartSize()));
  }


  /* Optional Properties, depending on columnType */
  /* Group A: Numeric */
  if(is_int || is_dec) {
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "isUnsigned"),
             Boolean::New(isolate, getIntColumnUnsigned(col)));
  }
  
  if(is_int) {    
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "intSize"),
             v8::Int32::New(isolate, col->getSizeInBytes()));
  }
  
  if(is_dec) {
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "scale"),
             v8::Int32::New(isolate, col->getScale()));
    
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "precision"),
             v8::Int32::New(isolate, col->getPrecision()));
  }

  SET_RO_PROPERTY(obj, SYMBOL(isolate, "isAutoincrement"),
           Boolean::New(isolate, col->getAutoIncrement()));
   
  /* Group B: Non-numeric */
  if(is_binary || is_char) {
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "isBinary"),
             Boolean::New(isolate, is_binary));
  
    SET_RO_PROPERTY(obj, SYMBOL(isolate, "isLob"),
             Boolean::New(isolate, is_lob));

    if(is_binary) {
      SET_RO_PROPERTY(obj, SYMBOL(isolate, "length"),
               v8::Int32::New(isolate, col->getLength()));
    }

    if(is_char) {
      const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);

      SET_RO_PROPERTY(obj, SYMBOL(isolate, "length"),
               v8::Int32::New(isolate, col->getLength() / csinfo->maxlen));

      SET_RO_PROPERTY(obj, SYMBOL(isolate, "charsetName"),
               NewUtf8String(isolate, csinfo->name));

      SET_RO_PROPERTY(obj, SYMBOL(isolate, "collationName"),
               NewUtf8String(isolate, csinfo->collationName));
    }
  }
    
  return scope.Escape(obj);
} 


/* getTable() method call
   ASYNC
   arg0: Ndb *
   arg1: database name
   arg2: table name
   arg3: user_callback
*/
void getTable(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(4);
  GetTableCall * ncallptr = new GetTableCall(args);
  ncallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}


const char * getColumnType(const NdbDictionary::Column * col) {

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
    "",               // 13 OLDDECIMAL
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
    ,
    "TIME",          // 31 TIME2
    "DATETIME",      // 32 DATETIME2
    "TIMESTAMP",     // 33 TIMESTAMP2
#endif
  };

  if(    (col->getType() == 20) && (col->getInlineSize() == 4000)
      && (col->getPartSize() == 8100))
  {
    return "JSON";
  }

  return typenames[col->getType()];
}


bool getIntColumnUnsigned(const NdbDictionary::Column *col) {
  switch(col->getType()) {
    case NdbDictionary::Column::Unsigned:
    case NdbDictionary::Column::Bigunsigned:
    case NdbDictionary::Column::Smallunsigned:
    case NdbDictionary::Column::Tinyunsigned:
    case NdbDictionary::Column::Mediumunsigned:
      return true;

    default:
      return false;
  }
}

Local<Value> getDefaultValue(v8::Isolate *isolate, const NdbDictionary::Column *col) {
  Local<Value> v = Null(isolate);
  unsigned int defaultLen = 0;
  
  const void* dictDefaultBuff = col->getDefaultValue(& defaultLen);
  if(defaultLen) {
    v = CopyToJsBuffer(isolate, (const char *) dictDefaultBuff, defaultLen);
  }
  return v;
}


/* arg0: TableMetadata wrapping NdbDictionary::Table *
   arg1: Ndb *
   arg2: number of columns
   arg3: array of NdbDictionary::Column *
*/
void getRecordForMapping(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  EscapableHandleScope scope(args.GetIsolate());
  const NdbDictionary::Table *table = 
    unwrapPointer<const NdbDictionary::Table *>(ToObject(args, args[0]));
  Ndb * ndb = unwrapPointer<Ndb *>(ToObject(args, args[1]));
  unsigned int nColumns =  GetInt32Arg(args, 2);
  Record * record = new Record(ndb->getDictionary(), nColumns);
  Local<Object> colArray = ToObject(args, args[3]);
  for(unsigned int i = 0 ; i < nColumns ; i++) {
    const NdbDictionary::Column * col = 
      unwrapPointer<const NdbDictionary::Column *>(ElementToObject(colArray, i));
    record->addColumn(col);
  }
  record->completeTableRecord(table);

  args.GetReturnValue().Set(scope.Escape(Record_Wrapper(record)));
}


void DBDictionaryImpl_initOnLoad(Local<Object> target) {
  Local<Object> dbdict_obj = Object::New(Isolate::GetCurrent());

  DEFINE_JS_FUNCTION(dbdict_obj, "listTables", listTables);
  DEFINE_JS_FUNCTION(dbdict_obj, "getTable", getTable);
  DEFINE_JS_FUNCTION(dbdict_obj, "getRecordForMapping", getRecordForMapping);

  SetProp(Isolate::GetCurrent(), target, "DBDictionary", dbdict_obj);

  uv_mutex_init(& threadListMutex);
}

