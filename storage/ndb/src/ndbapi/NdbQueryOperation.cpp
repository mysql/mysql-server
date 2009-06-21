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

#include <ndb_global.h>

#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"
#include "NdbQueryOperationImpl.hpp"
#include "NdbDictionaryImpl.hpp"


NdbQuery::NdbQuery(NdbQueryImpl *pimpl):
  m_pimpl(pimpl)
{}

NdbQuery::~NdbQuery()
{}


//static
NdbQuery*
NdbQuery::buildQuery(Ndb& ndb, const NdbQueryDef& queryDef)
{ return NdbQueryImpl::buildQuery(ndb, queryDef);
}

NdbQuery*
NdbQuery::buildQuery(Ndb& ndb)  // TEMP hack to be removed
{ return NdbQueryImpl::buildQuery(ndb);
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
NdbQuery::getQueryOperation(Uint32 ident) const
{
  return m_pimpl->getQueryOperation(ident);
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
NdbQueryOperation::buildQueryOperation(NdbQuery& query,
                                       class NdbOperation& operation)
{
  return NdbQueryOperationImpl::buildQueryOperation(query,operation);
}

NdbQueryOperation* NdbQuery::getRootOperation() const
{
  return m_pimpl->getRootOperation();
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
NdbQueryImpl::NdbQueryImpl(Ndb& ndb):
  NdbQuery(this),
  m_magic(MAGIC),
  m_ndb(ndb),
  m_error(),
  m_rootOperation(NULL),
  m_id(ndb.theImpl->theNdbObjectIdMap.map(this)),
  m_msgCount(0)
{
  assert(m_id != NdbObjectIdMap::InvalidId);
}

NdbQueryImpl::NdbQueryImpl(Ndb& ndb, const NdbQueryDef& queryDef):
  NdbQuery(this),
  m_magic(MAGIC),
  m_ndb(ndb),
  m_error(),
  m_rootOperation(NULL),
  m_id(ndb.theImpl->theNdbObjectIdMap.map(this)),
  m_msgCount(0)
{
  assert(m_id != NdbObjectIdMap::InvalidId);

  const NdbQueryOperationDef* def = queryDef.getRootOperation();
  if (def != NULL)
  {
    NdbQueryOperationImpl* op = new NdbQueryOperationImpl(*this, def);
    m_rootOperation = op;

    // TODO: Instantiate NdbQueryOperationImpl's for
    // entire operation three.
  }
}

NdbQueryImpl::~NdbQueryImpl()
{
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb.theImpl->theNdbObjectIdMap.unmap(m_id, this);
  }
}

//static
NdbQuery*
NdbQueryImpl::buildQuery(Ndb& ndb, const NdbQueryDef& queryDef)
{
  return new NdbQueryImpl(ndb, queryDef);
}

NdbQuery*
NdbQueryImpl::buildQuery(Ndb& ndb)  // TEMP hack to be removed
{
  return new NdbQueryImpl(ndb);
}


NdbQueryOperation*
NdbQueryImpl::getRootOperation() const
{
  return m_rootOperation;
}

Uint32
NdbQueryImpl::getNoOfOperations() const
{
  return 0;  // FIXME
}

NdbQueryOperation*
NdbQueryImpl::getQueryOperation(const char* ident) const
{
  return NULL; // FIXME
}

NdbQueryOperation*
NdbQueryImpl::getQueryOperation(Uint32 ident) const
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
  return NULL;  // FIXME
}

void 
NdbQueryImpl::prepareSend() const{
  // TODO: Fix for cases with siblings.
  NdbQueryOperation* curr = m_rootOperation;
  while(curr!=NULL){
    curr->getImpl().prepareSend();
    curr = curr->getChildOperation(0);
  }
}

void 
NdbQueryImpl::release() const{
  // TODO: Fix for cases with siblings.
  NdbQueryOperation* curr = m_rootOperation;
  while(curr!=NULL){
    curr->getImpl().release();
    curr = curr->getChildOperation(0);
  }
}

bool NdbQueryImpl::execTCOPCONF(Uint32 len) const{
  // TODO: Fix for cases with siblings.
  Uint32 expectedLen = 0;
  NdbQueryOperation* curr = m_rootOperation;
  while(curr!=NULL){
    expectedLen += curr->getImpl().getExpectedResultLength();
    curr = curr->getChildOperation(0);
  }
  //assert(len<=expectedLen);
  ndbout << "NdbQueryImpl::execTCOPCONF() len=" << len
	 << " exepectedLen =" << expectedLen << endl;
  return expectedLen == len;
}

////////////////////////////////////////////////////
/////////  NdbQueryOperationImpl methods ///////////
////////////////////////////////////////////////////

NdbQueryOperationImpl::NdbQueryOperationImpl(
           NdbQuery& query,
           const NdbQueryOperationDef* def):
  NdbQueryOperation(this),
  m_child(NULL),
  m_receiver(&query.getImpl().getNdb()),
  m_query(query)
{ 
  m_receiver.init(NdbReceiver::NDB_OPERATION, false, NULL);
}

///////////////////////////////////////////////////////////
// START: Temp code for Jan until we have a propper NdbQueryOperationDef

/** Only used for result processing prototype purposes. To be removed.*/
NdbQueryOperationImpl::NdbQueryOperationImpl(
           NdbQuery& query,
           class NdbOperation& operation):
  NdbQueryOperation(this),
  m_child(NULL),
  m_receiver(&query.getImpl().getNdb()),
  m_query(query),
  m_operation(&operation)
{ 
  m_receiver.init(NdbReceiver::NDB_OPERATION, false, &operation);
}

// Temp factory for Jan - will be removed later
// static
NdbQueryOperationImpl*
NdbQueryOperationImpl::buildQueryOperation(NdbQuery& query,
                                       class NdbOperation& operation)
{
  NdbQueryOperationImpl* op = new NdbQueryOperationImpl(query, operation);
  return op;
}
// END temp code
//////////////////////////////////////////////////////////

NdbQueryOperation*
NdbQueryOperationImpl::getRootOperation() const
{
  return m_query.getRootOperation();
}

Uint32
NdbQueryOperationImpl::getNoOfParentOperations() const
{
  return 0;  // FIXME
}

NdbQueryOperation*
NdbQueryOperationImpl::getParentOperation(Uint32 i) const
{
  return NULL;  // FIXME
}
Uint32 
NdbQueryOperationImpl::getNoOfChildOperations() const
{
  return 0;  // FIXME
}

NdbQueryOperation* 
NdbQueryOperationImpl::getChildOperation(Uint32 i) const
{
  assert(i==0);
  return m_child;
}

const NdbQueryOperationDef* 
NdbQueryOperationImpl::getQueryOperationDef() const
{
  return NULL;  // FIXME
}

NdbQuery& 
NdbQueryOperationImpl::getQuery() const
{
  return m_query;
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
  /*if(getQuery().getRootOperation()==this){
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
  return 0; // FIXME
}

int
NdbQueryOperationImpl::setResultRowRef (
                       const NdbRecord* rec,
                       char* & bufRef,
                       const unsigned char* result_mask)
{
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

