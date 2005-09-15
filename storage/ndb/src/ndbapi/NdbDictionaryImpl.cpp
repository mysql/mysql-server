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

#define DEBUG_PRINT 0
#define INCOMPATIBLE_VERSION -2

extern Uint64 g_latest_trans_gci;

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
  m_defaultValue = col.m_defaultValue;
  m_attrSize = col.m_attrSize; 
  m_arraySize = col.m_arraySize;
  m_keyInfoPos = col.m_keyInfoPos;
  m_blobTable = col.m_blobTable;
  // Do not copy m_facade !!

  return *this;
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
    break;
  case Olddecimal:
  case Olddecimalunsigned:
  case Decimal:
  case Decimalunsigned:
    m_precision = 10;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    break;
  case Char:
  case Varchar:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = default_cs;
    break;
  case Binary:
  case Varbinary:
  case Datetime:
  case Date:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    break;
  case Blob:
    m_precision = 256;
    m_scale = 8000;
    m_length = 4;
    m_cs = NULL;
    break;
  case Text:
    m_precision = 256;
    m_scale = 8000;
    m_length = 4;
    m_cs = default_cs;
    break;
  case Time:
  case Year:
  case Timestamp:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    break;
  case Bit:
    m_precision = 0;
    m_scale = 0;
    m_length = 1;
    m_cs = NULL;
    break;
  case Longvarchar:
    m_precision = 0;
    m_scale = 0;
    m_length = 1; // legal
    m_cs = default_cs;
    break;
  case Longvarbinary:
    m_precision = 0;
    m_scale = 0;
    m_length = 1; // legal
    m_cs = NULL;
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
}

NdbColumnImpl::~NdbColumnImpl()
{
}

bool
NdbColumnImpl::equal(const NdbColumnImpl& col) const 
{
  DBUG_ENTER("NdbColumnImpl::equal");
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
#ifdef ndb_dictionary_dkey_fixed
  if(m_pk){
    if(m_distributionKey != col.m_distributionKey){
      DBUG_RETURN(false);
    }
  }
#endif
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
  } else if(!strcmp(name, "NDB$FRAGMENT_MEMORY")){
    col->setType(NdbDictionary::Column::Bigunsigned);
    col->m_impl.m_attrId = AttributeHeader::FRAGMENT_MEMORY;
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
  } else if(!strcmp(name, "NDB$RECORDS_IN_RANGE")){
    col->setType(NdbDictionary::Column::Unsigned);
    col->m_impl.m_attrId = AttributeHeader::RECORDS_IN_RANGE;
    col->m_impl.m_attrSize = 4;
    col->m_impl.m_arraySize = 4;
  } else {
    abort();
  }
  return col;
}

/**
 * NdbTableImpl
 */

NdbTableImpl::NdbTableImpl()
  : NdbDictionary::Table(* this), m_facade(this)
{
  init();
}

NdbTableImpl::NdbTableImpl(NdbDictionary::Table & f)
  : NdbDictionary::Table(* this), m_facade(&f)
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
}

void
NdbTableImpl::init(){
  m_changeMask= 0;
  m_tableId= RNIL;
  m_primaryTableId= RNIL;
  m_frm.clear();
  m_fragmentType= NdbDictionary::Object::DistrKeyHash;
  m_hashValueMask= 0;
  m_hashpointerValue= 0;
  m_logging= true;
  m_kvalue= 6;
  m_minLoadFactor= 78;
  m_maxLoadFactor= 80;
  m_keyLenInWords= 0;
  m_fragmentCount= 0;
  m_dictionary= NULL;
  m_index= NULL;
  m_indexType= NdbDictionary::Index::Undefined;
  m_noOfKeys= 0;
  m_noOfDistributionKeys= 0;
  m_noOfBlobs= 0;
  m_replicaCount= 0;
}

bool
NdbTableImpl::equal(const NdbTableImpl& obj) const 
{
  DBUG_ENTER("NdbTableImpl::equal");
  if ((m_internalName.c_str() == NULL) || 
      (strcmp(m_internalName.c_str(), "") == 0) ||
      (obj.m_internalName.c_str() == NULL) || 
      (strcmp(obj.m_internalName.c_str(), "") == 0)) {
    // Shallow equal
    if(strcmp(getName(), obj.getName()) != 0){
      DBUG_PRINT("info",("name %s != %s",getName(),obj.getName()));
      DBUG_RETURN(false);    
    }
  } else 
    // Deep equal
    if(strcmp(m_internalName.c_str(), obj.m_internalName.c_str()) != 0){
    {
      DBUG_PRINT("info",("m_internalName %s != %s",
			 m_internalName.c_str(),obj.m_internalName.c_str()));
      DBUG_RETURN(false);
    }
  }
  if(m_fragmentType != obj.m_fragmentType){
    DBUG_PRINT("info",("m_fragmentType %d != %d",m_fragmentType,obj.m_fragmentType));
    DBUG_RETURN(false);
  }
  if(m_columns.size() != obj.m_columns.size()){
    DBUG_PRINT("info",("m_columns.size %d != %d",m_columns.size(),obj.m_columns.size()));
    DBUG_RETURN(false);
  }

  for(unsigned i = 0; i<obj.m_columns.size(); i++){
    if(!m_columns[i]->equal(* obj.m_columns[i])){
      DBUG_PRINT("info",("m_columns [%d] != [%d]",i,i));
      DBUG_RETURN(false);
    }
  }
  
  if(m_logging != obj.m_logging){
    DBUG_PRINT("info",("m_logging %d != %d",m_logging,obj.m_logging));
    DBUG_RETURN(false);
  }

  if(m_kvalue != obj.m_kvalue){
    DBUG_PRINT("info",("m_kvalue %d != %d",m_kvalue,obj.m_kvalue));
    DBUG_RETURN(false);
  }

  if(m_minLoadFactor != obj.m_minLoadFactor){
    DBUG_PRINT("info",("m_minLoadFactor %d != %d",m_minLoadFactor,obj.m_minLoadFactor));
    DBUG_RETURN(false);
  }

  if(m_maxLoadFactor != obj.m_maxLoadFactor){
    DBUG_PRINT("info",("m_maxLoadFactor %d != %d",m_maxLoadFactor,obj.m_maxLoadFactor));
    DBUG_RETURN(false);
  }
  
   DBUG_RETURN(true);
}

void
NdbTableImpl::assign(const NdbTableImpl& org)
{
  m_tableId = org.m_tableId;
  m_internalName.assign(org.m_internalName);
  updateMysqlName();
  m_externalName.assign(org.m_externalName);
  m_newExternalName.assign(org.m_newExternalName);
  m_frm.assign(org.m_frm.get_data(), org.m_frm.length());
  m_ng.assign(org.m_ng.get_data(), org.m_ng.length());
  m_fragmentType = org.m_fragmentType;
  m_fragmentCount = org.m_fragmentCount;

  for(unsigned i = 0; i<org.m_columns.size(); i++){
    NdbColumnImpl * col = new NdbColumnImpl();
    const NdbColumnImpl * iorg = org.m_columns[i];
    (* col) = (* iorg);
    m_columns.push_back(col);
  }

  m_logging = org.m_logging;
  m_kvalue = org.m_kvalue;
  m_minLoadFactor = org.m_minLoadFactor;
  m_maxLoadFactor = org.m_maxLoadFactor;
  
  if (m_index != 0)
    delete m_index;
  m_index = org.m_index;
  
  m_noOfDistributionKeys = org.m_noOfDistributionKeys;
  m_noOfKeys = org.m_noOfKeys;
  m_keyLenInWords = org.m_keyLenInWords;
  m_noOfBlobs = org.m_noOfBlobs;

  m_version = org.m_version;
  m_status = org.m_status;
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
  m_facade(this)
{
  init();
}

NdbIndexImpl::NdbIndexImpl(NdbDictionary::Index & f) : 
  NdbDictionary::Index(* this), 
  m_facade(&f)
{
  init();
}

void NdbIndexImpl::init()
{
  m_indexId= RNIL;
  m_type= NdbDictionary::Index::Undefined;
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
  m_facade(this)
{
  init();
}

NdbEventImpl::NdbEventImpl(NdbDictionary::Event & f) : 
  NdbDictionary::Event(* this),
  m_facade(&f)
{
  init();
}

void NdbEventImpl::init()
{
  m_eventId= RNIL;
  m_eventKey= RNIL;
  m_tableId= RNIL;
  mi_type= 0;
  m_dur= NdbDictionary::Event::ED_UNDEFINED;
  m_tableImpl= NULL;
}

NdbEventImpl::~NdbEventImpl()
{
  for (unsigned i = 0; i < m_columns.size(); i++)
    delete  m_columns[i];
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
  m_tableImpl= &NdbTableImpl::getImpl(table);
  m_tableName.assign(m_tableImpl->getName());
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

int NdbEventImpl::getNoOfEventColumns() const
{
  return m_attrIds.size() + m_columns.size();
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
      delete NdbDictionary::Column::FRAGMENT_MEMORY;
      delete NdbDictionary::Column::ROW_COUNT;
      delete NdbDictionary::Column::COMMIT_COUNT;
      delete NdbDictionary::Column::ROW_SIZE;
      delete NdbDictionary::Column::RANGE_NO;
      delete NdbDictionary::Column::RECORDS_IN_RANGE;
      NdbDictionary::Column::FRAGMENT= 0;
      NdbDictionary::Column::FRAGMENT_MEMORY= 0;
      NdbDictionary::Column::ROW_COUNT= 0;
      NdbDictionary::Column::COMMIT_COUNT= 0;
      NdbDictionary::Column::ROW_SIZE= 0;
      NdbDictionary::Column::RANGE_NO= 0;
      NdbDictionary::Column::RECORDS_IN_RANGE= 0;
    }
    m_globalHash->unlock();
  } else {
    assert(curr == 0);
  }
}

Ndb_local_table_info *
NdbDictionaryImpl::fetchGlobalTableImpl(const BaseString& internalTableName)
{
  NdbTableImpl *impl;

  m_globalHash->lock();
  impl = m_globalHash->get(internalTableName.c_str());
  m_globalHash->unlock();

  if (impl == 0){
    impl = m_receiver.getTable(internalTableName,
			       m_ndb.usingFullyQualifiedNames());
    m_globalHash->lock();
    m_globalHash->put(internalTableName.c_str(), impl);
    m_globalHash->unlock();
    
    if(impl == 0){
      return 0;
    }
  }

  Ndb_local_table_info *info=
    Ndb_local_table_info::create(impl, m_local_table_data_size);

  m_localHash.put(internalTableName.c_str(), info);

  m_ndb.theFirstTupleId[impl->getTableId()] = ~0;
  m_ndb.theLastTupleId[impl->getTableId()]  = ~0;
  
  return info;
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
      NdbDictionary::Column::FRAGMENT_MEMORY= 
	NdbColumnImpl::create_pseudo("NDB$FRAGMENT_MEMORY");
      NdbDictionary::Column::ROW_COUNT= 
	NdbColumnImpl::create_pseudo("NDB$ROW_COUNT");
      NdbDictionary::Column::COMMIT_COUNT= 
	NdbColumnImpl::create_pseudo("NDB$COMMIT_COUNT");
      NdbDictionary::Column::ROW_SIZE=
	NdbColumnImpl::create_pseudo("NDB$ROW_SIZE");
      NdbDictionary::Column::RANGE_NO= 
	NdbColumnImpl::create_pseudo("NDB$RANGE_NO");
      NdbDictionary::Column::RECORDS_IN_RANGE= 
	NdbColumnImpl::create_pseudo("NDB$RECORDS_IN_RANGE");
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
  const BaseString internalName(
    m_ndb.internalize_index_name(table, index->getName()));
  return getTable(m_ndb.externalizeTableName(internalName.c_str()));
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
NdbDictInterface::dictSignal(NdbApiSignal* signal, 
			     LinearSectionPtr ptr[3],int noLSP,
			     const int useMasterNodeId,
			     const Uint32 RETRIES,
			     const WaitSignalType wst,
			     const int theWait,
			     const int *errcodes,
			     const int noerrcodes,
			     const int temporaryMask)
{
  DBUG_ENTER("NdbDictInterface::dictSignal");
  DBUG_PRINT("enter", ("useMasterNodeId: %d", useMasterNodeId));
  for(Uint32 i = 0; i<RETRIES; i++){
    //if (useMasterNodeId == 0)
    m_buffer.clear();

    // Protected area
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(m_transporter, &m_waiter, refToBlock(m_reference));
    Uint32 aNodeId;
    if (useMasterNodeId) {
      if ((m_masterNodeId == 0) ||
	  (!m_transporter->get_node_alive(m_masterNodeId))) {
	m_masterNodeId = m_transporter->get_an_alive_node();
      }//if
      aNodeId = m_masterNodeId;
    } else {
      aNodeId = m_transporter->get_an_alive_node();
    }
    if(aNodeId == 0){
      m_error.code= 4009;
      DBUG_RETURN(-1);
    }
    {
      int r;
      if (ptr) {
#ifdef EVENT_DEBUG
	printf("Long signal %d ptr", noLSP);
	for (int q=0;q<noLSP;q++) {
	  printf(" sz %d", ptr[q].sz);
	}
	printf("\n");
#endif
	r = m_transporter->sendFragmentedSignal(signal, aNodeId, ptr, noLSP);
      } else {
#ifdef EVENT_DEBUG
	printf("Short signal\n");
#endif
	r = m_transporter->sendSignal(signal, aNodeId);
      }
      if(r != 0){
	continue;
      }
    }
    
    m_error.code= 0;
    int ret_val= poll_guard.wait_n_unlock(theWait, aNodeId, wst);
    // End of Protected area  
    
    if(ret_val == 0 && m_error.code == 0){
      // Normal return
      DBUG_RETURN(0);
    }
    
    /**
     * Handle error codes
     */
    if(ret_val == -2) //WAIT_NODE_FAILURE
      continue;

    if(m_waiter.m_state == WST_WAIT_TIMEOUT)
    {
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
    if (errcodes) {
      int doContinue = 0;
      for (int j=0; j < noerrcodes; j++)
	if(m_error.code == errcodes[j]) {
	  doContinue = 1;
	  break;
	}
      if (doContinue)
	continue;
    }

    DBUG_RETURN(-1);
  }
  DBUG_RETURN(-1);
}
#if 0
/*
  Get dictionary information for a table using table id as reference

  DESCRIPTION
    Sends a GET_TABINFOREQ signal containing the table id
 */
NdbTableImpl *
NdbDictInterface::getTable(int tableId, bool fullyQualifiedNames)
{
  NdbApiSignal tSignal(m_reference);
  GetTabInfoReq* const req = CAST_PTR(GetTabInfoReq, tSignal.getDataPtrSend());

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
#endif


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
  m_buffer.grow(namelen_words*4);
  m_buffer.append(name.c_str(), namelen);

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
  int errCodes[] = {GetTabInfoRef::Busy };

  int r = dictSignal(signal,ptr,noOfSections,
		     0/*do not use masternode id*/,
		     100,
		     WAIT_GET_TAB_INFO_REQ,
		     WAITFOR_RESPONSE_TIMEOUT,
		     errCodes, 1);
  if (r) return 0;

  NdbTableImpl * rt = 0;
  m_error.code= parseTableInfo(&rt, 
			       (Uint32*)m_buffer.get_data(), 
			       m_buffer.length() / 4, fullyQualifiedNames);
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
  const GetTabInfoRef* ref = CAST_CONSTPTR(GetTabInfoRef, signal->getDataPtr());

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
				 bool fullyQualifiedNames)
{
  DBUG_ENTER("NdbDictInterface::parseTableInfo");

  SimplePropertiesLinearReader it(data, len);
  DictTabInfo::Table tableDesc; tableDesc.init();
  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::unpack(it, &tableDesc, 
			       DictTabInfo::TableMapping, 
			       DictTabInfo::TableMappingSize, 
			       true, true);
  
  if(s != SimpleProperties::Break){
    DBUG_RETURN(703);
  }
  const char * internalName = tableDesc.TableName;
  const char * externalName = Ndb::externalizeTableName(internalName, fullyQualifiedNames);

  NdbTableImpl * impl = new NdbTableImpl();
  impl->m_tableId = tableDesc.TableId;
  impl->m_version = tableDesc.TableVersion;
  impl->m_status = NdbDictionary::Object::Retrieved;
  impl->m_internalName.assign(internalName);
  impl->updateMysqlName();
  impl->m_externalName.assign(externalName);

  impl->m_frm.assign(tableDesc.FrmData, tableDesc.FrmLen);
  impl->m_ng.assign(tableDesc.FragmentData, tableDesc.FragmentDataLen);
  
  impl->m_fragmentType = (NdbDictionary::Object::FragmentType)
    getApiConstant(tableDesc.FragmentType, 
		   fragmentTypeMapping, 
		   (Uint32)NdbDictionary::Object::FragUndefined);
  
  impl->m_logging = tableDesc.TableLoggedFlag;
  impl->m_kvalue = tableDesc.TableKValue;
  impl->m_minLoadFactor = tableDesc.MinLoadFactor;
  impl->m_maxLoadFactor = tableDesc.MaxLoadFactor;

  impl->m_indexType = (NdbDictionary::Index::Type)
    getApiConstant(tableDesc.TableType,
		   indexTypeMapping,
		   NdbDictionary::Index::Undefined);
  
  if(impl->m_indexType == NdbDictionary::Index::Undefined){
  } else {
    const char * externalPrimary = 
      Ndb::externalizeTableName(tableDesc.PrimaryTable, fullyQualifiedNames);
    impl->m_primaryTable.assign(externalPrimary);
  }
  
  Uint32 keyInfoPos = 0;
  Uint32 keyCount = 0;
  Uint32 blobCount = 0;
  Uint32 distKeys = 0;
  
  Uint32 i;
  for(i = 0; i < tableDesc.NoOfAttributes; i++) {
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    s = SimpleProperties::unpack(it, 
				 &attrDesc, 
				 DictTabInfo::AttributeMapping, 
				 DictTabInfo::AttributeMappingSize, 
				 true, true);
    if(s != SimpleProperties::Break){
      delete impl;
      DBUG_RETURN(703);
    }
    
    NdbColumnImpl * col = new NdbColumnImpl();
    col->m_attrId = attrDesc.AttributeId;
    col->setName(attrDesc.AttributeName);

    // check type and compute attribute size and array size
    if (! attrDesc.translateExtType()) {
      delete impl;
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
      DBUG_RETURN(703);
    }
    if (col->getCharType()) {
      col->m_cs = get_charset(cs_number, MYF(0));
      if (col->m_cs == NULL) {
        delete impl;
        DBUG_RETURN(743);
      }
    }
    col->m_attrSize = (1 << attrDesc.AttributeSize) / 8;
    col->m_arraySize = attrDesc.AttributeArraySize;
    if(attrDesc.AttributeSize == 0)
    {
      col->m_attrSize = 4;
      col->m_arraySize = (attrDesc.AttributeArraySize + 31) >> 5;
    }
    
    col->m_pk = attrDesc.AttributeKeyFlag;
    col->m_distributionKey = attrDesc.AttributeDKey;
    col->m_nullable = attrDesc.AttributeNullableFlag;
    col->m_autoIncrement = (attrDesc.AttributeAutoIncrement ? true : false);
    col->m_autoIncrementInitialValue = ~0;
    col->m_defaultValue.assign(attrDesc.AttributeDefaultValue);

    if(attrDesc.AttributeKeyFlag){
      col->m_keyInfoPos = keyInfoPos + 1;
      keyInfoPos += ((col->m_attrSize * col->m_arraySize + 3) / 4);
      keyCount++;
      
      if(attrDesc.AttributeDKey)
	distKeys++;
    } else {
      col->m_keyInfoPos = 0;
    }
    if (col->getBlobType())
      blobCount++;
    NdbColumnImpl * null = 0;
    impl->m_columns.fill(attrDesc.AttributeId, null);
    if(impl->m_columns[attrDesc.AttributeId] != 0){
      delete col;
      delete impl;
      DBUG_RETURN(703);
    }
    impl->m_columns[attrDesc.AttributeId] = col;
    it.next();
  }

  impl->m_noOfKeys = keyCount;
  impl->m_keyLenInWords = keyInfoPos;
  impl->m_noOfBlobs = blobCount;
  impl->m_noOfDistributionKeys = distKeys;

  if(tableDesc.FragmentDataLen > 0)
  {
    Uint16 replicaCount = tableDesc.FragmentData[0];
    Uint16 fragCount = tableDesc.FragmentData[1];

    impl->m_replicaCount = replicaCount;
    impl->m_fragmentCount = fragCount;
    DBUG_PRINT("info", ("replicaCount=%x , fragCount=%x",replicaCount,fragCount));
    for(i = 0; i<(fragCount*replicaCount); i++)
    {
      impl->m_fragments.push_back(tableDesc.FragmentData[i+2]);
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
    impl->m_fragmentCount = tableDesc.FragmentCount;
    impl->m_replicaCount = 0;
    impl->m_hashValueMask = 0;
    impl->m_hashpointerValue = 0;
  }

  if(distKeys == 0)
  {
    for(i = 0; i < tableDesc.NoOfAttributes; i++)
    {
      if(impl->m_columns[i]->getPrimaryKey())
	impl->m_columns[i]->m_distributionKey = true;
    }
  }
  
  * ret = impl;

  DBUG_RETURN(0);
}

/*****************************************************************
 * Create table and alter table
 */
int
NdbDictionaryImpl::createTable(NdbTableImpl &t)
{ 
  DBUG_ENTER("NdbDictionaryImpl::createTable");
  if (m_receiver.createTable(m_ndb, t) != 0)
  {
    DBUG_RETURN(-1);
  }
  if (t.m_noOfBlobs == 0)
  {
    DBUG_RETURN(0);
  }
  // update table def from DICT
  Ndb_local_table_info *info=
    get_local_table_info(t.m_internalName,false);
  if (info == NULL) {
    m_error.code= 709;
    DBUG_RETURN(-1);
  }
  if (createBlobTables(*(info->m_table_impl)) != 0) {
    int save_code = m_error.code;
    (void)dropTable(t);
    m_error.code= save_code;
    DBUG_RETURN(-1);
  }
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
    if (createTable(bt) != 0)
    {
      DBUG_RETURN(-1);
    }
    // Save BLOB table handle
    Ndb_local_table_info *info=
      get_local_table_info(bt.m_internalName, false);
    if (info == 0)
    {
      DBUG_RETURN(-1);
    }
    c.m_blobTable = info->m_table_impl;
  }
  DBUG_RETURN(0); 
}

int
NdbDictionaryImpl::addBlobTables(NdbTableImpl &t)
{
  unsigned n= t.m_noOfBlobs;
  DBUG_ENTER("NdbDictioanryImpl::addBlobTables");
  // optimized for blob column being the last one
  // and not looking for more than one if not neccessary
  for (unsigned i = t.m_columns.size(); i > 0 && n > 0;) {
    i--;
    NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    n--;
    char btname[NdbBlobImpl::BlobTableNameSize];
    NdbBlob::getBlobTableName(btname, &t, &c);
    // Save BLOB table handle
    NdbTableImpl * cachedBlobTable = getTable(btname);
    if (cachedBlobTable == 0) {
      DBUG_RETURN(-1);
    }
    c.m_blobTable = cachedBlobTable;
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
  if((local= get_local_table_info(originalInternalName, false)) == 0)
  {
    m_error.code = 709;
    DBUG_RETURN(-1);
  }

  // Alter the table
  int ret = m_receiver.alterTable(m_ndb, impl);
  if(ret == 0){
    // Remove cached information and let it be refreshed at next access
    m_globalHash->lock();
    local->m_table_impl->m_status = NdbDictionary::Object::Invalid;
    m_globalHash->drop(local->m_table_impl);
    m_globalHash->unlock();
    m_localHash.drop(originalInternalName);
  }
  DBUG_RETURN(ret);
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
  DBUG_ENTER("NdbDictInterface::createOrAlterTable");
  unsigned i;
  if((unsigned)impl.getNoOfPrimaryKeys() > NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY){
    m_error.code= 4317;
    DBUG_RETURN(-1);
  }
  unsigned sz = impl.m_columns.size();
  if (sz > NDB_MAX_ATTRIBUTES_IN_TABLE){
    m_error.code= 4318;
    DBUG_RETURN(-1);
  }

  if (!impl.m_newExternalName.empty()) {
    impl.m_externalName.assign(impl.m_newExternalName);
    AlterTableReq::setNameFlag(impl.m_changeMask, true);
  }

  //validate();
  //aggregate();

  const BaseString internalName(
    ndb.internalize_table_name(impl.m_externalName.c_str()));
  impl.m_internalName.assign(internalName);
  impl.updateMysqlName();
  UtilBufferWriter w(m_buffer);
  DictTabInfo::Table tmpTab;
  tmpTab.init();
  BaseString::snprintf(tmpTab.TableName,
	   sizeof(tmpTab.TableName),
	   internalName.c_str());

  bool haveAutoIncrement = false;
  Uint64 autoIncrementValue = 0;
  Uint32 distKeys= 0;
  for(i = 0; i<sz; i++){
    const NdbColumnImpl * col = impl.m_columns[i];
    if(col == 0)
      continue;
    if (col->m_autoIncrement) {
      if (haveAutoIncrement) {
        m_error.code= 4335;
        DBUG_RETURN(-1);
      }
      haveAutoIncrement = true;
      autoIncrementValue = col->m_autoIncrementInitialValue;
    }
    if (col->m_distributionKey)
      distKeys++;
  }
  if (distKeys == impl.m_noOfKeys)
    distKeys= 0;
  impl.m_noOfDistributionKeys= distKeys;


  // Check max length of frm data
  if (impl.m_frm.length() > MAX_FRM_DATA_SIZE){
    m_error.code= 1229;
    DBUG_RETURN(-1);
  }
  tmpTab.FrmLen = impl.m_frm.length();
  memcpy(tmpTab.FrmData, impl.m_frm.get_data(), impl.m_frm.length());
  tmpTab.FragmentDataLen = impl.m_ng.length();
  memcpy(tmpTab.FragmentData, impl.m_ng.get_data(), impl.m_ng.length());

  tmpTab.TableLoggedFlag = impl.m_logging;
  tmpTab.TableKValue = impl.m_kvalue;
  tmpTab.MinLoadFactor = impl.m_minLoadFactor;
  tmpTab.MaxLoadFactor = impl.m_maxLoadFactor;
  tmpTab.TableType = DictTabInfo::UserTable;
  tmpTab.PrimaryTableId = impl.m_primaryTableId;
  tmpTab.NoOfAttributes = sz;
  
  tmpTab.FragmentType = getKernelConstant(impl.m_fragmentType,
					  fragmentTypeMapping,
					  DictTabInfo::AllNodesSmallTable);
  tmpTab.TableVersion = rand();

  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w, 
			     &tmpTab,
			     DictTabInfo::TableMapping, 
			     DictTabInfo::TableMappingSize, true);
  
  if(s != SimpleProperties::Eof){
    abort();
  }
  
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
    tmpAttr.AttributeId = i;
    tmpAttr.AttributeKeyFlag = col->m_pk;
    tmpAttr.AttributeNullableFlag = col->m_nullable;
    tmpAttr.AttributeDKey = distKeys ? col->m_distributionKey : 0;

    tmpAttr.AttributeExtType = (Uint32)col->m_type;
    tmpAttr.AttributeExtPrecision = ((unsigned)col->m_precision & 0xFFFF);
    tmpAttr.AttributeExtScale = col->m_scale;
    tmpAttr.AttributeExtLength = col->m_length;

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
    if (col->m_pk && ! NdbSqlUtil::usable_in_pk(col->m_type, col->m_cs)) {
      m_error.code= (col->m_cs != 0 ? 743 : 739);
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

  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  
  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32*)m_buffer.get_data();
  ptr[0].sz = m_buffer.length() / 4;
  int ret;
  if (alter)
  {
    AlterTableReq * const req = 
      CAST_PTR(AlterTableReq, tSignal.getDataPtrSend());
    
    req->senderRef = m_reference;
    req->senderData = 0;
    req->changeMask = impl.m_changeMask;
    req->tableId = impl.m_tableId;
    req->tableVersion = impl.m_version;;
    tSignal.theVerId_signalNumber   = GSN_ALTER_TABLE_REQ;
    tSignal.theLength = AlterTableReq::SignalLength;
    ret= alterTable(&tSignal, ptr);
  }
  else
  {
    CreateTableReq * const req = 
      CAST_PTR(CreateTableReq, tSignal.getDataPtrSend());
    
    req->senderRef = m_reference;
    req->senderData = 0;
    tSignal.theVerId_signalNumber   = GSN_CREATE_TABLE_REQ;
    tSignal.theLength = CreateTableReq::SignalLength;
    ret= createTable(&tSignal, ptr);

    if (ret)
      DBUG_RETURN(ret);

    if (haveAutoIncrement) {
      if (!ndb.setAutoIncrementValue(impl.m_externalName.c_str(),
				     autoIncrementValue)) {
	if (ndb.theError.code == 0) {
	  m_error.code= 4336;
	  ndb.theError = m_error;
	} else
	  m_error= ndb.theError;
	ret = -1; // errorcode set in initialize_autoincrement
      }
    }
  }
  DBUG_RETURN(ret);
}

int
NdbDictInterface::createTable(NdbApiSignal* signal, LinearSectionPtr ptr[3])
{
#if DEBUG_PRINT
  ndbout_c("BufferLen = %d", ptr[0].sz);
  SimplePropertiesLinearReader r(ptr[0].p, ptr[0].sz);
  r.printAll(ndbout);
#endif
  const int noErrCodes = 2;
  int errCodes[noErrCodes] = 
     {CreateTableRef::Busy,
      CreateTableRef::NotMaster};
  return dictSignal(signal,ptr,1,
		    1/*use masternode id*/,
		    100,
		    WAIT_CREATE_INDX_REQ,
		    WAITFOR_RESPONSE_TIMEOUT,
		    errCodes,noErrCodes);
}


void
NdbDictInterface::execCREATE_TABLE_CONF(NdbApiSignal * signal,
					LinearSectionPtr ptr[3])
{
#if 0
  const CreateTableConf* const conf=
    CAST_CONSTPTR(CreateTableConf, signal->getDataPtr());
  Uint32 tableId= conf->tableId;
  Uint32 tableVersion= conf->tableVersion;
#endif
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_TABLE_REF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  const CreateTableRef* const ref=
    CAST_CONSTPTR(CreateTableRef, signal->getDataPtr());
  m_error.code= ref->errorCode;
  m_masterNodeId = ref->masterNodeId;
  m_waiter.signal(NO_WAIT);  
}

int
NdbDictInterface::alterTable(NdbApiSignal* signal, LinearSectionPtr ptr[3])
{
#if DEBUG_PRINT
  ndbout_c("BufferLen = %d", ptr[0].sz);
  SimplePropertiesLinearReader r(ptr[0].p, ptr[0].sz);
  r.printAll(ndbout);
#endif
  const int noErrCodes = 2;
  int errCodes[noErrCodes] =
    {AlterTableRef::NotMaster,
     AlterTableRef::Busy};
  int r = dictSignal(signal,ptr,1,
		     1/*use masternode id*/,
		     100,WAIT_ALTER_TAB_REQ,
		     WAITFOR_RESPONSE_TIMEOUT,
		     errCodes, noErrCodes);
  if(m_error.code == AlterTableRef::InvalidTableVersion) {
    // Clear caches and try again
    return INCOMPATIBLE_VERSION;
  }

  return r;
}

void
NdbDictInterface::execALTER_TABLE_CONF(NdbApiSignal * signal,
                                       LinearSectionPtr ptr[3])
{
  //AlterTableConf* const conf = CAST_CONSTPTR(AlterTableConf, signal->getDataPtr());
  m_waiter.signal(NO_WAIT);
}

void
NdbDictInterface::execALTER_TABLE_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  const AlterTableRef * const ref = 
    CAST_CONSTPTR(AlterTableRef, signal->getDataPtr());
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
    tab->m_status = NdbDictionary::Object::Invalid;
    m_globalHash->drop(tab);
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

  if (impl.m_indexType != NdbDictionary::Index::Undefined) {
    m_receiver.m_error.code= 1228;
    return -1;
  }

  List list;
  if ((res = listIndexes(list, impl.m_tableId)) == -1){
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
  if(ret == 0 || m_error.code == 709){
    const char * internalTableName = impl.m_internalName.c_str();

    
    m_localHash.drop(internalTableName);
    m_globalHash->lock();
    impl.m_status = NdbDictionary::Object::Invalid;
    m_globalHash->drop(&impl);
    m_globalHash->unlock();

    return 0;
  }
  
  return ret;
}

int
NdbDictionaryImpl::dropBlobTables(NdbTableImpl & t)
{
  DBUG_ENTER("NdbDictionaryImpl::dropBlobTables");
  for (unsigned i = 0; i < t.m_columns.size(); i++) {
    NdbColumnImpl & c = *t.m_columns[i];
    if (! c.getBlobType() || c.getPartSize() == 0)
      continue;
    char btname[NdbBlobImpl::BlobTableNameSize];
    NdbBlob::getBlobTableName(btname, &t, &c);
    if (dropTable(btname) != 0) {
      if (m_error.code != 709){
	DBUG_PRINT("exit",("error %u - exiting",m_error.code));
        DBUG_RETURN(-1);
      }
      DBUG_PRINT("info",("error %u - continuing",m_error.code));
    }
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
  
  DropTableReq * const req = CAST_PTR(DropTableReq, tSignal.getDataPtrSend());
  req->senderRef = m_reference;
  req->senderData = 0;
  req->tableId = impl.m_tableId;
  req->tableVersion = impl.m_version;

  return dropTable(&tSignal, 0);
}

int
NdbDictInterface::dropTable(NdbApiSignal* signal, LinearSectionPtr ptr[3])
{
  const int noErrCodes = 3;
  int errCodes[noErrCodes] =
         {DropTableRef::NoDropTableRecordAvailable,
          DropTableRef::NotMaster,
          DropTableRef::Busy};
  int r = dictSignal(signal,NULL,0,
		     1/*use masternode id*/,
		     100,WAIT_DROP_TAB_REQ,
		     WAITFOR_RESPONSE_TIMEOUT,
		     errCodes, noErrCodes);
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
  const DropTableRef* const ref = CAST_CONSTPTR(DropTableRef, signal->getDataPtr());
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
  impl.m_status = NdbDictionary::Object::Invalid;
  m_globalHash->drop(&impl);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

int
NdbDictionaryImpl::removeCachedObject(NdbTableImpl & impl)
{
  const char * internalTableName = impl.m_internalName.c_str();

  m_localHash.drop(internalTableName);  
  m_globalHash->lock();
  m_globalHash->release(&impl);
  m_globalHash->unlock();
  return 0;
}

/*****************************************************************
 * Get index info
 */
NdbIndexImpl*
NdbDictionaryImpl::getIndexImpl(const char * externalName,
				const BaseString& internalName)
{
  Ndb_local_table_info * info = get_local_table_info(internalName,
						     false);
  if(info == 0){
    m_error.code = 4243;
    return 0;
  }
  NdbTableImpl * tab = info->m_table_impl;

  if(tab->m_indexType == NdbDictionary::Index::Undefined){
    // Not an index
    m_error.code = 4243;
    return 0;
  }

  NdbTableImpl* prim = getTable(tab->m_primaryTable.c_str());
  if(prim == 0){
    m_error.code = 4243;
    return 0;
  }

  /**
   * Create index impl
   */
  NdbIndexImpl* idx;
  if(NdbDictInterface::create_index_obj_from_table(&idx, tab, prim) == 0){
    idx->m_table = tab;
    idx->m_externalName.assign(externalName);
    idx->m_internalName.assign(internalName);
    // TODO Assign idx to tab->m_index
    // Don't do it right now since assign can't asign a table with index
    // tab->m_index = idx;
    return idx;
  }
  return 0;
}

int
NdbDictInterface::create_index_obj_from_table(NdbIndexImpl** dst,
					      NdbTableImpl* tab,
					      const NdbTableImpl* prim){
  NdbIndexImpl *idx = new NdbIndexImpl();
  idx->m_version = tab->m_version;
  idx->m_status = tab->m_status;
  idx->m_indexId = tab->m_tableId;
  idx->m_externalName.assign(tab->getName());
  idx->m_tableName.assign(prim->m_externalName);
  NdbDictionary::Index::Type type = idx->m_type = tab->m_indexType;
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

    if(type == NdbDictionary::Index::OrderedIndex && 
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

  * dst = idx;

  return 0;
}

/*****************************************************************
 * Create index
 */
int
NdbDictionaryImpl::createIndex(NdbIndexImpl &ix)
{
  NdbTableImpl* tab = getTable(ix.getTable());
  if(tab == 0){
    m_error.code = 4249;
    return -1;
  }
  
  return m_receiver.createIndex(m_ndb, ix, * tab);
}

int 
NdbDictInterface::createIndex(Ndb & ndb,
			      NdbIndexImpl & impl, 
			      const NdbTableImpl & table)
{
  //validate();
  //aggregate();
  unsigned i;
  UtilBufferWriter w(m_buffer);
  const size_t len = strlen(impl.m_externalName.c_str()) + 1;
  if(len > MAX_TAB_NAME_SIZE) {
    m_error.code = 4241;
    return -1;
  }
  const BaseString internalName(
    ndb.internalize_index_name(&table, impl.getName()));
  impl.m_internalName.assign(internalName);

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
  
  req->setTableId(table.m_tableId);
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
    // Copy column definition
    *impl.m_columns[i] = *col;

    // index key type check
    if (it == DictTabInfo::UniqueHashIndex &&
        ! NdbSqlUtil::usable_in_hash_index(col->m_type, col->m_cs) ||
        it == DictTabInfo::OrderedIndex &&
        ! NdbSqlUtil::usable_in_ordered_index(col->m_type, col->m_cs)) {
      m_error.code = 743;
      return -1;
    }
    attributeList.id[i] = col->m_attrId;
  }
  LinearSectionPtr ptr[2];
  ptr[0].p = (Uint32*)&attributeList;
  ptr[0].sz = 1 + attributeList.sz;
  ptr[1].p = (Uint32*)m_buffer.get_data();
  ptr[1].sz = m_buffer.length() >> 2;                //BUG?
  return createIndex(&tSignal, ptr);
}

int
NdbDictInterface::createIndex(NdbApiSignal* signal, 
			      LinearSectionPtr ptr[3])
{
  const int noErrCodes = 2;
  int errCodes[noErrCodes] = {CreateIndxRef::Busy, CreateIndxRef::NotMaster};
  return dictSignal(signal,ptr,2,
		    1 /*use masternode id*/,
		    100,
		    WAIT_CREATE_INDX_REQ,
		    -1,
		    errCodes,noErrCodes);
}

void
NdbDictInterface::execCREATE_INDX_CONF(NdbApiSignal * signal,
				       LinearSectionPtr ptr[3])
{
  //CreateTableConf* const conf = CAST_CONSTPTR(CreateTableConf, signal->getDataPtr());
  
  m_waiter.signal(NO_WAIT);  
}

void
NdbDictInterface::execCREATE_INDX_REF(NdbApiSignal * signal,
				      LinearSectionPtr ptr[3])
{
  const CreateIndxRef* const ref = CAST_CONSTPTR(CreateIndxRef, signal->getDataPtr());
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
    idx->m_table->m_status = NdbDictionary::Object::Invalid;
    m_globalHash->drop(idx->m_table);
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

    int ret = m_receiver.dropIndex(impl, *timpl);
    if(ret == 0){
      m_localHash.drop(internalIndexName.c_str());
      m_globalHash->lock();
      impl.m_table->m_status = NdbDictionary::Object::Invalid;
      m_globalHash->drop(impl.m_table);
      m_globalHash->unlock();
    }
    return ret;
  }

  m_error.code = 4243;
  return -1;
}

int
NdbDictInterface::dropIndex(const NdbIndexImpl & impl, 
			    const NdbTableImpl & timpl)
{
  NdbApiSignal tSignal(m_reference);
  tSignal.theReceiversBlockNumber = DBDICT;
  tSignal.theVerId_signalNumber   = GSN_DROP_INDX_REQ;
  tSignal.theLength = DropIndxReq::SignalLength;

  DropIndxReq * const req = CAST_PTR(DropIndxReq, tSignal.getDataPtrSend());
  req->setUserRef(m_reference);
  req->setConnectionPtr(0);
  req->setRequestType(DropIndxReq::RT_USER);
  req->setTableId(~0);  // DICT overwrites
  req->setIndexId(timpl.m_tableId);
  req->setIndexVersion(timpl.m_version);

  return dropIndex(&tSignal, 0);
}

int
NdbDictInterface::dropIndex(NdbApiSignal* signal, LinearSectionPtr ptr[3])
{
  const int noErrCodes = 2;
  int errCodes[noErrCodes] = {DropIndxRef::Busy, DropIndxRef::NotMaster};
  int r = dictSignal(signal,NULL,0,
		     1/*Use masternode id*/,
		     100,
		     WAIT_DROP_INDX_REQ,
		     WAITFOR_RESPONSE_TIMEOUT,
		     errCodes,noErrCodes);
  if(m_error.code == DropIndxRef::InvalidIndexVersion) {
    // Clear caches and try again
    return INCOMPATIBLE_VERSION;
  }
  return r;
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
  const DropIndxRef* const ref = CAST_CONSTPTR(DropIndxRef, signal->getDataPtr());
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
      DBUG_RETURN(-1);
    }
  }

  DBUG_PRINT("info",("Table: id: %d version: %d", tab->m_tableId, tab->m_version));

  evnt.m_tableId = tab->m_tableId;
  evnt.m_tableVersion = tab->m_version;
  evnt.m_tableImpl = tab;
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
      DBUG_RETURN(-1);
    }
  }

  evnt.m_attrIds.clear();

  attributeList_sz = evnt.m_columns.size();

  DBUG_PRINT("info",("Event on tableId=%d, tableVersion=%d, event name %s, no of columns %d",
		     evnt.m_tableId, evnt.m_tableVersion,
		     evnt.m_name.c_str(),
		     evnt.m_columns.size()));

  int pk_count = 0;
  evnt.m_attrListBitmask.clear();

  for(i = 0; i<attributeList_sz; i++){
    const NdbColumnImpl* col = 
      table.getColumn(evnt.m_columns[i]->m_name.c_str());
    if(col == 0){
      m_error.code= 4247;
      DBUG_RETURN(-1);
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
      DBUG_RETURN(-1);
    }
  }
  
#ifdef EVENT_DEBUG
  char buf[128] = {0};
  evnt.m_attrListBitmask.getText(buf);
  ndbout_c("createEvent: mask = %s", buf);
#endif

  // NdbDictInterface m_receiver;
  DBUG_RETURN(m_receiver.createEvent(m_ndb, evnt, 0 /* getFlag unset */));
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
		       evnt.m_tableId, evnt.m_tableVersion));
    // creating event in Dictionary
    req->setRequestType(CreateEvntReq::RT_USER_CREATE);
    req->setTableId(evnt.m_tableId);
    req->setTableVersion(evnt.m_tableVersion);
    req->setAttrListBitmask(evnt.m_attrListBitmask);
    req->setEventType(evnt.mi_type);
  }

  UtilBufferWriter w(m_buffer);

  const size_t len = strlen(evnt.m_name.c_str()) + 1;
  if(len > MAX_TAB_NAME_SIZE) {
    m_error.code= 4241;
    DBUG_RETURN(-1);
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

  int ret = createEvent(&tSignal, ptr, 1);

  if (ret) {
    DBUG_RETURN(ret);
  }

  char *dataPtr = (char *)m_buffer.get_data();
  unsigned int lenCreateEvntConf = *((unsigned int *)dataPtr);
  dataPtr += sizeof(lenCreateEvntConf);
  CreateEvntConf const * evntConf = (CreateEvntConf *)dataPtr;
  dataPtr += lenCreateEvntConf;
  
  //  NdbEventImpl *evntImpl = (NdbEventImpl *)evntConf->getUserData();

  if (getFlag) {
    evnt.m_tableId         = evntConf->getTableId();
    evnt.m_tableVersion    = evntConf->getTableVersion();
    evnt.m_attrListBitmask = evntConf->getAttrListBitmask();
    evnt.mi_type           = evntConf->getEventType();
    evnt.setTable(dataPtr);
  } else {
    if (evnt.m_tableId         != evntConf->getTableId() ||
	evnt.m_tableVersion    != evntConf->getTableVersion() ||
	//evnt.m_attrListBitmask != evntConf->getAttrListBitmask() ||
	evnt.mi_type           != evntConf->getEventType()) {
      ndbout_c("ERROR*************");
      DBUG_RETURN(1);
    }
  }

  evnt.m_eventId         = evntConf->getEventId();
  evnt.m_eventKey        = evntConf->getEventKey();

  DBUG_RETURN(0);
}

int
NdbDictInterface::createEvent(NdbApiSignal* signal,
			      LinearSectionPtr ptr[3], int noLSP)
{
  return dictSignal(signal,ptr,noLSP,
		    1 /*use masternode id*/,
		    100,
		    WAIT_CREATE_INDX_REQ /*WAIT_CREATE_EVNT_REQ*/,
		    -1,
		    NULL,0, -1);
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

  int errCodes[] = { SubStartRef::Busy };
  DBUG_RETURN(dictSignal(&tSignal,NULL,0,
			 1 /*use masternode id*/,
			 100,
			 WAIT_CREATE_INDX_REQ /*WAIT_CREATE_EVNT_REQ*/,
			 -1,
			 errCodes, sizeof(errCodes)/sizeof(errCodes[0])));
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

  int errCodes[] = { SubStopRef::Busy };
  DBUG_RETURN(dictSignal(&tSignal,NULL,0,
			 1 /*use masternode id*/,
			 100,
			 WAIT_CREATE_INDX_REQ /*WAIT_SUB_STOP__REQ*/,
			 -1,
			 errCodes, sizeof(errCodes)/sizeof(errCodes[0])));
}

NdbEventImpl * 
NdbDictionaryImpl::getEvent(const char * eventName)
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
  Ndb_local_table_info *info;
  int retry= 0;
  while (1)
  {
    info= get_local_table_info(ev->getTableName(), true);
    if (info == 0)
    {
      DBUG_PRINT("error",("unable to find table %s", ev->getTableName()));
      delete ev;
      DBUG_RETURN(NULL);
    }

    if (ev->m_tableId      == info->m_table_impl->m_tableId &&
	ev->m_tableVersion == info->m_table_impl->m_version)
      break;
    if (retry)
    {
      m_error.code= 241;
      DBUG_PRINT("error",("%s: table version mismatch, event: [%u,%u] table: [%u,%u]",
			  ev->getTableName(), ev->m_tableId, ev->m_tableVersion,
			  info->m_table_impl->m_tableId, info->m_table_impl->m_version));
      delete ev;
      DBUG_RETURN(NULL);
    }
    invalidateObject(*info->m_table_impl);
    retry++;
  }

  ev->m_tableImpl= info->m_table_impl;
  ev->setTable(m_ndb.externalizeTableName(ev->getTableName()));

  // get the columns from the attrListBitmask

  NdbTableImpl &table = *ev->m_tableImpl;
  AttributeMask & mask = ev->m_attrListBitmask;
  unsigned attributeList_sz = mask.count();

  DBUG_PRINT("info",("Table: id: %d version: %d", table.m_tableId, table.m_version));

#ifndef DBUG_OFF
  char buf[128] = {0};
  mask.getText(buf);
  DBUG_PRINT("info",("attributeList_sz= %d, mask= %s", attributeList_sz, buf));
#endif

  
  if ( attributeList_sz > table.getNoOfColumns() )
  {
    DBUG_PRINT("error",("Invalid version, too many columns"));
    delete ev;
    DBUG_RETURN(NULL);
  }

  assert( (int)attributeList_sz <= table.getNoOfColumns() );
  for(unsigned id= 0; ev->m_columns.size() < attributeList_sz; id++) {
    if ( id >= table.getNoOfColumns())
    {
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
  m_waiter.signal(NO_WAIT);
  DBUG_VOID_RETURN;
}

/*****************************************************************
 * Drop event
 */
int 
NdbDictionaryImpl::dropEvent(const char * eventName)
{
  NdbEventImpl *ev= new NdbEventImpl();
  ev->setName(eventName);
  int ret= m_receiver.dropEvent(*ev);
  delete ev;  

  //  printf("__________________RET %u\n", ret);
  return ret;
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

  return dropEvent(&tSignal, ptr, 1);
}

int
NdbDictInterface::dropEvent(NdbApiSignal* signal,
			    LinearSectionPtr ptr[3], int noLSP)
{
  //TODO
  return dictSignal(signal,ptr,noLSP,
		    1 /*use masternode id*/,
		    100,
		    WAIT_CREATE_INDX_REQ /*WAIT_CREATE_EVNT_REQ*/,
		    -1,
		    NULL,0, -1);
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
    int ret_val= poll_guard.wait_n_unlock(WAITFOR_RESPONSE_TIMEOUT,
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
    m_waiter.wait(WAITFOR_RESPONSE_TIMEOUT);
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
  g_latest_trans_gci= conf->gcp;
  m_waiter.signal(NO_WAIT);
}

void
NdbDictInterface::execWAIT_GCP_REF(NdbApiSignal* signal,
				    LinearSectionPtr ptr[3])
{
  m_waiter.signal(NO_WAIT);
}

template class Vector<int>;
template class Vector<Uint16>;
template class Vector<Uint32>;
template class Vector<Vector<Uint32> >;
template class Vector<NdbTableImpl*>;
template class Vector<NdbColumnImpl*>;
