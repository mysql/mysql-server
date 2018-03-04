/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"
#include <NdbOut.hpp>
#include <SimpleProperties.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <AttributeHeader.hpp>
#include <zlib.h>                    //compress, uncompress
#include <NdbEnv.h>
#include <util/version.h>
#include <NdbSleep.h>
#include <signaldata/IndexStatSignal.hpp>

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/CreateEvnt.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/DropTable.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/ListTables.hpp>
#include <signaldata/DropFilegroup.hpp>
#include <signaldata/CreateFilegroup.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/SchemaTrans.hpp>
#include <signaldata/CreateHashMap.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/CreateFK.hpp>
#include <signaldata/DropFK.hpp>

#define DEBUG_PRINT 0
#define INCOMPATIBLE_VERSION -2


/**
 * Signal response timeouts
 *
 * We define long and short signal response timeouts for use with Dict
 * signals.  These define how long NdbApi will wait for a response to
 * a request to the kernel before considering the request failed.
 *
 * If a response to an individual request takes longer than its timeout
 * time then it is considered a software bug.
 *
 * Most Dict request/response signalling is implemented inside a retry
 * loop which will retry the request up to (say) 100 times for cases
 * where a response is received which indicates a temporary or otherwise
 * acceptable error.  Each retry will reset the response timeout duration
 * for the next request.
 *
 * The short timeout is used for requests which should be processed more
 * or less instantaneously, with only communication and limited computation
 * or delays involved.
 *
 * This includes requests for in-memory information, waits for the next
 * epoch/GCP, start of schema transactions, parse stage of schema transaction
 * operations etc..
 *
 * The long timeout is used for requests which can involve a significant
 * amount of work in the data nodes before a CONF response can be
 * expected.  This can include things like the prepare, commit + complete
 * phases of schema object creation, index build, online re-org etc.
 * With schema transactions these phases all occur as part of the processing
 * of GSN_SCHEMA_TRANS_END_REQ.
 *
 * The long timeout remains at 7 days for now.
 */
#define DICT_SHORT_WAITFOR_TIMEOUT (120*1000)
#define DICT_LONG_WAITFOR_TIMEOUT (7*24*60*60*1000)

#define ERR_RETURN(a,b) \
{\
   DBUG_PRINT("exit", ("error %d  return %d", (a).code, b));\
   DBUG_RETURN(b);\
}

int ndb_dictionary_is_mysqld = 0;

bool
is_ndb_blob_table(const char* name, Uint32* ptab_id, Uint32* pcol_no)
{
  return DictTabInfo::isBlobTableName(name, ptab_id, pcol_no);
}

bool
is_ndb_blob_table(const NdbTableImpl* t)
{
  return is_ndb_blob_table(t->m_internalName.c_str());
}

bool
ignore_broken_blob_tables()
{
  /* To be able to fix broken blob tables, we must be able
   * to ignore them when getting the table description
   */
  char envBuf[10];
  const char* v = NdbEnv_GetEnv("NDB_FORCE_IGNORE_BROKEN_BLOB",
                                envBuf,
                                10);
  return (v != NULL && *v != 0 && *v != '0' && *v != 'n' && *v != 'N');
}

//#define EVENT_DEBUG

/**
 * Column
 */
NdbColumnImpl::NdbColumnImpl()
  : NdbDictionary::Column(* this), m_attrId(-1), m_facade(this)
{
  init();
}

NdbColumnImpl::NdbColumnImpl(NdbDictionary::Column & f)
  : NdbDictionary::Column(* this), m_attrId(-1), m_facade(&f)
{
  init();
}

NdbColumnImpl&
NdbColumnImpl::operator=(const NdbColumnImpl& col)
{
  m_attrId = col.m_attrId;
  m_name = col.m_name;
  m_type = col.m_type;
  m_precision = col.m_precision;
  m_cs = col.m_cs;
  m_scale = col.m_scale;
  m_length = col.m_length;
  m_pk = col.m_pk;
  m_distributionKey = col.m_distributionKey;
  m_nullable = col.m_nullable;
  m_autoIncrement = col.m_autoIncrement;
  m_autoIncrementInitialValue = col.m_autoIncrementInitialValue;
  m_defaultValue.assign(col.m_defaultValue);
  m_attrSize = col.m_attrSize; 
  m_arraySize = col.m_arraySize;
  m_arrayType = col.m_arrayType;
  m_storageType = col.m_storageType;
  m_blobVersion = col.m_blobVersion;
  m_dynamic = col.m_dynamic;
  m_indexSourced = col.m_indexSourced;
  m_keyInfoPos = col.m_keyInfoPos;
  if (col.m_blobTable == NULL)
    m_blobTable = NULL;
  else {
    if (m_blobTable == NULL)
      m_blobTable = new NdbTableImpl();
    m_blobTable->assign(*col.m_blobTable);
  }
  m_column_no = col.m_column_no;
  // Do not copy m_facade !!

  return *this;
}

void
NdbColumnImpl::init(Type t)
{
  // do not use default_charset_info as it may not be initialized yet
  // use binary collation until NDB tests can handle charsets
  CHARSET_INFO* default_cs = &my_charset_bin;
  m_blobVersion = 0;
  m_type = t;
  switch (m_type) {
  case Tinyint:
  case Tinyunsigned:
  case Smallint:
  case Smallunsigned:
  case Mediumint:
  case Mediumunsigned:
  case Int:
  case Unsigned:
  case Bigint:
  case Bigunsigned:
  case Float:
  case Double:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Olddecimal:
  case Olddecimalunsigned:
  case Decimal:
  case Decimalunsigned:
    m_precision = 10;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Char:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = default_cs;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Varchar:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = default_cs;
    m_arrayType = NDB_ARRAYTYPE_SHORT_VAR;
    break;
  case Binary:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Varbinary:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_SHORT_VAR;
    break;
  case Datetime:
  case Date:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Blob:
  case Text:
    m_precision = 256;
    m_scale = 8000;
    m_length = 0; // default no striping
    m_cs = m_type == Blob ? NULL : default_cs;
    m_arrayType = NDB_ARRAYTYPE_MEDIUM_VAR;
    m_blobVersion = NDB_BLOB_V2;
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
    if (NdbEnv_GetEnv("NDB_DEFAULT_BLOB_V1", (char *)0, 0)) {
      m_length = 4;
      m_arrayType = NDB_ARRAYTYPE_FIXED;
      m_blobVersion = NDB_BLOB_V1;
    }
#endif
#endif
    break;
  case Time:
  case Year:
  case Timestamp:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Bit:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Longvarchar:
    m_precision = 0;
    m_scale = 0;
    m_length = 1; // legal
    m_cs = default_cs;
    m_arrayType = NDB_ARRAYTYPE_MEDIUM_VAR;
    break;
  case Longvarbinary:
    m_precision = 0;
    m_scale = 0;
    m_length = 1; // legal
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_MEDIUM_VAR;
    break;
  case Time2:
  case Datetime2:
  case Timestamp2:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  default:
  case Undefined:
    assert(false);
    break;
  }
  m_pk = false;
  m_nullable = false;
  m_distributionKey = false;
  m_keyInfoPos = 0;
  // next 2 are set at run time
  m_attrSize = 0;
  m_arraySize = 0;
  m_autoIncrement = false;
  m_autoIncrementInitialValue = 1;
  m_blobTable = NULL;
  m_storageType = NDB_STORAGETYPE_MEMORY;
  m_dynamic = false;
  m_indexSourced= false;
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  if(NdbEnv_GetEnv("NDB_DEFAULT_DISK", (char *)0, 0))
    m_storageType = NDB_STORAGETYPE_DISK;
#endif
#endif
}

NdbColumnImpl::~NdbColumnImpl()
{
  if (m_blobTable != NULL)
    delete m_blobTable;
  m_blobTable = NULL;
}

bool
NdbColumnImpl::equal(const NdbColumnImpl& col) const 
{
  DBUG_ENTER("NdbColumnImpl::equal");
  DBUG_PRINT("info", ("this: %p  &col: %p", this, &col));
  /* New member comparisons added here should also be
   * handled in the BackupRestore::column_compatible_check()
   * member of tools/restore/consumer_restore.cpp
   */
  if(strcmp(m_name.c_str(), col.m_name.c_str()) != 0){
    DBUG_RETURN(false);
  }
  if(m_type != col.m_type){
    DBUG_RETURN(false);
  }
  if(m_pk != col.m_pk){
    DBUG_RETURN(false);
  }
  if(m_nullable != col.m_nullable){
    DBUG_RETURN(false);
  }
  if (m_pk) {
    if (m_distributionKey != col.m_distributionKey) {
      DBUG_RETURN(false);
    }
  }
  if (m_precision != col.m_precision ||
      m_scale != col.m_scale ||
      m_length != col.m_length ||
      m_cs != col.m_cs) {
    DBUG_RETURN(false);
  }
  if (m_autoIncrement != col.m_autoIncrement){
    DBUG_RETURN(false);
  }
  if (m_defaultValue.length() != col.m_defaultValue.length())
    DBUG_RETURN(false);

  if(memcmp(m_defaultValue.get_data(), col.m_defaultValue.get_data(), m_defaultValue.length()) != 0){
    DBUG_RETURN(false);
  }

  if (m_arrayType != col.m_arrayType || m_storageType != col.m_storageType){
    DBUG_RETURN(false);
  }
  if (m_blobVersion != col.m_blobVersion) {
    DBUG_RETURN(false);
  }
  if(m_dynamic != col.m_dynamic){
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

void
NdbColumnImpl::create_pseudo_columns()
{
  NdbDictionary::Column::FRAGMENT=
    NdbColumnImpl::create_pseudo("NDB$FRAGMENT");
  NdbDictionary::Column::FRAGMENT_FIXED_MEMORY=
    NdbColumnImpl::create_pseudo("NDB$FRAGMENT_FIXED_MEMORY");
  NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY=
    NdbColumnImpl::create_pseudo("NDB$FRAGMENT_VARSIZED_MEMORY");
  NdbDictionary::Column::ROW_COUNT=
    NdbColumnImpl::create_pseudo("NDB$ROW_COUNT");
  NdbDictionary::Column::COMMIT_COUNT=
    NdbColumnImpl::create_pseudo("NDB$COMMIT_COUNT");
  NdbDictionary::Column::ROW_SIZE=
    NdbColumnImpl::create_pseudo("NDB$ROW_SIZE");
  NdbDictionary::Column::RANGE_NO=
    NdbColumnImpl::create_pseudo("NDB$RANGE_NO");
  NdbDictionary::Column::DISK_REF=
    NdbColumnImpl::create_pseudo("NDB$DISK_REF");
  NdbDictionary::Column::RECORDS_IN_RANGE=
    NdbColumnImpl::create_pseudo("NDB$RECORDS_IN_RANGE");
  NdbDictionary::Column::ROWID=
    NdbColumnImpl::create_pseudo("NDB$ROWID");
  NdbDictionary::Column::ROW_GCI=
    NdbColumnImpl::create_pseudo("NDB$ROW_GCI");
  NdbDictionary::Column::ROW_GCI64 =
    NdbColumnImpl::create_pseudo("NDB$ROW_GCI64");
  NdbDictionary::Column::ROW_AUTHOR =
    NdbColumnImpl::create_pseudo("NDB$ROW_AUTHOR");
  NdbDictionary::Column::ANY_VALUE=
    NdbColumnImpl::create_pseudo("NDB$ANY_VALUE");
  NdbDictionary::Column::COPY_ROWID=
    NdbColumnImpl::create_pseudo("NDB$COPY_ROWID");
  NdbDictionary::Column::OPTIMIZE=
    NdbColumnImpl::create_pseudo("NDB$OPTIMIZE");
  NdbDictionary::Column::FRAGMENT_EXTENT_SPACE =
    NdbColumnImpl::create_pseudo("NDB$FRAGMENT_EXTENT_SPACE");
  NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE =
    NdbColumnImpl::create_pseudo("NDB$FRAGMENT_FREE_EXTENT_SPACE");
  NdbDictionary::Column::LOCK_REF = 
    NdbColumnImpl::create_pseudo("NDB$LOCK_REF");
  NdbDictionary::Column::OP_ID = 
    NdbColumnImpl::create_pseudo("NDB$OP_ID");
}

void
NdbColumnImpl::destory_pseudo_columns()
{
  delete NdbDictionary::Column::FRAGMENT;
  delete NdbDictionary::Column::FRAGMENT_FIXED_MEMORY;
  delete NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY;
  delete NdbDictionary::Column::ROW_COUNT;
  delete NdbDictionary::Column::COMMIT_COUNT;
  delete NdbDictionary::Column::ROW_SIZE;
  delete NdbDictionary::Column::RANGE_NO;
  delete NdbDictionary::Column::DISK_REF;
  delete NdbDictionary::Column::RECORDS_IN_RANGE;
  delete NdbDictionary::Column::ROWID;
  delete NdbDictionary::Column::ROW_GCI;
  delete NdbDictionary::Column::ROW_GCI64;
  delete NdbDictionary::Column::ROW_AUTHOR;
  delete NdbDictionary::Column::ANY_VALUE;
  delete NdbDictionary::Column::OPTIMIZE;
  NdbDictionary::Column::FRAGMENT= 0;
  NdbDictionary::Column::FRAGMENT_FIXED_MEMORY= 0;
  NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY= 0;
  NdbDictionary::Column::ROW_COUNT= 0;
  NdbDictionary::Column::COMMIT_COUNT= 0;
  NdbDictionary::Column::ROW_SIZE= 0;
  NdbDictionary::Column::RANGE_NO= 0;
  NdbDictionary::Column::DISK_REF= 0;
  NdbDictionary::Column::RECORDS_IN_RANGE= 0;
  NdbDictionary::Column::ROWID= 0;
  NdbDictionary::Column::ROW_GCI= 0;
  NdbDictionary::Column::ROW_GCI64= 0;
  NdbDictionary::Column::ROW_AUTHOR= 0;
  NdbDictionary::Column::ANY_VALUE= 0;
  NdbDictionary::Column::OPTIMIZE= 0;

  delete NdbDictionary::Column::COPY_ROWID;
  NdbDictionary::Column::COPY_ROWID = 0;

  delete NdbDictionary::Column::FRAGMENT_EXTENT_SPACE;
  NdbDictionary::Column::FRAGMENT_EXTENT_SPACE = 0;

  delete NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE;
  NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE = 0;

  delete NdbDictionary::Column::LOCK_REF;
  delete NdbDictionary::Column::OP_ID;
  NdbDictionary::Column::LOCK_REF = 0;
  NdbDictionary::Column::OP_ID = 0;
}

NdbDictionary::Column *
NdbColumnImpl::create_pseudo(const char * name){
  NdbDictionary::Column * col = new NdbDictionary::Column();
  col->setName(name);
  if(!strcmp(name, "NDB$FRAGMENT")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$FRAGMENT_FIXED_MEMORY")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT_FIXED_MEMORY;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$FRAGMENT_VARSIZED_MEMORY")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT_VARSIZED_MEMORY;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$ROW_COUNT")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::ROW_COUNT;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$COMMIT_COUNT")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::COMMIT_COUNT;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$ROW_SIZE")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::ROW_SIZE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$RANGE_NO")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::RANGE_NO;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$DISK_REF")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::DISK_REF;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$RECORDS_IN_RANGE")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::RECORDS_IN_RANGE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 4;
  } else if(!strcmp(name, "NDB$ROWID")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::ROWID;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 2;
  } else if(!strcmp(name, "NDB$ROW_GCI")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::ROW_GCI;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
    col->m_impl.m_nullable = true;
  } else if(!strcmp(name, "NDB$ROW_GCI64")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::ROW_GCI64;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
    col->m_impl.m_nullable = true;
  } else if(!strcmp(name, "NDB$ROW_AUTHOR")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::ROW_AUTHOR;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
    col->m_impl.m_nullable = true;
  } else if(!strcmp(name, "NDB$ANY_VALUE")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::ANY_VALUE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$COPY_ROWID")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::COPY_ROWID;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 2;
  } else if(!strcmp(name, "NDB$OPTIMIZE")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::OPTIMIZE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 1;
  } else if(!strcmp(name, "NDB$FRAGMENT_EXTENT_SPACE")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT_EXTENT_SPACE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 2;
  } else if(!strcmp(name, "NDB$FRAGMENT_FREE_EXTENT_SPACE")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT_FREE_EXTENT_SPACE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 2;
  } else if (!strcmp(name, "NDB$LOCK_REF")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::LOCK_REF;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 3;
  } else if (!strcmp(name, "NDB$OP_ID")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::OP_ID;
    col->m_impl.m_attrSize = 8;
    col->m_impl.m_arraySize = 1;
  }
  else {
    abort();
  }
  col->m_impl.m_storageType = NDB_STORAGETYPE_MEMORY;
  return col;
}

/**
 * NdbTableImpl
 */

NdbTableImpl::NdbTableImpl()
  : NdbDictionary::Table(* this), 
    NdbDictObjectImpl(NdbDictionary::Object::UserTable), m_facade(this)
{
  init();
}

NdbTableImpl::NdbTableImpl(NdbDictionary::Table & f)
  : NdbDictionary::Table(* this), 
    NdbDictObjectImpl(NdbDictionary::Object::UserTable), m_facade(&f)
{
  init();
}

NdbTableImpl::~NdbTableImpl()
{
  if (m_index != 0) {
    delete m_index;
    m_index = 0;
  }
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete m_columns[i];
  
  if (m_ndbrecord !=0) {
    free(m_ndbrecord); // As it was calloc'd
    m_ndbrecord= 0;
  }

  if (m_pkMask != 0) {
    free(const_cast<unsigned char *>(m_pkMask));
    m_pkMask= 0;
  }
}

void
NdbTableImpl::init(){
  m_id= RNIL;
  m_version = ~0;
  m_status = NdbDictionary::Object::Invalid;
  m_type = NdbDictionary::Object::TypeUndefined;
  m_primaryTableId= RNIL;
  m_internalName.clear();
  m_externalName.clear();
  m_mysqlName.clear();
  m_frm.clear();
  m_fd.clear();
  m_range.clear();
  m_fragmentType= NdbDictionary::Object::HashMapPartition;
  m_hashValueMask= 0;
  m_hashpointerValue= 0;
  m_linear_flag= true;
  m_primaryTable.clear();
  m_default_no_part_flag = 1;
  m_logging= true;
  m_temporary = false;
  m_row_gci = true;
  m_row_checksum = true;
  m_force_var_part = false;
  m_has_default_values = false;
  m_kvalue= 6;
  m_minLoadFactor= 78;
  m_maxLoadFactor= 80;
  m_keyLenInWords= 0;
  m_partitionBalance = NdbDictionary::Object::PartitionBalance_ForRPByLDM;
  m_fragmentCount= 0;
  m_partitionCount = 0;
  m_index= NULL;
  m_indexType= NdbDictionary::Object::TypeUndefined;
  m_noOfKeys= 0;
  m_noOfDistributionKeys= 0;
  m_noOfBlobs= 0;
  m_replicaCount= 0;
  m_noOfAutoIncColumns = 0;
  m_ndbrecord= 0;
  m_pkMask= 0;
  m_min_rows = 0;
  m_max_rows = 0;
  m_tablespace_name.clear();
  m_tablespace_id = RNIL;
  m_tablespace_version = ~0;
  m_single_user_mode = 0;
  m_hash_map_id = RNIL;
  m_hash_map_version = ~0;
  m_storageType = NDB_STORAGETYPE_DEFAULT;
  m_extra_row_gci_bits = 0;
  m_extra_row_author_bits = 0;
  m_read_backup = 0;
  m_fully_replicated = false;

#ifdef VM_TRACE
  {
    char buf[100];
    const char* b = NdbEnv_GetEnv("NDB_READ_BACKUP_TABLES", buf, sizeof(buf));
    if (b)
    {
      m_read_backup = 1;
    }
    if (NdbEnv_GetEnv("NDB_FULLY_REPLICATED", buf, sizeof(buf)) != 0)
    {
      m_read_backup = 1;
      m_fully_replicated = 1;
      m_partitionBalance =
        NdbDictionary::Object::PartitionBalance_ForRAByLDM;
    }
  }
#endif
}

bool
NdbTableImpl::equal(const NdbTableImpl& obj) const 
{
  DBUG_ENTER("NdbTableImpl::equal");
  if ((m_internalName.c_str() == NULL) || 
      (strcmp(m_internalName.c_str(), "") == 0) ||
      (obj.m_internalName.c_str() == NULL) || 
      (strcmp(obj.m_internalName.c_str(), "") == 0))
  {
    // Shallow equal
    if(strcmp(getName(), obj.getName()) != 0)
    {
      DBUG_PRINT("info",("name %s != %s",getName(),obj.getName()));
      DBUG_RETURN(false);    
    }
  }
  else
  {
    // Deep equal
    if(strcmp(m_internalName.c_str(), obj.m_internalName.c_str()) != 0)
    {
      DBUG_PRINT("info",("m_internalName %s != %s",
			 m_internalName.c_str(),obj.m_internalName.c_str()));
      DBUG_RETURN(false);
    }
  }
  if (m_frm.length() != obj.m_frm.length() ||
      (memcmp(m_frm.get_data(), obj.m_frm.get_data(), m_frm.length())))
  {
    DBUG_PRINT("info",("m_frm not equal"));
    DBUG_RETURN(false);
  }
  if (!m_fd.equal(obj.m_fd))
  {
    DBUG_PRINT("info",("m_fd not equal"));
    DBUG_RETURN(false);
  }
  if (!m_range.equal(obj.m_range))
  {
    DBUG_PRINT("info",("m_range not equal"));
    DBUG_RETURN(false);
  }

  if (m_partitionBalance != obj.m_partitionBalance)
  {
    DBUG_RETURN(false);
  }

  /**
   * TODO: Why is not fragment count compared??
   */

  if(m_fragmentType != obj.m_fragmentType)
  {
    DBUG_PRINT("info",("m_fragmentType %d != %d",m_fragmentType,
                        obj.m_fragmentType));
    DBUG_RETURN(false);
  }
  if(m_columns.size() != obj.m_columns.size())
  {
    DBUG_PRINT("info",("m_columns.size %d != %d",m_columns.size(),
                       obj.m_columns.size()));
    DBUG_RETURN(false);
  }

  for(unsigned i = 0; i<obj.m_columns.size(); i++)
  {
    if(!m_columns[i]->equal(* obj.m_columns[i]))
    {
      DBUG_PRINT("info",("m_columns [%d] != [%d]",i,i));
      DBUG_RETURN(false);
    }
  }
  
  if(m_linear_flag != obj.m_linear_flag)
  {
    DBUG_PRINT("info",("m_linear_flag %d != %d",m_linear_flag,
                        obj.m_linear_flag));
    DBUG_RETURN(false);
  }

  if(m_max_rows != obj.m_max_rows)
  {
    DBUG_PRINT("info",("m_max_rows %d != %d",(int32)m_max_rows,
                       (int32)obj.m_max_rows));
    DBUG_RETURN(false);
  }

  if(m_default_no_part_flag != obj.m_default_no_part_flag)
  {
    DBUG_PRINT("info",("m_default_no_part_flag %d != %d",m_default_no_part_flag,
                        obj.m_default_no_part_flag));
    DBUG_RETURN(false);
  }

  if(m_logging != obj.m_logging)
  {
    DBUG_PRINT("info",("m_logging %d != %d",m_logging,obj.m_logging));
    DBUG_RETURN(false);
  }

  if(m_temporary != obj.m_temporary)
  {
    DBUG_PRINT("info",("m_temporary %d != %d",m_temporary,obj.m_temporary));
    DBUG_RETURN(false);
  }

  if(m_row_gci != obj.m_row_gci)
  {
    DBUG_PRINT("info",("m_row_gci %d != %d",m_row_gci,obj.m_row_gci));
    DBUG_RETURN(false);
  }

  if(m_row_checksum != obj.m_row_checksum)
  {
    DBUG_PRINT("info",("m_row_checksum %d != %d",m_row_checksum,
                        obj.m_row_checksum));
    DBUG_RETURN(false);
  }

  if(m_kvalue != obj.m_kvalue)
  {
    DBUG_PRINT("info",("m_kvalue %d != %d",m_kvalue,obj.m_kvalue));
    DBUG_RETURN(false);
  }

  if(m_minLoadFactor != obj.m_minLoadFactor)
  {
    DBUG_PRINT("info",("m_minLoadFactor %d != %d",m_minLoadFactor,
                        obj.m_minLoadFactor));
    DBUG_RETURN(false);
  }

  if(m_maxLoadFactor != obj.m_maxLoadFactor)
  {
    DBUG_PRINT("info",("m_maxLoadFactor %d != %d",m_maxLoadFactor,
                        obj.m_maxLoadFactor));
    DBUG_RETURN(false);
  }

  if(m_tablespace_id != obj.m_tablespace_id)
  {
    DBUG_PRINT("info",("m_tablespace_id %d != %d",m_tablespace_id,
                        obj.m_tablespace_id));
    DBUG_RETURN(false);
  }

  if(m_tablespace_version != obj.m_tablespace_version)
  {
    DBUG_PRINT("info",("m_tablespace_version %d != %d",m_tablespace_version,
                        obj.m_tablespace_version));
    DBUG_RETURN(false);
  }

  if(m_id != obj.m_id)
  {
    DBUG_PRINT("info",("m_id %d != %d",m_id,obj.m_id));
    DBUG_RETURN(false);
  }

  if(m_version != obj.m_version)
  {
    DBUG_PRINT("info",("m_version %d != %d",m_version,obj.m_version));
    DBUG_RETURN(false);
  }

  if(m_type != obj.m_type)
  {
    DBUG_PRINT("info",("m_type %d != %d",m_type,obj.m_type));
    DBUG_RETURN(false);
  }

  if (m_type == NdbDictionary::Object::UniqueHashIndex ||
      m_type == NdbDictionary::Object::OrderedIndex)
  {
    if(m_primaryTableId != obj.m_primaryTableId)
    {
      DBUG_PRINT("info",("m_primaryTableId %d != %d",m_primaryTableId,
                 obj.m_primaryTableId));
      DBUG_RETURN(false);
    }
    if (m_indexType != obj.m_indexType)
    {
      DBUG_PRINT("info",("m_indexType %d != %d",m_indexType,obj.m_indexType));
      DBUG_RETURN(false);
    }
    if(strcmp(m_primaryTable.c_str(), obj.m_primaryTable.c_str()) != 0)
    {
      DBUG_PRINT("info",("m_primaryTable %s != %s",
			 m_primaryTable.c_str(),obj.m_primaryTable.c_str()));
      DBUG_RETURN(false);
    }
  }
  
  if(m_single_user_mode != obj.m_single_user_mode)
  {
    DBUG_PRINT("info",("m_single_user_mode %d != %d",
                       (int32)m_single_user_mode,
                       (int32)obj.m_single_user_mode));
    DBUG_RETURN(false);
  }

  if (m_extra_row_gci_bits != obj.m_extra_row_gci_bits)
  {
    DBUG_PRINT("info",("m_extra_row_gci_bits %d != %d",
                       (int32)m_extra_row_gci_bits,
                       (int32)obj.m_extra_row_gci_bits));
    DBUG_RETURN(false);
  }

  if (m_extra_row_author_bits != obj.m_extra_row_author_bits)
  {
    DBUG_PRINT("info",("m_extra_row_author_bits %d != %d",
                       (int32)m_extra_row_author_bits,
                       (int32)obj.m_extra_row_author_bits));
    DBUG_RETURN(false);
  }

  if (m_read_backup != obj.m_read_backup)
  {
    DBUG_PRINT("info",("m_read_backup %d != %d",
                       (int32)m_read_backup,
                       (int32)obj.m_read_backup));
    DBUG_RETURN(false);
  }

  if (m_fully_replicated != obj.m_fully_replicated)
  {
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

int
NdbTableImpl::assign(const NdbTableImpl& org)
{
  DBUG_ENTER("NdbTableImpl::assign");
  DBUG_PRINT("info", ("this: %p  &org: %p", this, &org));
  m_primaryTableId = org.m_primaryTableId;
  if (!m_internalName.assign(org.m_internalName) ||
      updateMysqlName())
  {
    DBUG_RETURN(-1);
  }
  m_externalName.assign(org.m_externalName);
  m_frm.assign(org.m_frm.get_data(), org.m_frm.length());
  m_fd.assign(org.m_fd);
  m_range.assign(org.m_range);

  m_fragmentType = org.m_fragmentType;
  if (m_fragmentType == NdbDictionary::Object::HashMapPartition)
  {
    m_hash_map_id = org.m_hash_map_id;
    m_hash_map_version = org.m_hash_map_version;
    m_hash_map.assign(org.m_hash_map);
  }
  else
  {
    m_hash_map_id = RNIL;
    m_hash_map_version = ~0;
  }
  /*
    m_columnHashMask, m_columnHash, m_hashValueMask, m_hashpointerValue
    is state calculated by computeAggregates and buildColumnHash
  */
  unsigned i;
  for(i = 0; i < m_columns.size(); i++)
  {
    delete m_columns[i];
  }
  m_columns.clear();
  for(i = 0; i < org.m_columns.size(); i++)
  {
    NdbColumnImpl * col = new NdbColumnImpl();
    if (col == NULL)
    {
      DBUG_RETURN(-1);
    }
    const NdbColumnImpl * iorg = org.m_columns[i];
    (* col) = (* iorg);
    if (m_columns.push_back(col))
    {
      delete col;
      DBUG_RETURN(-1);
    }
  }

  m_fragments = org.m_fragments;

  m_linear_flag = org.m_linear_flag;
  m_max_rows = org.m_max_rows;
  m_default_no_part_flag = org.m_default_no_part_flag;
  m_logging = org.m_logging;
  m_temporary = org.m_temporary;
  m_row_gci = org.m_row_gci;
  m_row_checksum = org.m_row_checksum;
  m_force_var_part = org.m_force_var_part;
  m_has_default_values = org.m_has_default_values;
  m_kvalue = org.m_kvalue;
  m_minLoadFactor = org.m_minLoadFactor;
  m_maxLoadFactor = org.m_maxLoadFactor;
  m_keyLenInWords = org.m_keyLenInWords;
  m_fragmentCount = org.m_fragmentCount;
  m_partitionCount = org.m_partitionCount;
  m_partitionBalance = org.m_partitionBalance;
  m_single_user_mode = org.m_single_user_mode;
  m_extra_row_gci_bits = org.m_extra_row_gci_bits;
  m_extra_row_author_bits = org.m_extra_row_author_bits;
  m_read_backup = org.m_read_backup;
  m_fully_replicated = org.m_fully_replicated;

  if (m_index != 0)
    delete m_index;
  m_index = org.m_index;
 
  m_primaryTable = org.m_primaryTable;
  m_indexType = org.m_indexType;

  m_noOfKeys = org.m_noOfKeys;
  m_noOfDistributionKeys = org.m_noOfDistributionKeys;
  m_noOfBlobs = org.m_noOfBlobs;
  m_replicaCount = org.m_replicaCount;

  m_noOfAutoIncColumns = org.m_noOfAutoIncColumns;

  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;

  m_max_rows = org.m_max_rows;
  m_min_rows = org.m_min_rows;

  m_tablespace_name = org.m_tablespace_name;
  m_tablespace_id= org.m_tablespace_id;
  m_tablespace_version = org.m_tablespace_version;
  m_storageType = org.m_storageType;

  m_hash_map_id = org.m_hash_map_id;
  m_hash_map_version = org.m_hash_map_version;

  DBUG_PRINT("info", ("m_logging: %u, m_read_backup %u"
                      " tableVersion: %u",
                      m_logging,
                      m_read_backup,
                      m_version));

  computeAggregates();
  if (buildColumnHash() != 0)
  {
    DBUG_RETURN(-1);
  }
         
  DBUG_RETURN(0);
}

int NdbTableImpl::setName(const char * name)
{
  return !m_externalName.assign(name);
}

const char * 
NdbTableImpl::getName() const
{
  return m_externalName.c_str();
}

int
NdbTableImpl::getDbName(char buf[], size_t len) const
{
  if (len == 0)
    return -1;

  // db/schema/table
  const char *ptr = m_internalName.c_str();

  size_t pos = 0;
  while (ptr[pos] && ptr[pos] != table_name_separator)
  {
    buf[pos] = ptr[pos];
    pos++;

    if (pos == len)
      return -1;
  }
  buf[pos] = 0;
  return 0;
}

int
NdbTableImpl::getSchemaName(char buf[], size_t len) const
{
  if (len == 0)
    return -1;

  // db/schema/table
  const char *ptr = m_internalName.c_str();

  // skip over "db"
  while (*ptr && *ptr != table_name_separator)
    ptr++;

  buf[0] = 0;
  if (*ptr == table_name_separator)
  {
    ptr++;
    size_t pos = 0;
    while (ptr[pos] && ptr[pos] != table_name_separator)
    {
      buf[pos] = ptr[pos];
      pos++;

      if (pos == len)
        return -1;
    }
    buf[pos] = 0;
  }

  return 0;
}

void
NdbTableImpl::setDbSchema(const char * db, const char * schema)
{
  m_internalName.assfmt("%s%c%s%c%s",
                        db,
                        table_name_separator,
                        schema,
                        table_name_separator,
                        m_externalName.c_str());
  updateMysqlName();
}

void
NdbTableImpl::computeAggregates()
{
  m_noOfKeys = 0;
  m_keyLenInWords = 0;
  m_noOfDistributionKeys = 0;
  m_noOfBlobs = 0;
  m_noOfDiskColumns = 0;
  Uint32 i, n;
  for (i = 0; i < m_columns.size(); i++) {
    NdbColumnImpl* col = m_columns[i];
    if (col->m_pk) {
      m_noOfKeys++;
      m_keyLenInWords += (col->m_attrSize * col->m_arraySize + 3) / 4;
    }
    if (col->m_distributionKey)
      m_noOfDistributionKeys++; // XXX check PK
    
    if (col->getBlobType())
      m_noOfBlobs++;

    if (col->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
      m_noOfDiskColumns++;
    
    col->m_keyInfoPos = ~0;

    if (col->m_autoIncrement)
      m_noOfAutoIncColumns++;
  }
  if (m_noOfDistributionKeys == m_noOfKeys) {
    // all is none!
    m_noOfDistributionKeys = 0;
  }

  if (m_noOfDistributionKeys == 0) 
  {
    // none is all!
    for (i = 0, n = m_noOfKeys; n != 0; i++) {
      NdbColumnImpl* col = m_columns[i];
      if (col->m_pk) {
        col->m_distributionKey = true;
        n--;
      }
    }
  }
  
  Uint32 keyInfoPos = 0;
  for (i = 0, n = m_noOfKeys; n != 0; i++) {
    NdbColumnImpl* col = m_columns[i];
    if (col->m_pk) {
      col->m_keyInfoPos = keyInfoPos++;
      n--;
    }
  }
}

// TODO add error checks
// TODO use these internally at create and retrieve
int
NdbTableImpl::aggregate(NdbError& error)
{
  computeAggregates();
  return 0;
}
int
NdbTableImpl::validate(NdbError& error)
{
  if (aggregate(error) == -1)
    return -1;
  return 0;
}

void
NdbTableImpl::setFragmentCount(Uint32 count)
{
  m_fragmentCount= count;
}

Uint32 NdbTableImpl::getFragmentCount() const
{
  return m_fragmentCount;
}

Uint32 NdbTableImpl::getPartitionCount() const
{
  return m_partitionCount;
}

int NdbTableImpl::setFrm(const void* data, Uint32 len)
{
  return m_frm.assign(data, len);
}


class Extra_metadata
{
  /*
    The extra metadata is packed into a blob consisting of a header followed
    by the compressed extra metadata. The header indicates which version
    of metadata it contains as well as original and compressed length of
    the compressed data. The header is written in machine independent format.
    The metadata is assumed to already be in machine independent format. The
    metadata is compressed with zlib which is also machine independent.

    version 4 bytes
    orglen  4 bytes
    complen 4 bytes
    compressed data [complen] bytes

  */

  static const size_t BLOB_HEADER_SZ = 12;

public:

  static
  bool
  check_header(void* pack_data, Uint32 pack_length,
               Uint32& version)
  {
    if (pack_length == 0)
    {
      // No extra metadata
      return false; // not ok
    }

    if (pack_length < BLOB_HEADER_SZ)
    {
      // There are extra metadata but it's too short
      // to even have a header
      return false; // not ok
    }

    // Verify the header
    const uchar* header = static_cast<const uchar*>(pack_data);

    // First part is version
    version = uint4korr(header);

    // Second part is original length

    // The third part is packed length and should be equal to the
    // packed data length minus header length
    DBUG_ASSERT(uint4korr(header + 8) == (Uint32)pack_length - BLOB_HEADER_SZ);

    return true; // OK
  }


  /*
    pack is a method used to pack the extra metadata
    for a table which is stored inside the dictionary of NDB.

    SYNOPSIS
      pack()
      version                 The version to be written into the header
                              of the packed data
      data                    Pointer to data which should be packed
      len                     Length of data to pack
      out:pack_data           Reference to the pointer of the packed data
                              which is returned. The memory returned is to
                              be released by the caller.
      out:pack_len            Length of the packed data returned

    RETURN VALUES
      0                       Success
      >0                      Failure
  */
  static
  int pack(const Uint32 version,
           const void* data, const Uint32 len,
           void** pack_data, Uint32* pack_len)
  {
    DBUG_ENTER("Extra_metadata::pack");
    DBUG_PRINT("enter", ("data: 0x%lx  len: %lu", (long) data, (ulong) len));

    // Allocate memory large enough to hold header and
    // packed data
    const size_t blob_len= BLOB_HEADER_SZ + compressBound(len);
    uchar *blob = (uchar*) malloc(blob_len);
    if (blob == NULL)
    {
      DBUG_PRINT("error", ("Could not allocate memory to pack the data into"));
      DBUG_RETURN(1);
    }

    // Compress the data into the newly allocated memory, leave room
    // for the header to be written in front of the packed data
    // NOTE! The compressed_len variables provides the size of
    // the allocated buffer and will return the compressed length
    // Use an aligned stack variable of expected type to avoid
    // potential alignment issues.
    uLongf compressed_len = (uLongf)blob_len;
    const int compress_result =
        compress((Bytef*) blob + BLOB_HEADER_SZ, &compressed_len,
                 (Bytef*) data, (uLong)len);
    if (compress_result != Z_OK)
    {
      DBUG_PRINT("error", ("Failed to compress, error: %d", compress_result));
      free(blob);
      DBUG_RETURN(2);
    }

    DBUG_PRINT("info", ("len: %lu, compressed_len: %lu",
                        (ulong) len, (ulong) compressed_len));
    DBUG_DUMP("compressed", blob + BLOB_HEADER_SZ, compressed_len);

    /* Write header in machine independent format */
    int4store(blob,   version);
    int4store(blob+4, len);
    int4store(blob+8, (uint32) compressed_len);    /* compressed length */

    /* Assign return variables */
    *pack_data= blob;
    *pack_len=  BLOB_HEADER_SZ + compressed_len;

    DBUG_PRINT("exit", ("pack_data: 0x%lx  pack_len: %lu",
                        (long) *pack_data, (ulong) *pack_len));
    DBUG_RETURN(0);
  }


  /*
    unpack is a method used to unpack the extra metadata
    for a table which is stored inside the dictionary of NDB.

    SYNOPSIS
      unpack()
      pack_data               Pointer to data which should be unpacked
      out:unpack_data         Reference to the pointer to the unpacked data
      out:unpack_len          Length of unpacked data

    RETURN VALUES
      0                       Success
      >0                      Failure
  */
  static
  int unpack(const void* pack_data,
             void** unpack_data, Uint32* unpack_len)
  {
     DBUG_ENTER("Extra_metadata::unpack");
     DBUG_PRINT("enter", ("pack_data: 0x%lx", (long) pack_data));

     const char* header = static_cast<const char*>(pack_data);
     const Uint32 orglen =  uint4korr(header+4);
     const Uint32 complen = uint4korr(header+8);

     DBUG_PRINT("blob",("complen: %lu, orglen: %lu",
                        (ulong) complen, (ulong) orglen));
     DBUG_DUMP("blob->data", (uchar*)header + BLOB_HEADER_SZ, complen);

     // Allocate memory large enough to hold unpacked data
     uchar *data = (uchar*)malloc(orglen);
     if (data == NULL)
     {
       DBUG_PRINT("error", ("Could not allocate memory to unpack into"));
       DBUG_RETURN(1);
     }

     // Uncompress the packed data into the newly allocated buffer
     // NOTE! The uncompressed_len variables provides the size of
     // the allocated buffer and will return the uncompressed length
     // Use an aligned stack variable of expected type to avoid
     // potential alignment issues.
     uLongf uncompressed_len= (uLongf) orglen;
     const int uncompress_result =
         uncompress((Bytef*) data, &uncompressed_len,
                    (Bytef*) pack_data + BLOB_HEADER_SZ, (uLong) complen);
    if (uncompress_result != Z_OK)
     {
       DBUG_PRINT("error", ("Failed to uncompress, error: %d",
                            uncompress_result));
       free(data);
       DBUG_RETURN(2);
     }
     // Check that the uncompressed length returned by uncompress()
     // matches the value in the header
     DBUG_ASSERT(uncompressed_len == orglen);
 
     *unpack_data= data;
     *unpack_len=  orglen;

     DBUG_PRINT("exit", ("frmdata: 0x%lx  len: %lu", (long) *unpack_data,
                         (ulong) *unpack_len));
     DBUG_RETURN(0);
  }
};


int
NdbTableImpl::setExtraMetadata(Uint32 version,
                               const void* data, Uint32 data_length)
{
  // Pack the extra metadata
  void* pack_data;
  Uint32 pack_length;
  const int pack_result =
      Extra_metadata::pack(version,
                           data, data_length,
                           &pack_data, &pack_length);
  if (pack_result)
  {
    return pack_result;
  }

  const int assign_result = m_frm.assign(pack_data, pack_length);
  free(pack_data);

  return assign_result;
}


int
NdbTableImpl::getExtraMetadata(Uint32& version,
                               void** data, Uint32* data_length) const
{
  if (!Extra_metadata::check_header(m_frm.get_data(),
                                    m_frm.length(),
                                    version))
  {
    // No extra metadata header
    return 1;
  }

  if (Extra_metadata::unpack(m_frm.get_data(),
                             data, data_length))
  {
    return 2;
  }

  return 0;
}


const void * 
NdbTableImpl::getFrmData() const
{
  return m_frm.get_data();
}

Uint32
NdbTableImpl::getFrmLength() const 
{
  return m_frm.length();
}

int
NdbTableImpl::setFragmentData(const Uint32* data, Uint32 cnt)
{
  return m_fd.assign(data, cnt);
}

const Uint32 *
NdbTableImpl::getFragmentData() const
{
  return m_fd.getBase();
}

Uint32
NdbTableImpl::getFragmentDataLen() const 
{
  return m_fd.size();
}

int
NdbTableImpl::setRangeListData(const Int32* data, Uint32 len)
{
  return m_range.assign(data, len);
}

const Int32 *
NdbTableImpl::getRangeListData() const
{
  return m_range.getBase();
}

Uint32
NdbTableImpl::getRangeListDataLen() const 
{
  return m_range.size();
}

Uint32
NdbTableImpl::getFragmentNodes(Uint32 fragmentId, 
                               Uint32* nodeIdArrayPtr,
                               Uint32 arraySize) const
{
  const Uint16 *shortNodeIds;
  Uint32 nodeCount = get_nodes(fragmentId, &shortNodeIds);

  for(Uint32 i = 0; 
      ((i < nodeCount) &&
       (i < arraySize)); 
      i++)
    nodeIdArrayPtr[i] = (Uint32) shortNodeIds[i];

  return nodeCount;
}

int
NdbTableImpl::updateMysqlName()
{
  Vector<BaseString> v;
  if (m_internalName.split(v,"/") == 3)
  {
    return !m_mysqlName.assfmt("%s/%s",v[0].c_str(),v[2].c_str());
  }
  return !m_mysqlName.assign("");
}

static Uint32 Hash( const char* str ){
  Uint32 h = 0;
  size_t len = strlen(str);
  while(len >= 4){
    h = (h << 5) + h + str[0];
    h = (h << 5) + h + str[1];
    h = (h << 5) + h + str[2];
    h = (h << 5) + h + str[3];
    len -= 4;
    str += 4;
  }
  
  switch(len){
  case 3:
    h = (h << 5) + h + *str++;
  case 2:
    h = (h << 5) + h + *str++;
  case 1:
    h = (h << 5) + h + *str++;
  }
  return h;
}


/**
 * Column name hash
 * 
 * First (#cols) entries are hash buckets
 * which are either single values (unibucket)
 * or chain headers, referring to contiguous
 * entries stored at indices > #cols.
 *
 * Lookup hashes passed name, then
 * checks stored hash(es), then
 * uses strcmp, should get close to
 * 1 strcmp / lookup.
 * 
 * UniBucket / Chain entry
 * 
 * 31                             0
 * ccccccccccuhhhhhhhhhhhhhhhhhhhhh
 * 10        1     21          bits
 *
 * c = col number
 * u = Unibucket(1)
 * h = hashvalue
 *
 * Chain header
 *
 * 31                             0
 * lllllllllluppppppppppppppppppppp
 * 10        1     21          bits
 * 
 * l = chain length
 * u = Unibucket(0)
 * p = Chain pos 
 *     (Offset from chain header bucket)
 */
static const Uint32 UniBucket       = 0x00200000;
static const Uint32 ColNameHashMask = 0x001FFFFF;
static const Uint32 ColShift = 22;

NdbColumnImpl *
NdbTableImpl::getColumnByHash(const char * name) const
{
  Uint32 sz = m_columns.size();
  NdbColumnImpl* const * cols = m_columns.getBase();
  const Uint32 * hashtable = m_columnHash.getBase(); 
 
  const Uint32 hashValue = Hash(name) & ColNameHashMask;
  Uint32 bucket = hashValue & m_columnHashMask;
  bucket = (bucket < sz ? bucket : bucket - sz);
  hashtable += bucket;
  Uint32 tmp = * hashtable;
  if(tmp & UniBucket)
  { // No chaining
    sz = 1;
  } 
  else 
  {
    sz = (tmp >> ColShift);
    hashtable += (tmp & ColNameHashMask);
    tmp = * hashtable;
  }
  do 
  {
    if(hashValue == (tmp & ColNameHashMask))
    {
      NdbColumnImpl* col = cols[tmp >> ColShift];
      if(strncmp(name, col->m_name.c_str(), col->m_name.length()) == 0)
      {
        return col;
      }
    }
    hashtable++;
    tmp = * hashtable;
  } while(--sz > 0);

  return NULL;
}

int
NdbTableImpl::buildColumnHash()
{
  const Uint32 size = m_columns.size();

  /* Find mask size needed */ 
  int i;
  for(i = 31; i >= 0; i--){
    if(((1 << i) & size) != 0){
      m_columnHashMask = (1 << (i + 1)) - 1;
      break;
    }
  }

#ifndef NDEBUG
  /**
   * Guards to ensure we can represent all columns
   * correctly
   * Reduce stored hash bits if more columns supported 
   * in future
   */  
  const Uint32 ColBits
    ((sizeof(Uint32) * 8) - ColShift); 
  const Uint32 MaxCols = Uint32(1) << ColBits;
  assert(MaxCols >= MAX_ATTRIBUTES_IN_TABLE);
  assert((UniBucket & ColNameHashMask) == 0);
  assert((UniBucket >> ColShift) == 0);
  assert((UniBucket << ColBits) == 0x80000000);
  assert(m_columnHashMask <= ColNameHashMask);
#endif

  /* Build 2d hash as precursor to 1d hash array */
  Vector<Uint32> hashValues;
  Vector<Vector<Uint32> > chains;
  if (chains.fill(size, hashValues))
  {
    return -1;
  }

  for(i = 0; i< (int) size; i++){
    Uint32 hv = Hash(m_columns[i]->getName()) & ColNameHashMask;
    Uint32 bucket = hv & m_columnHashMask;
    bucket = (bucket < size ? bucket : bucket - size);
    assert(bucket < size);
    if (hashValues.push_back(hv) ||
        chains[bucket].push_back(i))
    {
      return -1;      
    }
  }

  /* Now build 1d hash array */
  m_columnHash.clear();
  Uint32 tmp = UniBucket;
  if (m_columnHash.fill((unsigned)size-1, tmp))   // Default no chaining
  {
    return -1;
  }

  Uint32 pos = 0; // In overflow vector
  for(i = 0; i< (int) size; i++)
  {
    const Uint32 sz = chains[i].size();
    if(sz == 1)
    {
      /* UniBucket */
      const Uint32 col = chains[i][0];
      const Uint32 hv = hashValues[col];
      Uint32 bucket = hv & m_columnHashMask;
      bucket = (bucket < size ? bucket : bucket - size);
      m_columnHash[bucket] = (col << ColShift) | UniBucket | hv;
    } 
    else if(sz > 1)
    {
      Uint32 col = chains[i][0];
      Uint32 hv = hashValues[col];
      Uint32 bucket = hv & m_columnHashMask;
      bucket = (bucket < size ? bucket : bucket - size);
      m_columnHash[bucket] = (sz << ColShift) | ((size - bucket) + pos);
      for(unsigned j = 0; j<sz; j++, pos++){
	Uint32 col = chains[i][j];	
	Uint32 hv = hashValues[col];
        if (m_columnHash.push_back((col << ColShift) | hv))
        {
          return -1;
        }
      }
    }
  }

  DBUG_PRINT("info", ("Column hash initialised with size %u for %u cols",
                      m_columnHash.size(),
                      m_columns.size()));

  assert(checkColumnHash());

  return 0;
}

void
NdbTableImpl::dumpColumnHash() const
{
  const Uint32 size = m_columns.size();

  printf("Table %s column hash stores %u columns in hash table size %u\n",
         getName(),
         size,
         m_columnHash.size());
  
  Uint32 comparisons = 0;

  for(size_t i = 0; i<m_columnHash.size(); i++){
    Uint32 tmp = m_columnHash[i];
    if(i < size)
    {
      if (tmp & UniBucket)
      {
        if (tmp == UniBucket)
        {
          printf("  m_columnHash[%d]  %x NULL\n", (Uint32) i, tmp);
        }
        else
        {
          Uint32 hash = m_columnHash[i] & ColNameHashMask;
          Uint32 bucket = (m_columnHash[i] & ColNameHashMask) & m_columnHashMask;
          printf("  m_columnHash[%d] %x %s HashVal %d Bucket %d Bucket2 %d\n", 
                 (Uint32) i, 
                 tmp,
                 m_columns[tmp >> ColShift]->getName(), 
                 hash,
                 bucket,
                 (bucket < size? bucket : bucket - size));
          comparisons++;
        }
      }
      else
      {
        /* Chain header */
        Uint32 chainStart = Uint32(i) + (tmp & ColNameHashMask);
        Uint32 chainLen = tmp >> ColShift;
        printf("  m_columnHash[%d] %x chain header of size %u @ +%u = %u\n",
               (Uint32) i,
               tmp,
               chainLen,
               (tmp & ColNameHashMask),
               chainStart);
        
        /* Always 1 comparison, sometimes more */
        comparisons += ((chainLen * (chainLen + 1)) / 2);
      }
    } 
    else /* i > size */
    { 
      /* Chain body  */
      Uint32 hash = m_columnHash[i] & ColNameHashMask;
      Uint32 bucket = (m_columnHash[i] & ColNameHashMask) & m_columnHashMask;
      printf("  m_columnHash[%d] %x %s HashVal %d Bucket %d Bucket2 %d\n", 
             (Uint32) i, 
             tmp,
             m_columns[tmp >> ColShift]->getName(), 
             hash,
             bucket,
             (bucket < size? bucket : bucket - size));
    }
  }

  Uint32 sigdig = comparisons/size;
  Uint32 places = 10000;
  printf("Entries = %u Hash Total comparisons = %u Average comparisons = %u.%u "
         "Expected average strcmps = 1\n",
         size,
         comparisons,
         sigdig,
         (comparisons * places / size) - (sigdig * places));
  /* Basic implementation behaviour (linear string search) */
  comparisons = (size * (size+1)) / 2;
  sigdig = comparisons / size;
  printf("Entries = %u Basic Total strcmps = %u Average strcmps = %u.%u\n",
         size,
         comparisons,
         sigdig,
         (comparisons * places / size) - (sigdig * places));
}

bool
NdbTableImpl::checkColumnHash() const
{
  bool ok = true;

  /**
   * Check hash lookup on a column object's name
   * maps back to itself
   */
  for (Uint32 i=0; i < m_columns.size(); i++)
  {
    const NdbColumnImpl* col = m_columns[i];
    
    const NdbColumnImpl* hashLookup = getColumnByHash(col->getName());
    if (hashLookup != col)
    {
      /**
       * We didn't get the column we expected
       * Can be hit in testcases checking tables having
       * duplicate column names for different columns.
       * If the column name is the same then it's not a 
       * hashing problem
       */
      if (strcmp(col->getName(), hashLookup->getName()) != 0)
      {
        printf("NdbDictionaryImpl.cpp::checkColumnHash() : "
               "Failed lookup on table %s col %u %s - gives %p %s\n",
               getName(),
               i, col->getName(),
               hashLookup,
               (hashLookup?hashLookup->getName():""));
        ok = false;
      }
    }
  }

  if (!ok)
  {
    dumpColumnHash();
  }

  return ok;
}

Uint32
NdbTableImpl::get_nodes(Uint32 fragmentId, const Uint16 ** nodes) const
{
  Uint32 pos = fragmentId * m_replicaCount;
  if (pos + m_replicaCount <= m_fragments.size())
  {
    *nodes = m_fragments.getBase()+pos;
    return m_replicaCount;
  }
  return 0;
}

int
NdbDictionary::Table::checkColumns(const Uint32* map, Uint32 len) const
{
  int ret = 0;
  Uint32 colCnt = m_impl.m_columns.size();
  if (map == 0)
  {
    ret |= 1;
    ret |= (m_impl.m_noOfDiskColumns) ? 2 : 0;
    ret |= (colCnt > m_impl.m_noOfDiskColumns) ? 4 : 0;
    return ret;
  }

  NdbColumnImpl** cols = m_impl.m_columns.getBase();
  const char * ptr = reinterpret_cast<const char*>(map);
  const char * end = ptr + len;
  Uint32 no = 0;
  while (ptr < end)
  {
    Uint32 val = (Uint32)* ptr;
    Uint32 idx = 1;
    for (Uint32 i = 0; i<8; i++)
    {
      if (val & idx)
      {
	if (cols[no]->getPrimaryKey())
	  ret |= 1;
	else
	{
	  if (cols[no]->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
	    ret |= 2;
	  else
	    ret |= 4;
	}
      }
      no ++;
      idx *= 2; 
      if (no == colCnt)
	return ret;
    }
    
    ptr++;
  }
  return ret;
}


  
/**
 * NdbIndexImpl
 */

NdbIndexImpl::NdbIndexImpl() : 
  NdbDictionary::Index(* this),
  NdbDictObjectImpl(NdbDictionary::Object::OrderedIndex), m_facade(this)
{
  init();
}

NdbIndexImpl::NdbIndexImpl(NdbDictionary::Index & f) : 
  NdbDictionary::Index(* this), 
  NdbDictObjectImpl(NdbDictionary::Object::OrderedIndex), m_facade(&f)
{
  init();
}

void NdbIndexImpl::init()
{
  m_id= RNIL;
  m_type= NdbDictionary::Object::TypeUndefined;
  m_logging= true;
  m_temporary= false;
  m_table= NULL;
}

NdbIndexImpl::~NdbIndexImpl(){
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete m_columns[i];  
}

int NdbIndexImpl::setName(const char * name)
{
  return !m_externalName.assign(name);
}

const char * 
NdbIndexImpl::getName() const
{
  return m_externalName.c_str();
}
 
int
NdbIndexImpl::setTable(const char * table)
{
  return !m_tableName.assign(table);
}
 
const char * 
NdbIndexImpl::getTable() const
{
  return m_tableName.c_str();
}

const NdbTableImpl *
NdbIndexImpl::getIndexTable() const
{
  return m_table;
}

/**
 * NdbOptimizeTableHandleImpl
 */

NdbOptimizeTableHandleImpl::NdbOptimizeTableHandleImpl(NdbDictionary::OptimizeTableHandle &f)
  : NdbDictionary::OptimizeTableHandle(* this),
    m_state(NdbOptimizeTableHandleImpl::CREATED),
    m_ndb(NULL), m_table(NULL),
    m_table_queue(NULL), m_table_queue_first(NULL), m_table_queue_end(NULL),
    m_trans(NULL), m_scan_op(NULL),
    m_facade(this)
{
}

NdbOptimizeTableHandleImpl::~NdbOptimizeTableHandleImpl()
{
  DBUG_ENTER("NdbOptimizeTableHandleImpl::~NdbOptimizeTableHandleImpl");
  close();
  DBUG_VOID_RETURN;
}

int NdbOptimizeTableHandleImpl::start()
{
  int noRetries = 100;
  DBUG_ENTER("NdbOptimizeTableImpl::start");

  if (m_table_queue)
  {
    const NdbTableImpl * table = m_table_queue->table;

    /*
     * Start/Restart transaction
     */
    while (noRetries-- > 0)
    {
      if (m_trans && (m_trans->restart() != 0))
      {
        m_ndb->closeTransaction(m_trans);
        m_trans = NULL;
      }
      else
        m_trans = m_ndb->startTransaction();
      if (!m_trans)
      {
        if (noRetries == 0)
          goto do_error;
        continue;
      }
      
      /*
       * Get first scan operation
       */ 
      if ((m_scan_op = m_trans->getNdbScanOperation(table->m_facade)) 
          == NULL)
      {
        m_ndb->getNdbError(m_trans->getNdbError().code);
        goto do_error;
      }
      
      /**
       * Define a result set for the scan.
       */ 
      if (m_scan_op->readTuples(NdbOperation::LM_Exclusive)) {
        m_ndb->getNdbError(m_trans->getNdbError().code);
        goto do_error;
      }
      
      /**
       * Start scan    (NoCommit since we are only reading at this stage);
       */
      if (m_trans->execute(NdbTransaction::NoCommit) != 0) {
        if (m_trans->getNdbError().status == NdbError::TemporaryError)
          continue;  /* goto next_retry */
        m_ndb->getNdbError(m_trans->getNdbError().code);
        goto do_error;
      }
      break;
    } // while (noRetries-- > 0)
    m_state = NdbOptimizeTableHandleImpl::INITIALIZED;
  } // if (m_table_queue)
  else
    m_state = NdbOptimizeTableHandleImpl::FINISHED;

  DBUG_RETURN(0);
do_error:
  DBUG_PRINT("info", ("NdbOptimizeTableImpl::start aborted"));
  m_state = NdbOptimizeTableHandleImpl::ABORTED;
  DBUG_RETURN(-1);
}

int NdbOptimizeTableHandleImpl::init(Ndb* ndb, const NdbTableImpl &table)
{
  DBUG_ENTER("NdbOptimizeTableHandleImpl::init");
  NdbDictionary::Dictionary* dict = ndb->getDictionary();
  Uint32 sz = table.m_columns.size();
  bool found_varpart = false;
  int blob_num = table.m_noOfBlobs;

  m_ndb = ndb;
  m_table = &table;

  /**
   * search whether there are var size columns in the table,
   * in first step, we only optimize var part, then if the
   * table has no var size columns, we do not do optimizing
   */
  for (Uint32 i = 0; i < sz; i++) {
    const NdbColumnImpl *col = m_table->m_columns[i];
    if (col != 0 && col->m_storageType == NDB_STORAGETYPE_MEMORY &&
        (col->m_dynamic || col->m_arrayType != NDB_ARRAYTYPE_FIXED)) {
      found_varpart= true;
      break;
    }
  }
  if (!found_varpart)
  {
    m_state = NdbOptimizeTableHandleImpl::FINISHED;
    DBUG_RETURN(0);
  }
  
  /*
   * Add main table to the table queue
   * to optimize
   */
  m_table_queue_end = new fifo_element_st(m_table, m_table_queue_end);
  m_table_queue = m_table_queue_first = m_table_queue_end;
  /*
   * Add any BLOB tables the table queue
   * to optimize.
   */
  for (int i = m_table->m_columns.size(); i > 0 && blob_num > 0;) {
    i--;
    NdbColumnImpl & c = *m_table->m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    
    blob_num--;
    const NdbTableImpl * blob_table = 
      (const NdbTableImpl *)dict->getBlobTable(m_table, c.m_attrId);
    if (blob_table)
    {
      m_table_queue_end = new fifo_element_st(blob_table, m_table_queue_end);
    }
  }
  /*
   * Initialize transaction
   */
  DBUG_RETURN(start());
}
 
int NdbOptimizeTableHandleImpl::next()
{
  int noRetries = 100;
  int done, check;
  DBUG_ENTER("NdbOptimizeTableHandleImpl::next");

  if (m_state == NdbOptimizeTableHandleImpl::FINISHED)
    DBUG_RETURN(0);
  else if (m_state != NdbOptimizeTableHandleImpl::INITIALIZED)
    DBUG_RETURN(-1);

  while (noRetries-- > 0)
  {
    if ((done = check = m_scan_op->nextResult(true)) == 0)
    {
      do 
      {
        /** 
         * Get update operation
         */
        NdbOperation * myUpdateOp = m_scan_op->updateCurrentTuple();
        if (myUpdateOp == 0)
        {
          m_ndb->getNdbError(m_trans->getNdbError().code);
          goto do_error;
        }
        /**
         * optimize a tuple through doing the update
         * first step, move varpart
         */
        Uint32 options = 0 | AttributeHeader::OPTIMIZE_MOVE_VARPART;
        myUpdateOp->setOptimize(options);
        /**
         * nextResult(false) means that the records
         * cached in the NDBAPI are modified before
         * fetching more rows from NDB.
         */
      } while ((check = m_scan_op->nextResult(false)) == 0);
    }

    /**
     * Commit when all cached tuple have been updated
     */
    if (check != -1)
      check = m_trans->execute(NdbTransaction::Commit);
    
    if (done == 1)
    {
      DBUG_PRINT("info", ("Done with table %s",
                          m_table_queue->table->getName()));
      /*
       * We are done with optimizing current table
       * move to next
       */
      fifo_element_st *current = m_table_queue;
      m_table_queue = current->next;
      /*
       * Start scan of next table
       */
      if (start() != 0) {
        m_ndb->getNdbError(m_trans->getNdbError().code);
        goto do_error;
      }
      DBUG_RETURN(1);
    }
    if (check == -1)
    {
      if (m_trans->getNdbError().status == NdbError::TemporaryError)
      {
        /*
         * If we encountered temporary error, retry
         */
        m_ndb->closeTransaction(m_trans);
        m_trans = NULL;
        if (start() != 0) {
          m_ndb->getNdbError(m_trans->getNdbError().code);
          goto do_error;
        }
        continue; //retry
      }
      m_ndb->getNdbError(m_trans->getNdbError().code);
      goto do_error;
    }
    if (m_trans->restart() != 0)
    {
      DBUG_PRINT("info", ("Failed to restart transaction"));
      m_ndb->closeTransaction(m_trans);
      m_trans = NULL;
      if (start() != 0) {
        m_ndb->getNdbError(m_trans->getNdbError().code);
        goto do_error;
      }
    }
 
    DBUG_RETURN(1);
  }
do_error:
  DBUG_PRINT("info", ("NdbOptimizeTableHandleImpl::next aborted"));
  m_state = NdbOptimizeTableHandleImpl::ABORTED;
  DBUG_RETURN(-1);
}
 
int NdbOptimizeTableHandleImpl::close()
{
  DBUG_ENTER("NdbOptimizeTableHandleImpl::close");
  /*
   * Drop queued tables
   */
  while(m_table_queue_first != NULL)
  {
    fifo_element_st *next = m_table_queue_first->next;
    delete m_table_queue_first;
    m_table_queue_first = next;
  }
  m_table_queue = m_table_queue_first = m_table_queue_end = NULL;
  if (m_trans)
  {
    m_ndb->closeTransaction(m_trans);
    m_trans = NULL;
  }
  m_state = NdbOptimizeTableHandleImpl::CLOSED;
  DBUG_RETURN(0);
}
 
/**
 * NdbOptimizeIndexHandleImpl
 */

NdbOptimizeIndexHandleImpl::NdbOptimizeIndexHandleImpl(NdbDictionary::OptimizeIndexHandle &f)
  : NdbDictionary::OptimizeIndexHandle(* this),
    m_state(NdbOptimizeIndexHandleImpl::CREATED),
    m_ndb(NULL), m_index(NULL),
    m_facade(this)
{
  DBUG_ENTER("NdbOptimizeIndexHandleImpl::NdbOptimizeIndexHandleImpl");
  DBUG_VOID_RETURN;
}

NdbOptimizeIndexHandleImpl::~NdbOptimizeIndexHandleImpl()
{
  DBUG_ENTER("NdbOptimizeIndexHandleImpl::~NdbOptimizeIndexHandleImpl");
  DBUG_VOID_RETURN;
}

int NdbOptimizeIndexHandleImpl::init(Ndb *ndb, const NdbIndexImpl &index)
{
  DBUG_ENTER("NdbOptimizeIndexHandleImpl::init");
  m_index = &index;
  m_state = NdbOptimizeIndexHandleImpl::INITIALIZED;
  /**
   * NOTE: we only optimize unique index
   */
  if (m_index->m_facade->getType() != NdbDictionary::Index::UniqueHashIndex)
    DBUG_RETURN(0);
  DBUG_RETURN(m_optimize_table_handle.m_impl.init(ndb, *index.getIndexTable()));
}
 
int NdbOptimizeIndexHandleImpl::next()
{
  DBUG_ENTER("NdbOptimizeIndexHandleImpl::next");
  if (m_state != NdbOptimizeIndexHandleImpl::INITIALIZED)
    DBUG_RETURN(0);
  if (m_index->m_facade->getType() != NdbDictionary::Index::UniqueHashIndex)
    DBUG_RETURN(0);
  DBUG_RETURN(m_optimize_table_handle.m_impl.next());
}
 
int NdbOptimizeIndexHandleImpl::close()
{
  DBUG_ENTER("NdbOptimizeIndexHandleImpl::close");
  m_state = NdbOptimizeIndexHandleImpl::CLOSED;
  if (m_index &&
      m_index->m_facade->getType() == NdbDictionary::Index::UniqueHashIndex)
    DBUG_RETURN(m_optimize_table_handle.m_impl.close());

  DBUG_RETURN(0);
}
 
/**
 * NdbEventImpl
 */

NdbEventImpl::NdbEventImpl() : 
  NdbDictionary::Event(* this),
  NdbDictObjectImpl(NdbDictionary::Object::TypeUndefined), m_facade(this)
{
  DBUG_ENTER("NdbEventImpl::NdbEventImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

NdbEventImpl::NdbEventImpl(NdbDictionary::Event & f) : 
  NdbDictionary::Event(* this),
  NdbDictObjectImpl(NdbDictionary::Object::TypeUndefined), m_facade(&f)
{
  DBUG_ENTER("NdbEventImpl::NdbEventImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

void NdbEventImpl::init()
{
  m_eventId= RNIL;
  m_eventKey= RNIL;
  mi_type= 0;
  m_dur= NdbDictionary::Event::ED_UNDEFINED;
  m_mergeEvents = false;
  m_tableImpl= NULL;
  m_rep= NdbDictionary::Event::ER_UPDATED;
}

NdbEventImpl::~NdbEventImpl()
{
  DBUG_ENTER("NdbEventImpl::~NdbEventImpl");
  DBUG_PRINT("info", ("this: %p", this));
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete  m_columns[i];
  if (m_tableImpl)
    delete m_tableImpl;
  DBUG_VOID_RETURN;
}

int NdbEventImpl::setName(const char * name)
{
  return !m_name.assign(name);
}

const char *NdbEventImpl::getName() const
{
  return m_name.c_str();
}

int
NdbEventImpl::setTable(const NdbDictionary::Table& table)
{
  setTable(&NdbTableImpl::getImpl(table));
  return !m_tableName.assign(m_tableImpl->getName());
}

int
NdbEventImpl::setTable(const NdbDictionary::Table *table)
{
  DBUG_ENTER("NdbEventImpl::setTable(const NdbDictionary::Table *table)");
  if (table == 0)
  {
    DBUG_PRINT("info", ("NdbEventImpl::setTable() this: %p invalid table ptr %p", this, table));
    DBUG_RETURN(-1);
  }
  setTable(&NdbTableImpl::getImpl(*table));
  DBUG_RETURN(!m_tableName.assign(m_tableImpl->getName()));
}

void 
NdbEventImpl::setTable(NdbTableImpl *tableImpl)
{
  DBUG_ENTER("NdbEventImpl::setTable");
  DBUG_PRINT("info", ("this: %p  tableImpl: %p", this, tableImpl));

  if (!m_tableImpl) 
    m_tableImpl = new NdbTableImpl();
  // Copy table, since event might be accessed from different threads
  m_tableImpl->assign(*tableImpl);
  DBUG_VOID_RETURN;
}

const NdbDictionary::Table *
NdbEventImpl::getTable() const
{
  if (m_tableImpl) 
    return m_tableImpl->m_facade;
  else
    return NULL;
}

int
NdbEventImpl::setTable(const char * table)
{
  return !m_tableName.assign(table);
}

const char *
NdbEventImpl::getTableName() const
{
  return m_tableName.c_str();
}

void
NdbEventImpl::addTableEvent(const NdbDictionary::Event::TableEvent t =  NdbDictionary::Event::TE_ALL)
{
  mi_type |= (unsigned)t;
}

bool
NdbEventImpl::getTableEvent(const NdbDictionary::Event::TableEvent t) const
{
  return (mi_type & (unsigned)t) == (unsigned)t;
}

void
NdbEventImpl::setDurability(NdbDictionary::Event::EventDurability d)
{
  m_dur = d;
}

NdbDictionary::Event::EventDurability
NdbEventImpl::getDurability() const
{
  return m_dur;
}

void
NdbEventImpl::setReport(NdbDictionary::Event::EventReport r)
{
  m_rep = r;
}

NdbDictionary::Event::EventReport
NdbEventImpl::getReport() const
{
  return m_rep;
}

int NdbEventImpl::getNoOfEventColumns() const
{
  return m_attrIds.size() + m_columns.size();
}

const NdbDictionary::Column *
NdbEventImpl::getEventColumn(unsigned no) const
{
  if (m_columns.size())
  {
    if (no < m_columns.size())
    {
      return m_columns[no];
    }
  }
  else if (m_attrIds.size())
  {
    if (no < m_attrIds.size())
    {
      NdbTableImpl* tab= m_tableImpl;
      if (tab == 0)
        return 0;
      return tab->getColumn(m_attrIds[no]);
    }
  }
  return 0;
}

/**
 * NdbDictionaryImpl
 */

/* Initialise static */
const Uint32 
NdbDictionaryImpl::m_emptyMask[MAXNROFATTRIBUTESINWORDS]= {0,0,0,0};

NdbDictionaryImpl::NdbDictionaryImpl(Ndb &ndb)
  : NdbDictionary::Dictionary(* this), 
    m_facade(this), 
    m_receiver(m_tx, m_error, m_warn),
    m_ndb(ndb)
{
  m_globalHash = 0;
  m_local_table_data_size= 0;
#ifdef VM_TRACE
  STATIC_ASSERT(
    (int)WarnUndobufferRoundUp == (int)CreateFilegroupConf::WarnUndobufferRoundUp &&
    (int)WarnUndofileRoundDown == (int)CreateFileConf::WarnUndofileRoundDown &&
    (int)WarnExtentRoundUp == (int)CreateFilegroupConf::WarnExtentRoundUp &&
    (int)WarnDatafileRoundDown == (int)CreateFileConf::WarnDatafileRoundDown &&
    (int)WarnDatafileRoundUp == (int)CreateFileConf::WarnDatafileRoundUp
  );
#endif
}

NdbDictionaryImpl::NdbDictionaryImpl(Ndb &ndb,
				     NdbDictionary::Dictionary & f)
  : NdbDictionary::Dictionary(* this), 
    m_facade(&f), 
    m_receiver(m_tx, m_error, m_warn),
    m_ndb(ndb)
{
  m_globalHash = 0;
  m_local_table_data_size= 0;
}

NdbDictionaryImpl::~NdbDictionaryImpl()
{
  /* Release local table references back to the global cache */
  NdbElement_t<Ndb_local_table_info> * curr = m_localHash.m_tableHash.getNext(0);
  if(m_globalHash){
    while(curr != 0){
      m_globalHash->lock();
      m_globalHash->release(curr->theData->m_table_impl);
      Ndb_local_table_info::destroy(curr->theData);
      m_globalHash->unlock();
      
      curr = m_localHash.m_tableHash.getNext(curr);
    }
  } else {
    assert(curr == 0);
  }
}

NdbTableImpl *
NdbDictionaryImpl::fetchGlobalTableImplRef(const GlobalCacheInitObject &obj)
{
  DBUG_ENTER("fetchGlobalTableImplRef");
  NdbTableImpl *impl;
  int error= 0;

  m_globalHash->lock();
  impl = m_globalHash->get(obj.m_name.c_str(), &error);
  m_globalHash->unlock();

  if (impl == 0){
    if (error == 0)
      impl = m_receiver.getTable(obj.m_name,
                                 m_ndb.usingFullyQualifiedNames());
    else
      m_error.code = 4000;
    if (impl != 0 && (obj.init(this, *impl)))
    {
      delete impl;
      impl = 0;
    }
    m_globalHash->lock();
    m_globalHash->put(obj.m_name.c_str(), impl);
    m_globalHash->unlock();
  }

  DBUG_RETURN(impl);
}

void
NdbDictionaryImpl::putTable(NdbTableImpl *impl)
{
  NdbTableImpl *old;

  int ret = getBlobTables(*impl);
  int error = 0;
  (void)ret;
  assert(ret == 0);

  m_globalHash->lock();
  if ((old= m_globalHash->get(impl->m_internalName.c_str(), &error)))
  {
    m_globalHash->alter_table_rep(old->m_internalName.c_str(),
                                  impl->m_id,
                                  impl->m_version,
                                  FALSE);
  }
  m_globalHash->put(impl->m_internalName.c_str(), impl);
  m_globalHash->unlock();
  Ndb_local_table_info *info=
    Ndb_local_table_info::create(impl, m_local_table_data_size);
  
  m_localHash.put(impl->m_internalName.c_str(), info);
}

int
NdbDictionaryImpl::getBlobTables(NdbTableImpl &t)
{
  unsigned n= t.m_noOfBlobs;
  DBUG_ENTER("NdbDictionaryImpl::getBlobTables");
  // optimized for blob column being the last one
  // and not looking for more than one if not neccessary
  for (unsigned i = t.m_columns.size(); i > 0 && n > 0;) {
    i--;
    NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    n--;
    // retrieve blob table def from DICT - by-pass cache
    char btname[NdbBlobImpl::BlobTableNameSize];
    NdbBlob::getBlobTableName(btname, &t, &c);
    BaseString btname_internal = m_ndb.internalize_table_name(btname);
    NdbTableImpl* bt =
      m_receiver.getTable(btname_internal, m_ndb.usingFullyQualifiedNames());
    if (bt == NULL)
    {
      if (ignore_broken_blob_tables())
      {
        DBUG_PRINT("info", ("Blob table %s not found, continuing", btname));
        continue;
      }
      DBUG_RETURN(-1);
    }

    // TODO check primary id/version when returned by DICT

    // the blob column owns the blob table
    assert(c.m_blobTable == NULL);
    c.m_blobTable = bt;

    // change storage type to that of PART column
    const char* colName = c.m_blobVersion == 1 ? "DATA" : "NDB$DATA";
    const NdbColumnImpl* bc = bt->getColumn(colName);
    assert(bc != 0);
    assert(c.m_storageType == NDB_STORAGETYPE_MEMORY);
    c.m_storageType = bc->m_storageType;
  }
  DBUG_RETURN(0); 
}

NdbTableImpl*
NdbDictionaryImpl::getBlobTable(const NdbTableImpl& tab, uint col_no)
{
  if (col_no < tab.m_columns.size()) {
    NdbColumnImpl* col = tab.m_columns[col_no];
    if (col != NULL) {
      NdbTableImpl* bt = col->m_blobTable;
      if (bt != NULL)
        return bt;
      else
        m_error.code = 4273; // No blob table..
    } else
      m_error.code = 4249; // Invalid table..
  } else
    m_error.code = 4318; // Invalid attribute..
  return NULL;
}

NdbTableImpl*
NdbDictionaryImpl::getBlobTable(uint tab_id, uint col_no)
{
  DBUG_ENTER("NdbDictionaryImpl::getBlobTable");
  DBUG_PRINT("enter", ("tab_id: %u col_no %u", tab_id, col_no));

  NdbTableImpl* tab = m_receiver.getTable(tab_id,
                                          m_ndb.usingFullyQualifiedNames());
  if (tab == NULL)
    DBUG_RETURN(NULL);
  Ndb_local_table_info* info =
    get_local_table_info(tab->m_internalName);
  delete tab;
  if (info == NULL)
    DBUG_RETURN(NULL);
  NdbTableImpl* bt = getBlobTable(*info->m_table_impl, col_no);
  DBUG_RETURN(bt);
}

bool
NdbDictionaryImpl::setTransporter(class Ndb* ndb, 
				  class TransporterFacade * tf)
{
  m_globalHash = tf->m_globalDictCache;
  if(m_receiver.setTransporter(ndb)){
    return true;
  }
  return false;
}

NdbTableImpl *
NdbDictionaryImpl::getIndexTable(NdbIndexImpl * index,
				 NdbTableImpl * table)
{
  const char *current_db= m_ndb.getDatabaseName();
  NdbTableImpl *index_table;
  const BaseString internalName(
    m_ndb.internalize_index_name(table, index->getName()));
  // Get index table in system database
  m_ndb.setDatabaseName(NDB_SYSTEM_DATABASE);
  index_table= getTable(m_ndb.externalizeTableName(internalName.c_str()));
  m_ndb.setDatabaseName(current_db);
  if (!index_table)
  {
    // Index table not found
    // Try geting index table in current database (old format)
    index_table= getTable(m_ndb.externalizeTableName(internalName.c_str()));    
  }
  return index_table;
}

bool
NdbDictInterface::setTransporter(class Ndb* ndb)
{
  m_reference = ndb->getReference();
  m_impl = ndb->theImpl;
  
  return true;
}

TransporterFacade *
NdbDictInterface::getTransporter() const
{
  return m_impl->m_transporter_facade;
}

NdbDictInterface::~NdbDictInterface()
{
}

void 
NdbDictInterface::execSignal(void* dictImpl, 
			     const class NdbApiSignal* signal,
			     const struct LinearSectionPtr ptr[3])
{
  NdbDictInterface * tmp = (NdbDictInterface*)dictImpl;
  
  const Uint32 gsn = signal->readSignalNumber();
  switch(gsn){
  case GSN_GET_TABINFOREF:
    tmp->execGET_TABINFO_REF(signal, ptr);
    break;
  case GSN_GET_TABINFO_CONF:
    tmp->execGET_TABINFO_CONF(signal, ptr);
    break;
  case GSN_CREATE_TABLE_REF:
    tmp->execCREATE_TABLE_REF(signal, ptr);
    break;
  case GSN_CREATE_TABLE_CONF:
    tmp->execCREATE_TABLE_CONF(signal, ptr);
    break;
  case GSN_DROP_TABLE_REF:
    tmp->execDROP_TABLE_REF(signal, ptr);
    break;
  case GSN_DROP_TABLE_CONF:
    tmp->execDROP_TABLE_CONF(signal, ptr);
    break;
  case GSN_ALTER_TABLE_REF:
    tmp->execALTER_TABLE_REF(signal, ptr);
    break;
  case GSN_ALTER_TABLE_CONF:
    tmp->execALTER_TABLE_CONF(signal, ptr);
    break;
  case GSN_CREATE_INDX_REF:
    tmp->execCREATE_INDX_REF(signal, ptr);
    break;
  case GSN_CREATE_INDX_CONF:
    tmp->execCREATE_INDX_CONF(signal, ptr);
    break;
  case GSN_DROP_INDX_REF:
    tmp->execDROP_INDX_REF(signal, ptr);
    break;
  case GSN_DROP_INDX_CONF:
    tmp->execDROP_INDX_CONF(signal, ptr);
    break;
  case GSN_INDEX_STAT_CONF:
    tmp->execINDEX_STAT_CONF(signal, ptr);
    break;
  case GSN_INDEX_STAT_REF:
    tmp->execINDEX_STAT_REF(signal, ptr);
    break;
  case GSN_CREATE_EVNT_REF:
    tmp->execCREATE_EVNT_REF(signal, ptr);
    break;
  case GSN_CREATE_EVNT_CONF:
    tmp->execCREATE_EVNT_CONF(signal, ptr);
    break;
  case GSN_SUB_START_CONF:
    tmp->execSUB_START_CONF(signal, ptr);
    break;
  case GSN_SUB_START_REF:
    tmp->execSUB_START_REF(signal, ptr);
    break;
  case GSN_SUB_STOP_CONF:
    tmp->execSUB_STOP_CONF(signal, ptr);
    break;
  case GSN_SUB_STOP_REF:
    tmp->execSUB_STOP_REF(signal, ptr);
    break;
  case GSN_DROP_EVNT_REF:
    tmp->execDROP_EVNT_REF(signal, ptr);
    break;
  case GSN_DROP_EVNT_CONF:
    tmp->execDROP_EVNT_CONF(signal, ptr);
    break;
  case GSN_LIST_TABLES_CONF:
    tmp->execLIST_TABLES_CONF(signal, ptr);
    break;
  case GSN_CREATE_FILEGROUP_REF:
    tmp->execCREATE_FILEGROUP_REF(signal, ptr);
    break;
  case GSN_CREATE_FILEGROUP_CONF:
    tmp->execCREATE_FILEGROUP_CONF(signal, ptr);
    break;
  case GSN_CREATE_FILE_REF:
    tmp->execCREATE_FILE_REF(signal, ptr);
    break;
  case GSN_CREATE_FILE_CONF:
    tmp->execCREATE_FILE_CONF(signal, ptr);
    break;
  case GSN_DROP_FILEGROUP_REF:
    tmp->execDROP_FILEGROUP_REF(signal, ptr);
    break;
  case GSN_DROP_FILEGROUP_CONF:
    tmp->execDROP_FILEGROUP_CONF(signal, ptr);
    break;
  case GSN_DROP_FILE_REF:
    tmp->execDROP_FILE_REF(signal, ptr);
    break;
  case GSN_DROP_FILE_CONF:
    tmp->execDROP_FILE_CONF(signal, ptr);
    break;
  case GSN_SCHEMA_TRANS_BEGIN_CONF:
    tmp->execSCHEMA_TRANS_BEGIN_CONF(signal, ptr);
    break;
  case GSN_SCHEMA_TRANS_BEGIN_REF:
    tmp->execSCHEMA_TRANS_BEGIN_REF(signal, ptr);
    break;
  case GSN_SCHEMA_TRANS_END_CONF:
    tmp->execSCHEMA_TRANS_END_CONF(signal, ptr);
    break;
  case GSN_SCHEMA_TRANS_END_REF:
    tmp->execSCHEMA_TRANS_END_REF(signal, ptr);
    break;
  case GSN_SCHEMA_TRANS_END_REP:
    tmp->execSCHEMA_TRANS_END_REP(signal, ptr);
    break;
  case GSN_WAIT_GCP_CONF:
    tmp->execWAIT_GCP_CONF(signal, ptr);
    break;
  case GSN_WAIT_GCP_REF:
    tmp->execWAIT_GCP_REF(signal, ptr);
    break;
  case GSN_CREATE_HASH_MAP_REF:
    tmp->execCREATE_HASH_MAP_REF(signal, ptr);
    break;
  case GSN_CREATE_HASH_MAP_CONF:
    tmp->execCREATE_HASH_MAP_CONF(signal, ptr);
    break;
  case GSN_CREATE_FK_REF:
    tmp->execCREATE_FK_REF(signal, ptr);
    break;
  case GSN_CREATE_FK_CONF:
    tmp->execCREATE_FK_CONF(signal, ptr);
    break;

  case GSN_DROP_FK_REF:
    tmp->execDROP_FK_REF(signal, ptr);
    break;
  case GSN_DROP_FK_CONF:
    tmp->execDROP_FK_CONF(signal, ptr);
    break;

  case GSN_NODE_FAILREP:
  {
    DBUG_ENTER("NdbDictInterface::NODE_FAILREP");
    const NodeFailRep *rep = CAST_CONSTPTR(NodeFailRep,
                                           signal->getDataPtr());
    Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
    assert(len == NodeBitmask::Size); // only full length in ndbapi
    for (Uint32 i = BitmaskImpl::find_first(len, rep->theAllNodes);
         i != BitmaskImpl::NotFound;
         i = BitmaskImpl::find_next(len, rep->theAllNodes, i + 1))
    {
      if (i <= MAX_DATA_NODE_ID)
      {
        // NdbDictInterface only cares about data-nodes (so far??)
        tmp->m_impl->theWaiter.nodeFail(i);
      }
    }
    DBUG_VOID_RETURN;
    break;
  }
  default:
    abort();
  }
}

void
NdbDictInterface::execNodeStatus(void* dictImpl, Uint32 aNode, Uint32 ns_event)
{
}

int
NdbDictInterface::dictSignal(NdbApiSignal* sig, 
			     LinearSectionPtr ptr[3], int secs,
			     int node_specification,
			     Uint32 wst,
			     int timeout, Uint32 RETRIES,
			     const int *errcodes, int temporaryMask)
{
  DBUG_ENTER("NdbDictInterface::dictSignal");
  DBUG_PRINT("enter", ("useMasterNodeId: %d", node_specification));

  int sleep = 50;
  int mod = 5;

  for(Uint32 i = 0; i<RETRIES; i++)
  {
    if (i > 0)
    {
      Uint32 t = sleep + 10 * (rand() % mod);
#ifdef VM_TRACE
      ndbout_c("retry sleep %ums on error %u", t, m_error.code);
#endif
      NdbSleep_MilliSleep(t);
    }
    if (i == RETRIES / 2)
    {
      mod = 10;
    }
    if (i == 3*RETRIES/4)
    {
      sleep = 100;
    }

    m_buffer.clear();

    // Protected area
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(* m_impl);
    Uint32 node;
    switch(node_specification){
    case 0:
      node = (m_impl->get_node_alive(m_masterNodeId) ? m_masterNodeId :
	      (m_masterNodeId = getTransporter()->get_an_alive_node()));
      break;
    case -1:
      node = getTransporter()->get_an_alive_node();
      break;
    default:
      node = node_specification;
    }
    DBUG_PRINT("info", ("node %d", node));
    if(node == 0){
      if (getTransporter()->is_cluster_completely_unavailable())
      {
        m_error.code= 4009;
      }
      else
      {
        m_error.code = 4035;
      }
      DBUG_RETURN(-1);
    }
    int res = (ptr ? 
	       m_impl->sendFragmentedSignal(sig, node, ptr, secs):
	       m_impl->sendSignal(sig, node));
    if(res != 0){
      DBUG_PRINT("info", ("dictSignal failed to send signal"));
      m_error.code = 4007;
      continue;
    }    
    
    m_impl->incClientStat(Ndb::WaitMetaRequestCount,1);
    m_error.code= 0;
    int ret_val= poll_guard.wait_n_unlock(timeout, node, wst, true);
    // End of Protected area  
    
    if(ret_val == 0 && m_error.code == 0){
      // Normal return
      DBUG_RETURN(0);
    }
    
    if(m_impl->get_ndbapi_config_parameters().m_verbose >= 2)
    {
      if (m_error.code == 0)
      {
        g_eventLogger->info("dictSignal() request gsn %u to 0x%x on node %u "
                            "with %u sections failed with no error",
                            sig->theVerId_signalNumber,
                            sig->theReceiversBlockNumber,
                            node,
                            secs);
        g_eventLogger->info("dictSignal() poll_guard.wait_n_unlock() "
                            "returned %d, state is %u",
                            ret_val,
                            m_impl->theWaiter.get_state());
      }
    }

    /**
     * Handle error codes
     */
    if(ret_val == -2) //WAIT_NODE_FAILURE
    {
      m_error.code = 4013;
      continue;
    }
    if(m_impl->theWaiter.get_state() == WST_WAIT_TIMEOUT)
    {
      DBUG_PRINT("info", ("dictSignal caught time-out"));
      if(m_impl->get_ndbapi_config_parameters().m_verbose >= 2)
      {
        g_eventLogger->info("NdbDictionaryImpl::dictSignal() WST_WAIT_TIMEOUT for gsn %u"
                            "to 0x%x on node %u with %u sections.",
                            sig->theVerId_signalNumber,
                            sig->theReceiversBlockNumber,
                            node,
                            secs);
      }
      m_error.code = 4008;
      DBUG_RETURN(-1);
    }
    
    if ( temporaryMask == -1)
    {
      const NdbError &error= getNdbError();
      if (error.status ==  NdbError::TemporaryError)
      {
        continue;
      }
    }
    else if ( (temporaryMask & m_error.code) != 0 )
    {
      continue;
    }
    DBUG_PRINT("info", ("dictSignal caught error= %d", m_error.code));
    
    if(m_error.code && errcodes)
    {
      int j;
      for(j = 0; errcodes[j] ; j++){
	if(m_error.code == errcodes[j]){
	  break;
	}
      }
      if(errcodes[j]) // Accepted error code
      {
        continue;
      }
    }
    break;
  }
  DBUG_RETURN(-1);
}

/*
  Get dictionary information for a table using table id as reference

  DESCRIPTION
    Sends a GET_TABINFOREQ signal containing the table id
 */
NdbTableImpl *
NdbDictInterface::getTable(int tableId, bool fullyQualifiedNames)
{
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());
  
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = tableId;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  return getTable(&tSignal, 0, 0, fullyQualifiedNames);
}

/*
  Get dictionary information for a table using table name as the reference

  DESCRIPTION
    Send GET_TABINFOREQ signal with the table name in the first
    long section part
*/

NdbTableImpl *
NdbDictInterface::getTable(const BaseString& name, bool fullyQualifiedNames)
{
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq* const req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  const Uint32 namelen= name.length() + 1; // NULL terminated
  const Uint32 namelen_words= (namelen + 3) >> 2; // Size in words

  req->senderRef= m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType=
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen= namelen;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber= DBDICT;
  tSignal.theVerId_signalNumber= GSN_GET_TABINFOREQ;
  tSignal.theLength= GetTabInfoReq::SignalLength;

  // Copy name to m_buffer to get a word sized buffer
  m_buffer.clear();
  if (m_buffer.grow(namelen_words*4+4) ||
      m_buffer.append(name.c_str(), namelen))
  {
    m_error.code= 4000;
    return NULL;
  }

#ifndef IGNORE_VALGRIND_WARNINGS
  Uint32 pad = 0;
  if (m_buffer.append(&pad, 4))
  {
    m_error.code= 4000;
    return NULL;
  }
#endif
  
  LinearSectionPtr ptr[1];
  ptr[0].p= (Uint32*)m_buffer.get_data();
  ptr[0].sz= namelen_words;

  return getTable(&tSignal, ptr, 1, fullyQualifiedNames);
}


NdbTableImpl *
NdbDictInterface::getTable(class NdbApiSignal * signal,
			   LinearSectionPtr ptr[3],
			   Uint32 noOfSections, bool fullyQualifiedNames)
{
  int errCodes[] = {GetTabInfoRef::Busy, 0 };
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::getTable"));
    timeout = 1000;
  });

  /* If timeout occurs while waiting for response to dict signal, timeout
   * state WST_WAIT_TIMEOUT is mapped to m_error.code = 4008 and dictSignal
   * returns -1. E.g. if getTable returns NULL, it does not necessarily mean
   * that the table was not found. The caller should check for error 4008,
   * and ensure that the error code is not overwritten by 'does not exist'
   * errors.
   */
  int r = dictSignal(signal, ptr, noOfSections,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
                     timeout,  // parse stage
                     100, 
                     errCodes); 

  if (r)
    return 0;
  
  NdbTableImpl * rt = 0;
  m_error.code = parseTableInfo(&rt, 
				(Uint32*)m_buffer.get_data(), 
  				m_buffer.length() / 4, 
				fullyQualifiedNames);
  if(rt)
  {
    if (rt->m_fragmentType == NdbDictionary::Object::HashMapPartition)
    {
      NdbHashMapImpl tmp;
      if (get_hashmap(tmp, rt->m_hash_map_id))
      {
        delete rt;
        return NULL;
      }
      for (Uint32 i = 0; i<tmp.m_map.size(); i++)
      {
        assert(tmp.m_map[i] <= NDB_PARTITION_MASK);
        rt->m_hash_map.push_back(tmp.m_map[i]);
      }
    }
  }
  
  return rt;
}

void
NdbDictInterface::execGET_TABINFO_CONF(const NdbApiSignal * signal,
				       const LinearSectionPtr ptr[3])
{
  const GetTabInfoConf* conf = CAST_CONSTPTR(GetTabInfoConf,
                                             signal->getDataPtr());
  const Uint32 i = GetTabInfoConf::DICT_TAB_INFO;

  if(!m_tx.checkRequestId(conf->senderData, "GET_TABINFO_CONF"))
    return; // signal from different (possibly timed-out) transaction

  if(signal->isFirstFragment()){
    m_fragmentId = signal->getFragmentId();
    if (m_buffer.grow(4 * conf->totalLen))
    {
      m_error.code= 4000;
      goto end;
    }
  } else {
    if(m_fragmentId != signal->getFragmentId()){
      abort();
    }
  }
  
  if (m_buffer.append(ptr[i].p, 4 * ptr[i].sz))
  {
    m_error.code= 4000;
  }
end:
  if(!signal->isLastFragment()){
    return;
  }  
  
  m_impl->theWaiter.signal(NO_WAIT);
}

void
NdbDictInterface::execGET_TABINFO_REF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execGET_TABINFO_REF");
  const GetTabInfoRef* ref = CAST_CONSTPTR(GetTabInfoRef, 
					   signal->getDataPtr());
  if(!m_tx.checkRequestId(ref->senderData, "GET_TABINFO_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  if (likely(signal->getLength() == GetTabInfoRef::SignalLength))
  {
    m_error.code= ref->errorCode;
  }
  else
  {
    /* 6.3 <-> 7.0 upgrade only */
    assert (signal->getLength() == GetTabInfoRef::OriginalSignalLength);
    m_error.code = (*(signal->getDataPtr() + 
                      GetTabInfoRef::OriginalErrorOffset));
  }
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Pack/Unpack tables
 */
struct ApiKernelMapping {
  Int32 kernelConstant;
  Int32 apiConstant;
};

Uint32
getApiConstant(Int32 kernelConstant, const ApiKernelMapping map[], Uint32 def)
{
  int i = 0;
  while(map[i].kernelConstant != kernelConstant){
    if(map[i].kernelConstant == -1 &&
       map[i].apiConstant == -1){
      return def;
    }
    i++;
  }
  return map[i].apiConstant;
}

Uint32
getKernelConstant(Int32 apiConstant, const ApiKernelMapping map[], Uint32 def)
{
  int i = 0;
  while(map[i].apiConstant != apiConstant){
    if(map[i].kernelConstant == -1 &&
       map[i].apiConstant == -1){
      return def;
    }
    i++;
  }
  return map[i].kernelConstant;
}

static const
ApiKernelMapping 
fragmentTypeMapping[] = {
  { DictTabInfo::AllNodesSmallTable,  NdbDictionary::Object::FragAllSmall },
  { DictTabInfo::AllNodesMediumTable, NdbDictionary::Object::FragAllMedium },
  { DictTabInfo::AllNodesLargeTable,  NdbDictionary::Object::FragAllLarge },
  { DictTabInfo::SingleFragment,      NdbDictionary::Object::FragSingle },
  { DictTabInfo::DistrKeyHash,      NdbDictionary::Object::DistrKeyHash },
  { DictTabInfo::DistrKeyLin,      NdbDictionary::Object::DistrKeyLin },
  { DictTabInfo::UserDefined,      NdbDictionary::Object::UserDefined },
  { DictTabInfo::HashMapPartition, NdbDictionary::Object::HashMapPartition },
  { -1, -1 }
};

static const
ApiKernelMapping
objectTypeMapping[] = {
  { DictTabInfo::SystemTable,        NdbDictionary::Object::SystemTable },
  { DictTabInfo::UserTable,          NdbDictionary::Object::UserTable },
  { DictTabInfo::UniqueHashIndex,    NdbDictionary::Object::UniqueHashIndex },
  { DictTabInfo::OrderedIndex,       NdbDictionary::Object::OrderedIndex },
  { DictTabInfo::HashIndexTrigger,   NdbDictionary::Object::HashIndexTrigger },
  { DictTabInfo::IndexTrigger,       NdbDictionary::Object::IndexTrigger },
  { DictTabInfo::SubscriptionTrigger,NdbDictionary::Object::SubscriptionTrigger },
  { DictTabInfo::ReadOnlyConstraint ,NdbDictionary::Object::ReadOnlyConstraint },
  { DictTabInfo::Tablespace,         NdbDictionary::Object::Tablespace },
  { DictTabInfo::LogfileGroup,       NdbDictionary::Object::LogfileGroup },
  { DictTabInfo::Datafile,           NdbDictionary::Object::Datafile },
  { DictTabInfo::Undofile,           NdbDictionary::Object::Undofile },
  { DictTabInfo::ReorgTrigger,       NdbDictionary::Object::ReorgTrigger },
  { DictTabInfo::FullyReplicatedTrigger,
    NdbDictionary::Object::FullyReplicatedTrigger },

  { DictTabInfo::ForeignKey,         NdbDictionary::Object::ForeignKey },
  { DictTabInfo::FKParentTrigger,    NdbDictionary::Object::FKParentTrigger },
  { DictTabInfo::FKChildTrigger,     NdbDictionary::Object::FKChildTrigger },
  { DictTabInfo::HashMap,            NdbDictionary::Object::HashMap },
  { -1, -1 }
};

static const
ApiKernelMapping
objectStateMapping[] = {
  { DictTabInfo::StateOffline,       NdbDictionary::Object::StateOffline },
  { DictTabInfo::StateBuilding,      NdbDictionary::Object::StateBuilding },
  { DictTabInfo::StateDropping,      NdbDictionary::Object::StateDropping },
  { DictTabInfo::StateOnline,        NdbDictionary::Object::StateOnline },
  { DictTabInfo::StateBackup,        NdbDictionary::Object::StateBackup },
  { DictTabInfo::StateBroken,        NdbDictionary::Object::StateBroken }, 
  { -1, -1 }
};

static const
ApiKernelMapping
objectStoreMapping[] = {
  { DictTabInfo::StoreNotLogged,     NdbDictionary::Object::StoreNotLogged },
  { DictTabInfo::StorePermanent,     NdbDictionary::Object::StorePermanent },
  { -1, -1 }
};

static const
ApiKernelMapping
indexTypeMapping[] = {
  { DictTabInfo::UniqueHashIndex,    NdbDictionary::Index::UniqueHashIndex },  
  { DictTabInfo::OrderedIndex,       NdbDictionary::Index::OrderedIndex },
  { -1, -1 }
};

int
NdbDictInterface::parseTableInfo(NdbTableImpl ** ret,
				 const Uint32 * data, Uint32 len,
				 bool fullyQualifiedNames,
                                 Uint32 version)
{
  SimplePropertiesLinearReader it(data, len);
  DictTabInfo::Table *tableDesc;
  SimpleProperties::UnpackStatus s;
  DBUG_ENTER("NdbDictInterface::parseTableInfo");

  tableDesc = (DictTabInfo::Table*)malloc(sizeof(DictTabInfo::Table));
  if (!tableDesc)
  {
    DBUG_RETURN(4000);
  }
  tableDesc->init();
  s = SimpleProperties::unpack(it, tableDesc, 
			       DictTabInfo::TableMapping, 
			       DictTabInfo::TableMappingSize, 
			       true, true);
  
  if(s != SimpleProperties::Break){
    free(tableDesc);
    DBUG_RETURN(703);
  }
  const char * internalName = tableDesc->TableName;
  const char * externalName = Ndb::externalizeTableName(internalName, fullyQualifiedNames);

  NdbTableImpl * impl = new NdbTableImpl();
  impl->m_id = tableDesc->TableId;
  impl->m_version = tableDesc->TableVersion;
  impl->m_status = NdbDictionary::Object::Retrieved;
  if (!impl->m_internalName.assign(internalName) ||
      impl->updateMysqlName() ||
      !impl->m_externalName.assign(externalName) ||
      impl->m_frm.assign(tableDesc->FrmData, tableDesc->FrmLen) ||
      impl->m_range.assign((Int32*)tableDesc->RangeListData,
                           /* yuck */tableDesc->RangeListDataLen / 4))
  {
    delete impl;
    free(tableDesc);
    DBUG_RETURN(4000);
  }

  {
    /**
     * NOTE: fragment data is currently an array of Uint16
     *       and len is specified in bytes (yuck)
     *       please change to Uint32 and len == count
     */
    Uint32 cnt = tableDesc->FragmentDataLen / 2;
    for (Uint32 i = 0; i<cnt; i++)
      if (impl->m_fd.push_back((Uint32)tableDesc->FragmentData[i]))
      {
        delete impl;
        free(tableDesc);
        DBUG_RETURN(4000);
      }
  }

  impl->m_fragmentCount = tableDesc->FragmentCount;

  /*
    We specifically don't get tablespace data and range/list arrays here
    since those are known by the MySQL Server through analysing the
    frm file.
    Fragment Data contains the real node group mapping and the fragment
    identities used for each fragment. At the moment we have no need for
    this.
    Frm file is needed for autodiscovery.
  */
  
  impl->m_fragmentType = (NdbDictionary::Object::FragmentType)
    getApiConstant(tableDesc->FragmentType, 
		   fragmentTypeMapping, 
		   (Uint32)NdbDictionary::Object::FragUndefined);

  if (impl->m_fragmentType == NdbDictionary::Object::HashMapPartition)
  {
    impl->m_hash_map_id = tableDesc->HashMapObjectId;
    impl->m_hash_map_version = tableDesc->HashMapVersion;
  }
  else
  {
    impl->m_hash_map_id = RNIL;
    impl->m_hash_map_version = ~0;
  }

  /**
   * In older version of ndb...hashMapObjectId was initialized to ~0
   *   instead of RNIL...
   */
  if (impl->m_hash_map_id == ~Uint32(0) &&
      impl->m_hash_map_version == ~Uint32(0))
  {
    impl->m_hash_map_id = RNIL;
  }

  Uint64 max_rows = ((Uint64)tableDesc->MaxRowsHigh) << 32;
  max_rows += tableDesc->MaxRowsLow;
  impl->m_max_rows = max_rows;
  Uint64 min_rows = ((Uint64)tableDesc->MinRowsHigh) << 32;
  min_rows += tableDesc->MinRowsLow;
  impl->m_min_rows = min_rows;
  impl->m_default_no_part_flag = tableDesc->DefaultNoPartFlag;
  impl->m_linear_flag = tableDesc->LinearHashFlag;
  impl->m_logging = tableDesc->TableLoggedFlag;
  impl->m_temporary = tableDesc->TableTemporaryFlag;
  impl->m_row_gci = tableDesc->RowGCIFlag;
  impl->m_row_checksum = tableDesc->RowChecksumFlag;
  impl->m_force_var_part = tableDesc->ForceVarPartFlag;
  impl->m_kvalue = tableDesc->TableKValue;
  impl->m_minLoadFactor = tableDesc->MinLoadFactor;
  impl->m_maxLoadFactor = tableDesc->MaxLoadFactor;
  impl->m_single_user_mode = tableDesc->SingleUserMode;
  impl->m_storageType = tableDesc->TableStorageType;
  impl->m_extra_row_gci_bits = tableDesc->ExtraRowGCIBits;
  impl->m_extra_row_author_bits = tableDesc->ExtraRowAuthorBits;
  impl->m_partitionBalance =
    (NdbDictionary::Object::PartitionBalance)tableDesc->PartitionBalance;
  impl->m_read_backup = tableDesc->ReadBackupFlag == 0 ? false : true;
  impl->m_partitionCount = tableDesc->PartitionCount;
  impl->m_fully_replicated =
    tableDesc->FullyReplicatedFlag == 0 ? false : true;


  DBUG_PRINT("info", ("m_logging: %u, partitionBalance: %d"
                      " m_read_backup %u, tableVersion: %u",
                      impl->m_logging,
                      impl->m_partitionBalance,
                      impl->m_read_backup,
                      impl->m_version));

  impl->m_indexType = (NdbDictionary::Object::Type)
    getApiConstant(tableDesc->TableType,
		   indexTypeMapping,
		   NdbDictionary::Object::TypeUndefined);

  bool columnsIndexSourced= false;

  if(impl->m_indexType == NdbDictionary::Object::TypeUndefined){
  } else {
    const char * externalPrimary = 
      Ndb::externalizeTableName(tableDesc->PrimaryTable, fullyQualifiedNames);
    if (!impl->m_primaryTable.assign(externalPrimary))
    {
      delete impl;
      free(tableDesc);
      DBUG_RETURN(4000);
    }
    columnsIndexSourced= true;
  }
  
  Uint32 i;
  for(i = 0; i < tableDesc->NoOfAttributes; i++) {
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    s = SimpleProperties::unpack(it, 
				 &attrDesc, 
				 DictTabInfo::AttributeMapping, 
				 DictTabInfo::AttributeMappingSize, 
				 true, true);
    if(s != SimpleProperties::Break){
      delete impl;
      free(tableDesc);
      DBUG_RETURN(703);
    }
    
    NdbColumnImpl * col = new NdbColumnImpl();
    col->m_attrId = attrDesc.AttributeId;
    col->setName(attrDesc.AttributeName);

    // check type and compute attribute size and array size
    if (! attrDesc.translateExtType()) {
      delete col;
      delete impl;
      free(tableDesc);
      DBUG_RETURN(703);
    }
    col->m_type = (NdbDictionary::Column::Type)attrDesc.AttributeExtType;
    col->m_precision = (attrDesc.AttributeExtPrecision & 0xFFFF);
    col->m_scale = attrDesc.AttributeExtScale;
    col->m_length = attrDesc.AttributeExtLength;
    // charset in upper half of precision
    unsigned cs_number = (attrDesc.AttributeExtPrecision >> 16);
    // charset is defined exactly for char types
    if (col->getCharType() != (cs_number != 0)) {
      delete col;
      delete impl;
      free(tableDesc);
      DBUG_RETURN(703);
    }
    if (col->getCharType()) {
      col->m_cs = get_charset(cs_number, MYF(0));
      if (col->m_cs == NULL) {
        delete col;
        delete impl;
        free(tableDesc);
        DBUG_RETURN(743);
      }
    }
    col->m_orgAttrSize = attrDesc.AttributeSize;
    col->m_attrSize = (1 << attrDesc.AttributeSize) / 8;
    col->m_arraySize = attrDesc.AttributeArraySize;
    col->m_arrayType = attrDesc.AttributeArrayType;
    if(attrDesc.AttributeSize == 0)
    {
      col->m_attrSize = 4;
      col->m_arraySize = (attrDesc.AttributeArraySize + 31) >> 5;
    }
    col->m_storageType = attrDesc.AttributeStorageType;
    col->m_dynamic = (attrDesc.AttributeDynamic != 0);
    col->m_indexSourced= columnsIndexSourced;

    if (col->getBlobType()) {
      if (unlikely(col->m_arrayType) == NDB_ARRAYTYPE_FIXED)
        col->m_blobVersion = NDB_BLOB_V1;
      else if (col->m_arrayType == NDB_ARRAYTYPE_MEDIUM_VAR)
        col->m_blobVersion = NDB_BLOB_V2;
      else {
        delete col;
        delete impl;
        free(tableDesc);
        DBUG_RETURN(4263);
      }
    }
    
    col->m_pk = attrDesc.AttributeKeyFlag;
    col->m_distributionKey = (attrDesc.AttributeDKey != 0);
    col->m_nullable = attrDesc.AttributeNullableFlag;
    col->m_autoIncrement = (attrDesc.AttributeAutoIncrement != 0);
    col->m_autoIncrementInitialValue = ~0;

    if (attrDesc.AttributeDefaultValueLen)
    {
      assert(attrDesc.AttributeDefaultValueLen >= sizeof(Uint32)); /* AttributeHeader */
      const char* defPtr = (const char*) attrDesc.AttributeDefaultValue;
      Uint32 a = * (const Uint32*) defPtr;
      AttributeHeader ah(ntohl(a));
      Uint32 bytesize = ah.getByteSize();
      assert(attrDesc.AttributeDefaultValueLen >= sizeof(Uint32) + bytesize);
      
      if (bytesize)
      {
        if (col->m_defaultValue.assign(defPtr + sizeof(Uint32), bytesize))
        {
          delete col;
          delete impl;
          free(tableDesc);
          DBUG_RETURN(4000);
        }
        
        /* Table meta-info is normally stored in network byte order by
         * SimpleProperties
         * For the default value 'Blob' we do the work
         */
        /* In-place convert network -> host */
        NdbSqlUtil::convertByteOrder(attrDesc.AttributeExtType,
                                     attrDesc.AttributeSize,
                                     attrDesc.AttributeArrayType,
                                     attrDesc.AttributeArraySize,
                                     (uchar*) col->m_defaultValue.get_data(),
                                     bytesize);
        
        impl->m_has_default_values = true;
      }
    }

    col->m_column_no = impl->m_columns.size();
    impl->m_columns.push_back(col);
    it.next();
  }

  impl->computeAggregates();
  if (impl->buildColumnHash() != 0)
  {
    delete impl;
    free(tableDesc);
    DBUG_RETURN(4000);
  }

  if(tableDesc->ReplicaDataLen > 0)
  {
    Uint16 replicaCount = ntohs(tableDesc->ReplicaData[0]);
    Uint16 fragCount = ntohs(tableDesc->ReplicaData[1]);

    assert(replicaCount <= 256);

    impl->m_replicaCount = (Uint8)replicaCount;
    impl->m_fragmentCount = fragCount;
    DBUG_PRINT("info", ("replicaCount=%x , fragCount=%x",replicaCount,fragCount));
    Uint32 pos = 2;
    for(i = 0; i < (Uint32) fragCount;i++)
    {
      pos++; // skip logpart
      for (Uint32 j = 0; j<(Uint32)replicaCount; j++)
      {
        if (impl->m_fragments.push_back(ntohs(tableDesc->ReplicaData[pos++])))
	 {
	   delete impl;
          free(tableDesc);
          DBUG_RETURN(4000);
        }
      }
    }

    Uint32 topBit = (1 << 31);
    for(; topBit && !(fragCount & topBit); ){
      topBit >>= 1;
    }
    impl->m_hashValueMask = topBit - 1;
    impl->m_hashpointerValue = fragCount - (impl->m_hashValueMask + 1);
  }
  else
  {
    impl->m_fragmentCount = tableDesc->FragmentCount;
    impl->m_replicaCount = 0;
    impl->m_hashValueMask = 0;
    impl->m_hashpointerValue = 0;
  }

  impl->m_tablespace_id = tableDesc->TablespaceId;
  impl->m_tablespace_version = tableDesc->TablespaceVersion;

  * ret = impl;

  if (version < MAKE_VERSION(5,1,3))
  {
    ;
  } 
  else
  {
    DBUG_ASSERT(impl->m_fragmentCount > 0);
  }
  free(tableDesc);
  DBUG_RETURN(0);
}

/*****************************************************************
 * Create table and alter table
 */
int
NdbDictionaryImpl::createTable(NdbTableImpl &t, NdbDictObjectImpl & objid)
{ 
  DBUG_ENTER("NdbDictionaryImpl::createTable");

  bool autoIncrement = false;
  Uint64 initialValue = 0;
  for (Uint32 i = 0; i < t.m_columns.size(); i++) {
    const NdbColumnImpl* c = t.m_columns[i];
    assert(c != NULL);
    if (c->m_autoIncrement) {
      if (autoIncrement) {
        m_error.code = 4335;
        DBUG_RETURN(-1);
      }
      autoIncrement = true;
      initialValue = c->m_autoIncrementInitialValue;
    }

    if (c->m_pk && (! c->m_defaultValue.empty())) {
      /* Default value for primary key column not supported */
      m_error.code = 792;
      DBUG_RETURN(-1);
    }
  }
 
  // create table
  if (m_receiver.createTable(m_ndb, t) != 0)
    DBUG_RETURN(-1);
  Uint32* data = (Uint32*)m_receiver.m_buffer.get_data();
  t.m_id = data[0];
  t.m_version = data[1];
  objid.m_id = data[0];
  objid.m_version = data[1];

  // update table def from DICT - by-pass cache
  NdbTableImpl* t2 =
    m_receiver.getTable(t.m_internalName, m_ndb.usingFullyQualifiedNames());

  // check if we got back same table
  if (t2 == NULL) {
    DBUG_PRINT("info", ("table %s dropped by another thread", 
                        t.m_internalName.c_str()));
    m_error.code = 283;
    DBUG_RETURN(-1);
  }
  if (t.m_id != t2->m_id || t.m_version != t2->m_version) {
    DBUG_PRINT("info", ("table %s re-created by another thread",
                        t.m_internalName.c_str()));
    m_error.code = 283;
    delete t2;
    DBUG_RETURN(-1);
  }

  // auto-increment - use "t" because initial value is not in DICT
  {
    if (autoIncrement) {
      // XXX unlikely race condition - t.m_id may no longer be same table
      // the tuple id range is not used on input
      Ndb::TupleIdRange range;
      if (m_ndb.setTupleIdInNdb(&t, range, initialValue, false) == -1) {
        assert(m_ndb.theError.code != 0);
        m_error.code = m_ndb.theError.code;
        delete t2;
        DBUG_RETURN(-1);
      }
    }
  }

  // blob tables - use "t2" to get values set by kernel
  if (t.m_noOfBlobs != 0) {

    // fix up disk data in t2 columns
    Uint32 i;
    for (i = 0; i < t.m_columns.size(); i++) {
      const NdbColumnImpl* c = t.m_columns[i];
      NdbColumnImpl* c2 = t2->m_columns[i];
      if (c->getBlobType()) {
        // type was mangled before sending to DICT
        assert(c2->m_storageType == NDB_STORAGETYPE_MEMORY);
        c2->m_storageType = c->m_storageType;
      }
    }

    if (createBlobTables(*t2) != 0) {
      int save_code = m_error.code;
      (void)dropTableGlobal(*t2);
      m_error.code = save_code;
      delete t2;
      DBUG_RETURN(-1);
    }
  }

  // not entered in cache
  delete t2;
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::optimizeTable(const NdbTableImpl &t,
                                 NdbOptimizeTableHandleImpl &h)
{
  DBUG_ENTER("NdbDictionaryImpl::optimizeTableGlobal(const NdbTableImpl)");
  DBUG_RETURN(h.init(&m_ndb, t));
}

int
NdbDictionaryImpl::optimizeIndex(const NdbIndexImpl &index,
                                 NdbOptimizeIndexHandleImpl &h)
{
  DBUG_ENTER("NdbDictionaryImpl::optimizeIndexGlobal(const NdbIndexImpl)");
  DBUG_RETURN(h.init(&m_ndb, index));
}

int
NdbDictionaryImpl::createBlobTables(const NdbTableImpl& t)
{
  DBUG_ENTER("NdbDictionaryImpl::createBlobTables");
  for (unsigned i = 0; i < t.m_columns.size(); i++) {
    const NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    DBUG_PRINT("info", ("col: %s array type: %u storage type: %u",
                        c.m_name.c_str(), c.m_arrayType, c.m_storageType));
    NdbTableImpl bt;
    NdbError error;
    if (NdbBlob::getBlobTable(bt, &t, &c, error) == -1) {
      m_error.code = error.code;
      DBUG_RETURN(-1);
    }
    NdbDictionary::Column::StorageType 
      d = NdbDictionary::Column::StorageTypeDisk;
    if (t.m_columns[i]->getStorageType() == d) {
      const char* colName = c.m_blobVersion == 1 ? "DATA" : "NDB$DATA";
      NdbColumnImpl* bc = bt.getColumn(colName);
      assert(bc != NULL);
      bc->setStorageType(d);
    }
    NdbDictionary::ObjectId objId; // ignore objid
    if (createTable(bt, NdbDictObjectImpl::getImpl(objId)) != 0) {
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0); 
}

int 
NdbDictInterface::createTable(Ndb & ndb,
			      NdbTableImpl & impl)
{
  int ret;

  DBUG_ENTER("NdbDictInterface::createTable");

  if (impl.m_fragmentType == NdbDictionary::Object::HashMapPartition)
  {
    if (impl.m_hash_map_id == RNIL && impl.m_hash_map_version == ~(Uint32)0)
    {
      /**
       * Make sure that hashmap exists (i.e after upgrade or similar)
       */
      Uint32 partitionBalance_Count = impl.getPartitionBalance();
      int req_type = CreateHashMapReq::CreateDefault |
                     CreateHashMapReq::CreateIfNotExists;
      if (!impl.getFullyReplicated())
      {
        if (partitionBalance_Count == NDB_PARTITION_BALANCE_SPECIFIC)
        {
          // For non fully replicated table partition count is fragment count.
          partitionBalance_Count = impl.getFragmentCount();
        }
      }
      else
      {
        if (partitionBalance_Count == NDB_PARTITION_BALANCE_SPECIFIC)
        {
          m_error.code = 797; // WrongPartitionBalanceFullyReplicated
          DBUG_RETURN(-1);
        }
        req_type |= CreateHashMapReq::CreateForOneNodegroup;
      }
      assert(partitionBalance_Count != 0);
      DBUG_PRINT("info", ("PartitionBalance: create_hashmap: %x",
                          partitionBalance_Count));
      NdbHashMapImpl hashmap;
      ret = create_hashmap(hashmap,
                           &hashmap,
                           req_type,
                           partitionBalance_Count);
      if (ret)
      {
        DBUG_RETURN(ret);
      }
      impl.m_hash_map_id = hashmap.m_id;
      impl.m_hash_map_version = hashmap.m_version;
    }
  }
  else
  {
    DBUG_PRINT("info", ("Hashmap already defined"));
  }

  syncInternalName(ndb, impl);

  UtilBufferWriter w(m_buffer);
  ret= serializeTableDesc(ndb, impl, w);
  if(ret != 0)
  {
    DBUG_RETURN(ret);
  }

  DBUG_RETURN(sendCreateTable(impl, w));
}

bool NdbDictionaryImpl::supportedAlterTable(NdbTableImpl &old_impl,
					    NdbTableImpl &impl)
{
  return m_receiver.supportedAlterTable(old_impl, impl);
}

bool NdbDictInterface::supportedAlterTable(const NdbTableImpl &old_impl,
					   NdbTableImpl &impl)
{
  Uint32 change_mask;
  return (compChangeMask(old_impl, impl, change_mask) == 0);
}

int NdbDictionaryImpl::alterTable(NdbTableImpl &old_impl,
                                  NdbTableImpl &impl)
{
  return alterTableGlobal(old_impl, impl);
}

int NdbDictionaryImpl::alterTableGlobal(NdbTableImpl &old_impl,
                                        NdbTableImpl &impl)
{
  DBUG_ENTER("NdbDictionaryImpl::alterTableGlobal");
  // Alter the table
  Uint32 changeMask = 0;
  int ret = m_receiver.alterTable(m_ndb, old_impl, impl, changeMask);
  if(ret == 0){
    NdbDictInterface::Tx::Op op;
    op.m_gsn = GSN_ALTER_TABLE_REQ;
    op.m_impl = &old_impl;
    if (m_tx.m_op.push_back(op) == -1) {
      m_error.code = 4000;
      DBUG_RETURN(-1);
    }
    m_globalHash->lock();
    ret = m_globalHash->inc_ref_count(op.m_impl);
    m_globalHash->unlock();
    if (ret != 0)
      m_error.code = 723;

    if (ret == 0)
    {
      if (alterBlobTables(old_impl, impl, changeMask) != 0)
      {
        DBUG_RETURN(-1);
      }
    }
    DBUG_RETURN(ret);
  }
  ERR_RETURN(getNdbError(), ret);
}

int
NdbDictionaryImpl::alterBlobTables(const NdbTableImpl & old_tab,
                                   const NdbTableImpl & new_tab,
                                   Uint32 tabChangeMask)
{
  DBUG_ENTER("NdbDictionaryImpl::alterBlobTables");
  if (old_tab.m_noOfBlobs == 0)
    DBUG_RETURN(0);

  char db[MAX_TAB_NAME_SIZE];
  char schema[MAX_TAB_NAME_SIZE];
  new_tab.getDbName(db, sizeof(db));
  new_tab.getSchemaName(schema, sizeof(schema));

  bool name_change = false;
  if (AlterTableReq::getNameFlag(tabChangeMask))
  {
    char old_db[MAX_TAB_NAME_SIZE];
    char old_schema[MAX_TAB_NAME_SIZE];
    if (old_tab.getDbName(old_db, sizeof(old_db)) != 0)
    {
      m_error.code = 705;
      DBUG_RETURN(-1);
    }
    if (old_tab.getSchemaName(old_schema, sizeof(old_schema)) != 0)
    {
      m_error.code = 705;
      DBUG_RETURN(-1);
    }
    bool db_change = strcmp(old_db, db) != 0;
    bool schema_change = strcmp(old_schema, schema) != 0;
    name_change = db_change || schema_change;
   }

  bool tab_frag_change = AlterTableReq::getAddFragFlag(tabChangeMask) != 0;

  for (unsigned i = 0; i < old_tab.m_columns.size(); i++)
  {
    NdbColumnImpl & c = *old_tab.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    NdbTableImpl* _bt = c.m_blobTable;
    if (_bt == NULL)
    {
      continue; // "force" mode on
    }

    NdbDictionary::Table& bt = * _bt->m_facade;
    NdbDictionary::Table new_bt(bt);

    if (name_change)
    {
      new_bt.m_impl.setDbSchema(db, schema);
    }

    bool frag_change = false;
    if (tab_frag_change)
    {
      frag_change =
        new_bt.getFragmentType() == old_tab.getFragmentType() &&
        new_bt.getFragmentCount() == old_tab.getFragmentCount() &&
        new_bt.getFragmentCount() != new_tab.getFragmentCount();
    }
    if (!frag_change)
    {
      if (new_bt.getPartitionBalance() == old_tab.getPartitionBalance() &&
          new_bt.getPartitionBalance() != new_tab.getPartitionBalance())
      {
        frag_change = true;
      }
    }
    if (frag_change)
    {
      new_bt.setPartitionBalance(new_tab.getPartitionBalance());
      new_bt.setFragmentType(new_tab.getFragmentType());
      new_bt.setDefaultNoPartitionsFlag(new_tab.getDefaultNoPartitionsFlag());
      new_bt.setFragmentCount(new_tab.getFragmentCount());
      new_bt.setFragmentData(new_tab.getFragmentData(), new_tab.getFragmentDataLen());
      NdbDictionary::HashMap hm;
      if (getHashMap(hm, &new_tab) != -1)
      {
        new_bt.setHashMap(hm);
      }
    }

    bool read_backup_change = false;
    if (new_tab.getReadBackupFlag() != old_tab.getReadBackupFlag())
    {
      read_backup_change = true;
      if (new_tab.getReadBackupFlag())
      {
        new_bt.setReadBackupFlag(true);
      }
      else
      {
        new_bt.setReadBackupFlag(false);
      }
    }

    Uint32 changeMask = 0;
    if (name_change || frag_change || read_backup_change)
    {
      int ret = m_receiver.alterTable(m_ndb, bt.m_impl, new_bt.m_impl, changeMask);
      if (ret != 0)
      {
        DBUG_RETURN(ret);
      }
      assert(!name_change || AlterTableReq::getNameFlag(changeMask));
      assert(!frag_change || AlterTableReq::getAddFragFlag(changeMask));
      assert(!read_backup_change || AlterTableReq::getReadBackupFlag(changeMask));
    }
  }
  DBUG_RETURN(0);
}

int
NdbDictInterface::alterTable(Ndb & ndb,
                             const NdbTableImpl &old_impl,
                             NdbTableImpl &impl,
                             Uint32 & change_mask)
{
  int ret;

  DBUG_ENTER("NdbDictInterface::alterTable");

  syncInternalName(ndb, impl);

  /* Check that alter request is valid and compute stuff to alter. */
  ret= compChangeMask(old_impl, impl, change_mask);
  if(ret != 0)
    DBUG_RETURN(ret);

  UtilBufferWriter w(m_buffer);
  ret= serializeTableDesc(ndb, impl, w);
  if(ret != 0)
    DBUG_RETURN(ret);

  DBUG_RETURN(sendAlterTable(impl, change_mask, w));
}

void
NdbDictInterface::syncInternalName(Ndb & ndb, NdbTableImpl &impl)
{
  const BaseString internalName(
    ndb.internalize_table_name(impl.m_externalName.c_str()));
  impl.m_internalName.assign(internalName);
  impl.updateMysqlName();
}

/*
  Compare old and new Table descriptors.
  Set the corresponding flag for any (supported) difference.
  Error on any difference not supported for alter table.
*/
int
NdbDictInterface::compChangeMask(const NdbTableImpl &old_impl,
                                 const NdbTableImpl &impl,
                                 Uint32 &change_mask)
{
  DBUG_ENTER("compChangeMask");
  bool found_varpart;
  change_mask= 0;
  Uint32 old_sz= old_impl.m_columns.size();
  Uint32 sz= impl.m_columns.size();

  /* These are the supported properties that may be altered. */
  DBUG_PRINT("info", ("old_impl.m_internalName='%s' impl.m_internalName='%s'",
                      old_impl.m_internalName.c_str(),
                      impl.m_internalName.c_str()));
  if(impl.m_internalName != old_impl.m_internalName)
  {
    bool old_blob = is_ndb_blob_table(old_impl.m_externalName.c_str());
    bool new_blob = is_ndb_blob_table(impl.m_externalName.c_str());
    if (unlikely(old_blob != new_blob))
    {
      /* Attempt to alter to/from Blob part table name */
      DBUG_PRINT("info", ("Attempt to alter to/from Blob part table name"));
      goto invalid_alter_table;
    }
    AlterTableReq::setNameFlag(change_mask, true);
  }
  if(!impl.m_frm.equal(old_impl.m_frm))
    AlterTableReq::setFrmFlag(change_mask, true);
  if(!impl.m_fd.equal(old_impl.m_fd))
    AlterTableReq::setFragDataFlag(change_mask, true);
  if(!impl.m_range.equal(old_impl.m_range))
    AlterTableReq::setRangeListFlag(change_mask, true);

  /* No other property can be changed in alter table. */
  if(impl.m_logging != old_impl.m_logging ||
     impl.m_temporary != old_impl.m_temporary ||
     impl.m_row_gci != old_impl.m_row_gci ||
     impl.m_row_checksum != old_impl.m_row_checksum ||
     impl.m_kvalue != old_impl.m_kvalue ||
     impl.m_minLoadFactor != old_impl.m_minLoadFactor ||
     impl.m_maxLoadFactor != old_impl.m_maxLoadFactor ||
     impl.m_primaryTableId != old_impl.m_primaryTableId ||
     impl.m_max_rows != old_impl.m_max_rows ||
     impl.m_min_rows != old_impl.m_min_rows ||
     impl.m_default_no_part_flag != old_impl.m_default_no_part_flag ||
     impl.m_linear_flag != old_impl.m_linear_flag ||
     impl.m_fragmentType != old_impl.m_fragmentType ||
     impl.m_tablespace_name != old_impl.m_tablespace_name ||
     impl.m_tablespace_id != old_impl.m_tablespace_id ||
     impl.m_tablespace_version != old_impl.m_tablespace_version ||
     impl.m_id != old_impl.m_id ||
     impl.m_version != old_impl.m_version ||
     sz < old_sz ||
     impl.m_extra_row_gci_bits != old_impl.m_extra_row_gci_bits ||
     impl.m_extra_row_author_bits != old_impl.m_extra_row_author_bits ||
     impl.m_fully_replicated != old_impl.m_fully_replicated)

  {
    DBUG_PRINT("info", ("Old and new table not compatible"));
    goto invalid_alter_table;
  }

  /**
   * PartitionBalance can change with alter table if it increases the
   * the number of fragments or the number stays the same. Changing to
   * a smaller number of fragments does however not work as this
   * requires drop partition to work.
   */

  if (impl.m_partitionBalance != old_impl.m_partitionBalance)
  {
    bool ok;
    if (old_impl.m_fully_replicated)
    {
      /**
       * Currently do not support changing partition balance of
       * fully replicated tables.
       */
      ok = false;
    }
    else if (old_impl.m_partitionBalance ==
               NdbDictionary::Object::PartitionBalance_Specific)
    {
      ok = false;
    }
    else if (impl.m_partitionBalance ==
               NdbDictionary::Object::PartitionBalance_Specific)
    {
      ok = true;
    }
    else if (old_impl.m_partitionBalance ==
               NdbDictionary::Object::PartitionBalance_ForRAByNode)
    {
      ok = true;
    }
    else if (old_impl.m_partitionBalance ==
               NdbDictionary::Object::PartitionBalance_ForRPByNode)
    {
      if (impl.m_partitionBalance !=
            NdbDictionary::Object::PartitionBalance_ForRAByNode)
      {
        ok = true;
      }
      else
      {
        ok = false;
      }
    }
    else if (old_impl.m_partitionBalance ==
             NdbDictionary::Object::PartitionBalance_ForRAByLDM)
    {
      if (impl.m_partitionBalance !=
            NdbDictionary::Object::PartitionBalance_ForRAByNode &&
          impl.m_partitionBalance !=
            NdbDictionary::Object::PartitionBalance_ForRPByNode)
      {
        ok = true;
      }
      else
      {
        ok = false;
      }
    }
    else
    {
      /**
       * Unknown partition balance
       */
      ok = false;
    }
    if (!ok)
    {
      goto invalid_alter_table;
    }
    AlterTableReq::setAddFragFlag(change_mask, true);
    AlterTableReq::setPartitionBalanceFlag(change_mask, true);
  }
  if (impl.m_fragmentCount != old_impl.m_fragmentCount)
  {
    if (impl.m_fragmentType != NdbDictionary::Object::HashMapPartition)
      goto invalid_alter_table;
    AlterTableReq::setAddFragFlag(change_mask, true);
  }
  else if (AlterTableReq::getPartitionBalanceFlag(change_mask))
  {
    ; // Already handled above
  }
  else
  { // Changing hash map only supported if adding fragments
    if (impl.m_fragmentType == NdbDictionary::Object::HashMapPartition &&
        (impl.m_hash_map_id != old_impl.m_hash_map_id ||
         impl.m_hash_map_version != old_impl.m_hash_map_version))
    {
      goto invalid_alter_table;
    }
  }
  if (impl.m_read_backup != old_impl.m_read_backup)
  {
    /* Change the read backup flag inplace */
    DBUG_PRINT("info", ("Set Change ReadBackup Flag, old: %u, new: %u",
                       old_impl.m_read_backup,
                       impl.m_read_backup));
    AlterTableReq::setReadBackupFlag(change_mask, true);
  }
  else
  {
    DBUG_PRINT("info", ("No ReadBackup change, val: %u",
                        impl.m_read_backup));
  }

  /*
    Check for new columns.
    We can add one or more new columns at the end, with some restrictions:
     - All existing columns must be unchanged.
     - The new column must be dynamic.
     - The new column must be nullable.
     - The new column must be memory based.
     - The new column can not be a primary key or distribution key.
     - There must already be at least one existing memory-stored dynamic or
       variable-sized column (so that the varpart is already allocated) or
       varPart must be forced
  */
  found_varpart= old_impl.getForceVarPart();
  for(Uint32 i= 0; i<old_sz; i++)
  {
    const NdbColumnImpl *col= impl.m_columns[i];
    if(!col->equal(*(old_impl.m_columns[i])))
    {
      DBUG_PRINT("info", ("Old and new column not equal"));
      goto invalid_alter_table;
    }
    if(col->m_storageType == NDB_STORAGETYPE_MEMORY &&
       (col->m_dynamic || col->m_arrayType != NDB_ARRAYTYPE_FIXED))
      found_varpart= true;
  }

  if(sz > old_sz)
  {
    if(!found_varpart)
    {
      DBUG_PRINT("info", ("No old dynamic column found"));
      goto invalid_alter_table;
    }

    for(Uint32 i=old_sz; i<sz; i++)
    {
      const NdbColumnImpl *col= impl.m_columns[i];
      if(!col->m_dynamic || !col->m_nullable ||
         !col->m_defaultValue.empty() ||
         col->m_storageType == NDB_STORAGETYPE_DISK ||
         col->m_pk ||
         col->m_distributionKey ||
         col->m_autoIncrement ||                   // ToDo: allow this?
	 (col->getBlobType() && col->getPartSize())
         )
      {
        goto invalid_alter_table;
      }
    }
    AlterTableReq::setAddAttrFlag(change_mask, true);
  }

  DBUG_RETURN(0);

 invalid_alter_table:
  m_error.code = 741;                           // "Unsupported alter table"
  DBUG_RETURN(-1);
}

int 
NdbDictInterface::serializeTableDesc(Ndb & ndb,
				     NdbTableImpl & impl,
				     UtilBufferWriter & w)
{
  unsigned i, err;
  DBUG_ENTER("NdbDictInterface::serializeTableDesc");

  impl.computeAggregates();

  if((unsigned)impl.getNoOfPrimaryKeys() > NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY){
    m_error.code= 4317;
    DBUG_RETURN(-1);
  }
  unsigned sz = impl.m_columns.size();
  if (sz > NDB_MAX_ATTRIBUTES_IN_TABLE){
    m_error.code= 4318;
    DBUG_RETURN(-1);
  }
  
  /*
     TODO RONM: Here I need to insert checks for fragment array and
     range or list array
  */
  
  //validate();
  //aggregate();

  DictTabInfo::Table *tmpTab = (DictTabInfo::Table*)malloc(sizeof(DictTabInfo::Table));
  if (!tmpTab)
  {
    m_error.code = 4000;
    DBUG_RETURN(-1);
  }
  tmpTab->init();
  BaseString::snprintf(tmpTab->TableName, sizeof(tmpTab->TableName),
                       "%s", impl.m_internalName.c_str());

  Uint32 distKeys= 0;
  for(i = 0; i<sz; i++) {
    const NdbColumnImpl * col = impl.m_columns[i];
    if (col == NULL) {
      m_error.code = 4272;
      free(tmpTab);
      DBUG_RETURN(-1);
    }
    if (col->m_distributionKey)
    {
      distKeys++;
      if (!col->m_pk)
      {
        m_error.code = 4327;
        free(tmpTab);
        DBUG_RETURN(-1);
      }
    }
  }
  if (distKeys == impl.m_noOfKeys)
    distKeys= 0;
  impl.m_noOfDistributionKeys= distKeys;


  // Check max length of frm data
  if (impl.m_frm.length() > MAX_FRM_DATA_SIZE){
    m_error.code= 1229;
    free(tmpTab);
    DBUG_RETURN(-1);
  }
  /*
    TODO RONM: This needs to change to dynamic arrays instead
    Frm Data, FragmentData, TablespaceData, RangeListData, TsNameData
  */
  tmpTab->FrmLen = impl.m_frm.length();
  memcpy(tmpTab->FrmData, impl.m_frm.get_data(), impl.m_frm.length());

  {
    /**
     * NOTE: fragment data is currently an array of Uint16
     *       and len is specified in bytes (yuck)
     *       please change to Uint32 and len == count
     */
    const Uint32* src = impl.m_fd.getBase();
    tmpTab->FragmentDataLen = 2*impl.m_fd.size();
    for (Uint32 i = 0; i<impl.m_fd.size(); i++)
      tmpTab->FragmentData[i] = (Uint16)src[i];
  }

  {
    /**
     * NOTE: len is specified in bytes (yuck)
     *       please change to len == count
     */
    tmpTab->RangeListDataLen = 4*impl.m_range.size();
    memcpy(tmpTab->RangeListData, impl.m_range.getBase(),4*impl.m_range.size());
  }

  tmpTab->PartitionBalance = (Uint32)impl.m_partitionBalance;
  tmpTab->FragmentCount= impl.m_fragmentCount;
  tmpTab->PartitionCount = impl.m_partitionCount;
  tmpTab->TableLoggedFlag = impl.m_logging;
  tmpTab->TableTemporaryFlag = impl.m_temporary;
  tmpTab->RowGCIFlag = impl.m_row_gci;
  tmpTab->RowChecksumFlag = impl.m_row_checksum;
  tmpTab->TableKValue = impl.m_kvalue;
  tmpTab->MinLoadFactor = impl.m_minLoadFactor;
  tmpTab->MaxLoadFactor = impl.m_maxLoadFactor;
  tmpTab->TableType = DictTabInfo::UserTable;
  tmpTab->PrimaryTableId = impl.m_primaryTableId;
  tmpTab->NoOfAttributes = sz;
  tmpTab->MaxRowsHigh = (Uint32)(impl.m_max_rows >> 32);
  tmpTab->MaxRowsLow = (Uint32)(impl.m_max_rows & 0xFFFFFFFF);
  tmpTab->MinRowsHigh = (Uint32)(impl.m_min_rows >> 32);
  tmpTab->MinRowsLow = (Uint32)(impl.m_min_rows & 0xFFFFFFFF);
  tmpTab->DefaultNoPartFlag = impl.m_default_no_part_flag;
  tmpTab->LinearHashFlag = impl.m_linear_flag;
  tmpTab->SingleUserMode = impl.m_single_user_mode;
  tmpTab->ForceVarPartFlag = impl.m_force_var_part;
  tmpTab->ExtraRowGCIBits = impl.m_extra_row_gci_bits;
  tmpTab->ExtraRowAuthorBits = impl.m_extra_row_author_bits;
  tmpTab->FullyReplicatedFlag = !!impl.m_fully_replicated;
  tmpTab->ReadBackupFlag = !!impl.m_read_backup;
  tmpTab->FragmentType = getKernelConstant(impl.m_fragmentType,
 					   fragmentTypeMapping,
					   DictTabInfo::AllNodesSmallTable);
  tmpTab->TableVersion = rand();

  tmpTab->HashMapObjectId = impl.m_hash_map_id;
  tmpTab->HashMapVersion = impl.m_hash_map_version;
  tmpTab->TableStorageType = impl.m_storageType;

  const char *tablespace_name= impl.m_tablespace_name.c_str();
loop:
  if(impl.m_tablespace_version != ~(Uint32)0)
  {
    tmpTab->TablespaceId = impl.m_tablespace_id;
    tmpTab->TablespaceVersion = impl.m_tablespace_version;
  }
  else if(strlen(tablespace_name))
  {
    NdbTablespaceImpl tmp;
    if(get_filegroup(tmp, NdbDictionary::Object::Tablespace, 
		     tablespace_name) == 0)
    {
      tmpTab->TablespaceId = tmp.m_id;
      tmpTab->TablespaceVersion = tmp.m_version;
    }
    else 
    {
      // error set by get filegroup
      if (m_error.code == 723)
	m_error.code = 755;
      
      free(tmpTab);
      DBUG_RETURN(-1);
    }
  } 
  else
  {
    for(i = 0; i<sz; i++)
    {
      if(impl.m_columns[i]->m_storageType == NDB_STORAGETYPE_DISK)
      {
	tablespace_name = "DEFAULT-TS";
	goto loop;
      }
    }
  }
  
  SimpleProperties::UnpackStatus s;
  w.reset();
  s = SimpleProperties::pack(w, 
			     tmpTab,
			     DictTabInfo::TableMapping, 
			     DictTabInfo::TableMappingSize, true);
  
  if(s != SimpleProperties::Eof){
    abort();
  }
  free(tmpTab);
  
  DBUG_PRINT("info",("impl.m_noOfDistributionKeys: %d impl.m_noOfKeys: %d distKeys: %d",
		     impl.m_noOfDistributionKeys, impl.m_noOfKeys, distKeys));
  if (distKeys == impl.m_noOfKeys)
    distKeys= 0;
  impl.m_noOfDistributionKeys= distKeys;
  
  for(i = 0; i<sz; i++){
    const NdbColumnImpl * col = impl.m_columns[i];
    if(col == 0)
      continue;
    
    DBUG_PRINT("info",("column: %s(%d) col->m_distributionKey: %d"
                       " array type: %u storage type: %u",
		       col->m_name.c_str(), i, col->m_distributionKey,
                       col->m_arrayType, col->m_storageType));
    DictTabInfo::Attribute tmpAttr; tmpAttr.init();
    BaseString::snprintf(tmpAttr.AttributeName, sizeof(tmpAttr.AttributeName), 
	     "%s", col->m_name.c_str());
    tmpAttr.AttributeId = col->m_attrId;
    tmpAttr.AttributeKeyFlag = col->m_pk;
    tmpAttr.AttributeNullableFlag = col->m_nullable;
    tmpAttr.AttributeDKey = distKeys ? col->m_distributionKey : 0;

    tmpAttr.AttributeExtType = (Uint32)col->m_type;
    tmpAttr.AttributeExtPrecision = ((unsigned)col->m_precision & 0xFFFF);
    tmpAttr.AttributeExtScale = col->m_scale;
    tmpAttr.AttributeExtLength = col->m_length;
    tmpAttr.AttributeArrayType = col->m_arrayType;

    if(col->m_pk)
      tmpAttr.AttributeStorageType = NDB_STORAGETYPE_MEMORY;      
    else
      tmpAttr.AttributeStorageType = col->m_storageType;
    tmpAttr.AttributeDynamic = (col->m_dynamic ? 1 : 0);

    if (col->getBlobType()) {
      tmpAttr.AttributeArrayType = col->m_arrayType;
      tmpAttr.AttributeStorageType = NDB_STORAGETYPE_MEMORY;      
    }
    
    // check type and compute attribute size and array size
    if (! tmpAttr.translateExtType()) {
      m_error.code= 703;
      DBUG_RETURN(-1);
    }
    // charset is defined exactly for char types
    if (col->getCharType() != (col->m_cs != NULL)) {
      m_error.code= 703;
      DBUG_RETURN(-1);
    }
    // primary key type check
    if (col->m_pk && 
        (err = NdbSqlUtil::check_column_for_pk(col->m_type, col->m_cs)))
    {
      m_error.code= err;
      DBUG_RETURN(-1);
    }
    // all PK types now allowed as dist key
    // charset in upper half of precision
    if (col->getCharType()) {
      tmpAttr.AttributeExtPrecision |= (col->m_cs->number << 16);
    }

    tmpAttr.AttributeAutoIncrement = col->m_autoIncrement;
    {
      Uint32 ah;
      Uint32 byteSize = col->m_defaultValue.length();
      assert(byteSize <= NDB_MAX_TUPLE_SIZE);

      if (byteSize)
      {
        if (unlikely(! ndb_native_default_support(ndb.getMinDbNodeVersion())))
        {
          /* We can't create a table with native defaults with
           * this kernel version
           * Schema feature requires data node upgrade
           */
          m_error.code = 794;
          DBUG_RETURN(-1);
        }
      }   

      //The AttributeId of a column isn't decided now, so 0 is used.
      AttributeHeader::init(&ah, 0, byteSize);

      /* Table meta-info is normally stored in network byte order
       * by SimpleProperties
       * For the default value 'Blob' we do the work
       */
      Uint32 a = htonl(ah);
      memcpy(tmpAttr.AttributeDefaultValue, &a, sizeof(Uint32));
      memcpy(tmpAttr.AttributeDefaultValue + sizeof(Uint32), 
             col->m_defaultValue.get_data(), byteSize);
      Uint32 defValByteLen = ((col->m_defaultValue.length() + 3) / 4) * 4;
      tmpAttr.AttributeDefaultValueLen = defValByteLen + sizeof(Uint32);

      if (defValByteLen)
      {
        /* In-place host->network conversion */
        NdbSqlUtil::convertByteOrder(tmpAttr.AttributeExtType,
                                     tmpAttr.AttributeSize,
                                     tmpAttr.AttributeArrayType,
                                     tmpAttr.AttributeArraySize,
                                     tmpAttr.AttributeDefaultValue + 
                                     sizeof(Uint32),
                                     defValByteLen);
      }
    }
    s = SimpleProperties::pack(w, 
			       &tmpAttr,
			       DictTabInfo::AttributeMapping, 
			       DictTabInfo::AttributeMappingSize, true);
    w.add(DictTabInfo::AttributeEnd, 1);
  }

  DBUG_RETURN(0);
}

int
NdbDictInterface::sendAlterTable(const NdbTableImpl &impl,
                                 Uint32 change_mask,
                                 UtilBufferWriter &w)
{
  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_ALTER_TABLE_REQ;
  tSignal.theLength = AlterTableReq::SignalLength;

  AlterTableReq * req = CAST_PTR(AlterTableReq, tSignal.getDataPtrSend());

  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->tableId = impl.m_id;
  req->tableVersion = impl.m_version;
  req->changeMask = change_mask;
  DBUG_PRINT("info", ("sendAlterTable: changeMask: %x", change_mask));

  int errCodes[] = { AlterTableRef::NotMaster, AlterTableRef::Busy, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_ALTER_TAB_REQ"
                        " in NdbDictInterface::sendAlterTable"));
    timeout = 1000;
  });

  int ret= dictSignal(&tSignal, ptr, 1,
                      0,                        // master
                      WAIT_ALTER_TAB_REQ,
                      timeout, 100,
                      errCodes);

  if(m_error.code == AlterTableRef::InvalidTableVersion) {
    // Clear caches and try again
    return(INCOMPATIBLE_VERSION);
  }

  return ret;
}

int
NdbDictInterface::sendCreateTable(const NdbTableImpl &impl,
                                  UtilBufferWriter &w)
{
  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_CREATE_TABLE_REQ;
  tSignal.theLength = CreateTableReq::SignalLength;

  CreateTableReq * req = CAST_PTR(CreateTableReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();

  int errCodes[]= { CreateTableRef::Busy, CreateTableRef::NotMaster, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_CREATE_TABLE_REQ"
                        " in NdbDictInterface::sendCreateTable"));
    timeout = 1000;
  });

  int ret= dictSignal(&tSignal, ptr, 1,
                      0,                        // master node
                      WAIT_CREATE_INDX_REQ,
                      timeout, 100,
                      errCodes);

  return ret;
}

void
NdbDictInterface::execCREATE_TABLE_CONF(const NdbApiSignal * signal,
					const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_TABLE_CONF");
  const CreateTableConf* const conf=
    CAST_CONSTPTR(CreateTableConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "CREATE_TABLE_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->tableId;
  data[1] = conf->tableVersion;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_TABLE_REF(const NdbApiSignal * sig,
				       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_TABLE_REF");
  const CreateTableRef* ref = CAST_CONSTPTR(CreateTableRef, sig->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "CREATE_TABLE_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code= ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execALTER_TABLE_CONF(const NdbApiSignal * signal,
                                       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execALTER_TABLE_CONF");
  const AlterTableConf * conf = CAST_CONSTPTR(AlterTableConf,
                                              signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "ALTER_TABLE_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execALTER_TABLE_REF(const NdbApiSignal * sig,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execALTER_TABLE_REF");
  const AlterTableRef * ref = CAST_CONSTPTR(AlterTableRef, sig->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "ALTER_TABLE_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code= ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Drop table
 */
int
NdbDictionaryImpl::dropTable(const char * name)
{
  DBUG_ENTER("NdbDictionaryImpl::dropTable");
  DBUG_PRINT("enter",("name: %s", name));
  ASSERT_NOT_MYSQLD;
  NdbTableImpl * tab = getTable(name);
  if(tab == 0){
    DBUG_RETURN(-1);
  }
  int ret = dropTable(* tab);
  // If table stored in cache is incompatible with the one in the kernel
  // we must clear the cache and try again
  if (ret == INCOMPATIBLE_VERSION) {
    const BaseString internalTableName(m_ndb.internalize_table_name(name));
    DBUG_PRINT("info",("INCOMPATIBLE_VERSION internal_name: %s", internalTableName.c_str()));
    m_localHash.drop(internalTableName.c_str());
    m_globalHash->lock();
    m_globalHash->release(tab, 1);
    m_globalHash->unlock();
    DBUG_RETURN(dropTable(name));
  }
  
  DBUG_RETURN(ret);
}

static bool
dropTableAllowDropChildFK(const NdbTableImpl& impl,
                          const NdbDictionary::ForeignKey& fk,
                          int flags)
{
  DBUG_ENTER("dropTableAllowDropChildFK");
  const char* table = impl.m_internalName.c_str();
  const char* child = fk.getChildTable();
  const char* parent = fk.getParentTable();
  DBUG_PRINT("info", ("table: %s child: %s parent: %s",
                      table, child, parent));
  const bool is_child = strcmp(table, child) == 0;
  const bool is_parent = strcmp(table, parent) == 0;
  if (flags & NdbDictionary::Dictionary::DropTableCascadeConstraints)
  {
    DBUG_PRINT("info", ("return true - cascade_constraints is on"));
    DBUG_RETURN(true);
  }
  if (is_child && !is_parent)
  {
    DBUG_PRINT("info", ("return true - !is_parent && is_child"));
    DBUG_RETURN(true);
  }
  if (is_child && is_parent)
  {
    // same table (self ref FK)
    DBUG_PRINT("info", ("return true - is_child && is_parent"));
    DBUG_RETURN(true);
  }
  if (flags & NdbDictionary::Dictionary::DropTableCascadeConstraintsDropDB)
  {
    // first part is db...
    const char * end = strchr(parent, table_name_separator);
    if (end != NULL)
    {
      size_t len = end - parent;
      if (strncmp(parent, child, len) == 0)
      {
        DBUG_PRINT("info",
                   ("return OK - DropTableCascadeConstraintsDropDB & same DB"));
        DBUG_RETURN(true);
      }
    }
  }

  DBUG_PRINT("info", ("return false"));
  DBUG_RETURN(false);
}

int
NdbDictionaryImpl::dropTable(NdbTableImpl & impl)
{
  int res;
  const char * name = impl.getName();
  if(impl.m_status == NdbDictionary::Object::New){
    return dropTable(name);
  }

  if (impl.m_indexType != NdbDictionary::Object::TypeUndefined)
  {
    m_receiver.m_error.code= 1228;
    return -1;
  }

  List list;
  if ((res = listDependentObjects(list, impl.m_id)) == -1){
    return -1;
  }

  // drop FKs before indexes (even if DBDICT may not care)

  for (unsigned i = 0; i < list.count; i++) {
    const List::Element& element = list.elements[i];
    if (DictTabInfo::isForeignKey(element.type))
    {
      NdbDictionary::ForeignKey fk;
      if ((res = getForeignKey(fk, element.name)) != 0)
      {
        return -1;
      }
      const bool cascade_constraints = true;
      if (!dropTableAllowDropChildFK(impl, fk, cascade_constraints))
      {
        m_receiver.m_error.code = 21080;
        /* Save the violated FK id in error.details
         * To provide additional context of the failure */
        m_receiver.m_error.details = (char *)UintPtr(fk.getObjectId());
        return -1;
      }
      if ((res = dropForeignKey(fk)) != 0)
      {
        return -1;
      }
    }
  }

  for (unsigned i = 0; i < list.count; i++) {
    const List::Element& element = list.elements[i];
    if (DictTabInfo::isIndex(element.type))
    {
      // note can also return -2 in error case(INCOMPATIBLE_VERSION),
      // hence compare with != 0
      if ((res = dropIndex(element.name, name, true)) != 0)
      {
        return -1;
      }
    }
  }
  
  if (impl.m_noOfBlobs != 0) {
    if (dropBlobTables(impl) != 0){
      return -1;
    }
  }
  
  int ret = m_receiver.dropTable(impl);  
  if(ret == 0 || m_error.code == 709 || m_error.code == 723){
    const char * internalTableName = impl.m_internalName.c_str();

    
    m_localHash.drop(internalTableName);
    m_globalHash->lock();
    m_globalHash->release(&impl, 1);
    m_globalHash->unlock();

    return 0;
  }
  
  return ret;
}

int
NdbDictionaryImpl::dropTableGlobal(NdbTableImpl & impl)
{
  return dropTableGlobal(impl, 0);
}

int
NdbDictionaryImpl::dropTableGlobal(NdbTableImpl & impl, int flags)
{
  int res;
  DBUG_ENTER("NdbDictionaryImpl::dropTableGlobal");
  DBUG_ASSERT(impl.m_status != NdbDictionary::Object::New);
  DBUG_ASSERT(impl.m_indexType == NdbDictionary::Object::TypeUndefined);

  List list;
  if ((res = listDependentObjects(list, impl.m_id)) == -1){
    ERR_RETURN(getNdbError(), -1);
  }

  {
    /**
     * To keep this method atomic...
     *   we first iterate the list and perform checks...
     *   before doing any drops
     *
     * Otherwise, some drops might have been performed and then we return error
     *   the semantics is a bit unclear for this situation but new code
     *   trying to handle foreign_key_checks relies to this
     *   being possible
     */
    for (unsigned i = 0; i < list.count; i++)
    {
      const List::Element& element = list.elements[i];

      if (DictTabInfo::isForeignKey(element.type))
      {
        NdbDictionary::ForeignKey fk;
        if ((res = getForeignKey(fk, element.name)) != 0)
        {
          ERR_RETURN(getNdbError(), -1);
        }
        if (!dropTableAllowDropChildFK(impl, fk, flags))
        {
          m_receiver.m_error.code = 21080;
          /* Save the violated FK id in error.details
           * To provide additional context of the failure */
          m_receiver.m_error.details = (char *)UintPtr(fk.getObjectId());
          ERR_RETURN(getNdbError(), -1);
        }
      }
    }
  }

  /**
   * Need to drop all FK first...as they might depend on indexes
   * No need to call dropTableAllowDropChildFK again...
   */
  for (unsigned i = 0; i < list.count; i++)
  {
    const List::Element& element = list.elements[i];

    if (DictTabInfo::isForeignKey(element.type))
    {
      NdbDictionary::ForeignKey fk;
      if ((res = getForeignKey(fk, element.name)) != 0)
      {
        ERR_RETURN(getNdbError(), -1);
      }

      if ((res = dropForeignKey(fk)) != 0)
      {
        ERR_RETURN(getNdbError(), -1);
      }
    }
  }

  /**
   * And then drop the indexes
   */
  for (unsigned i = 0; i < list.count; i++)
  {
    const List::Element& element = list.elements[i];
    if (DictTabInfo::isIndex(element.type))
    {
      // note can also return -2 in error case(INCOMPATIBLE_VERSION),
      // hence compare with != 0
      NdbIndexImpl *idx= getIndexGlobal(element.name, impl);
      if (idx == NULL)
      {
        ERR_RETURN(getNdbError(), -1);
      }

      // note can also return -2 in error case(INCOMPATIBLE_VERSION),
      // hence compare with != 0
      if ((res = dropIndexGlobal(*idx, true)) != 0)
      {
        releaseIndexGlobal(*idx, 1);
        ERR_RETURN(getNdbError(), -1);
      }
      releaseIndexGlobal(*idx, 1);
    }
  }

  if (impl.m_noOfBlobs != 0) {
    if (dropBlobTables(impl) != 0){
      ERR_RETURN(getNdbError(), -1);
    }
  }
  
  int ret = m_receiver.dropTable(impl);  
  impl.m_status = NdbDictionary::Object::Invalid;
  if(ret == 0 || m_error.code == 709 || m_error.code == 723)
  {
    DBUG_RETURN(0);
  }
  
  ERR_RETURN(getNdbError(), ret);
}

int
NdbDictionaryImpl::dropBlobTables(NdbTableImpl & t)
{
  DBUG_ENTER("NdbDictionaryImpl::dropBlobTables");
  for (unsigned i = 0; i < t.m_columns.size(); i++) {
    NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    NdbTableImpl* bt = c.m_blobTable;
    if (bt == NULL) {
      DBUG_PRINT("info", ("col %s: blob table pointer is NULL",
                          c.m_name.c_str()));
      continue; // "force" mode on
    }
    // drop directly - by-pass cache
    int ret = m_receiver.dropTable(*c.m_blobTable);
    if (ret != 0) {
      DBUG_PRINT("info", ("col %s: blob table %s: error %d",
                 c.m_name.c_str(), bt->m_internalName.c_str(), m_error.code));
      if (! (ret == 709 || ret == 723)) // "force" mode on
        ERR_RETURN(getNdbError(), -1);
    }
    // leave c.m_blobTable defined
  }
  DBUG_RETURN(0);
}

int
NdbDictInterface::dropTable(const NdbTableImpl & impl)
{
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_DROP_TABLE_REQ;
  tSignal.theLength = DropTableReq::SignalLength;
  
  DropTableReq * req = CAST_PTR(DropTableReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->tableId = impl.m_id;
  req->tableVersion = impl.m_version;

  int errCodes[] =
    { DropTableRef::NoDropTableRecordAvailable,
      DropTableRef::NotMaster,
      DropTableRef::Busy, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_DROP_TAB_REQ"
                        " in NdbDictInterface::dropTable"));
    timeout = 1000;
  });

  int r = dictSignal(&tSignal, 0, 0,
		     0, // master
		     WAIT_DROP_TAB_REQ, 
		     timeout, 100,
		     errCodes);
  if(m_error.code == DropTableRef::InvalidTableVersion) {
    // Clear caches and try again
    return INCOMPATIBLE_VERSION;
  }
  return r;
}

void
NdbDictInterface::execDROP_TABLE_CONF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_TABLE_CONF");
  const DropTableConf* conf = CAST_CONSTPTR(DropTableConf,
                                            signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "DROP_TABLE_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_TABLE_REF(const NdbApiSignal * signal,
				     const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_TABLE_REF");
  const DropTableRef* ref = CAST_CONSTPTR(DropTableRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "DROP_TABLE_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code= ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

int
NdbDictionaryImpl::invalidateObject(NdbTableImpl & impl)
{
  const char * internalTableName = impl.m_internalName.c_str();
  DBUG_ENTER("NdbDictionaryImpl::invalidateObject");
  DBUG_PRINT("enter", ("internal_name: %s", internalTableName));

  m_localHash.drop(internalTableName);
  m_globalHash->lock();
  m_globalHash->release(&impl, 1);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::removeCachedObject(NdbTableImpl & impl)
{
  const char * internalTableName = impl.m_internalName.c_str();
  DBUG_ENTER("NdbDictionaryImpl::removeCachedObject");
  DBUG_PRINT("enter", ("internal_name: %s", internalTableName));

  m_localHash.drop(internalTableName);  
  m_globalHash->lock();
  m_globalHash->release(&impl);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

int
NdbDictInterface::create_index_obj_from_table(NdbIndexImpl** dst,
					      NdbTableImpl* tab,
					      const NdbTableImpl* prim)
{
  DBUG_ENTER("NdbDictInterface::create_index_obj_from_table");
  NdbIndexImpl *idx = new NdbIndexImpl();
  if (idx == NULL)
  {
    errno = ENOMEM;
    DBUG_RETURN(-1);
  }
  idx->m_version = tab->m_version;
  idx->m_status = tab->m_status;
  idx->m_id = tab->m_id;
  if (!idx->m_externalName.assign(tab->getName()) ||
      !idx->m_tableName.assign(prim->m_externalName))
  {
    delete idx;
    errno = ENOMEM;
    DBUG_RETURN(-1);
  }
  NdbDictionary::Object::Type type = idx->m_type = tab->m_indexType;
  idx->m_logging = tab->m_logging;
  idx->m_temporary = tab->m_temporary;

  const Uint32 distKeys = prim->m_noOfDistributionKeys;
  Uint32 keyCount =
    (type == NdbDictionary::Object::UniqueHashIndex) ?
    tab->m_noOfKeys : (distKeys ? distKeys : prim->m_noOfKeys);
  const Uint32 fullKeyCount = keyCount;

  unsigned i;
  // skip last attribute (NDB$PK or NDB$TNODE)
  for(i = 0; i+1<tab->m_columns.size(); i++){
    NdbColumnImpl* org = tab->m_columns[i];

    NdbColumnImpl* col = new NdbColumnImpl;
    if (col == NULL)
    {
      errno = ENOMEM;
      delete idx;
      DBUG_RETURN(-1);
    }
    // Copy column definition
    *col = * org;
    if (idx->m_columns.push_back(col))
    {
      delete col;
      delete idx;
      DBUG_RETURN(-1);
    }

    /**
     * reverse map
     */
    const NdbColumnImpl* primCol = prim->getColumn(col->getName());
    if (primCol == 0)
    {
      delete idx;
      DBUG_RETURN(-1);
    }

    int key_id = primCol->getColumnNo();
    int fill = -1;
    idx->m_key_ids.fill(key_id, fill);
    idx->m_key_ids[key_id] = i;
    col->m_keyInfoPos = key_id;

    if(type == NdbDictionary::Object::OrderedIndex && 
       (primCol->m_distributionKey ||
	(distKeys == 0 && primCol->getPrimaryKey())))
    {
      keyCount--;
      org->m_distributionKey = 1;
    }
    else if (type == NdbDictionary::Object::UniqueHashIndex)
    {
      keyCount--;
      org->m_distributionKey = 1;
    }
  }

  if(keyCount == 0)
  {
    tab->m_noOfDistributionKeys = fullKeyCount;
  }
  else 
  {
    for(i = 0; i+1<tab->m_columns.size(); i++)
      tab->m_columns[i]->m_distributionKey = 0;
  }

  idx->m_table_id = prim->getObjectId();
  idx->m_table_version = prim->getObjectVersion();
  
  * dst = idx;
  DBUG_PRINT("exit", ("m_id: %d  m_version: %d", idx->m_id, idx->m_version));
  DBUG_RETURN(0);
}

/*****************************************************************
 * Create index
 */
int
NdbDictionaryImpl::createIndex(NdbIndexImpl &ix, bool offline)
{
  ASSERT_NOT_MYSQLD;
  NdbTableImpl* tab = getTable(ix.getTable());
  if(tab == 0)
  {
    if(m_error.code == 0)
      m_error.code = 4249;
    return -1;
  }
  
  return m_receiver.createIndex(m_ndb, ix, * tab, offline);
}

int
NdbDictionaryImpl::createIndex(NdbIndexImpl &ix, NdbTableImpl &tab,
                               bool offline)
{
  return m_receiver.createIndex(m_ndb, ix, tab, offline);
}

int 
NdbDictInterface::createIndex(Ndb & ndb,
			      const NdbIndexImpl & impl, 
			      const NdbTableImpl & table,
                              bool offline)
{
  //validate();
  //aggregate();
  unsigned i, err;
  UtilBufferWriter w(m_buffer);
  const size_t len = strlen(impl.m_externalName.c_str()) + 1;
  if(len > MAX_TAB_NAME_SIZE) {
    m_error.code = 4241;
    return -1;
  }
  const BaseString internalName(
    ndb.internalize_index_name(&table, impl.getName()));
  w.add(DictTabInfo::TableName, internalName.c_str());
  w.add(DictTabInfo::TableLoggedFlag, impl.m_logging);
  w.add(DictTabInfo::TableTemporaryFlag, impl.m_temporary);

  /**
   * DICT ensures that the table gets the same partitioning
   * for unique indexes as the main table.
   */
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_CREATE_INDX_REQ;
  tSignal.theLength = CreateIndxReq::SignalLength;
  
  CreateIndxReq * const req = CAST_PTR(CreateIndxReq,
                                       tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = offline ? CreateIndxReq::RF_BUILD_OFFLINE : 0;
  req->requestInfo |= m_tx.requestFlags();

  Uint32 it = getKernelConstant(impl.m_type,
				indexTypeMapping,
				DictTabInfo::UndefTableType);
  
  if(it == DictTabInfo::UndefTableType){
    m_error.code = 4250;
    return -1;
  }

  if (it == DictTabInfo::UniqueHashIndex)
  {
    /**
     * We derive the Read backup flag and Fully replicated flag
     * from the main table. This is only done in the NDB API
     * here. This enables us to easily make this settable per
     * table by changing the NDB API. Setting it in data node
     * makes it harder to change to a more flexible manner in
     * the future if need arises.
     *
     * Ordered indexes are hardcoded in data nodes to always
     * use the Read backup and Fully replicated flags from the
     * base table.
     */
    DBUG_PRINT("info", ("Index settings: name: %s, read_backup: %u,"
                        " fully_replicated: %u",
                        internalName.c_str(),
                        table.m_read_backup,
                        table.m_fully_replicated));

    w.add(DictTabInfo::ReadBackupFlag, table.m_read_backup);
    w.add(DictTabInfo::FullyReplicatedFlag, table.m_fully_replicated);
  }

  req->indexType = it;
  
  req->tableId = table.m_id;
  req->tableVersion = table.m_version;
  req->online = true;
  IndexAttributeList attributeList;
  attributeList.sz = impl.m_columns.size();
  for(i = 0; i<attributeList.sz; i++){
    const NdbColumnImpl* col = 
      table.getColumn(impl.m_columns[i]->m_name.c_str());
    if(col == 0){
      m_error.code = 4247;
      return -1;
    }
    // Copy column definition  XXX must be wrong, overwrites
    *impl.m_columns[i] = *col;

    // index key type check
    if ((it == DictTabInfo::UniqueHashIndex &&
         (err = NdbSqlUtil::check_column_for_hash_index(col->m_type, col->m_cs)))
        ||
        (it == DictTabInfo::OrderedIndex &&
         (err = NdbSqlUtil::check_column_for_ordered_index(col->m_type, col->m_cs))))
    {
      m_error.code = err;
      return -1;
    }
    // API uses external column number to talk to DICT
    attributeList.id[i] = col->m_column_no;
  }
  LinearSectionPtr ptr[2];
  ptr[0].p = (Uint32*)&attributeList;
  ptr[0].sz = 1 + attributeList.sz;
  ptr[1].p = (Uint32*)m_buffer.get_data();
  ptr[1].sz = m_buffer.length() >> 2;                //BUG?

  int errCodes[] = { CreateIndxRef::Busy, CreateIndxRef::NotMaster, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_CREATE_INDX_REQ"
                        " in NdbDictInterface::createIndex()"));
    timeout = 1000;
  });

  return dictSignal(&tSignal, ptr, 2,
		    0, // master
		    WAIT_CREATE_INDX_REQ,
		    timeout, 100,
		    errCodes);
}

void
NdbDictInterface::execCREATE_INDX_CONF(const NdbApiSignal * signal,
				       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_INDX_CONF");
  const CreateIndxConf* conf = CAST_CONSTPTR(CreateIndxConf,
                                             signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "CREATE_INDX_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_INDX_REF(const NdbApiSignal * sig,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_INDX_REF");
  const CreateIndxRef* ref = CAST_CONSTPTR(CreateIndxRef, sig->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "CREATE_INDX_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  if (m_error.code == ref->NotMaster)
    m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

// INDEX_STAT

int
NdbDictionaryImpl::updateIndexStat(const NdbIndexImpl& index,
                                   const NdbTableImpl& table)
{
  Uint32 rt = IndexStatReq::RT_UPDATE_STAT;
  return m_receiver.doIndexStatReq(m_ndb, index, table, rt);
}

int
NdbDictionaryImpl::updateIndexStat(Uint32 indexId,
                                   Uint32 indexVersion,
                                   Uint32 tableId)
{
  Uint32 rt = IndexStatReq::RT_UPDATE_STAT;
  return m_receiver.doIndexStatReq(m_ndb, indexId, indexVersion, tableId, rt);
}

int
NdbDictionaryImpl::deleteIndexStat(const NdbIndexImpl& index,
                                   const NdbTableImpl& table)
{
  Uint32 rt = IndexStatReq::RT_DELETE_STAT;
  return m_receiver.doIndexStatReq(m_ndb, index, table, rt);
}

int
NdbDictionaryImpl::deleteIndexStat(Uint32 indexId,
                                   Uint32 indexVersion,
                                   Uint32 tableId)
{
  Uint32 rt = IndexStatReq::RT_DELETE_STAT;
  return m_receiver.doIndexStatReq(m_ndb, indexId, indexVersion, tableId, rt);
}

int
NdbDictInterface::doIndexStatReq(Ndb& ndb,
                                 const NdbIndexImpl& index,
                                 const NdbTableImpl& table,
                                 Uint32 rt)
{
  return doIndexStatReq(ndb, index.m_id, index.m_version, table.m_id, rt);
}

int
NdbDictInterface::doIndexStatReq(Ndb& ndb,
                                 Uint32 indexId,
                                 Uint32 indexVersion,
                                 Uint32 tableId,
                                 Uint32 requestType)
{
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_INDEX_STAT_REQ;
  tSignal.theLength = IndexStatReq::SignalLength;

  IndexStatReq* req = CAST_PTR(IndexStatReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = requestType;
  req->requestFlag = 0;
  req->indexId = indexId;
  req->indexVersion = indexVersion;
  req->tableId = tableId;

  int errCodes[] = { IndexStatRef::Busy, IndexStatRef::NotMaster, 0 };
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_INDEX_STAT_REQ"
                        " in NdbDictInterface::doIndexStatReq()"));
    timeout = 1000;
  });

  return dictSignal(&tSignal, 0, 0,
                    0,
                    WAIT_CREATE_INDX_REQ,
                    timeout, 100,
                    errCodes);
}

void
NdbDictInterface::execINDEX_STAT_CONF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execINDEX_STAT_CONF");
  const IndexStatConf* conf = CAST_CONSTPTR(IndexStatConf,
                                            signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "INDX_STAT_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execINDEX_STAT_REF(const NdbApiSignal * signal,
				     const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execINDEX_STAT_REF");
  const IndexStatRef* ref = CAST_CONSTPTR(IndexStatRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "INDX_STAT_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  if (m_error.code == ref->NotMaster)
    m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Drop index
 */
int
NdbDictionaryImpl::dropIndex(const char * indexName, 
			     const char * tableName)
{
  return dropIndex(indexName, tableName, false);
}

int
NdbDictionaryImpl::dropIndex(const char * indexName, 
			     const char * tableName,
                             bool ignoreFKs)
{
  ASSERT_NOT_MYSQLD;
  NdbIndexImpl * idx = getIndex(indexName, tableName);
  if (idx == 0) {
    if(m_error.code == 0)
      m_error.code = 4243;
    return -1;
  }
  int ret = dropIndex(*idx, tableName, ignoreFKs);
  // If index stored in cache is incompatible with the one in the kernel
  // we must clear the cache and try again
  if (ret == INCOMPATIBLE_VERSION) {
    const BaseString internalIndexName((tableName)
      ?
      m_ndb.internalize_index_name(getTable(tableName), indexName)
      :
      m_ndb.internalize_table_name(indexName)); // Index is also a table

    m_localHash.drop(internalIndexName.c_str());
    m_globalHash->lock();
    m_globalHash->release(idx->m_table, 1);
    m_globalHash->unlock();
    return dropIndex(indexName, tableName);
  }

  return ret;
}

int
NdbDictionaryImpl::dropIndex(NdbIndexImpl & impl, const char * tableName)
{
  return dropIndex(impl, tableName, false);
}

int
NdbDictionaryImpl::dropIndex(NdbIndexImpl & impl, const char * tableName,
                             bool ignoreFKs)
{
  const char * indexName = impl.getName();
  if (tableName || m_ndb.usingFullyQualifiedNames()) {
    NdbTableImpl * timpl = impl.m_table;
    
    if (timpl == 0) {
      m_error.code = 709;
      return -1;
    }

    const BaseString internalIndexName((tableName)
      ?
      m_ndb.internalize_index_name(getTable(tableName), indexName)
      :
      m_ndb.internalize_table_name(indexName)); // Index is also a table

    if(impl.m_status == NdbDictionary::Object::New){
      return dropIndex(indexName, tableName, ignoreFKs);
    }

    int ret= dropIndexGlobal(impl, ignoreFKs);
    if (ret == 0)
    {
      m_globalHash->lock();
      m_globalHash->release(impl.m_table, 1);
      m_globalHash->unlock();
      m_localHash.drop(internalIndexName.c_str());
    }
    return ret;
  }
  if(m_error.code == 0)
    m_error.code = 4243;
  return -1;
}

int
NdbDictionaryImpl::dropIndexGlobal(NdbIndexImpl & impl)
{
  return dropIndexGlobal(impl, false);
}

int
NdbDictionaryImpl::dropIndexGlobal(NdbIndexImpl & impl, bool ignoreFKs)
{
  DBUG_ENTER("NdbDictionaryImpl::dropIndexGlobal");
  const char* index_name = impl.m_internalName.c_str();
  DBUG_PRINT("info", ("index name: %s", index_name));

  List list;
  if (listDependentObjects(list, impl.m_id) != 0)
    ERR_RETURN(getNdbError(), -1);

  if (!ignoreFKs)
  {
    /* prevent dropping index if used by a FK */
    for (unsigned i = 0; i < list.count; i++)
    {
      const List::Element& element = list.elements[i];
      const char* fk_name = element.name;

      if (DictTabInfo::isForeignKey(element.type))
      {
        NdbDictionary::ForeignKey fk;
        DBUG_PRINT("info", ("fk name: %s", fk_name));
        if (getForeignKey(fk, fk_name) != 0)
        {
          ERR_RETURN(getNdbError(), -1);
        }

        const char* parent = fk.getParentIndex();
        const char* child = fk.getChildIndex();
        DBUG_PRINT("info", ("parent index: %s child index: %s",
                             parent?parent:"PK", child?child:"PK"));
        if (parent != 0 && strcmp(parent, index_name) == 0)
        {
          m_receiver.m_error.code = 21081;
          ERR_RETURN(getNdbError(), -1);
        }
        if (child != 0 && strcmp(child, index_name) == 0)
        {
          m_receiver.m_error.code = 21082;
          ERR_RETURN(getNdbError(), -1);
        }
      }
    }
  }

  int ret = m_receiver.dropIndex(impl, *impl.m_table);
  impl.m_status = NdbDictionary::Object::Invalid;
  if(ret == 0)
  {
    DBUG_RETURN(0);
  }
  ERR_RETURN(getNdbError(), ret);
}

int
NdbDictInterface::dropIndex(const NdbIndexImpl & impl, 
			    const NdbTableImpl & timpl)
{
  DBUG_ENTER("NdbDictInterface::dropIndex");
  DBUG_PRINT("enter", ("indexId: %d  indexVersion: %d",
                       timpl.m_id, timpl.m_version));
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_DROP_INDX_REQ;
  tSignal.theLength = DropIndxReq::SignalLength;

  DropIndxReq * const req = CAST_PTR(DropIndxReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->indexId = timpl.m_id;
  req->indexVersion = timpl.m_version;

  int errCodes[] = { DropIndxRef::Busy, DropIndxRef::NotMaster, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_DROP_INDX_REQ"
                        " in NdbDictInterface::dropIndex()"));
    timeout = 1000;
  });

  int r = dictSignal(&tSignal, 0, 0,
		     0, // master
		     WAIT_DROP_INDX_REQ,
		     timeout, 100,
		     errCodes);
  if(m_error.code == DropIndxRef::InvalidIndexVersion) {
    // Clear caches and try again
    ERR_RETURN(m_error, INCOMPATIBLE_VERSION);
  }
  ERR_RETURN(m_error, r);
}

void
NdbDictInterface::execDROP_INDX_CONF(const NdbApiSignal * signal,
				       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::DROP_INDX_CONF");
  const DropIndxConf* conf = CAST_CONSTPTR(DropIndxConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->clientData, "DROP_INDX_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_INDX_REF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_INDX_REF");
  const DropIndxRef* ref = CAST_CONSTPTR(DropIndxRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->clientData, "DROP_INDX_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  if (m_error.code == ref->NotMaster)
    m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Create event
 */

int
NdbDictionaryImpl::createEvent(NdbEventImpl & evnt)
{
  DBUG_ENTER("NdbDictionaryImpl::createEvent");
  int i;
  NdbTableImpl* tab= evnt.m_tableImpl;
  if (tab == 0)
  {
    tab= getTable(evnt.getTableName());
    if(tab == 0){
      DBUG_PRINT("info",("NdbDictionaryImpl::createEvent: table not found: %s",
			 evnt.getTableName()));
      ERR_RETURN(getNdbError(), -1);
    }
    evnt.setTable(tab);
  }

  DBUG_PRINT("info",("Table: id: %d version: %d", tab->m_id, tab->m_version));

  NdbTableImpl &table = *evnt.m_tableImpl;

  int attributeList_sz = evnt.m_attrIds.size();

  for (i = 0; i < attributeList_sz; i++) {
    NdbColumnImpl *col_impl = table.getColumn(evnt.m_attrIds[i]);
    if (col_impl) {
      evnt.m_facade->addColumn(*(col_impl->m_facade));
    } else {
      ndbout_c("Attr id %u in table %s not found", evnt.m_attrIds[i],
	       evnt.getTableName());
      m_error.code= 4713;
      ERR_RETURN(getNdbError(), -1);
    }
  }

  evnt.m_attrIds.clear();

  attributeList_sz = evnt.m_columns.size();

  DBUG_PRINT("info",("Event on tableId=%d, tableVersion=%d, event name %s, no of columns %d",
		     table.m_id, table.m_version,
		     evnt.m_name.c_str(),
		     evnt.m_columns.size()));

  int pk_count = 0;
  evnt.m_attrListBitmask.clear();

  for(i = 0; i<attributeList_sz; i++){
    const NdbColumnImpl* col = 
      table.getColumn(evnt.m_columns[i]->m_name.c_str());
    if(col == 0){
      m_error.code= 4247;
      ERR_RETURN(getNdbError(), -1);
    }
    // Copy column definition
    *evnt.m_columns[i] = *col;
    
    if(col->m_pk){
      pk_count++;
    }
    
    evnt.m_attrListBitmask.set(col->m_attrId);
  }
  
  // Sort index attributes according to primary table (using insertion sort)
  for(i = 1; i < attributeList_sz; i++) {
    NdbColumnImpl* temp = evnt.m_columns[i];
    unsigned int j = i;
    while((j > 0) && (evnt.m_columns[j - 1]->m_attrId > temp->m_attrId)) {
      evnt.m_columns[j] = evnt.m_columns[j - 1];
      j--;
    }
    evnt.m_columns[j] = temp;
  }
  // Check for illegal duplicate attributes
  for(i = 1; i<attributeList_sz; i++) {
    if (evnt.m_columns[i-1]->m_attrId == evnt.m_columns[i]->m_attrId) {
      m_error.code= 4258;
      ERR_RETURN(getNdbError(), -1);
    }
  }

  // NdbDictInterface m_receiver;
  if (m_receiver.createEvent(m_ndb, evnt, 0 /* getFlag unset */) != 0)
    ERR_RETURN(getNdbError(), -1);

  // Create blob events
  if (evnt.m_mergeEvents && createBlobEvents(evnt) != 0) {
    int save_code = m_error.code;
    (void)dropEvent(evnt.m_name.c_str(), 0);
    m_error.code = save_code;
    ERR_RETURN(getNdbError(), -1);
  }
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::createBlobEvents(NdbEventImpl& evnt)
{
  DBUG_ENTER("NdbDictionaryImpl::createBlobEvents");
  NdbTableImpl& t = *evnt.m_tableImpl;
  Uint32 n = t.m_noOfBlobs;
  Uint32 i;
  for (i = 0; i < evnt.m_columns.size() && n > 0; i++) {
    NdbColumnImpl & c = *evnt.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    n--;
    NdbEventImpl blob_evnt;
    NdbBlob::getBlobEvent(blob_evnt, &evnt, &c);
    if (createEvent(blob_evnt) != 0)
      ERR_RETURN(getNdbError(), -1);
  }
  DBUG_RETURN(0);
}

int
NdbDictInterface::createEvent(class Ndb & ndb,
			      NdbEventImpl & evnt,
			      int getFlag)
{
  DBUG_ENTER("NdbDictInterface::createEvent");
  DBUG_PRINT("enter",("getFlag=%d",getFlag));

  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_CREATE_EVNT_REQ;
  if (getFlag)
    tSignal.theLength = CreateEvntReq::SignalLengthGet;
  else
    tSignal.theLength = CreateEvntReq::SignalLengthCreate;

  CreateEvntReq * const req = CAST_PTR(CreateEvntReq,
                                       tSignal.getDataPtrSend());
  
  req->setUserRef(m_reference);
  req->setUserData(0);

  Uint32 seccnt = 1;
  LinearSectionPtr ptr[2];

  if (getFlag) {
    // getting event from Dictionary
    req->setRequestType(CreateEvntReq::RT_USER_GET);
  } else {
    DBUG_PRINT("info",("tableId: %u tableVersion: %u",
		       evnt.m_tableImpl->m_id, 
                       evnt.m_tableImpl->m_version));
    // creating event in Dictionary
    req->setRequestType(CreateEvntReq::RT_USER_CREATE);
    req->setTableId(evnt.m_tableImpl->m_id);
    req->setTableVersion(evnt.m_tableImpl->m_version);
    req->setAttrListBitmask(evnt.m_attrListBitmask);
    req->setEventType(evnt.mi_type);
    req->clearFlags();
    if (evnt.m_rep & NdbDictionary::Event::ER_ALL)
      req->setReportAll();
    if (evnt.m_rep & NdbDictionary::Event::ER_SUBSCRIBE)
      req->setReportSubscribe();
    if (evnt.m_rep & NdbDictionary::Event::ER_DDL)
    {
      req->setReportDDL();
    }
    else
    {
      req->clearReportDDL();
    }
    ptr[1].p = evnt.m_attrListBitmask.rep.data;
    ptr[1].sz = evnt.m_attrListBitmask.getSizeInWords();
    seccnt++;
  }

  UtilBufferWriter w(m_buffer);

  const size_t len = strlen(evnt.m_name.c_str()) + 1;
  if(len > MAX_TAB_NAME_SIZE) {
    m_error.code= 4241;
    ERR_RETURN(getNdbError(), -1);
  }

  w.add(SimpleProperties::StringValue, evnt.m_name.c_str());

  if (getFlag == 0)
  {
    const BaseString internal_tabname(
      ndb.internalize_table_name(evnt.m_tableName.c_str()));
    w.add(SimpleProperties::StringValue,
	 internal_tabname.c_str());
  }

  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = (m_buffer.length()+3) >> 2;

  int ret = dictSignal(&tSignal,ptr, seccnt,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       DICT_LONG_WAITFOR_TIMEOUT,  // Lightweight request
                       100,
		       0, -1);

  if (ret) {
    ERR_RETURN(getNdbError(), ret);
  }
  
  char *dataPtr = (char *)m_buffer.get_data();
  unsigned int lenCreateEvntConf = *((unsigned int *)dataPtr);
  dataPtr += sizeof(lenCreateEvntConf);
  CreateEvntConf const * evntConf = (CreateEvntConf *)dataPtr;
  dataPtr += lenCreateEvntConf;
  
  //  NdbEventImpl *evntImpl = (NdbEventImpl *)evntConf->getUserData();

  evnt.m_eventId = evntConf->getEventId();
  evnt.m_eventKey = evntConf->getEventKey();
  evnt.m_table_id = evntConf->getTableId();
  evnt.m_table_version = evntConf->getTableVersion();

  if (getFlag) {
    evnt.m_attrListBitmask = evntConf->getAttrListBitmask();
    evnt.mi_type           = evntConf->getEventType();
    evnt.setTable(dataPtr);
    if (!m_tableData.empty())
    {
      Uint32 len = m_tableData.length();
      assert((len & 3) == 0);
      len /= 4;
      if (len <= evnt.m_attrListBitmask.getSizeInWords())
      {
        evnt.m_attrListBitmask.clear();
        memcpy(evnt.m_attrListBitmask.rep.data, m_tableData.get_data(), 4*len);
      }
      else
      {
        memcpy(evnt.m_attrListBitmask.rep.data, m_tableData.get_data(),
               4*evnt.m_attrListBitmask.getSizeInWords());
      }
    }
  } else {
    if ((Uint32) evnt.m_tableImpl->m_id         != evntConf->getTableId() ||
	evnt.m_tableImpl->m_version    != evntConf->getTableVersion() ||
	//evnt.m_attrListBitmask != evntConf->getAttrListBitmask() ||
	evnt.mi_type           != evntConf->getEventType()) {
      ndbout_c("ERROR*************");
      m_buffer.clear();
      m_tableData.clear();
      ERR_RETURN(getNdbError(), 1);
    }
  }

  m_buffer.clear();
  m_tableData.clear();

  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::executeSubscribeEvent(NdbEventOperationImpl & ev_op)
{
  // NdbDictInterface m_receiver;
  return m_receiver.executeSubscribeEvent(m_ndb, ev_op);
}

int
NdbDictInterface::executeSubscribeEvent(class Ndb & ndb,
					NdbEventOperationImpl & ev_op)
{
  DBUG_ENTER("NdbDictInterface::executeSubscribeEvent");
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_SUB_START_REQ;
  tSignal.theLength = SubStartReq::SignalLength;
  
  SubStartReq * req = CAST_PTR(SubStartReq, tSignal.getDataPtrSend());

  req->subscriptionId   = ev_op.m_eventImpl->m_eventId;
  req->subscriptionKey  = ev_op.m_eventImpl->m_eventKey;
  req->part             = SubscriptionData::TableData;
  req->subscriberData   = ev_op.m_oid;
  req->subscriberRef    = m_reference;

  DBUG_PRINT("info",("GSN_SUB_START_REQ subscriptionId=%d,subscriptionKey=%d,"
		     "subscriberData=%d",req->subscriptionId,
		     req->subscriptionKey,req->subscriberData));

  int errCodes[] = { SubStartRef::Busy,
                     SubStartRef::BusyWithNR,
                     SubStartRef::NotMaster,
                     0 };
  int ret = dictSignal(&tSignal,NULL,0,
                       0 /*use masternode id*/,
                       WAIT_CREATE_INDX_REQ /*WAIT_CREATE_EVNT_REQ*/,
                       DICT_LONG_WAITFOR_TIMEOUT, 100,
                       errCodes, -1);

  DBUG_RETURN(ret);
}

int
NdbDictionaryImpl::stopSubscribeEvent(NdbEventOperationImpl & ev_op,
                                      Uint64& stop_gci)
{
  // NdbDictInterface m_receiver;
  return m_receiver.stopSubscribeEvent(m_ndb, ev_op, stop_gci);
}

int
NdbDictInterface::stopSubscribeEvent(class Ndb & ndb,
				     NdbEventOperationImpl & ev_op,
                                     Uint64& stop_gci)
{
  DBUG_ENTER("NdbDictInterface::stopSubscribeEvent");

  NdbApiSignal tSignal(m_reference);
  //  tSignal.theReceiversBlockNumber = SUMA;
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_SUB_STOP_REQ;
  tSignal.theLength = SubStopReq::SignalLength;
  
  SubStopReq * req = CAST_PTR(SubStopReq, tSignal.getDataPtrSend());

  req->subscriptionId  = ev_op.m_eventImpl->m_eventId;
  req->subscriptionKey = ev_op.m_eventImpl->m_eventKey;
  req->subscriberData  = ev_op.m_oid;
  req->part            = (Uint32) SubscriptionData::TableData;
  req->subscriberRef   = m_reference;
  req->requestInfo     = 0;

  DBUG_PRINT("info",("GSN_SUB_STOP_REQ subscriptionId=%d,subscriptionKey=%d,"
		     "subscriberData=%d",req->subscriptionId,
		     req->subscriptionKey,req->subscriberData));

  int errCodes[] = { SubStartRef::Busy,
                     SubStartRef::BusyWithNR,
                     SubStartRef::NotMaster,
                     0 };
  int ret= dictSignal(&tSignal,NULL,0,
                      0 /*use masternode id*/,
                      WAIT_CREATE_INDX_REQ /*WAIT_SUB_STOP__REQ*/,
                      DICT_LONG_WAITFOR_TIMEOUT, 100,
                      errCodes, -1);
  if (ret == 0)
  {
    Uint32 *data = (Uint32*)m_buffer.get_data();
    stop_gci = data[1] | (Uint64(data[0]) << 32);
  }
  DBUG_RETURN(ret);
}

NdbEventImpl * 
NdbDictionaryImpl::getEvent(const char * eventName, NdbTableImpl* tab)
{
  DBUG_ENTER("NdbDictionaryImpl::getEvent");
  DBUG_PRINT("enter",("eventName= %s", eventName));

  NdbEventImpl *ev =  new NdbEventImpl();
  if (ev == NULL) {
    DBUG_RETURN(NULL);
  }

  ev->setName(eventName);

  int ret = m_receiver.createEvent(m_ndb, *ev, 1 /* getFlag set */);

  if (ret) {
    delete ev;
    DBUG_RETURN(NULL);
  }

  // We only have the table name with internal name
  DBUG_PRINT("info",("table %s", ev->getTableName()));
  if (tab == NULL)
  {
    tab= fetchGlobalTableImplRef(InitTable(ev->getTableName()));
    if (tab == 0)
    {
      DBUG_PRINT("error",("unable to find table %s", ev->getTableName()));
      delete ev;
      DBUG_RETURN(NULL);
    }
    if ((tab->m_status != NdbDictionary::Object::Retrieved) ||
        ((Uint32) tab->m_id != ev->m_table_id) ||
        (table_version_major(tab->m_version) !=
         table_version_major(ev->m_table_version)))
    {
      DBUG_PRINT("info", ("mismatch on verison in cache"));
      releaseTableGlobal(*tab, 1);
      tab= fetchGlobalTableImplRef(InitTable(ev->getTableName()));
      if (tab == 0)
      {
        DBUG_PRINT("error",("unable to find table %s", ev->getTableName()));
        delete ev;
        DBUG_RETURN(NULL);
      }
    }
    ev->setTable(tab);
    releaseTableGlobal(*tab, 0);
  }
  else
    ev->setTable(tab);
  tab = 0;

  ev->setTable(m_ndb.externalizeTableName(ev->getTableName()));  
  // get the columns from the attrListBitmask
  NdbTableImpl &table = *ev->m_tableImpl;
  AttributeMask & mask = ev->m_attrListBitmask;
  unsigned attributeList_sz = mask.count();

  DBUG_PRINT("info",("Table: id: %d version: %d", 
                     table.m_id, table.m_version));

  if ((Uint32) table.m_id != ev->m_table_id ||
      table_version_major(table.m_version) !=
      table_version_major(ev->m_table_version))
  {
    m_error.code = 241;
    delete ev;
    DBUG_RETURN(NULL);
  }

  if ( attributeList_sz > (uint) table.getNoOfColumns() )
  {
    m_error.code = 241;
    DBUG_PRINT("error",("Invalid version, too many columns"));
    delete ev;
    DBUG_RETURN(NULL);
  }

  assert( (int)attributeList_sz <= table.getNoOfColumns() );
  for(unsigned id= 0; ev->m_columns.size() < attributeList_sz; id++) {
    if ( id >= (uint) table.getNoOfColumns())
    {
      m_error.code = 241;
      DBUG_PRINT("error",("Invalid version, column %d out of range", id));
      delete ev;
      DBUG_RETURN(NULL);
    }
    if (!mask.get(id))
      continue;

    const NdbColumnImpl* col = table.getColumn(id);
    DBUG_PRINT("info",("column %d %s", id, col->getName()));
    NdbColumnImpl* new_col = new NdbColumnImpl;
    // Copy column definition
    *new_col = *col;
    ev->m_columns.push_back(new_col);
  }
  DBUG_RETURN(ev);
}

// ev is main event and has been retrieved previously
NdbEventImpl *
NdbDictionaryImpl::getBlobEvent(const NdbEventImpl& ev, uint col_no)
{
  DBUG_ENTER("NdbDictionaryImpl::getBlobEvent");
  DBUG_PRINT("enter", ("ev=%s col=%u", ev.m_name.c_str(), col_no));

  NdbTableImpl* tab = ev.m_tableImpl;
  assert(tab != NULL && col_no < tab->m_columns.size());
  NdbColumnImpl* col = tab->m_columns[col_no];
  assert(col != NULL && col->getBlobType() && col->getPartSize() != 0);
  NdbTableImpl* blob_tab = col->m_blobTable;
  assert(blob_tab != NULL);
  char bename[MAX_TAB_NAME_SIZE];
  NdbBlob::getBlobEventName(bename, &ev, col);

  NdbEventImpl* blob_ev = getEvent(bename, blob_tab);
  DBUG_RETURN(blob_ev);
}

void
NdbDictInterface::execCREATE_EVNT_CONF(const NdbApiSignal * signal,
				       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_EVNT_CONF");

  m_buffer.clear();
  m_tableData.clear();
  unsigned int len = signal->getLength() << 2;
  m_buffer.append((char *)&len, sizeof(len));
  m_buffer.append(signal->getDataPtr(), len);

  if (signal->m_noOfSections > 0) {
    m_buffer.append((char *)ptr[0].p, strlen((char *)ptr[0].p)+1);
  }
  if (signal->m_noOfSections > 1)
  {
    m_tableData.append(ptr[1].p, 4 * ptr[1].sz);
  }

#ifdef DEBUG_OUTPUT
  const CreateEvntConf * const createEvntConf=
    CAST_CONSTPTR(CreateEvntConf, signal->getDataPtr());

  DBUG_PRINT("info",("nodeid=%d,subscriptionId=%d,subscriptionKey=%d",
		     refToNode(signal->theSendersBlockRef),
		     createEvntConf->getEventId(),
                     createEvntConf->getEventKey()));
#endif
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_EVNT_REF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_EVNT_REF");

  const CreateEvntRef* const ref=
    CAST_CONSTPTR(CreateEvntRef, signal->getDataPtr());
  m_error.code= ref->getErrorCode();
  DBUG_PRINT("error",("error=%d,line=%d,node=%d",ref->getErrorCode(),
		      ref->getErrorLine(),ref->getErrorNode()));
  if (m_error.code == CreateEvntRef::NotMaster)
    m_masterNodeId = ref->getMasterNode();
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_STOP_CONF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_STOP_CONF");
  const SubStopConf * const subStopConf=
    CAST_CONSTPTR(SubStopConf, signal->getDataPtr());

  DBUG_PRINT("info",("subscriptionId=%d,subscriptionKey=%d,subscriberData=%d",
		     subStopConf->subscriptionId,
                     subStopConf->subscriptionKey,
                     subStopConf->subscriberData));

  Uint32 gci_hi= 0;
  Uint32 gci_lo= 0;
  if (SubStopConf::SignalLength >= SubStopConf::SignalLengthWithGci)
  {
    gci_hi= subStopConf->gci_hi;
    gci_lo= subStopConf->gci_lo;
  }

  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = gci_hi;
  data[1] = gci_lo;

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_STOP_REF(const NdbApiSignal * signal,
				     const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_STOP_REF");
  const SubStopRef * const subStopRef=
    CAST_CONSTPTR(SubStopRef, signal->getDataPtr());

  m_error.code= subStopRef->errorCode;

  DBUG_PRINT("error",("subscriptionId=%d,subscriptionKey=%d,subscriberData=%d,error=%d",
		      subStopRef->subscriptionId,
                      subStopRef->subscriptionKey,
                      subStopRef->subscriberData,
                      m_error.code));
  if (m_error.code == SubStopRef::NotMaster &&
      signal->getLength() >= SubStopRef::SL_MasterNode)
  {
    m_masterNodeId = subStopRef->m_masterNodeId;
  }
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_START_CONF(const NdbApiSignal * signal,
				     const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_START_CONF");
  const SubStartConf * const subStartConf=
    CAST_CONSTPTR(SubStartConf, signal->getDataPtr());
  const Uint32 sigLen = signal->getLength();

  SubscriptionData::Part part = 
    (SubscriptionData::Part)subStartConf->part;

  switch(part) {
  case SubscriptionData::MetaData: {
    DBUG_PRINT("error",("SubscriptionData::MetaData"));
    m_error.code= 1;
    break;
  }
  case SubscriptionData::TableData: {
    DBUG_PRINT("info",("SubscriptionData::TableData"));
    break;
  }
  default: {
    DBUG_PRINT("error",("wrong data"));
    m_error.code= 2;
    break;
  }
  }

  DBUG_PRINT("info",("subscriptionId=%d,subscriptionKey=%d,subscriberData=%d",
		     subStartConf->subscriptionId,
                     subStartConf->subscriptionKey,
                     subStartConf->subscriberData));
  /*
   * If this is the first subscription NdbEventBuffer needs to be
   * notified.  NdbEventBuffer will start listen to Suma signals
   * such as SUB_GCP_COMPLETE_REP.  Also NdbEventBuffer will use
   * the total bucket count from signal.
   */
  m_impl->m_ndb.theEventBuffer->execSUB_START_CONF(subStartConf, sigLen);
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_START_REF(const NdbApiSignal * signal,
				    const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_START_REF");
  const SubStartRef * const subStartRef=
    CAST_CONSTPTR(SubStartRef, signal->getDataPtr());
  m_error.code= subStartRef->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  if (m_error.code == SubStartRef::NotMaster)
    m_masterNodeId = subStartRef->m_masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Drop event
 */
int 
NdbDictionaryImpl::dropEvent(const char * eventName, int force)
{
  DBUG_ENTER("NdbDictionaryImpl::dropEvent");
  DBUG_PRINT("enter", ("name:%s  force: %d", eventName, force));

  NdbEventImpl *evnt = NULL;
  if (!force)
  {
    evnt = getEvent(eventName); // allocated
    if (evnt == NULL)
    {
      if (m_error.code != 723 && // no such table
          m_error.code != 241)   // invalid table
      {
        DBUG_PRINT("info", ("no table err=%d", m_error.code));
        DBUG_RETURN(-1);
      }
      DBUG_PRINT("info", ("no table err=%d, drop by name alone", m_error.code));   
    }
  }
  if (evnt == NULL)
  {
    evnt = new NdbEventImpl();
    evnt->setName(eventName);
  }
  int ret = dropEvent(*evnt);
  delete evnt;  
  DBUG_RETURN(ret);
}

int
NdbDictionaryImpl::dropEvent(const NdbEventImpl& evnt)
{
  if (dropBlobEvents(evnt) != 0)
    return -1;
  if (m_receiver.dropEvent(evnt) != 0)
    return -1;
  return 0;
}

int
NdbDictionaryImpl::dropBlobEvents(const NdbEventImpl& evnt)
{
  DBUG_ENTER("NdbDictionaryImpl::dropBlobEvents");
  if (evnt.m_tableImpl != 0) {
    const NdbTableImpl& t = *evnt.m_tableImpl;
    Uint32 n = t.m_noOfBlobs;
    Uint32 i;
    for (i = 0; i < evnt.m_columns.size() && n > 0; i++) {
      const NdbColumnImpl& c = *evnt.m_columns[i];
      if (! c.getBlobType() || c.getPartSize() == 0)
        continue;
      n--;
      NdbEventImpl* blob_evnt = getBlobEvent(evnt, i);
      if (blob_evnt == NULL)
        continue;
      (void)dropEvent(*blob_evnt);
      delete blob_evnt;
    }
  }
  else
  {
    DBUG_PRINT("info", ("no table definition, listing events"));
    char bename[MAX_TAB_NAME_SIZE];
    int val;
    // XXX should get name from NdbBlob
    sprintf(bename, "NDB$BLOBEVENT_%s_%s", evnt.getName(), "%d");
    List list;
    if (listEvents(list))
      DBUG_RETURN(-1);
    for (unsigned i = 0; i < list.count; i++)
    {
      NdbDictionary::Dictionary::List::Element& elt = list.elements[i];
      switch (elt.type)
      {
      case NdbDictionary::Object::TableEvent:
        if (sscanf(elt.name, bename, &val) == 1)
        {
          DBUG_PRINT("info", ("found blob event %s, removing...", elt.name));
          NdbEventImpl* bevnt = new NdbEventImpl();
          bevnt->setName(elt.name);
          (void)m_receiver.dropEvent(*bevnt);
          delete bevnt;
        }
        else
          DBUG_PRINT("info", ("found event %s, skipping...", elt.name));
        break;
      default:
        break;
      }
    }
  }
  DBUG_RETURN(0);
}

int
NdbDictInterface::dropEvent(const NdbEventImpl &evnt)
{
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_DROP_EVNT_REQ;
  tSignal.theLength = DropEvntReq::SignalLength;
  
  DropEvntReq * const req = CAST_PTR(DropEvntReq, tSignal.getDataPtrSend());

  req->setUserRef(m_reference);
  req->setUserData(0);

  UtilBufferWriter w(m_buffer);

  w.add(SimpleProperties::StringValue, evnt.m_name.c_str());

  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = (m_buffer.length()+3) >> 2;

  return dictSignal(&tSignal,ptr, 1,
		    0 /*use masternode id*/,
		    WAIT_CREATE_INDX_REQ,
		    DICT_LONG_WAITFOR_TIMEOUT, 100,
		    0, -1);
}

void
NdbDictInterface::execDROP_EVNT_CONF(const NdbApiSignal * signal,
				     const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_EVNT_CONF");
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_EVNT_REF(const NdbApiSignal * signal,
				    const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_EVNT_REF");
  const DropEvntRef* const ref=
    CAST_CONSTPTR(DropEvntRef, signal->getDataPtr());
  m_error.code= ref->getErrorCode();

  DBUG_PRINT("info",("ErrorCode=%u Errorline=%u ErrorNode=%u",
	     ref->getErrorCode(), ref->getErrorLine(), ref->getErrorNode()));
  if (m_error.code == DropEvntRef::NotMaster)
    m_masterNodeId = ref->getMasterNode();
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

static int scanEventTable(Ndb* pNdb, 
                          const NdbDictionary::Table* pTab,
                          NdbDictionary::Dictionary::List &list)
{
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  NdbTransaction       *pTrans = NULL;
  NdbScanOperation     *pOp = NULL;
  NdbRecAttr *event_name, *event_id;
  NdbError err;
  const Uint32 codeWords= 1;
  Uint32 codeSpace[ codeWords ];
  NdbInterpretedCode code(pTab,
                          &codeSpace[0],
                          codeWords);
  if ((code.interpret_exit_last_row() != 0) ||
      (code.finalise() != 0))
  {
    return code.getNdbError().code;
  }

  while (true)
  {
    NdbDictionary::Dictionary::List tmp_list;

    if (retryAttempt)
    {
      if (retryAttempt >= retryMax)
      {
        ndbout << "ERROR: has retried this operation " << retryAttempt 
               << " times, failing!" << endl;
        goto error;
      }
      if (pTrans)
        pNdb->closeTransaction(pTrans);
      NdbSleep_MilliSleep(50);
    }
    retryAttempt++;
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
      if (pNdb->getNdbError().status == NdbError::TemporaryError)
        continue;
      goto error;
    }

    Uint64 row_count = 0;
    {
      if ((pOp = pTrans->getNdbScanOperation(pTab)) == NULL)
        goto error;
      if (pOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 1) != 0)
        goto error;
      if (pOp->setInterpretedCode(&code) != 0)
        goto error;

      Uint64 tmp;
      pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&tmp);
      if (pTrans->execute(NdbTransaction::NoCommit) == -1)
        goto error;

      int eof;
      while ((eof = pOp->nextResult(true)) == 0)
        row_count += tmp;
    
      if (eof == -1)
      {
        if (pTrans->getNdbError().status == NdbError::TemporaryError)
          continue;
        goto error;
      }
    }

    if ((pOp = pTrans->getNdbScanOperation(pTab)) == NULL)
      goto error;

    if (pOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 1) != 0)
      goto error;
    
    if ((event_id   = pOp->getValue(6)) == 0 ||
        (event_name = pOp->getValue(0u)) == 0)
      goto error;

    if (pTrans->execute(NdbTransaction::NoCommit) == -1)
    {
      const NdbError err = pTrans->getNdbError();
      if (err.status == NdbError::TemporaryError)
        continue;
      goto error;
    }

    /* Cannot handle > 2^32 yet (limit on tmp_list.count is unsigned int) */
    assert((row_count & 0xffffffff) == row_count);

    tmp_list.count = (unsigned int)row_count;
    tmp_list.elements =
      new NdbDictionary::Dictionary::List::Element[(unsigned int)row_count];

    int eof;
    unsigned rows = 0;
    while((eof = pOp->nextResult()) == 0)
    {
      if (rows < tmp_list.count)
      {
        NdbDictionary::Dictionary::List::Element &el = tmp_list.elements[rows];
        el.id = event_id->u_32_value();
        el.type = NdbDictionary::Object::TableEvent;
        el.state = NdbDictionary::Object::StateOnline;
        el.store = NdbDictionary::Object::StorePermanent;
        Uint32 len = (Uint32)strlen(event_name->aRef());
        el.name = new char[len+1];
        memcpy(el.name, event_name->aRef(), len);
        el.name[len] = 0;
      }
      rows++;
    }
    if (eof == -1)
    {
      if (pTrans->getNdbError().status == NdbError::TemporaryError)
        continue;
      goto error;
    }

    pNdb->closeTransaction(pTrans);

    if (rows < tmp_list.count)
      tmp_list.count = rows;

    list = tmp_list;
    tmp_list.count = 0;
    tmp_list.elements = NULL;

    return 0;
  }
error:
  int error_code;
  if (pTrans)
  {
    error_code = pTrans->getNdbError().code;
    pNdb->closeTransaction(pTrans);
  }
  else
    error_code = pNdb->getNdbError().code;

  return error_code;
}

int
NdbDictionaryImpl::listEvents(List& list)
{
  int error_code;

  BaseString currentDb(m_ndb.getDatabaseName());
  BaseString currentSchema(m_ndb.getDatabaseSchemaName());

  m_ndb.setDatabaseName("sys");
  m_ndb.setDatabaseSchemaName("def");
  {
    const NdbDictionary::Table* pTab =
      m_facade->getTableGlobal("NDB$EVENTS_0");

    if(pTab == NULL)
      error_code = m_facade->getNdbError().code;
    else
    {     
      error_code = scanEventTable(&m_ndb, pTab, list);
      m_facade->removeTableGlobal(*pTab, 0);
    }
  }

  m_ndb.setDatabaseName(currentDb.c_str());
  m_ndb.setDatabaseSchemaName(currentSchema.c_str());
  if (error_code)
  {
    m_error.code = error_code;
    return -1;
  }
  return 0;
}

/*****************************************************************
 * List objects or indexes
 */
int
NdbDictionaryImpl::listObjects(List& list, 
                               NdbDictionary::Object::Type type,
                               bool fullyQualified)
{
  int ret;
  List list1, list2;
  if (type == NdbDictionary::Object::TableEvent)
    return listEvents(list);

  if (type == NdbDictionary::Object::TypeUndefined)
  {
    ret = listEvents(list2);
    if (ret)
      return ret;
  }

  ListTablesReq req;
  req.init();
  req.setTableId(0);
  req.setTableType(getKernelConstant(type, objectTypeMapping, 0));
  req.setListNames(true);
  if (!list2.count)
    return m_receiver.listObjects(list, req, fullyQualified);
  ret = m_receiver.listObjects(list1, req, fullyQualified);
  if (ret)
    return ret;
  list.count = list1.count + list2.count;
  list.elements = new NdbDictionary::Dictionary::List::Element[list.count];
  unsigned i;
  const NdbDictionary::Dictionary::List::Element null_el;
  for (i = 0; i < list1.count; i++)
  {
    NdbDictionary::Dictionary::List::Element &el = list1.elements[i];
    list.elements[i] = el;
    el = null_el;
  }
  for (i = 0; i < list2.count; i++)
  {
    NdbDictionary::Dictionary::List::Element &el = list2.elements[i];
    list.elements[i + list1.count] = el;
    el = null_el;
  }
  return 0;
}

int
NdbDictionaryImpl::listIndexes(List& list, Uint32 indexId, bool fullyQualified)
{
  ListTablesReq req;
  req.init();
  req.setTableId(indexId);
  req.setTableType(0);
  req.setListNames(true);
  req.setListIndexes(true);
  return m_receiver.listObjects(list, req, fullyQualified);
}

int
NdbDictionaryImpl::listDependentObjects(List& list, Uint32 tableId)
{
  ListTablesReq req;
  req.init();
  req.setTableId(tableId);
  req.setTableType(0);
  req.setListNames(true);
  req.setListDependent(true);
  return m_receiver.listObjects(list, req, m_ndb.usingFullyQualifiedNames());
}

int
NdbDictInterface::listObjects(NdbDictionary::Dictionary::List& list,
                              ListTablesReq& ltreq, bool fullyQualifiedNames)
{
  bool listTablesLongSignal = false;
  NdbApiSignal tSignal(m_reference);
  ListTablesReq* const req = CAST_PTR(ListTablesReq, tSignal.getDataPtrSend());
  memcpy(req, &ltreq, sizeof(ListTablesReq));
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  if (ltreq.getTableId() > 4096)
  {
    /*
      Enforce new long signal format,
      if this is not supported by the
      called node the request will fail
     */
    listTablesLongSignal = true;
  }

  /*
    Set table id and type according to old format
    in case sent to old nodes (during upgrade).
  */
  req->oldSetTableId(ltreq.getTableId());
  req->oldSetTableType(ltreq.getTableType());

  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_LIST_TABLES_REQ;
  tSignal.theLength = ListTablesReq::SignalLength;
  if (listObjects(&tSignal, listTablesLongSignal) != 0)
    return -1;

  if (listTablesLongSignal)
  {
    return unpackListTables(list, fullyQualifiedNames);
  }
  else
  {
    return unpackOldListTables(list, fullyQualifiedNames);
  }
}

int
NdbDictInterface::unpackListTables(NdbDictionary::Dictionary::List& list,
                                   bool fullyQualifiedNames)
{
  Uint32 count = 0;
  Uint32* tableData = (Uint32*)m_tableData.get_data();
  Uint32* tableNames = (Uint32*)m_tableNames.get_data();
  const Uint32 listTablesDataSizeInWords = (sizeof(ListTablesData) + 3) / 4;
  list.count = m_noOfTables;
  list.elements = new NdbDictionary::Dictionary::List::Element[m_noOfTables];

  while (count < m_noOfTables)
  {
    NdbDictionary::Dictionary::List::Element& element = list.elements[count];
    ListTablesData _ltd;
    ListTablesData * ltd = &_ltd;
    memcpy(ltd, tableData, 4 * listTablesDataSizeInWords);
    tableData += listTablesDataSizeInWords;
    element.id = ltd->getTableId();
    element.type = (NdbDictionary::Object::Type)
      getApiConstant(ltd->getTableType(), objectTypeMapping, 0);
    element.state = (NdbDictionary::Object::State)
      getApiConstant(ltd->getTableState(), objectStateMapping, 0);
    element.store = (NdbDictionary::Object::Store)
      getApiConstant(ltd->getTableStore(), objectStoreMapping, 0);
    element.temp = ltd->getTableTemp();
    // table or index name
    BaseString databaseName;
    BaseString schemaName;
    BaseString objectName;
    if (!databaseName || !schemaName || !objectName)
    {
      m_error.code= 4000;
      return -1;
    }
    Uint32 size = tableNames[0];
    Uint32 wsize = (size + 3) / 4;
    tableNames++;
    if ((element.type == NdbDictionary::Object::UniqueHashIndex) ||
	(element.type == NdbDictionary::Object::OrderedIndex)) {
      char * indexName = new char[size];
      if (indexName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(indexName, (char *) tableNames, size);
      if (!(databaseName = Ndb::getDatabaseFromInternalName(indexName)) ||
          !(schemaName = Ndb::getSchemaFromInternalName(indexName)))
      {
        delete [] indexName;
        m_error.code= 4000;
        return -1;
      }
      objectName = BaseString(Ndb::externalizeIndexName(indexName,
                                                        fullyQualifiedNames));
      delete [] indexName;
    } else if ((element.type == NdbDictionary::Object::SystemTable) ||
	       (element.type == NdbDictionary::Object::UserTable)) {
      char * tableName = new char[size];
      if (tableName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(tableName, (char *) tableNames, size);
      if (!(databaseName = Ndb::getDatabaseFromInternalName(tableName)) ||
          !(schemaName = Ndb::getSchemaFromInternalName(tableName)))
      {
        delete [] tableName;
        m_error.code= 4000;
        return -1;
      }
      objectName = BaseString(Ndb::externalizeTableName(tableName,
                                                        fullyQualifiedNames));
      delete [] tableName;
    }
    else {
      char * otherName = new char[size];
      if (otherName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(otherName, (char *) tableNames, size);
      if (!(objectName = BaseString(otherName)))
      {
        m_error.code= 4000;
        return -1;
      }
      delete [] otherName;
    }
    if (!(element.database = new char[databaseName.length() + 1]) ||
        !(element.schema = new char[schemaName.length() + 1]) ||
        !(element.name = new char[objectName.length() + 1]))
    {
      m_error.code= 4000;
      return -1;
    }
    strcpy(element.database, databaseName.c_str());
    strcpy(element.schema, schemaName.c_str());
    strcpy(element.name, objectName.c_str());
    count++;
    tableNames += wsize;
  }

  return 0;
}

int
NdbDictInterface::unpackOldListTables(NdbDictionary::Dictionary::List& list,
                                      bool fullyQualifiedNames)
{
  // count
  const Uint32* data = (const Uint32*)m_buffer.get_data();
  const unsigned length = m_buffer.length() / 4;
  list.count = 0;
  bool ok = true;
  unsigned pos, count;
  pos = count = 0;
  while (pos < length) {
    // table id - name length - name
    pos++;
    if (pos >= length) {
      ok = false;
      break;
    }
    Uint32 n = (data[pos++] + 3) >> 2;
    pos += n;
    if (pos > length) {
      ok = false;
      break;
    }
    count++;
  }
  if (! ok) {
    // bad signal data
    m_error.code= 4213;
    return -1;
  }
  list.count = count;
  list.elements = new NdbDictionary::Dictionary::List::Element[count];
  pos = count = 0;
  while (pos < length) {
    NdbDictionary::Dictionary::List::Element& element = list.elements[count];
    Uint32 d = data[pos++];
    element.id = OldListTablesConf::getTableId(d);
    element.type = (NdbDictionary::Object::Type)
      getApiConstant(OldListTablesConf::getTableType(d), objectTypeMapping, 0);
    element.state = (NdbDictionary::Object::State)
      getApiConstant(OldListTablesConf::getTableState(d), objectStateMapping, 0);
    element.store = (NdbDictionary::Object::Store)
      getApiConstant(OldListTablesConf::getTableStore(d), objectStoreMapping, 0);
    element.temp = OldListTablesConf::getTableTemp(d);
    // table or index name
    Uint32 n = (data[pos++] + 3) >> 2;
    BaseString databaseName;
    BaseString schemaName;
    BaseString objectName;
    if (!databaseName || !schemaName || !objectName)
    {
      m_error.code= 4000;
      return -1;
    }
    if ((element.type == NdbDictionary::Object::UniqueHashIndex) ||
	(element.type == NdbDictionary::Object::OrderedIndex)) {
      char * indexName = new char[n << 2];
      if (indexName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(indexName, &data[pos], n << 2);
      if (!(databaseName = Ndb::getDatabaseFromInternalName(indexName)) ||
          !(schemaName = Ndb::getSchemaFromInternalName(indexName)))
      {
        delete [] indexName;
        m_error.code= 4000;
        return -1;
      }
      objectName = BaseString(Ndb::externalizeIndexName(indexName, fullyQualifiedNames));
      delete [] indexName;
    } else if ((element.type == NdbDictionary::Object::SystemTable) || 
	       (element.type == NdbDictionary::Object::UserTable)) {
      char * tableName = new char[n << 2];
      if (tableName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(tableName, &data[pos], n << 2);
      if (!(databaseName = Ndb::getDatabaseFromInternalName(tableName)) ||
          !(schemaName = Ndb::getSchemaFromInternalName(tableName)))
      {
        delete [] tableName;
        m_error.code= 4000;
        return -1;
      }
      objectName = BaseString(Ndb::externalizeTableName(tableName, fullyQualifiedNames));
      delete [] tableName;
    }
    else {
      char * otherName = new char[n << 2];
      if (otherName == NULL)
      {
        m_error.code= 4000;
        return -1;
      }
      memcpy(otherName, &data[pos], n << 2);
      if (!(objectName = BaseString(otherName)))
      {
        m_error.code= 4000;
        return -1;
      }
      delete [] otherName;
    }
    if (!(element.database = new char[databaseName.length() + 1]) ||
        !(element.schema = new char[schemaName.length() + 1]) ||
        !(element.name = new char[objectName.length() + 1]))
    {
      m_error.code= 4000;
      return -1;
    }
    strcpy(element.database, databaseName.c_str());
    strcpy(element.schema, schemaName.c_str());
    strcpy(element.name, objectName.c_str());
    pos += n;
    count++;
  }
  return 0;
}

int
NdbDictInterface::listObjects(NdbApiSignal* signal,
                              bool& listTablesLongSignal)
{
  const Uint32 RETRIES = 100;
  for (Uint32 i = 0; i < RETRIES; i++) {
    m_buffer.clear();
    // begin protected
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(* m_impl);
    Uint16 aNodeId = getTransporter()->get_an_alive_node();
    if (aNodeId == 0) {
      if (getTransporter()->is_cluster_completely_unavailable())
      {
        m_error.code= 4009;
      }
      else
      {
        m_error.code = 4035;
      }
      return -1;
    }
    NodeInfo info = m_impl->getNodeInfo(aNodeId).m_info;
    if (ndbd_LIST_TABLES_CONF_long_signal(info.m_version))
    {
      /*
        Called node will return a long signal
       */
      listTablesLongSignal = true;
    }
    else if (listTablesLongSignal)
    {
      /*
        We are requesting info from a table with table id > 4096
        and older versions don't support that, bug#36044
      */
      m_error.code= 4105;
      return -1;
    }

    if (m_impl->sendSignal(signal, aNodeId) != 0) {
      continue;
    }
    m_impl->incClientStat(Ndb::WaitMetaRequestCount, 1);
    m_error.code= 0;

    int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
    DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_LIST_TABLES_REQ"
                        " in NdbDictInterface::listObjects()"));
      timeout = 1000;
    });

    int ret_val= poll_guard.wait_n_unlock(timeout,
                                          aNodeId, WAIT_LIST_TABLES_CONF,
                                          true);
    // end protected
    if(m_error.code == 0 && m_impl->theWaiter.get_state() == WST_WAIT_TIMEOUT)
    {
      DBUG_PRINT("info", ("wait_n_unlock caught time-out"));
      m_error.code = 4008;
      return -1;
    }

    if (ret_val == 0 && m_error.code == 0)
      return 0;
    if (ret_val == -2) //WAIT_NODE_FAILURE
      continue;
    return -1;
  }
  return -1;
}

void
NdbDictInterface::execLIST_TABLES_CONF(const NdbApiSignal* signal,
                                       const LinearSectionPtr ptr[3])
{
  Uint16 nodeId = refToNode(signal->theSendersBlockRef);
  NodeInfo info = m_impl->getNodeInfo(nodeId).m_info;
  if (!ndbd_LIST_TABLES_CONF_long_signal(info.m_version))
  {
    /*
      Sender doesn't support new signal format
     */
    NdbDictInterface::execOLD_LIST_TABLES_CONF(signal, ptr);
    return;
  }

  const ListTablesConf* const conf=
    CAST_CONSTPTR(ListTablesConf, signal->getDataPtr());
  if(!m_tx.checkRequestId(conf->senderData, "LIST_TABLES_CONF"))
    return; // signal from different (possibly timed-out) transaction

  if (signal->isFirstFragment())
  {
    m_fragmentId = signal->getFragmentId();
    m_noOfTables = 0;
    m_tableData.clear();
    m_tableNames.clear();
  }
  else
  {
    if (m_fragmentId != signal->getFragmentId())
    {
      abort();
    }
  }

  /*
    Save the count
   */
  m_noOfTables+= conf->noOfTables;

  bool fragmented = signal->isFragmented();
  Uint32 sigLen = signal->getLength() - 1;
  const Uint32 secs = signal->m_noOfSections;
  const Uint32 directMap[3] = {0,1,2};
  const Uint32 * const secNos =
    (fragmented) ?
    &signal->getDataPtr()[sigLen - secs]
    : (const Uint32 *) &directMap;

  for(Uint32 i = 0; i<secs; i++)
  {
    Uint32 sectionNo = secNos[i];
    switch (sectionNo) {
    case(ListTablesConf::TABLE_DATA):
      if (m_tableData.append(ptr[i].p, 4 * ptr[i].sz))
      {
        m_error.code= 4000;
        goto end;
      }
      break;
    case(ListTablesConf::TABLE_NAMES):
      if (m_tableNames.append(ptr[i].p, 4 * ptr[i].sz))
      {
        m_error.code= 4000;
        goto end;
      }
      break;
    default:
      abort();
    }
  }

 end:
  if(!signal->isLastFragment()){
    return;
  }

  m_impl->theWaiter.signal(NO_WAIT);
}


void
NdbDictInterface::execOLD_LIST_TABLES_CONF(const NdbApiSignal* signal,
                                           const LinearSectionPtr ptr[3])
{
  const unsigned off = OldListTablesConf::HeaderLength;
  const unsigned len = (signal->getLength() - off);
  if (m_buffer.append(signal->getDataPtr() + off, len << 2))
  {
    m_error.code= 4000;
  }
  if (signal->getLength() < OldListTablesConf::SignalLength) {
    // last signal has less than full length
    m_impl->theWaiter.signal(NO_WAIT);
  }
}

int
NdbDictionaryImpl::forceGCPWait(int type)
{
  return m_receiver.forceGCPWait(type);
}

int
NdbDictInterface::forceGCPWait(int type)
{
  NdbApiSignal tSignal(m_reference);
  if (type == 0 || type == 2)
  {
    WaitGCPReq* const req = CAST_PTR(WaitGCPReq, tSignal.getDataPtrSend());
    req->senderRef = m_reference;
    req->senderData = m_tx.nextRequestId();
    req->requestType = 
      type == 0 ? 
      WaitGCPReq::CompleteForceStart : WaitGCPReq::RestartGCI;
      
    tSignal.theReceiversBlockNumber = DBDIH;
    tSignal.theVerId_signalNumber = GSN_WAIT_GCP_REQ;
    tSignal.theLength = WaitGCPReq::SignalLength;

    const Uint32 RETRIES = 100;
    for (Uint32 i = 0; i < RETRIES; i++)
    {
      PollGuard pg(* m_impl);
      Uint16 aNodeId = getTransporter()->get_an_alive_node();
      if (aNodeId == 0) {
        if (getTransporter()->is_cluster_completely_unavailable())
        {
          m_error.code= 4009;
        }
        else
        {
          m_error.code = 4035;
        }
        return -1;
      }
      if (m_impl->sendSignal(&tSignal, aNodeId) != 0)
      {
        continue;
      }

      m_error.code= 0;
      
      m_impl->incClientStat(Ndb::WaitMetaRequestCount, 1);
      int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
      DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
        DBUG_PRINT("info", ("Reducing timeout for DICT GSN_WAIT_GCP_REQ"
                            " in NdbDictInterface::forceGCPWait()"));
        timeout = 1000;
      });

      int ret_val= pg.wait_n_unlock(timeout,
                                    aNodeId, WAIT_LIST_TABLES_CONF);
      // end protected
      if(m_error.code == 0 &&
         m_impl->theWaiter.get_state() == WST_WAIT_TIMEOUT)
      {
        DBUG_PRINT("info", ("wait_n_unlock caught time-out"));
        m_error.code = 4008;
        return -1;
      }

      if (ret_val == 0 && m_error.code == 0)
        return 0;
      if (ret_val == -2) //WAIT_NODE_FAILURE
        continue;
      return -1;
    }
    return -1;
  }
  else if (type == 1)
  {
    tSignal.getDataPtrSend()[0] = 6099;
    tSignal.theReceiversBlockNumber = DBDIH;
    tSignal.theVerId_signalNumber = GSN_DUMP_STATE_ORD;
    tSignal.theLength = 1;

    const Uint32 RETRIES = 100;
    for (Uint32 i = 0; i < RETRIES; i++)
    {
      m_impl->lock();
      Uint16 aNodeId = getTransporter()->get_an_alive_node();
      if (aNodeId == 0) {
        if (getTransporter()->is_cluster_completely_unavailable())
        {
          m_error.code= 4009;
        }
        else
        {
          m_error.code = 4035;
        }
        m_impl->unlock();
        return -1;
      }
      if (m_impl->sendSignal(&tSignal, aNodeId) != 0) {
        m_impl->unlock();
        continue;
      }

      m_impl->do_forceSend();
      m_impl->unlock();
    }
    return m_error.code == 0 ? 0 : -1;
  }
  else
  {
    m_error.code = 4003;
  }
  return -1;
}

int
NdbDictionaryImpl::getRestartGCI(Uint32 * gci)
{
  int res = m_receiver.forceGCPWait(2);
  if (res == 0 && gci != 0)
  {
    * gci = m_receiver.m_data.m_wait_gcp_conf.gci_hi;
  }
  return res;
}

void
NdbDictInterface::execWAIT_GCP_CONF(const NdbApiSignal* signal,
				    const LinearSectionPtr ptr[3])
{
  const WaitGCPConf* conf = CAST_CONSTPTR(WaitGCPConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "WAIT_GCP_CONF"))
    return; // signal from different (possibly timed-out) transaction

  m_data.m_wait_gcp_conf.gci_lo = conf->gci_lo;
  m_data.m_wait_gcp_conf.gci_hi = conf->gci_hi;
  m_impl->theWaiter.signal(NO_WAIT);
}

void
NdbDictInterface::execWAIT_GCP_REF(const NdbApiSignal* signal,
                                   const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::WAIT_GCP_REF");
  const WaitGCPRef* ref = CAST_CONSTPTR(WaitGCPRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "WAIT_GCP_REF"))
    return; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

NdbFilegroupImpl::NdbFilegroupImpl(NdbDictionary::Object::Type t)
  : NdbDictObjectImpl(t)
{
  m_extent_size = 0;
  m_undo_buffer_size = 0;
  m_logfile_group_id = RNIL;
  m_logfile_group_version = ~0;
}

NdbTablespaceImpl::NdbTablespaceImpl() : 
  NdbDictionary::Tablespace(* this), 
  NdbFilegroupImpl(NdbDictionary::Object::Tablespace), m_facade(this)
{
}

NdbTablespaceImpl::NdbTablespaceImpl(NdbDictionary::Tablespace & f) : 
  NdbDictionary::Tablespace(* this), 
  NdbFilegroupImpl(NdbDictionary::Object::Tablespace), m_facade(&f)
{
}

NdbTablespaceImpl::~NdbTablespaceImpl(){
}

int
NdbTablespaceImpl::assign(const NdbTablespaceImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  if (!m_name.assign(org.m_name))
    return -1;
  m_grow_spec = org.m_grow_spec;
  m_extent_size = org.m_extent_size;
  m_undo_free_words = org.m_undo_free_words;
  m_logfile_group_id = org.m_logfile_group_id;
  m_logfile_group_version = org.m_logfile_group_version;
  if (!m_logfile_group_name.assign(org.m_logfile_group_name))
    return -1;
  m_undo_free_words = org.m_undo_free_words;
  return 0;
}

NdbLogfileGroupImpl::NdbLogfileGroupImpl() : 
  NdbDictionary::LogfileGroup(* this), 
  NdbFilegroupImpl(NdbDictionary::Object::LogfileGroup), m_facade(this)
{
}

NdbLogfileGroupImpl::NdbLogfileGroupImpl(NdbDictionary::LogfileGroup & f) : 
  NdbDictionary::LogfileGroup(* this), 
  NdbFilegroupImpl(NdbDictionary::Object::LogfileGroup), m_facade(&f)
{
}

NdbLogfileGroupImpl::~NdbLogfileGroupImpl(){
}

int
NdbLogfileGroupImpl::assign(const NdbLogfileGroupImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  if (!m_name.assign(org.m_name))
    return -1;
  m_grow_spec = org.m_grow_spec;
  m_extent_size = org.m_extent_size;
  m_undo_free_words = org.m_undo_free_words;
  m_logfile_group_id = org.m_logfile_group_id;
  m_logfile_group_version = org.m_logfile_group_version;
  if (!m_logfile_group_name.assign(org.m_logfile_group_name))
    return -1;
  m_undo_free_words = org.m_undo_free_words;
  return 0;
}

NdbFileImpl::NdbFileImpl(NdbDictionary::Object::Type t)
  : NdbDictObjectImpl(t)
{
  m_size = 0;
  m_free = 0;
  m_filegroup_id = RNIL;
  m_filegroup_version = ~0;
}

NdbDatafileImpl::NdbDatafileImpl() : 
  NdbDictionary::Datafile(* this), 
  NdbFileImpl(NdbDictionary::Object::Datafile), m_facade(this)
{
}

NdbDatafileImpl::NdbDatafileImpl(NdbDictionary::Datafile & f) : 
  NdbDictionary::Datafile(* this), 
  NdbFileImpl(NdbDictionary::Object::Datafile), m_facade(&f)
{
}

NdbDatafileImpl::~NdbDatafileImpl(){
}

int
NdbDatafileImpl::assign(const NdbDatafileImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  m_size = org.m_size;
  m_free = org.m_free;
  m_filegroup_id = org.m_filegroup_id;
  m_filegroup_version = org.m_filegroup_version;
  if (!m_path.assign(org.m_path) ||
      !m_filegroup_name.assign(org.m_filegroup_name))
    return -1;
  return 0;
}

NdbUndofileImpl::NdbUndofileImpl() : 
  NdbDictionary::Undofile(* this), 
  NdbFileImpl(NdbDictionary::Object::Undofile), m_facade(this)
{
}

NdbUndofileImpl::NdbUndofileImpl(NdbDictionary::Undofile & f) : 
  NdbDictionary::Undofile(* this), 
  NdbFileImpl(NdbDictionary::Object::Undofile), m_facade(&f)
{
}

NdbUndofileImpl::~NdbUndofileImpl(){
}

int
NdbUndofileImpl::assign(const NdbUndofileImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  m_size = org.m_size;
  m_free = org.m_free;
  m_filegroup_id = org.m_filegroup_id;
  m_filegroup_version = org.m_filegroup_version;
  if (!m_path.assign(org.m_path) ||
      !m_filegroup_name.assign(org.m_filegroup_name))
    return 4000;
  return 0;
}

int 
NdbDictionaryImpl::createDatafile(const NdbDatafileImpl & file, 
				  bool force,
				  NdbDictObjectImpl* obj)
  
{
  DBUG_ENTER("NdbDictionaryImpl::createDatafile");
  NdbFilegroupImpl tmp(NdbDictionary::Object::Tablespace);
  if(file.m_filegroup_version != ~(Uint32)0){
    tmp.m_id = file.m_filegroup_id;
    tmp.m_version = file.m_filegroup_version;
    DBUG_RETURN(m_receiver.create_file(file, tmp, force, obj));
  }
  
  
  if(m_receiver.get_filegroup(tmp, NdbDictionary::Object::Tablespace,
			      file.m_filegroup_name.c_str()) == 0){
    DBUG_RETURN(m_receiver.create_file(file, tmp, force, obj));
  }
  DBUG_RETURN(-1); 
}

int
NdbDictionaryImpl::dropDatafile(const NdbDatafileImpl & file){
  return m_receiver.drop_file(file);
}

int
NdbDictionaryImpl::createUndofile(const NdbUndofileImpl & file, 
				  bool force,
				  NdbDictObjectImpl* obj)
{
  DBUG_ENTER("NdbDictionaryImpl::createUndofile");
  NdbFilegroupImpl tmp(NdbDictionary::Object::LogfileGroup);
  if(file.m_filegroup_version != ~(Uint32)0){
    tmp.m_id = file.m_filegroup_id;
    tmp.m_version = file.m_filegroup_version;
    DBUG_RETURN(m_receiver.create_file(file, tmp, force, obj));
  }
  
  
  if(m_receiver.get_filegroup(tmp, NdbDictionary::Object::LogfileGroup,
			      file.m_filegroup_name.c_str()) == 0){
    DBUG_RETURN(m_receiver.create_file(file, tmp, force, obj));
  }
  DBUG_PRINT("info", ("Failed to find filegroup"));
  if(m_error.code == 0)
    m_error.code = 789;
  DBUG_RETURN(-1);
}

int
NdbDictionaryImpl::dropUndofile(const NdbUndofileImpl & file)
{
  return m_receiver.drop_file(file);
}

int
NdbDictionaryImpl::createTablespace(const NdbTablespaceImpl & fg,
				    NdbDictObjectImpl* obj)
{
  return m_receiver.create_filegroup(fg, obj);
}

int
NdbDictionaryImpl::dropTablespace(const NdbTablespaceImpl & fg)
{
  return m_receiver.drop_filegroup(fg);
}

int
NdbDictionaryImpl::createLogfileGroup(const NdbLogfileGroupImpl & fg,
				      NdbDictObjectImpl* obj)
{
  return m_receiver.create_filegroup(fg, obj);
}

int
NdbDictionaryImpl::dropLogfileGroup(const NdbLogfileGroupImpl & fg)
{
  return m_receiver.drop_filegroup(fg);
}

static int
cmp_ndbrec_attr(const void *a, const void *b)
{
  const NdbRecord::Attr *r1= (const NdbRecord::Attr *)a;
  const NdbRecord::Attr *r2= (const NdbRecord::Attr *)b;
  if(r1->attrId < r2->attrId)
    return -1;
  else if(r1->attrId == r2->attrId)
    return 0;
  else
    return 1;
}

struct BitRange{
  Uint64 start; /* First occupied bit */
  Uint64 end; /* Last occupied bit */
};

static int
cmp_bitrange(const void* a, const void* b)
{
  /* Sort them by start bit */
  const BitRange& brA= *(const BitRange*)a;
  const BitRange& brB= *(const BitRange*)b;

  if (brA.start < brB.start)
    return -1;
  else if (brA.start == brB.start)
    return 0;
  else
    return 1;
}

bool
NdbDictionaryImpl::validateRecordSpec(const NdbDictionary::RecordSpecification *recSpec,
                                      Uint32 length,
                                      Uint32 flags) 
{
  /* We check that there's no overlap between any of the data values
   * or Null bits
   */
  
  /* Column data + NULL bits with at least 1 non nullable PK */
  const Uint32 MaxRecordElements= (2* NDB_MAX_ATTRIBUTES_IN_TABLE) - 1;
  Uint32 numElements= 0;
  BitRange bitRanges[ MaxRecordElements ];

  if (length > NDB_MAX_ATTRIBUTES_IN_TABLE)
  {
    m_error.code= 4548;
    return false;
  }
  
  /* Populate bitRanges array with ranges of bits occupied by 
   * data values and null bits
   */
  for (Uint32 rs=0; rs < length; rs++)
  {
    const NdbDictionary::Column* col= recSpec[rs].column;
    Uint64 elementByteOffset= recSpec[rs].offset;
    Uint64 elementByteLength= col->getSizeInBytesForRecord();
    Uint64 nullLength= col->getNullable() ? 1 : 0;

    /*
     Validate column flags
     1. Check if the column_flag has any invalid values
     2. If the BitColMapsNullBitOnly flag is enabled, RecMysqldBitfield
        should have been enabled and the column length should be 1
    */
    if((flags & NdbDictionary::RecPerColumnFlags) &&
       (recSpec[rs].column_flags &
           ~NdbDictionary::RecordSpecification::BitColMapsNullBitOnly) &&
       ((recSpec[rs].column_flags &
            NdbDictionary::RecordSpecification::BitColMapsNullBitOnly) &&
         !((col->getLength() == 1) &&
           (flags & NdbDictionary::RecMysqldBitfield))))
    {
      m_error.code= 4556;
      return false;
    }

    const NdbDictionary::Column::Type type= col->getType();
    if ((type == NdbDictionary::Column::Bit) &&
        (flags & NdbDictionary::RecMysqldBitfield))
    {
      if((flags & NdbDictionary::RecPerColumnFlags) &&
         (recSpec[rs].column_flags &
            NdbDictionary::RecordSpecification::BitColMapsNullBitOnly))
      {
        /* skip counting overflow bits */
        elementByteLength = 0;
      }
      else
      {
        /* MySQLD Bit format puts 'fractional' part of bit types 
         * in with the null bits - so there's 1 optional Null 
         * bit followed by n (max 7) databits, at position 
         * given by the nullbit offsets.  Then the rest of
         * the bytes go at the normal offset position.
         */
        Uint32 bitLength= col->getLength();
        Uint32 fractionalBits= bitLength % 8;
        nullLength+= fractionalBits;
        elementByteLength= bitLength / 8;
      }
    }

    /* Does the element itself have any bytes?
     * (MySQLD bit format may have all data as 'null bits'
     */
    if (elementByteLength)
    {
      bitRanges[numElements].start= 8 * elementByteOffset;
      bitRanges[numElements].end= (8 * (elementByteOffset + elementByteLength)) - 1;
      
      numElements++;
    }

    if (nullLength)
    {
      bitRanges[numElements].start= 
        (8* recSpec[rs].nullbit_byte_offset) + 
        recSpec[rs].nullbit_bit_in_byte;
      bitRanges[numElements].end= bitRanges[numElements].start + 
        (nullLength -1);

      numElements++;
    }
  }
  
  /* Now sort the 'elements' by start bit */
  qsort(bitRanges,
        numElements,
        sizeof(BitRange),
        cmp_bitrange);

  Uint64 endOfPreviousRange= bitRanges[0].end;

  /* Now check that there's no overlaps */
  for (Uint32 rangeNum= 1; rangeNum < numElements; rangeNum++)
  {
    if (unlikely((bitRanges[rangeNum].start <= endOfPreviousRange)))
    {
      /* Oops, this range overlaps with previous one */
      m_error.code= 4547;
      return false;
    }
    endOfPreviousRange= bitRanges[rangeNum].end;
  }

  /* All relevant ranges are distinct */
  return true;
}


/* ndb_set_record_specification
 * This procedure sets the contents of the passed RecordSpecification
 * for the given column in the given table.
 * The column is placed at the storageOffset given, and a new
 * storageOffset, beyond the end of this column, is returned.
 * Null bits are stored at the start of the row in consecutive positions.
 * The caller must ensure that enough space exists for all of the nullable
 * columns, before the first bit of data.
 * The new storageOffset is returned.
 */
static Uint32
ndb_set_record_specification(Uint32 storageOffset,
                             Uint32 field_num,
                             Uint32& nullableColNum,
                             NdbDictionary::RecordSpecification *spec,
                             NdbColumnImpl *col)
{
  spec->column= col->m_facade;

  spec->offset= storageOffset;
  /* For Blobs we just need the NdbBlob* */
  const Uint32 sizeOfElement= col->getBlobType() ? 
    sizeof(NdbBlob*) :
    spec->column->getSizeInBytes();
  
  if (spec->column->getNullable())
  {
    spec->nullbit_byte_offset= (nullableColNum >> 3);
    spec->nullbit_bit_in_byte= (nullableColNum & 7);
    nullableColNum ++;
  }
  else
  {
    /* For non-nullable columns, use visibly bad offsets */
    spec->nullbit_byte_offset= ~0;
    spec->nullbit_bit_in_byte= ~0;
  }

  return storageOffset + sizeOfElement;
}


/* This method creates an NdbRecord for the given table or index which
 * contains all columns (except pseudo columns).
 * For a table, only the tableOrIndex parameter should be supplied.
 * For an index, the index 'table object' should be supplied as the
 * tableOrIndex parameter, and the underlying indexed table object
 * should be supplied as the baseTableForIndex parameter.
 * The underlying table object is required to get the correct column
 * objects to build the NdbRecord object.
 * The record is created with all null bits packed together starting
 * from the first word, in attrId order, followed by all attributes
 * in attribute order.
 */
int
NdbDictionaryImpl::createDefaultNdbRecord(NdbTableImpl *tableOrIndex,
                                          const NdbTableImpl *baseTableForIndex)
{
  /* We create a full NdbRecord for the columns in the table 
   */
  DBUG_ENTER("NdbDictionaryImpl::createNdbRecords()");
  NdbDictionary::RecordSpecification spec[NDB_MAX_ATTRIBUTES_IN_TABLE];
  NdbRecord *rec;
  Uint32 i;
  Uint32 numCols= tableOrIndex->m_columns.size();
  // Reserve space for Null bits at the start
  Uint32 baseTabCols= numCols;
  unsigned char* pkMask= NULL;
  bool isIndex= false;

  if (baseTableForIndex != NULL)
  {
    /* Check we've really got an index */
    assert((tableOrIndex->m_indexType == NdbDictionary::Object::OrderedIndex ||
        tableOrIndex->m_indexType == NdbDictionary::Object::UniqueHashIndex));
        
    /* Update baseTabCols to real number of cols in indexed table */
    baseTabCols= baseTableForIndex->m_columns.size();

    /* Ignore extra info column at end of index table */
    numCols--; 

    isIndex= true;

    // Could do further string checks to make sure the base table and 
    // index are related
  }
  else
  {
    /* Check we've not got an index */
    assert((tableOrIndex->m_indexType != NdbDictionary::Object::OrderedIndex &&
         tableOrIndex->m_indexType != NdbDictionary::Object::UniqueHashIndex));
  }

  Uint32 nullableCols= 0;
  /* Determine number of nullable columns */
  for (i=0; i<numCols; i++)
  {
    /* As the Index NdbRecord is built using Columns from the base table,
     * it will get/set Null according to their Nullability.
     * If this is an index, then we need to take the 'Nullability' from
     * the base table column objects - unique index table column objects
     * will not be nullable as they are part of the key.
     */
    const NdbColumnImpl* col= NULL;
    
    if (isIndex)
    {
      Uint32 baseTableColNum= 
        tableOrIndex->m_index->m_columns[i]->m_keyInfoPos;
      col= baseTableForIndex->m_columns[baseTableColNum];
    }
    else
    {
      col= tableOrIndex->m_columns[i];
    }
    
    if (col->m_nullable)
      nullableCols ++;
  }

  /* Offset of first byte of data in the NdbRecord */
  Uint32 offset= (nullableCols+7) / 8;

  /* Allocate and zero column presence bitmasks */
  Uint32 bitMaskBytes= (baseTabCols + 7) / 8;
  pkMask=    (unsigned char*) calloc(1, bitMaskBytes);

  if (pkMask == NULL)
  {
    /* Memory allocation problem */
    m_error.code= 4000;
    return -1;
  }
  
  Uint32 nullableColNum= 0;

  /* Build record specification array for this table. */
  for (i= 0; i < numCols; i++)
  {
    /* Have to use columns from 'real' table for indexes as described
     * in NdbRecord documentation
     */
    NdbColumnImpl *col= NULL;

    if (isIndex)
    {
      /* From index table, get m_index pointer to NdbIndexImpl object.
       * m_index has m_key_ids[] array mapping index column numbers to
       * real table column numbers.
       * Use this number to get the correct column object from the
       * base table structure
       * No need to worry about Blobs here as Blob columns can't be
       * indexed
       */
      Uint32 baseTableColNum= 
        tableOrIndex->m_index->m_columns[i]->m_keyInfoPos;
      col= baseTableForIndex->m_columns[baseTableColNum];
      
      /* Set pk bitmask bit based on the base-table col number of this
       * column
       */
      assert( baseTableColNum < baseTabCols);
      pkMask[ baseTableColNum >> 3 ] |= ( 1 << ( baseTableColNum & 7 ));
    }
    else
    {
      col= tableOrIndex->m_columns[i];

      if (col->m_pk)
      {
        /* Set pk bitmask bit based on the col number of this column */
        pkMask[ i >> 3 ] |= ( 1 << (i & 7));
      }

      /* If this column's a Blob then we need to create
       * a default NdbRecord for the Blob table too
       * (unless it's a really small one with no parts table).
       */
      if (col->getBlobType() && col->getPartSize() != 0)
      {
        if (likely(col->m_blobTable != NULL))
        {
          int res= createDefaultNdbRecord(col->m_blobTable, NULL);
          if (res != 0)
          {
            free(pkMask);
            DBUG_RETURN(-1);
          }
        } 
        else
        {
          if (!ignore_broken_blob_tables())
          {
            assert(false);
            /* 4263 - Invalid blob attributes or invalid blob parts table */
            m_error.code = 4263;
            free(pkMask);
            DBUG_RETURN(-1);
          }
        }
      } 
    }

    offset= ndb_set_record_specification(offset, 
                                         i,
                                         nullableColNum,
                                         &spec[i], 
                                         col);
  }

  rec= createRecord(tableOrIndex, 
                    spec, 
                    numCols, 
                    sizeof(spec[0]), 
                    0,              // No special flags
                    true);          // default record
  if (rec == NULL)
  {
    free(pkMask);
    DBUG_RETURN(-1);
  }

  /* Store in the table definition */
  tableOrIndex->m_ndbrecord= rec;
  tableOrIndex->m_pkMask= pkMask;

  DBUG_RETURN(0);
}

/* This method initialises the data for a single
 * column in the passed NdbRecord structure
 */
int
NdbDictionaryImpl::initialiseColumnData(bool isIndex,
                                        Uint32 flags,
                                        const NdbDictionary::RecordSpecification *recSpec,
                                        Uint32 colNum,
                                        NdbRecord *rec)
{
  const NdbColumnImpl *col= &NdbColumnImpl::getImpl(*(recSpec->column));
  if (!col)
  {
    // Missing column specification in NdbDictionary::RecordSpecification
    m_error.code= 4290;
    return -1;
  }

  if (col->m_attrId & AttributeHeader::PSEUDO)
  {
    /* Pseudo columns not supported by NdbRecord */
    m_error.code= 4523;
    return -1;
  }

  if (col->m_indexSourced)
  {
    // Attempt to pass an index column to createRecord...
    m_error.code= 4540;
    return -1;
  }

  NdbRecord::Attr *recCol= &rec->columns[colNum];
  recCol->attrId= col->m_attrId;
  recCol->column_no= col->m_column_no;
  recCol->index_attrId= ~0;
  recCol->offset= recSpec->offset;
  recCol->maxSize= col->getSizeInBytesForRecord();
  recCol->orgAttrSize= col->m_orgAttrSize;
  if (recCol->offset+recCol->maxSize > rec->m_row_size)
    rec->m_row_size= recCol->offset+recCol->maxSize;
  recCol->charset_info= col->m_cs;
  recCol->compare_function= NdbSqlUtil::getType(col->m_type).m_cmp;
  recCol->flags= 0;
  if (!isIndex && col->m_pk)
    recCol->flags|= NdbRecord::IsKey;
  /* For indexes, we set key membership below. */
  if (col->m_storageType == NDB_STORAGETYPE_DISK)
    recCol->flags|= NdbRecord::IsDisk;
  if (col->m_nullable)
  {
    recCol->flags|= NdbRecord::IsNullable;
    recCol->nullbit_byte_offset= recSpec->nullbit_byte_offset;
    recCol->nullbit_bit_in_byte= recSpec->nullbit_bit_in_byte;

    const Uint32 nullbit_byte= recSpec->nullbit_byte_offset + 
      (recSpec->nullbit_bit_in_byte >> 3);
    if (nullbit_byte >= rec->m_row_size)
      rec->m_row_size= nullbit_byte + 1;
  }
  if (col->m_arrayType==NDB_ARRAYTYPE_SHORT_VAR)
  {
    recCol->flags|= NdbRecord::IsVar1ByteLen;
    if (flags & NdbDictionary::RecMysqldShrinkVarchar)
      recCol->flags|= NdbRecord::IsMysqldShrinkVarchar;
  }
  else if (col->m_arrayType==NDB_ARRAYTYPE_MEDIUM_VAR)
  {
    recCol->flags|= NdbRecord::IsVar2ByteLen;
  }
  if (col->m_type == NdbDictionary::Column::Bit)
  {
    recCol->bitCount= col->m_length;
    if (flags & NdbDictionary::RecMysqldBitfield)
    {
      recCol->flags|= NdbRecord::IsMysqldBitfield;
      if (!(col->m_nullable))
      {
        /*
          We need these to access the overflow bits stored within
          the null bitmap.
        */
        recCol->nullbit_byte_offset= recSpec->nullbit_byte_offset;
        recCol->nullbit_bit_in_byte= recSpec->nullbit_bit_in_byte;
      }
      if ((flags & NdbDictionary::RecPerColumnFlags) &&
          (recSpec->column_flags &
             NdbDictionary::RecordSpecification::BitColMapsNullBitOnly))
      {
        /* Bitfield maps only null bit values. No overflow bits*/
        recCol->flags|= NdbRecord::BitFieldMapsNullBitOnly;
      }
    }
  }
  else
    recCol->bitCount= 0;
  if (col->m_distributionKey)
    recCol->flags|= NdbRecord::IsDistributionKey;
  if (col->getBlobType())
  {
    recCol->flags|= NdbRecord::IsBlob;
    rec->flags|= NdbRecord::RecHasBlob;
  }
  return 0;
}

/**
 * createRecordInternal
 * Create an NdbRecord object using the table implementation and
 * RecordSpecification array passed.
 * The table pointer may be a proper table, or the underlying
 * table of an Index.  In any case, it is assumed that is is a
 * global table object, which may be safely shared between
 * multiple threads.  The responsibility for ensuring that it is
 * a global object rests with the caller. Called internally by
 * the createRecord method
 */
NdbRecord *
NdbDictionaryImpl::createRecordInternal(const NdbTableImpl *table,
                                        const NdbDictionary::RecordSpecification *recSpec,
                                        Uint32 length,
                                        Uint32 elemSize,
                                        Uint32 flags,
                                        bool defaultRecord)
{
  NdbRecord *rec= NULL;
  Uint32 numKeys, tableNumKeys, numIndexDistrKeys, min_distkey_prefix_length;
  Uint32 oldAttrId;
  bool isIndex;
  Uint32 i;

  if (!validateRecordSpec(recSpec, length, flags))
  {
    /* Error set in call */
    return NULL;
  }

  isIndex= (table->m_indexType==NdbDictionary::Object::OrderedIndex ||
            table->m_indexType==NdbDictionary::Object::UniqueHashIndex);

  /* Count the number of key columns in the table or index. */
  if (isIndex)
  {
    assert(table->m_index);
    /* Ignore the extra NDB$TNODE column at the end. */
    tableNumKeys= table->m_columns.size() - 1;
  }
  else
  {
    tableNumKeys= 0;
    for (i= 0; i<table->m_columns.size(); i++)
    {
      if (table->m_columns[i]->m_pk)
        tableNumKeys++;
    }
  }
  Uint32 tableNumDistKeys;
  if (isIndex || table->m_noOfDistributionKeys != 0)
    tableNumDistKeys= table->m_noOfDistributionKeys;
  else
    tableNumDistKeys= table->m_noOfKeys;

  int max_attrId = -1;
  for (i = 0; i < length; i++)
  {
    Uint32 attrId = recSpec[i].column->getAttrId();
    if ((int)attrId > max_attrId)
      max_attrId = (int)attrId;
  }
  Uint32 attrId_indexes_length = (Uint32)(max_attrId + 1);

  /*
    We need to allocate space for
     1. The struct itself.
     2. The columns[] array at the end of struct (length #columns).
     3. An extra Uint32 array key_indexes (length #key columns).
     4. An extra Uint32 array distkey_indexes (length #distribution keys).
     5. An extra int array attrId_indexes (length max attrId)
  */
  const Uint32 ndbRecBytes= sizeof(NdbRecord);
  const Uint32 colArrayBytes= length*sizeof(NdbRecord::Attr);
  const Uint32 tableKeyMapBytes= tableNumKeys*sizeof(Uint32);
  const Uint32 tableDistKeyMapBytes= tableNumDistKeys*sizeof(Uint32);
  const Uint32 attrIdMapBytes= (attrId_indexes_length + 1)*sizeof(int);
  rec= (NdbRecord *)calloc(1, ndbRecBytes +
                              colArrayBytes +
                              tableKeyMapBytes + 
                              tableDistKeyMapBytes + 
                              attrIdMapBytes);
  if (!rec)
  {
    m_error.code= 4000;
    return NULL;
  }
  Uint32 *key_indexes= (Uint32 *)((unsigned char *)rec + 
                                  ndbRecBytes + 
                                  colArrayBytes);
  Uint32 *distkey_indexes= (Uint32 *)((unsigned char *)rec + 
                                      ndbRecBytes + 
                                      colArrayBytes + 
                                      tableKeyMapBytes);
  int *attrId_indexes = (int *)((unsigned char *)rec + 
                                ndbRecBytes + 
                                colArrayBytes + 
                                tableKeyMapBytes + 
                                tableDistKeyMapBytes);
  /**
   * We overallocate one word of attribute index words. This is to be able
   * to speed up receive_packed_ndbrecord by reading ahead, the value we read
   * there will never be used, but to ensure we don't crash because of it we
   * allocate a word and set it to -1.
   */
  for (i = 0; i < (attrId_indexes_length + 1); i++)
  {
    attrId_indexes[i] = -1;
  }

  rec->table= table;
  rec->tableId= table->m_id;
  rec->tableVersion= table->m_version;
  rec->flags= 0;
  rec->noOfColumns= length;
  rec->m_no_of_distribution_keys= tableNumDistKeys;

  /* Check for any blobs in the base table. */
  for (i= 0; i<table->m_columns.size(); i++)
  {
    if (table->m_columns[i]->getBlobType())
    {
      rec->flags|= NdbRecord::RecTableHasBlob;
      break;
    }
  }

  rec->m_row_size= 0;
  for (i= 0; i<length; i++)
  {
    const NdbDictionary::RecordSpecification *rs= &recSpec[i];

    /* Initialise this column in NdbRecord from column
     * info
     */
    if (initialiseColumnData(isIndex,
                             flags,
                             rs,
                             i,
                             rec) != 0)
      goto err;

    /*
      Distibution key flag for unique index needs to be corrected
      to reflect the keys in the index base table
    */
    if (table->m_indexType == NdbDictionary::Object::UniqueHashIndex)
    {
      NdbRecord::Attr *recCol= &rec->columns[i];
      if (table->m_columns[i]->m_distributionKey)
        recCol->flags|= NdbRecord::IsDistributionKey;
      else
        recCol->flags&= ~NdbRecord::IsDistributionKey;
    }
  }

  /* Now we sort the array in attrId order. */
  qsort(rec->columns,
        rec->noOfColumns,
        sizeof(rec->columns[0]),
        cmp_ndbrec_attr);

  /*
    Now check for the presence of primary keys, and set flags for whether
    this NdbRecord can be used for insert and/or for specifying keys for
    read/update.

    Also test for duplicate columns, easy now that they are sorted.
    Also set up key_indexes array.
    Also compute if an index includes all of the distribution key.
    Also set up distkey_indexes array.
  */

  oldAttrId= ~0;
  numKeys= 0;
  min_distkey_prefix_length= 0;
  numIndexDistrKeys= 0;
  for (i= 0; i<rec->noOfColumns; i++)
  {
    NdbRecord::Attr *recCol= &rec->columns[i];
    if (i > 0 && oldAttrId==recCol->attrId)
    {
      m_error.code= 4291;
      goto err;
    }
    oldAttrId= recCol->attrId;

    assert(recCol->attrId < attrId_indexes_length);
    attrId_indexes[recCol->attrId] = i;

    if (isIndex)
    {
      Uint32 colNo= recCol->column_no;
      int key_idx;
      if (colNo < table->m_index->m_key_ids.size() &&
          (key_idx= table->m_index->m_key_ids[colNo]) != -1)
      {
        assert((Uint32)key_idx < tableNumKeys);
        recCol->flags|= NdbRecord::IsKey;
        key_indexes[key_idx]= i;
        recCol->index_attrId= table->m_columns[key_idx]->m_attrId;
        numKeys++;

        if (recCol->flags & NdbRecord::IsDistributionKey)
        {
          if (min_distkey_prefix_length <= (Uint32)key_idx)
            min_distkey_prefix_length= key_idx+1;
          if (numIndexDistrKeys < tableNumDistKeys)
            distkey_indexes[numIndexDistrKeys++]= i;
        }
      }
    }
    else
    {
      if (recCol->flags & NdbRecord::IsKey)
      {
        key_indexes[numKeys]= i;
        numKeys++;
      }
      if (recCol->flags & NdbRecord::IsDistributionKey)
      {
        if (numIndexDistrKeys < tableNumDistKeys)
          distkey_indexes[numIndexDistrKeys++]= i;
      }
    }
  }
  if (defaultRecord)
    rec->flags|= NdbRecord::RecIsDefaultRec;

  rec->key_indexes= key_indexes;
  rec->key_index_length= tableNumKeys;
  rec->m_min_distkey_prefix_length= min_distkey_prefix_length;
  rec->distkey_indexes= distkey_indexes;
  rec->distkey_index_length= numIndexDistrKeys;
  rec->m_attrId_indexes = attrId_indexes;
  rec->m_attrId_indexes_length = attrId_indexes_length;

  /*
    Since we checked for duplicates, we can check for primary key completeness
    simply by counting.
  */
  if (numKeys == tableNumKeys)
  {
    rec->flags|= NdbRecord::RecHasAllKeys;
    if (rec->noOfColumns == tableNumKeys)
      rec->flags|= NdbRecord::RecIsKeyRecord;
  }
  if (isIndex)
    rec->flags|= NdbRecord::RecIsIndex;
  rec->m_keyLenInWords= table->m_keyLenInWords;

  if (table->m_fragmentType == NdbDictionary::Object::UserDefined)
    rec->flags |= NdbRecord::RecHasUserDefinedPartitioning;

  return rec;

 err:
  if (rec)
    free(rec);
  return NULL;
}

/**
 * createRecord
 * Create an NdbRecord object using the table implementation and
 * RecordSpecification array passed.
 * The table pointer may be a proper table, or the underlying
 * table of an Index.  In any case, it is assumed that is is a
 * global table object, which may be safely shared between
 * multiple threads.  The responsibility for ensuring that it is
 * a global object rests with the caller. Method validates the
 * version of the sent RecordSpecification instance, maps it to
 * a newer version if necessary and internally calls
 * createRecordInternal to do the processing
 */
NdbRecord *
NdbDictionaryImpl::createRecord(const NdbTableImpl *table,
                                const NdbDictionary::RecordSpecification *recSpec,
                                Uint32 length,
                                Uint32 elemSize,
                                Uint32 flags,
                                bool defaultRecord)
{
  NdbDictionary::RecordSpecification *newRecordSpec = NULL;

  /* Check if recSpec is an instance of the newer version */
  if (elemSize != sizeof(NdbDictionary::RecordSpecification))
  {
    if(elemSize == sizeof(NdbDictionary::RecordSpecification_v1))
    {
      /*
        Older RecordSpecification in use.
        Map it to an instance of newer version.
      */
      const NdbDictionary::RecordSpecification_v1* oldRecordSpec =
          (const NdbDictionary::RecordSpecification_v1*) recSpec;

      newRecordSpec = (NdbDictionary::RecordSpecification*)
                         malloc(length * sizeof(NdbDictionary::RecordSpecification));
      if(newRecordSpec == NULL)
      {
        m_error.code= 4000;
        return NULL;
      }
      for (Uint32 i= 0; i < length; i++)
      {
        /* map values from older version to newer version */
        newRecordSpec[i].column = oldRecordSpec[i].column;
        newRecordSpec[i].offset = oldRecordSpec[i].offset;
        newRecordSpec[i].nullbit_byte_offset =
            oldRecordSpec[i].nullbit_byte_offset;
        newRecordSpec[i].nullbit_bit_in_byte =
            oldRecordSpec[i].nullbit_bit_in_byte;
        newRecordSpec[i].column_flags = 0;
      }
      recSpec = &newRecordSpec[0];
    }
    else
    {
      m_error.code= 4289;
      return NULL;
    }
  }
  NdbRecord *ndbRec = createRecordInternal(table,
                                           recSpec,
                                           length,
                                           elemSize,
                                           flags,
                                           defaultRecord);
  free(newRecordSpec);
  return ndbRec;
}

void
NdbRecord::copyMask(Uint32 *dst, const unsigned char *src) const
{
  Uint32 i;

  BitmaskImpl::clear((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5, dst);
  if (src)
  {
    for (i= 0; i<noOfColumns; i++)
    {
      Uint32 attrId= columns[i].attrId;

      assert(!(attrId & AttributeHeader::PSEUDO));

      if (src[attrId>>3] & (1 << (attrId&7)))
        BitmaskImpl::set((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5, dst, attrId);
    }
  }
  else
  {
    for (i= 0; i<noOfColumns; i++)
    {
      Uint32 attrId= columns[i].attrId;
      
      assert(!(attrId & AttributeHeader::PSEUDO));

      BitmaskImpl::set((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5, dst, attrId);
    }
  }
}

void
NdbRecord::Attr::get_mysqld_bitfield(const char *src_row, char *dst_buffer) const
{
  assert(flags & IsMysqldBitfield);
  Uint64 bits;
  Uint32 remaining_bits= bitCount;
  Uint32 fractional_bitcount= remaining_bits % 8;

  /* Copy fractional bits, if any. */
  if (fractional_bitcount > 0 &&
      !(flags & BitFieldMapsNullBitOnly))
  {
    Uint32 fractional_shift= nullbit_bit_in_byte + ((flags & IsNullable) != 0);
    Uint32 fractional_bits= (unsigned char)(src_row[nullbit_byte_offset]);
    if (fractional_shift + fractional_bitcount > 8)
      fractional_bits|= (unsigned char)(src_row[nullbit_byte_offset+1]) << 8;
    fractional_bits=
      (fractional_bits >> fractional_shift) & ((1 << fractional_bitcount) - 1);
    bits= fractional_bits;
  }
  else
    bits= 0;

  /* Copy whole bytes. The mysqld format stored bit fields big-endian. */
  assert(remaining_bits <= 64);
  const unsigned char *src_ptr= (const unsigned char *)&src_row[offset];
  while (remaining_bits >= 8)
  {
    bits= (bits << 8) | (*src_ptr++);
    remaining_bits-= 8;
  }

  Uint32 small_bits= (Uint32)bits;
  memcpy(dst_buffer, &small_bits, 4);
  if (maxSize > 4)
  {
    small_bits= (Uint32)(bits >> 32);
    memcpy(dst_buffer+4, &small_bits, 4);
  }
}

void
NdbRecord::Attr::put_mysqld_bitfield(char *dst_row, const char *src_buffer) const
{
  assert(flags & IsMysqldBitfield);
  char *dst_ptr= &dst_row[offset];
  Uint64 bits;
  Uint32 small_bits;
  memcpy(&small_bits, src_buffer, 4);
  bits= small_bits;
  if (maxSize > 4)
  {
    memcpy(&small_bits, src_buffer+4, 4);
    bits|= ((Uint64)small_bits) << 32;
  }

  /* Copy whole bytes. The mysqld format stores bitfields big-endian. */
  Uint32 remaining_bits= bitCount;
  assert(remaining_bits <= 64);
  dst_ptr+= remaining_bits/8;
  while (remaining_bits >= 8)
  {
    *--dst_ptr= (char)(bits & 0xff);
    bits>>= 8;
    remaining_bits-= 8;
  }

  /* Copy fractional bits, if any. */
  if (remaining_bits > 0 &&
      !(flags & BitFieldMapsNullBitOnly))
  {
    Uint32 shift= nullbit_bit_in_byte + ((flags & IsNullable) != 0);
    Uint32 mask= ((1 << remaining_bits) - 1) << shift;
    bits= (bits << shift) & mask;
    dst_row[nullbit_byte_offset]=
      Uint8((dst_row[nullbit_byte_offset] & ~mask) | bits);
    if (shift + remaining_bits > 8)
    {
      mask>>= 8;
      bits>>= 8;
      dst_row[nullbit_byte_offset+1]=
        Uint8((dst_row[nullbit_byte_offset+1] & ~mask) | bits);
    }
  }
}

void NdbDictionaryImpl::releaseRecord_impl(NdbRecord *rec)
{
  if (rec)
  {
    /* Silently do nothing if they've passed the default
     * record in (similar to null handling behaviour)
     */
    if (!(rec->flags & NdbRecord::RecIsDefaultRec))
    {
      /* For non-default records, we need to release the
       * global table / index reference 
       */
      if (rec->flags & NdbRecord::RecIsIndex)
        releaseIndexGlobal(*rec->table->m_index, 
                           false); // Don't invalidate
      else
        releaseTableGlobal(*rec->table, 
                           false); // Don't invalidate
      
      free(rec);
    }
  }
}

NdbDictionary::RecordType
NdbDictionaryImpl::getRecordType(const NdbRecord* record)
{
  if (record->flags & NdbRecord::RecIsIndex)
    return NdbDictionary::IndexAccess;
  else
    return NdbDictionary::TableAccess;
}

const char*
NdbDictionaryImpl::getRecordTableName(const NdbRecord* record)
{
  if (!(record->flags & NdbRecord::RecIsIndex))
  {
    return record->table->m_externalName.c_str();
  }
  
  return NULL;
}

const char*
NdbDictionaryImpl::getRecordIndexName(const NdbRecord* record)
{
  if (record->flags & NdbRecord::RecIsIndex)
  {
    assert(record->table->m_index != NULL);
    assert(record->table->m_index->m_facade != NULL);

    return record->table->m_index->m_externalName.c_str();
  }

  return NULL;
}

bool
NdbDictionaryImpl::getNextAttrIdFrom(const NdbRecord* record,
                                     Uint32 startAttrId,
                                     Uint32& nextAttrId)
{
  for (Uint32 i= startAttrId; i < record->m_attrId_indexes_length; i++)
  {
    if (record->m_attrId_indexes[i] != -1)
    {
      nextAttrId= i;
      return true;
    }
  }
  return false; 
}

bool
NdbDictionaryImpl::getOffset(const NdbRecord* record,
                             Uint32 attrId,
                             Uint32& offset)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int) record->noOfColumns);

      offset= record->columns[attrIdIndex].offset;
      return true;
    }
  }
  
  /* AttrId not part of this NdbRecord */
  return false;
}

bool
NdbDictionaryImpl::getNullBitOffset(const NdbRecord* record,
                                    Uint32 attrId,
                                    Uint32& nullbit_byte_offset,
                                    Uint32& nullbit_bit_in_byte)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int) record->noOfColumns);

      NdbRecord::Attr attr= record->columns[attrIdIndex];

      nullbit_byte_offset= attr.nullbit_byte_offset;
      nullbit_bit_in_byte= attr.nullbit_bit_in_byte;
      return true;
    }
  }
  
  /* AttrId not part of this NdbRecord */
  return false;
}

const char*
NdbDictionaryImpl::getValuePtr(const NdbRecord* record,
                               const char* row,
                               Uint32 attrId)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int) record->noOfColumns);

      return row + (record->columns[attrIdIndex].offset);
    }
  }
  
  /* AttrId not part of this NdbRecord */
  return NULL;
}

char*
NdbDictionaryImpl::getValuePtr(const NdbRecord* record,
                               char* row,
                               Uint32 attrId)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int)record->noOfColumns);

      return row + (record->columns[attrIdIndex].offset);
    }
  }
  
  /* AttrId not part of this NdbRecord */
  return NULL;
}

bool
NdbDictionaryImpl::isNull(const NdbRecord* record,
                          const char* row,
                          Uint32 attrId)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int)record->noOfColumns);
      return record->columns[attrIdIndex].is_null(row);
    }
  }
  
  /* AttrId not part of this NdbRecord or is not nullable */
  return false;
}

int
NdbDictionaryImpl::setNull(const NdbRecord* record,
                           char* row,
                           Uint32 attrId,
                           bool value)
{
  if (attrId < record->m_attrId_indexes_length)
  {
    int attrIdIndex= record->m_attrId_indexes[attrId];
    
    if (attrIdIndex != -1)
    {
      assert(attrIdIndex < (int)record->noOfColumns);
      NdbRecord::Attr attr= record->columns[attrIdIndex];
      
      if (attr.flags & NdbRecord::IsNullable)
      {
        if (value)
          *(row + attr.nullbit_byte_offset) |= 
            (1 << attr.nullbit_bit_in_byte);
        else
          *(row + attr.nullbit_byte_offset) &=
            ~(1 << attr.nullbit_bit_in_byte);
        
        return 0;
      }
    }
  }
  
  /* AttrId not part of this NdbRecord or is not nullable */
  return -1;
}

Uint32
NdbDictionaryImpl::getRecordRowLength(const NdbRecord* record)
{
  return record->m_row_size;
}



int
NdbDictInterface::create_file(const NdbFileImpl & file,
			      const NdbFilegroupImpl & group,
			      bool overwrite,
			      NdbDictObjectImpl* obj)
{
  DBUG_ENTER("NdbDictInterface::create_file"); 
  UtilBufferWriter w(m_buffer);
  DictFilegroupInfo::File f; f.init();
  BaseString::snprintf(f.FileName, sizeof(f.FileName), "%s", file.m_path.c_str());
  f.FileType = file.m_type;
  f.FilegroupId = group.m_id;
  f.FilegroupVersion = group.m_version;
  f.FileSizeHi = (Uint32)(file.m_size >> 32);
  f.FileSizeLo = (Uint32)(file.m_size & 0xFFFFFFFF);
  
  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w, 
			     &f,
			     DictFilegroupInfo::FileMapping, 
			     DictFilegroupInfo::FileMappingSize, true);
  
  if(s != SimpleProperties::Eof){
    abort();
  }
  
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_CREATE_FILE_REQ;
  tSignal.theLength = CreateFileReq::SignalLength;
  
  CreateFileReq* req = CAST_PTR(CreateFileReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->objType = file.m_type;
  req->requestInfo = 0;
  if (overwrite)
    req->requestInfo |= CreateFileReq::ForceCreateFile;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[] = { CreateFileRef::Busy, CreateFileRef::NotMaster, 0};
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_CREATE_FILE_REQ"
                        " in NdbDictInterface::create_file()"));
    timeout = 1000;
  });
  int ret = dictSignal(&tSignal, ptr, 1,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       timeout, 100,
		       err);

  if (ret == 0)
  {
    Uint32* data = (Uint32*)m_buffer.get_data();
    if (obj)
    {
      obj->m_id = data[0];
      obj->m_version = data[1];
    }
    m_warn = data[2];
    DBUG_PRINT("info", ("warning flags: 0x%x", m_warn));
  }

  DBUG_RETURN(ret);
}

void
NdbDictInterface::execCREATE_FILE_CONF(const NdbApiSignal * signal,
				       const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_FILE_CONF");
  const CreateFileConf* conf=
    CAST_CONSTPTR(CreateFileConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "CREATE_FILE_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_buffer.grow(4 * 3); // 3 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->fileId;
  data[1] = conf->fileVersion;
  data[2] = conf->warningFlags;
  
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_FILE_REF(const NdbApiSignal * signal,
				      const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_FILE_REF");
  const CreateFileRef* ref = 
    CAST_CONSTPTR(CreateFileRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "CREATE_FILE_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

int
NdbDictInterface::drop_file(const NdbFileImpl & file)
{
  DBUG_ENTER("NdbDictInterface::drop_file");
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_DROP_FILE_REQ;
  tSignal.theLength = DropFileReq::SignalLength;
  
  DropFileReq* req = CAST_PTR(DropFileReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->file_id = file.m_id;
  req->file_version = file.m_version;
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();

  int err[] = { DropFileRef::Busy, DropFileRef::NotMaster, 0};

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_DROP_FILE_REQ"
                        " in NdbDictInterface::drop_file()"));
    timeout = 1000;
  });

  DBUG_RETURN(dictSignal(&tSignal, 0, 0,
	                 0, // master
		         WAIT_CREATE_INDX_REQ,
		         timeout, 100,
		         err));
}

void
NdbDictInterface::execDROP_FILE_CONF(const NdbApiSignal * signal,
					    const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FILE_CONF");
  const DropFileConf* conf =
    CAST_CONSTPTR(DropFileConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "DROP_FILE_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_FILE_REF(const NdbApiSignal * signal,
					   const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FILE_REF");
  const DropFileRef* ref = 
    CAST_CONSTPTR(DropFileRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "DROP_FILE_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

int
NdbDictInterface::create_filegroup(const NdbFilegroupImpl & group,
				   NdbDictObjectImpl* obj)
{
  DBUG_ENTER("NdbDictInterface::create_filegroup");
  UtilBufferWriter w(m_buffer);
  DictFilegroupInfo::Filegroup fg; fg.init();
  BaseString::snprintf(fg.FilegroupName, sizeof(fg.FilegroupName),
           "%s", group.m_name.c_str());
  switch(group.m_type){
  case NdbDictionary::Object::Tablespace:
  {
    fg.FilegroupType = DictTabInfo::Tablespace;
    //fg.TS_DataGrow = group.m_grow_spec;
    fg.TS_ExtentSize = group.m_extent_size;

    if(group.m_logfile_group_version != ~(Uint32)0)
    {
      fg.TS_LogfileGroupId = group.m_logfile_group_id;
      fg.TS_LogfileGroupVersion = group.m_logfile_group_version;
    }
    else 
    {
      NdbLogfileGroupImpl tmp;
      if(get_filegroup(tmp, NdbDictionary::Object::LogfileGroup, 
		       group.m_logfile_group_name.c_str()) == 0)
      {
	fg.TS_LogfileGroupId = tmp.m_id;
	fg.TS_LogfileGroupVersion = tmp.m_version;
      }
      else // error set by get filegroup
      {
	DBUG_RETURN(-1);
      }
    }
  }
  break;
  case NdbDictionary::Object::LogfileGroup:
    fg.LF_UndoBufferSize = group.m_undo_buffer_size;
    fg.FilegroupType = DictTabInfo::LogfileGroup;
    //fg.LF_UndoGrow = group.m_grow_spec;
    break;
  default:
    abort();
    DBUG_RETURN(-1);
  };
  
  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w, 
			     &fg,
			     DictFilegroupInfo::Mapping, 
			     DictFilegroupInfo::MappingSize, true);
  
  if(s != SimpleProperties::Eof){
    abort();
  }
  
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_CREATE_FILEGROUP_REQ;
  tSignal.theLength = CreateFilegroupReq::SignalLength;
  
  CreateFilegroupReq* req = 
    CAST_PTR(CreateFilegroupReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->objType = fg.FilegroupType;
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[] = { CreateFilegroupRef::Busy, CreateFilegroupRef::NotMaster, 0};
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for GSN_CREATE_FILEGROUP_REQ"
                        " in NdbDictInterface::create_filegroup()"));
    timeout = 1000;
  });
  int ret = dictSignal(&tSignal, ptr, 1,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       timeout, 100,
		       err);
  
  if (ret == 0)
  {
    Uint32* data = (Uint32*)m_buffer.get_data();
    if (obj)
    {
      obj->m_id = data[0];
      obj->m_version = data[1];
    }
    m_warn = data[2];
    DBUG_PRINT("info", ("warning flags: 0x%x", m_warn));
  }
  
  DBUG_RETURN(ret);
}

void
NdbDictInterface::execCREATE_FILEGROUP_CONF(const NdbApiSignal * signal,
					    const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("execCREATE_FILEGROUP_CONF");
  const CreateFilegroupConf* conf=
    CAST_CONSTPTR(CreateFilegroupConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "CREATE_FILEGROUP_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_buffer.grow(4 * 3); // 3 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->filegroupId;
  data[1] = conf->filegroupVersion;
  data[2] = conf->warningFlags;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_FILEGROUP_REF(const NdbApiSignal * signal,
					   const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_FILEGROUP_REF");
  const CreateFilegroupRef* ref = 
    CAST_CONSTPTR(CreateFilegroupRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "CREATE_FILEGROUP_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

int
NdbDictInterface::drop_filegroup(const NdbFilegroupImpl & group)
{
  DBUG_ENTER("NdbDictInterface::drop_filegroup");
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_DROP_FILEGROUP_REQ;
  tSignal.theLength = DropFilegroupReq::SignalLength;
  
  DropFilegroupReq* req = CAST_PTR(DropFilegroupReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->filegroup_id = group.m_id;
  req->filegroup_version = group.m_version;
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();

  int err[] = { DropFilegroupRef::Busy, DropFilegroupRef::NotMaster, 0};
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for GSN_DROP_FILEGROUP_REQ"
                        " in NdbDictInterface::drop_filegroup()"));
    timeout = 1000;
  });
  DBUG_RETURN(dictSignal(&tSignal, 0, 0,
                         0, // master
		         WAIT_CREATE_INDX_REQ,
		         timeout, 100,
		         err));
}

void
NdbDictInterface::execDROP_FILEGROUP_CONF(const NdbApiSignal * signal,
					    const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FILEGROUP_CONF");
  const DropFilegroupConf* conf =
    CAST_CONSTPTR(DropFilegroupConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "DROP_FILEGROUP_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_FILEGROUP_REF(const NdbApiSignal * signal,
					   const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FILEGROUP_REF");
  const DropFilegroupRef* ref = 
    CAST_CONSTPTR(DropFilegroupRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "DROP_FILEGROUP_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}


int
NdbDictInterface::get_filegroup(NdbFilegroupImpl & dst,
				NdbDictionary::Object::Type type,
				const char * name){
  DBUG_ENTER("NdbDictInterface::get_filegroup");
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  Uint32 strLen = (Uint32)strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType = 
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  LinearSectionPtr ptr[1];
  ptr[0].p  = (Uint32*)name;
  ptr[0].sz = (strLen + 3)/4;
  
#ifndef IGNORE_VALGRIND_WARNINGS
  if (strLen & 3)
  {
    Uint32 pad = 0;
    m_buffer.clear();
    m_buffer.append(name, strLen);
    m_buffer.append(&pad, 4);
    ptr[0].p = (Uint32*)m_buffer.get_data();
  }
#endif
  
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_filegroup()"));
    timeout = 1000;
  });

  int r = dictSignal(&tSignal, ptr, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100);
  if (r)
  {
    dst.m_id = RNIL;
    dst.m_version = ~0;
    
    DBUG_PRINT("info", ("get_filegroup failed dictSignal"));
    DBUG_RETURN(-1);
  }

  m_error.code = parseFilegroupInfo(dst,
				    (Uint32*)m_buffer.get_data(),
				    m_buffer.length() / 4);

  if(m_error.code)
  {
    DBUG_PRINT("info", ("get_filegroup failed parseFilegroupInfo %d",
                         m_error.code));
    DBUG_RETURN(m_error.code);
  }

  if(dst.m_type == NdbDictionary::Object::Tablespace)
  {
    NdbDictionary::LogfileGroup tmp;
    get_filegroup(NdbLogfileGroupImpl::getImpl(tmp),
		  NdbDictionary::Object::LogfileGroup,
		  dst.m_logfile_group_id);
    if (!dst.m_logfile_group_name.assign(tmp.getName()))
      DBUG_RETURN(m_error.code = 4000);
  }
  
  if(dst.m_type == type)
  {
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("get_filegroup failed no such filegroup"));
  DBUG_RETURN(m_error.code = GetTabInfoRef::TableNotDefined);
}

int
NdbDictInterface::parseFilegroupInfo(NdbFilegroupImpl &dst,
				     const Uint32 * data, Uint32 len)
  
{
  SimplePropertiesLinearReader it(data, len);

  SimpleProperties::UnpackStatus status;
  DictFilegroupInfo::Filegroup fg; fg.init();
  status = SimpleProperties::unpack(it, &fg, 
				    DictFilegroupInfo::Mapping, 
				    DictFilegroupInfo::MappingSize, 
				    true, true);
  
  if(status != SimpleProperties::Eof){
    return CreateFilegroupRef::InvalidFormat;
  }

  dst.m_id = fg.FilegroupId;
  dst.m_version = fg.FilegroupVersion;
  dst.m_type = (NdbDictionary::Object::Type)fg.FilegroupType;
  dst.m_status = NdbDictionary::Object::Retrieved;
  
  if (!dst.m_name.assign(fg.FilegroupName))
    return 4000;
  dst.m_extent_size = fg.TS_ExtentSize;
  dst.m_undo_buffer_size = fg.LF_UndoBufferSize;
  dst.m_logfile_group_id = fg.TS_LogfileGroupId;
  dst.m_logfile_group_version = fg.TS_LogfileGroupVersion;
  dst.m_undo_free_words= ((Uint64)fg.LF_UndoFreeWordsHi << 32)
    | (fg.LF_UndoFreeWordsLo);

  return 0;
}

int
NdbDictInterface::get_filegroup(NdbFilegroupImpl & dst,
				NdbDictionary::Object::Type type,
				Uint32 id){
  DBUG_ENTER("NdbDictInterface::get_filegroup");
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = id;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_filegroup()"));
    timeout = 1000;
  });

  int r = dictSignal(&tSignal, NULL, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100);
  if (r)
  {
    DBUG_PRINT("info", ("get_filegroup failed dictSignal"));
    DBUG_RETURN(-1);
  }

  m_error.code = parseFilegroupInfo(dst,
				    (Uint32*)m_buffer.get_data(),
				    m_buffer.length() / 4);

  if(m_error.code)
  {
    DBUG_PRINT("info", ("get_filegroup failed parseFilegroupInfo %d",
                         m_error.code));
    DBUG_RETURN(m_error.code);
  }

  if(dst.m_type == type)
  {
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("get_filegroup failed no such filegroup"));
  DBUG_RETURN(m_error.code = GetTabInfoRef::TableNotDefined);
}

int
NdbDictInterface::get_file(NdbFileImpl & dst,
			   NdbDictionary::Object::Type type,
			   int node,
			   const char * name){
  DBUG_ENTER("NdbDictInterface::get_file");
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  Uint32 strLen = (Uint32)strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  LinearSectionPtr ptr[1];
  ptr[0].p  = (Uint32*)name;
  ptr[0].sz = (strLen + 3)/4;
  
#ifndef IGNORE_VALGRIND_WARNINGS
  if (strLen & 3)
  {
    Uint32 pad = 0;
    m_buffer.clear();
    m_buffer.append(name, strLen);
    m_buffer.append(&pad, 4);
    ptr[0].p = (Uint32*)m_buffer.get_data();
  }
#endif
  
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_file()"));
    timeout = 1000;
  });

  int r = dictSignal(&tSignal, ptr, 1,
		     node,
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100);
  if (r)
  {
    DBUG_PRINT("info", ("get_file failed dictSignal"));
    DBUG_RETURN(-1);
  }

  m_error.code = parseFileInfo(dst,
			       (Uint32*)m_buffer.get_data(),
			       m_buffer.length() / 4);

  if(m_error.code)
  {
    DBUG_PRINT("info", ("get_file failed parseFileInfo %d",
                         m_error.code));
    DBUG_RETURN(m_error.code);
  }

  if(dst.m_type == NdbDictionary::Object::Undofile)
  {
    NdbDictionary::LogfileGroup tmp;
    get_filegroup(NdbLogfileGroupImpl::getImpl(tmp),
		  NdbDictionary::Object::LogfileGroup,
		  dst.m_filegroup_id);
    if (!dst.m_filegroup_name.assign(tmp.getName()))
      DBUG_RETURN(m_error.code = 4000);
  }
  else if(dst.m_type == NdbDictionary::Object::Datafile)
  {
    NdbDictionary::Tablespace tmp;
    get_filegroup(NdbTablespaceImpl::getImpl(tmp),
		  NdbDictionary::Object::Tablespace,
		  dst.m_filegroup_id);
    if (!dst.m_filegroup_name.assign(tmp.getName()))
      DBUG_RETURN(m_error.code = 4000);
    dst.m_free *= tmp.getExtentSize();
  }
  else
    dst.m_filegroup_name.assign("Not Yet Implemented");
  
  if(dst.m_type == type)
  {
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("get_file failed no such file"));
  DBUG_RETURN(m_error.code = GetTabInfoRef::TableNotDefined);
}

int
NdbDictInterface::parseFileInfo(NdbFileImpl &dst,
				const Uint32 * data, Uint32 len)
{
  SimplePropertiesLinearReader it(data, len);

  SimpleProperties::UnpackStatus status;
  DictFilegroupInfo::File f; f.init();
  status = SimpleProperties::unpack(it, &f,
				    DictFilegroupInfo::FileMapping,
				    DictFilegroupInfo::FileMappingSize,
				    true, true);

  if(status != SimpleProperties::Eof){
    return CreateFilegroupRef::InvalidFormat;
  }

  dst.m_type= (NdbDictionary::Object::Type)f.FileType;
  dst.m_id= f.FileId;
  dst.m_version = f.FileVersion;

  dst.m_size= ((Uint64)f.FileSizeHi << 32) | (f.FileSizeLo);
  if (!dst.m_path.assign(f.FileName))
    return 4000;

  dst.m_filegroup_id= f.FilegroupId;
  dst.m_filegroup_version= f.FilegroupVersion;
  dst.m_free=  f.FileFreeExtents;
  return 0;
}

/**
 * HashMap
 */

NdbHashMapImpl::NdbHashMapImpl()
  : NdbDictionary::HashMap(* this),
    NdbDictObjectImpl(NdbDictionary::Object::HashMap), m_facade(this)
{
  m_id = RNIL;
  m_version = ~Uint32(0);
}

NdbHashMapImpl::NdbHashMapImpl(NdbDictionary::HashMap & f)
  : NdbDictionary::HashMap(* this),
    NdbDictObjectImpl(NdbDictionary::Object::HashMap), m_facade(&f)
{
  m_id = RNIL;
  m_version = ~Uint32(0);
}

NdbHashMapImpl::~NdbHashMapImpl()
{
}

int
NdbHashMapImpl::assign(const NdbHashMapImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;

  m_name.assign(org.m_name);
  m_map.assign(org.m_map);

  return 0;
}

int
NdbDictInterface::get_hashmap(NdbHashMapImpl & dst,
                              const char * name)
{
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  Uint32 strLen = (Uint32)strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  LinearSectionPtr ptr[1];
  ptr[0].p  = (Uint32*)name;
  ptr[0].sz = (strLen + 3)/4;

#ifndef IGNORE_VALGRIND_WARNINGS
  if (strLen & 3)
  {
    Uint32 pad = 0;
    m_buffer.clear();
    m_buffer.append(name, strLen);
    m_buffer.append(&pad, 4);
    ptr[0].p = (Uint32*)m_buffer.get_data();
  }
#endif

  int errCodes[] = {GetTabInfoRef::Busy, 0 };
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_hashmap"));
    timeout = 1000;
  });
  int r = dictSignal(&tSignal, ptr, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100, errCodes);
  if (r)
  {
    dst.m_id = -1;
    dst.m_version = ~0;

    return -1;
  }

  m_error.code = parseHashMapInfo(dst,
                                  (Uint32*)m_buffer.get_data(),
                                  m_buffer.length() / 4);

  return m_error.code;
}

int
NdbDictInterface::get_hashmap(NdbHashMapImpl & dst,
                              Uint32 id)
{
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = id;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  int errCodes[] = {GetTabInfoRef::Busy, 0 };
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_hashmap()"));
    timeout = 1000;
  });
  int r = dictSignal(&tSignal, 0, 0,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100, errCodes);
  if (r)
  {
    dst.m_id = -1;
    dst.m_version = ~0;

    return -1;
  }

  m_error.code = parseHashMapInfo(dst,
                                  (Uint32*)m_buffer.get_data(),
                                  m_buffer.length() / 4);

  return m_error.code;
}

int
NdbDictInterface::parseHashMapInfo(NdbHashMapImpl &dst,
                                   const Uint32 * data, Uint32 len)
{
  SimplePropertiesLinearReader it(data, len);

  SimpleProperties::UnpackStatus status;
  DictHashMapInfo::HashMap* hm = new DictHashMapInfo::HashMap();
  hm->init();
  status = SimpleProperties::unpack(it, hm,
                                    DictHashMapInfo::Mapping,
                                    DictHashMapInfo::MappingSize,
                                    true, true);

  if(status != SimpleProperties::Eof){
    delete hm;
    return CreateFilegroupRef::InvalidFormat;
  }

  dst.m_name.assign(hm->HashMapName);
  dst.m_id= hm->HashMapObjectId;
  dst.m_version = hm->HashMapVersion;

  /**
   * pack is stupid...and requires bytes!
   * we store shorts...so divide by 2
   */
  hm->HashMapBuckets /= sizeof(Uint16);

  dst.m_map.clear();
  for (Uint32 i = 0; i<hm->HashMapBuckets; i++)
  {
    dst.m_map.push_back(hm->HashMapValues[i]);
  }

  delete hm;
  
  return 0;
}

int
NdbDictInterface::create_hashmap(const NdbHashMapImpl& src,
                                 NdbDictObjectImpl* obj,
                                 Uint32 flags,
                                 Uint32 partitionBalance_Count)
{
  {
    DictHashMapInfo::HashMap* hm = new DictHashMapInfo::HashMap(); 
    hm->init();
    BaseString::snprintf(hm->HashMapName, sizeof(hm->HashMapName), 
                         "%s", src.getName());
    hm->HashMapBuckets = src.getMapLen();
    for (Uint32 i = 0; i<hm->HashMapBuckets; i++)
    {
      assert(NdbHashMapImpl::getImpl(src).m_map[i] <= NDB_PARTITION_MASK);
      hm->HashMapValues[i] = NdbHashMapImpl::getImpl(src).m_map[i];
    }
    
    /**
     * pack is stupid...and requires bytes!
     * we store shorts...so multiply by 2
     */
    hm->HashMapBuckets *= sizeof(Uint16);
    SimpleProperties::UnpackStatus s;
    UtilBufferWriter w(m_buffer);
    s = SimpleProperties::pack(w,
                               hm,
                               DictHashMapInfo::Mapping,
                               DictHashMapInfo::MappingSize, true);
    
    if(s != SimpleProperties::Eof)
    {
      abort();
    }
    
    delete hm;
  }
  
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_CREATE_HASH_MAP_REQ;
  tSignal.theLength = CreateHashMapReq::SignalLength;

  CreateHashMapReq* req = CAST_PTR(CreateHashMapReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->requestInfo = flags;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->fragments = partitionBalance_Count;
  req->buckets = 0; // not used from here

  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[]= { CreateTableRef::Busy, CreateTableRef::NotMaster, 0 };

  /*
    Send signal without time-out since creating files can take a very long
    time if the file is very big.
  */
  Uint32 seccnt = 1;
  if (flags & CreateHashMapReq::CreateDefault)
  {
    seccnt = 0;
  }
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for GSN_CREATE_HASH_MAP_REQ"
                        " in NdbDictInterface::create_hashmap()"));
    timeout = 1000;
  });
  DBUG_PRINT("info", ("CREATE_HASH_MAP_REQ: cnt: %u, fragments: %x",
             seccnt, req->fragments));
  assert(partitionBalance_Count != 0);
  int ret = dictSignal(&tSignal, ptr, seccnt,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       timeout, 100,
		       err);

  if (ret == 0 && obj)
  {
    Uint32* data = (Uint32*)m_buffer.get_data();
    obj->m_id = data[0];
    obj->m_version = data[1];
  }

  return ret;
}

void
NdbDictInterface::execCREATE_HASH_MAP_REF(const NdbApiSignal * signal,
                                          const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_HASH_MAP_REF");
  const CreateHashMapRef* ref =
    CAST_CONSTPTR(CreateHashMapRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "CREATE_HASH_MAP_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}


void
NdbDictInterface::execCREATE_HASH_MAP_CONF(const NdbApiSignal * signal,
                                           const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_HASH_MAP_CONF");
  const CreateHashMapConf* conf=
    CAST_CONSTPTR(CreateHashMapConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "CREATE_HASH_MAP_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->objectId;
  data[1] = conf->objectVersion;

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/**
 * ForeignKey
 */
NdbForeignKeyImpl::NdbForeignKeyImpl()
  : NdbDictionary::ForeignKey(* this),
    NdbDictObjectImpl(NdbDictionary::Object::ForeignKey), m_facade(this)
{
  init();
}

NdbForeignKeyImpl::NdbForeignKeyImpl(NdbDictionary::ForeignKey & f)
  : NdbDictionary::ForeignKey(* this),
    NdbDictObjectImpl(NdbDictionary::Object::ForeignKey), m_facade(&f)
{
  init();
}

NdbForeignKeyImpl::~NdbForeignKeyImpl()
{
}

void
NdbForeignKeyImpl::init()
{
  m_parent_columns.clear();
  m_child_columns.clear();
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_references); i++)
  {
    m_references[i].m_objectId = RNIL;
    m_references[i].m_objectVersion = RNIL;
  }
  m_on_update_action = NoAction;
  m_on_delete_action = NoAction;
}

int
NdbForeignKeyImpl::assign(const NdbForeignKeyImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  if (!m_name.assign(org.m_name))
    return -1;

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_references); i++)
  {
    if (!m_references[i].m_name.assign(org.m_references[i].m_name))
      return -1;

    m_references[i].m_objectId = org.m_references[i].m_objectId;
    m_references[i].m_objectVersion = org.m_references[i].m_objectVersion;
  }

  m_parent_columns.clear();
  for (unsigned i = 0; i < org.m_parent_columns.size(); i++)
    m_parent_columns.push_back(org.m_parent_columns[i]);

  m_child_columns.clear();
  for (unsigned i = 0; i < org.m_child_columns.size(); i++)
    m_child_columns.push_back(org.m_child_columns[i]);

  m_on_update_action = org.m_impl.m_on_update_action;
  m_on_delete_action = org.m_impl.m_on_delete_action;

  return 0;
}

int
NdbDictInterface::create_fk(const NdbForeignKeyImpl& src,
                            NdbDictObjectImpl* obj,
                            Uint32 flags)
{
  DBUG_ENTER("NdbDictInterface::create_fk");

  DictForeignKeyInfo::ForeignKey fk; fk.init();
  BaseString::snprintf(fk.Name, sizeof(fk.Name),
                       "%s", src.getName());

  BaseString::snprintf(fk.ParentTableName, sizeof(fk.ParentTableName),
                       "%s", src.getParentTable());

  BaseString::snprintf(fk.ChildTableName, sizeof(fk.ChildTableName),
                       "%s", src.getChildTable());

  fk.ParentIndexName[0] = 0;
  if (src.getParentIndex())
  {
    BaseString::snprintf(fk.ParentIndexName, sizeof(fk.ParentIndexName),
                         "%s", src.getParentIndex());
  }

  fk.ChildIndexName[0] = 0;
  if (src.getChildIndex())
  {
    BaseString::snprintf(fk.ChildIndexName, sizeof(fk.ChildIndexName),
                         "%s", src.getChildIndex());
  }
  fk.ParentTableId = src.m_references[0].m_objectId;
  fk.ParentTableVersion = src.m_references[0].m_objectVersion;
  fk.ChildTableId = src.m_references[1].m_objectId;
  fk.ChildTableVersion = src.m_references[1].m_objectVersion;
  fk.ParentIndexId = src.m_references[2].m_objectId;
  fk.ParentIndexVersion = src.m_references[2].m_objectVersion;
  fk.ChildIndexId = src.m_references[3].m_objectId;
  fk.ChildIndexVersion = src.m_references[3].m_objectVersion;
  fk.OnUpdateAction = (Uint32)src.m_on_update_action;
  fk.OnDeleteAction = (Uint32)src.m_on_delete_action;
  for (unsigned i = 0; i < src.m_parent_columns.size(); i++)
    fk.ParentColumns[i] = src.m_parent_columns[i];
  fk.ParentColumnsLength = 4 * src.m_parent_columns.size(); // bytes :(
  for (unsigned i = 0; i < src.m_child_columns.size(); i++)
    fk.ChildColumns[i] = src.m_child_columns[i];
  fk.ChildColumnsLength = 4 * src.m_child_columns.size(); // bytes :(

#ifndef DBUG_OFF
  {
    char buf[2048];
    ndbout_print(fk, buf, sizeof(buf));
    DBUG_PRINT("info", ("FK: %s", buf));
  }
#endif

  {
    // don't allow slash in fk name
    if (strchr(fk.Name, '/') != 0)
    {
      m_error.code = 21090;
      DBUG_RETURN(-1);
    }
    // enforce format <parentid>/<childid>/name
    char buf[MAX_TAB_NAME_SIZE];
    BaseString::snprintf(buf, sizeof(buf), "%u/%u/%s",
                         fk.ParentTableId, fk.ChildTableId, fk.Name);
    strcpy(fk.Name, buf);
  }

  SimpleProperties::UnpackStatus s;
  UtilBufferWriter w(m_buffer);
  s = SimpleProperties::pack(w,
                             &fk,
                             DictForeignKeyInfo::Mapping,
                             DictForeignKeyInfo::MappingSize, true);

  if (s != SimpleProperties::Eof)
  {
    abort();
  }

  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_CREATE_FK_REQ;
  tSignal.theLength = CreateFKReq::SignalLength;

  CreateFKReq* req = CAST_PTR(CreateFKReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->requestInfo = flags;
  req->requestInfo |= m_tx.requestFlags();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();

  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[]= { CreateTableRef::Busy, CreateTableRef::NotMaster, 0 };

  Uint32 seccnt = 1;
  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_CREATE_FK_REQ"
                        " in NdbDictInterface::create_fk()"));
    timeout = 1000;
  });
  int ret = dictSignal(&tSignal, ptr, seccnt,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       timeout, 100,
		       err);

  if (ret == 0 && obj)
  {
    Uint32* data = (Uint32*)m_buffer.get_data();
    obj->m_id = data[0];
    obj->m_version = data[1];
  }

  DBUG_RETURN(ret);
}

void
NdbDictInterface::execCREATE_FK_REF(const NdbApiSignal * signal,
                                          const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_FK_REF");
  const CreateFKRef* ref = CAST_CONSTPTR(CreateFKRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "CREATE_FK_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_FK_CONF(const NdbApiSignal * signal,
                                           const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_FK_CONF");
  const CreateFKConf* conf= CAST_CONSTPTR(CreateFKConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "CREATE_FK_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->fkId;
  data[1] = conf->fkVersion;

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

int
NdbDictInterface::get_fk(NdbForeignKeyImpl & dst,
                         const char * name)
{
  DBUG_ENTER("NdbDictInterface::get_fk");
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  Uint32 strLen = (Uint32)strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = m_tx.nextRequestId();
  req->requestType =
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
  req->schemaTransId = m_tx.transId();
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  LinearSectionPtr ptr[1];
  ptr[0].p  = (Uint32*)name;
  ptr[0].sz = (strLen + 3)/4;

#ifndef IGNORE_VALGRIND_WARNINGS
  if (strLen & 3)
  {
    Uint32 pad = 0;
    m_buffer.clear();
    m_buffer.append(name, strLen);
    m_buffer.append(&pad, 4);
    ptr[0].p = (Uint32*)m_buffer.get_data();
  }
#endif

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_GET_TABINFOREQ"
                        " in NdbDictInterface::get_fk()"));
    timeout = 1000;
  });
  int r = dictSignal(&tSignal, ptr, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     timeout, 100);
  if (r)
  {
    DBUG_PRINT("info", ("get_fk failed dictSignal"));
    DBUG_RETURN(-1);
  }

  m_error.code = parseForeignKeyInfo(dst,
                                     (Uint32*)m_buffer.get_data(),
                                     m_buffer.length() / 4);

  if (m_error.code)
  {
    DBUG_PRINT("info", ("get_fk failed parseFileInfo %d",
                         m_error.code));
    DBUG_RETURN(m_error.code);
  }

  DBUG_RETURN(0);
}

int
NdbDictInterface::parseForeignKeyInfo(NdbForeignKeyImpl &dst,
                                      const Uint32 * data, Uint32 len)
{
  SimplePropertiesLinearReader it(data, len);

  SimpleProperties::UnpackStatus status;
  DictForeignKeyInfo::ForeignKey fk; fk.init();
  status = SimpleProperties::unpack(it, &fk,
				    DictForeignKeyInfo::Mapping,
				    DictForeignKeyInfo::MappingSize,
				    true, true);

  if(status != SimpleProperties::Eof)
  {
    return CreateFilegroupRef::InvalidFormat;
  }

  dst.m_id = fk.ForeignKeyId;
  dst.m_version = fk.ForeignKeyVersion;
  dst.m_type = NdbDictionary::Object::ForeignKey;
  dst.m_status = NdbDictionary::Object::Retrieved;

  if (!dst.m_name.assign(fk.Name))
    return 4000;

  dst.m_references[0].m_name.assign(fk.ParentTableName);
  dst.m_references[0].m_objectId = fk.ParentTableId;
  dst.m_references[0].m_objectVersion = fk.ParentTableVersion;
  dst.m_references[1].m_name.assign(fk.ChildTableName);
  dst.m_references[1].m_objectId = fk.ChildTableId;
  dst.m_references[1].m_objectVersion = fk.ChildTableVersion;
  if (fk.ParentIndexName[0] != 0)
  {
    dst.m_references[2].m_name.assign(fk.ParentIndexName);
  }
  dst.m_references[2].m_objectId = fk.ParentIndexId;
  dst.m_references[2].m_objectVersion = fk.ParentIndexVersion;
  if (fk.ChildIndexName[0] != 0)
  {
    dst.m_references[3].m_name.assign(fk.ChildIndexName);
  }
  dst.m_references[3].m_objectId = fk.ChildIndexId;
  dst.m_references[3].m_objectVersion = fk.ChildIndexVersion;
  dst.m_on_update_action =
    static_cast<NdbDictionary::ForeignKey::FkAction>(fk.OnUpdateAction);
  dst.m_on_delete_action =
    static_cast<NdbDictionary::ForeignKey::FkAction>(fk.OnDeleteAction);

  dst.m_parent_columns.clear();
  for (unsigned i = 0; i < fk.ParentColumnsLength / 4; i++)
    dst.m_parent_columns.push_back(fk.ParentColumns[i]);

  dst.m_child_columns.clear();
  for (unsigned i = 0; i < fk.ChildColumnsLength / 4; i++)
    dst.m_child_columns.push_back(fk.ChildColumns[i]);

  return 0;
}

int
NdbDictInterface::drop_fk(const NdbDictObjectImpl & impl)
{
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_DROP_FK_REQ;
  tSignal.theLength = DropFKReq::SignalLength;

  DropFKReq * req = CAST_PTR(DropFKReq, tSignal.getDataPtrSend());
  req->clientRef = m_reference;
  req->clientData = m_tx.nextRequestId();
  req->transId = m_tx.transId();
  req->transKey = m_tx.transKey();
  req->requestInfo = 0;
  req->requestInfo |= m_tx.requestFlags();
  req->fkId = impl.m_id;
  req->fkVersion = impl.m_version;

  int errCodes[] =
    { DropTableRef::NoDropTableRecordAvailable,
      DropTableRef::NotMaster,
      DropTableRef::Busy, 0 };

  int timeout = DICT_SHORT_WAITFOR_TIMEOUT;
  DBUG_EXECUTE_IF("ndb_dictsignal_timeout", {
    DBUG_PRINT("info", ("Reducing timeout for DICT GSN_DROP_FK_REQ"
                        " in NdbDictInterface::drop_fk()"));
    timeout = 1000;
  });
  return dictSignal(&tSignal, 0, 0,
                    0, // master
                    WAIT_DROP_TAB_REQ,
                    timeout, 100,
                    errCodes);
}

void
NdbDictInterface::execDROP_FK_CONF(const NdbApiSignal * signal,
                                   const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FK_CONF");
  const DropFKConf* conf = CAST_CONSTPTR(DropFKConf, signal->getDataPtr());

  if(!m_tx.checkRequestId(conf->senderData, "DROP_FK_CONF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_FK_REF(const NdbApiSignal * signal,
                                  const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_FK_REF");
  const DropFKRef* ref = CAST_CONSTPTR(DropFKRef, signal->getDataPtr());

  if(!m_tx.checkRequestId(ref->senderData, "DROP_FK_REF"))
    DBUG_VOID_RETURN; // signal from different (possibly timed-out) transaction

  m_error.code= ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

template class Vector<NdbTableImpl*>;
template class Vector<NdbColumnImpl*>;

int
NdbDictionaryImpl::beginSchemaTrans(bool retry711)
{
  DBUG_ENTER("beginSchemaTrans");
  if (m_tx.m_state == NdbDictInterface::Tx::Started) {
    m_error.code = 4410;
    DBUG_RETURN(-1);
  }
  if (!m_receiver.checkAllNodeVersionsMin(NDBD_SCHEMA_TRANS_VERSION))
  {
    /* Upgrade 6.3 -> 7.0 path */
    /* Schema transaction not possible until upgrade complete */
    m_error.code = 4411;
    DBUG_RETURN(-1);
  }
  // TODO real transId
  m_tx.m_transId = rand();
  if (m_tx.m_transId == 0)
    m_tx.m_transId = 1;

  m_tx.m_state = NdbDictInterface::Tx::NotStarted;
  m_tx.m_error.code = 0;
  m_tx.m_transKey = 0;

  int ret = m_receiver.beginSchemaTrans(retry711);
  if (ret == -1) {
    assert(m_tx.m_state == NdbDictInterface::Tx::NotStarted);
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("transId: %x transKey: %x",
                      m_tx.m_transId, m_tx.m_transKey));

  assert(m_tx.m_state == NdbDictInterface::Tx::Started);
  assert(m_tx.m_error.code == 0);
  assert(m_tx.m_transKey != 0);
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::endSchemaTrans(Uint32 flags)
{
  DBUG_ENTER("endSchemaTrans");
  if (m_tx.m_state == NdbDictInterface::Tx::NotStarted) {
    DBUG_RETURN(0);
  }
  /*
    Check if schema transaction has been aborted
    already, for example because of master node failure.
   */
  if (m_tx.m_state != NdbDictInterface::Tx::Started)
  {
    m_tx.m_op.clear();
    DBUG_PRINT("info", ("endSchemaTrans: state %u, flags 0x%x\n", m_tx.m_state, flags));
    if (m_tx.m_state == NdbDictInterface::Tx::Aborted && // rollback at master takeover
        flags & NdbDictionary::Dictionary::SchemaTransAbort)
    {
      m_tx.m_error.code = 0;
      DBUG_RETURN(0);
    }
    m_error.code = m_tx.m_error.code;
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("transId: %x transKey: %x",
                      m_tx.m_transId, m_tx.m_transKey));
  int ret = m_receiver.endSchemaTrans(flags);
  if (ret == -1 || m_tx.m_error.code != 0) {
    DBUG_PRINT("info", ("endSchemaTrans: state %u, flags 0x%x\n", m_tx.m_state, flags));
    if (m_tx.m_state == NdbDictInterface::Tx::Committed && // rollforward at master takeover
        !(flags & NdbDictionary::Dictionary::SchemaTransAbort))
      goto committed;
    m_tx.m_op.clear();
    if (m_tx.m_state == NdbDictInterface::Tx::Aborted && // rollback at master takeover
        flags & NdbDictionary::Dictionary::SchemaTransAbort)
    {
      m_error.code = m_tx.m_error.code = 0;
      m_tx.m_state = NdbDictInterface::Tx::NotStarted;
      DBUG_RETURN(0);
    }
    if (m_tx.m_error.code != 0)
      m_error.code = m_tx.m_error.code;
    m_tx.m_state = NdbDictInterface::Tx::NotStarted;
    DBUG_RETURN(-1);
  }
committed:
  // invalidate old version of altered table
  uint i;
  for (i = 0; i < m_tx.m_op.size(); i++) {
    NdbDictInterface::Tx::Op& op = m_tx.m_op[i];
    if (op.m_gsn == GSN_ALTER_TABLE_REQ)
    {
      op.m_impl->m_status = NdbDictionary::Object::Invalid;
      m_globalHash->lock();
      int ret = m_globalHash->dec_ref_count(op.m_impl);
      m_globalHash->unlock();
      if (ret != 0)
        abort();
    }
  }
  m_tx.m_state = NdbDictInterface::Tx::NotStarted;
  m_tx.m_op.clear();
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::getDefaultHashmapSize() const
{
  return m_ndb.theImpl->get_ndbapi_config_parameters().m_default_hashmap_size;
}

bool
NdbDictInterface::checkAllNodeVersionsMin(Uint32 minNdbVersion) const
{
  for (Uint32 nodeId = 1; nodeId < MAX_NODES; nodeId++)
  {
    if (m_impl->getIsDbNode(nodeId) &&
        m_impl->getIsNodeSendable(nodeId) &&
        (m_impl->getNodeNdbVersion(nodeId) <
         minNdbVersion))
    {
      /* At least 1 sendable data node has lower-than-min
       * version
       */
      return false;
    }
  }
  
  return true;
}


int
NdbDictInterface::beginSchemaTrans(bool retry711)
{
  assert(m_tx.m_op.size() == 0);
  NdbApiSignal tSignal(m_reference);
  SchemaTransBeginReq* req =
    CAST_PTR(SchemaTransBeginReq, tSignal.getDataPtrSend());

  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_SCHEMA_TRANS_BEGIN_REQ;
  tSignal.theLength = SchemaTransBeginReq::SignalLength;

  req->clientRef =  m_reference;
  req->transId = m_tx.m_transId;
  req->requestInfo = 0;

  int errCodes[] = {
    SchemaTransBeginRef::NotMaster,
    SchemaTransBeginRef::Busy,
    retry711 ? SchemaTransBeginRef::BusyWithNR : 0,
    0
  };

  int ret = dictSignal(
      &tSignal,
      0,
      0,
      0,
      WAIT_SCHEMA_TRANS,
      DICT_SHORT_WAITFOR_TIMEOUT, // Lightweight request
      100,
      errCodes);
  if (ret == -1)
    return -1;
  return 0;
}

int
NdbDictInterface::endSchemaTrans(Uint32 flags)
{
  NdbApiSignal tSignal(m_reference);
  SchemaTransEndReq* req =
    CAST_PTR(SchemaTransEndReq, tSignal.getDataPtrSend());

  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_SCHEMA_TRANS_END_REQ;
  tSignal.theLength = SchemaTransEndReq::SignalLength;

  req->clientRef =  m_reference;
  req->transId = m_tx.m_transId;
  req->requestInfo = 0;
  req->transKey = m_tx.m_transKey;
  req->flags = flags;

  int errCodes[] = {
    SchemaTransEndRef::NotMaster,
    0
  };
  int ret = dictSignal(
      &tSignal,
      0,
      0,
      0,
      WAIT_SCHEMA_TRANS,
      DICT_LONG_WAITFOR_TIMEOUT,  // Potentially very heavy request
      100,
      errCodes);
  if (ret == -1)
    return -1;
  return 0;
}

void
NdbDictInterface::execSCHEMA_TRANS_BEGIN_CONF(const NdbApiSignal * signal,
                                              const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSCHEMA_TRANS_BEGIN_CONF");
  const SchemaTransBeginConf* conf=
    CAST_CONSTPTR(SchemaTransBeginConf, signal->getDataPtr());
  assert(m_tx.m_transId == conf->transId);
  assert(m_tx.m_state == Tx::NotStarted);
  m_tx.m_state = Tx::Started;
  m_tx.m_transKey = conf->transKey;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSCHEMA_TRANS_BEGIN_REF(const NdbApiSignal * signal,
                                             const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSCHEMA_TRANS_BEGIN_REF");
  const SchemaTransBeginRef* ref =
    CAST_CONSTPTR(SchemaTransBeginRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSCHEMA_TRANS_END_CONF(const NdbApiSignal * signal,
                                            const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSCHEMA_TRANS_END_CONF");
#ifndef NDEBUG
  const SchemaTransEndConf* conf=
    CAST_CONSTPTR(SchemaTransEndConf, signal->getDataPtr());
  assert(m_tx.m_transId == conf->transId);
#endif
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSCHEMA_TRANS_END_REF(const NdbApiSignal * signal,
                                           const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSCHEMA_TRANS_END_REF");
  const SchemaTransEndRef* ref =
    CAST_CONSTPTR(SchemaTransEndRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  DBUG_PRINT("info", ("Error code = %d", m_error.code));
  m_tx.m_error.code = ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSCHEMA_TRANS_END_REP(const NdbApiSignal * signal,
                                           const LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::SCHEMA_TRANS_END_REP");
  const SchemaTransEndRep* rep =
    CAST_CONSTPTR(SchemaTransEndRep, signal->getDataPtr());

  if (m_tx.m_state != Tx::Started)
  {
    // Ignore TRANS_END_REP if Txn was never started
    DBUG_VOID_RETURN;
  }

  (rep->errorCode == 0) ?
    m_tx.m_state = Tx::Committed
    :
    m_tx.m_state = Tx::Aborted;
  m_tx.m_error.code = rep->errorCode;
  m_masterNodeId = rep->masterNodeId;
  m_impl->theWaiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT = 0;
const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT_FIXED_MEMORY = 0;
const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_COUNT = 0;
const NdbDictionary::Column * NdbDictionary::Column::COMMIT_COUNT = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_SIZE = 0;
const NdbDictionary::Column * NdbDictionary::Column::RANGE_NO = 0;
const NdbDictionary::Column * NdbDictionary::Column::DISK_REF = 0;
const NdbDictionary::Column * NdbDictionary::Column::RECORDS_IN_RANGE = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROWID = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_GCI = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_GCI64 = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_AUTHOR = 0;
const NdbDictionary::Column * NdbDictionary::Column::ANY_VALUE = 0;
const NdbDictionary::Column * NdbDictionary::Column::COPY_ROWID = 0;
const NdbDictionary::Column * NdbDictionary::Column::OPTIMIZE = 0;
const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT_EXTENT_SPACE = 0;
const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE = 0;
const NdbDictionary::Column * NdbDictionary::Column::LOCK_REF = 0;
const NdbDictionary::Column * NdbDictionary::Column::OP_ID = 0;

template class Vector<NdbDictInterface::Tx::Op>;
