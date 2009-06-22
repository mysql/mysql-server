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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbQueryOperationImpl_H
#define NdbQueryOperationImpl_H

#include "NdbQueryOperation.hpp"
#include "NdbQueryBuilder.hpp"
#include "NdbImpl.hpp"
#include "NdbError.hpp"
#include <ObjectMap.hpp>
#include "NdbTransaction.hpp"
#include <NdbOut.hpp>

class NdbQueryImpl : public NdbQuery {
private:
  // Only constructable from factory ::buildQuery();
  explicit NdbQueryImpl(NdbTransaction& trans, 
			const NdbQueryDef& queryDef);

  ~NdbQueryImpl();
public:
  STATIC_CONST (MAGIC = 0xdeadface);

  // Factory method which instantiate a query from its definition
  static NdbQuery*
  buildQuery(NdbTransaction& trans, const NdbQueryDef& queryDef);

  ///////////////////////////////////////////////////
  // START: Temp hacks for Jans result set coding 
  explicit NdbQueryImpl(NdbTransaction& trans);  // TEMP, will be removed

  static NdbQuery*
  buildQuery(NdbTransaction& trans);

  /** Set an NdbQueryOperation to be the root of a linked operation */
  // LATER: root and *all* NdbQueryOperations will be constructed 
  // together with NdbQuery itself in :.buildQuery()
  void setRootOperation(NdbQueryOperation* root) {m_rootOperation = root;};
  //// END: TEMP hacks
  //////////////////////////////////////////////////
  
  // get NdbQueryOperation being the root of a linked operation
  NdbQueryOperation* getRootOperation() const;

  Uint32 getNoOfOperations() const;

  // Get a specific NdbQueryOperation by ident specified
  // when the NdbQueryOperationDef was created.
  NdbQueryOperation* getQueryOperation(const char* ident) const;
  NdbQueryOperation* getQueryOperation(Uint32 ident) const;
//NdbQueryOperation* getQueryOperation(const NdbQueryOperationDef* def) const;

  Uint32 getNoOfParameters() const;
  const NdbParamOperand* getParameter(const char* name) const;
  const NdbParamOperand* getParameter(Uint32 num) const;

  int nextResult(bool fetchAllowed, bool forceSend);

  void close(bool forceSend, bool release);

  NdbTransaction* getNdbTransaction() const;

  const NdbError& getNdbError() const;

  bool countTCKEYCONF(){
    ndbout << "countTCKEYCONF() m_missingConfRefs=" << m_missingConfRefs 
	   << " m_missingTransidAIs=" << m_missingTransidAIs << endl;
    m_missingConfRefs--;
    m_missingTransidAIs++;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  bool countTCKEYREF(){
    ndbout << "countTCKEYREF() m_missingConfRefs=" << m_missingConfRefs 
	   << " m_missingTransidAIs=" << m_missingTransidAIs << endl;
    m_missingConfRefs--;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  bool countTRANSID_AI(){
    ndbout << "countTRANSID_AI() m_missingConfRefs=" << m_missingConfRefs 
	   << " m_missingTransidAIs=" << m_missingTransidAIs << endl;
    m_missingTransidAIs--;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  void prepareSend() const;

  void release() const;

  bool checkMagicNumber() const { return m_magic == MAGIC;}
  Uint32 ptr2int() const {return m_id;}
  
private:
  const Uint32 m_magic;
  const Uint32 m_id;
  NdbError m_error;
  NdbTransaction& m_transaction;
  NdbQueryOperation* m_rootOperation;
  /** Each operation should yiedl either a TCKEYCONF or a TCKEYREF message.
   This is the no of such messages pending.*/
  int m_missingConfRefs;
  /** We should receive the same number of  TCKEYCONF and TRANSID_AI messages.
   This is the (possibly negative) no of such messages pending.*/
  int m_missingTransidAIs;
}; // class QueryImpl

/** This class contains data members for NdbQueryOperation, such that these
 * do not need to exposed in NdbQueryOperation.hpp. This class may be 
 * changed without forcing the curstomer to recompile his application.*/
class NdbQueryOperationImpl : public NdbQueryOperation {
  friend class NdbQueryImpl;
public:
  STATIC_CONST (MAGIC = 0xfade1234);


  NdbQueryOperation* getRootOperation() const;

  Uint32 getNoOfParentOperations() const;
  NdbQueryOperation* getParentOperation(Uint32 i) const;

  Uint32 getNoOfChildOperations() const;
  NdbQueryOperation* getChildOperation(Uint32 i) const;

  const NdbQueryOperationDef* getQueryOperationDef() const;

  // Get the entire query object which this operation is part of
  NdbQuery& getQuery() const;

  NdbRecAttr* getValue(const char* anAttrName, char* aValue);
  NdbRecAttr* getValue(Uint32 anAttrId, char* aValue);
  NdbRecAttr* getValue(const NdbDictionary::Column*, char* aValue);

  int setResultRowBuf (const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask);

  int setResultRowRef (const NdbRecord* rec,
                       char* & bufRef,
                       const unsigned char* result_mask);

  bool isRowNULL() const;    // Row associated with Operation is NULL value?

  bool isRowChanged() const; // Prev ::nextResult() on NdbQuery retrived a new
                             // value for this NdbQueryOperation

  /** Returns an I-value for the NdbReceiver object that shall receive results
   * for this operation. 
   * @return The I-value.
   */
  Uint32 getResultPtr() const {
    return m_receiver.getId();
  };

  void prepareSend(){
    m_receiver.prepareSend();
  }
  
  void release(){
    m_receiver.release();
  }

  Uint32 getExpectedResultLength() const{
    return m_receiver.m_expected_result_length;
  }

  //////////////////////////////////////////
  // START Jans temp hack for result prototype
  static NdbQueryOperationImpl*
  buildQueryOperation(NdbQueryImpl& queryImpl, class NdbOperation& operation);

  // To become obsolete as NdbQueryImpl::buildQuery() should
  // construct all QueryOperations
  void setFirstChild(NdbQueryOperation* child){
    m_child = child;
  }
  // End: Hack
  //////////////////////////////////////////////


  bool execTRANSID_AI(const Uint32* ptr, Uint32 len);
  
  Uint32 ptr2int() const {
    return m_id;
  }
  
  bool checkMagicNumber() const { 
    return m_magic == MAGIC;
  }

private:
  const Uint32 m_magic;
  const Uint32 m_id;
  /** First child of this operation.*/
  NdbQueryOperation* m_child;
  /** For processing results from this operation.*/
  NdbReceiver m_receiver;
  /** NdbQuery to which this operation belongs. */
  NdbQueryImpl& m_queryImpl;
  /** TODO:Only used for result processing prototype purposes. To be removed.*/
  NdbOperation& m_operation;
  explicit NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
				 const NdbQueryOperationDef* def);
  explicit NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
				 NdbOperation& operation);

  ~NdbQueryOperationImpl(){
    if (m_id != NdbObjectIdMap::InvalidId) {
      m_queryImpl.getNdbTransaction()->getNdb()->theImpl
	->theNdbObjectIdMap.unmap(m_id, this);
    }
  }

}; // class NdbQueryOperationImpl

#endif
