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


NdbQuery::NdbQuery(NdbQueryImpl *pimpl):
  m_pimpl(pimpl)
{}

NdbQuery::~NdbQuery()
{}


//static
NdbQuery*
NdbQuery::buildQuery(NdbTransaction& trans, const NdbQueryDef& queryDef)
{ return NdbQueryImpl::buildQuery(trans, queryDef);
}

NdbQuery*
NdbQuery::buildQuery(NdbTransaction& trans)  // TEMP hack to be removed
{ return NdbQueryImpl::buildQuery(trans);
}

Uint32
NdbQuery::getNoOfOperations() const
{
  return m_pimpl->getNoOfOperations();
}

NdbQueryOperation*
NdbQuery::getQueryOperation(const char* ident) const
{
  return m_pimpl->getQueryOperation(ident);
}

NdbQueryOperation*
NdbQuery::getQueryOperation(Uint32 index) const
{
  return m_pimpl->getQueryOperation(index);
}

Uint32
NdbQuery::getNoOfParameters() const
{
  return m_pimpl->getNoOfParameters();
}

const NdbParamOperand*
NdbQuery::getParameter(const char* name) const
{
  return m_pimpl->getParameter(name);
}

const NdbParamOperand*
NdbQuery::getParameter(Uint32 num) const
{
  return m_pimpl->getParameter(num);
}

int
NdbQuery::nextResult(bool fetchAllowed, bool forceSend)
{
  return m_pimpl->nextResult(fetchAllowed, forceSend);
}

void
NdbQuery::close(bool forceSend, bool release)
{
  m_pimpl->close(forceSend,release);
}

NdbTransaction*
NdbQuery::getNdbTransaction() const
{
  return m_pimpl->getNdbTransaction();
}

const NdbError& 
NdbQuery::getNdbError() const {
  return m_pimpl->getNdbError();
};

NdbQueryOperation::NdbQueryOperation(NdbQueryOperationImpl *pimpl)
  :m_pimpl(pimpl)
{}
NdbQueryOperation::~NdbQueryOperation()
{}

// TODO: Remove this factory. Needed for result prototype only.
// Temp factory for Jan - will be replaced later
//static
NdbQueryOperation*
NdbQueryOperation::buildQueryOperation(NdbQueryImpl& queryImpl,
                                       class NdbOperation& operation)
{
  return NdbQueryOperationImpl::buildQueryOperation(queryImpl, operation);
}

Uint32
NdbQueryOperation::getNoOfParentOperations() const
{
  return m_pimpl->getNoOfParentOperations();
}

NdbQueryOperation*
NdbQueryOperation::getParentOperation(Uint32 i) const
{
  return m_pimpl->getParentOperation(i);
}
Uint32 
NdbQueryOperation::getNoOfChildOperations() const
{
  return m_pimpl->getNoOfChildOperations();
}

NdbQueryOperation* 
NdbQueryOperation::getChildOperation(Uint32 i) const
{
  return m_pimpl->getChildOperation(i);
}

const NdbQueryOperationDef* 
NdbQueryOperation::getQueryOperationDef() const
{
  return m_pimpl->getQueryOperationDef();
}

NdbQuery& 
NdbQueryOperation::getQuery() const {
  return m_pimpl->getQuery();
};

NdbRecAttr*
NdbQueryOperation::getValue(const char* anAttrName,
			    char* aValue)
{
  return m_pimpl->getValue(anAttrName, aValue);
}

NdbRecAttr*
NdbQueryOperation::getValue(Uint32 anAttrId, 
			    char* aValue)
{
  return m_pimpl->getValue(anAttrId, aValue);
}

NdbRecAttr*
NdbQueryOperation::getValue(const NdbDictionary::Column* column, 
			    char* aValue)
{
  return m_pimpl->getValue(column, aValue);
}

int
NdbQueryOperation::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  return m_pimpl->setResultRowBuf(rec, resBuffer, result_mask);
}

int
NdbQueryOperation::setResultRowRef (
                       const NdbRecord* rec,
                       char* & bufRef,
                       const unsigned char* result_mask)
{
  return m_pimpl->setResultRowRef(rec, bufRef, result_mask);
}

bool
NdbQueryOperation::isRowNULL() const
{
  return m_pimpl->isRowNULL();
}

bool
NdbQueryOperation::isRowChanged() const
{
  return m_pimpl->isRowChanged();
}


///////////////////////////////////////////
/////////  NdbQueryImpl methods ///////////
///////////////////////////////////////////
NdbQueryImpl::NdbQueryImpl(NdbTransaction& trans):
  NdbQuery(this),
  m_magic(MAGIC),
  m_id(trans.getNdb()->theImpl->theNdbObjectIdMap.map(this)),
  m_error(),
  m_transaction(trans),
  m_operations(),
  m_tcKeyConfReceived(false),
  m_pendingOperations(0)
{
  assert(m_id != NdbObjectIdMap::InvalidId);
}

NdbQueryImpl::NdbQueryImpl(NdbTransaction& trans, const NdbQueryDef& queryDef):
  NdbQuery(this),
  m_magic(MAGIC),
  m_id(trans.getNdb()->theImpl->theNdbObjectIdMap.map(this)),
  m_error(),
  m_transaction(trans),
  m_operations(),
  m_tcKeyConfReceived(false),
  m_pendingOperations(0)
{
  assert(m_id != NdbObjectIdMap::InvalidId);

  for (Uint32 i=0; i<queryDef.getNoOfOperations(); ++i)
  {
    const NdbQueryOperationDef* def = queryDef.getQueryOperation(i);
    assert(def!=NULL);

    NdbQueryOperationImpl* op = new NdbQueryOperationImpl(*this, def);

    // Fill in operations parent refs, and append it as child of its parents
    for (Uint32 p=0; p<def->getNoOfParentOperations(); ++p)
    { 
      const NdbQueryOperationDef* parent = def->getParentOperation(p);
      Uint32 ix = parent->getImpl().getQueryOperationIx();
      assert (ix < m_operations.size());
      op->m_parents.push_back(m_operations[ix]);
      m_operations[ix]->m_children.push_back(op);
    }

    m_operations.push_back(op);
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
NdbQuery*
NdbQueryImpl::buildQuery(NdbTransaction& trans, const NdbQueryDef& queryDef)
{
  return new NdbQueryImpl(trans, queryDef);
}

NdbQuery*
NdbQueryImpl::buildQuery(NdbTransaction& trans)  // TEMP hack to be removed
{
  return new NdbQueryImpl(trans);
}

Uint32
NdbQueryImpl::getNoOfOperations() const
{
  return m_operations.size();
}

NdbQueryOperation*
NdbQueryImpl::getQueryOperation(const char* ident) const
{
  return NULL; // FIXME
}

NdbQueryOperation*
NdbQueryImpl::getQueryOperation(Uint32 index) const
{
  return m_operations[index];
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
  return &m_transaction;  // FIXME
}

void 
NdbQueryImpl::prepareSend(){
  m_pendingOperations = m_operations.size();
  for(Uint32 i = 0; i < m_operations.size(); i++){
      m_operations[i]->prepareSend();
  }
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
           const NdbQueryOperationDef* def):
  NdbQueryOperation(this),
  m_magic(MAGIC),
  m_id(queryImpl.getNdbTransaction()->getNdb()->theImpl
       ->theNdbObjectIdMap.map(this)),
  m_parents(def->getNoOfParentOperations()),
  m_children(def->getNoOfChildOperations()),
  m_receiver(queryImpl.getNdbTransaction()->getNdb()),
  m_queryImpl(queryImpl),
  m_state(State_Initial),
  m_operation(*((NdbOperation*)NULL))
{ 
  assert(m_id != NdbObjectIdMap::InvalidId);
  m_receiver.init(NdbReceiver::NDB_OPERATION, false, &m_operation);
}

///////////////////////////////////////////////////////////
// START: Temp code for Jan until we have a propper NdbQueryOperationDef

/** Only used for result processing prototype purposes. To be removed.*/
NdbQueryOperationImpl::NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
					     NdbOperation& operation):
  NdbQueryOperation(this),
  m_magic(MAGIC),
  m_id(queryImpl.getNdbTransaction()->getNdb()->theImpl
       ->theNdbObjectIdMap.map(this)),
  m_parents(),
  m_children(),
  m_receiver(queryImpl.getNdbTransaction()->getNdb()),
  m_queryImpl(queryImpl),
  m_state(State_Initial),
  m_operation(operation)
{ 
  assert(m_id != NdbObjectIdMap::InvalidId);
  m_receiver.init(NdbReceiver::NDB_OPERATION, false, &operation);
  queryImpl.addQueryOperation(this);
}

// Temp factory for Jan - will be removed later
// static
NdbQueryOperationImpl*
NdbQueryOperationImpl::buildQueryOperation(NdbQueryImpl& queryImpl,
                                       class NdbOperation& operation)
{
  NdbQueryOperationImpl* op = new NdbQueryOperationImpl(queryImpl, operation);
  return op;
}
// END temp code
//////////////////////////////////////////////////////////

Uint32
NdbQueryOperationImpl::getNoOfParentOperations() const
{
  return m_parents.size();
}

NdbQueryOperation*
NdbQueryOperationImpl::getParentOperation(Uint32 i) const
{
  return m_parents[i];
}

Uint32 
NdbQueryOperationImpl::getNoOfChildOperations() const
{
  return m_children.size();
}

NdbQueryOperation* 
NdbQueryOperationImpl::getChildOperation(Uint32 i) const
{
  return m_children[i];
}

const NdbQueryOperationDef* 
NdbQueryOperationImpl::getQueryOperationDef() const
{
  return NULL;  // FIXME
}

NdbQuery& 
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

void NdbQueryOperationImpl::prepareSend(){
  m_receiver.prepareSend();
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
 
