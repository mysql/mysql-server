/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "NdbDictionaryImpl.hpp"
#include "API.hpp"
#include <NdbOut.hpp>
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
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
#include <SimpleProperties.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <NdbEventOperation.hpp>
#include "NdbEventOperationImpl.hpp"
#include <NdbBlob.hpp>
#include "NdbBlobImpl.hpp"
#include <AttributeHeader.hpp>
#include <my_sys.h>
#include <NdbEnv.h>
#include <NdbMem.h>
#include <ndb_version.h>

#define DEBUG_PRINT 0
#define INCOMPATIBLE_VERSION -2

#define DICT_WAITFOR_TIMEOUT (7*24*60*60*1000)

#define ERR_RETURN(a,b) \
{\
   DBUG_PRINT("exit", ("error %d", (a).code));\
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

//#define EVENT_DEBUG

/**
 * Column
 */
NdbColumnImpl::NdbColumnImpl()
  : NdbDictionary::Column(* this), m_attrId(-1), m_facade(this)
{
  DBUG_ENTER("NdbColumnImpl::NdbColumnImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

NdbColumnImpl::NdbColumnImpl(NdbDictionary::Column & f)
  : NdbDictionary::Column(* this), m_attrId(-1), m_facade(&f)
{
  DBUG_ENTER("NdbColumnImpl::NdbColumnImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

NdbColumnImpl&
NdbColumnImpl::operator=(const NdbColumnImpl& col)
{
  DBUG_ENTER("NdbColumnImpl::operator=");
  DBUG_PRINT("info", ("this: %p  &col: %p", this, &col));
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
  m_defaultValue = col.m_defaultValue;
  m_attrSize = col.m_attrSize; 
  m_arraySize = col.m_arraySize;
  m_arrayType = col.m_arrayType;
  m_storageType = col.m_storageType;
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

  DBUG_RETURN(*this);
}

void
NdbColumnImpl::init(Type t)
{
  // do not use default_charset_info as it may not be initialized yet
  // use binary collation until NDB tests can handle charsets
  CHARSET_INFO* default_cs = &my_charset_bin;
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
    m_precision = 256;
    m_scale = 8000;
    m_length = 4;
    m_cs = NULL;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
    break;
  case Text:
    m_precision = 256;
    m_scale = 8000;
    m_length = 4;
    m_cs = default_cs;
    m_arrayType = NDB_ARRAYTYPE_FIXED;
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
#ifdef VM_TRACE
  if(NdbEnv_GetEnv("NDB_DEFAULT_DISK", (char *)0, 0))
    m_storageType = NDB_STORAGETYPE_DISK;
#endif
}

NdbColumnImpl::~NdbColumnImpl()
{
  DBUG_ENTER("NdbColumnImpl::~NdbColumnImpl");
  DBUG_PRINT("info", ("this: %p", this));
  if (m_blobTable != NULL)
    delete m_blobTable;
  m_blobTable = NULL;
  DBUG_VOID_RETURN;
}

bool
NdbColumnImpl::equal(const NdbColumnImpl& col) const 
{
  DBUG_ENTER("NdbColumnImpl::equal");
  DBUG_PRINT("info", ("this: %p  &col: %p", this, &col));
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
    if ((bool)m_distributionKey != (bool)col.m_distributionKey) {
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
  if(strcmp(m_defaultValue.c_str(), col.m_defaultValue.c_str()) != 0){
    DBUG_RETURN(false);
  }

  if (m_arrayType != col.m_arrayType || m_storageType != col.m_storageType){
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
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
  } else {
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
  DBUG_ENTER("NdbTableImpl::NdbTableImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

NdbTableImpl::NdbTableImpl(NdbDictionary::Table & f)
  : NdbDictionary::Table(* this), 
    NdbDictObjectImpl(NdbDictionary::Object::UserTable), m_facade(&f)
{
  DBUG_ENTER("NdbTableImpl::NdbTableImpl");
  DBUG_PRINT("info", ("this: %p", this));
  init();
  DBUG_VOID_RETURN;
}

NdbTableImpl::~NdbTableImpl()
{
  DBUG_ENTER("NdbTableImpl::~NdbTableImpl");
  DBUG_PRINT("info", ("this: %p", this));
  if (m_index != 0) {
    delete m_index;
    m_index = 0;
  }
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete m_columns[i];
  DBUG_VOID_RETURN;
}

void
NdbTableImpl::init(){
  m_changeMask= 0;
  m_id= RNIL;
  m_version = ~0;
  m_status = NdbDictionary::Object::Invalid;
  m_type = NdbDictionary::Object::TypeUndefined;
  m_primaryTableId= RNIL;
  m_internalName.clear();
  m_externalName.clear();
  m_newExternalName.clear();
  m_mysqlName.clear();
  m_frm.clear();
  m_newFrm.clear();
  m_ts_name.clear();
  m_new_ts_name.clear();
  m_ts.clear();
  m_new_ts.clear();
  m_fd.clear();
  m_new_fd.clear();
  m_range.clear();
  m_new_range.clear();
  m_fragmentType= NdbDictionary::Object::FragAllSmall;
  m_hashValueMask= 0;
  m_hashpointerValue= 0;
  m_linear_flag= true;
  m_primaryTable.clear();
  m_default_no_part_flag = 1;
  m_logging= true;
  m_row_gci = true;
  m_row_checksum = true;
  m_kvalue= 6;
  m_minLoadFactor= 78;
  m_maxLoadFactor= 80;
  m_keyLenInWords= 0;
  m_fragmentCount= 0;
  m_index= NULL;
  m_indexType= NdbDictionary::Object::TypeUndefined;
  m_noOfKeys= 0;
  m_noOfDistributionKeys= 0;
  m_noOfBlobs= 0;
  m_replicaCount= 0;
  m_min_rows = 0;
  m_max_rows = 0;
  m_tablespace_name.clear();
  m_tablespace_id = ~0;
  m_tablespace_version = ~0;
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
  if (m_fd.length() != obj.m_fd.length() ||
      (memcmp(m_fd.get_data(), obj.m_fd.get_data(), m_fd.length())))
  {
    DBUG_PRINT("info",("m_fd not equal"));
    DBUG_RETURN(false);
  }
  if (m_ts.length() != obj.m_ts.length() ||
      (memcmp(m_ts.get_data(), obj.m_ts.get_data(), m_ts.length())))
  {
    DBUG_PRINT("info",("m_ts not equal"));
    DBUG_RETURN(false);
  }
  if (m_range.length() != obj.m_range.length() ||
      (memcmp(m_range.get_data(), obj.m_range.get_data(), m_range.length())))
  {
    DBUG_PRINT("info",("m_range not equal"));
    DBUG_RETURN(false);
  }
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
  DBUG_RETURN(true);
}

void
NdbTableImpl::assign(const NdbTableImpl& org)
{
  DBUG_ENTER("NdbColumnImpl::assign");
  DBUG_PRINT("info", ("this: %p  &org: %p", this, &org));
  /* m_changeMask intentionally not copied */
  m_primaryTableId = org.m_primaryTableId;
  m_internalName.assign(org.m_internalName);
  updateMysqlName();
  // If the name has been explicitly set, use that name
  // otherwise use the fetched name
  if (!org.m_newExternalName.empty())
    m_externalName.assign(org.m_newExternalName);
  else
    m_externalName.assign(org.m_externalName);
  m_frm.assign(org.m_frm.get_data(), org.m_frm.length());
  m_ts_name.assign(org.m_ts_name.get_data(), org.m_ts_name.length());
  m_new_ts_name.assign(org.m_new_ts_name.get_data(),
                       org.m_new_ts_name.length());
  m_ts.assign(org.m_ts.get_data(), org.m_ts.length());
  m_new_ts.assign(org.m_new_ts.get_data(), org.m_new_ts.length());
  m_fd.assign(org.m_fd.get_data(), org.m_fd.length());
  m_new_fd.assign(org.m_new_fd.get_data(), org.m_new_fd.length());
  m_range.assign(org.m_range.get_data(), org.m_range.length());
  m_new_range.assign(org.m_new_range.get_data(), org.m_new_range.length());

  m_fragmentType = org.m_fragmentType;
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
    const NdbColumnImpl * iorg = org.m_columns[i];
    (* col) = (* iorg);
    m_columns.push_back(col);
  }

  m_fragments = org.m_fragments;

  m_linear_flag = org.m_linear_flag;
  m_max_rows = org.m_max_rows;
  m_default_no_part_flag = org.m_default_no_part_flag;
  m_logging = org.m_logging;
  m_row_gci = org.m_row_gci;
  m_row_checksum = org.m_row_checksum;
  m_kvalue = org.m_kvalue;
  m_minLoadFactor = org.m_minLoadFactor;
  m_maxLoadFactor = org.m_maxLoadFactor;
  m_keyLenInWords = org.m_keyLenInWords;
  m_fragmentCount = org.m_fragmentCount;
  
  if (m_index != 0)
    delete m_index;
  m_index = org.m_index;
 
  m_primaryTable = org.m_primaryTable;
  m_indexType = org.m_indexType;

  m_noOfKeys = org.m_noOfKeys;
  m_noOfDistributionKeys = org.m_noOfDistributionKeys;
  m_noOfBlobs = org.m_noOfBlobs;
  m_replicaCount = org.m_replicaCount;

  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;

  m_max_rows = org.m_max_rows;
  m_min_rows = org.m_min_rows;

  m_tablespace_name = org.m_tablespace_name;
  m_tablespace_id= org.m_tablespace_id;
  m_tablespace_version = org.m_tablespace_version;
  DBUG_VOID_RETURN;
}

void NdbTableImpl::setName(const char * name)
{
  m_newExternalName.assign(name);
}

const char * 
NdbTableImpl::getName() const
{
  if (m_newExternalName.empty())
    return m_externalName.c_str();
  else
    return m_newExternalName.c_str();
}

void
NdbTableImpl::computeAggregates()
{
  m_noOfKeys = 0;
  m_keyLenInWords = 0;
  m_noOfDistributionKeys = 0;
  m_noOfBlobs = 0;
  Uint32 i, n;
  for (i = 0; i < m_columns.size(); i++) {
    NdbColumnImpl* col = m_columns[i];
    if (col->m_pk) {
      m_noOfKeys++;
      m_keyLenInWords += (col->m_attrSize * col->m_arraySize + 3) / 4;
    }
    if (col->m_distributionKey == 2)    // set by user
      m_noOfDistributionKeys++;
    
    if (col->getBlobType())
      m_noOfBlobs++;
    col->m_keyInfoPos = ~0;
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
        col->m_distributionKey = true;  // set by us
        n--;
      }
    }
  }
  else 
  {
    for (i = 0, n = m_noOfKeys; n != 0; i++) {
      NdbColumnImpl* col = m_columns[i];
      if (col->m_pk)
      {
	if(col->m_distributionKey == 1)
	  col->m_distributionKey = 0;  
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

const void*
NdbTableImpl::getTablespaceNames() const
{
  if (m_new_ts_name.empty())
    return m_ts_name.get_data();
  else
    return m_new_ts_name.get_data();
}

Uint32
NdbTableImpl::getTablespaceNamesLen() const
{
  if (m_new_ts_name.empty())
    return m_ts_name.length();
  else
    return m_new_ts_name.length();
}

void NdbTableImpl::setTablespaceNames(const void *data, Uint32 len)
{
  m_new_ts_name.assign(data, len);
}

void NdbTableImpl::setFragmentCount(Uint32 count)
{
  m_fragmentCount= count;
}

Uint32 NdbTableImpl::getFragmentCount() const
{
  return m_fragmentCount;
}

void NdbTableImpl::setFrm(const void* data, Uint32 len)
{
  m_newFrm.assign(data, len);
}

const void * 
NdbTableImpl::getFrmData() const
{
  if (m_newFrm.empty())
    return m_frm.get_data();
  else
    return m_newFrm.get_data();
}

Uint32
NdbTableImpl::getFrmLength() const 
{
  if (m_newFrm.empty())
    return m_frm.length();
  else
    return m_newFrm.length();
}

void NdbTableImpl::setFragmentData(const void* data, Uint32 len)
{
  m_new_fd.assign(data, len);
}

const void * 
NdbTableImpl::getFragmentData() const
{
  if (m_new_fd.empty())
    return m_fd.get_data();
  else
    return m_new_fd.get_data();
}

Uint32
NdbTableImpl::getFragmentDataLen() const 
{
  if (m_new_fd.empty())
    return m_fd.length();
  else
    return m_new_fd.length();
}

void NdbTableImpl::setTablespaceData(const void* data, Uint32 len)
{
  m_new_ts.assign(data, len);
}

const void * 
NdbTableImpl::getTablespaceData() const
{
  if (m_new_ts.empty())
    return m_ts.get_data();
  else
    return m_new_ts.get_data();
}

Uint32
NdbTableImpl::getTablespaceDataLen() const 
{
  if (m_new_ts.empty())
    return m_ts.length();
  else
    return m_new_ts.length();
}

void NdbTableImpl::setRangeListData(const void* data, Uint32 len)
{
  m_new_range.assign(data, len);
}

const void * 
NdbTableImpl::getRangeListData() const
{
  if (m_new_range.empty())
    return m_range.get_data();
  else
    return m_new_range.get_data();
}

Uint32
NdbTableImpl::getRangeListDataLen() const 
{
  if (m_new_range.empty())
    return m_range.length();
  else
    return m_new_range.length();
}

void
NdbTableImpl::updateMysqlName()
{
  Vector<BaseString> v;
  if (m_internalName.split(v,"/") == 3)
  {
    m_mysqlName.assfmt("%s/%s",v[0].c_str(),v[2].c_str());
    return;
  }
  m_mysqlName.assign("");
}

void
NdbTableImpl::buildColumnHash(){
  const Uint32 size = m_columns.size();
  int i;
  for(i = 31; i >= 0; i--){
    if(((1 << i) & size) != 0){
      m_columnHashMask = (1 << (i + 1)) - 1;
      break;
    }
  }

  Vector<Uint32> hashValues;
  Vector<Vector<Uint32> > chains; chains.fill(size, hashValues);
  for(i = 0; i< (int) size; i++){
    Uint32 hv = Hash(m_columns[i]->getName()) & 0xFFFE;
    Uint32 bucket = hv & m_columnHashMask;
    bucket = (bucket < size ? bucket : bucket - size);
    assert(bucket < size);
    hashValues.push_back(hv);
    chains[bucket].push_back(i);
  }

  m_columnHash.clear();
  Uint32 tmp = 1; 
  m_columnHash.fill((unsigned)size-1, tmp);   // Default no chaining

  Uint32 pos = 0; // In overflow vector
  for(i = 0; i< (int) size; i++){
    Uint32 sz = chains[i].size();
    if(sz == 1){
      Uint32 col = chains[i][0];
      Uint32 hv = hashValues[col];
      Uint32 bucket = hv & m_columnHashMask;
      bucket = (bucket < size ? bucket : bucket - size);
      m_columnHash[bucket] = (col << 16) | hv | 1;
    } else if(sz > 1){
      Uint32 col = chains[i][0];
      Uint32 hv = hashValues[col];
      Uint32 bucket = hv & m_columnHashMask;
      bucket = (bucket < size ? bucket : bucket - size);
      m_columnHash[bucket] = (sz << 16) | (((size - bucket) + pos) << 1);
      for(size_t j = 0; j<sz; j++, pos++){
	Uint32 col = chains[i][j];	
	Uint32 hv = hashValues[col];
	m_columnHash.push_back((col << 16) | hv);
      }
    }
  }

  m_columnHash.push_back(0); // Overflow when looping in end of array

#if 0
  for(size_t i = 0; i<m_columnHash.size(); i++){
    Uint32 tmp = m_columnHash[i];
    int col = -1;
    if(i < size && (tmp & 1) == 1){
      col = (tmp >> 16);
    } else if(i >= size){
      col = (tmp >> 16);
    }
    ndbout_c("m_columnHash[%d] %s = %x", 
	     i, col > 0 ? m_columns[col]->getName() : "" , m_columnHash[i]);
  }
#endif
}

Uint32
NdbTableImpl::get_nodes(Uint32 hashValue, const Uint16 ** nodes) const
{
  Uint32 fragmentId;
  if(m_replicaCount == 0)
    return 0;
  switch (m_fragmentType)
  {
    case NdbDictionary::Object::FragAllSmall:
    case NdbDictionary::Object::FragAllMedium:
    case NdbDictionary::Object::FragAllLarge:
    case NdbDictionary::Object::FragSingle:
    case NdbDictionary::Object::DistrKeyLin:
    {
      fragmentId = hashValue & m_hashValueMask;
      if(fragmentId < m_hashpointerValue) 
        fragmentId = hashValue & ((m_hashValueMask << 1) + 1);
      break;
    }
    case NdbDictionary::Object::DistrKeyHash:
    {
      fragmentId = hashValue % m_fragmentCount;
      break;
    }
    default:
      return 0;
  }
  Uint32 pos = fragmentId * m_replicaCount;
  if (pos + m_replicaCount <= m_fragments.size())
  {
    *nodes = m_fragments.getBase()+pos;
    return m_replicaCount;
  }
  return 0;
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
  m_table= NULL;
}

NdbIndexImpl::~NdbIndexImpl(){
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete m_columns[i];  
}

void NdbIndexImpl::setName(const char * name)
{
  m_externalName.assign(name);
}

const char * 
NdbIndexImpl::getName() const
{
  return m_externalName.c_str();
}
 
void 
NdbIndexImpl::setTable(const char * table)
{
  m_tableName.assign(table);
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

void NdbEventImpl::setName(const char * name)
{
  m_name.assign(name);
}

const char *NdbEventImpl::getName() const
{
  return m_name.c_str();
}

void 
NdbEventImpl::setTable(const NdbDictionary::Table& table)
{
  setTable(&NdbTableImpl::getImpl(table));
  m_tableName.assign(m_tableImpl->getName());
}

void 
NdbEventImpl::setTable(NdbTableImpl *tableImpl)
{
  DBUG_ENTER("NdbEventImpl::setTable");
  DBUG_PRINT("info", ("this: %p  tableImpl: %p", this, tableImpl));
  DBUG_ASSERT(tableImpl->m_status != NdbDictionary::Object::Invalid);
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

void 
NdbEventImpl::setTable(const char * table)
{
  m_tableName.assign(table);
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

NdbDictionaryImpl::NdbDictionaryImpl(Ndb &ndb)
  : NdbDictionary::Dictionary(* this), 
    m_facade(this), 
    m_receiver(m_error),
    m_ndb(ndb)
{
  m_globalHash = 0;
  m_local_table_data_size= 0;
}

NdbDictionaryImpl::NdbDictionaryImpl(Ndb &ndb,
				     NdbDictionary::Dictionary & f)
  : NdbDictionary::Dictionary(* this), 
    m_facade(&f), 
    m_receiver(m_error),
    m_ndb(ndb)
{
  m_globalHash = 0;
  m_local_table_data_size= 0;
}

static int f_dictionary_count = 0;

NdbDictionaryImpl::~NdbDictionaryImpl()
{
  NdbElement_t<Ndb_local_table_info> * curr = m_localHash.m_tableHash.getNext(0);
  if(m_globalHash){
    while(curr != 0){
      m_globalHash->lock();
      m_globalHash->release(curr->theData->m_table_impl);
      Ndb_local_table_info::destroy(curr->theData);
      m_globalHash->unlock();
      
      curr = m_localHash.m_tableHash.getNext(curr);
    }
    
    m_globalHash->lock();
    if(--f_dictionary_count == 0){
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
    }
    m_globalHash->unlock();
  } else {
    assert(curr == 0);
  }
}

NdbTableImpl *
NdbDictionaryImpl::fetchGlobalTableImplRef(const GlobalCacheInitObject &obj)
{
  DBUG_ENTER("fetchGlobalTableImplRef");
  NdbTableImpl *impl;

  m_globalHash->lock();
  impl = m_globalHash->get(obj.m_name.c_str());
  m_globalHash->unlock();

  if (impl == 0){
    impl = m_receiver.getTable(obj.m_name.c_str(),
			       m_ndb.usingFullyQualifiedNames());
    if (impl != 0 && obj.init(*impl))
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
  assert(ret == 0);

  m_globalHash->lock();
  if ((old= m_globalHash->get(impl->m_internalName.c_str())))
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
  DBUG_ENTER("NdbDictionaryImpl::addBlobTables");
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
      DBUG_RETURN(-1);

    // TODO check primary id/version when returned by DICT

    // the blob column owns the blob table
    assert(c.m_blobTable == NULL);
    c.m_blobTable = bt;
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

#if 0
bool
NdbDictionaryImpl::setTransporter(class TransporterFacade * tf)
{
  if(tf != 0){
    m_globalHash = &tf->m_globalDictCache;
    return m_receiver.setTransporter(tf);
  }
  
  return false;
}
#endif

bool
NdbDictionaryImpl::setTransporter(class Ndb* ndb, 
				  class TransporterFacade * tf)
{
  m_globalHash = &tf->m_globalDictCache;
  if(m_receiver.setTransporter(ndb, tf)){
    m_globalHash->lock();
    if(f_dictionary_count++ == 0){
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
    }
    m_globalHash->unlock();
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

#if 0
bool
NdbDictInterface::setTransporter(class TransporterFacade * tf)
{
  if(tf == 0)
    return false;
  
  Guard g(tf->theMutexPtr);
  
  m_blockNumber = tf->open(this,
			   execSignal,
			   execNodeStatus);
  
  if ( m_blockNumber == -1 ) {
    m_error.code= 4105;
    return false; // no more free blocknumbers
  }//if
  Uint32 theNode = tf->ownId();
  m_reference = numberToRef(m_blockNumber, theNode);
  m_transporter = tf;
  m_waiter.m_mutex = tf->theMutexPtr;

  return true;
}
#endif

bool
NdbDictInterface::setTransporter(class Ndb* ndb, class TransporterFacade * tf)
{
  m_reference = ndb->getReference();
  m_transporter = tf;
  m_waiter.m_mutex = tf->theMutexPtr;
  
  return true;
}

NdbDictInterface::~NdbDictInterface()
{
}

void 
NdbDictInterface::execSignal(void* dictImpl, 
			     class NdbApiSignal* signal, 
			     class LinearSectionPtr ptr[3])
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
  case GSN_WAIT_GCP_CONF:
    tmp->execWAIT_GCP_CONF(signal, ptr);
    break;
  case GSN_WAIT_GCP_REF:
    tmp->execWAIT_GCP_REF(signal, ptr);
    break;
  default:
    abort();
  }
}

void
NdbDictInterface::execNodeStatus(void* dictImpl, Uint32 aNode,
				 bool alive, bool nfCompleted)
{
  NdbDictInterface * tmp = (NdbDictInterface*)dictImpl;
  
  if(!alive && !nfCompleted){
    return;
  }
  
  if (!alive && nfCompleted){
    tmp->m_waiter.nodeFail(aNode);
  }
}

int
NdbDictInterface::dictSignal(NdbApiSignal* sig, 
			     LinearSectionPtr ptr[3], int secs,
			     int node_specification,
			     WaitSignalType wst,
			     int timeout, Uint32 RETRIES,
			     const int *errcodes, int temporaryMask)
{
  DBUG_ENTER("NdbDictInterface::dictSignal");
  DBUG_PRINT("enter", ("useMasterNodeId: %d", node_specification));
  for(Uint32 i = 0; i<RETRIES; i++){
    m_buffer.clear();

    // Protected area
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(m_transporter, &m_waiter, refToBlock(m_reference));
    Uint32 node;
    switch(node_specification){
    case 0:
      node = (m_transporter->get_node_alive(m_masterNodeId) ? m_masterNodeId :
	      (m_masterNodeId = m_transporter->get_an_alive_node()));
      break;
    case -1:
      node = m_transporter->get_an_alive_node();
      break;
    default:
      node = node_specification;
    }
    DBUG_PRINT("info", ("node %d", node));
    if(node == 0){
      m_error.code= 4009;
      DBUG_RETURN(-1);
    }
    int res = (ptr ? 
	       m_transporter->sendFragmentedSignal(sig, node, ptr, secs):
	       m_transporter->sendSignal(sig, node));
    if(res != 0){
      DBUG_PRINT("info", ("dictSignal failed to send signal"));
      continue;
    }    
    
    m_error.code= 0;
    int ret_val= poll_guard.wait_n_unlock(timeout, node, wst);
    // End of Protected area  
    
    if(ret_val == 0 && m_error.code == 0){
      // Normal return
      DBUG_RETURN(0);
    }
    
    /**
     * Handle error codes
     */
    if(ret_val == -2) //WAIT_NODE_FAILURE
    {
      continue;
    }
    if(m_waiter.m_state == WST_WAIT_TIMEOUT)
    {
      DBUG_PRINT("info", ("dictSignal caught time-out"));
      m_error.code = 4008;
      DBUG_RETURN(-1);
    }
    
    if ( temporaryMask == -1)
    {
      const NdbError &error= getNdbError();
      if (error.status ==  NdbError::TemporaryError)
	continue;
    }
    else if ( (temporaryMask & m_error.code) != 0 ) {
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
	continue;
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
  req->senderData = 0;
  req->requestType =
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = tableId;
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
  req->senderData= 0;
  req->requestType=
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen= namelen;
  tSignal.theReceiversBlockNumber= DBDICT;
  tSignal.theVerId_signalNumber= GSN_GET_TABINFOREQ;
  tSignal.theLength= GetTabInfoReq::SignalLength;

  // Copy name to m_buffer to get a word sized buffer
  m_buffer.clear();
  m_buffer.grow(namelen_words*4+4);
  m_buffer.append(name.c_str(), namelen);

#ifndef IGNORE_VALGRIND_WARNINGS
  Uint32 pad = 0;
  m_buffer.append(&pad, 4);
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
  int r = dictSignal(signal, ptr, noOfSections,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     DICT_WAITFOR_TIMEOUT, 100, errCodes);

  if (r)
    return 0;
  
  NdbTableImpl * rt = 0;
  m_error.code = parseTableInfo(&rt, 
				(Uint32*)m_buffer.get_data(), 
  				m_buffer.length() / 4, 
				fullyQualifiedNames);
  if(rt)
    rt->buildColumnHash();
  
  return rt;
}

void
NdbDictInterface::execGET_TABINFO_CONF(NdbApiSignal * signal, 
				       LinearSectionPtr ptr[3])
{
  const GetTabInfoConf* conf = CAST_CONSTPTR(GetTabInfoConf, signal->getDataPtr());
  if(signal->isFirstFragment()){
    m_fragmentId = signal->getFragmentId();
    m_buffer.grow(4 * conf->totalLen);
  } else {
    if(m_fragmentId != signal->getFragmentId()){
      abort();
    }
  }
  
  const Uint32 i = GetTabInfoConf::DICT_TAB_INFO;
  m_buffer.append(ptr[i].p, 4 * ptr[i].sz);

  if(!signal->isLastFragment()){
    return;
  }  
  
  m_waiter.signal(NO_WAIT);
}

void
NdbDictInterface::execGET_TABINFO_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  const GetTabInfoRef* ref = CAST_CONSTPTR(GetTabInfoRef, 
					   signal->getDataPtr());
  
  m_error.code= ref->errorCode;
  m_waiter.signal(NO_WAIT);
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
  { DictTabInfo::StoreTemporary,     NdbDictionary::Object::StoreTemporary },
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

  tableDesc = (DictTabInfo::Table*)NdbMem_Allocate(sizeof(DictTabInfo::Table));
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
    NdbMem_Free((void*)tableDesc);
    DBUG_RETURN(703);
  }
  const char * internalName = tableDesc->TableName;
  const char * externalName = Ndb::externalizeTableName(internalName, fullyQualifiedNames);

  NdbTableImpl * impl = new NdbTableImpl();
  impl->m_id = tableDesc->TableId;
  impl->m_version = tableDesc->TableVersion;
  impl->m_status = NdbDictionary::Object::Retrieved;
  impl->m_internalName.assign(internalName);
  impl->updateMysqlName();
  impl->m_externalName.assign(externalName);

  impl->m_frm.assign(tableDesc->FrmData, tableDesc->FrmLen);
  impl->m_fd.assign(tableDesc->FragmentData, tableDesc->FragmentDataLen);
  impl->m_range.assign(tableDesc->RangeListData, tableDesc->RangeListDataLen);
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
  
  Uint64 max_rows = ((Uint64)tableDesc->MaxRowsHigh) << 32;
  max_rows += tableDesc->MaxRowsLow;
  impl->m_max_rows = max_rows;
  Uint64 min_rows = ((Uint64)tableDesc->MinRowsHigh) << 32;
  min_rows += tableDesc->MinRowsLow;
  impl->m_min_rows = min_rows;
  impl->m_default_no_part_flag = tableDesc->DefaultNoPartFlag;
  impl->m_linear_flag = tableDesc->LinearHashFlag;
  impl->m_logging = tableDesc->TableLoggedFlag;
  impl->m_row_gci = tableDesc->RowGCIFlag;
  impl->m_row_checksum = tableDesc->RowChecksumFlag;
  impl->m_kvalue = tableDesc->TableKValue;
  impl->m_minLoadFactor = tableDesc->MinLoadFactor;
  impl->m_maxLoadFactor = tableDesc->MaxLoadFactor;

  impl->m_indexType = (NdbDictionary::Object::Type)
    getApiConstant(tableDesc->TableType,
		   indexTypeMapping,
		   NdbDictionary::Object::TypeUndefined);
  
  if(impl->m_indexType == NdbDictionary::Object::TypeUndefined){
  } else {
    const char * externalPrimary = 
      Ndb::externalizeTableName(tableDesc->PrimaryTable, fullyQualifiedNames);
    impl->m_primaryTable.assign(externalPrimary);
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
      NdbMem_Free((void*)tableDesc);
      DBUG_RETURN(703);
    }
    
    NdbColumnImpl * col = new NdbColumnImpl();
    col->m_attrId = attrDesc.AttributeId;
    col->setName(attrDesc.AttributeName);

    // check type and compute attribute size and array size
    if (! attrDesc.translateExtType()) {
      delete impl;
      NdbMem_Free((void*)tableDesc);
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
      delete impl;
      NdbMem_Free((void*)tableDesc);
      DBUG_RETURN(703);
    }
    if (col->getCharType()) {
      col->m_cs = get_charset(cs_number, MYF(0));
      if (col->m_cs == NULL) {
        delete impl;
        NdbMem_Free((void*)tableDesc);
        DBUG_RETURN(743);
      }
    }
    col->m_attrSize = (1 << attrDesc.AttributeSize) / 8;
    col->m_arraySize = attrDesc.AttributeArraySize;
    col->m_arrayType = attrDesc.AttributeArrayType;
    if(attrDesc.AttributeSize == 0)
    {
      col->m_attrSize = 4;
      col->m_arraySize = (attrDesc.AttributeArraySize + 31) >> 5;
    }
    col->m_storageType = attrDesc.AttributeStorageType;
    
    col->m_pk = attrDesc.AttributeKeyFlag;
    col->m_distributionKey = attrDesc.AttributeDKey ? 2 : 0;
    col->m_nullable = attrDesc.AttributeNullableFlag;
    col->m_autoIncrement = (attrDesc.AttributeAutoIncrement ? true : false);
    col->m_autoIncrementInitialValue = ~0;
    col->m_defaultValue.assign(attrDesc.AttributeDefaultValue);

    col->m_column_no = impl->m_columns.size();
    impl->m_columns.push_back(col);
    it.next();
  }

  impl->computeAggregates();

  if(tableDesc->ReplicaDataLen > 0)
  {
    Uint16 replicaCount = ntohs(tableDesc->ReplicaData[0]);
    Uint16 fragCount = ntohs(tableDesc->ReplicaData[1]);

    impl->m_replicaCount = replicaCount;
    impl->m_fragmentCount = fragCount;
    DBUG_PRINT("info", ("replicaCount=%x , fragCount=%x",replicaCount,fragCount));
    for(i = 0; i < (Uint32) (fragCount*replicaCount); i++)
    {
      impl->m_fragments.push_back(ntohs(tableDesc->ReplicaData[i+2]));
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

  NdbMem_Free((void*)tableDesc);
  if (version < MAKE_VERSION(5,1,3))
  {
    ;
  } 
  else
  {
    DBUG_ASSERT(impl->m_fragmentCount > 0);
  }
  DBUG_RETURN(0);
}

/*****************************************************************
 * Create table and alter table
 */
int
NdbDictionaryImpl::createTable(NdbTableImpl &t)
{ 
  DBUG_ENTER("NdbDictionaryImpl::createTable");

  // if the new name has not been set, use the copied name
  if (t.m_newExternalName.empty())
    t.m_newExternalName.assign(t.m_externalName);

  // create table
  if (m_receiver.createTable(m_ndb, t) != 0)
    DBUG_RETURN(-1);
  Uint32* data = (Uint32*)m_receiver.m_buffer.get_data();
  t.m_id = data[0];
  t.m_version = data[1];

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
    bool autoIncrement = false;
    Uint64 initialValue = 0;
    for (Uint32 i = 0; i < t.m_columns.size(); i++) {
      const NdbColumnImpl* c = t.m_columns[i];
      assert(c != NULL);
      if (c->m_autoIncrement) {
        if (autoIncrement) {
          m_error.code = 4335;
          delete t2;
          DBUG_RETURN(-1);
        }
        autoIncrement = true;
        initialValue = c->m_autoIncrementInitialValue;
      }
    }
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
  if (t2->m_noOfBlobs != 0 && createBlobTables(*t2) != 0) {
    int save_code = m_error.code;
    (void)dropTable(*t2);
    m_error.code = save_code;
    delete t2;
    DBUG_RETURN(-1);
  }

  // not entered in cache
  delete t2;
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::createBlobTables(NdbTableImpl &t)
{
  DBUG_ENTER("NdbDictionaryImpl::createBlobTables");
  for (unsigned i = 0; i < t.m_columns.size(); i++) {
    NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    NdbTableImpl bt;
    NdbBlob::getBlobTable(bt, &t, &c);
    if (createTable(bt) != 0) {
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0); 
}

int 
NdbDictInterface::createTable(Ndb & ndb,
			      NdbTableImpl & impl)
{
  DBUG_ENTER("NdbDictInterface::createTable");
  DBUG_RETURN(createOrAlterTable(ndb, impl, false));
}

int NdbDictionaryImpl::alterTable(NdbTableImpl &impl)
{
  BaseString internalName(impl.m_internalName);
  const char * originalInternalName = internalName.c_str();

  DBUG_ENTER("NdbDictionaryImpl::alterTable");
  Ndb_local_table_info * local = 0;
  if((local= get_local_table_info(originalInternalName)) == 0)
  {
    m_error.code = 709;
    DBUG_RETURN(-1);
  }

  // Alter the table
  int ret = alterTableGlobal(*local->m_table_impl, impl);
  if(ret == 0)
  {
    m_globalHash->lock();
    m_globalHash->release(local->m_table_impl, 1);
    m_globalHash->unlock();
    m_localHash.drop(originalInternalName);
  }
  DBUG_RETURN(ret);
}

int NdbDictionaryImpl::alterTableGlobal(NdbTableImpl &old_impl,
                                        NdbTableImpl &impl)
{
  DBUG_ENTER("NdbDictionaryImpl::alterTableGlobal");
  // Alter the table
  int ret = m_receiver.alterTable(m_ndb, impl);
  old_impl.m_status = NdbDictionary::Object::Invalid;
  if(ret == 0){
    DBUG_RETURN(ret);
  }
  ERR_RETURN(getNdbError(), ret);
}

int 
NdbDictInterface::alterTable(Ndb & ndb,
			      NdbTableImpl & impl)
{
  DBUG_ENTER("NdbDictInterface::alterTable");
  DBUG_RETURN(createOrAlterTable(ndb, impl, true));
}

int 
NdbDictInterface::createOrAlterTable(Ndb & ndb,
				     NdbTableImpl & impl,
				     bool alter)
{
  unsigned i, err;
  char *ts_names[MAX_NDB_PARTITIONS];
  DBUG_ENTER("NdbDictInterface::createOrAlterTable");

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
  
  // Check if any changes for alter table
  
  // Name change
  if (!impl.m_newExternalName.empty()) {
    if (alter)
    {
      AlterTableReq::setNameFlag(impl.m_changeMask, true);
    }
    impl.m_externalName.assign(impl.m_newExternalName);
    impl.m_newExternalName.clear();
  }
  // Definition change (frm)
  if (!impl.m_newFrm.empty())
  {
    if (alter)
    {
      AlterTableReq::setFrmFlag(impl.m_changeMask, true);
    }
    impl.m_frm.assign(impl.m_newFrm.get_data(), impl.m_newFrm.length());
    impl.m_newFrm.clear();
  }
  // Change FragmentData (fragment identity, state, tablespace id)
  if (!impl.m_new_fd.empty())
  {
    if (alter)
    {
      AlterTableReq::setFragDataFlag(impl.m_changeMask, true);
    }
    impl.m_fd.assign(impl.m_new_fd.get_data(), impl.m_new_fd.length());
    impl.m_new_fd.clear();
  }
  // Change Tablespace Name Data
  if (!impl.m_new_ts_name.empty())
  {
    if (alter)
    {
      AlterTableReq::setTsNameFlag(impl.m_changeMask, true);
    }
    impl.m_ts_name.assign(impl.m_new_ts_name.get_data(),
                          impl.m_new_ts_name.length());
    impl.m_new_ts_name.clear();
  }
  // Change Range/List Data
  if (!impl.m_new_range.empty())
  {
    if (alter)
    {
      AlterTableReq::setRangeListFlag(impl.m_changeMask, true);
    }
    impl.m_range.assign(impl.m_new_range.get_data(),
                          impl.m_new_range.length());
    impl.m_new_range.clear();
  }
  // Change Tablespace Data
  if (!impl.m_new_ts.empty())
  {
    if (alter)
    {
      AlterTableReq::setTsFlag(impl.m_changeMask, true);
    }
    impl.m_ts.assign(impl.m_new_ts.get_data(),
                     impl.m_new_ts.length());
    impl.m_new_ts.clear();
  }


  /*
     TODO RONM: Here I need to insert checks for fragment array and
     range or list array
  */
  
  //validate();
  //aggregate();

  const BaseString internalName(
    ndb.internalize_table_name(impl.m_externalName.c_str()));
  impl.m_internalName.assign(internalName);
  impl.updateMysqlName();
  DictTabInfo::Table *tmpTab;

  tmpTab = (DictTabInfo::Table*)NdbMem_Allocate(sizeof(DictTabInfo::Table));
  if (!tmpTab)
  {
    m_error.code = 4000;
    DBUG_RETURN(-1);
  }
  tmpTab->init();
  BaseString::snprintf(tmpTab->TableName,
	   sizeof(tmpTab->TableName),
	   internalName.c_str());

  Uint32 distKeys= 0;
  for(i = 0; i<sz; i++) {
    const NdbColumnImpl * col = impl.m_columns[i];
    if (col == NULL) {
      m_error.code = 4272;
      NdbMem_Free((void*)tmpTab);
      DBUG_RETURN(-1);
    }
    if (col->m_distributionKey)
    {
      distKeys++;
    }
  }
  if (distKeys == impl.m_noOfKeys)
    distKeys= 0;
  impl.m_noOfDistributionKeys= distKeys;


  // Check max length of frm data
  if (impl.m_frm.length() > MAX_FRM_DATA_SIZE){
    m_error.code= 1229;
    NdbMem_Free((void*)tmpTab);
    DBUG_RETURN(-1);
  }
  /*
    TODO RONM: This needs to change to dynamic arrays instead
    Frm Data, FragmentData, TablespaceData, RangeListData, TsNameData
  */
  tmpTab->FrmLen = impl.m_frm.length();
  memcpy(tmpTab->FrmData, impl.m_frm.get_data(), impl.m_frm.length());

  tmpTab->FragmentDataLen = impl.m_fd.length();
  memcpy(tmpTab->FragmentData, impl.m_fd.get_data(), impl.m_fd.length());

  tmpTab->TablespaceDataLen = impl.m_ts.length();
  memcpy(tmpTab->TablespaceData, impl.m_ts.get_data(), impl.m_ts.length());

  tmpTab->RangeListDataLen = impl.m_range.length();
  memcpy(tmpTab->RangeListData, impl.m_range.get_data(),
         impl.m_range.length());

  memcpy(ts_names, impl.m_ts_name.get_data(),
         impl.m_ts_name.length());

  tmpTab->FragmentCount= impl.m_fragmentCount;
  tmpTab->TableLoggedFlag = impl.m_logging;
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

  if (impl.m_ts_name.length())
  {
    char **ts_name_ptr= (char**)ts_names;
    i= 0;
    do
    {
      NdbTablespaceImpl tmp;
      if (*ts_name_ptr)
      {
        if(get_filegroup(tmp, NdbDictionary::Object::Tablespace, 
                         (const char*)*ts_name_ptr) == 0)
        {
          tmpTab->TablespaceData[2*i] = tmp.m_id;
          tmpTab->TablespaceData[2*i + 1] = tmp.m_version;
        }
        else
        { 
          NdbMem_Free((void*)tmpTab);
          DBUG_RETURN(-1);
        }
      }
      else
      {
        /*
          No tablespace used, set tablespace id to NULL
        */
        tmpTab->TablespaceData[2*i] = RNIL;
        tmpTab->TablespaceData[2*i + 1] = 0;
      }
      ts_name_ptr++;
    } while (++i < tmpTab->FragmentCount);
    tmpTab->TablespaceDataLen= 4*i;
  }

  tmpTab->FragmentType = getKernelConstant(impl.m_fragmentType,
 					   fragmentTypeMapping,
					   DictTabInfo::AllNodesSmallTable);
  tmpTab->TableVersion = rand();

  const char *tablespace_name= impl.m_tablespace_name.c_str();
loop:
  if(impl.m_tablespace_id != ~(Uint32)0)
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
      
      NdbMem_Free((void*)tmpTab);
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
  
  UtilBufferWriter w(m_buffer);
  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w, 
			     tmpTab,
			     DictTabInfo::TableMapping, 
			     DictTabInfo::TableMappingSize, true);
  
  if(s != SimpleProperties::Eof){
    abort();
  }
  NdbMem_Free((void*)tmpTab);
  
  DBUG_PRINT("info",("impl.m_noOfDistributionKeys: %d impl.m_noOfKeys: %d distKeys: %d",
		     impl.m_noOfDistributionKeys, impl.m_noOfKeys, distKeys));
  if (distKeys == impl.m_noOfKeys)
    distKeys= 0;
  impl.m_noOfDistributionKeys= distKeys;
  
  for(i = 0; i<sz; i++){
    const NdbColumnImpl * col = impl.m_columns[i];
    if(col == 0)
      continue;
    
    DBUG_PRINT("info",("column: %s(%d) col->m_distributionKey: %d",
		       col->m_name.c_str(), i, col->m_distributionKey));
    DictTabInfo::Attribute tmpAttr; tmpAttr.init();
    BaseString::snprintf(tmpAttr.AttributeName, sizeof(tmpAttr.AttributeName), 
	     col->m_name.c_str());
    tmpAttr.AttributeId = col->m_attrId;
    tmpAttr.AttributeKeyFlag = col->m_pk;
    tmpAttr.AttributeNullableFlag = col->m_nullable;
    tmpAttr.AttributeDKey = distKeys ? (bool)col->m_distributionKey : 0;

    tmpAttr.AttributeExtType = (Uint32)col->m_type;
    tmpAttr.AttributeExtPrecision = ((unsigned)col->m_precision & 0xFFFF);
    tmpAttr.AttributeExtScale = col->m_scale;
    tmpAttr.AttributeExtLength = col->m_length;
    if(col->m_storageType == NDB_STORAGETYPE_DISK)
      tmpAttr.AttributeArrayType = NDB_ARRAYTYPE_FIXED;
    else
      tmpAttr.AttributeArrayType = col->m_arrayType;

    if(col->m_pk)
      tmpAttr.AttributeStorageType = NDB_STORAGETYPE_MEMORY;      
    else
      tmpAttr.AttributeStorageType = col->m_storageType;

    if(col->getBlobType())
      tmpAttr.AttributeStorageType = NDB_STORAGETYPE_MEMORY;      
    
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
    // distribution key not supported for Char attribute
    if (distKeys && col->m_distributionKey && col->m_cs != NULL) {
      // we can allow this for non-var char where strxfrm does nothing
      if (col->m_type == NdbDictionary::Column::Char &&
          (col->m_cs->state & MY_CS_BINSORT))
        ;
      else {
        m_error.code= 745;
        DBUG_RETURN(-1);
      }
    }
    // charset in upper half of precision
    if (col->getCharType()) {
      tmpAttr.AttributeExtPrecision |= (col->m_cs->number << 16);
    }

    tmpAttr.AttributeAutoIncrement = col->m_autoIncrement;
    BaseString::snprintf(tmpAttr.AttributeDefaultValue, 
	     sizeof(tmpAttr.AttributeDefaultValue),
	     col->m_defaultValue.c_str());
    s = SimpleProperties::pack(w, 
			       &tmpAttr,
			       DictTabInfo::AttributeMapping, 
			       DictTabInfo::AttributeMappingSize, true);
    w.add(DictTabInfo::AttributeEnd, 1);
  }

  int ret;
  
  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  if (alter) {
    tSignal.theVerId_signalNumber   = GSN_ALTER_TABLE_REQ;
    tSignal.theLength = AlterTableReq::SignalLength;

    AlterTableReq * req = CAST_PTR(AlterTableReq, tSignal.getDataPtrSend());
    
    req->senderRef = m_reference;
    req->senderData = 0;
    req->changeMask = impl.m_changeMask;
    req->tableId = impl.m_id;
    req->tableVersion = impl.m_version;;

    int errCodes[] = { AlterTableRef::NotMaster, AlterTableRef::Busy, 0 };
    ret = dictSignal(&tSignal, ptr, 1,
		     0, // master
		     WAIT_ALTER_TAB_REQ,
		     DICT_WAITFOR_TIMEOUT, 100,
		     errCodes);
    
    if(m_error.code == AlterTableRef::InvalidTableVersion) {
      // Clear caches and try again
      DBUG_RETURN(INCOMPATIBLE_VERSION);
    }
  } else {
    tSignal.theVerId_signalNumber   = GSN_CREATE_TABLE_REQ;
    tSignal.theLength = CreateTableReq::SignalLength;

    CreateTableReq * req = CAST_PTR(CreateTableReq, tSignal.getDataPtrSend());
    req->senderRef = m_reference;
    req->senderData = 0;
    int errCodes[] = { CreateTableRef::Busy, CreateTableRef::NotMaster, 0 };
    ret = dictSignal(&tSignal, ptr, 1,
		     0, // master node
		     WAIT_CREATE_INDX_REQ,
		     DICT_WAITFOR_TIMEOUT, 100,
		     errCodes);
  }
  
  DBUG_RETURN(ret);
}

void
NdbDictInterface::execCREATE_TABLE_CONF(NdbApiSignal * signal,
					LinearSectionPtr ptr[3])
{
  const CreateTableConf* const conf=
    CAST_CONSTPTR(CreateTableConf, signal->getDataPtr());
  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->tableId;
  data[1] = conf->tableVersion;
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_TABLE_REF(NdbApiSignal * sig,
				       LinearSectionPtr ptr[3])
{
  const CreateTableRef* ref = CAST_CONSTPTR(CreateTableRef, sig->getDataPtr());
  m_error.code= ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execALTER_TABLE_CONF(NdbApiSignal * signal,
                                       LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);
}

void
NdbDictInterface::execALTER_TABLE_REF(NdbApiSignal * sig,
				      LinearSectionPtr ptr[3])
{
  const AlterTableRef * ref = CAST_CONSTPTR(AlterTableRef, sig->getDataPtr());
  m_error.code= ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);
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
  if ((res = listIndexes(list, impl.m_id)) == -1){
    return -1;
  }
  for (unsigned i = 0; i < list.count; i++) {
    const List::Element& element = list.elements[i];
    if ((res = dropIndex(element.name, name)) == -1)
    {
      return -1;
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
  int res;
  const char * name = impl.getName();
  DBUG_ENTER("NdbDictionaryImpl::dropTableGlobal");
  DBUG_ASSERT(impl.m_status != NdbDictionary::Object::New);
  DBUG_ASSERT(impl.m_indexType == NdbDictionary::Object::TypeUndefined);

  List list;
  if ((res = listIndexes(list, impl.m_id)) == -1){
    ERR_RETURN(getNdbError(), -1);
  }
  for (unsigned i = 0; i < list.count; i++) {
    const List::Element& element = list.elements[i];
    NdbIndexImpl *idx= getIndexGlobal(element.name, impl);
    if (idx == NULL)
    {
      ERR_RETURN(getNdbError(), -1);
    }
    if ((res = dropIndexGlobal(*idx)) == -1)
    {
      releaseIndexGlobal(*idx, 1);
      ERR_RETURN(getNdbError(), -1);
    }
    releaseIndexGlobal(*idx, 1);
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
  req->senderRef = m_reference;
  req->senderData = 0;
  req->tableId = impl.m_id;
  req->tableVersion = impl.m_version;

  int errCodes[] =
    { DropTableRef::NoDropTableRecordAvailable,
      DropTableRef::NotMaster,
      DropTableRef::Busy, 0 };
  int r = dictSignal(&tSignal, 0, 0,
		     0, // master
		     WAIT_DROP_TAB_REQ, 
		     DICT_WAITFOR_TIMEOUT, 100,
		     errCodes);
  if(m_error.code == DropTableRef::InvalidTableVersion) {
    // Clear caches and try again
    return INCOMPATIBLE_VERSION;
  }
  return r;
}

void
NdbDictInterface::execDROP_TABLE_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_TABLE_CONF");
  //DropTableConf* const conf = CAST_CONSTPTR(DropTableConf, signal->getDataPtr());

  m_waiter.signal(NO_WAIT);  
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_TABLE_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_TABLE_REF");
  const DropTableRef* ref = CAST_CONSTPTR(DropTableRef, signal->getDataPtr());
  m_error.code= ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
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
  idx->m_version = tab->m_version;
  idx->m_status = tab->m_status;
  idx->m_id = tab->m_id;
  idx->m_externalName.assign(tab->getName());
  idx->m_tableName.assign(prim->m_externalName);
  NdbDictionary::Object::Type type = idx->m_type = tab->m_indexType;
  idx->m_logging = tab->m_logging;
  // skip last attribute (NDB$PK or NDB$TNODE)

  const Uint32 distKeys = prim->m_noOfDistributionKeys;
  Uint32 keyCount = (distKeys ? distKeys : prim->m_noOfKeys);

  unsigned i;
  for(i = 0; i+1<tab->m_columns.size(); i++){
    NdbColumnImpl* org = tab->m_columns[i];

    NdbColumnImpl* col = new NdbColumnImpl;
    // Copy column definition
    *col = * org;
    idx->m_columns.push_back(col);

    /**
     * reverse map
     */
    const NdbColumnImpl* primCol = prim->getColumn(col->getName());
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
  }

  if(keyCount == 0)
  {
    tab->m_noOfDistributionKeys = (distKeys ? distKeys : prim->m_noOfKeys);
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
NdbDictionaryImpl::createIndex(NdbIndexImpl &ix)
{
  ASSERT_NOT_MYSQLD;
  NdbTableImpl* tab = getTable(ix.getTable());
  if(tab == 0){
    m_error.code = 4249;
    return -1;
  }
  
  return m_receiver.createIndex(m_ndb, ix, * tab);
}

int
NdbDictionaryImpl::createIndex(NdbIndexImpl &ix, NdbTableImpl &tab)
{
  return m_receiver.createIndex(m_ndb, ix, tab);
}

int 
NdbDictInterface::createIndex(Ndb & ndb,
			      const NdbIndexImpl & impl, 
			      const NdbTableImpl & table)
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

  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_CREATE_INDX_REQ;
  tSignal.theLength = CreateIndxReq::SignalLength;
  
  CreateIndxReq * const req = CAST_PTR(CreateIndxReq, tSignal.getDataPtrSend());
  
  req->setUserRef(m_reference);
  req->setConnectionPtr(0);
  req->setRequestType(CreateIndxReq::RT_USER);
  
  Uint32 it = getKernelConstant(impl.m_type,
				indexTypeMapping,
				DictTabInfo::UndefTableType);
  
  if(it == DictTabInfo::UndefTableType){
    m_error.code = 4250;
    return -1;
  }
  req->setIndexType((DictTabInfo::TableType) it);
  
  req->setTableId(table.m_id);
  req->setOnline(true);
  AttributeList attributeList;
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
    if (it == DictTabInfo::UniqueHashIndex &&
        (err = NdbSqlUtil::check_column_for_hash_index(col->m_type, col->m_cs))
        ||
        it == DictTabInfo::OrderedIndex &&
        (err = NdbSqlUtil::check_column_for_ordered_index(col->m_type, col->m_cs)))
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
  return dictSignal(&tSignal, ptr, 2,
		    0, // master
		    WAIT_CREATE_INDX_REQ,
		    DICT_WAITFOR_TIMEOUT, 100,
		    errCodes);
}

void
NdbDictInterface::execCREATE_INDX_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_INDX_REF(NdbApiSignal * sig,
				      LinearSectionPtr ptr[3])
{
  const CreateIndxRef* ref = CAST_CONSTPTR(CreateIndxRef, sig->getDataPtr());
  m_error.code = ref->getErrorCode();
  if(m_error.code == ref->NotMaster)
    m_masterNodeId= ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
}

/*****************************************************************
 * Drop index
 */
int
NdbDictionaryImpl::dropIndex(const char * indexName, 
			     const char * tableName)
{
  ASSERT_NOT_MYSQLD;
  NdbIndexImpl * idx = getIndex(indexName, tableName);
  if (idx == 0) {
    m_error.code = 4243;
    return -1;
  }
  int ret = dropIndex(*idx, tableName);
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
      return dropIndex(indexName, tableName);
    }

    int ret= dropIndexGlobal(impl);
    if (ret == 0)
    {
      m_globalHash->lock();
      m_globalHash->release(impl.m_table, 1);
      m_globalHash->unlock();
      m_localHash.drop(internalIndexName.c_str());
    }
    return ret;
  }

  m_error.code = 4243;
  return -1;
}

int
NdbDictionaryImpl::dropIndexGlobal(NdbIndexImpl & impl)
{
  DBUG_ENTER("NdbDictionaryImpl::dropIndexGlobal");
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
  req->setUserRef(m_reference);
  req->setConnectionPtr(0);
  req->setRequestType(DropIndxReq::RT_USER);
  req->setTableId(~0);  // DICT overwrites
  req->setIndexId(timpl.m_id);
  req->setIndexVersion(timpl.m_version);

  int errCodes[] = { DropIndxRef::Busy, DropIndxRef::NotMaster, 0 };
  int r = dictSignal(&tSignal, 0, 0,
		     0, // master
		     WAIT_DROP_INDX_REQ,
		     DICT_WAITFOR_TIMEOUT, 100,
		     errCodes);
  if(m_error.code == DropIndxRef::InvalidIndexVersion) {
    // Clear caches and try again
    ERR_RETURN(m_error, INCOMPATIBLE_VERSION);
  }
  ERR_RETURN(m_error, r);
}

void
NdbDictInterface::execDROP_INDX_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execDROP_INDX_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  const DropIndxRef* ref = CAST_CONSTPTR(DropIndxRef, signal->getDataPtr());
  m_error.code = ref->getErrorCode();
  if(m_error.code == ref->NotMaster)
    m_masterNodeId= ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
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
  
#ifdef EVENT_DEBUG
  char buf[128] = {0};
  evnt.m_attrListBitmask.getText(buf);
  ndbout_c("createEvent: mask = %s", buf);
#endif

  // NdbDictInterface m_receiver;
  if (m_receiver.createEvent(m_ndb, evnt, 0 /* getFlag unset */) != 0)
    ERR_RETURN(getNdbError(), -1);

  // Create blob events
  if (evnt.m_mergeEvents && createBlobEvents(evnt) != 0) {
    int save_code = m_error.code;
    (void)dropEvent(evnt.m_name.c_str());
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

  CreateEvntReq * const req = CAST_PTR(CreateEvntReq, tSignal.getDataPtrSend());
  
  req->setUserRef(m_reference);
  req->setUserData(0);

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

  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = (m_buffer.length()+3) >> 2;

  int ret = dictSignal(&tSignal,ptr, 1,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       DICT_WAITFOR_TIMEOUT, 100,
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
  } else {
    if (evnt.m_tableImpl->m_id         != evntConf->getTableId() ||
	evnt.m_tableImpl->m_version    != evntConf->getTableVersion() ||
	//evnt.m_attrListBitmask != evntConf->getAttrListBitmask() ||
	evnt.mi_type           != evntConf->getEventType()) {
      ndbout_c("ERROR*************");
      ERR_RETURN(getNdbError(), 1);
    }
  }

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
  tSignal.theLength = SubStartReq::SignalLength2;
  
  SubStartReq * req = CAST_PTR(SubStartReq, tSignal.getDataPtrSend());

  req->subscriptionId   = ev_op.m_eventImpl->m_eventId;
  req->subscriptionKey  = ev_op.m_eventImpl->m_eventKey;
  req->part             = SubscriptionData::TableData;
  req->subscriberData   = ev_op.m_oid;
  req->subscriberRef    = m_reference;

  DBUG_PRINT("info",("GSN_SUB_START_REQ subscriptionId=%d,subscriptionKey=%d,"
		     "subscriberData=%d",req->subscriptionId,
		     req->subscriptionKey,req->subscriberData));

  DBUG_RETURN(dictSignal(&tSignal,NULL,0,
			 0 /*use masternode id*/,
			 WAIT_CREATE_INDX_REQ /*WAIT_CREATE_EVNT_REQ*/,
			 -1, 100,
			 0, -1));
}

int
NdbDictionaryImpl::stopSubscribeEvent(NdbEventOperationImpl & ev_op)
{
  // NdbDictInterface m_receiver;
  return m_receiver.stopSubscribeEvent(m_ndb, ev_op);
}

int
NdbDictInterface::stopSubscribeEvent(class Ndb & ndb,
				     NdbEventOperationImpl & ev_op)
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

  DBUG_PRINT("info",("GSN_SUB_STOP_REQ subscriptionId=%d,subscriptionKey=%d,"
		     "subscriberData=%d",req->subscriptionId,
		     req->subscriptionKey,req->subscriberData));

  DBUG_RETURN(dictSignal(&tSignal,NULL,0,
			 0 /*use masternode id*/,
			 WAIT_CREATE_INDX_REQ /*WAIT_SUB_STOP__REQ*/,
			 -1, 100,
			 0, -1));
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
    tab= fetchGlobalTableImplRef(InitTable(this, ev->getTableName()));
    if (tab == 0)
    {
      DBUG_PRINT("error",("unable to find table %s", ev->getTableName()));
      delete ev;
      DBUG_RETURN(NULL);
    }
    if ((tab->m_status != NdbDictionary::Object::Retrieved) ||
        (tab->m_id != ev->m_table_id) ||
        (table_version_major(tab->m_version) !=
         table_version_major(ev->m_table_version)))
    {
      DBUG_PRINT("info", ("mismatch on verison in cache"));
      releaseTableGlobal(*tab, 1);
      tab= fetchGlobalTableImplRef(InitTable(this, ev->getTableName()));
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

  if (table.m_id != ev->m_table_id ||
      table_version_major(table.m_version) !=
      table_version_major(ev->m_table_version))
  {
    m_error.code = 241;
    delete ev;
    DBUG_RETURN(NULL);
  }
#ifndef DBUG_OFF
  char buf[128] = {0};
  mask.getText(buf);
  DBUG_PRINT("info",("attributeList_sz= %d, mask= %s", 
                     attributeList_sz, buf));
#endif

  
  if ( attributeList_sz > table.getNoOfColumns() )
  {
    m_error.code = 241;
    DBUG_PRINT("error",("Invalid version, too many columns"));
    delete ev;
    DBUG_RETURN(NULL);
  }

  assert( (int)attributeList_sz <= table.getNoOfColumns() );
  for(unsigned id= 0; ev->m_columns.size() < attributeList_sz; id++) {
    if ( id >= table.getNoOfColumns())
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
NdbDictInterface::execCREATE_EVNT_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_EVNT_CONF");

  m_buffer.clear();
  unsigned int len = signal->getLength() << 2;
  m_buffer.append((char *)&len, sizeof(len));
  m_buffer.append(signal->getDataPtr(), len);

  if (signal->m_noOfSections > 0) {
    m_buffer.append((char *)ptr[0].p, strlen((char *)ptr[0].p)+1);
  }

  const CreateEvntConf * const createEvntConf=
    CAST_CONSTPTR(CreateEvntConf, signal->getDataPtr());

  Uint32 subscriptionId = createEvntConf->getEventId();
  Uint32 subscriptionKey = createEvntConf->getEventKey();

  DBUG_PRINT("info",("nodeid=%d,subscriptionId=%d,subscriptionKey=%d",
		     refToNode(signal->theSendersBlockRef),
		     subscriptionId,subscriptionKey));
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execCREATE_EVNT_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execCREATE_EVNT_REF");

  const CreateEvntRef* const ref=
    CAST_CONSTPTR(CreateEvntRef, signal->getDataPtr());
  m_error.code= ref->getErrorCode();
  DBUG_PRINT("error",("error=%d,line=%d,node=%d",ref->getErrorCode(),
		      ref->getErrorLine(),ref->getErrorNode()));
  if (m_error.code == CreateEvntRef::NotMaster)
    m_masterNodeId = ref->getMasterNode();
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_STOP_CONF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_STOP_CONF");
  const SubStopConf * const subStopConf=
    CAST_CONSTPTR(SubStopConf, signal->getDataPtr());

  Uint32 subscriptionId = subStopConf->subscriptionId;
  Uint32 subscriptionKey = subStopConf->subscriptionKey;
  Uint32 subscriberData = subStopConf->subscriberData;

  DBUG_PRINT("info",("subscriptionId=%d,subscriptionKey=%d,subscriberData=%d",
		     subscriptionId,subscriptionKey,subscriberData));
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_STOP_REF(NdbApiSignal * signal,
				     LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_STOP_REF");
  const SubStopRef * const subStopRef=
    CAST_CONSTPTR(SubStopRef, signal->getDataPtr());

  Uint32 subscriptionId = subStopRef->subscriptionId;
  Uint32 subscriptionKey = subStopRef->subscriptionKey;
  Uint32 subscriberData = subStopRef->subscriberData;
  m_error.code= subStopRef->errorCode;

  DBUG_PRINT("error",("subscriptionId=%d,subscriptionKey=%d,subscriberData=%d,error=%d",
		      subscriptionId,subscriptionKey,subscriberData,m_error.code));
  if (m_error.code == SubStopRef::NotMaster)
    m_masterNodeId = subStopRef->m_masterNodeId;
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_START_CONF(NdbApiSignal * signal,
				     LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_START_CONF");
  const SubStartConf * const subStartConf=
    CAST_CONSTPTR(SubStartConf, signal->getDataPtr());

  Uint32 subscriptionId = subStartConf->subscriptionId;
  Uint32 subscriptionKey = subStartConf->subscriptionKey;
  SubscriptionData::Part part = 
    (SubscriptionData::Part)subStartConf->part;
  Uint32 subscriberData = subStartConf->subscriberData;

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
		     subscriptionId,subscriptionKey,subscriberData));
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execSUB_START_REF(NdbApiSignal * signal,
				    LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execSUB_START_REF");
  const SubStartRef * const subStartRef=
    CAST_CONSTPTR(SubStartRef, signal->getDataPtr());
  m_error.code= subStartRef->errorCode;
  if (m_error.code == SubStartRef::NotMaster)
    m_masterNodeId = subStartRef->m_masterNodeId;
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Drop event
 */
int 
NdbDictionaryImpl::dropEvent(const char * eventName)
{
  DBUG_ENTER("NdbDictionaryImpl::dropEvent");
  DBUG_PRINT("info", ("name=%s", eventName));

  NdbEventImpl *evnt = getEvent(eventName); // allocated
  if (evnt == NULL) {
    if (m_error.code != 723 && // no such table
        m_error.code != 241)   // invalid table
      DBUG_RETURN(-1);
    DBUG_PRINT("info", ("no table err=%d, drop by name alone", m_error.code));
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
  } else {
    // loop over MAX_ATTRIBUTES_IN_TABLE ...
    Uint32 i;
    DBUG_PRINT("info", ("missing table definition, looping over "
                        "MAX_ATTRIBUTES_IN_TABLE(%d)",
                        MAX_ATTRIBUTES_IN_TABLE));
    for (i = 0; i < MAX_ATTRIBUTES_IN_TABLE; i++) {
      char bename[MAX_TAB_NAME_SIZE];
      // XXX should get name from NdbBlob
      sprintf(bename, "NDB$BLOBEVENT_%s_%u", evnt.getName(), i);
      NdbEventImpl* bevnt = new NdbEventImpl();
      bevnt->setName(bename);
      (void)m_receiver.dropEvent(*bevnt);
      delete bevnt;
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
		    -1, 100,
		    0, -1);
}

void
NdbDictInterface::execDROP_EVNT_CONF(NdbApiSignal * signal,
				     LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_EVNT_CONF");
  m_waiter.signal(NO_WAIT);  
  DBUG_VOID_RETURN;
}

void
NdbDictInterface::execDROP_EVNT_REF(NdbApiSignal * signal,
				    LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbDictInterface::execDROP_EVNT_REF");
  const DropEvntRef* const ref=
    CAST_CONSTPTR(DropEvntRef, signal->getDataPtr());
  m_error.code= ref->getErrorCode();

  DBUG_PRINT("info",("ErrorCode=%u Errorline=%u ErrorNode=%u",
	     ref->getErrorCode(), ref->getErrorLine(), ref->getErrorNode()));
  if (m_error.code == DropEvntRef::NotMaster)
    m_masterNodeId = ref->getMasterNode();
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * List objects or indexes
 */
int
NdbDictionaryImpl::listObjects(List& list, NdbDictionary::Object::Type type)
{
  ListTablesReq req;
  req.requestData = 0;
  req.setTableType(getKernelConstant(type, objectTypeMapping, 0));
  req.setListNames(true);
  return m_receiver.listObjects(list, req.requestData, m_ndb.usingFullyQualifiedNames());
}

int
NdbDictionaryImpl::listIndexes(List& list, Uint32 indexId)
{
  ListTablesReq req;
  req.requestData = 0;
  req.setTableId(indexId);
  req.setListNames(true);
  req.setListIndexes(true);
  return m_receiver.listObjects(list, req.requestData, m_ndb.usingFullyQualifiedNames());
}

int
NdbDictInterface::listObjects(NdbDictionary::Dictionary::List& list,
			      Uint32 requestData, bool fullyQualifiedNames)
{
  NdbApiSignal tSignal(m_reference);
  ListTablesReq* const req = CAST_PTR(ListTablesReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = 0;
  req->requestData = requestData;
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber = GSN_LIST_TABLES_REQ;
  tSignal.theLength = ListTablesReq::SignalLength;
  if (listObjects(&tSignal) != 0)
    return -1;
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
    element.id = ListTablesConf::getTableId(d);
    element.type = (NdbDictionary::Object::Type)
      getApiConstant(ListTablesConf::getTableType(d), objectTypeMapping, 0);
    element.state = (NdbDictionary::Object::State)
      getApiConstant(ListTablesConf::getTableState(d), objectStateMapping, 0);
    element.store = (NdbDictionary::Object::Store)
      getApiConstant(ListTablesConf::getTableStore(d), objectStoreMapping, 0);
    // table or index name
    Uint32 n = (data[pos++] + 3) >> 2;
    BaseString databaseName;
    BaseString schemaName;
    BaseString objectName;
    if ((element.type == NdbDictionary::Object::UniqueHashIndex) ||
	(element.type == NdbDictionary::Object::OrderedIndex)) {
      char * indexName = new char[n << 2];
      memcpy(indexName, &data[pos], n << 2);
      databaseName = Ndb::getDatabaseFromInternalName(indexName);
      schemaName = Ndb::getSchemaFromInternalName(indexName);
      objectName = BaseString(Ndb::externalizeIndexName(indexName, fullyQualifiedNames));
      delete [] indexName;
    } else if ((element.type == NdbDictionary::Object::SystemTable) || 
	       (element.type == NdbDictionary::Object::UserTable)) {
      char * tableName = new char[n << 2];
      memcpy(tableName, &data[pos], n << 2);
      databaseName = Ndb::getDatabaseFromInternalName(tableName);
      schemaName = Ndb::getSchemaFromInternalName(tableName);
      objectName = BaseString(Ndb::externalizeTableName(tableName, fullyQualifiedNames));
      delete [] tableName;
    }
    else {
      char * otherName = new char[n << 2];
      memcpy(otherName, &data[pos], n << 2);
      objectName = BaseString(otherName);
      delete [] otherName;
    }
    element.database = new char[databaseName.length() + 1]; 
    strcpy(element.database, databaseName.c_str());
    element.schema = new char[schemaName.length() + 1]; 
    strcpy(element.schema, schemaName.c_str());
    element.name = new char[objectName.length() + 1]; 
    strcpy(element.name, objectName.c_str());
    pos += n;
    count++;
  }
  return 0;
}

int
NdbDictInterface::listObjects(NdbApiSignal* signal)
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
    PollGuard poll_guard(m_transporter, &m_waiter, refToBlock(m_reference));
    Uint16 aNodeId = m_transporter->get_an_alive_node();
    if (aNodeId == 0) {
      m_error.code= 4009;
      return -1;
    }
    if (m_transporter->sendSignal(signal, aNodeId) != 0) {
      continue;
    }
    m_error.code= 0;
    int ret_val= poll_guard.wait_n_unlock(DICT_WAITFOR_TIMEOUT,
                                          aNodeId, WAIT_LIST_TABLES_CONF);
    // end protected
    if (ret_val == 0 && m_error.code == 0)
      return 0;
    if (ret_val == -2) //WAIT_NODE_FAILURE
      continue;
    return -1;
  }
  return -1;
}

void
NdbDictInterface::execLIST_TABLES_CONF(NdbApiSignal* signal,
				       LinearSectionPtr ptr[3])
{
  const unsigned off = ListTablesConf::HeaderLength;
  const unsigned len = (signal->getLength() - off);
  m_buffer.append(signal->getDataPtr() + off, len << 2);
  if (signal->getLength() < ListTablesConf::SignalLength) {
    // last signal has less than full length
    m_waiter.signal(NO_WAIT);
  }
}

int
NdbDictionaryImpl::forceGCPWait()
{
  return m_receiver.forceGCPWait();
}

int
NdbDictInterface::forceGCPWait()
{
  NdbApiSignal tSignal(m_reference);
  WaitGCPReq* const req = CAST_PTR(WaitGCPReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = 0;
  req->requestType = WaitGCPReq::CompleteForceStart;
  tSignal.theReceiversBlockNumber = DBDIH;
  tSignal.theVerId_signalNumber = GSN_WAIT_GCP_REQ;
  tSignal.theLength = WaitGCPReq::SignalLength;

  const Uint32 RETRIES = 100;
  for (Uint32 i = 0; i < RETRIES; i++)
  {
    m_transporter->lock_mutex();
    Uint16 aNodeId = m_transporter->get_an_alive_node();
    if (aNodeId == 0) {
      m_error.code= 4009;
      m_transporter->unlock_mutex();
      return -1;
    }
    if (m_transporter->sendSignal(&tSignal, aNodeId) != 0) {
      m_transporter->unlock_mutex();
      continue;
    }
    m_error.code= 0;
    m_waiter.m_node = aNodeId;
    m_waiter.m_state = WAIT_LIST_TABLES_CONF;
    m_waiter.wait(DICT_WAITFOR_TIMEOUT);
    m_transporter->unlock_mutex();    
    return 0;
  }
  return -1;
}

void
NdbDictInterface::execWAIT_GCP_CONF(NdbApiSignal* signal,
				    LinearSectionPtr ptr[3])
{
  const WaitGCPConf * const conf=
    CAST_CONSTPTR(WaitGCPConf, signal->getDataPtr());
  m_waiter.signal(NO_WAIT);
}

void
NdbDictInterface::execWAIT_GCP_REF(NdbApiSignal* signal,
				    LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);
}

NdbFilegroupImpl::NdbFilegroupImpl(NdbDictionary::Object::Type t)
  : NdbDictObjectImpl(t)
{
  m_extent_size = 0;
  m_undo_buffer_size = 0;
  m_logfile_group_id = ~0;
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

void
NdbTablespaceImpl::assign(const NdbTablespaceImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  m_name.assign(org.m_name);
  m_grow_spec = org.m_grow_spec;
  m_extent_size = org.m_extent_size;
  m_undo_free_words = org.m_undo_free_words;
  m_logfile_group_id = org.m_logfile_group_id;
  m_logfile_group_version = org.m_logfile_group_version;
  m_logfile_group_name.assign(org.m_logfile_group_name);
  m_undo_free_words = org.m_undo_free_words;
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

void
NdbLogfileGroupImpl::assign(const NdbLogfileGroupImpl& org)
{
  m_id = org.m_id;
  m_version = org.m_version;
  m_status = org.m_status;
  m_type = org.m_type;

  m_name.assign(org.m_name);
  m_grow_spec = org.m_grow_spec;
  m_extent_size = org.m_extent_size;
  m_undo_free_words = org.m_undo_free_words;
  m_logfile_group_id = org.m_logfile_group_id;
  m_logfile_group_version = org.m_logfile_group_version;
  m_logfile_group_name.assign(org.m_logfile_group_name);
  m_undo_free_words = org.m_undo_free_words;
}

NdbFileImpl::NdbFileImpl(NdbDictionary::Object::Type t)
  : NdbDictObjectImpl(t)
{
  m_size = 0;
  m_free = 0;
  m_filegroup_id = ~0;
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

void
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
  m_path.assign(org.m_path);
  m_filegroup_name.assign(org.m_filegroup_name);
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

void
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
  m_path.assign(org.m_path);
  m_filegroup_name.assign(org.m_filegroup_name);
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

int
NdbDictInterface::create_file(const NdbFileImpl & file,
			      const NdbFilegroupImpl & group,
			      bool overwrite,
			      NdbDictObjectImpl* obj)
{
  DBUG_ENTER("NdbDictInterface::create_file"); 
  UtilBufferWriter w(m_buffer);
  DictFilegroupInfo::File f; f.init();
  snprintf(f.FileName, sizeof(f.FileName), file.m_path.c_str());
  f.FileType = file.m_type;
  f.FilegroupId = group.m_id;
  f.FilegroupVersion = group.m_version;
  f.FileSizeHi = (file.m_size >> 32);
  f.FileSizeLo = (file.m_size & 0xFFFFFFFF);
  
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
  req->senderData = 0;
  req->objType = file.m_type;
  req->requestInfo = 0;
  if (overwrite)
    req->requestInfo |= CreateFileReq::ForceCreateFile;
  
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[] = { CreateFileRef::Busy, CreateFileRef::NotMaster, 0};
  /*
    Send signal without time-out since creating files can take a very long
    time if the file is very big.
  */
  int ret = dictSignal(&tSignal, ptr, 1,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       -1, 100,
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
NdbDictInterface::execCREATE_FILE_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  const CreateFileConf* conf=
    CAST_CONSTPTR(CreateFileConf, signal->getDataPtr());
  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->fileId;
  data[1] = conf->fileVersion;
  
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_FILE_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  const CreateFileRef* ref = 
    CAST_CONSTPTR(CreateFileRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
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
  req->senderData = 0;
  req->file_id = file.m_id;
  req->file_version = file.m_version;

  int err[] = { DropFileRef::Busy, DropFileRef::NotMaster, 0};
  DBUG_RETURN(dictSignal(&tSignal, 0, 0,
	                 0, // master
		         WAIT_CREATE_INDX_REQ,
		         DICT_WAITFOR_TIMEOUT, 100,
		         err));
}

void
NdbDictInterface::execDROP_FILE_CONF(NdbApiSignal * signal,
					    LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execDROP_FILE_REF(NdbApiSignal * signal,
					   LinearSectionPtr ptr[3])
{
  const DropFileRef* ref = 
    CAST_CONSTPTR(DropFileRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
}

int
NdbDictInterface::create_filegroup(const NdbFilegroupImpl & group,
				   NdbDictObjectImpl* obj)
{
  DBUG_ENTER("NdbDictInterface::create_filegroup");
  UtilBufferWriter w(m_buffer);
  DictFilegroupInfo::Filegroup fg; fg.init();
  snprintf(fg.FilegroupName, sizeof(fg.FilegroupName), group.m_name.c_str());
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
  req->senderData = 0;
  req->objType = fg.FilegroupType;
  
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;

  int err[] = { CreateFilegroupRef::Busy, CreateFilegroupRef::NotMaster, 0};
  int ret = dictSignal(&tSignal, ptr, 1,
		       0, // master
		       WAIT_CREATE_INDX_REQ,
		       DICT_WAITFOR_TIMEOUT, 100,
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
NdbDictInterface::execCREATE_FILEGROUP_CONF(NdbApiSignal * signal,
					    LinearSectionPtr ptr[3])
{
  const CreateFilegroupConf* conf=
    CAST_CONSTPTR(CreateFilegroupConf, signal->getDataPtr());
  m_buffer.grow(4 * 2); // 2 words
  Uint32* data = (Uint32*)m_buffer.get_data();
  data[0] = conf->filegroupId;
  data[1] = conf->filegroupVersion;
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_FILEGROUP_REF(NdbApiSignal * signal,
					   LinearSectionPtr ptr[3])
{
  const CreateFilegroupRef* ref = 
    CAST_CONSTPTR(CreateFilegroupRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
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
  req->senderData = 0;
  req->filegroup_id = group.m_id;
  req->filegroup_version = group.m_version;
  
  int err[] = { DropFilegroupRef::Busy, DropFilegroupRef::NotMaster, 0};
  DBUG_RETURN(dictSignal(&tSignal, 0, 0,
                         0, // master
		         WAIT_CREATE_INDX_REQ,
		         DICT_WAITFOR_TIMEOUT, 100,
		         err));
}

void
NdbDictInterface::execDROP_FILEGROUP_CONF(NdbApiSignal * signal,
					    LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execDROP_FILEGROUP_REF(NdbApiSignal * signal,
					   LinearSectionPtr ptr[3])
{
  const DropFilegroupRef* ref = 
    CAST_CONSTPTR(DropFilegroupRef, signal->getDataPtr());
  m_error.code = ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
}


int
NdbDictInterface::get_filegroup(NdbFilegroupImpl & dst,
				NdbDictionary::Object::Type type,
				const char * name){
  DBUG_ENTER("NdbDictInterface::get_filegroup");
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq * req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

  size_t strLen = strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = 0;
  req->requestType = 
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
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
  
  int r = dictSignal(&tSignal, ptr, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     DICT_WAITFOR_TIMEOUT, 100);
  if (r)
  {
    dst.m_id = -1;
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
    dst.m_logfile_group_name.assign(tmp.getName());
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
  
  dst.m_name.assign(fg.FilegroupName);
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
  req->senderData = 0;
  req->requestType =
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = id;
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_GET_TABINFOREQ;
  tSignal.theLength = GetTabInfoReq::SignalLength;

  int r = dictSignal(&tSignal, NULL, 1,
		     -1, // any node
		     WAIT_GET_TAB_INFO_REQ,
		     DICT_WAITFOR_TIMEOUT, 100);
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

  size_t strLen = strlen(name) + 1;

  req->senderRef = m_reference;
  req->senderData = 0;
  req->requestType =
    GetTabInfoReq::RequestByName | GetTabInfoReq::LongSignalConf;
  req->tableNameLen = strLen;
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
  
  int r = dictSignal(&tSignal, ptr, 1,
		     node,
		     WAIT_GET_TAB_INFO_REQ,
		     DICT_WAITFOR_TIMEOUT, 100);
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
    dst.m_filegroup_name.assign(tmp.getName());
  }
  else if(dst.m_type == NdbDictionary::Object::Datafile)
  {
    NdbDictionary::Tablespace tmp;
    get_filegroup(NdbTablespaceImpl::getImpl(tmp),
		  NdbDictionary::Object::Tablespace,
		  dst.m_filegroup_id);
    dst.m_filegroup_name.assign(tmp.getName());
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
  dst.m_path.assign(f.FileName);

  dst.m_filegroup_id= f.FilegroupId;
  dst.m_filegroup_version= f.FilegroupVersion;
  dst.m_free=  f.FileFreeExtents;
  return 0;
}

template class Vector<int>;
template class Vector<Uint16>;
template class Vector<Uint32>;
template class Vector<Vector<Uint32> >;
template class Vector<NdbTableImpl*>;
template class Vector<NdbColumnImpl*>;

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
