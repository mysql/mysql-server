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

#include <v8.h>

#include "Ndbapi.hpp"
#include "CharsetMap.hpp"

using namespace v8;

/****
 DBColumn = {
 columnType : "",     //  a ColumnType
 intSize : ""   ,     //  1,2,3,4, or 8
 isUnsigned: "" ,     //  TRUE for UNSIGNED
 isNullable: "" ,     //  TRUE if NULLABLE
 scale: ""      ,     //  DECIMAL scale
 precision: ""  ,     //  DECIMAL precision
 length: ""     ,     //  CHAR or VARCHAR length
 isBinary: ""   ,     //  TRUE for BLOB/BINARY/VARBINARY
 charset: ""    ,     //  name of charset
 name: ""       ,     //  column name
 columnNumber: "",     //  position of column in table
 isPrimaryKey: "",    //  TRUE if column is part of PK
 userData : ""        //  Data stored in the DBColumn by the ORM layer
 };
 */// 


/* This may return the string _UNDEFINED
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


Handle<Value> getIntColumnSigned(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;

  if(! do_test) return Null();
   
  switch(col->getType()) {
    case NdbDictionary::Column::Unsigned:
    case NdbDictionary::Column::BigUnsigned:
    case NdbDictionary::Column::SmallUnsigned:
    case NdbDictionary::Column::TinyUnsigned:
    case NdbDictionary::Column::MediumUnsigned:
      return scope.Close(Boolean::New(false));
    
    default:
      return scope.Close(Boolean::New(true));
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


Handle<Value> getColumnCharset(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  
  if(! do_test) return Null();
  
  CharsetMap csmap();
  
  Local<String> s = String::New(csmap.getMysqlName(col->getCharsetNumber()));
  return scope.Close(s);
}


Handle<Value> getColumnCharset(bool do_test, NdbDictionary::Column *col) {
  HandleScope scope;
  if(! do_test) return Null();
  return scope.Close(Integer::New(col->getColumnNo()));
}

/****
 DBColumn = {
 columnType : "",     //  a ColumnType
 intSize : ""   ,     //  1,2,3,4, or 8
 isUnsigned: "" ,     //  TRUE for UNSIGNED
 isNullable: "" ,     //  TRUE if NULLABLE
 scale: ""      ,     //  DECIMAL scale
 precision: ""  ,     //  DECIMAL precision
 length: ""     ,     //  CHAR or VARCHAR length
 isBinary: ""   ,     //  TRUE for BLOB/BINARY/VARBINARY
 charset: ""    ,     //  name of charset
 name: ""       ,     //  column name
 columnNo: ""   ,     //  position of column in table
 isPrimaryKey: "",    //  TRUE if column is part of PK
 userData : ""        //  Data stored in the DBColumn by the ORM layer
 };
*/// 

Handle<Object> buildDBColumn() {
  HandleScope scope;
  

  Local<Object> obj = Object::New();

  
  



} 