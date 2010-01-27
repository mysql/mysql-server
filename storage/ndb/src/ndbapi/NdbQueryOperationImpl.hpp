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
#include "NdbIndexScanOperation.hpp"
#include <NdbError.hpp>
#include <ndb_limits.h>
#include <Vector.hpp>
#include <Bitmask.hpp>

// Forward declarations
class NdbTableImpl;
class NdbIndexImpl;
class NdbApiSignal;
class NdbResultStream;
class NdbParamOperand;
class NdbTransaction;
class NdbReceiver;
class NdbOut;
class NdbRootFragment;

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
                                  const NdbQueryDefImpl& queryDef);

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

  /** Close query: 
   *  - Release datanode resources,
   *  - Discard pending result sets,
   *  - optionaly dealloc NdbQuery structures
   */
  int close(bool forceSend);

  /** Deallocate 'this' NdbQuery object and its NdbQueryOperation objects.
   *  If not already closed, it will also ::close() the NdbQuery.
   */
  void release();

  NdbTransaction& getNdbTransaction() const
  { return m_transaction; }

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
    m_state = Failed;
  }

  /** Set an error code and initiate transaction abort.*/
  void setErrorCodeAbort(int aErrorCode);

  /** Assign supplied parameter values to the parameter placeholders
   *  created when the query was defined.
   *  Values are *copied* into this NdbQueryImpl object:
   *  Memory location used as source for parameter values don't have
   *  to be valid after this assignment.
   */
  int assignParameters(const constVoidPtr paramValues[]);

  int setBound(const NdbRecord *keyRecord,
               const NdbIndexScanOperation::IndexBound *bound);

  /** Prepare for execution. 
   *  @return possible error code.
   */
  int prepareSend();

  /** Send prepared signals from this NdbQuery to start execution
   *  @return #signals sent, -1 if error.
   */
  int doSend(int aNodeId, bool lastFlag);

  NdbQuery& getInterface()
  { return m_interface; }

  /** Get next query in same transaction.*/
  NdbQueryImpl* getNext() const
  { return m_next; }
  void setNext(NdbQueryImpl* next)
  { m_next = next; }

  /** Get the maximal number of rows that may be returned in a single 
   * transaction.*/
  Uint32 getMaxBatchRows() const
  { return m_maxBatchRows; }

  /** Get the (transaction independent) definition of this query. */
  const NdbQueryDefImpl& getQueryDef() const
  { return m_queryDef; }

  /** Process TCKEYCONF message. Return true if query is complete. */
  bool execTCKEYCONF();

  /** Process SCAN_TABCONF w/ EndOfData which is a 'Close Scan Reply'. */
  void execCLOSE_SCAN_REP(bool isClosed);

  /** Determines if query has completed and may be garbage collected
   *  A query is considder complete when it either:
   *  - Has returned all its rows to the client m_state == EndOfData.
   *  - Has been closed by the client (m_state == Closed)
   *  - Has encountered a failure (m_state == Failed)
   *  - Is a lookup query which has been executed. (single row fetched)
   */
  bool hasCompleted() const
  { return m_state >= Closed ||
           (!m_queryDef.isScanQuery() && m_state >= Executing); 
  }
  
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

  /** A stack of NdbRootFragment pointers. */
  class FragStack{
  public:
    explicit FragStack();

    ~FragStack() {
      delete[] m_array;
    }

    // Prepare internal datastructures.
    // Return 0 if ok, else errorcode
    int prepare(int capacity);

    NdbRootFragment* top() const { 
      return m_current>=0 ? m_array[m_current] : NULL; 
    }

    NdbRootFragment* pop() { 
      assert(m_current>=0);
      return m_array[m_current--];
    }
    
    void push(NdbRootFragment& frag);

    void clear() {
      m_current = -1;
    }

  private:
    /** Capacity of stack.*/
    int m_capacity;
    /** Index of current top of stack.*/
    int m_current;
    NdbRootFragment** m_array;
    // No copying.
    FragStack(const FragStack&);
    FragStack& operator=(const FragStack&);
  }; // class FragStack

  class OrderedFragSet{
  public:
    explicit OrderedFragSet();

    ~OrderedFragSet() 
    { delete[] m_array; }

    // Prepare internal datastructures.
    // Return 0 if ok, else errorcode
    int prepare(NdbScanOrdering ordering, 
                int size, 
                const NdbRecord* keyRecord,
                const NdbRecord* resultRecord);

    NdbRootFragment* getCurrent();

    void reorder();

    void add(NdbRootFragment& frag);

    void clear() 
    { m_size = 0; }


    /** When doing an ordered scan, get the fragment that needs a new batch.*/
    NdbRootFragment* getEmpty() const;

    bool verifySortOrder() const;

  private:
    /** Max no of fragments.*/
    int m_capacity;
    /** Current number of fragments.*/
    int m_size;
    /** Current number of completed fragments (where all data have been received
     * and processed).*/
    int m_completedFrags;
    /** Ordering of index scan result.*/
    NdbScanOrdering m_ordering;
    /** Needed for comparing records when ordering results.*/
    const NdbRecord* m_keyRecord;
    /** Needed for comparing records when ordering results.*/
    const NdbRecord* m_resultRecord;
    NdbRootFragment** m_array;
    // No copying.
    OrderedFragSet(const OrderedFragSet&);
    OrderedFragSet& operator=(const OrderedFragSet&);
    /** For sorting fragment reads according to index value of first record. 
     * Also f1<f2 if f2 has reached end of data and f1 has not.
     * @return 1 if f1>f2, 0 if f1==f2, -1 if f1<f2.*/
    int compare(const NdbRootFragment& frag1,
                const NdbRootFragment& frag2) const;

  }; // class OrderedFragSet

  /** The interface that is visible to the application developer.*/
  NdbQuery m_interface;

  enum {        // State of NdbQuery in API
    Initial,    // Constructed object, assiciated with a defined query
    Defined,    // Parameter values has been assigned
    Prepared,   // KeyInfo & AttrInfo prepared for execution
    Executing,  // Signal with exec. req. sent to TC
    EndOfData,  // All results rows consumed
    Closed,     // Query has been ::close()'ed 
    Failed,     
    Destructed
  } m_state;

  enum {        // Assumed state of query cursor in TC block
    Inactive,   // Execution not started at TC
    Active
  } m_tcState;

  /** Next query in same transaction.*/
  NdbQueryImpl* m_next;
  /** Definition of this query.*/
  const NdbQueryDefImpl& m_queryDef;

  /** Possible error status of this query.*/
  NdbError m_error;
  /** Transaction in which this query instance executes.*/
  NdbTransaction& m_transaction;

  /** Scan queries creates their own sub transaction which they
   *  execute within.
   *  Has same transId, Ndb*, ++ as the 'real' transaction above.
   */
  NdbTransaction* m_scanTransaction;

  /** The operations constituting this query.*/
  NdbQueryOperationImpl *m_operations;  // 'Array of ' OperationImpls
  size_t m_countOperations;             // #elements in above array

  /** Number of root fragments not yet completed within the current batch.*/
  Uint32 m_pendingFrags;

  /** Number of fragments to be read by the root operation. (1 if root 
   * operation is a lookup)*/
  Uint32 m_rootFragCount;

  /**
   * This is an array with one element for each fragment that the root
   * operation accesses (i.e. one for a lookup, all for a table scan).
   * It keeps the state of the read operation on that fragment, and on
   * any child operation instance derived from it.
   */
  NdbRootFragment* m_rootFrags;

  /** Max rows (per resultStream) in a scan batch.*/
  Uint32 m_maxBatchRows;

  /** Root fragments that the application is currently iterating over. Only 
   * accessed by application thread.
   */
  OrderedFragSet m_applFrags;

  /** Root frgaments that have received a complete batch. Shared between 
   *  application thread and receiving thread. Access should be mutex protected.
   */
  FragStack m_fullFrags;

  /** Number of root fragments for which confirmation for the final batch 
   * (with tcPtrI=RNIL) has been received. Observe that even if 
   * m_finalBatchFrags==m_rootFragCount, all tuples for the final batches may
   * still not have been received (i.e. m_pendingFrags>0).
   */
  Uint32 m_finalBatchFrags;

  /* Number of IndexBounds set by API (index scans only) */
  Uint32 m_num_bounds;

  /* Most recently added IndexBound's range number */
  Uint32 m_previous_range_num;

  /**
   * Signal building section:
   */
  Uint32Buffer m_attrInfo;  // ATTRINFO: QueryTree + serialized parameters
  Uint32Buffer m_keyInfo;   // KEYINFO:  Lookup key or scan bounds

  // Only constructable from factory ::buildQuery();
  explicit NdbQueryImpl(
             NdbTransaction& trans,
             const NdbQueryDefImpl& queryDef);

  ~NdbQueryImpl();

  /** Release resources after scan has returned last available result */
  void postFetchRelease();

  /** Get more scan results, ask for the next batch if necessary.*/
  FetchResult fetchMoreResults(bool forceSend);

  /** Send SCAN_NEXTREQ signal to fetch another batch from a scan query
   *  @return #signals sent, -1 if error.
   */
  int sendFetchMore(int nodeId);

  /** Close cursor on TC */
  int closeTcCursor(bool forceSend);

  /** Send SCAN_NEXTREQ(close) signal to close cursor on TC and datanodes.
   *  @return #signals sent, -1 if error.
   */
  int sendClose(int nodeId);

  /** Close scan receivers used for lookups. (Since scans and lookups should
   * have the same semantics for nextResult(), lookups use scan-type 
   * NdbReceiver objects.)
   */
  void closeSingletonScans();

  /** Fix parent-child references when a complete batch has been received
   * for a given root fragment.
   */
  void buildChildTupleLinks(Uint32 rootFragNo);

  const NdbQuery& getInterface() const
  { return m_interface; }

  NdbQueryOperationImpl& getRoot() const 
  { return getQueryOperation(0U); }

  /** Count number of completed root fragments within this batch. 
   *  @param increment Change in count of completed root frgaments.
   *  @return True if batch is complete.
   */
  bool incrementPendingFrags(int increment);

  /** Get the number of fragments to be read for the root operation.*/
  Uint32 getRootFragCount() const
  { return m_rootFragCount; }

  /** Check if batch is complete (no outstanding messages).*/
  bool isBatchComplete() const;

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
  NdbRecAttr* getValue(const NdbColumnImpl&, char* resultBuffer);

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
  bool execSCAN_TABCONF(Uint32 tcPtrI, Uint32 rowCount, NdbReceiver* receiver); 

  const NdbQueryOperation& getInterface() const
  { return m_interface; }
  NdbQueryOperation& getInterface()
  { return m_interface; }

  int setOrdering(NdbScanOrdering ordering);

  NdbScanOrdering getOrdering() const
  { return m_ordering; }

  /**
   * Set the NdbInterpretedCode needed for defining a scan filter for 
   * this operation. 
   * It is an error to call this method on a lookup operation.
   * @param code The interpreted code. This object is copied internally, 
   * meaning that 'code' may be destroyed as soon as this method returns.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setInterpretedCode(NdbInterpretedCode& code);

  NdbResultStream& getResultStream(Uint32 rootFragNo) const;

  const NdbReceiver& getReceiver(Uint32 rootFragNo) const;

  /** Verify magic number.*/
  bool checkMagicNumber() const
  { return m_magic == MAGIC; }

private:
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

  /** For processing results from this operation (Array of).*/
  NdbResultStream** m_resultStreams;
  /** Buffer for parameters in serialized format */
  Uint32Buffer m_params;

  /** Internally allocated buffer for temporary storing one result batch.*/
  char* m_batchBuffer;
  /** User specified buffer for final storage of result.*/
  char* m_resultBuffer;
  /** User specified pointer to application pointer that should be 
   * set to point to the current row inside m_batchBuffer
   * @see NdbQueryOperationImpl::setResultRowRef */
  const char** m_resultRef;
  /** True if this operation gave no result for the current row.*/
  bool m_isRowNull;
  /** Batch size for scans or lookups with scan parents.*/
  Uint32 m_batchByteSize;

  /** Result record & optional bitmask to disable read of selected cols.*/
  const NdbRecord* m_ndbRecord;
  const unsigned char* m_read_mask;

  /** Head & tail of NdbRecAttr list defined by this operation.
    * Used for old-style result retrieval (using getValue()).*/
  NdbRecAttr* m_firstRecAttr;
  NdbRecAttr* m_lastRecAttr;

  /** Ordering of scan results (only applies to ordered index scans.)*/
  NdbScanOrdering m_ordering;

  /** A scan filter is mapped to an interpeter code program, which is stored
   * here. (This field is NULL if no scan filter has been defined.)*/
  NdbInterpretedCode* m_interpretedCode;

  explicit NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
                                 const NdbQueryOperationDefImpl& def);
  ~NdbQueryOperationImpl();

  /** Release resources after scan has returned last available result */
  void postFetchRelease();

  /** Fetch result for non-root operation.*/
  void updateChildResult(Uint32 rootFragNo, Uint32 rowNo);

  /** Count number of descendant operations (excluding the operation itself) */
  Int32 getNoOfDescendantOperations() const;

  /** Copy any NdbRecAttr results into application buffers.*/
  void fetchRecAttrResults(Uint32 rootFragNo);

  /** Serialize parameter values.
   *  @return possible error code.*/
  int serializeParams(const constVoidPtr paramValues[]);

  int serializeProject(Uint32Buffer& attrInfo) const;

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
  
  /** 
   * If the operation has a scan filter, append the corresponding
   * interpreter code to a buffer.
   * @param attrInfo The buffer to which the code should be appended.
   * @return possible error code */
  int prepareScanFilter(Uint32Buffer& attrInfo) const;
}; // class NdbQueryOperationImpl


#endif
