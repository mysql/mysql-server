/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbBlob_H
#define NdbBlob_H

#include <ndb_types.h>
#include "NdbDictionary.hpp"
#include "NdbError.hpp"
#include "NdbTransaction.hpp"

class Ndb;
class NdbTransaction;
class NdbOperation;
class NdbRecAttr;
class NdbTableImpl;
class NdbColumnImpl;
class NdbEventOperationImpl;

/**
 * @class NdbBlob
 * @brief Blob handle
 *
 * Blob data is stored in 2 places:
 *
 * - "header" and "inline bytes" stored in the blob attribute
 * - "blob parts" stored in a separate table NDB$BLOB_<tid>_<cid>
 *
 * Inline and part sizes can be set via NdbDictionary::Column methods
 * when the table is created.
 *
 * NdbBlob is a blob handle.  To access blob data, the handle must be
 * created using NdbOperation::getBlobHandle in operation prepare phase.
 * The handle has following states:
 *
 * - prepared: before the operation is executed
 * - active: after execute or next result but before transaction commit
 * - closed: after blob handle is closed or after transaction commit
 * - invalid: after rollback or transaction close
 *
 * NdbBlob supports 3 styles of data access:
 *
 * - in prepare phase, NdbBlob methods getValue and setValue are used to
 *   prepare a read or write of a blob value of known size
 *
 * - in prepare phase, setActiveHook is used to define a routine which
 *   is invoked as soon as the handle becomes active
 *
 * - in active phase, readData and writeData are used to read or write
 *   blob data of arbitrary size
 *
 * The styles can be applied in combination (in above order).
 *
 * Blob operations take effect at next transaction execute.  In some
 * cases NdbBlob is forced to do implicit executes.  To avoid this,
 * operate on complete blob parts.
 *
 * Use NdbTransaction::executePendingBlobOps to flush your reads and
 * writes.  It avoids execute penalty if nothing is pending.  It is not
 * needed after execute (obviously) or after next scan result.
 *
 * NdbBlob also supports reading post or pre blob data from events.  The
 * handle can be read after next event on main table has been retrieved.
 * The data is available immediately.  See NdbEventOperation.
 *
 * Non-void NdbBlob methods return -1 on error and 0 on success.  Output
 * parameters are used when necessary.
 *
 * Usage notes for different operation types:
 *
 * - insertTuple must be followed by a setValue() call for every non
 *   nullable blob in the row.
 *
 * - readTuple or scan readTuples with lock mode LM_CommittedRead is
 *   temporarily upgraded to lock mode LM_Read if any blob attributes
 *   are accessed (to guarantee consistent view).  After the Blob
 *   handle is closed, the LM_Read lock is removed on the next
 *   execute() call.
 *
 * - readTuple (with any lock mode) can only read blob value
 *
 * - updateTuple can either overwrite existing value with setValue or
 *   update it in active phase
 *
 * - writeTuple always overwrites blob value and must use setValue if
 *   blob attribute is non-nullable
 *
 * - deleteTuple creates implicit non-accessible blob handles
 *
 * - scan readTuples (any lock mode) can use its blob handles only
 *   to read blob value
 *
 * - scan readTuples with lock mode LM_Exclusive can update row and blob
 *   value using updateCurrentTuple, where the operation returned must
 *   create its own blob handles explicitly
 *
 * - scan readTuples with lock mode LM_Exclusive can delete row (and
 *   therefore blob values) using deleteCurrentTuple, which creates
 *   implicit non-accessible blob handles
 *
 * - the operation returned by lockCurrentTuple cannot update blob value
 *
 * Bugs / limitations:
 *
 * - too many pending blob ops can blow up i/o buffers
 *
 * - table and its blob part tables are not created atomically
 */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
 * - there is no support for an asynchronous interface
 */
#endif

class NdbBlob {
 public:
  /**
   * State.
   */
  enum State { Idle = 0, Prepared = 1, Active = 2, Closed = 3, Invalid = 9 };
  /**
   * Get the state of a NdbBlob object.
   */
  State getState();
  /**
   * Returns -1 for normal statement based blob and 0/1 for event
   * operation post/pre data blob.  Always succeeds.
   */
  void getVersion(int &version);
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Blob head V1 is 8 bytes:
   *   8 bytes blob length - native endian (of ndb apis)
   *
   * Blob head V2 is 16 bytes:
   *   2 bytes head+inline length bytes (MEDIUM_VAR) - little-endian
   *   2 bytes reserved (zero)
   *   4 bytes NDB$PKID for blob events - little-endian
   *   8 bytes blob length - litte-endian
   *
   * Following struct is for packing/unpacking the fields.  It must
   * not be C-cast to/from the head+inline attribute value.
   */
  struct Head {
    Uint16 varsize;   // length of head+inline minus the 2 length bytes
    Uint16 reserved;  // must be 0  wl3717_todo checksum?
    Uint32 pkid;      // connects part and row with same PK within tx
    Uint64 length;    // blob length
    //
    Uint32 headsize;  // for convenience, number of bytes in head
    Head() : varsize(0), reserved(0), pkid(0), length(0), headsize(0) {}
  };
  static void packBlobHead(const Head &head, char *buf, int blobVersion);
  static void unpackBlobHead(Head &head, const char *buf, int blobVersion);
#endif
  /**
   * Prepare to read blob value.  The value is available after execute.
   * Use getNull() to check for NULL and getLength() to get the real length
   * and to check for truncation.  Sets current read/write position to
   * after the data read.
   */
  int getValue(void *data, Uint32 bytes);
  /**
   * Prepare to insert or update blob value.  An existing longer blob
   * value will be truncated.  The data buffer must remain valid until
   * execute.  Sets current read/write position to after the data.  Set
   * data to null pointer (0) to create a NULL value.
   */
  int setValue(const void *data, Uint32 bytes);
  /**
   * Callback for setActiveHook().  Invoked immediately when the prepared
   * operation has been executed (but not committed).  Any getValue() or
   * setValue() is done first.  The blob handle is active so readData or
   * writeData() etc can be used to manipulate blob value.  A user-defined
   * argument is passed along.  Returns non-zero on error.
   */
  typedef int ActiveHook(NdbBlob *me, void *arg);
  /**
   * Define callback for blob handle activation.  The queue of prepared
   * operations will be executed in no commit mode up to this point and
   * then the callback is invoked.
   */
  int setActiveHook(ActiveHook *activeHook, void *arg);
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int getDefined(int &isNull);
  int getNull(bool &isNull);
#endif
  /**
   * Return -1, 0, 1 if blob is undefined, non-null, or null.  For
   * non-event blob, undefined causes a state error.
   */
  int getNull(int &isNull);
  /**
   * Set blob to NULL.
   */
  int setNull();
  /**
   * Get current length in bytes.  Use getNull to distinguish between
   * length 0 blob and NULL blob.
   */
  int getLength(Uint64 &length);
  /**
   * Truncate blob to given length.  Has no effect if the length is
   * larger than current length.
   */
  int truncate(Uint64 length = 0);
  /**
   * Get current read/write position.
   */
  int getPos(Uint64 &pos);
  /**
   * Set read/write position.  Must be between 0 and current length.
   * "Sparse blobs" are not supported.
   */
  int setPos(Uint64 pos);
  /**
   * Read at current position and set new position to first byte after
   * the data read.  A read past blob end returns actual number of bytes
   * read in the in/out bytes parameter.
   */
  int readData(void *data, Uint32 &bytes);
  /**
   * Write at current position and set new position to first byte after
   * the data written.  A write past blob end extends the blob value.
   */
  int writeData(const void *data, Uint32 bytes);
  /**
   * Return the blob column.
   */
  const NdbDictionary::Column *getColumn();
  /**
   * Get blob parts table name.  Useful only to test programs.
   */
  static int getBlobTableName(char *btname, Ndb *anNdb, const char *tableName,
                              const char *columnName);
  /**
   * Get blob event name.  The blob event is created if the main event
   * monitors the blob column.  The name includes main event name.
   */
  static int getBlobEventName(char *bename, Ndb *anNdb, const char *eventName,
                              const char *columnName);
  /**
   * Return error object.  The error may be blob specific or may be
   * copied from a failed implicit operation.
   *
   * The error code is copied back to the operation unless the operation
   * already has a non-zero error code.
   */
  const NdbError &getNdbError() const;
  /**
   * Get a pointer to the operation which this Blob Handle
   * was initially created as part of.
   * Note that this could be a scan operation.
   * Note that the pointer returned is a const pointer.
   */
  const NdbOperation *getNdbOperation() const;
  /**
   * Return info about all blobs in this operation.
   *
   * Get first blob in list.
   */
  NdbBlob *blobsFirstBlob();
  /**
   * Return info about all blobs in this operation.
   *
   * Get next blob in list. Initialize with blobsFirstBlob().
   */
  NdbBlob *blobsNextBlob();
  /**
   * Close the BlobHandle
   *
   * The BlobHandle can be closed to release internal
   * resources before transaction commit / abort time.
   *
   * The close method can only be called when the Blob is in
   * Active state.
   *
   * If execPendingBlobOps = true then pending Blob operations
   * will be flushed before the Blob handle is closed.
   * If execPendingBlobOps = false then the Blob handle must
   * have no pending read or write operations.
   *
   * Read operations and locks
   *
   * Where a Blob handle is created on a read operation using
   * lockmode LM_Read or LM_Exclusive, the read operation can
   * only be unlocked after all Blob handles created on the
   * operation are closed.
   *
   * Where a row containing Blobs has been read with lockmode
   * LM_CommittedRead, the lockmode is automatically upgraded to
   * LM_Read to ensure consistency.
   * In this case, when all the BlobHandles for the row have been
   * close()d, an unlock operation for the row is automatically
   * issued by the close() call, adding a pending 'write' operation
   * to the Blob.
   * After the next execute() call, the upgraded lock is released.
   */
  int close(bool execPendingBlobOps = true);

 private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbTransaction;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbDictionaryImpl;
  friend class NdbResultSet;  // atNextResult
  friend class NdbEventBuffer;
  friend class NdbEventOperationImpl;
  friend class NdbReceiver;
  friend class NdbImportImpl;
#endif

  /**
   * BlobTask
   *
   * Encapsulated state for some task requested to be performed
   * on a Blob
   */
  class BlobTask {
   public:
    enum State {
      BTS_INIT,
      BTS_READ_HEAD,
      BTS_READ_PARTS,
      BTS_READ_LAST_PART,
      BTS_WRITE_HEAD,
      BTS_WRITE_PARTS,
      BTS_DONE
    } m_state;

    char *m_readBuffer;
    Uint64 m_readBufferLen;

    Uint16 m_lastPartLen;

    const char *m_writeBuffer;
    Uint64 m_writeBufferLen;

    Uint64 m_oldLen;
    Uint64 m_position;

    NdbOperation *m_lastDeleteOp;

#ifndef BUG_31546136_FIXED
    bool m_delayedWriteHead;
#endif

    BlobTask()
        : m_state(BTS_INIT),
          m_readBuffer(nullptr),
          m_readBufferLen(0),
          m_lastPartLen(0),
          m_writeBuffer(nullptr),
          m_writeBufferLen(0),
          m_oldLen(0),
          m_position(0),
          m_lastDeleteOp(nullptr)
#ifndef BUG_31546136_FIXED
          ,
          m_delayedWriteHead(false)
#endif
    {
    }
  } m_blobOp;

  int theBlobVersion;
  /*
   * Disk data does not yet support Var* attrs.  In both V1 and V2,
   * if the primary table blob attr is specified as disk attr then:
   * - the primary table blob attr remains a memory attr
   * - the blob parts "DATA" attr becomes a disk attr
   * - the blob parts "DATA" attr is fixed size
   * Use following flag.  It is always set for V1.
   */
  bool theFixedDataFlag;
  Uint32 theHeadSize;
  Uint32 theVarsizeBytes;
  // state
  State theState;
  // True if theNdbOp is using NdbRecord, false if NdbRecAttr.
  bool theNdbRecordFlag;
  void setState(State newState);
  // quick and dirty support for events (consider subclassing)
  int theEventBlobVersion;  // -1=data op 0=post event 1=pre event
  // define blob table
  static void getBlobTableName(char *btname, const NdbTableImpl *t,
                               const NdbColumnImpl *c);
  static int getBlobTable(NdbTableImpl &bt, const NdbTableImpl *t,
                          const NdbColumnImpl *c, struct NdbError &error);
  static void getBlobEventName(char *bename, const NdbEventImpl *e,
                               const NdbColumnImpl *c);
  static void getBlobEvent(NdbEventImpl &be, const NdbEventImpl *e,
                           const NdbColumnImpl *c);
  // compute blob table column number for faster access
  enum {
    BtColumnPk = 0,   /* V1 only */
    BtColumnDist = 1, /* if stripe size != 0 */
    BtColumnPart = 2,
    BtColumnPkid = 3, /* V2 only */
    BtColumnData = 4
  };
  int theBtColumnNo[5];
  // ndb api stuff
  Ndb *theNdb;
  NdbTransaction *theNdbCon;
  NdbOperation *theNdbOp;
  NdbEventOperationImpl *theEventOp;
  NdbEventOperationImpl *theBlobEventOp;
  NdbRecAttr *theBlobEventPkRecAttr;
  NdbRecAttr *theBlobEventDistRecAttr;
  NdbRecAttr *theBlobEventPartRecAttr;
  NdbRecAttr *theBlobEventPkidRecAttr;
  NdbRecAttr *theBlobEventDataRecAttr;
  const NdbTableImpl *theTable;
  const NdbTableImpl *theAccessTable;
  const NdbTableImpl *theBlobTable;
  const NdbColumnImpl *theColumn;
  unsigned char theFillChar;
  // sizes
  Uint32 theInlineSize;
  Uint32 thePartSize;
  Uint32 theStripeSize;
  // getValue/setValue
  bool theGetFlag;
  char *theGetBuf;
  bool theSetFlag;
  bool theSetValueInPreExecFlag;
  const char *theSetBuf;
  Uint32 theGetSetBytes;
  // pending ops
  Uint8 thePendingBlobOps;
  // activation callback
  ActiveHook *theActiveHook;
  void *theActiveHookArg;
  // buffers
  struct Buf {
    char *data;
    unsigned size;
    unsigned maxsize;
    Buf();
    ~Buf();
    void alloc(unsigned n);
    void release();
    void zerorest();
    void copyfrom(const Buf &src);
  };
  Buf theKeyBuf;
  Buf theAccessKeyBuf;
  Buf thePackKeyBuf;
  Buf theHeadInlineBuf;
  Buf theHeadInlineCopyBuf;  // for writeTuple
  Buf thePartBuf;
  Uint16 thePartLen;
  Buf theBlobEventDataBuf;
  Uint32 theBlobEventDistValue;
  Uint32 theBlobEventPartValue;
  Uint32 theBlobEventPkidValue;
  Head theHead;
  char *theInlineData;
  NdbRecAttr *theHeadInlineRecAttr;
  NdbOperation *theHeadInlineReadOp;
  bool theHeadInlineUpdateFlag;
  // partition id for data events
  bool userDefinedPartitioning;
  Uint32 noPartitionId() { return ~(Uint32)0; }
  Uint32 thePartitionId;
  NdbRecAttr *thePartitionIdRecAttr;
  // length and read/write position
  int theNullFlag;
  Uint64 theLength;
  Uint64 thePos;
  // errors
  // Allow update error from const methods.
  mutable NdbError theError;
  // for keeping in lists
  NdbBlob *theNext;

  /* For key hashing */
  bool m_keyHashSet;
  Uint32 m_keyHash;
  NdbBlob *m_keyHashNext;

  typedef enum blobAction {
    BA_ERROR = -1, /* A fatal error */
    BA_DONE = 0,   /* All operations defined */
    BA_EXEC = 1,   /* Execute needed and then more work */
  } BlobAction;

  // initialization
  NdbBlob(Ndb *);
  void init();
  void release();
  // classify operations
  bool isTableOp();
  bool isIndexOp();
  bool isKeyOp();
  bool isReadOp();
  bool isInsertOp();
  bool isUpdateOp();
  bool isWriteOp();
  bool isDeleteOp();
  bool isScanOp();
  bool isReadOnlyOp();
  bool isTakeOverOp();
  // computations
  Uint32 getPartNumber(Uint64 pos);
  Uint32 getPartOffset(Uint64 pos);
  Uint32 getPartCount();
  Uint32 getDistKey(Uint32 part);
  // pack / unpack
  int packKeyValue(const NdbTableImpl *aTable, const Buf &srcBuf);
  int unpackKeyValue(const NdbTableImpl *aTable, Buf &dstBuf);
  int copyKeyFromRow(const NdbRecord *record, const char *row, Buf &packedBuf,
                     Buf &unpackedBuf);
  Uint32 getHeadInlineSize() { return theHeadSize + theInlineSize; }
  void prepareSetHeadInlineValue();
  void getNullOrEmptyBlobHeadDataPtr(const char *&data, Uint32 &byteSize);
  // getters and setters
  void packBlobHead();
  void unpackBlobHead();
  int getTableKeyValue(NdbOperation *anOp);
  int setTableKeyValue(NdbOperation *anOp);
  int setAccessKeyValue(NdbOperation *anOp);
  int setDistKeyValue(NdbOperation *anOp, Uint32 part);
  int setPartKeyValue(NdbOperation *anOp, Uint32 part);
  int setPartPkidValue(NdbOperation *anOp, Uint32 pkid);
  int getPartDataValue(NdbOperation *anOp, char *buf, Uint16 *aLenLoc);
  int setPartDataValue(NdbOperation *anOp, const char *buf, const Uint16 &aLen);
  int getHeadInlineValue(NdbOperation *anOp);
  void getHeadFromRecAttr();
  int setHeadInlineValue(NdbOperation *anOp);
  void setHeadPartitionId(NdbOperation *anOp);
  void setPartPartitionId(NdbOperation *anOp);

  // Blob async tasks
  int initBlobTask(NdbTransaction::ExecType anExecType);
  NdbBlob::BlobAction handleBlobTask(NdbTransaction::ExecType anExecType);

  // data operations
  int readDataPrivate(char *buf, Uint32 &bytes);
  int writeDataPrivate(const char *buf, Uint32 bytes);
  int readParts(char *buf, Uint32 part, Uint32 count);
  int readPart(char *buf, Uint32 part, Uint16 &len);
  int readTableParts(char *buf, Uint32 part, Uint32 count);
  int readTablePart(char *buf, Uint32 part, Uint16 &len);
  int readEventParts(char *buf, Uint32 part, Uint32 count);
  int readEventPart(char *buf, Uint32 part, Uint16 &len);
  int insertParts(const char *buf, Uint32 part, Uint32 count);
  int insertPart(const char *buf, Uint32 part, const Uint16 &len);
  int updateParts(const char *buf, Uint32 part, Uint32 count);
  int updatePart(const char *buf, Uint32 part, const Uint16 &len);
  int deletePartsThrottled(Uint32 part, Uint32 count);
  int deleteParts(Uint32 part, Uint32 count);
  int deletePartsUnknown(Uint32 part);
  int writePart(const char *buf, Uint32 part, const Uint16 &len);
  // pending ops
  int executePendingBlobReads();
  int executePendingBlobWrites();
  // callbacks
  int invokeActiveHook();
  // blob handle maintenance
  int atPrepare(NdbTransaction *aCon, NdbOperation *anOp,
                const NdbColumnImpl *aColumn);
  int atPrepareNdbRecord(NdbTransaction *aCon, NdbOperation *anOp,
                         const NdbColumnImpl *aColumn,
                         const NdbRecord *key_record, const char *key_row);
  int atPrepareNdbRecordTakeover(NdbTransaction *aCon, NdbOperation *anOp,
                                 const NdbColumnImpl *aColumn,
                                 const char *keyinfo, Uint32 keyinfo_bytes);
  int atPrepareNdbRecordScan(NdbTransaction *aCon, NdbOperation *anOp,
                             const NdbColumnImpl *aColumn);
  int atPrepareCommon(NdbTransaction *aCon, NdbOperation *anOp,
                      const NdbColumnImpl *aColumn);
  int atPrepare(NdbEventOperationImpl *anOp, NdbEventOperationImpl *aBlobOp,
                const NdbColumnImpl *aColumn, int version);
  int prepareColumn();
  BlobAction preExecute(NdbTransaction::ExecType anExecType);
  BlobAction postExecute(NdbTransaction::ExecType anExecType);
  int preCommit();
  int atNextResult();
  int atNextResultNdbRecord(const char *keyinfo, Uint32 keyinfo_bytes);
  int atNextResultCommon();
  int atNextEvent();
  // errors
  void setErrorCode(int anErrorCode, bool invalidFlag = false);
  void setErrorCode(NdbOperation *anOp, bool invalidFlag = false);
  void setErrorCode(NdbEventOperationImpl *anOp, bool invalidFlag = false);
  // list stuff
  void next(NdbBlob *obj) { theNext = obj; }
  NdbBlob *next() { return theNext; }

  /**
   * Batching support
   *
   * Use operation types and operation key info to decide
   * whether operations can execute concurrently in a batch
   */
  typedef enum opTypes {
    OT_READ = 1 << 0,
    OT_INSERT = 1 << 1,
    OT_UPDATE = 1 << 2,
    OT_WRITE = 1 << 3,
    OT_DELETE = 1 << 4
  } OpTypes;
  Uint32 getOpType();  // Not const as used methods !const
  static bool isOpTypeSafeWithBatch(const Uint32 batchOpTypes,
                                    const Uint32 newOpType);

  // Key compare
  /* Returns 0 if different, 1 if same, - otherwise */
  int isBlobOnDifferentKey(const NdbBlob *other);

  Uint32 getBlobKeyHash();
  int getBlobKeysEqual(NdbBlob *other);
  void setBlobHashNext(NdbBlob *next);
  NdbBlob *getBlobHashNext() const;

  friend class BlobBatchChecker;

  friend struct Ndb_free_list_t<NdbBlob>;

  NdbBlob(const NdbBlob &);  // Not impl.
  NdbBlob &operator=(const NdbBlob &);
};

#endif
