/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbBlob_H
#define NdbBlob_H

#include <ndb_types.h>
#include <NdbDictionary.hpp>
#include <NdbConnection.hpp>
#include <NdbError.hpp>

class Ndb;
class NdbConnection;
class NdbOperation;
class NdbRecAttr;
class NdbTableImpl;
class NdbColumnImpl;

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
 * - closed: after transaction commit
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
 * Use NdbConnection::executePendingBlobOps to flush your reads and
 * writes.  It avoids execute penalty if nothing is pending.  It is not
 * needed after execute (obviously) or after next scan result.
 *
 * NdbBlob methods return -1 on error and 0 on success, and use output
 * parameters when necessary.
 *
 * Operation types:
 * - insertTuple must use setValue if blob column is non-nullable
 * - readTuple with exclusive lock can also update existing value
 * - updateTuple can overwrite with setValue or update existing value
 * - writeTuple always overwrites and must use setValue if non-nullable
 * - deleteTuple creates implicit non-accessible blob handles
 * - scan with exclusive lock can also update existing value
 * - scan "lock takeover" update op must do its own getBlobHandle
 *
 * Bugs / limitations:
 * - lock mode upgrade should be handled automatically
 * - lock mode vs allowed operation is not checked
 * - too many pending blob ops can blow up i/o buffers
 * - table and its blob part tables are not created atomically
 * - there is no support for an asynchronous interface
 */
class NdbBlob {
public:
  /**
   * State.
   */
  enum State {
    Idle = 0,
    Prepared = 1,
    Active = 2,
    Closed = 3,
    Invalid = 9
  };
  State getState();
  /**
   * Inline blob header.
   */
  struct Head {
    Uint64 length;
  };
  /**
   * Prepare to read blob value.  The value is available after execute.
   * Use getNull to check for NULL and getLength to get the real length
   * and to check for truncation.  Sets current read/write position to
   * after the data read.
   */
  int getValue(void* data, Uint32 bytes);
  /**
   * Prepare to insert or update blob value.  An existing longer blob
   * value will be truncated.  The data buffer must remain valid until
   * execute.  Sets current read/write position to after the data.  Set
   * data to null pointer (0) to create a NULL value.
   */
  int setValue(const void* data, Uint32 bytes);
  /**
   * Callback for setActiveHook.  Invoked immediately when the prepared
   * operation has been executed (but not committed).  Any getValue or
   * setValue is done first.  The blob handle is active so readData or
   * writeData etc can be used to manipulate blob value.  A user-defined
   * argument is passed along.  Returns non-zero on error.
   */
  typedef int ActiveHook(NdbBlob* me, void* arg);
  /**
   * Define callback for blob handle activation.  The queue of prepared
   * operations will be executed in no commit mode up to this point and
   * then the callback is invoked.
   */
  int setActiveHook(ActiveHook* activeHook, void* arg);
  /**
   * Check if blob is null.
   */
  int getNull(bool& isNull);
  /**
   * Set blob to NULL.
   */
  int setNull();
  /**
   * Get current length in bytes.  Use getNull to distinguish between
   * length 0 blob and NULL blob.
   */
  int getLength(Uint64& length);
  /**
   * Truncate blob to given length.  Has no effect if the length is
   * larger than current length.
   */
  int truncate(Uint64 length = 0);
  /**
   * Get current read/write position.
   */
  int getPos(Uint64& pos);
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
  int readData(void* data, Uint32& bytes);
  /**
   * Write at current position and set new position to first byte after
   * the data written.  A write past blob end extends the blob value.
   */
  int writeData(const void* data, Uint32 bytes);
  /**
   * Return the blob column.
   */
  const NdbDictionary::Column* getColumn();
  /**
   * Get blob parts table name.  Useful only to test programs.
   */
  STATIC_CONST( BlobTableNameSize = 40 );
  static int getBlobTableName(char* btname, Ndb* anNdb, const char* tableName, const char* columnName);
  /**
   * Return error object.  The error may be blob specific (below) or may
   * be copied from a failed implicit operation.
   */
  const NdbError& getNdbError() const;
  // "Invalid blob attributes or invalid blob parts table"
  STATIC_CONST( ErrTable = 4263 );
  // "Invalid usage of blob attribute" 
  STATIC_CONST( ErrUsage = 4264 );
  // "Method is not valid in current blob state"
  STATIC_CONST( ErrState = 4265 );
  // "Invalid blob seek position"
  STATIC_CONST( ErrSeek = 4266 );
  // "Corrupted blob value"
  STATIC_CONST( ErrCorrupt = 4267 );
  // "Error in blob head update forced rollback of transaction"
  STATIC_CONST( ErrAbort = 4268 );
  // "Unknown blob error"
  STATIC_CONST( ErrUnknown = 4269 );
  /**
   * Return info about all blobs in this operation.
   */
  // Get first blob in list
  NdbBlob* blobsFirstBlob();
  // Get next blob in list after this one
  NdbBlob* blobsNextBlob();

private:
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbDictionaryImpl;
  friend class NdbResultSet; // atNextResult
  // state
  State theState;
  void setState(State newState);
  // define blob table
  static void getBlobTableName(char* btname, const NdbTableImpl* t, const NdbColumnImpl* c);
  static void getBlobTable(NdbTableImpl& bt, const NdbTableImpl* t, const NdbColumnImpl* c);
  // ndb api stuff
  Ndb* theNdb;
  NdbConnection* theNdbCon;
  NdbOperation* theNdbOp;
  const NdbTableImpl* theTable;
  const NdbTableImpl* theAccessTable;
  const NdbTableImpl* theBlobTable;
  const NdbColumnImpl* theColumn;
  char theFillChar;
  // sizes
  Uint32 theInlineSize;
  Uint32 thePartSize;
  Uint32 theStripeSize;
  // getValue/setValue
  bool theGetFlag;
  char* theGetBuf;
  bool theSetFlag;
  const char* theSetBuf;
  Uint32 theGetSetBytes;
  // pending ops
  Uint8 thePendingBlobOps;
  // activation callback
  ActiveHook* theActiveHook;
  void* theActiveHookArg;
  // buffers
  struct Buf {
    char* data;
    unsigned size;
    unsigned maxsize;
    Buf();
    ~Buf();
    void alloc(unsigned n);
    void copyfrom(const Buf& src);
  };
  Buf theKeyBuf;
  Buf theAccessKeyBuf;
  Buf theHeadInlineBuf;
  Buf theHeadInlineCopyBuf;     // for writeTuple
  Buf thePartBuf;
  Head* theHead;
  char* theInlineData;
  NdbRecAttr* theHeadInlineRecAttr;
  NdbOperation* theHeadInlineReadOp;
  bool theHeadInlineUpdateFlag;
  // length and read/write position
  int theNullFlag;
  Uint64 theLength;
  Uint64 thePos;
  // errors
  NdbError theError;
  // for keeping in lists
  NdbBlob* theNext;
  // initialization
  NdbBlob();
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
  // computations
  Uint32 getPartNumber(Uint64 pos);
  Uint32 getPartCount();
  Uint32 getDistKey(Uint32 part);
  // getters and setters
  int getTableKeyValue(NdbOperation* anOp);
  int setTableKeyValue(NdbOperation* anOp);
  int setAccessKeyValue(NdbOperation* anOp);
  int setPartKeyValue(NdbOperation* anOp, Uint32 part);
  int getHeadInlineValue(NdbOperation* anOp);
  void getHeadFromRecAttr();
  int setHeadInlineValue(NdbOperation* anOp);
  // data operations
  int readDataPrivate(char* buf, Uint32& bytes);
  int writeDataPrivate(const char* buf, Uint32 bytes);
  int readParts(char* buf, Uint32 part, Uint32 count);
  int insertParts(const char* buf, Uint32 part, Uint32 count);
  int updateParts(const char* buf, Uint32 part, Uint32 count);
  int deleteParts(Uint32 part, Uint32 count);
  int deletePartsUnknown(Uint32 part);
  // pending ops
  int executePendingBlobReads();
  int executePendingBlobWrites();
  // callbacks
  int invokeActiveHook();
  // blob handle maintenance
  int atPrepare(NdbConnection* aCon, NdbOperation* anOp, const NdbColumnImpl* aColumn);
  int preExecute(ExecType anExecType, bool& batch);
  int postExecute(ExecType anExecType);
  int preCommit();
  int atNextResult();
  // errors
  void setErrorCode(int anErrorCode, bool invalidFlag = true);
  void setErrorCode(NdbOperation* anOp, bool invalidFlag = true);
  void setErrorCode(NdbConnection* aCon, bool invalidFlag = true);
#ifdef VM_TRACE
  int getOperationType() const;
  friend class NdbOut& operator<<(NdbOut&, const NdbBlob&);
#endif
};

#endif
