/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbScanOperation_H
#define NdbScanOperation_H

#include "NdbOperation.hpp"

class NdbBlob;
class NdbResultSet;
class PollGuard;

/**
 * @class NdbScanOperation
 * @brief Class of scan operations for use in transactions.  
 */
class NdbScanOperation : public NdbOperation {
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbTransaction;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbBlob;
  friend class NdbScanFilter;
  friend class NdbQueryOperationImpl;
#endif

public:
  /**
   * Scan flags.  OR-ed together and passed as argument to
   * readTuples, scanIndex, and scanTable. Note that SF_MultiRange
   * has to be set if several ranges (bounds) are to be passed.
   */
  enum ScanFlag {
    /* Scan in TUP order (the order of rows in memory). Table scan only. */
    SF_TupScan = (1 << 16),
    /* Scan in DISK order (the order of rows on disk). Table scan only. */
    SF_DiskScan = (2 << 16),
    /*
      Return rows from an index scan sorted, ordered on the index key.
      Both ascending order or descending order scans are affected by this flag.
      This flag makes the API perform a merge-sort among the ordered scans of
      each fragment, to get a single sorted result set.
      Note that :
      1) Ordered indexes are distributed - there is one for each fragment of a
         table.
      2) Range scans are often parallel - across all index fragments.  
         Occasionally they can be pruned to one index fragment.
      3) Each index fragment range scan will return results in either ascending
         or descending order.  Ascending is the default, but descending is
         chosen if SF_Descending is set.
      4) Where multiple index fragments are scanned in parallel, the results
         are sent back to NdbApi where they can optionally be merge-sorted
         before being returned to the user.  This merge sorting is controlled
         via the SF_OrderBy and SF_OrderByFull flags.
      5) Without SF_OrderBy* flags, the results from each index fragment will
         be in-order (ascending or descending), but results from different
         fragments may be interleaved.
      6) With SF_OrderBy* flags, some extra constraints are imposed internally:
        i) If the range scan is not pruned to one index fragment then all
           index fragments must be scanned in parallel.  (Non SF_OrderBy* flag
           scans can be executed with lower than full-parallelism)
        ii) Results from every index fragment must be available before returning
            any row, to ensure a correct merge sort.  This serialises the 
            'scrolling' of the scan, potentially resulting in lower row
            throughput.
        iii) Non SF_OrderBy* flag scans can return rows to the Api before all
             index fragments have returned a batch, and can overlap next-batch
             requests with Api row processing.
    */
    SF_OrderBy = (1 << 24),
    /**
     * Same as order by, except that it will automatically 
     *   add all key columns into the read-mask
     */
    SF_OrderByFull = (16 << 24),

    /* Index scan in descending order, instead of default ascending. */
    SF_Descending = (2 << 24),
    /*
      Enable @ref get_range_no (index scan only).
      When this flag is set, NdbIndexScanOperation::get_range_no() can be
      called to read back the range_no defined in
      NdbIndexScanOperation::setBound(). See @ref setBound() for
      explanation.
      Additionally, when this flag is set and SF_OrderBy* is also set, results
      from ranges are returned in their entirety before any results are returned
      from subsequent ranges.
    */
    SF_ReadRangeNo = (4 << 24),
    /* Scan is part of multi-range scan. */
    SF_MultiRange = (8 << 24),
    /*
      Request KeyInfo to be sent back.
      This enables the option to take over the row lock taken by the scan using
      lockCurrentTuple(), by making sure that the kernel sends back the
      information needed to identify the row and the lock.
      It is enabled by default for scans using LM_Exclusive, but must be
      explicitly specified to enable the taking-over of LM_Read locks.
    */
    SF_KeyInfo = 1
  };


  /*
   * ScanOptions
   *  These are options passed to the NdbRecord based scanTable and 
   *  scanIndex methods of the NdbTransaction class.
   *  Each option type is marked as present by setting the corresponding
   *  bit in the optionsPresent field.  Only the option types marked 
   *  in the optionsPresent field need have sensible data.
   *  All data is copied out of the ScanOptions structure (and any
   *  subtended structures) at operation definition time.
   *  If no options are required, then NULL may be passed as the 
   *  ScanOptions pointer.
   *
   *  Most methods take a supplementary sizeOfOptions parameter.  This
   *  is optional, and is intended to allow the interface implementation
   *  to remain backwards compatible with older un-recompiled clients 
   *  that may pass an older (smaller) version of the ScanOptions 
   *  structure.  This effect is achieved by passing
   *  sizeof(NdbScanOperation::ScanOptions) into this parameter.
   */
  struct ScanOptions
  {
    /*
      Size of the ScanOptions structure.
    */
    static inline Uint32 size()
    {
        return sizeof(ScanOptions);
    }

    /* Which options are present - see below for possibilities */
    Uint64 optionsPresent;

    enum Type { SO_SCANFLAGS    = 0x01,
                SO_PARALLEL     = 0x02,
                SO_BATCH        = 0x04,
                SO_GETVALUE     = 0x08,
                SO_PARTITION_ID = 0x10,
                SO_INTERPRETED  = 0x20,
                SO_CUSTOMDATA   = 0x40,
                SO_PART_INFO    = 0x80
    };

    /* Flags controlling scan behaviour
     * See NdbScanOperation::ScanFlag for details
     */
    Uint32 scan_flags;

    /* Desired scan parallelism.
     * Default == 0 == Maximum parallelism
     */
    Uint32 parallel;

    /* Desired scan batchsize in rows 
     * for NDBD -> API transfers
     * Default == 0 == Automatically chosen size
     */
    Uint32 batch;
    
    /* Extra values to be read for each row meeting
     * scan criteria
     */
    NdbOperation::GetValueSpec *extraGetValues;
    Uint32                     numExtraGetValues;

    /* Specific partition to limit this scan to
     * Alternatively, an Ndb::PartitionSpec can be supplied.
     * For Index Scans, partitioning information can be supplied for
     * each range
     */
    Uint32 partitionId;

    /* Interpreted code to execute as part of the scan */
    const NdbInterpretedCode *interpretedCode;

    /* CustomData ptr to associate with the scan operation */
    void * customData;

    /* Partition information for bounding this scan */
    const Ndb::PartitionSpec* partitionInfo;
    Uint32 sizeOfPartInfo;
  };


  /**
   * readTuples
   * Method used in old scan Api to specify scan operation details
   * 
   * @param lock_mode Lock mode
   * @param scan_flags see @ref ScanFlag
   * @param parallel Number of fragments to scan in parallel (0=max)
   * @param batch Number of rows to fetch in each batch
   */ 
  virtual
  int readTuples(LockMode lock_mode = LM_Read, 
                 Uint32 scan_flags = 0, 
		 Uint32 parallel = 0,
		 Uint32 batch = 0);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * readTuples
   * @param lock_mode Lock mode
   * @param batch No of rows to fetch from each fragment at a time
   * @param parallel No of fragments to scan in parallell
   * @note specifying 0 for batch and parallel means max performance
   */ 
#ifdef ndb_readtuples_impossible_overload
  int readTuples(LockMode lock_mode = LM_Read, 
		 Uint32 batch = 0, Uint32 parallel = 0, 
                 bool keyinfo = false, bool multi_range = false);
#endif
  
  inline int readTuples(int parallell){
    return readTuples(LM_Read, 0, parallell);
  }
  
  inline int readTuplesExclusive(int parallell = 0){
    return readTuples(LM_Exclusive, 0, parallell);
  }

  /* First version of ScanOptions, defined here for backwards
   * compatibility reasons
   */
  struct ScanOptions_v1
  {
    /* Which options are present - see below for possibilities */
    Uint64 optionsPresent;

    enum Type { SO_SCANFLAGS    = 0x01,
                SO_PARALLEL     = 0x02,
                SO_BATCH        = 0x04,
                SO_GETVALUE     = 0x08,
                SO_PARTITION_ID = 0x10,
                SO_INTERPRETED  = 0x20,
                SO_CUSTOMDATA   = 0x40 };

    /* Flags controlling scan behaviour
     * See NdbScanOperation::ScanFlag for details
     */
    Uint32 scan_flags;

    /* Desired scan parallelism.
     * Default == 0 == Maximum parallelism
     */
    Uint32 parallel;

    /* Desired scan batchsize in rows 
     * for NDBD -> API transfers
     * Default == 0 == Automatically chosen size
     */
    Uint32 batch;
    
    /* Extra values to be read for each row meeting
     * scan criteria
     */
    NdbOperation::GetValueSpec *extraGetValues;
    Uint32                     numExtraGetValues;

    /* Specific partition to limit this scan to
     * Only applicable for tables defined with UserDefined partitioning
     * For Index Scans, partitioning information can be supplied for
     * each range
     */
    Uint32 partitionId;

    /* Interpreted code to execute as part of the scan */
    const NdbInterpretedCode *interpretedCode;

    /* CustomData ptr to associate with the scan operation */
    void * customData;
  };
#endif
  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  virtual NdbBlob* getBlobHandle(const char* anAttrName);
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId);

  /** 
   * setInterpretedCode
   *
   * This method is used to set an interpreted program to be executed
   * against every row returned by the scan.  This is used to filter
   * rows out of the returned set.  This method is only supported for
   * old Api scans.  For NdbRecord scans, pass the interpreted program
   * via the ScanOptions structure.
   * 
   * @param code The interpreted program to be executed for each
   * candidate result row in this scan.
   * @return 0 if successful, -1 otherwise
   */
  int setInterpretedCode(const NdbInterpretedCode *code);

#endif


  /**
   * Get the next tuple in a scan transaction. 
   * 
   * After each call to nextResult
   * the buffers and NdbRecAttr objects defined in 
   * NdbOperation::getValue are updated with values 
   * from the scanned tuple. 
   *
   * @param fetchAllowed  If set to false, then fetching is disabled
   * @param forceSend If true send will occur immediately (see @ref secAdapt)
   *
   * The NDB API will receive tuples from each fragment in batches, and
   * needs to explicitly request from the NDB Kernel the sending of each new
   * batch. When a new batch is requested, the NDB Kernel will remove any
   * locks taken on rows in the previous batch, unless they have been already
   * taken over by the application executing updateCurrentTuple(),
   * lockCurrentTuple(), etc.
   *
   * The fetchAllowed parameter is used to control this release of
   * locks from the application. When fetchAllowed is set to false,
   * the NDB API will not request new batches from the NDB Kernel when
   * all received rows have been exhausted, but will instead return 2
   * from nextResult(), indicating that new batches must be
   * requested. You must then call nextResult with fetchAllowed = true
   * in order to contact the NDB Kernel for more records, after taking over
   * locks as appropriate.
   *
   * fetchAllowed = false is useful when you want to update or 
   * delete all the records fetched in one transaction(This will save a
   *  lot of round trip time and make updates or deletes of scanned 
   * records a lot faster).
   *
   * While nextResult(false) returns 0, take over the record to
   * another transaction. When nextResult(false) returns 2 you must
   * execute and commit the other transaction. This will cause the
   * locks to be transferred to the other transaction, updates or
   * deletes will be made and then the locks will be released.
   *
   * After that, call nextResult(true) which will fetch new records and
   * cache them in the NdbApi. 
   * 
   * @note  If you don't take over the records to another transaction the 
   *        locks on those records will be released the next time NDB Kernel
   *        is contacted for more records.
   *
   * @note  Please contact for examples of efficient scan
   *        updates and deletes.
   * 
   * @note  See ndb/examples/ndbapi_scan_example for usage.
   *
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   */
  int nextResult(bool fetchAllowed = true, bool forceSend = false);

  /*
   * NdbRecord version of nextResult.
   * 
   * When 0 is returned, this method updates out_row_ptr to point 
   * to the next result row.  The location pointed to is valid 
   * (only) until the next call to nextResult() with
   * fetchAllowed == true.
   * The NdbRecord object defining the row format was specified in the
   * NdbTransaction::scanTable (or scanIndex) call.
   * Note that this variant of nextResult has three parameters, and
   * all must be supplied to avoid invoking the two-parameter, non
   * NdbRecord variant of nextResult.
   */
  int nextResult(const char ** out_row_ptr,
                 bool fetchAllowed, 
                 bool forceSend);

  /*
   * Alternate NdbRecord version of nextResult.
   *
   * When 0 is returned, this method copies data from the result
   * to the output buffer. The buffer must be long enough for the
   * result NdbRecord row as returned by
   * NdbDictionary::getRecordRowLength(const NdbRecord* record);
   * The NdbRecord object defining the row format was specified in the
   * NdbTransaction::scanTable (or scanIndex) call.
   * @return
   * -  -1: if unsuccessful
   * -   0: if another tuple was received
   * -   1: if there are no more tuples to scan
   * -   2: if there are no more cached records in NdbApi
   */
  int nextResultCopyOut(char * buffer,
                 bool fetchAllowed,
                 bool forceSend);

  /**
   * Close scan
   */
  void close(bool forceSend = false, bool releaseOp = false);

  /**
   * Lock current tuple
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation* lockCurrentTuple();
  /**
   * Lock current tuple
   *
   * @param lockTrans Transaction that should perform the lock
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	lockCurrentTuple(NdbTransaction* lockTrans);
  /**
   * Update current tuple
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation* updateCurrentTuple();
  /**
   * Update current tuple
   *
   * @param updateTrans Transaction that should perform the update
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	updateCurrentTuple(NdbTransaction* updateTrans);

  /**
   * Delete current tuple
   * @return 0 on success or -1 on failure
   */
  int deleteCurrentTuple();
  /**
   * Delete current tuple
   *
   * @param takeOverTransaction Transaction that should perform the delete
   *
   * @return 0 on success or -1 on failure
   */
  int deleteCurrentTuple(NdbTransaction* takeOverTransaction);
  
  /*
   * NdbRecord versions of scan lock take-over operations.
   *
   * Note that calling NdbRecord scan lock take-over on an NdbRecAttr-style
   * scan is not valid, nor is calling NdbRecAttr-style scan lock take-over
   * on an NdbRecord-style scan.
   */

  /*
   * Take over the lock without changing the row.
   * Optionally also read from the row (call with default value NULL for 
   * result_row to not read any attributes.).
   * The NdbRecord * is required even when not reading any attributes.
   * Supported OperationOptions : OO_ABORTOPTION, OO_GETVALUE, OO_ANYVALUE
   */
  const NdbOperation *lockCurrentTuple(NdbTransaction *takeOverTrans,
                                       const NdbRecord *result_rec,
                                       char *result_row= 0,
                                       const unsigned char *result_mask= 0,
                                       const NdbOperation::OperationOptions *opts = 0,
                                       Uint32 sizeOfOptions = 0);

  /*
   * Update the current tuple, NdbRecord version.
   * Values to update with are contained in the passed-in row.
   * Supported OperationOptions : OO_ABORTOPTION, OO_SETVALUE, 
   *                              OO_INTERPRETED, OO_ANYVALUE
   */
  const NdbOperation *updateCurrentTuple(NdbTransaction *takeOverTrans,
                                         const NdbRecord *attr_rec,
                                         const char *attr_row,
                                         const unsigned char *mask= 0,
                                         const NdbOperation::OperationOptions *opts = 0,
                                         Uint32 sizeOfOptions = 0);

  /* Delete the current tuple. NdbRecord version.
   * The tuple can be read before being deleted.  Specify the columns to read
   * and the result storage as usual with result_rec, result_row and result_mask.
   * Supported OperationOptions : OO_ABORTOPTION, OO_GETVALUE, OO_ANYVALUE
   */
  const NdbOperation *deleteCurrentTuple(NdbTransaction *takeOverTrans,
                                         const NdbRecord *result_rec,
                                         char *result_row = 0,
                                         const unsigned char *result_mask = 0,
                                         const NdbOperation::OperationOptions *opts = 0,
                                         Uint32 sizeOfOptions = 0);

  /**
   * Get NdbTransaction object for this scan operation
   */
  NdbTransaction* getNdbTransaction() const;


  /**
   * Is scan operation pruned to a single table partition?
   * For NdbRecord defined scans, valid before+after execute.
   * For Old Api defined scans, valid only after execute.
   */
  bool getPruned() const;

protected:
  NdbScanOperation(Ndb* aNdb,
                   NdbOperation::Type aType = NdbOperation::TableScan);
  virtual ~NdbScanOperation();

  virtual NdbRecAttr* getValue_impl(const NdbColumnImpl*, char* aValue = 0);
  NdbRecAttr* getValue_NdbRecord_scan(const NdbColumnImpl*, char* aValue);
  NdbRecAttr* getValue_NdbRecAttr_scan(const NdbColumnImpl*, char* aValue);

  int handleScanGetValuesOldApi();
  int addInterpretedCode();
  int handleScanOptionsVersion(const ScanOptions*& optionsPtr, 
                               Uint32 sizeOfOptions,
                               ScanOptions& currOptions);
  int handleScanOptions(const ScanOptions *options);
  int validatePartInfoPtr(const Ndb::PartitionSpec*& partInfo,
                          Uint32 sizeOfPartInfo,
                          Ndb::PartitionSpec& partValue);
  int getPartValueFromInfo(const Ndb::PartitionSpec* partInfo,
                           const NdbTableImpl* table,
                           Uint32* partValue);
  int generatePackedReadAIs(const NdbRecord *reseult_record, bool& haveBlob,
                            const Uint32 * readMask);
  int scanImpl(const NdbScanOperation::ScanOptions *options, 
               const Uint32 * readMask);
  int scanTableImpl(const NdbRecord *result_record,
                    NdbOperation::LockMode lock_mode,
                    const unsigned char *result_mask,
                    const NdbScanOperation::ScanOptions *options,
                    Uint32 sizeOfOptions);

  int nextResultNdbRecord(const char * & out_row,
                          bool fetchAllowed, bool forceSend);
  virtual void release();
  
  int close_impl(bool forceSend,
                 PollGuard *poll_guard);

  /* Helper for NdbScanFilter to allocate an InterpretedCode
   * object owned by the Scan operation
   */
  NdbInterpretedCode* allocInterpretedCodeOldApi();
  void freeInterpretedCodeOldApi();

  int doSendSetAISectionSizes();

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded private methods from NdbOperation
  int init(const NdbTableImpl* tab, NdbTransaction*);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId,
                  NdbOperation::AbortOption);
  int doSend(int ProcessorId);
  virtual void setReadLockMode(LockMode lockMode);

  virtual void setErrorCode(int aErrorCode) const;
  virtual void setErrorCodeAbort(int aErrorCode) const;
  
  /* This is the transaction which defined this scan
   *   The transaction(connection) used for the scan is
   *   pointed to by NdbOperation::theNdbCon
   */
  NdbTransaction *m_transConnection;

  // Scan related variables
  Uint32 theParallelism;
  /*
    Whether keyInfo is requested from Kernel.
    KeyInfo is requested by application (using the SF_KeyInfo scan flag), and
    also enabled automatically when using exclusive locking (lockmode
    LM_Exclusive), or when requesting blobs (getBlobHandle()).
  */
  Uint32 m_keyInfo;

  int getFirstATTRINFOScan();
  int doSendScan(int ProcessorId);
  void finaliseScan();
  int finaliseScanOldApi();
  int prepareSendScan(Uint32 TC_ConnectPtr, Uint64 TransactionId,
                      const Uint32 * readMask);
  
  int fix_receivers(Uint32 parallel);
  void reset_receivers(Uint32 parallel, Uint32 ordered);
  Uint32* m_array; // containing all arrays below
  Uint32 m_allocated_receivers;
  NdbReceiver** m_receivers;      // All receivers

  Uint32* m_prepared_receivers;   // These are to be sent

  /*
    Owned by API/user thread.

    These receivers, stored in the m_api_receivers array, have all attributes
    from the current batch fully received, and the API thread has moved them
    here (under mutex protection) from m_conf_receivers, so that all further
    nextResult() can access them without extra mutex contention.

    The m_current_api_receiver member is the index (into m_api_receivers) of
    the receiver that delivered the last row to the application in
    nextResult(). If no rows have been delivered yet, it is set to 0 for table
    scans and to one past the end of the array for ordered index scans.

    For ordered index scans, the m_api_receivers array is further kept sorted.
    The entries from (m_current_api_receiver+1) to the end of the array are
    kept in the order that their first row will be returned in nextResult().

    Note also that for table scans, the entries available to the API thread
    are stored in entries 0..(m_api_receivers_count-1), while for ordered
    index scans, they are stored in entries m_current_api_receiver..array end.
   */
  Uint32 m_current_api_receiver;
  Uint32 m_api_receivers_count;
  NdbReceiver** m_api_receivers;  // These are currently used by api
  
  /*
    Shared by receiver thread and API thread.
    These are receivers that the receiver thread has obtained all attribute
    data for (of the current batch).
    API thread will move them (under mutex protection) to m_api_receivers on
    first access with nextResult().
   */
  Uint32 m_conf_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_conf_receivers; // receive thread puts them here
  
  /*
   Owned by receiver thread
   These are the receivers that the receiver thread is currently receiving
   attribute data for (of the current batch).
   Once all is received, they will be moved to m_conf_receivers.
  */
  Uint32 m_sent_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_sent_receivers; // receive thread puts them here
  
  int send_next_scan(Uint32 cnt, bool close);
  void receiver_delivered(NdbReceiver*);
  void receiver_completed(NdbReceiver*);
  void execCLOSE_SCAN_REP();

  int getKeyFromKEYINFO20(Uint32* data, Uint32 & size);
  NdbOperation*	takeOverScanOp(OperationType opType, NdbTransaction*);
  NdbOperation* takeOverScanOpNdbRecord(OperationType opType,
                                        NdbTransaction* pTrans,
                                        const NdbRecord *record,
                                        char *row,
                                        const unsigned char *mask,
                                        const NdbOperation::OperationOptions *opts,
                                        Uint32 sizeOfOptions);
  bool m_ordered;
  bool m_descending;
  Uint32 m_read_range_no;
  /*
    m_curr_row: Pointer to last returned row (linked list of NdbRecAttr
    objects).
    First comes keyInfo, if requested (explicitly with SF_KeyInfo, or
    implicitly when using LM_Exclusive).
    Then comes range_no, if requested with SF_ReadRangeNo, included first in
    the list of sort columns to get sorting of multiple range scans right.
    Then the 'real' columns that are participating in the scan.    
  */
  NdbRecAttr *m_curr_row;
  bool m_multi_range; // Mark if operation is part of multi-range scan
  bool m_executed; // Marker if operation should be released at close

  /* Buffer given to NdbReceivers for batch of rows received 
     during NdbRecord scans, or NULL. Buffer is chunked up
     to construct several NdbReceiverBuffer, but is allocated
     as a single chunk from the NdbScanOperation
  */
  Uint32 *m_scan_buffer;
  
  /* Initialise scan operation with user provided information */
  virtual int processTableScanDefs(LockMode lock_mode, 
                                   Uint32 scan_flags, 
                                   Uint32 parallel,
                                   Uint32 batch);

  /* This flag indicates whether a scan operation is using the old API */
  bool  m_scanUsingOldApi;

  /* Whether readTuples has been called - only valid for old Api scans */
  bool m_readTuplesCalled;

  /* Scan definition information saved by RecAttr scan API */
  LockMode m_savedLockModeOldApi;
  Uint32 m_savedScanFlagsOldApi;
  Uint32 m_savedParallelOldApi;
  Uint32 m_savedBatchOldApi;

  /* NdbInterpretedCode object owned by ScanOperation to support
   * old NdbScanFilter Api
   */
  NdbInterpretedCode* m_interpretedCodeOldApi;

  enum ScanPruningState {
    SPS_UNKNOWN,           // Initial state
    SPS_FIXED,             // Explicit partitionId passed in ScanOptions
    SPS_ONE_PARTITION,     // Scan pruned to one partition by previous range
    SPS_MULTI_PARTITION    // Scan cannot be pruned due to previous ranges
  };
  
  ScanPruningState m_pruneState;
  Uint32 m_pruningKey;  // Can be distr key hash or actual partition id.

  /**
   * This flag indicates whether a scan operation was 
   * succesfully finalised
   */
  bool  m_scanFinalisedOk;
private:
  NdbScanOperation(const NdbScanOperation&); // Not impl.
  NdbScanOperation&operator=(const NdbScanOperation&);

  /**
   * Const variants overloaded...calling NdbOperation::getBlobHandle()
   *  (const NdbOperation::getBlobHandle implementation
   *   only returns existing Blob operations)
   *
   * I'm not sure...but these doesn't seem to be an users of this...
   * so I make them private...
   */
  virtual NdbBlob* getBlobHandle(const char* anAttrName) const;
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId) const;
};

inline
NdbOperation* 
NdbScanOperation::lockCurrentTuple(){
  return lockCurrentTuple(m_transConnection);
}

inline
NdbOperation* 
NdbScanOperation::lockCurrentTuple(NdbTransaction* takeOverTrans){
  return takeOverScanOp(NdbOperation::ReadRequest, 
			takeOverTrans);
}

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(){
  return updateCurrentTuple(m_transConnection);
}

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(NdbTransaction* takeOverTrans){
  return takeOverScanOp(NdbOperation::UpdateRequest, 
			takeOverTrans);
}

inline
int
NdbScanOperation::deleteCurrentTuple(){
  return deleteCurrentTuple(m_transConnection);
}

inline
int
NdbScanOperation::deleteCurrentTuple(NdbTransaction * takeOverTrans){
  void * res = takeOverScanOp(NdbOperation::DeleteRequest, 
			      takeOverTrans);
  if(res == 0)
    return -1;
  return 0;
}

inline
const NdbOperation *
NdbScanOperation::updateCurrentTuple(NdbTransaction *takeOverTrans,
                                     const NdbRecord *attr_rec,
                                     const char *attr_row,
                                     const unsigned char *mask,
                                     const NdbOperation::OperationOptions *opts,
                                     Uint32 sizeOfOptions)
{
  /*
    We share the code implementing lockCurrentTuple() and updateCurrentTuple().
    For lock the row may be updated, for update it is const.
    Therefore we need to cast away const here, though we won't actually change
    the row since we pass type 'UpdateRequest'.
   */
  return takeOverScanOpNdbRecord(NdbOperation::UpdateRequest, takeOverTrans,
                                 attr_rec, (char *)attr_row, mask,
                                 opts, sizeOfOptions);
}

inline
const NdbOperation *
NdbScanOperation::deleteCurrentTuple(NdbTransaction *takeOverTrans,
                                     const NdbRecord *result_rec,
                                     char *result_row,
                                     const unsigned char *result_mask,
                                     const NdbOperation::OperationOptions *opts,
                                     Uint32 sizeOfOptions)
{
  return takeOverScanOpNdbRecord(NdbOperation::DeleteRequest, takeOverTrans,
                                 result_rec, result_row, result_mask,
                                 opts, sizeOfOptions);
}

inline
NdbTransaction*
NdbScanOperation::getNdbTransaction() const
{
  /* return the user visible transaction object ptr, not the
   * scan's 'internal' / buddy transaction object
   */
  return m_transConnection;
}

#endif
