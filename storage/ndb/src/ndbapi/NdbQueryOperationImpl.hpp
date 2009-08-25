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
#include "NdbImpl.hpp"
#include "NdbError.hpp"
#include "NdbTransaction.hpp"
#include <ObjectMap.hpp>
#include <Vector.hpp>
#include <NdbOut.hpp>

class NdbQueryImpl {
private:
  // Only constructable from factory ::buildQuery();
  explicit NdbQueryImpl(
             NdbTransaction& trans,
             const NdbQueryDefImpl& queryDef,
             NdbQueryImpl* next);

  ~NdbQueryImpl();
public:
  STATIC_CONST (MAGIC = 0xdeadface);

  // Factory method which instantiate a query from its definition
  static NdbQueryImpl* buildQuery(NdbTransaction& trans, 
                                  const NdbQueryDefImpl& queryDef, 
                                  NdbQueryImpl* next);

  Uint32 getNoOfOperations() const;

  // Get a specific NdbQueryOperation by ident specified
  // when the NdbQueryOperationDef was created.
  NdbQueryOperationImpl& getQueryOperation(Uint32 ident) const;
  NdbQueryOperationImpl* getQueryOperation(const char* ident) const;

  Uint32 getNoOfParameters() const;
  const NdbParamOperand* getParameter(const char* name) const;
  const NdbParamOperand* getParameter(Uint32 num) const;

  NdbQuery::NextResultOutcome nextResult(bool fetchAllowed, bool forceSend);

  void close(bool forceSend, bool release);

  NdbTransaction* getNdbTransaction() const;

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
  }
  void setErrorCodeAbort(int aErrorCode);

 /** Process TCKEYCONF message. Return true if query is complete.*/
  bool execTCKEYCONF();

  /** Count number of completed streams within this batch. (There should be 
   * 'parallelism' number of streams for each operation.)
   * @param increment Change in count of  completed operations.
   * @return True if batch is complete.*/
  bool incPendingStreams(int increment);

  /** Prepare for execution. 
   *  @return possible error code.*/
  int prepareSend();

  /** Release all NdbReceiver instances.*/
  void release();

  bool checkMagicNumber() const
  { return m_magic == MAGIC; }

  Uint32 ptr2int() const
  { return m_id; }
  
  const NdbQuery& getInterface() const
  { return m_interface; }

  NdbQuery& getInterface()
  { return m_interface; }
  
  /** Get next query in same transaction.*/
  NdbQueryImpl* getNext() const
  { return m_next; }

  /** TODO: Remove. Temporary hack for prototype.*/
  NdbOperation* getNdbOperation() const
  { return m_ndbOperation; }

  Uint32 getParallelism() const
  { return m_parallelism; } 

  const NdbQueryDefImpl& getQueryDef() const
  { return m_queryDef; }

  NdbQueryOperationImpl& getRoot() const 
  { return getQueryOperation(0U);}
  
private:
  NdbQuery m_interface;

  /** For verifying pointers to this class.*/
  const Uint32 m_magic;
  /** I-value for object maps.*/
  const Uint32 m_id;
  /** Possible error status of this query.*/
  NdbError m_error;
  /** Transaction in which this query instance executes.*/
  NdbTransaction& m_transaction;
  /** The operations constituting this query.*/
  NdbQueryOperationImpl *m_operations;  // 'Array of ' OperationImpls
  size_t m_countOperations;                       // #elements in above array

  /** True if a TCKEYCONF message has been received for this query.*/
  bool m_tcKeyConfReceived;
  /** Number of streams not yet completed within the current batch.*/
  int m_pendingStreams;
  /** Serialized representation of parameters. To be sent in TCKEYREQ*/
  Uint32Buffer m_serializedParams;
  /** Next query in same transaction.*/
  NdbQueryImpl* const m_next;
  /** TODO: Remove this.*/
  NdbOperation* m_ndbOperation;
  /** Definition of this query.*/
  const NdbQueryDefImpl& m_queryDef;
  /** Number of fragments to be scanned in parallel. (1 if root operation is 
   * a lookup)*/
  Uint32 m_parallelism;
}; // class NdbQueryImpl


/** This class contains data members for NdbQueryOperation, such that these
 * do not need to exposed in NdbQueryOperation.hpp. This class may be 
 * changed without forcing the customer to recompile his application.*/
class NdbQueryOperationImpl {

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl&);
public:
  STATIC_CONST (MAGIC = 0xfade1234);

  explicit NdbQueryOperationImpl(NdbQueryImpl& queryImpl, 
                                 const NdbQueryOperationDefImpl& def);
  ~NdbQueryOperationImpl();

  /** Fetch next result row. 
   * @see NdbQuery::nextResult */
  static NdbQuery::NextResultOutcome nextResult(NdbQueryImpl& queryImpl, 
                                                bool fetchAllowed, 
                                                bool forceSend);

  /** Close scan receivers used for lookups. (Since scans and lookups should
   * have the same semantics for nextResult(), lookups use scan-type 
   * NdbReceiver objects.)*/
  static void closeSingletonScans(const NdbQueryImpl& query);

  Uint32 getNoOfParentOperations() const;
  NdbQueryOperationImpl& getParentOperation(Uint32 i) const;

  Uint32 getNoOfChildOperations() const;
  NdbQueryOperationImpl& getChildOperation(Uint32 i) const;

  const NdbQueryOperationDefImpl& getQueryOperationDef() const;

  // Get the entire query object which this operation is part of
  NdbQueryImpl& getQuery() const;

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

  /** Returns an I-value for the NdbReceiver object that shall receive results
   * for this operation. 
   * @return The I-value.
   */
  /*Uint32 getResultPtr() const {
    return m_receiver.getId();
    };*/


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

  /** Serialize parameter values.
   *  @return possible error code.*/
  int serializeParams(const constVoidPtr paramValues[]);

  /** Prepare for execution. 
   *  @return possible error code.*/
  int prepareSend(Uint32Buffer& serializedParams);

  /** Release NdbReceiver objects.*/
  void release();

  /* TODO: Remove this method. Only needed in spj_test.cpp.*/
  /** Return I-value for putting in object map.*/
  Uint32 ptr2int() const {
    return m_id;
  }
  
  /** Verify magic number.*/
  bool checkMagicNumber() const { 
    return m_magic == MAGIC;
  }

  /** Check if batch is complete for this operation.*/
  bool isBatchComplete() const;

  /** Return true if this operation is a scan.*/
  bool isScan() const {
    return getQueryOperationDef().getType() 
      == NdbQueryOperationDefImpl::TableScan ||
      getQueryOperationDef().getType() 
      == NdbQueryOperationDefImpl::OrderedIndexScan;
  }

  const NdbQueryOperation& getInterface() const
  { return m_interface; }
  NdbQueryOperation& getInterface()
  { return m_interface; }

  const NdbReceiver& getReceiver(Uint32 recNo) const;

  /** Find max number of rows per batch per ResultStream.*/
  void findMaxRows();

  Uint32 getMaxBatchRows() const { return m_maxBatchRows;}

  /** A shorthand for getting the root operation.*/
  NdbQueryOperationImpl& getRoot() const { 
    return getQuery().getRoot();
  }

private:
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
     * @param withCorrelation Include correlation data in projection.
     * @return Possible error code.*/
    int serialize(Uint32Slice dst, bool withCorrelation) const;
    
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

  /** A 'void' index for a tuple in structures below.*/
  STATIC_CONST( tupleNotFound = 0xffffffff);

  /** A map from tuple correlation Id to tuple number.*/
  class TupleIdMap{
  public:
    explicit TupleIdMap():m_vector(){}

    void put(Uint16 id, Uint32 num);

    Uint32 get(Uint16 id) const;

  private:
    struct Pair{
      Uint16 m_id;
      Uint16 m_num;
    };
    Vector<Pair> m_vector;
    /** No copying.*/
    TupleIdMap(const TupleIdMap&);
    TupleIdMap& operator=(const TupleIdMap&);
  }; // class TupleIdMap

  /** For scans, we receiver n parallel streams of data. There is a 
   * ResultStream object for each such stream. (For lookups, there is a 
   * single result stream.)*/
  class ResultStream{
  public:
    /** Stream number within operation (0 - parallelism)*/
    const Uint32 m_streamNo;
    /** The receiver object that unpacks transid_AI messages.*/
    NdbReceiver m_receiver;
    /** The number of transid_AI messages received.*/
    Uint32 m_transidAICount;
    /** A map from tuple correlation Id to tuple number.*/
    TupleIdMap m_correlToTupNumMap;
    /** Number of pending TCKEYREF or TRANSID_AI messages for this stream.*/
    int m_pendingResults;
    /** True if there is a pending SCAN_TABCONF messages for this stream.*/
    bool m_pendingScanTabConf;

    explicit ResultStream(NdbQueryOperationImpl& operation, Uint32 streamNo);

    ~ResultStream();

    Uint32 getChildTupleIdx(Uint32 childNo, Uint32 tupleNo) const {
      return m_childTupleIdx[tupleNo*m_operation.getNoOfChildOperations()
                             +childNo];
    }

    void setChildTupleIdx(Uint32 childNo, Uint32 tupleNo, Uint32 index){
      m_childTupleIdx[tupleNo*m_operation.getNoOfChildOperations()
                      +childNo] = index;
    }
    
    /** Prepare for receiving results (Invoked via NdbTransaction::execute()).*/
    void prepare();

    /** Get the correlation number of the parent of a given row. This number
     * can be used */
    Uint32 getParentTupleCorr(Uint32 rowNo) const { 
      return m_parentTupleCorr[rowNo];
    }

    void setParentTupleCorr(Uint32 rowNo, Uint32 correlationNum) const { 
      m_parentTupleCorr[rowNo] = correlationNum;
    }
    
    /** Check if batch is complete for this stream. */
    bool isBatchComplete() const { 
      return m_pendingResults==0 && !m_pendingScanTabConf;
    }

  private:
    /** Operation to which this resultStream belong.*/
    NdbQueryOperationImpl& m_operation;
    /** One-dimensional array. For each tuple, this array holds the correlation
     * number of the corresponding parent tuple. */
    Uint32* m_parentTupleCorr;
    /** Two dimenional array of indexes to child tuples ([childNo, ownTupleNo])
     * This is used for finding the child tuple in the corresponding resultStream of 
     the child operation. */
    Uint32* m_childTupleIdx;
    /** No copying.*/
    ResultStream(const ResultStream&);
    ResultStream& operator=(const ResultStream&);
  };

  /** A stack of ResultStream pointers.*/
  class StreamStack{
  public:
    explicit StreamStack(int size);

    ~StreamStack(){
      delete[] m_array;
    }

    ResultStream* top() const { 
      return m_current>=0 ? m_array[m_current] : NULL; 
    }

    void pop(){ 
      assert(m_current>=0);
      m_current--;
    }
    
    void push(ResultStream& stream);

    void clear(){
      m_current = -1;
    }

  private:
    const int m_size;
    int m_current;
    ResultStream** const m_array;
    // No copying.
    StreamStack(const StreamStack&);
    StreamStack& operator=(const StreamStack&);
  }; // class StreamStack

  /** Possible return values from NdbQuery::fetchMoreResults. Integer values
   * matches those returned from PoolGourad::waitScan().*/
  enum FetchResult{
    FetchResult_otherError = -4,
    FetchResult_sendFail = -3,
    FetchResult_nodeFail = -2,
    FetchResult_timeOut = -1,
    FetchResult_ok = 0,
    FetchResult_scanComplete = 1
  };

  /** Fix parent-child references when a complete batch has been received
   * for a given stream.*/
  static void buildChildTupleLinks(const NdbQueryImpl& query, Uint32 streamNo);

  /** Interface for the application developer.*/
  NdbQueryOperation m_interface;
  /** For verifying pointers to this class.*/
  const Uint32 m_magic;
  /** I-value for object maps.*/
  const Uint32 m_id;
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
  /** Possibly (hidden) operation being index (and parent) for this op. */
//NdbQueryOperationImpl* const m_index;

  /** For processing results from this operation.*/
  ResultStream** m_resultStreams;
  /** Buffer for parameters in serialized format */
  Uint32Buffer m_params;
  /** Projection to be sent to the application.*/
  UserProjection m_userProjection;
  /** NdbRecord and old style result retrieval may not be combined.*/
  enum {
    Style_None,       // Not set yet.
    Style_NdbRecord,  // Use old style result retrieval.
    Style_NdbRecAttr, // Use NdbRecord.
  } m_resultStyle;
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
  /** Max rows (per resultStream) in a scan batch.*/
  Uint32 m_maxBatchRows;
  /** Result record.*/
  const NdbRecord* m_ndbRecord;
  /** Streams that the application is currently iterating over. Only accessed
   * by application thread.*/
  StreamStack m_applStreams;
  /** Streams that have received a complete batch. Shared between 
   * application thread and receiving thread. Access should be mutex protected.
   */
  StreamStack m_fullStreams;
  /** Fetch result for non-root operation.*/
  void updateChildResult(Uint32 resultStreamNo, Uint32 rowNo);
  /** Get more scan results, ask for the next batch if necessary.*/
  FetchResult fetchMoreResults(bool forceSend);
}; // class NdbQueryOperationImpl



#endif
