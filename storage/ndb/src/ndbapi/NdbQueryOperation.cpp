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

#include "NdbQueryOperation.hpp"

#include <NdbQueryOperationImpl.hpp>
#include <NdbDictionaryImpl.hpp>

NdbQuery::NdbQuery(Ndb& ndb):
  m_pimpl(new NdbQueryImpl(ndb)){
}

NdbQueryOperation* NdbQuery::getRootOperation() const{
  return m_pimpl->m_rootOperation;
}

NdbQueryOperation::NdbQueryOperation(NdbQuery& query, NdbOperation& operation)
  :m_pimpl(new NdbQueryOperationImpl(query, operation)){
}


NdbRecAttr * NdbQueryOperation::getValue(const NdbDictionary::Column* column, 
			    char* aValue){
  /* This code will only work for the lookup example in test_spj.cpp.
   */
  assert(aValue==NULL);
  /*if(getQuery().getRootOperation()==this){
    m_pimpl->m_operation.getValue(column);
    }*/
  return m_pimpl->m_receiver.getValue(&NdbColumnImpl::getImpl(*column), aValue);
}

NdbQueryOperation* NdbQueryOperation::getChildOperation(Uint32 i) const{
  assert(i==0);
  return getImpl().m_child;
}


NdbQuery& NdbQueryOperation::getQuery(){
  return m_pimpl->m_query;
};

const NdbQuery& NdbQueryOperation::getQuery() const{
  return m_pimpl->m_query;
};


void NdbQueryImpl::prepareSend() const{
  // TODO: Fix for cases with siblings.
  NdbQueryOperation* curr = m_rootOperation;
  while(curr!=NULL){
    curr->getImpl().prepareSend();
    curr = curr->getChildOperation(0);
  }
}

void NdbQueryImpl::release() const{
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

bool NdbQueryOperationImpl::execTRANSID_AI(const Uint32* ptr, Uint32 len){
  m_receiver.execTRANSID_AI(ptr, len);
  return m_query.receivedTRANSID_AI();
}
