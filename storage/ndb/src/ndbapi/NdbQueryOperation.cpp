/*
   Copyright (C) 2009 Sun Microsystems Inc
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include "NdbQueryOperationImpl.hpp"
#include <ndb_global.h>
#include "NdbQueryBuilder.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include "signaldata/QueryTree.hpp"
#include "AttributeHeader.hpp"

NdbQuery::NdbQuery(NdbQueryImpl& impl):
  m_impl(impl)
{}

NdbQuery::~NdbQuery()
{}

Uint32
NdbQuery::getNoOfOperations() const
{
  return m_impl.getNoOfOperations();
}

NdbQueryOperation*
NdbQuery::getQueryOperation(Uint32 index) const
{
  return &m_impl.getQueryOperation(index).getInterface();
}

NdbQueryOperation*
NdbQuery::getQueryOperation(const char* ident) const
{
  NdbQueryOperationImpl* op = m_impl.getQueryOperation(ident);
  return (op) ? &op->getInterface() : NULL;
}

Uint32
NdbQuery::getNoOfParameters() const
{
  return m_impl.getNoOfParameters();
}

const NdbParamOperand*
NdbQuery::getParameter(const char* name) const
{
  return m_impl.getParameter(name);
}

const NdbParamOperand*
NdbQuery::getParameter(Uint32 num) const
{
  return m_impl.getParameter(num);
}

int
NdbQuery::nextResult(bool fetchAllowed, bool forceSend)
{
  return m_impl.nextResult(fetchAllowed, forceSend);
}

void
NdbQuery::close(bool forceSend, bool release)
{
  m_impl.close(forceSend,release);
}

NdbTransaction*
NdbQuery::getNdbTransaction() const
{
  return m_impl.getNdbTransaction();
}

const NdbError& 
NdbQuery::getNdbError() const {
  return m_impl.getNdbError();
};

NdbQueryOperation::NdbQueryOperation(NdbQueryOperationImpl& impl)
  :m_impl(impl)
{}
NdbQueryOperation::~NdbQueryOperation()
{}

Uint32
NdbQueryOperation::getNoOfParentOperations() const
{
  return m_impl.getNoOfParentOperations();
}

NdbQueryOperation*
NdbQueryOperation::getParentOperation(Uint32 i) const
{
  return &m_impl.getParentOperation(i).getInterface();
}

Uint32 
NdbQueryOperation::getNoOfChildOperations() const
{
  return m_impl.getNoOfChildOperations();
}

NdbQueryOperation* 
NdbQueryOperation::getChildOperation(Uint32 i) const
{
  return &m_impl.getChildOperation(i).getInterface();
}

const NdbQueryOperationDef&
NdbQueryOperation::getQueryOperationDef() const
{
  return m_impl.getQueryOperationDef().getInterface();
}

NdbQuery& 
NdbQueryOperation::getQuery() const {
  return m_impl.getQuery().getInterface();
};

NdbRecAttr*
NdbQueryOperation::getValue(const char* anAttrName,
			    char* aValue)
{
  return m_impl.getValue(anAttrName, aValue);
}

NdbRecAttr*
NdbQueryOperation::getValue(Uint32 anAttrId, 
			    char* aValue)
{
  return m_impl.getValue(anAttrId, aValue);
}

NdbRecAttr*
NdbQueryOperation::getValue(const NdbDictionary::Column* column, 
			    char* aValue)
{
  return m_impl.getValue(column, aValue);
}

int
NdbQueryOperation::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  return m_impl.setResultRowBuf(rec, resBuffer, result_mask);
}

int
NdbQueryOperation::setResultRowRef (
                       const NdbRecord* rec,
                       char* & bufRef,
                       const unsigned char* result_mask)
{
  return m_impl.setResultRowRef(rec, bufRef, result_mask);
}

bool
NdbQueryOperation::isRowNULL() const
{
  return m_impl.isRowNULL();
}

bool
NdbQueryOperation::isRowChanged() const
{
  return m_impl.isRowChanged();
}


///////////////////////////////////////////
/////////  NdbQueryImpl methods ///////////
///////////////////////////////////////////
NdbQueryImpl::NdbQueryImpl(NdbTransaction& trans, 
                           const NdbQueryDefImpl& queryDef, 
                           const void* const param[],
                           NdbQueryImpl* next):
  m_interface(*this),
  m_magic(MAGIC),
  m_id(trans.getNdb()->theImpl->theNdbObjectIdMap.map(this)),
  m_error(),
  m_transaction(trans),
  m_operations(),
  /* We will always receive a TCKEYCONF signal, even if the root operation
   * yields no result.*/
  m_tcKeyConfReceived(false),
  // Initially, only a result from the root is expected.
  m_pendingOperations(1), 
  m_param(param),
  m_next(next),
  m_ndbOperation(NULL),
  m_queryDef(queryDef)
{
  assert(m_id != NdbObjectIdMap::InvalidId);

  for (Uint32 i=0; i<queryDef.getNoOfOperations(); ++i)
  {
    const NdbQueryOperationDefImpl& def = queryDef.getQueryOperation(i);

    NdbQueryOperationImpl* op = new NdbQueryOperationImpl(*this, def);

    m_operations.push_back(op);
    if(def.getNoOfParentOperations()==0)
    {
      // TODO: Remove references to NdbOperation class.
      assert(m_ndbOperation == NULL);
      if (def.getType() == NdbQueryOperationDefImpl::PrimaryKeyAccess)
      {
        NdbOperation* lookupOp = m_transaction.getNdbOperation(&def.getTable());
        lookupOp->readTuple(NdbOperation::LM_Dirty);
        lookupOp->m_isLinked = true;
        lookupOp->setQueryImpl(this);
        m_ndbOperation = lookupOp;
      }
      else if (def.getType() == NdbQueryOperationDefImpl::TableScan)
      {
        NdbScanOperation* scanOp = m_transaction.scanTable(def.getTable().getDefaultRecord(), NdbOperation::LM_Dirty);
//      scanOp->readTuples(NdbOperation::LM_Dirty);
        scanOp->m_isLinked = true;
        scanOp->setQueryImpl(this);
        m_ndbOperation = scanOp;
      } else {
        assert(false);
      }
    }
  }
}

NdbQueryImpl::~NdbQueryImpl()
{
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_transaction.getNdb()->theImpl->theNdbObjectIdMap.unmap(m_id, this);
  }

  for (Uint32 i=0; i<m_operations.size(); ++i)
  { delete m_operations[i];
  }
}

//static
NdbQueryImpl*
NdbQueryImpl::buildQuery(NdbTransaction& trans, 
                         const NdbQueryDefImpl& queryDef, 
                         const void* const param[],
                         NdbQueryImpl* next)
{
  return new NdbQueryImpl(trans, queryDef, param, next);
}

Uint32
NdbQueryImpl::getNoOfOperations() const
{
  return m_operations.size();
}

NdbQueryOperationImpl&
NdbQueryImpl::getQueryOperation(Uint32 index) const
{
  return *m_operations[index];
}

NdbQueryOperationImpl*
NdbQueryImpl::getQueryOperation(const char* ident) const
{
  return NULL; // FIXME
}

Uint32
NdbQueryImpl::getNoOfParameters() const
{
  return 0;  // FIXME
}

const NdbParamOperand*
NdbQueryImpl::getParameter(const char* name) const
{
  return NULL; // FIXME
}

const NdbParamOperand*
NdbQueryImpl::getParameter(Uint32 num) const
{
  return NULL; // FIXME
}

int
NdbQueryImpl::nextResult(bool fetchAllowed, bool forceSend)
{
  return 1; // FIXME
}

void
NdbQueryImpl::close(bool forceSend, bool release)
{
  // FIXME
}

NdbTransaction*
NdbQueryImpl::getNdbTransaction() const
{
  return &m_transaction;
}

bool 
NdbQueryImpl::execTCKEYCONF(){
  ndbout << "NdbQueryImpl::execTCKEYCONF()  m_pendingOperations=" 
         << m_pendingOperations << endl;
  m_tcKeyConfReceived = true;
#ifndef NDEBUG // Compile with asserts activated.
  if(m_pendingOperations==0){
    for(Uint32 i = 0; i < getNoOfOperations(); i++){
      assert(getQueryOperation(i).isComplete());
    }
  }
#endif
  return m_pendingOperations==0;
}

bool 
NdbQueryImpl::incPendingOperations(int increment){
  m_pendingOperations += increment;
#ifndef NDEBUG // Compile with asserts activated.
  if(m_pendingOperations==0 && m_tcKeyConfReceived){
    for(Uint32 i = 0; i < getNoOfOperations(); i++){
      assert(getQueryOperation(i).isComplete());
    }
  }
#endif
  return m_pendingOperations==0 && m_tcKeyConfReceived;
}

int
NdbQueryImpl::prepareSend(){
  // Serialize parameters.
  for(Uint32 i = 0; i < m_operations.size(); i++){
    const int error = m_operations[i]->prepareSend(m_serializedParams);
    if(error != 0){
      return error;
    }
  }
  // Append serialized query tree and params to ATTRINFO of the NdnOperation.
  m_ndbOperation->insertATTRINFOloop(&m_queryDef.getSerialized().get(0),
                                     m_queryDef.getSerialized().getSize());
  m_ndbOperation->insertATTRINFOloop(&m_serializedParams.get(0), 
                                     m_serializedParams.getSize());

  // Build explicit key/filter/bounds for root operation.
  m_operations[0]->getQueryOperationDef()
    .materializeRootOperands(*getNdbOperation(), m_param);
#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized params for all : ";
  for(Uint32 i = 0; i < m_serializedParams.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", m_serializedParams.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
  return 0;
}

void 
NdbQueryImpl::release(){
  for(Uint32 i = 0; i < m_operations.size(); i++){
      m_operations[i]->release();
  }
}

////////////////////////////////////////////////////
/////////  NdbQueryOperationImpl methods ///////////
////////////////////////////////////////////////////

NdbQueryOperationImpl::NdbQueryOperationImpl(
           NdbQueryImpl& queryImpl,
           const NdbQueryOperationDefImpl& def):
  m_interface(*this),
  m_magic(MAGIC),
  m_id(queryImpl.getNdbTransaction()->getNdb()->theImpl
       ->theNdbObjectIdMap.map(this)),
  m_operationDef(def),
  m_parents(def.getNoOfParentOperations()),
  m_children(def.getNoOfChildOperations()),
  m_receiver(queryImpl.getNdbTransaction()->getNdb()),
  m_queryImpl(queryImpl),
  /* Initially, a result is only expected for the root operation.*/
  m_pendingResults(def.getQueryOperationIx() == 0 ? 1 : 0),
  m_userProjection(def.getTable())
{ 
  assert(m_id != NdbObjectIdMap::InvalidId);
  m_receiver.init(NdbReceiver::NDB_OPERATION, false, NULL);
  // Fill in operations parent refs, and append it as child of its parents
  for (Uint32 p=0; p<def.getNoOfParentOperations(); ++p)
  { 
    const NdbQueryOperationDefImpl& parent = def.getParentOperation(p);
    const Uint32 ix = parent.getQueryOperationIx();
    assert (ix < m_queryImpl.getNoOfOperations());
    m_parents.push_back(&m_queryImpl.getQueryOperation(ix));
    m_queryImpl.getQueryOperation(ix).m_children.push_back(this);
  }
}

Uint32
NdbQueryOperationImpl::getNoOfParentOperations() const
{
  return m_parents.size();
}

NdbQueryOperationImpl&
NdbQueryOperationImpl::getParentOperation(Uint32 i) const
{
  return *m_parents[i];
}

Uint32 
NdbQueryOperationImpl::getNoOfChildOperations() const
{
  return m_children.size();
}

NdbQueryOperationImpl&
NdbQueryOperationImpl::getChildOperation(Uint32 i) const
{
  return *m_children[i];
}

const NdbQueryOperationDefImpl&
NdbQueryOperationImpl::getQueryOperationDef() const
{
  return m_operationDef;
}

NdbQueryImpl& 
NdbQueryOperationImpl::getQuery() const
{
  return m_queryImpl;
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const char* anAttrName,
                            char* aValue)
{
  return NULL; // FIXME
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            Uint32 anAttrId, 
                            char* aValue)
{
  return NULL; // FIXME
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const NdbDictionary::Column* column, 
                            char* aValue)
{
  /* This code will only work for the lookup example in test_spj.cpp.
   */
  assert(aValue==NULL);
  m_userProjection.addColumn(*column);
  return m_receiver.getValue(&NdbColumnImpl::getImpl(*column), aValue);
}


int
NdbQueryOperationImpl::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
/***
  if (rec->tableId != m_table->tableId)
  {
    setErrorCode(4287);
    return -1;
  }
***/
  return 0; // FIXME
}

int
NdbQueryOperationImpl::setResultRowRef (
                       const NdbRecord* rec,
                       char* & bufRef,
                       const unsigned char* result_mask)
{
/***
  if (rec->tableId != m_table->tableId)
  {
    setErrorCode(4287);
    return -1;
  }
***/
  return 0; // FIXME
}

bool
NdbQueryOperationImpl::isRowNULL() const
{
  return true;  // FIXME
}

bool
NdbQueryOperationImpl::isRowChanged() const
{
  return false;  // FIXME
}

// Constructor.
NdbQueryOperationImpl::UserProjection
::UserProjection(const NdbDictionary::Table& tab):
  m_columnCount(0),
  m_noOfColsInTable(tab.getNoOfColumns()),
  m_isOrdered(true),
  m_maxColNo(-1){
  assert(m_noOfColsInTable<=MAX_ATTRIBUTES_IN_TABLE);
}

void
NdbQueryOperationImpl::UserProjection
::addColumn(const NdbDictionary::Column& col){
  const int colNo = col.getColumnNo();
  assert(colNo<m_noOfColsInTable);
  if(colNo<=m_maxColNo){
    m_isOrdered = false;
  }
  m_maxColNo = MAX(colNo, m_maxColNo);
  m_columns[m_columnCount++] = &col;
  // TODO: Add error handling that somehow returns an error to the application.
  assert(m_columnCount<=MAX_ATTRIBUTES_IN_TABLE);
  m_mask.set(colNo);
}

int
NdbQueryOperationImpl::UserProjection::serialize(Uint32Slice dst) const{
  /* If the columns in the projections are ordered according to ascending
   * column number, we can pack the projection more compactly.*/
  if(m_isOrdered){
    // Special case: get all columns.
    if(m_columnCount==m_noOfColsInTable){
      dst.get(0) = 1; // Size of projection in words.
      AttributeHeader::init(&dst.get(1), 
                            AttributeHeader::READ_ALL,
                            m_columnCount);
    }else{
      /* Serialize projection as a bitmap.*/
      const Uint32 wordCount = 1+m_maxColNo/32; // Size of mask.
      dst.get(0) = wordCount+1; // Size of projection in words.
      AttributeHeader::init(&dst.get(1), 
                            AttributeHeader::READ_PACKED, 4*wordCount);
      
      memcpy(&dst.get(2, wordCount), &m_mask, 4*wordCount);
    }
  }else{
    /* General case: serialize projection as a list of column numbers.*/
    dst.get(0) = m_columnCount; // Size of projection in words. 
    for(int i = 0; i<m_columnCount; i++){
      AttributeHeader::init(&dst.get(i+1),
                            m_columns[i]->getColumnNo(),
                            0);
    }
  }
  if(unlikely(dst.isMaxSizeExceeded())){
    return QRY_DEFINITION_TOO_LARGE; // Query definition too large.
  }
  return 0;
}

#define POS_IN_PARAM(field) \
(offsetof(QueryNodeParameters, field)/sizeof(Uint32))

#define POS_IN_LOOKUP_PARAM(field) \
(offsetof(QN_LookupParameters, field)/sizeof(Uint32)) 


int NdbQueryOperationImpl::prepareSend(Uint32Buffer& serializedParams)
{
  const NdbQueryOperationDefImpl::Type opType 
    = getQueryOperationDef().getType();
  const bool isScan = (opType == NdbQueryOperationDefImpl::TableScan ||
                       opType == NdbQueryOperationDefImpl::OrderedIndexScan);

  m_receiver.prepareSend();
  Uint32Slice lookupParams(serializedParams, serializedParams.getSize());
  Uint32 requestInfo = 0;
  lookupParams.get(POS_IN_PARAM(requestInfo)) = 0;
  lookupParams.get(POS_IN_PARAM(resultData)) = m_id;
  Uint32Slice optional(lookupParams, POS_IN_LOOKUP_PARAM(optional));

  int optPos = 0;

  // SPJ block assume PARAMS to be supplied before ATTR_LIST
  if (false)  // TODO: Check if serialized tree code has 'NI_KEY_PARAMS'
  {
    int size = 0;
    requestInfo |= DABits::PI_KEY_PARAMS;
    Uint32Slice keyParam(optional, optPos);

    assert (getQuery().getParam(0) != NULL);
    // FIXME: Add parameters here, unsure about the serialized format yet

    optPos += size;
  }

  if (true)
  {
    requestInfo |= DABits::PI_ATTR_LIST;

    // TODO: Fix this such that we get desired projection and not just all fields.
    const int error = m_userProjection.serialize(Uint32Slice(optional, optPos));
    if(error!=0){
      return error;
    }
  }
  lookupParams.get(POS_IN_PARAM(requestInfo)) = requestInfo;

  // TODO: Implement for scans as well.
  QueryNodeParameters::setOpLen(lookupParams.get(POS_IN_PARAM(len)),
				isScan 
                                  ?QueryNodeParameters::QN_SCAN_FRAG
                                  :QueryNodeParameters::QN_LOOKUP,
				lookupParams.getSize());

  if(unlikely(lookupParams.isMaxSizeExceeded())){
    return QRY_DEFINITION_TOO_LARGE; // Query definition too large.
  }
#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized params for node " 
	 << m_operationDef.getQueryOperationIx() << " : ";
  for(Uint32 i = 0; i < lookupParams.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", lookupParams.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
  return 0;
}


void NdbQueryOperationImpl::release(){
  m_receiver.release();
}



bool 
NdbQueryOperationImpl::execTRANSID_AI(const Uint32* ptr, Uint32 len){
  ndbout << "NdbQueryOperationImpl::execTRANSID_AI(): *this="
	 << *this << endl;  
  // Process result values.
  m_receiver.execTRANSID_AI(ptr, len);
  m_pendingResults--;
  /* Receiving this message means that each child has been instantiated 
   * one more. Therefore, increment the pending message count for the children.
   */
  for(Uint32 i = 0; i<getNoOfChildOperations(); i++){
    getChildOperation(i).m_pendingResults++;
    if(getChildOperation(i).m_pendingResults == 0){
      /* This child appears to be complete. Therefore decrement total count
       * of pending operations.*/
     m_queryImpl.incPendingOperations(-1);
    }else if(getChildOperation(i).m_pendingResults == 1){
      /* This child appeared to be complete prior to receiving this message, 
       * but now we know that there will be
       * an extra instance. Therefore, increment total count of pending 
       * operations.*/
      m_queryImpl.incPendingOperations(1);
    }
  }

  if(m_pendingResults == 0){ 
    /* If this operation is complete, check if the query is also complete.*/
    return m_queryImpl.incPendingOperations(-1);
  }else if(m_pendingResults == -1){
    /* This happens because we received results for the child before those
     * of the parent. This operation will be set as complete again when the 
     * TRANSID_AI for the parent arrives.*/
    m_queryImpl.incPendingOperations(1);
  }
  return false;
}

bool 
NdbQueryOperationImpl::execTCKEYREF(){
  ndbout << "NdbQueryOperationImpl::execTCKEYREF(): *this="
	 << *this << endl;
  m_pendingResults--;
  if(m_pendingResults==0){
    /* This operation is complete. Check if the query is also complete.*/
    return m_queryImpl.incPendingOperations(-1);
  }else if(m_pendingResults==-1){
    /* This happens because we received results for the child before those
     * of the parent. This operation will be set as complete again when the 
     * TRANSID_AI for the parent arrives.*/
    m_queryImpl.incPendingOperations(1);
  }
  return false;
}



/** For debugging.*/
NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl& op){
  out << "[ this: " << &op
      << "  m_magic: " << op.m_magic 
      << "  m_id: " << op.m_id;
  for(unsigned int i = 0; i<op.getNoOfParentOperations(); i++){
    out << "  m_parents[" << i << "]" << &op.getParentOperation(i); 
  }
  for(unsigned int i = 0; i<op.getNoOfChildOperations(); i++){
    out << "  m_children[" << i << "]" << &op.getChildOperation(i); 
  }
  out << "  m_queryImpl: " << &op.m_queryImpl;
  out << "  m_pendingResults: " << op.m_pendingResults;
  
  out << " ]";
  return out;
}
 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;

