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
  m_tcKeyConfReceived(false),
  m_pendingOperations(0),
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

int
NdbQueryImpl::prepareSend(){
  const Uint32Buffer& serializedDef = m_queryDef.getSerialized();
  m_pendingOperations = m_operations.size();  
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
  m_state(State_Initial)
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
  /*if(getQuery().getQueryOperation(0)==this){
    m_operation->getValue(column);
    }*/
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

#define POS_IN_PARAM(field) \
(offsetof(QueryNodeParameters, field)/sizeof(Uint32))

#define POS_IN_LOOKUP_PARAM(field) \
(offsetof(QN_LookupParameters, field)/sizeof(Uint32)) 


int NdbQueryOperationImpl::prepareSend(Uint32Buffer& serializedParams)
{
  NdbQueryOperationDefImpl::Type opType = getQueryOperationDef().getType();
  bool isScan = (opType == NdbQueryOperationDefImpl::TableScan ||
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
    Uint32Slice attrList(optional, optPos);
    attrList.get(0) = 1; // Length of user projection
    AttributeHeader::init(&attrList.get(1), 
			  AttributeHeader::READ_ALL,
			  m_operationDef.getTable().getNoOfColumns());
    optPos += 2;
  }
  lookupParams.get(POS_IN_PARAM(requestInfo)) = requestInfo;

  // TODO: Implement for scans as well.
  QueryNodeParameters::setOpLen(lookupParams.get(POS_IN_PARAM(len)),
				isScan 
                                  ?QueryNodeParameters::QN_SCAN_FRAG
                                  :QueryNodeParameters::QN_LOOKUP,
				lookupParams.getSize());

  if(unlikely(lookupParams.isMaxSizeExceeded())){
    return 4808; // Query definition too large.
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
  if(m_state!=State_Initial){
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI(): unexpected state "
	   << *this << endl;
    assert(false);
    return false;
  }
  m_receiver.execTRANSID_AI(ptr, len);
  bool done = true;
  unsigned int i = 0;
  while(done && i<m_children.size()){
    done = m_children[i]->m_state == State_Complete;
    i++;
  }
  if(done){
    m_state = State_Complete;
    for(unsigned int j = 0; j < m_parents.size(); j++){
      m_parents[j]->handleCompletedChild();
    }
    return m_queryImpl.countCompletedOperation();
  }else{
    m_state = State_WaitForChildren;
    return false;
  }
}

bool 
NdbQueryOperationImpl::execTCKEYREF(){
  ndbout << "NdbQueryOperationImpl::execTCKEYREF(): *this="
	 << *this << endl;  
  if(m_state!=State_Initial){
    ndbout << "NdbQueryOperationImpl::execTCKEYREF(): unexpected state "
	   << *this << endl;
    return false;
  }
  m_state = State_Complete;
  for(unsigned int i = 0; i < m_parents.size(); i++){
    m_parents[i]->handleCompletedChild();
  }
  return m_queryImpl.countCompletedOperation();
}

void
NdbQueryOperationImpl::handleCompletedChild(){
  switch(m_state){
  case State_Initial:
    break;
  case State_WaitForChildren:
    {
      bool done = true;
      unsigned int i = 0;
      while(done && i<m_children.size()){
	done = m_children[i]->m_state == State_Complete;
	i++;
      }
      if(done){
	m_state = State_Complete;
	for(unsigned int j = 0; j < m_parents.size(); j++){
	  m_parents[j]->handleCompletedChild();
	}
	m_queryImpl.countCompletedOperation();
      }
    }
    break;
  default:
    ndbout << "NdbQueryOperationImpl::handleCompletedChild(): unexpected state "
	   << *this << endl;
    assert(false);
  }
}

/** For debugging.*/
NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl& op){
  out << "[ this: " << &op
      << "  m_magic: " << op.m_magic 
      << "  m_id: " << op.m_id;
  for(unsigned int i = 0; i<op.m_parents.size(); i++){
    out << "  m_parents[" << i << "]" << op.m_parents[i]; 
  }
  for(unsigned int i = 0; i<op.m_children.size(); i++){
    out << "  m_children[" << i << "]" << op.m_children[i]; 
  }
  out << "  m_queryImpl: " << &op.m_queryImpl
      << "  m_state: ";
  switch(op.m_state){
  case NdbQueryOperationImpl::State_Initial: 
    out << "State_Initial";
    break;
  case NdbQueryOperationImpl::State_WaitForChildren: 
    out << "State_WaitForChildren";
    break;
  case NdbQueryOperationImpl::State_Complete: 
    out << "State_Complete";
    break;
  default:
    out << "<Illegal enum>" << op.m_state;
  }
  out << " ]";
  return out;
}
 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;

