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

#include <NdbDictionary.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbOut.hpp>

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

void 
NdbDictionary::Column::setName(const char * name){
  m_impl.m_name.assign(name);
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
NdbDictionary::Column::setTupleKey(bool val){
  m_impl.m_tupleKey = val;
}

bool 
NdbDictionary::Column::getTupleKey() const {
  return m_impl.m_tupleKey;
}

void 
NdbDictionary::Column::setDistributionKey(bool val){
  m_impl.m_distributionKey = val;
}

bool 
NdbDictionary::Column::getDistributionKey() const{
  return m_impl.m_distributionKey;
}

void
NdbDictionary::Column::setDistributionGroup(bool val, int bits){
  m_impl.m_distributionGroup = val;
  m_impl.m_distributionGroupBits = bits;
}

bool 
NdbDictionary::Column::getDistributionGroup() const {
  return m_impl.m_distributionGroup;
}

int 
NdbDictionary::Column::getDistributionGroupBits() const{
  return m_impl.m_distributionGroupBits;
}

void 
NdbDictionary::Column::setIndexOnlyStorage(bool val){
  m_impl.m_indexOnly = val;
}

bool 
NdbDictionary::Column::getIndexOnlyStorage() const {
  return m_impl.m_indexOnly;
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

void
NdbDictionary::Column::setDefaultValue(const char* defaultValue)
{
  m_impl.m_defaultValue.assign(defaultValue);
}

const char*
NdbDictionary::Column::getDefaultValue() const
{
  return m_impl.m_defaultValue.c_str();
}

int
NdbDictionary::Column::getColumnNo() const {
  return m_impl.m_attrId;
}

bool
NdbDictionary::Column::equal(const NdbDictionary::Column & col) const {
  return m_impl.equal(col.m_impl);
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
  : NdbDictionary::Object(),
    m_impl(* new NdbTableImpl(* this))
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

void 
NdbDictionary::Table::setName(const char * name){
  m_impl.setName(name);
}

const char * 
NdbDictionary::Table::getName() const {
  return m_impl.getName();
}

int
NdbDictionary::Table::getTableId() const {
  return m_impl.m_tableId;
}

void 
NdbDictionary::Table::addColumn(const Column & c){
  NdbColumnImpl* col = new NdbColumnImpl;
  (* col) = NdbColumnImpl::getImpl(c);
  m_impl.m_columns.push_back(col);
  if(c.getPrimaryKey()){
    m_impl.m_noOfKeys++;
  }
  if (col->getBlobType()) {
    m_impl.m_noOfBlobs++;
  }
  m_impl.buildColumnHash();
}

const NdbDictionary::Column*
NdbDictionary::Table::getColumn(const char * name) const {
  return m_impl.getColumn(name);
}

const NdbDictionary::Column* 
NdbDictionary::Table::getColumn(const int attrId) const {
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
  return m_impl.m_frm.get_data();
}

Uint32
NdbDictionary::Table::getFrmLength() const {
  return m_impl.m_frm.length();
}

void
NdbDictionary::Table::setFrm(const void* data, Uint32 len){
  m_impl.m_frm.assign(data, len);
}

NdbDictionary::Object::Status
NdbDictionary::Table::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::Table::getObjectVersion() const {
  return m_impl.m_version;
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
    const NdbColumnImpl & col = NdbColumnImpl::getImpl(* c);
    sz += (((col.m_attrSize * col.m_arraySize) + 3) / 4);
  }
  return sz * 4;
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

void 
NdbDictionary::Index::setName(const char * name){
  m_impl.setName(name);
}

const char * 
NdbDictionary::Index::getName() const {
  return m_impl.getName();
}

void 
NdbDictionary::Index::setTable(const char * table){
  m_impl.setTable(table);
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

void
NdbDictionary::Index::addColumn(const Column & c){
  NdbColumnImpl* col = new NdbColumnImpl;
  (* col) = NdbColumnImpl::getImpl(c);
  m_impl.m_columns.push_back(col);
}

void
NdbDictionary::Index::addColumnName(const char * name){
  const Column c(name);
  addColumn(c);
}

void
NdbDictionary::Index::addIndexColumn(const char * name){
  const Column c(name);
  addColumn(c);
}

void
NdbDictionary::Index::addColumnNames(unsigned noOfNames, const char ** names){
  for(unsigned i = 0; i < noOfNames; i++) {
    const Column c(names[i]);
    addColumn(c);
  }
}

void
NdbDictionary::Index::addIndexColumns(int noOfNames, const char ** names){
  for(int i = 0; i < noOfNames; i++) {
    const Column c(names[i]);
    addColumn(c);
  }
}

void
NdbDictionary::Index::setType(NdbDictionary::Index::Type t){
  m_impl.m_type = t;
}

NdbDictionary::Index::Type
NdbDictionary::Index::getType() const {
  return m_impl.m_type;
}

void
NdbDictionary::Index::setLogging(bool val){
  m_impl.m_logging = val;
}

bool 
NdbDictionary::Index::getLogging() const {
  return m_impl.m_logging;
}

NdbDictionary::Object::Status
NdbDictionary::Index::getObjectStatus() const {
  return m_impl.m_status;
}

int 
NdbDictionary::Index::getObjectVersion() const {
  return m_impl.m_version;
}

/*****************************************************************
 * Event facade
 */
NdbDictionary::Event::Event(const char * name)
  : m_impl(* new NdbEventImpl(* this))
{
  setName(name);
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

void 
NdbDictionary::Event::setName(const char * name)
{
  m_impl.setName(name);
}

void 
NdbDictionary::Event::setTable(const char * table)
{
  m_impl.setTable(table);
}

void
NdbDictionary::Event::addTableEvent(const TableEvent t)
{
  m_impl.addTableEvent(t);
}

void
NdbDictionary::Event::setDurability(const EventDurability d)
{
  m_impl.setDurability(d);
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

void NdbDictionary::Event::print()
{
  m_impl.print();
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
NdbDictionary::Dictionary::createTable(const Table & t){
  return m_impl.createTable(NdbTableImpl::getImpl(t));
}

int
NdbDictionary::Dictionary::dropTable(Table & t){
  return m_impl.dropTable(NdbTableImpl::getImpl(t));
}

int
NdbDictionary::Dictionary::dropTable(const char * name){
  return m_impl.dropTable(name);
}

int
NdbDictionary::Dictionary::alterTable(const Table & t){
  return m_impl.alterTable(NdbTableImpl::getImpl(t));
}

const NdbDictionary::Table * 
NdbDictionary::Dictionary::getTable(const char * name, void **data){
  NdbTableImpl * t = m_impl.getTable(name, data);
  if(t)
    return t->m_facade;
  return 0;
}

void NdbDictionary::Dictionary::set_local_table_data_size(unsigned sz)
{
  m_impl.m_local_table_data_size= sz;
}

const NdbDictionary::Table * 
NdbDictionary::Dictionary::getTable(const char * name){
  return getTable(name, 0);
}

void
NdbDictionary::Dictionary::invalidateTable(const char * name){
  NdbTableImpl * t = m_impl.getTable(name);
  if(t)
    m_impl.invalidateObject(* t);
}

void
NdbDictionary::Dictionary::removeCachedTable(const char * name){
  NdbTableImpl * t = m_impl.getTable(name);
  if(t)
    m_impl.removeCachedObject(* t);
}

NdbDictionary::Table
NdbDictionary::Dictionary::getTableForAlteration(const char * name){
  const NdbDictionary::Table * oldTable = getTable(name);
  return (oldTable) ? 
    NdbDictionary::Table(*oldTable) 
    : NdbDictionary::Table();
}

int
NdbDictionary::Dictionary::createIndex(const Index & ind)
{
  return m_impl.createIndex(NdbIndexImpl::getImpl(ind));
}

int 
NdbDictionary::Dictionary::dropIndex(const char * indexName,
				     const char * tableName)
{
  return m_impl.dropIndex(indexName, tableName);
}

const NdbDictionary::Index * 
NdbDictionary::Dictionary::getIndex(const char * indexName,
				    const char * tableName)
{
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  if(i)
    return i->m_facade;
  return 0;
}

void
NdbDictionary::Dictionary::invalidateIndex(const char * indexName,
                                           const char * tableName){
  NdbIndexImpl * i = m_impl.getIndex(indexName, tableName);
  if(i) {
    assert(i->m_table != 0);
    m_impl.invalidateObject(* i->m_table);
  }
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
					 const char * tableName)
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
NdbDictionary::Dictionary::listIndexes(List& list, const char * tableName)
{
  return m_impl.listIndexes(list, tableName);
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
  case NdbDictionary::Column::Decimal:
    out << "Decimal(" << col.getScale() << "," << col.getPrecision() << ")";
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
  case NdbDictionary::Column::Timespec:
    out << "Timespec";
    break;
  case NdbDictionary::Column::Blob:
    out << "Blob(" << col.getInlineSize() << "," << col.getPartSize()
        << ";" << col.getStripeSize() << ")";
    break;
  case NdbDictionary::Column::Text:
    out << "Text(" << col.getInlineSize() << "," << col.getPartSize()
        << ";" << col.getStripeSize() << ";" << csname << ")";
    break;
  case NdbDictionary::Column::Undefined:
    out << "Undefined";
    break;
  default:
    out << "Type" << (Uint32)col.getType();
    break;
  }
  if (col.getPrimaryKey())
    out << " PRIMARY KEY";
  else if (! col.getNullable())
    out << " NOT NULL";
  else
    out << " NULL";
  return out;
}

const NdbDictionary::Column * NdbDictionary::Column::FRAGMENT = 0;
const NdbDictionary::Column * NdbDictionary::Column::ROW_COUNT = 0;
const NdbDictionary::Column * NdbDictionary::Column::COMMIT_COUNT = 0;

