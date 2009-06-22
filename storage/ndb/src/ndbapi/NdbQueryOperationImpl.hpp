/* Copyright (C) 2009 Sun Microsystems Inc

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

#include <NdbQueryOperation.hpp>
#include <ObjectMap.hpp>
#include <NdbImpl.hpp>

class NdbQueryImpl{
  friend class NdbQuery;
public:
  STATIC_CONST (MAGIC = 0xdeadface);
  explicit NdbQueryImpl(NdbTransaction& trans);

  ~NdbQueryImpl(){
    if (m_id != NdbObjectIdMap::InvalidId) {
      m_transaction.getNdb()->theImpl->theNdbObjectIdMap.unmap(m_id, this);
    }
  }

  /** Set an NdbQueryOperation to be the root of a linked operation */
  void setRootOperation(NdbQueryOperation* root) {m_rootOperation = root;};
  
  void prepareSend() const;
  void release() const;
  NdbTransaction& getTransaction() const {return m_transaction;}

  bool countTCKEYCONF(){
    m_missingConfRefs--;
    m_missingTransidAIs++;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  bool countTCKEYREF(){
    m_missingConfRefs--;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  bool countTRANSID_AI(){
    m_missingTransidAIs--;
    return m_missingTransidAIs==0 && m_missingConfRefs == 0;
  }

  bool checkMagicNumber() const { return m_magic == MAGIC;}
  Uint32 ptr2int() const {return m_id;}
  bool allRepliesReceived(){
    return ++m_msgCount == 2;
  }
  
private:
  const Uint32 m_magic;
  const Uint32 m_id;
  NdbTransaction& m_transaction;
  NdbQueryOperation* m_rootOperation;
  /** Each operation should yiedl either a TCKEYCONF or a TCKEYREF message.
   This is the no of such messages pending.*/
  int m_missingConfRefs;
  /** We should receive the same number of  TCKEYCONF and TRANSID_AI messages.
   This is the (possibly negative) no of such messages pending.*/
  int m_missingTransidAIs;
};

/** This class contains data members for NdbQueryOperation, such that these
 * do not need to exposed in NdbQueryOperation.hpp. This class may be 
 * changed without forcing the curstomer to recompile his application.*/
class NdbQueryOperationImpl{
  friend class NdbQueryOperation;
public:
  STATIC_CONST (MAGIC = 0xfade1234);

  /** Returns an I-value for the NdbReceiver object that shall receive results
   * for this operation. 
   * @return The I-value.
   */
  Uint32 getResultPtr() const {return m_receiver.getId();};

  void setFirstChild(NdbQueryOperation* child){
    m_child = child;
  }

  void prepareSend(){
    m_receiver.prepareSend();
  }
  
  void release(){
    m_receiver.release();
  }

  Uint32 getExpectedResultLength() const{
    return m_receiver.m_expected_result_length;
  }

  bool execTRANSID_AI(const Uint32* ptr, Uint32 len);
  
  Uint32 ptr2int() const {return m_id;}
  
  bool checkMagicNumber() const { return m_magic == MAGIC;}

private:
  const Uint32 m_magic;
  const Uint32 m_id;
  /** First child of this operation.*/
  NdbQueryOperation* m_child;
  /** For processing results from this operation.*/
  NdbReceiver m_receiver;
  /** NdbQuery to which this operation belongs. */
  NdbQuery& m_query;
  /** TODO:Only used for result processing prototype purposes. To be removed.*/
  NdbOperation& m_operation;
  explicit NdbQueryOperationImpl(NdbQuery& query, NdbOperation& operation):
    m_magic(MAGIC),
    m_id(query.getImpl().getTransaction().getNdb()->theImpl->theNdbObjectIdMap.map(this)),
    m_child(NULL),
    m_receiver(&query.getImpl().getNdb()),
    m_query(query),
    m_operation(operation)
  { 
    assert(m_id != NdbObjectIdMap::InvalidId);
    m_receiver.init(NdbReceiver::NDB_OPERATION, false, &operation, 
		    &query.getImpl().getNdb());
  }

  ~NdbQueryOperationImpl(){
    if (m_id != NdbObjectIdMap::InvalidId) {
      query.getImpl().getTransaction().getNdb()->theImpl->theNdbObjectIdMap.unmap(m_id, this);
    }
  }

};

#endif
