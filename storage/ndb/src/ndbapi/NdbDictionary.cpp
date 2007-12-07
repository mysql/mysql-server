/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NdbDictionary.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbOut.hpp>

NdbDictionary::ObjectId::ObjectId()
  : m_impl(* new NdbDictObjectImpl(NdbDictionary::Object::TypeUndefined))
{
}

NdbDictionary::ObjectId::~ObjectId()
{
  NdbDictObjectImpl * tmp = &m_impl;  
  delete tmp;
}

NdbDictionary::Object::Status
NdbDictionary::ObjectId::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::ObjectId::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::ObjectId::getObjectId() const {
  return m_impl.m_id;
}

/*****************************************************************
 * Column facade
 */
NdbDictionary::Column::Column(const char * name) 
  : m_impl(* new NdbColumnImpl(* this))
{
  setName(name);
}

NdbDictionary::Column::Column(const NdbDictionary::Column & org)
  : m_impl(* new NdbColumnImpl(* this))
{
  m_impl = org.m_impl;
}

NdbDictionary::Column::Column(NdbColumnImpl& impl)
  : m_impl(impl)
{
}

NdbDictionary::Column::~Column(){
  NdbColumnImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

NdbDictionary::Column&
NdbDictionary::Column::operator=(const NdbDictionary::Column& column)
{
  m_impl = column.m_impl;
  
  return *this;
}

int
NdbDictionary::Column::setName(const char * name){
  return !m_impl.m_name.assign(name);
}

const char* 
NdbDictionary::Column::getName() const {
  return m_impl.m_name.c_str();
}

void
NdbDictionary::Column::setType(Type t){
  m_impl.init(t);
}

NdbDictionary::Column::Type 
NdbDictionary::Column::getType() const {
  return m_impl.m_type;
}

void 
NdbDictionary::Column::setPrecision(int val){
  m_impl.m_precision = val;
}

int 
NdbDictionary::Column::getPrecision() const {
  return m_impl.m_precision;
}

void 
NdbDictionary::Column::setScale(int val){
  m_impl.m_scale = val;
}

int 
NdbDictionary::Column::getScale() const{
  return m_impl.m_scale;
}

void
NdbDictionary::Column::setLength(int length){
  m_impl.m_length = length;
}

int 
NdbDictionary::Column::getLength() const{
  return m_impl.m_length;
}

void
NdbDictionary::Column::setInlineSize(int size)
{
  m_impl.m_precision = size;
}

void
NdbDictionary::Column::setCharset(CHARSET_INFO* cs)
{
  m_impl.m_cs = cs;
}

CHARSET_INFO*
NdbDictionary::Column::getCharset() const
{
  return m_impl.m_cs;
}

int
NdbDictionary::Column::getInlineSize() const
{
  return m_impl.m_precision;
}

void
NdbDictionary::Column::setPartSize(int size)
{
  m_impl.m_scale = size;
}

int
NdbDictionary::Column::getPartSize() const
{
  return m_impl.m_scale;
}

void
NdbDictionary::Column::setStripeSize(int size)
{
  m_impl.m_length = size;
}

int
NdbDictionary::Column::getStripeSize() const
{
  return m_impl.m_length;
}

int 
NdbDictionary::Column::getSize() const{
  return m_impl.m_attrSize;
}

void 
NdbDictionary::Column::setNullable(bool val){
  m_impl.m_nullable = val;
}

bool 
NdbDictionary::Column::getNullable() const {
  return m_impl.m_nullable;
}

void 
NdbDictionary::Column::setPrimaryKey(bool val){
  m_impl.m_pk = val;
}

bool 
NdbDictionary::Column::getPrimaryKey() const {
  return m_impl.m_pk;
}

void 
NdbDictionary::Column::setPartitionKey(bool val){
  m_impl.m_distributionKey = val;
}

bool 
NdbDictionary::Column::getPartitionKey() const{
  return m_impl.m_distributionKey;
}

const NdbDictionary::Table * 
NdbDictionary::Column::getBlobTable() const {
  NdbTableImpl * t = m_impl.m_blobTable;
  if (t)
    return t->m_facade;
  return 0;
}

void 
NdbDictionary::Column::setAutoIncrement(bool val){
  m_impl.m_autoIncrement = val;
}

bool 
NdbDictionary::Column::getAutoIncrement() const {
  return m_impl.m_autoIncrement;
}

void
NdbDictionary::Column::setAutoIncrementInitialValue(Uint64 val){
  m_impl.m_autoIncrementInitialValue = val;
}

int
NdbDictionary::Column::setDefaultValue(const char* defaultValue)
{
  return !m_impl.m_defaultValue.assign(defaultValue);
}

const char*
NdbDictionary::Column::getDefaultValue() const
{
  return m_impl.m_defaultValue.c_str();
}

int
NdbDictionary::Column::getColumnNo() const {
  return m_impl.m_column_no;
}

int
NdbDictionary::Column::getAttrId() const {
  return m_impl.m_attrId;;
}

bool
NdbDictionary::Column::equal(const NdbDictionary::Column & col) const {
  return m_impl.equal(col.m_impl);
}

int
NdbDictionary::Column::getSizeInBytes() const 
{
  return m_impl.m_attrSize * m_impl.m_arraySize;
}

void
NdbDictionary::Column::setArrayType(ArrayType type)
{
  m_impl.m_arrayType = type;
}

NdbDictionary::Column::ArrayType
NdbDictionary::Column::getArrayType() const
{
  return (ArrayType)m_impl.m_arrayType;
}

void
NdbDictionary::Column::setStorageType(StorageType type)
{
  m_impl.m_storageType = type;
}

NdbDictionary::Column::StorageType
NdbDictionary::Column::getStorageType() const
{
  return (StorageType)m_impl.m_storageType;
}

/*****************************************************************
 * Table facade
 */
NdbDictionary::Table::Table(const char * name)
  : m_impl(* new NdbTableImpl(* this)) 
{
  setName(name);
}

NdbDictionary::Table::Table(const NdbDictionary::Table & org)
  : Object(org), m_impl(* new NdbTableImpl(* this))
{
  m_impl.assign(org.m_impl);
}

NdbDictionary::Table::Table(NdbTableImpl & impl)
  : m_impl(impl)
{
}

NdbDictionary::Table::~Table(){
  NdbTableImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

NdbDictionary::Table&
NdbDictionary::Table::operator=(const NdbDictionary::Table& table)
{
  m_impl.assign(table.m_impl);
  
  m_impl.m_facade = this;
  return *this;
}

int
NdbDictionary::Table::setName(const char * name){
  return m_impl.setName(name);
}

const char * 
NdbDictionary::Table::getName() const {
  return m_impl.getName();
}

const char *
NdbDictionary::Table::getMysqlName() const {
  return m_impl.getMysqlName();
}

int
NdbDictionary::Table::getTableId() const {
  return m_impl.m_id;
}

int
NdbDictionary::Table::addColumn(const Column & c){
  NdbColumnImpl* col = new NdbColumnImpl;
  if (col ==  NULL)
  {
    errno = ENOMEM;
    return -1;
  }
  (* col) = NdbColumnImpl::getImpl(c);
  if (m_impl.m_columns.push_back(col))
  {
    return -1;
  }
  if (m_impl.buildColumnHash())
  {
    return -1;
  }
  return 0;
}

const NdbDictionary::Column*
NdbDictionary::Table::getColumn(const char * name) const {
  return m_impl.getColumn(name);
}

const NdbDictionary::Column* 
NdbDictionary::Table::getColumn(const int attrId) const {
  return m_impl.getColumn(attrId);
}

NdbDictionary::Column*
NdbDictionary::Table::getColumn(const char * name) 
{
  return m_impl.getColumn(name);
}

NdbDictionary::Column* 
NdbDictionary::Table::getColumn(const int attrId)
{
  return m_impl.getColumn(attrId);
}

void
NdbDictionary::Table::setLogging(bool val){
  m_impl.m_logging = val;
}

bool 
NdbDictionary::Table::getLogging() const {
  return m_impl.m_logging;
}

void
NdbDictionary::Table::setFragmentType(FragmentType ft){
  m_impl.m_fragmentType = ft;
}

NdbDictionary::Object::FragmentType 
NdbDictionary::Table::getFragmentType() const {
  return m_impl.m_fragmentType;
}

void 
NdbDictionary::Table::setKValue(int kValue){
  m_impl.m_kvalue = kValue;
}

int
NdbDictionary::Table::getKValue() const {
  return m_impl.m_kvalue;
}

void 
NdbDictionary::Table::setMinLoadFactor(int lf){
  m_impl.m_minLoadFactor = lf;
}

int 
NdbDictionary::Table::getMinLoadFactor() const {
  return m_impl.m_minLoadFactor;
}

void 
NdbDictionary::Table::setMaxLoadFactor(int lf){
  m_impl.m_maxLoadFactor = lf;  
}

int 
NdbDictionary::Table::getMaxLoadFactor() const {
  return m_impl.m_maxLoadFactor;
}

int
NdbDictionary::Table::getNoOfColumns() const {
  return m_impl.m_columns.size();
}

int
NdbDictionary::Table::getNoOfPrimaryKeys() const {
  return m_impl.m_noOfKeys;
}

void
NdbDictionary::Table::setMaxRows(Uint64 maxRows)
{
  m_impl.m_max_rows = maxRows;
}

Uint64
NdbDictionary::Table::getMaxRows() const
{
  return m_impl.m_max_rows;
}

void
NdbDictionary::Table::setMinRows(Uint64 minRows)
{
  m_impl.m_min_rows = minRows;
}

Uint64
NdbDictionary::Table::getMinRows() const
{
  return m_impl.m_min_rows;
}

void
NdbDictionary::Table::setDefaultNoPartitionsFlag(Uint32 flag)
{
  m_impl.m_default_no_part_flag = flag;;
}

Uint32
NdbDictionary::Table::getDefaultNoPartitionsFlag() const
{
  return m_impl.m_default_no_part_flag;
}

const char*
NdbDictionary::Table::getPrimaryKey(int no) const {
  int count = 0;
  for (unsigned i = 0; i < m_impl.m_columns.size(); i++) {
    if (m_impl.m_columns[i]->m_pk) {
      if (count++ == no)
        return m_impl.m_columns[i]->m_name.c_str();
    }
  }
  return 0;
}

const void* 
NdbDictionary::Table::getFrmData() const {
  return m_impl.getFrmData();
}

Uint32
NdbDictionary::Table::getFrmLength() const {
  return m_impl.getFrmLength();
}

enum NdbDictionary::Table::SingleUserMode
NdbDictionary::Table::getSingleUserMode() const
{
  return (enum SingleUserMode)m_impl.m_single_user_mode;
}

void
NdbDictionary::Table::setSingleUserMode(enum NdbDictionary::Table::SingleUserMode mode)
{
  m_impl.m_single_user_mode = (Uint8)mode;
}

int
NdbDictionary::Table::setTablespaceNames(const void *data, Uint32 len)
{
  return m_impl.setTablespaceNames(data, len);
}

const void*
NdbDictionary::Table::getTablespaceNames()
{
  return m_impl.getTablespaceNames();
}

Uint32
NdbDictionary::Table::getTablespaceNamesLen() const
{
  return m_impl.getTablespaceNamesLen();
}

void
NdbDictionary::Table::setLinearFlag(Uint32 flag)
{
  m_impl.m_linear_flag = flag;
}

bool
NdbDictionary::Table::getLinearFlag() const
{
  return m_impl.m_linear_flag;
}

void
NdbDictionary::Table::setFragmentCount(Uint32 count)
{
  m_impl.setFragmentCount(count);
}

Uint32
NdbDictionary::Table::getFragmentCount() const
{
  return m_impl.getFragmentCount();
}

int
NdbDictionary::Table::setFrm(const void* data, Uint32 len){
  return m_impl.setFrm(data, len);
}

const void* 
NdbDictionary::Table::getFragmentData() const {
  return m_impl.getFragmentData();
}

Uint32
NdbDictionary::Table::getFragmentDataLen() const {
  return m_impl.getFragmentDataLen();
}

int
NdbDictionary::Table::setFragmentData(const void* data, Uint32 len)
{
  return m_impl.setFragmentData(data, len);
}

const void* 
NdbDictionary::Table::getTablespaceData() const {
  return m_impl.getTablespaceData();
}

Uint32
NdbDictionary::Table::getTablespaceDataLen() const {
  return m_impl.getTablespaceDataLen();
}

int
NdbDictionary::Table::setTablespaceData(const void* data, Uint32 len)
{
  return m_impl.setTablespaceData(data, len);
}

const void* 
NdbDictionary::Table::getRangeListData() const {
  return m_impl.getRangeListData();
}

Uint32
NdbDictionary::Table::getRangeListDataLen() const {
  return m_impl.getRangeListDataLen();
}

int
NdbDictionary::Table::setRangeListData(const void* data, Uint32 len)
{
  return m_impl.setRangeListData(data, len);
}

NdbDictionary::Object::Status
NdbDictionary::Table::getObjectStatus() const {
  return m_impl.m_status;
}

void
NdbDictionary::Table::setStatusInvalid() const {
  m_impl.m_status = NdbDictionary::Object::Invalid;
}

int 
NdbDictionary::Table::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::Table::getObjectId() const {
  return m_impl.m_id;
}

bool
NdbDictionary::Table::equal(const NdbDictionary::Table & col) const {
  return m_impl.equal(col.m_impl);
}

int
NdbDictionary::Table::getRowSizeInBytes() const {  
  int sz = 0;
  for(int i = 0; i<getNoOfColumns(); i++){
    const NdbDictionary::Column * c = getColumn(i);
    sz += (c->getSizeInBytes()+ 3) / 4;
  }
  return sz * 4;
}

int
NdbDictionary::Table::getReplicaCount() const {  
  return m_impl.m_replicaCount;
}

bool
NdbDictionary::Table::getTemporary() {
  return m_impl.m_temporary;
}

void
NdbDictionary::Table::setTemporary(bool val) {
  m_impl.m_temporary = val;
}

int
NdbDictionary::Table::createTableInDb(Ndb* pNdb, bool equalOk) const {  
  const NdbDictionary::Table * pTab = 
    pNdb->getDictionary()->getTable(getName());
  if(pTab != 0 && equal(* pTab))
    return 0;
  if(pTab != 0 && !equal(* pTab))
    return -1;
  return pNdb->getDictionary()->createTable(* this);
}

bool
NdbDictionary::Table::getTablespace(Uint32 *id, Uint32 *version) const 
{
  if (m_impl.m_tablespace_id == RNIL)
    return false;
  if (id)
    *id= m_impl.m_tablespace_id;
  if (version)
    *version= m_impl.m_version;
  return true;
}

const char *
NdbDictionary::Table::getTablespaceName() const 
{
  return m_impl.m_tablespace_name.c_str();
}

int
NdbDictionary::Table::setTablespaceName(const char * name){
  m_impl.m_tablespace_id = ~0;
  m_impl.m_tablespace_version = ~0;
  return !m_impl.m_tablespace_name.assign(name);
}

int
NdbDictionary::Table::setTablespace(const NdbDictionary::Tablespace & ts){
  m_impl.m_tablespace_id = NdbTablespaceImpl::getImpl(ts).m_id;
  m_impl.m_tablespace_version = ts.getObjectVersion();
  return !m_impl.m_tablespace_name.assign(ts.getName());
}

void
NdbDictionary::Table::setRowChecksumIndicator(bool val){
  m_impl.m_row_checksum = val;
}

bool 
NdbDictionary::Table::getRowChecksumIndicator() const {
  return m_impl.m_row_checksum;
}

void
NdbDictionary::Table::setRowGCIIndicator(bool val){
  m_impl.m_row_gci = val;
}

bool 
NdbDictionary::Table::getRowGCIIndicator() const {
  return m_impl.m_row_gci;
}

void
NdbDictionary::Table::setForceVarPart(bool val){
  m_impl.m_force_var_part = val;
}

bool 
NdbDictionary::Table::getForceVarPart() const {
  return m_impl.m_force_var_part;
}

int
NdbDictionary::Table::aggregate(NdbError& error)
{
  return m_impl.aggregate(error);
}

int
NdbDictionary::Table::validate(NdbError& error)
{
  return m_impl.validate(error);
}


/*****************************************************************
 * Index facade
 */

NdbDictionary::Index::Index(const char * name)
  : m_impl(* new NdbIndexImpl(* this))
{
  setName(name);
}

NdbDictionary::Index::Index(NdbIndexImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::Index::~Index(){
  NdbIndexImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

int
NdbDictionary::Index::setName(const char * name){
  return m_impl.setName(name);
}

const char * 
NdbDictionary::Index::getName() const {
  return m_impl.getName();
}

int
NdbDictionary::Index::setTable(const char * table){
  return m_impl.setTable(table);
}

const char * 
NdbDictionary::Index::getTable() const {
  return m_impl.getTable();
}

unsigned
NdbDictionary::Index::getNoOfColumns() const {
  return m_impl.m_columns.size();
}

int
NdbDictionary::Index::getNoOfIndexColumns() const {
  return m_impl.m_columns.size();
}

const NdbDictionary::Column *
NdbDictionary::Index::getColumn(unsigned no) const {
  if(no < m_impl.m_columns.size())
    return m_impl.m_columns[no];
  return NULL;
}

const char *
NdbDictionary::Index::getIndexColumn(int no) const {
  const NdbDictionary::Column* col = getColumn(no);

  if (col)
    return col->getName();
  else
    return NULL;
}

int
NdbDictionary::Index::addColumn(const Column & c){
  NdbColumnImpl* col = new NdbColumnImpl;
  if (col == NULL)
  {
    errno = ENOMEM;
    return -1;
  }
  (* col) = NdbColumnImpl::getImpl(c);
  if (m_impl.m_columns.push_back(col))
  {
    return -1;
  }
  return 0;
}

int
NdbDictionary::Index::addColumnName(const char * name){
  const Column c(name);
  return addColumn(c);
}

int
NdbDictionary::Index::addIndexColumn(const char * name){
  const Column c(name);
  return addColumn(c);
}

int
NdbDictionary::Index::addColumnNames(unsigned noOfNames, const char ** names){
  for(unsigned i = 0; i < noOfNames; i++) {
    const Column c(names[i]);
    if (addColumn(c))
    {
      return -1;
    }
  }
  return 0;
}

int
NdbDictionary::Index::addIndexColumns(int noOfNames, const char ** names){
  for(int i = 0; i < noOfNames; i++) {
    const Column c(names[i]);
    if (addColumn(c))
    {
      return -1;
    }
  }
  return 0;
}

void
NdbDictionary::Index::setType(NdbDictionary::Index::Type t){
  m_impl.m_type = (NdbDictionary::Object::Type)t;
}

NdbDictionary::Index::Type
NdbDictionary::Index::getType() const {
  return (NdbDictionary::Index::Type)m_impl.m_type;
}

void
NdbDictionary::Index::setLogging(bool val){
  m_impl.m_logging = val;
}

bool
NdbDictionary::Index::getTemporary(){
  return m_impl.m_temporary;
}

void
NdbDictionary::Index::setTemporary(bool val){
  m_impl.m_temporary = val;
}

bool 
NdbDictionary::Index::getLogging() const {
  return m_impl.m_logging;
}

NdbDictionary::Object::Status
NdbDictionary::Index::getObjectStatus() const {
  return m_impl.m_table->m_status;
}

int 
NdbDictionary::Index::getObjectVersion() const {
  return m_impl.m_table->m_version;
}

int 
NdbDictionary::Index::getObjectId() const {
  return m_impl.m_table->m_id;
}


/*****************************************************************
 * Event facade
 */
NdbDictionary::Event::Event(const char * name)
  : m_impl(* new NdbEventImpl(* this))
{
  setName(name);
}

NdbDictionary::Event::Event(const char * name, const Table& table)
  : m_impl(* new NdbEventImpl(* this))
{
  setName(name);
  setTable(table);
}

NdbDictionary::Event::Event(NdbEventImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::Event::~Event()
{
  NdbEventImpl * tmp = &m_impl;
  if(this != tmp){
    delete tmp;
  }
}

int
NdbDictionary::Event::setName(const char * name)
{
  return m_impl.setName(name);
}

const char *
NdbDictionary::Event::getName() const
{
  return m_impl.getName();
}

void 
NdbDictionary::Event::setTable(const Table& table)
{
  m_impl.setTable(table);
}

const NdbDictionary::Table *
NdbDictionary::Event::getTable() const
{
  return m_impl.getTable();
}

int
NdbDictionary::Event::setTable(const char * table)
{
  return m_impl.setTable(table);
}

const char*
NdbDictionary::Event::getTableName() const
{
  return m_impl.getTableName();
}

void
NdbDictionary::Event::addTableEvent(const TableEvent t)
{
  m_impl.addTableEvent(t);
}

bool
NdbDictionary::Event::getTableEvent(const TableEvent t) const
{
  return m_impl.getTableEvent(t);
}

void
NdbDictionary::Event::setDurability(EventDurability d)
{
  m_impl.setDurability(d);
}

NdbDictionary::Event::EventDurability
NdbDictionary::Event::getDurability() const
{
  return m_impl.getDurability();
}

void
NdbDictionary::Event::setReport(EventReport r)
{
  m_impl.setReport(r);
}

NdbDictionary::Event::EventReport
NdbDictionary::Event::getReport() const
{
  return m_impl.getReport();
}

void
NdbDictionary::Event::addColumn(const Column & c){
  NdbColumnImpl* col = new NdbColumnImpl;
  (* col) = NdbColumnImpl::getImpl(c);
  m_impl.m_columns.push_back(col);
}

void
NdbDictionary::Event::addEventColumn(unsigned attrId)
{
  m_impl.m_attrIds.push_back(attrId);
}

void
NdbDictionary::Event::addEventColumn(const char * name)
{
  const Column c(name);
  addColumn(c);
}

void
NdbDictionary::Event::addEventColumns(int n, const char ** names)
{
  for (int i = 0; i < n; i++)
    addEventColumn(names[i]);
}

int NdbDictionary::Event::getNoOfEventColumns() const
{
  return m_impl.getNoOfEventColumns();
}

const NdbDictionary::Column *
NdbDictionary::Event::getEventColumn(unsigned no) const
{
  return m_impl.getEventColumn(no);
}

void NdbDictionary::Event::mergeEvents(bool flag)
{
  m_impl.m_mergeEvents = flag;
}

NdbDictionary::Object::Status
NdbDictionary::Event::getObjectStatus() const
{
  return m_impl.m_status;
}

int 
NdbDictionary::Event::getObjectVersion() const
{
  return m_impl.m_version;
}

int 
NdbDictionary::Event::getObjectId() const {
  return m_impl.m_id;
}

void NdbDictionary::Event::print()
{
  m_impl.print();
}

/*****************************************************************
 * Tablespace facade
 */
NdbDictionary::Tablespace::Tablespace()
  : m_impl(* new NdbTablespaceImpl(* this))
{
}

NdbDictionary::Tablespace::Tablespace(NdbTablespaceImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::Tablespace::Tablespace(const NdbDictionary::Tablespace & org)
  : Object(org), m_impl(* new NdbTablespaceImpl(* this))
{
  m_impl.assign(org.m_impl);
}

NdbDictionary::Tablespace::~Tablespace(){
  NdbTablespaceImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

void 
NdbDictionary::Tablespace::setName(const char * name){
  m_impl.m_name.assign(name);
}

const char * 
NdbDictionary::Tablespace::getName() const {
  return m_impl.m_name.c_str();
}

void
NdbDictionary::Tablespace::setAutoGrowSpecification
(const NdbDictionary::AutoGrowSpecification& spec){
  m_impl.m_grow_spec = spec;
}
const NdbDictionary::AutoGrowSpecification& 
NdbDictionary::Tablespace::getAutoGrowSpecification() const {
  return m_impl.m_grow_spec;
}

void
NdbDictionary::Tablespace::setExtentSize(Uint32 sz){
  m_impl.m_extent_size = sz;
}

Uint32
NdbDictionary::Tablespace::getExtentSize() const {
  return m_impl.m_extent_size;
}

void
NdbDictionary::Tablespace::setDefaultLogfileGroup(const char * name){
  m_impl.m_logfile_group_id = ~0;
  m_impl.m_logfile_group_version = ~0;
  m_impl.m_logfile_group_name.assign(name);
}

void
NdbDictionary::Tablespace::setDefaultLogfileGroup
(const NdbDictionary::LogfileGroup & lg){
  m_impl.m_logfile_group_id = NdbLogfileGroupImpl::getImpl(lg).m_id;
  m_impl.m_logfile_group_version = lg.getObjectVersion();
  m_impl.m_logfile_group_name.assign(lg.getName());
}

const char * 
NdbDictionary::Tablespace::getDefaultLogfileGroup() const {
  return m_impl.m_logfile_group_name.c_str();
}

Uint32
NdbDictionary::Tablespace::getDefaultLogfileGroupId() const {
  return m_impl.m_logfile_group_id;
}

NdbDictionary::Object::Status
NdbDictionary::Tablespace::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::Tablespace::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::Tablespace::getObjectId() const {
  return m_impl.m_id;
}

/*****************************************************************
 * LogfileGroup facade
 */
NdbDictionary::LogfileGroup::LogfileGroup()
  : m_impl(* new NdbLogfileGroupImpl(* this))
{
}

NdbDictionary::LogfileGroup::LogfileGroup(NdbLogfileGroupImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::LogfileGroup::LogfileGroup(const NdbDictionary::LogfileGroup & org)
  : Object(org), m_impl(* new NdbLogfileGroupImpl(* this)) 
{
  m_impl.assign(org.m_impl);
}

NdbDictionary::LogfileGroup::~LogfileGroup(){
  NdbLogfileGroupImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

void 
NdbDictionary::LogfileGroup::setName(const char * name){
  m_impl.m_name.assign(name);
}

const char * 
NdbDictionary::LogfileGroup::getName() const {
  return m_impl.m_name.c_str();
}

void
NdbDictionary::LogfileGroup::setUndoBufferSize(Uint32 sz){
  m_impl.m_undo_buffer_size = sz;
}

Uint32
NdbDictionary::LogfileGroup::getUndoBufferSize() const {
  return m_impl.m_undo_buffer_size;
}

void
NdbDictionary::LogfileGroup::setAutoGrowSpecification
(const NdbDictionary::AutoGrowSpecification& spec){
  m_impl.m_grow_spec = spec;
}
const NdbDictionary::AutoGrowSpecification& 
NdbDictionary::LogfileGroup::getAutoGrowSpecification() const {
  return m_impl.m_grow_spec;
}

Uint64 NdbDictionary::LogfileGroup::getUndoFreeWords() const {
  return m_impl.m_undo_free_words;
}

NdbDictionary::Object::Status
NdbDictionary::LogfileGroup::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::LogfileGroup::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::LogfileGroup::getObjectId() const {
  return m_impl.m_id;
}

/*****************************************************************
 * Datafile facade
 */
NdbDictionary::Datafile::Datafile()
  : m_impl(* new NdbDatafileImpl(* this))
{
}

NdbDictionary::Datafile::Datafile(NdbDatafileImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::Datafile::Datafile(const NdbDictionary::Datafile & org)
  : Object(org), m_impl(* new NdbDatafileImpl(* this)) 
{
  m_impl.assign(org.m_impl);
}

NdbDictionary::Datafile::~Datafile(){
  NdbDatafileImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

void 
NdbDictionary::Datafile::setPath(const char * path){
  m_impl.m_path.assign(path);
}

const char * 
NdbDictionary::Datafile::getPath() const {
  return m_impl.m_path.c_str();
}

void 
NdbDictionary::Datafile::setSize(Uint64 sz){
  m_impl.m_size = sz;
}

Uint64
NdbDictionary::Datafile::getSize() const {
  return m_impl.m_size;
}

Uint64
NdbDictionary::Datafile::getFree() const {
  return m_impl.m_free;
}

int
NdbDictionary::Datafile::setTablespace(const char * tablespace){
  m_impl.m_filegroup_id = ~0;
  m_impl.m_filegroup_version = ~0;
  return !m_impl.m_filegroup_name.assign(tablespace);
}

int
NdbDictionary::Datafile::setTablespace(const NdbDictionary::Tablespace & ts){
  m_impl.m_filegroup_id = NdbTablespaceImpl::getImpl(ts).m_id;
  m_impl.m_filegroup_version = ts.getObjectVersion();
  return !m_impl.m_filegroup_name.assign(ts.getName());
}

const char *
NdbDictionary::Datafile::getTablespace() const {
  return m_impl.m_filegroup_name.c_str();
}

void
NdbDictionary::Datafile::getTablespaceId(NdbDictionary::ObjectId* dst) const 
{
  if (dst)
  {
    NdbDictObjectImpl::getImpl(* dst).m_id = m_impl.m_filegroup_id;
    NdbDictObjectImpl::getImpl(* dst).m_version = m_impl.m_filegroup_version;
  }
}

NdbDictionary::Object::Status
NdbDictionary::Datafile::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::Datafile::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::Datafile::getObjectId() const {
  return m_impl.m_id;
}

/*****************************************************************
 * Undofile facade
 */
NdbDictionary::Undofile::Undofile()
  : m_impl(* new NdbUndofileImpl(* this))
{
}

NdbDictionary::Undofile::Undofile(NdbUndofileImpl & impl)
  : m_impl(impl) 
{
}

NdbDictionary::Undofile::Undofile(const NdbDictionary::Undofile & org)
  : Object(org), m_impl(* new NdbUndofileImpl(* this))
{
  m_impl.assign(org.m_impl);
}

NdbDictionary::Undofile::~Undofile(){
  NdbUndofileImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

void 
NdbDictionary::Undofile::setPath(const char * path){
  m_impl.m_path.assign(path);
}

const char * 
NdbDictionary::Undofile::getPath() const {
  return m_impl.m_path.c_str();
}

void 
NdbDictionary::Undofile::setSize(Uint64 sz){
  m_impl.m_size = sz;
}

Uint64
NdbDictionary::Undofile::getSize() const {
  return m_impl.m_size;
}

void 
NdbDictionary::Undofile::setLogfileGroup(const char * logfileGroup){
  m_impl.m_filegroup_id = ~0;
  m_impl.m_filegroup_version = ~0;
  m_impl.m_filegroup_name.assign(logfileGroup);
}

void 
NdbDictionary::Undofile::setLogfileGroup
(const NdbDictionary::LogfileGroup & ts){
  m_impl.m_filegroup_id = NdbLogfileGroupImpl::getImpl(ts).m_id;
  m_impl.m_filegroup_version = ts.getObjectVersion();
  m_impl.m_filegroup_name.assign(ts.getName());
}

const char *
NdbDictionary::Undofile::getLogfileGroup() const {
  return m_impl.m_filegroup_name.c_str();
}

void
NdbDictionary::Undofile::getLogfileGroupId(NdbDictionary::ObjectId * dst)const 
{
  if (dst)
  {
    NdbDictObjectImpl::getImpl(* dst).m_id = m_impl.m_filegroup_id;
    NdbDictObjectImpl::getImpl(* dst).m_version = m_impl.m_filegroup_version;
  }
}

NdbDictionary::Object::Status
NdbDictionary::Undofile::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::Undofile::getObjectVersion() const {
  return m_impl.m_version;
}

int 
NdbDictionary::Undofile::getObjectId() const {
  return m_impl.m_id;
}

/*****************************************************************
 * Dictionary facade
 */
NdbDictionary::Dictionary::Dictionary(Ndb & ndb)
  : m_impl(* new NdbDictionaryImpl(ndb, *this))
{
}

NdbDictionary::Dictionary::Dictionary(NdbDictionaryImpl & impl)
  : m_impl(impl) 
{
}
NdbDictionary::Dictionary::~Dictionary(){
  NdbDictionaryImpl * tmp = &m_impl;  
  if(this != tmp){
    delete tmp;
  }
}

int 
NdbDictionary::Dictionary::createTable(const Table & t)
{
  DBUG_ENTER("NdbDictionary::Dictionary::createTable");
  DBUG_RETURN(m_impl.createTable(NdbTableImpl::getImpl(t)));
}

int
NdbDictionary::Dictionary::dropTable(Table & t){
  return m_impl.dropTable(NdbTableImpl::getImpl(t));
}

int
NdbDictionary::Dictionary::dropTableGlobal(const Table & t){
  return m_impl.dropTableGlobal(NdbTableImpl::getImpl(t));
}

int
NdbDictionary::Dictionary::dropTable(const char * name){
  return m_impl.dropTable(name);
}

int
NdbDictionary::Dictionary::alterTable(const Table & t){
  return m_impl.alterTable(NdbTableImpl::getImpl(t));
}

int
NdbDictionary::Dictionary::alterTableGlobal(const Table & f,
                                            const Table & t)
{
  return m_impl.alterTableGlobal(NdbTableImpl::getImpl(f),
                                 NdbTableImpl::getImpl(t));
}

const NdbDictionary::Table * 
NdbDictionary::Dictionary::getTable(const char * name, void **data) const
{
  NdbTableImpl * t = m_impl.getTable(name, data);
  if(t)
    return t->m_facade;
  return 0;
}

const NdbDictionary::Index * 
NdbDictionary::Dictionary::getIndexGlobal(const char * indexName,
                                          const Table &ndbtab) const
{
  NdbIndexImpl * i = m_impl.getIndexGlobal(indexName,
                                           NdbTableImpl::getImpl(ndbtab));
  if(i)
    return i->m_facade;
  return 0;
}

const NdbDictionary::Table * 
NdbDictionary::Dictionary::getTableGlobal(const char * name) const
{
  NdbTableImpl * t = m_impl.getTableGlobal(name);
  if(t)
    return t->m_facade;
  return 0;
}

int
NdbDictionary::Dictionary::removeIndexGlobal(const Index &ndbidx,
                                             int invalidate) const
{
  return m_impl.releaseIndexGlobal(NdbIndexImpl::getImpl(ndbidx), invalidate);
}

int
NdbDictionary::Dictionary::removeTableGlobal(const Table &ndbtab,
                                             int invalidate) const
{
  return m_impl.releaseTableGlobal(NdbTableImpl::getImpl(ndbtab), invalidate);
}

void NdbDictionary::Dictionary::putTable(const NdbDictionary::Table * table)
{
 NdbDictionary::Table  *copy_table = new NdbDictionary::Table;
  *copy_table = *table;
  m_impl.putTable(&NdbTableImpl::getImpl(*copy_table));
}

void NdbDictionary::Dictionary::set_local_table_data_size(unsigned sz)
{
  m_impl.m_local_table_data_size= sz;
}

const NdbDictionary::Table * 
NdbDictionary::Dictionary::getTable(const char * name) const
{
  return getTable(name, 0);
}

const NdbDictionary::Table *
NdbDictionary::Dictionary::getBlobTable(const NdbDictionary::Table* table,
                                        const char* col_name)
{
  const NdbDictionary::Column* col = table->getColumn(col_name);
  if (col == NULL) {
    m_impl.m_error.code = 4318;
    return NULL;
  }
  return getBlobTable(table, col->getColumnNo());
}

const NdbDictionary::Table *
NdbDictionary::Dictionary::getBlobTable(const NdbDictionary::Table* table,
                                        Uint32 col_no)
{
  return m_impl.getBlobTable(NdbTableImpl::getImpl(*table), col_no);
}

void
NdbDictionary::Dictionary::invalidateTable(const char * name){
  DBUG_ENTER("NdbDictionaryImpl::invalidateTable");
  NdbTableImpl * t = m_impl.getTable(name);
  if(t)
    m_impl.invalidateObject(* t);
  DBUG_VOID_RETURN;
}

void
NdbDictionary::Dictionary::invalidateTable(const Table *table){
  NdbTableImpl &t = NdbTableImpl::getImpl(*table);
  m_impl.invalidateObject(t);
}

void
NdbDictionary::Dictionary::removeCachedTable(const char * name){
  NdbTableImpl * t = m_impl.getTable(name);
  if(t)
    m_impl.removeCachedObject(* t);
}

void
NdbDictionary::Dictionary::removeCachedTable(const Table *table){
  NdbTableImpl &t = NdbTableImpl::getImpl(*table);
  m_impl.removeCachedObject(t);
}

int
NdbDictionary::Dictionary::createIndex(const Index & ind)
{
  return m_impl.createIndex(NdbIndexImpl::getImpl(ind));
}

int
NdbDictionary::Dictionary::createIndex(const Index & ind, const Table & tab)
{
  return m_impl.createIndex(NdbIndexImpl::getImpl(ind),
                            NdbTableImpl::getImpl(tab));
}

int 
NdbDictionary::Dictionary::dropIndex(const char * indexName,
				     const char * tableName)
{
  return m_impl.dropIndex(indexName, tableName);
}

int 
NdbDictionary::Dictionary::dropIndexGlobal(const Index &ind)
{
  return m_impl.dropIndexGlobal(NdbIndexImpl::getImpl(ind));
}

const NdbDictionary::Index * 
NdbDictionary::Dictionary::getIndex(const char * indexName,
				    const char * tableName) const
{
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  if(i)
    return i->m_facade;
  return 0;
}

void
NdbDictionary::Dictionary::invalidateIndex(const Index *index){
  DBUG_ENTER("NdbDictionary::Dictionary::invalidateIndex");
  NdbIndexImpl &i = NdbIndexImpl::getImpl(*index);
  assert(i.m_table != 0);
  m_impl.invalidateObject(* i.m_table);
  DBUG_VOID_RETURN;
}

void
NdbDictionary::Dictionary::invalidateIndex(const char * indexName,
                                           const char * tableName){
  DBUG_ENTER("NdbDictionaryImpl::invalidateIndex");
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  if(i) {
    assert(i->m_table != 0);
    m_impl.invalidateObject(* i->m_table);
  }
  DBUG_VOID_RETURN;
}

int
NdbDictionary::Dictionary::forceGCPWait()
{
  return m_impl.forceGCPWait();
}

void
NdbDictionary::Dictionary::removeCachedIndex(const Index *index){
  DBUG_ENTER("NdbDictionary::Dictionary::removeCachedIndex");
  NdbIndexImpl &i = NdbIndexImpl::getImpl(*index);
  assert(i.m_table != 0);
  m_impl.removeCachedObject(* i.m_table);
  DBUG_VOID_RETURN;
}

void
NdbDictionary::Dictionary::removeCachedIndex(const char * indexName,
					     const char * tableName){
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  if(i) {
    assert(i->m_table != 0);
    m_impl.removeCachedObject(* i->m_table);
  }
}

const NdbDictionary::Table *
NdbDictionary::Dictionary::getIndexTable(const char * indexName, 
					 const char * tableName) const
{
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  NdbTableImpl * t = m_impl.getTable(tableName);
  if(i && t) {
    NdbTableImpl * it = m_impl.getIndexTable(i, t);
    return it->m_facade;
  }
  return 0;
}


int
NdbDictionary::Dictionary::createEvent(const Event & ev)
{
  return m_impl.createEvent(NdbEventImpl::getImpl(ev));
}

int 
NdbDictionary::Dictionary::dropEvent(const char * eventName)
{
  return m_impl.dropEvent(eventName);
}

const NdbDictionary::Event *
NdbDictionary::Dictionary::getEvent(const char * eventName)
{
  NdbEventImpl * t = m_impl.getEvent(eventName);
  if(t)
    return t->m_facade;
  return 0;
}

int
NdbDictionary::Dictionary::listObjects(List& list, Object::Type type)
{
  return m_impl.listObjects(list, type);
}

int
NdbDictionary::Dictionary::listObjects(List& list, Object::Type type) const
{
  return m_impl.listObjects(list, type);
}

int
NdbDictionary::Dictionary::listIndexes(List& list, const char * tableName)
{
  const NdbDictionary::Table* tab= getTable(tableName);
  if(tab == 0)
  {
    return -1;
  }
  return m_impl.listIndexes(list, tab->getTableId());
}

int
NdbDictionary::Dictionary::listIndexes(List& list,
				       const char * tableName) const
{
  const NdbDictionary::Table* tab= getTable(tableName);
  if(tab == 0)
  {
    return -1;
  }
  return m_impl.listIndexes(list, tab->getTableId());
}

int
NdbDictionary::Dictionary::listIndexes(List& list,
				       const NdbDictionary::Table &table) const
{
  return m_impl.listIndexes(list, table.getTableId());
}


const struct NdbError & 
NdbDictionary::Dictionary::getNdbError() const {
  return m_impl.getNdbError();
}

// printers

NdbOut&
operator<<(NdbOut& out, const NdbDictionary::Column& col)
{
  const CHARSET_INFO *cs = col.getCharset();
  const char *csname = cs ? cs->name : "?";
  out << col.getName() << " ";
  switch (col.getType()) {
  case NdbDictionary::Column::Tinyint:
    out << "Tinyint";
    break;
  case NdbDictionary::Column::Tinyunsigned:
    out << "Tinyunsigned";
    break;
  case NdbDictionary::Column::Smallint:
    out << "Smallint";
    break;
  case NdbDictionary::Column::Smallunsigned:
    out << "Smallunsigned";
    break;
  case NdbDictionary::Column::Mediumint:
    out << "Mediumint";
    break;
  case NdbDictionary::Column::Mediumunsigned:
    out << "Mediumunsigned";
    break;
  case NdbDictionary::Column::Int:
    out << "Int";
    break;
  case NdbDictionary::Column::Unsigned:
    out << "Unsigned";
    break;
  case NdbDictionary::Column::Bigint:
    out << "Bigint";
    break;
  case NdbDictionary::Column::Bigunsigned:
    out << "Bigunsigned";
    break;
  case NdbDictionary::Column::Float:
    out << "Float";
    break;
  case NdbDictionary::Column::Double:
    out << "Double";
    break;
  case NdbDictionary::Column::Olddecimal:
    out << "Olddecimal(" << col.getPrecision() << "," << col.getScale() << ")";
    break;
  case NdbDictionary::Column::Olddecimalunsigned:
    out << "Olddecimalunsigned(" << col.getPrecision() << "," << col.getScale() << ")";
    break;
  case NdbDictionary::Column::Decimal:
    out << "Decimal(" << col.getPrecision() << "," << col.getScale() << ")";
    break;
  case NdbDictionary::Column::Decimalunsigned:
    out << "Decimalunsigned(" << col.getPrecision() << "," << col.getScale() << ")";
    break;
  case NdbDictionary::Column::Char:
    out << "Char(" << col.getLength() << ";" << csname << ")";
    break;
  case NdbDictionary::Column::Varchar:
    out << "Varchar(" << col.getLength() << ";" << csname << ")";
    break;
  case NdbDictionary::Column::Binary:
    out << "Binary(" << col.getLength() << ")";
    break;
  case NdbDictionary::Column::Varbinary:
    out << "Varbinary(" << col.getLength() << ")";
    break;
  case NdbDictionary::Column::Datetime:
    out << "Datetime";
    break;
  case NdbDictionary::Column::Date:
    out << "Date";
    break;
  case NdbDictionary::Column::Blob:
    out << "Blob(" << col.getInlineSize() << "," << col.getPartSize()
        << ";" << col.getStripeSize() << ")";
    break;
  case NdbDictionary::Column::Text:
    out << "Text(" << col.getInlineSize() << "," << col.getPartSize()
        << ";" << col.getStripeSize() << ";" << csname << ")";
    break;
  case NdbDictionary::Column::Time:
    out << "Time";
    break;
  case NdbDictionary::Column::Year:
    out << "Year";
    break;
  case NdbDictionary::Column::Timestamp:
    out << "Timestamp";
    break;
  case NdbDictionary::Column::Undefined:
    out << "Undefined";
    break;
  case NdbDictionary::Column::Bit:
    out << "Bit(" << col.getLength() << ")";
    break;
  case NdbDictionary::Column::Longvarchar:
    out << "Longvarchar(" << col.getLength() << ";" << csname << ")";
    break;
  case NdbDictionary::Column::Longvarbinary:
    out << "Longvarbinary(" << col.getLength() << ")";
    break;
  default:
    out << "Type" << (Uint32)col.getType();
    break;
  }
  // show unusual (non-MySQL) array size
  if (col.getLength() != 1) {
    switch (col.getType()) {
    case NdbDictionary::Column::Char:
    case NdbDictionary::Column::Varchar:
    case NdbDictionary::Column::Binary:
    case NdbDictionary::Column::Varbinary:
    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Text:
    case NdbDictionary::Column::Bit:
    case NdbDictionary::Column::Longvarchar:
    case NdbDictionary::Column::Longvarbinary:
      break;
    default:
      out << " [" << col.getLength() << "]";
      break;
    }
  }

  if (col.getPrimaryKey())
    out << " PRIMARY KEY";
  else if (! col.getNullable())
    out << " NOT NULL";
  else
    out << " NULL";

  if(col.getDistributionKey())
    out << " DISTRIBUTION KEY";

  switch (col.getArrayType()) {
  case NDB_ARRAYTYPE_FIXED:
    out << " AT=FIXED";
    break;
  case NDB_ARRAYTYPE_SHORT_VAR:
    out << " AT=SHORT_VAR";
    break;
  case NDB_ARRAYTYPE_MEDIUM_VAR:
    out << " AT=MEDIUM_VAR";
    break;
  default:
    out << " AT=" << (int)col.getArrayType() << "?";
    break;
  }

  switch (col.getStorageType()) {
  case NDB_STORAGETYPE_MEMORY:
    out << " ST=MEMORY";
    break;
  case NDB_STORAGETYPE_DISK:
    out << " ST=DISK";
    break;
  default:
    out << " ST=" << (int)col.getStorageType() << "?";
    break;
  }

  return out;
}

int
NdbDictionary::Dictionary::createLogfileGroup(const LogfileGroup & lg,
					      ObjectId * obj)
{
  return m_impl.createLogfileGroup(NdbLogfileGroupImpl::getImpl(lg),
				   obj ? 
				   & NdbDictObjectImpl::getImpl(* obj) : 0);
}

int
NdbDictionary::Dictionary::dropLogfileGroup(const LogfileGroup & lg)
{
  return m_impl.dropLogfileGroup(NdbLogfileGroupImpl::getImpl(lg));
}

NdbDictionary::LogfileGroup
NdbDictionary::Dictionary::getLogfileGroup(const char * name)
{
  NdbDictionary::LogfileGroup tmp;
  m_impl.m_receiver.get_filegroup(NdbLogfileGroupImpl::getImpl(tmp), 
				  NdbDictionary::Object::LogfileGroup, name);
  return tmp;
}

int
NdbDictionary::Dictionary::createTablespace(const Tablespace & lg,
					    ObjectId * obj)
{
  return m_impl.createTablespace(NdbTablespaceImpl::getImpl(lg),
				 obj ? 
				 & NdbDictObjectImpl::getImpl(* obj) : 0);
}

int
NdbDictionary::Dictionary::dropTablespace(const Tablespace & lg)
{
  return m_impl.dropTablespace(NdbTablespaceImpl::getImpl(lg));
}

NdbDictionary::Tablespace
NdbDictionary::Dictionary::getTablespace(const char * name)
{
  NdbDictionary::Tablespace tmp;
  m_impl.m_receiver.get_filegroup(NdbTablespaceImpl::getImpl(tmp), 
				  NdbDictionary::Object::Tablespace, name);
  return tmp;
}

NdbDictionary::Tablespace
NdbDictionary::Dictionary::getTablespace(Uint32 tablespaceId)
{
  NdbDictionary::Tablespace tmp;
  m_impl.m_receiver.get_filegroup(NdbTablespaceImpl::getImpl(tmp), 
				  NdbDictionary::Object::Tablespace,
                                  tablespaceId);
  return tmp;
}

int
NdbDictionary::Dictionary::createDatafile(const Datafile & df, 
					  bool force,
					  ObjectId * obj)
{
  return m_impl.createDatafile(NdbDatafileImpl::getImpl(df), 
			       force,
			       obj ? & NdbDictObjectImpl::getImpl(* obj) : 0);
}

int
NdbDictionary::Dictionary::dropDatafile(const Datafile& df)
{
  return m_impl.dropDatafile(NdbDatafileImpl::getImpl(df));
}

NdbDictionary::Datafile
NdbDictionary::Dictionary::getDatafile(Uint32 node, const char * path)
{
  NdbDictionary::Datafile tmp;
  m_impl.m_receiver.get_file(NdbDatafileImpl::getImpl(tmp),
			     NdbDictionary::Object::Datafile,
			     node ? (int)node : -1, path);
  return tmp;
}

int
NdbDictionary::Dictionary::createUndofile(const Undofile & df, 
					  bool force,
					  ObjectId * obj)
{
  return m_impl.createUndofile(NdbUndofileImpl::getImpl(df), 
			       force,
			       obj ? & NdbDictObjectImpl::getImpl(* obj) : 0);
}

int
NdbDictionary::Dictionary::dropUndofile(const Undofile& df)
{
  return m_impl.dropUndofile(NdbUndofileImpl::getImpl(df));
}

NdbDictionary::Undofile
NdbDictionary::Dictionary::getUndofile(Uint32 node, const char * path)
{
  NdbDictionary::Undofile tmp;
  m_impl.m_receiver.get_file(NdbUndofileImpl::getImpl(tmp),
			     NdbDictionary::Object::Undofile,
			     node ? (int)node : -1, path);
  return tmp;
}

