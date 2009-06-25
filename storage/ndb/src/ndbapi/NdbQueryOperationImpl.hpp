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
  void setRootOperation(NdbQueryOperation* root) {
    m_rootOperation = &root->getImpl();};
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

  /** Process TCKEYCONF message. Return true if query is complete.*/
  bool execTCKEYCONF(){
    ndbout << "NdbQueryImpl::execTCKEYCONF() isComplete()= " 
	   << isComplete() << endl;
    m_tcKeyConfReceived = true;
    return isComplete();
  }

  /** Check if all expected signals have been received.*/
  bool isComplete();

  void prepareSend();

  void release();

  bool checkMagicNumber() const { return m_magic == MAGIC;}
  Uint32 ptr2int() const {return m_id;}
  
private:
  const Uint32 m_magic;
  const Uint32 m_id;
  NdbError m_error;
  NdbTransaction& m_transaction;
  NdbQueryOperationImpl* m_rootOperation;
  bool m_tcKeyConfReceived; 
  /** Each operation should yiedl either a TCKEYCONF or a TCKEYREF message.
   This is the no of such messages pending.*/
  //int m_missingConfRefs;
  /** We should receive the same number of  TCKEYCONF and TRANSID_AI messages.
   This is the (possibly negative) no of such messages pending.*/
  //int m_missingTransidAIs;
}; // class NdbQueryImpl


// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;

/** This class contains data members for NdbQueryOperation, such that these
 * do not need to exposed in NdbQueryOperation.hpp. This class may be 
 * changed without forcing the curstomer to recompile his application.*/
class NdbQueryOperationImpl : public NdbQueryOperation {
  friend class NdbQueryImpl;
  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl&);
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
  /*Uint32 getResultPtr() const {
    return m_receiver.getId();
    };*/

  Uint32 getExpectedResultLength() const{
    return m_receiver.m_expected_result_length;
  }

  //////////////////////////////////////////
  // START Jans temp hack for result prototype
  static NdbQueryOperationImpl*
  buildQueryOperation(NdbQueryImpl& queryImpl, class NdbOperation& operation);

  // To become obsolete as NdbQueryImpl::buildQuery() should
  // construct all QueryOperations
  void addChild(NdbQueryOperation& child){
    m_children.push_back(&child.getImpl());
    child.getImpl().m_parents.push_back(this);
  }
  // End: Hack
  //////////////////////////////////////////////

  /** Process result data for this operation. Return true if query complete.*/
  bool execTRANSID_AI(const Uint32* ptr, Uint32 len);
  
  /** Process absence of result data for this operation. 
   * Return true if query complete.*/
  bool execTCKEYREF();

  /** Check if this operation and all descendants are complete.*/
  bool isComplete(){
    return m_state==State_Complete;
  }

  void prepareSend();

  void release();

  /** Return I-value for putting in objetc map.*/
  Uint32 ptr2int() const {
    return m_id;
  }
  
  /** Verify magic number.*/
  bool checkMagicNumber() const { 
    return m_magic == MAGIC;
  }

private:
  /** State of the operation (in terms of pending messages.) */
  enum State{
    /** Awaiting TCKEYREF or TRANSID_AI for this operation.*/
    State_Initial,
    /** This operation is done, but its children are not.*/
    State_WaitForChildren,
    /** Operation and all children are done.*/
    State_Complete
  };
  /** For verifying pointers to this class.*/
  const Uint32 m_magic;
  /** I-value for object maps.*/
  const Uint32 m_id;
  /** Parents of this operation.*/
  Vector<NdbQueryOperationImpl*> m_parents;
  /** Children of this operation.*/
  Vector<NdbQueryOperationImpl*> m_children;
  /** For processing results from this operation.*/
  NdbReceiver m_receiver;
  /** NdbQuery to which this operation belongs. */
  NdbQueryImpl& m_queryImpl;
  /** Progress of operation.*/
  State m_state;
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

  /** A child operation is complete. 
   * Set this as complete if all children are complete.*/
  void handleCompletedChild();

}; // class NdbQueryOperationImpl



#endif
