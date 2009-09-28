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
#include "NdbQueryBuilderImpl.hpp"
#include "NdbError.hpp"
#include "ndb_limits.h"
#include "Vector.hpp"
#include "Bitmask.hpp"

// Forward declarations
class NdbApiSignal;
class NdbResultStream;
class NdbParamOperand;
class NdbTransaction;
class NdbReceiver;
class NdbOut;

/** This class is the internal implementation of the interface defined by
 * NdbQuery. This class should thus not be visible to the application 
 * programmer. @see NdbQuery.*/
class NdbQueryImpl {

  /* NdbQueryOperations are allowed to access it containing query */
  friend class NdbQueryOperationImpl;

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const class NdbQueryOperationImpl&);

public:
  /** Factory method which instantiate a query from its definition.
   (There is no public constructor.)*/
  static NdbQueryImpl* buildQuery(NdbTransaction& trans, 
                                  const NdbQueryDefImpl& queryDef, 
                                  NdbQueryImpl* next);
  /** Return number of operations in query.*/
  Uint32 getNoOfOperations() const;

  // Get a specific NdbQueryOperation instance by ident specified
  // when the NdbQueryOperationDef was created.
  NdbQueryOperationImpl& getQueryOperation(Uint32 ident) const;
  NdbQueryOperationImpl* getQueryOperation(const char* ident) const;
  // Consider to introduce these as convenient shortcuts
//NdbQueryOperationDefImpl& getQueryOperationDef(Uint32 ident) const;
//NdbQueryOperationDefImpl* getQueryOperationDef(const char* ident) const;

  /** Return number of parameter operands in query.*/
  Uint32 getNoOfParameters() const;
  const NdbParamOperand* getParameter(const char* name) const;
  const NdbParamOperand* getParameter(Uint32 num) const;

  /** Get the next tuple(s) from the global cursor on the query.
   * @param fetchAllowed If treu, the method may block while waiting for more
   * results to arrive. Otherwise, the method will return immediately if no more
   * results are buffered in the API.
   * @param forceSend FIXME: Describe this this.
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   * @see NdbQueryOperation::nextResult()
   */ 
  NdbQuery::NextResultOutcome nextResult(bool fetchAllowed, bool forceSend);

  /** Close query*/
  void close(bool forceSend, bool release);

  NdbTransaction* getNdbTransaction() const;

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
  }

  /** Set an error code and initiate transaction abort.*/
  void setErrorCodeAbort(int aErrorCode);

  /** Assign supplied parameter values to the parameter placeholders
   *  Created when the query was defined.
   *  Values are *copied* into this NdbQueryImpl object:
   *  Memory location used as source for parameter values don't have
   *  to be valid after this assignment.
   */
  int assignParameters(const constVoidPtr paramValues[]);

  /** Prepare for execution. 
   *  @return possible error code.
   */
  int prepareSend();

  /** Send prepared signals from this NdbQuery
   *  @return possible error code.
   */
  int doSend(int aNodeId, bool lastFlag);

  NdbQuery& getInterface()
  { return m_interface; }

  /** Get next query in same transaction.*/
  NdbQueryImpl* getNext() const
  { return m_next; }

  /** Get the maximal number of rows that may be returned in a single 
   * transaction.*/
  Uint32 getMaxBatchRows() const
  { return m_maxBatchRows; }

  /** Get the (transaction independent) definition of this query.*/
  const NdbQueryDefImpl& getQueryDef() const
  { return m_queryDef; }

  /** Process TCKEYCONF message. Return true if query is complete.*/
  bool execTCKEYCONF();

private:
  /** Possible return values from NdbQuery::fetchMoreResults. Integer values
   * matches those returned from PoolGuard::waitScan().
   */
  enum FetchResult{
    FetchResult_otherError = -4,
    FetchResult_sendFail = -3,
    FetchResult_nodeFail = -2,
    FetchResult_timeOut = -1,
    FetchResult_ok = 0,
    FetchResult_scanComplete = 1
  };

  /** A stack of NdbResultStream pointers. */
  class StreamStack{
  public:
    explicit StreamStack();

    ~StreamStack(){
      delete[] m_array;
    }

    // Prepare internal datastructures.
    // Return 0 if ok, else errorcode
    int prepare(int size);

    NdbResultStream* top() const { 
      return m_current>=0 ? m_array[m_current] : NULL; 
    }

    void pop(){ 
      assert(m_current>=0);
      m_current--;
    }
    
    void push(NdbResultStream& stream);

    void clear(){
      m_current = -1;
    }

  private:
    /** Capacity of stack.*/
    int m_size;
    /** Index of current top of stack.*/
    int m_current;
    NdbResultStream** m_array;
    // No copying.
    StreamStack(const StreamStack&);
    StreamStack& operator=(const StreamStack&);
  }; // class StreamStack

  /** The interface that is visible to the application developer.*/
  NdbQuery m_interface;

  /** Possible error status of this query.*/
  NdbError m_error;
  /** Transaction in which this query instance executes.*/
  NdbTransaction& m_transaction;

  /** The operations constituting this query.*/
  NdbQueryOperationImpl *m_operations;  // 'Array of ' OperationImpls
  size_t m_countOperations;             // #elements in above array

  /** True if a TCKEYCONF message has been received for this query.*/
  bool m_tcKeyConfReceived;
  /** Number of streams not yet completed within the current batch.*/
  Uint32 m_pendingStreams;
  /** Next query in same transaction.*/
  NdbQueryImpl* const m_next;
  /** Definition of this query.*/
  const NdbQueryDefImpl& m_queryDef;

  /** Number of fragments to be scanned in parallel. (1 if root operation is 
   *  a lookup)*/
  Uint32 m_parallelism;

  /** Max rows (per resultStream) in a scan batch.*/
  Uint32 m_maxBatchRows;

  /** Streams that the application is currently iterating over. Only accessed
   *  by application thread.
   */
  StreamStack m_applStreams;

  /** Streams that have received a complete batch. Shared between 
   *  application thread and receiving thread. Access should be mutex protected.
   */
  StreamStack m_fullStreams;

  /** Prunable property, and optional hashValue, valid for scans, */
  bool m_isPruned;
  Uint32 m_hashValue;

  /**
   * Signal building section:
   */
  NdbApiSignal* m_signal;
  Uint32Buffer m_attrInfo;  // ATTRINFO: QueryTree + serialized parameters
  Uint32Buffer m_keyInfo;   // KEYINFO:  Lookup key or scan bounds

  // Only constructable from factory ::buildQuery();
  explicit NdbQueryImpl(
             NdbTransaction& trans,
             const NdbQueryDefImpl& queryDef,
             NdbQueryImpl* next);

  ~NdbQueryImpl();

  /** Get more scan results, ask for the next batch if necessary.*/
  FetchResult fetchMoreResults(bool forceSend);

  /** Close scan receivers used for lookups. (Since scans and lookups should
   * have the same semantics for nextResult(), lookups use scan-type 
   * NdbReceiver objects.)
   */
  void closeSingletonScans();

  /** Fix parent-child references when a complete batch has been received
   * for a given stream.
   */
  void buildChildTupleLinks(Uint32 streamNo);

  const NdbQuery& getInterface() const
  { return m_interface; }

  Uint32 getParallelism() const
  { return m_parallelism; } 

  NdbQueryOperationImpl& getRoot() const 
  { return getQueryOperation(0U); }

  /** Count number of completed streams within this batch. (There should be 
   *  'parallelism' number of streams for each operation.)
   *  @param increment Change in count of  completed operations.
   *  @return True if batch is complete.
   */
  bool incPendingStreams(int increment);

}; // class NdbQueryImpl


/** This class contains data members for NdbQueryOperation, such that these
 *  do not need to exposed in NdbQueryOperation.hpp. This class may be 
 *  changed without forcing the customer to recompile his application.
 */
class NdbQueryOperationImpl {

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl&);

  friend class NdbQueryImpl;

public:
  Uint32 getNoOfParentOperations() const;
  NdbQueryOperationImpl& getParentOperation(Uint32 i) const;

  Uint32 getNoOfChildOperations() const;
  NdbQueryOperationImpl& getChildOperation(Uint32 i) const;

  /** A shorthand for getting the root operation. */
  NdbQueryOperationImpl& getRoot() const
  { return m_queryImpl.getRoot(); }

  const NdbQueryDefImpl& getQueryDef() const
  { return m_queryImpl.getQueryDef(); }

  const NdbQueryOperationDefImpl& getQueryOperationDef() const
  { return m_operationDef; }

  // Get the entire query object which this operation is part of
  NdbQueryImpl& getQuery() const
  { return m_queryImpl; }

  NdbRecAttr* getValue(const char* anAttrName, char* resultBuffer);
  NdbRecAttr* getValue(Uint32 anAttrId, char* resultBuffer);
  NdbRecAttr* getValue(const NdbDictionary::Column*, char* resultBuffer);

  int setResultRowBuf (const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask);

  int setResultRowRef (const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask);

  bool isRowNULL() const;    // Row associated with Operation is NULL value?

  bool isRowChanged() const; // Prev ::nextResult() on NdbQuery retrived a new
                             // value for this NdbQueryOperation

  /** Process result data for this operation. Return true if batch complete.*/
  bool execTRANSID_AI(const Uint32* ptr, Uint32 len);
  
  /** Process absence of result data for this operation. (Only used when the 
   * root operation is a lookup.)
   * @return true if query complete.*/
  bool execTCKEYREF(NdbApiSignal* aSignal);

  /** Called once per complete (within batch) fragment when a SCAN_TABCONF 
   * signal is received.
   * @param tcPtrI not in use.
   * @param rowCount Number of rows for this fragment, including all rows from 
   * descendant lookup operations.
   * @param receiver The receiver object that shall process the results.*/
  void execSCAN_TABCONF(Uint32 tcPtrI, Uint32 rowCount, NdbReceiver* receiver); 

  const NdbQueryOperation& getInterface() const
  { return m_interface; }
  NdbQueryOperation& getInterface()
  { return m_interface; }

private:
  /** NdbRecord and NdbRecAttr may not be combined. Also, results may not 
   * be requested for all operations.*/
  enum ResultStyle{
    Style_None,       // Not set yet.
    Style_NdbRecord,  // Use old style result retrieval.
    Style_NdbRecAttr, // Use NdbRecord.
  };
 
  /** This class represents a projection that shall be sent to the 
   * application.*/
  class UserProjection{
  public:
    explicit UserProjection(const NdbDictionary::Table& tab);

    /** Add a column to the projection.
     * @return Possible error code.*/ 
    int addColumn(const NdbDictionary::Column& col);
    
    /** Make a serialize representation of this object, to be sent to the 
     * SPJ block.
     * @param dst Buffer for storing serialized projection.
     * @param style NdbRecord, NdbRecattr or empty projection.
     * @param withCorrelation Include correlation data in projection.
     * @return Possible error code.*/
    int serialize(Uint32Buffer& buffer, 
                  ResultStyle resultStyle, 
                  bool withCorrelation) const;
    
    /** Get number of columns.*/
    int getColumnCount() const {return m_columnCount;}

  private:
    /** The columns that consitutes the projection.*/
    const NdbDictionary::Column* m_columns[MAX_ATTRIBUTES_IN_TABLE];
    /** The number of columns in the projection.*/
    int m_columnCount;
    /** The number of columns in the table that the operation refers to.*/
    const int m_noOfColsInTable;
    /** User Projection, represented as a bitmap (indexed with column numbers).*/
    Bitmask<MAXNROFATTRIBUTESINWORDS> m_mask;
    /** True if columns were added in ascending order (ordered according to 
     * column number).*/
    bool m_isOrdered;
    /** Highest column number seen so far.*/
    int m_maxColNo;
  }; // class UserProjection

  STATIC_CONST (MAGIC = 0xfade1234);

  /** Interface for the application developer.*/
  NdbQueryOperation m_interface;
  /** For verifying pointers to this class.*/
  const Uint32 m_magic;
  /** NdbQuery to which this operation belongs. */
  NdbQueryImpl& m_queryImpl;
  /** The (transaction independent ) definition from which this instance
   * was created.*/
  const NdbQueryOperationDefImpl& m_operationDef;
  /* MAYBE: replace m_children and m_parents with navigation via 
   * m_operationDef.getParentOperation() etc.*/
  /** Parents of this operation.*/
  Vector<NdbQueryOperationImpl*> m_parents;
  /** Children of this operation.*/
  Vector<NdbQueryOperationImpl*> m_children;

  /** For processing results from this operation.*/
  NdbResultStream** m_resultStreams;
  /** Buffer for parameters in serialized format */
  Uint32Buffer m_params;
  /** Projection to be sent to the application.*/
  UserProjection m_userProjection;
  /** NdbRecord, NdbRecAttr or none.*/
  ResultStyle m_resultStyle;
  /** For temporary storing one result batch.*/
  char* m_batchBuffer;
  /** Buffer for final storage of result.*/
  char* m_resultBuffer;
  /** Pointer to application pointer that should be set to point to the 
   * current row.
   * @see NdbQueryOperationImpl::setResultRowRef */
  const char** m_resultRef;
  /** True if this operation gave no result for the current row.*/
  bool m_isRowNull;
  /** Batch size for scans or lookups with scan parents.*/
  Uint32 m_batchByteSize;
  /** Result record.*/
  const NdbRecord* m_ndbRecord;

  /** Head & tail of NdbRecAttr list defined by this operation.
    * Used for old-style result retrieval (using getValue()).*/
  NdbRecAttr* m_firstRecAttr;
  NdbRecAttr* m_lastRecAttr;

  explicit NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
                                 const NdbQueryOperationDefImpl& def);
  ~NdbQueryOperationImpl();

  /** Fetch result for non-root operation.*/
  void updateChildResult(Uint32 resultStreamNo, Uint32 rowNo);

  /** Copy any NdbRecAttr results into application buffers.*/
  void fetchRecAttrResults(Uint32 streamNo);

  /** Fix parent-child references when a complete batch has been received
   * for a given stream.
   */
  void buildChildTupleLinks(Uint32 streamNo);


  /** Serialize parameter values.
   *  @return possible error code.*/
  int serializeParams(const constVoidPtr paramValues[]);

  /** Construct and prepare receiver streams for result processing. */
  int prepareReceiver();

  /** Prepare ATTRINFO for execution. (Add execution params++)
   *  @return possible error code.*/
  int prepareAttrInfo(Uint32Buffer& attrInfo);

  /** Return I-value (for putting in object map) for a receiver pointing back 
   * to this object. TCKEYCONF is processed by first looking up an 
   * NdbReceiver instance in the object map, and then following 
   * NdbReceiver::m_query_operation_impl here.*/
  Uint32 getIdOfReceiver() const;
  
  /** Verify magic number.*/
  bool checkMagicNumber() const
  { return m_magic == MAGIC; }

  /** Check if batch is complete for this operation.*/
  bool isBatchComplete() const;

  const NdbReceiver& getReceiver(Uint32 recNo) const;

}; // class NdbQueryOperationImpl


#endif
