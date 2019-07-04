/*
   Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"
#include <signaldata/TcKeyReq.hpp>
#include <NdbEnv.h>
#include <ndb_version.h>

/*
 * Reading index table directly (as a table) is faster but there are
 * bugs or limitations.  Keep the code and make possible to choose.
 */
static const bool g_ndb_blob_ok_to_read_index_table = false;

// get state

NdbBlob::State
NdbBlob::getState()
{
  return theState;
}

void
NdbBlob::getVersion(int& version)
{
  version = theEventBlobVersion;
}

// set state (inline)

inline void
NdbBlob::setState(State newState)
{
  DBUG_ENTER("NdbBlob::setState");
  DBUG_PRINT("info", ("this=%p newState=%u", this, newState));
  theState = newState;
  DBUG_VOID_RETURN;
}

// define blob table

int
NdbBlob::getBlobTableName(char* btname, Ndb* anNdb, const char* tableName, const char* columnName)
{
  DBUG_ENTER("NdbBlob::getBlobTableName");
  NdbTableImpl* t = anNdb->theDictionary->m_impl.getTable(tableName);
  if (t == NULL)
    DBUG_RETURN(-1);
  NdbColumnImpl* c = t->getColumn(columnName);
  if (c == NULL)
    DBUG_RETURN(-1);
  getBlobTableName(btname, t, c);
  DBUG_RETURN(0);
}

void
NdbBlob::getBlobTableName(char* btname, const NdbTableImpl* t, const NdbColumnImpl* c)
{
  DBUG_ENTER("NdbBlob::getBlobTableName");
  assert(t != 0 && c != 0 && c->getBlobType() && c->getPartSize() != 0);
  memset(btname, 0, NdbBlobImpl::BlobTableNameSize);
  sprintf(btname, "NDB$BLOB_%d_%d", (int)t->m_id, (int)c->m_column_no);
  DBUG_PRINT("info", ("blob table name: %s", btname));
  DBUG_VOID_RETURN;
}

int
NdbBlob::getBlobTable(NdbTableImpl& bt, const NdbTableImpl* t, const NdbColumnImpl* c, NdbError& error)
{
  DBUG_ENTER("NdbBlob::getBlobTable");
  const int blobVersion = c->getBlobVersion();
  assert(blobVersion == NDB_BLOB_V1 || blobVersion == NDB_BLOB_V2);
  char btname[NdbBlobImpl::BlobTableNameSize];
  getBlobTableName(btname, t, c);
  bt.setName(btname);
  bt.setLogging(t->getLogging());
  /*
    BLOB tables use the same fragmentation as the original table
    It also uses the same tablespaces and it never uses any range or
    list arrays.
  */
  bt.m_primaryTableId = t->m_id;
  bt.m_fd.clear();
  bt.m_range.clear();
  bt.setFragmentCount(t->getFragmentCount());
  bt.m_tablespace_id = t->m_tablespace_id;
  bt.m_tablespace_version = t->m_tablespace_version;
  bt.setFragmentType(t->getFragmentType());
  bt.setPartitionBalance(t->getPartitionBalance());
  bt.setReadBackupFlag(t->getReadBackupFlag());
  bt.setFullyReplicated(t->getFullyReplicated());

  if (t->getFragmentType() == NdbDictionary::Object::HashMapPartition)
  {
    bt.m_hash_map_id = t->m_hash_map_id;
    bt.m_hash_map_version = t->m_hash_map_version;
  }
  DBUG_PRINT("info", ("Define BLOB table V%d with"
                      " primary table = %u and Fragment Type = %u",
                      blobVersion,
                      bt.m_primaryTableId, (uint)bt.getFragmentType()));
  if (unlikely(blobVersion == NDB_BLOB_V1)) {
    /*
     * Stripe size 0 in V1 does not work as intended.
     * No point to add support for it now.
     */
    if (c->getStripeSize() == 0) {
      error.code = NdbBlobImpl::ErrTable;
      DBUG_RETURN(-1);
    }
    { NdbDictionary::Column bc("PK");
      bc.setType(NdbDictionary::Column::Unsigned);
      assert(t->m_keyLenInWords != 0);
      bc.setLength(t->m_keyLenInWords);
      bc.setPrimaryKey(true);
      bc.setDistributionKey(true);
      bt.addColumn(bc);
    }
    { NdbDictionary::Column bc("DIST");
      bc.setType(NdbDictionary::Column::Unsigned);
      bc.setPrimaryKey(true);
      bc.setDistributionKey(true);
      bt.addColumn(bc);
    }
    { NdbDictionary::Column bc("PART");
      bc.setType(NdbDictionary::Column::Unsigned);
      bc.setPrimaryKey(true);
      bc.setDistributionKey(false);
      bt.addColumn(bc);
    }
    { NdbDictionary::Column bc("DATA");
      switch (c->m_type) {
      case NdbDictionary::Column::Blob:
        bc.setType(NdbDictionary::Column::Binary);
        break;
      case NdbDictionary::Column::Text:
        bc.setType(NdbDictionary::Column::Char);
        break;
      default:
        assert(false);
        break;
      }
      bc.setLength(c->getPartSize());
      bc.setStorageType(c->getStorageType());
      bt.addColumn(bc);
    }
  } else {
    {
      // table PK attributes
      const uint noOfKeys = t->m_noOfKeys;
      uint n = 0;
      uint i;
      for (i = 0; n < noOfKeys; i++) {
        assert(i < t->m_columns.size());
        const NdbColumnImpl* c = t->getColumn(i);
        assert(c != NULL);
        if (c->m_pk) {
          bt.addColumn(*c);
          // addColumn might usefully return the column added..
          NdbColumnImpl* bc = bt.getColumn(n);
          assert(bc != NULL);
          if (c->getDistributionKey()) {
            bc->setDistributionKey(true);
          }
          // confuses restore and wrong anyway
          bc->setAutoIncrement(false);
          bc->setDefaultValue("");
          n++;
        }
      }
    }
    // in V2 add NDB$ to avoid conflict with table PK
    if (c->getStripeSize() != 0)
    { NdbDictionary::Column bc("NDB$DIST");
      bc.setType(NdbDictionary::Column::Unsigned);
      bc.setPrimaryKey(true);
      bc.setDistributionKey(true);
      bt.addColumn(bc);
    }
    { NdbDictionary::Column bc("NDB$PART");
      bc.setType(NdbDictionary::Column::Unsigned);
      bc.setPrimaryKey(true);
      bc.setDistributionKey(false);
      bt.addColumn(bc);
    }
    // in V2 add id sequence for use in blob event code
    { NdbDictionary::Column bc("NDB$PKID");
      bc.setType(NdbDictionary::Column::Unsigned);
      bc.setPrimaryKey(false);
      bc.setDistributionKey(false);
      bt.addColumn(bc);
    }
    // in V2 changes to Longvar* regardless of size
    { NdbDictionary::Column bc("NDB$DATA");
      const Uint32 storageType = (Uint32)c->getStorageType();
      switch (c->m_type) {
      case NdbDictionary::Column::Blob:
        if (storageType == NDB_STORAGETYPE_MEMORY)
          bc.setType(NdbDictionary::Column::Longvarbinary);
        else
          bc.setType(NdbDictionary::Column::Binary);
        break;
      case NdbDictionary::Column::Text:
        if (storageType == NDB_STORAGETYPE_MEMORY)
          bc.setType(NdbDictionary::Column::Longvarchar);
        else
          bc.setType(NdbDictionary::Column::Char);
        break;
      default:
        assert(false);
        break;
      }
      // the 2 length bytes are not part of part size
      bc.setLength(c->getPartSize());
      bc.setStorageType(c->getStorageType());
      bt.addColumn(bc);
    }
  }
  DBUG_RETURN(0);
}

int
NdbBlob::getBlobEventName(char* bename, Ndb* anNdb, const char* eventName, const char* columnName)
{
  NdbEventImpl* e = anNdb->theDictionary->m_impl.getEvent(eventName);
  if (e == NULL)
    return -1;
  NdbColumnImpl* c = e->m_tableImpl->getColumn(columnName);
  if (c == NULL)
    return -1;
  getBlobEventName(bename, e, c);
  delete e; // it is from new NdbEventImpl
  return 0;
}

void
NdbBlob::getBlobEventName(char* bename, const NdbEventImpl* e, const NdbColumnImpl* c)
{
  // XXX events should have object id
  BaseString::snprintf(bename, MAX_TAB_NAME_SIZE, "NDB$BLOBEVENT_%s_%d", e->m_name.c_str(), (int)c->m_column_no);
}

void
NdbBlob::getBlobEvent(NdbEventImpl& be, const NdbEventImpl* e, const NdbColumnImpl* c)
{
  DBUG_ENTER("NdbBlob::getBlobEvent");
  // blob table
  assert(c->m_blobTable != NULL);
  const NdbTableImpl& bt = *c->m_blobTable;
  // blob event name
  char bename[MAX_TAB_NAME_SIZE+1];
  getBlobEventName(bename, e, c);
  bename[sizeof(bename)-1]= 0;
  be.setName(bename);
  be.setTable(bt);
  // simple assigments
  be.mi_type = e->mi_type;
  be.m_dur = e->m_dur;
  be.m_mergeEvents = e->m_mergeEvents;
  // report unchanged data
  // not really needed now since UPD is DEL o INS and we subscribe to all
  be.setReport(NdbDictionary::Event::ER_ALL);
  // columns PK - DIST - PART - DATA
  { const NdbColumnImpl* bc = bt.getColumn((Uint32)0);
    be.addColumn(*bc);
  }
  { const NdbColumnImpl* bc = bt.getColumn((Uint32)1);
    be.addColumn(*bc);
  }
  { const NdbColumnImpl* bc = bt.getColumn((Uint32)2);
    be.addColumn(*bc);
  }
  { const NdbColumnImpl* bc = bt.getColumn((Uint32)3);
    be.addColumn(*bc);
  }
  DBUG_VOID_RETURN;
}

// initialization

NdbBlob::NdbBlob(Ndb*)
{
  init();
}

void
NdbBlob::init()
{
  theBlobVersion = 0;
  theFixedDataFlag = false;
  theHeadSize = 0;
  theVarsizeBytes = 0;
  theState = Idle;
  theEventBlobVersion = -1;
  theBtColumnNo[0] = -1;
  theBtColumnNo[1] = -1;
  theBtColumnNo[2] = -1;
  theBtColumnNo[3] = -1;
  theBtColumnNo[4] = -1;
  theNdb = NULL;
  theNdbCon = NULL;
  theNdbOp = NULL;
  theEventOp = NULL;
  theBlobEventOp = NULL;
  theBlobEventPkRecAttr = NULL;
  theBlobEventDistRecAttr = NULL;
  theBlobEventPartRecAttr = NULL;
  theBlobEventPkidRecAttr = NULL;
  theBlobEventDataRecAttr = NULL;
  theTable = NULL;
  theAccessTable = NULL;
  theBlobTable = NULL;
  theColumn = NULL;
  theFillChar = 0xFF;
  theInlineSize = 0;
  thePartSize = 0;
  theStripeSize = 0;
  theGetFlag = false;
  theGetBuf = NULL;
  theSetFlag = false;
  theSetValueInPreExecFlag = false;
  theSetBuf = NULL;
  theGetSetBytes = 0;
  thePendingBlobOps = 0;
  theActiveHook = NULL;
  theActiveHookArg = NULL;
  thePartLen = 0;
  theInlineData = NULL;
  theHeadInlineRecAttr = NULL;
  theHeadInlineReadOp = NULL;
  theHeadInlineUpdateFlag = false;
  userDefinedPartitioning = false;
  thePartitionId = noPartitionId();
  thePartitionIdRecAttr = NULL;
  theNullFlag = -1;
  theLength = 0;
  thePos = 0;
  theNext = NULL;
}

void
NdbBlob::release()
{
  theKeyBuf.release();
  theAccessKeyBuf.release();
  thePackKeyBuf.release();
  theHeadInlineBuf.release();
  theHeadInlineCopyBuf.release();
  thePartBuf.release();
  theBlobEventDataBuf.release();
  setState(Idle);
}

// buffers

NdbBlob::Buf::Buf() :
  data(NULL),
  size(0),
  maxsize(0)
{
}

NdbBlob::Buf::~Buf()
{
  delete [] data;
}

void
NdbBlob::Buf::alloc(unsigned n)
{
  size = n;
  if (maxsize < n) {
    delete [] data;
    // align to Uint64
    if (n % 8 != 0)
      n += 8 - n % 8;
    data = new char [n];
    maxsize = n;
  }
#ifdef VM_TRACE
  memset(data, 'X', maxsize);
#endif
}

void
NdbBlob::Buf::release()
{
  if (data)
    delete [] data;
  data = NULL;
  size = 0;
  maxsize = 0;
}

void
NdbBlob::Buf::zerorest()
{
  assert(size <= maxsize);
  memset(data + size, 0, maxsize - size);
}

void
NdbBlob::Buf::copyfrom(const NdbBlob::Buf& src)
{
  size = src.size;
  memcpy(data, src.data, size);
}

// classify operations (inline)

inline bool
NdbBlob::isTableOp()
{
  return theTable == theAccessTable;
}

inline bool
NdbBlob::isIndexOp()
{
  return theTable != theAccessTable;
}

inline bool
NdbBlob::isKeyOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::InsertRequest ||
    theNdbOp->theOperationType == NdbOperation::UpdateRequest ||
    theNdbOp->theOperationType == NdbOperation::WriteRequest ||
    theNdbOp->theOperationType == NdbOperation::ReadRequest ||
    theNdbOp->theOperationType == NdbOperation::ReadExclusive ||
    theNdbOp->theOperationType == NdbOperation::DeleteRequest;
}

inline bool
NdbBlob::isReadOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::ReadRequest ||
    theNdbOp->theOperationType == NdbOperation::ReadExclusive;
}

inline bool
NdbBlob::isInsertOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::InsertRequest;
}

inline bool
NdbBlob::isUpdateOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::UpdateRequest;
}

inline bool
NdbBlob::isWriteOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::WriteRequest;
}

inline bool
NdbBlob::isDeleteOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::DeleteRequest;
}

inline bool
NdbBlob::isScanOp()
{
  return
    theNdbOp->theOperationType == NdbOperation::OpenScanRequest ||
    theNdbOp->theOperationType == NdbOperation::OpenRangeScanRequest;
}

inline bool
NdbBlob::isReadOnlyOp()
{
  return ! (
    theNdbOp->theOperationType == NdbOperation::InsertRequest ||
    theNdbOp->theOperationType == NdbOperation::UpdateRequest ||
    theNdbOp->theOperationType == NdbOperation::WriteRequest
  );
}

inline bool
NdbBlob::isTakeOverOp()
{
  return
    TcKeyReq::getTakeOverScanFlag(theNdbOp->theScanInfo);
}

// computations (inline)

inline Uint32
NdbBlob::getPartNumber(Uint64 pos)
{
  assert(thePartSize != 0 && pos >= theInlineSize);
  Uint64 partNo = (pos - theInlineSize) / thePartSize;
  assert(partNo < (Uint64(1) << 32));
  return Uint32(partNo);
}

inline Uint32
NdbBlob::getPartOffset(Uint64 pos)
{
  assert(thePartSize != 0 && pos >= theInlineSize);
  return (pos - theInlineSize) % thePartSize;
}

inline Uint32
NdbBlob::getPartCount()
{
  if (theLength <= theInlineSize)
    return 0;
  return 1 + getPartNumber(theLength - 1);
}

inline Uint32
NdbBlob::getDistKey(Uint32 part)
{
  assert(theStripeSize != 0);
  Uint32 dist = 0;
  if (unlikely(theBlobVersion == NDB_BLOB_V1))
    dist = (part / theStripeSize) % theStripeSize;
  else {
    // correct the mistake
    dist = (part / theStripeSize);
  }
  return dist;
}

inline void
NdbBlob::setHeadPartitionId(NdbOperation* anOp)
{
  /* For UserDefined partitioned tables,
   * we must set the head row's partition id
   * manually when reading/modifying it with
   * primary key or unique key.
   * For scans we do not have to.
   */
  if (userDefinedPartitioning &&
      (thePartitionId != noPartitionId())) {
    anOp->setPartitionId(thePartitionId);
  }
}

inline void
NdbBlob::setPartPartitionId(NdbOperation* anOp)
{
  /* For UserDefined partitioned tables
   * we must set the part row's partition 
   * id manually when performing operations.
   * This means that stripe size is ignored
   * for UserDefined partitioned tables.
   * All part row operations use primary keys
   */
  if (userDefinedPartitioning) {
    assert(thePartitionId != noPartitionId());
    anOp->setPartitionId(thePartitionId);
  }
}

// pack/unpack table/index key  XXX support routines, shortcuts

int
NdbBlob::packKeyValue(const NdbTableImpl* aTable, const Buf& srcBuf)
{
  DBUG_ENTER("NdbBlob::packKeyValue");
  const Uint32* data = (const Uint32*)srcBuf.data;
  unsigned pos = 0;
  Uint32* pack_data = (Uint32*)thePackKeyBuf.data;
  unsigned pack_pos = 0;
  for (unsigned i = 0; i < aTable->m_columns.size(); i++) {
    NdbColumnImpl* c = aTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      Uint32 pack_len;
      bool ok = c->get_var_length(&data[pos], pack_len);
      if (! ok) {
        setErrorCode(NdbBlobImpl::ErrCorruptPK);
        DBUG_RETURN(-1);
      }
      memcpy(&pack_data[pack_pos], &data[pos], pack_len);
      while (pack_len % 4 != 0) {
        char* p = (char*)&pack_data[pack_pos] + pack_len++;
        *p = 0;
      }
      pos += (len + 3) / 4;
      pack_pos += pack_len / 4;
    }
  }
  assert(4 * pos == srcBuf.size);
  assert(4 * pack_pos <= thePackKeyBuf.maxsize);
  thePackKeyBuf.size = 4 * pack_pos;
  thePackKeyBuf.zerorest();
  DBUG_RETURN(0);
}

int
NdbBlob::unpackKeyValue(const NdbTableImpl* aTable, Buf& dstBuf)
{
  DBUG_ENTER("NdbBlob::unpackKeyValue");
  Uint32* data = (Uint32*)dstBuf.data;
  unsigned pos = 0;
  const Uint32* pack_data = (const Uint32*)thePackKeyBuf.data;
  unsigned pack_pos = 0;
  for (unsigned i = 0; i < aTable->m_columns.size(); i++) {
    NdbColumnImpl* c = aTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      Uint32 pack_len;
      bool ok = c->get_var_length(&pack_data[pack_pos], pack_len);
      if (! ok) {
        setErrorCode(NdbBlobImpl::ErrCorruptPK);
        DBUG_RETURN(-1);
      }
      memcpy(&data[pos], &pack_data[pack_pos], pack_len);
      while (pack_len % 4 != 0) {
        char* p = (char*)&data[pos] + pack_len++;
        *p = 0;
      }
      pos += (len + 3) / 4;
      pack_pos += pack_len / 4;
    }
  }
  assert(4 * pos == dstBuf.size);
  assert(4 * pack_pos == thePackKeyBuf.size);
  DBUG_RETURN(0);
}

/* Set both packed and unpacked KeyBuf from NdbRecord and row. */
int
NdbBlob::copyKeyFromRow(const NdbRecord *record, const char *row,
                        Buf& packedBuf, Buf& unpackedBuf)
{
  char buf[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];
  DBUG_ENTER("NdbBlob::copyKeyFromRow");

  assert(record->flags & NdbRecord::RecHasAllKeys);

  char *packed= packedBuf.data;
  char *unpacked= unpackedBuf.data;

  for (Uint32 i= 0; i < record->key_index_length; i++)
  {
    const NdbRecord::Attr *col= &record->columns[record->key_indexes[i]];

    Uint32 len= ~0;
    bool len_ok;
    const char *src;
    if (col->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      /* Used to support special varchar format for mysqld keys. */
      len_ok= col->shrink_varchar(row, len, buf);
      src= buf;
    }
    else
    {
      len_ok= col->get_var_length(row, len);
      src= &row[col->offset];
    }

    if (!len_ok)
    {
      setErrorCode(NdbBlobImpl::ErrCorruptPK);
      DBUG_RETURN(-1);
    }

    /* Copy the key. */
    memcpy(packed, src, len);
    memcpy(unpacked, src, len);

    /* Zero-pad if needed. */
    Uint32 packed_len= (len + 3) & ~3;
    Uint32 unpacked_len= (col->maxSize + 3) & ~3;
    Uint32 packed_pad= packed_len - len;
    Uint32 unpacked_pad= unpacked_len - len;
    if (packed_pad > 0)
      bzero(packed + len, packed_pad);
    if (unpacked_pad > 0)
      bzero(unpacked + len, unpacked_pad);
    packed+= packed_len;
    unpacked+= unpacked_len;
  }

  packedBuf.size= (Uint32)(packed - packedBuf.data);
  packedBuf.zerorest();
  assert(unpacked == unpackedBuf.data + unpackedBuf.size);
  DBUG_RETURN(0);
}

/* 
 * This method is used to get data ptr and length values for the
 * header for an 'empty' Blob.  This is a blob with length zero,
 * or a NULL BLOB.
 * This header is used to build signals for an insert or write 
 * operation before the correct blob header information is known.  
 * Once the blob header information is known, another operation will 
 * set the header information correctly.
 */
void
NdbBlob::getNullOrEmptyBlobHeadDataPtr(const char * & data, 
                                       Uint32 & byteSize)
{
  /* Only for use when preparing signals before a blob value has been set
   * e.g. NdbRecord
   */
  assert(theState==Prepared);
  assert(theLength==0);
  assert(theSetBuf==NULL);
  assert(theGetSetBytes==0);
  assert(thePos==0);
  assert(theHeadInlineBuf.data!=NULL);

  DBUG_PRINT("info", ("getNullOrEmptyBlobHeadDataPtr.  Nullable : %d",
                      theColumn->m_nullable));

  if (theColumn->m_nullable)
  {
    /* Null Blob */
    data = NULL;
    byteSize = 0;
    return;
  }

  /* Set up the buffer ptr to appear to be pointing to some data */
  theSetBuf=(char*) 1; // Extremely nasty way of being non-null
                       // If it's ever de-reffed, should show up 
  
  /* Pack header etc. */
  prepareSetHeadInlineValue();
  
  data=theHeadInlineBuf.data;

  /* Calculate size */
  if (unlikely(theBlobVersion == NDB_BLOB_V1))
    byteSize = theHeadInlineBuf.size;
  else
    byteSize = theHead.varsize + 2;

  /* Reset affected members */
  theSetBuf=NULL;
  memset(&theHead, 0, sizeof(theHead));

  /* This column is not null anymore - record the fact so that
   * a setNull() call will modify state
   */
  theNullFlag=false;
}


// getters and setters

void
NdbBlob::packBlobHead(const Head& head, char* buf, int blobVersion)
{
  DBUG_ENTER("NdbBlob::packBlobHead");
  DBUG_PRINT("info", ("version=%d", blobVersion));
  if (unlikely(blobVersion == NDB_BLOB_V1)) {
    // native
    memcpy(buf, &head.length, sizeof(head.length));
  } else {
    unsigned char* p = (unsigned char*)buf;
    // all little-endian
    uint i, n;
    for (i = 0, n = 0; i < 2; i++, n += 8)
      *p++ = (head.varsize >> n) & 0xff;
    for (i = 0, n = 0; i < 2; i++, n += 8)
      *p++ = (head.reserved >> n) & 0xff;
    for (i = 0, n = 0; i < 4; i++, n += 8)
      *p++ = (head.pkid >> n) & 0xff;
    for (i = 0, n = 0; i < 8; i++, n += 8)
      *p++ = (head.length >> n) & 0xff;
    assert(p - (uchar*)buf == 16);
    assert(head.reserved == 0);
    DBUG_DUMP("info", (uchar*)buf, 16);
  }
  DBUG_PRINT("info", ("pack: varsize=%u length=%u pkid=%u",
                      (uint)head.varsize, (uint)head.length, (uint)head.pkid));
  DBUG_VOID_RETURN;
}

void
NdbBlob::unpackBlobHead(Head& head, const char* buf, int blobVersion)
{
  DBUG_ENTER("NdbBlob::unpackBlobHead");
  DBUG_PRINT("info", ("version=%d", blobVersion));
  head.varsize = 0;
  head.reserved = 0;
  head.pkid = 0;
  head.length = 0;
  if (unlikely(blobVersion == NDB_BLOB_V1)) {
    // native
    memcpy(&head.length, buf, sizeof(head.length));
    head.headsize = (NDB_BLOB_V1_HEAD_SIZE << 2);
  } else {
    const unsigned char* p = (const unsigned char*)buf;
    // all little-endian
    uint i, n;
    for (i = 0, n = 0; i < 2; i++, n += 8)
      head.varsize |= ((Uint16)*p++ << n);
    for (i = 0, n = 0; i < 2; i++, n += 8)
      head.reserved |= ((Uint32)*p++ << n);
    for (i = 0, n = 0; i < 4; i++, n += 8)
      head.pkid |= ((Uint32)*p++ << n);
    for (i = 0, n = 0; i < 8; i++, n += 8)
      head.length |= ((Uint64)*p++ << n);
    assert(p - (uchar*)buf == 16);
    assert(head.reserved == 0);
    head.headsize = (NDB_BLOB_V2_HEAD_SIZE << 2);
    DBUG_DUMP("info", (uchar*)buf, 16);
  }
  DBUG_PRINT("info", ("unpack: varsize=%u length=%u pkid=%u",
                      (uint)head.varsize, (uint)head.length, (uint)head.pkid));
  DBUG_VOID_RETURN;
}

inline void
NdbBlob::packBlobHead()
{
  packBlobHead(theHead, theHeadInlineBuf.data, theBlobVersion);
}

inline void
NdbBlob::unpackBlobHead()
{
  unpackBlobHead(theHead, theHeadInlineBuf.data, theBlobVersion);
}

int
NdbBlob::getTableKeyValue(NdbOperation* anOp)
{
  DBUG_ENTER("NdbBlob::getTableKeyValue");
  Uint32* data = (Uint32*)theKeyBuf.data;
  unsigned pos = 0;
  for (unsigned i = 0; i < theTable->m_columns.size(); i++) {
    NdbColumnImpl* c = theTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (anOp->getValue_impl(c, (char*)&data[pos]) == NULL) {
        setErrorCode(anOp);
        DBUG_RETURN(-1);
      }
      // odd bytes receive no data and must be zeroed
      while (len % 4 != 0) {
        char* p = (char*)&data[pos] + len++;
        *p = 0;
      }
      pos += len / 4;
    }
  }
  assert(pos == theKeyBuf.size / 4);
  DBUG_RETURN(0);
}

// in V2 operation can also be on blob part
int
NdbBlob::setTableKeyValue(NdbOperation* anOp)
{
  DBUG_ENTER("NdbBlob::setTableKeyValue");
  DBUG_DUMP("info", (uchar*) theKeyBuf.data, 4 * theTable->m_keyLenInWords);
  const bool isBlobPartOp = (anOp->m_currentTable == theBlobTable);
  const Uint32* data = (const Uint32*)theKeyBuf.data;
  uint n = 0;
  const uint noOfKeys = theTable->m_noOfKeys;
  unsigned pos = 0;
  for (unsigned i = 0;  n < noOfKeys; i++) {
    assert(i < theTable->m_columns.size());
    const NdbColumnImpl* c = theTable->getColumn(i);
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (isBlobPartOp) {
        c = theBlobTable->getColumn(n);
        assert(c != NULL);
      }
      if (anOp->equal_impl(c, (const char*)&data[pos]) == -1) {
        setErrorCode(anOp);
        DBUG_RETURN(-1);
      }
      pos += (len + 3) / 4;
      n++;
    }
  }
  assert(pos == theKeyBuf.size / 4);
  DBUG_RETURN(0);
}

int
NdbBlob::setAccessKeyValue(NdbOperation* anOp)
{
  DBUG_ENTER("NdbBlob::setAccessKeyValue");
  DBUG_DUMP("info", (uchar*) theAccessKeyBuf.data,
            4 * theAccessTable->m_keyLenInWords);
  const Uint32* data = (const Uint32*)theAccessKeyBuf.data;
  const unsigned columns = theAccessTable->m_columns.size();
  unsigned pos = 0;
  for (unsigned i = 0; i < columns; i++) {
    NdbColumnImpl* c = theAccessTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (anOp->equal_impl(c, (const char*)&data[pos]) == -1) {
        setErrorCode(anOp);
        DBUG_RETURN(-1);
      }
      pos += (len + 3) / 4;
    }
  }
  assert(pos == theAccessKeyBuf.size / 4);
  DBUG_RETURN(0);
}

int
NdbBlob::setDistKeyValue(NdbOperation* anOp, Uint32 part)
{
  DBUG_ENTER("NdbBlob::setDistKeyValue");
  if (theStripeSize != 0) {
    Uint32 dist = getDistKey(part);
    DBUG_PRINT("info", ("dist=%u", dist));
    if (anOp->equal(theBtColumnNo[BtColumnDist], dist) == -1)
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int
NdbBlob::setPartKeyValue(NdbOperation* anOp, Uint32 part)
{
  DBUG_ENTER("NdbBlob::setPartKeyValue");
  DBUG_PRINT("info", ("part=%u packkey=", part));
  DBUG_DUMP("info", (uchar*) thePackKeyBuf.data, thePackKeyBuf.size);
  // TODO use attr ids after compatibility with 4.1.7 not needed
  if (unlikely(theBlobVersion == NDB_BLOB_V1)) {
    // keep using names
    if (anOp->equal("PK", thePackKeyBuf.data) == -1 ||
        anOp->equal("DIST", getDistKey(part)) == -1 ||
        anOp->equal("PART", part) == -1) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  } else {
    if (setTableKeyValue(anOp) == -1 ||
        setDistKeyValue(anOp, part) == -1 ||
        anOp->equal(theBtColumnNo[BtColumnPart], part) == -1) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  }
  setPartPartitionId(anOp);
  DBUG_RETURN(0);
}

int
NdbBlob::setPartPkidValue(NdbOperation* anOp, Uint32 pkid)
{
  DBUG_ENTER("NdbBlob::setPartPkidValue");
  DBUG_PRINT("info", ("pkid=%u", pkid));
  if (unlikely(theBlobVersion == NDB_BLOB_V1))
    ;
  else {
    if (anOp->setValue(theBtColumnNo[BtColumnPkid], pkid) == -1) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

int
NdbBlob::getPartDataValue(NdbOperation* anOp, char* buf, Uint16* aLenLoc)
{
  DBUG_ENTER("NdbBlob::getPartDataValue");
  assert(aLenLoc != NULL);
  Uint32 bcNo = theBtColumnNo[BtColumnData];
  if (theFixedDataFlag) {
    if (anOp->getValue(bcNo, buf) == NULL) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
    // length is full size and is not returned via NDB API
    *aLenLoc = thePartSize;
  } else {
    const NdbColumnImpl* bc = theBlobTable->getColumn(bcNo);
    assert(bc != NULL);
    if (anOp->getVarValue(bc, buf, aLenLoc) == NULL) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
    // in V2 length is set when next execute returns
  }
  DBUG_RETURN(0);
}

int
NdbBlob::setPartDataValue(NdbOperation* anOp, const char* buf, const Uint16& aLen)
{
  DBUG_ENTER("NdbBlob::setPartDataValue");
  assert(aLen != 0);
  Uint32 bcNo = theBtColumnNo[BtColumnData];
  if (theFixedDataFlag) {
    if (anOp->setValue(bcNo, buf) == -1) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  } else {
    const NdbColumnImpl* bc = theBlobTable->getColumn(bcNo);
    assert(bc != NULL);
    if (anOp->setVarValue(bc, buf, aLen) == -1) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

int
NdbBlob::getHeadInlineValue(NdbOperation* anOp)
{
  DBUG_ENTER("NdbBlob::getHeadInlineValue");

  /* Get values using implementation of getValue to avoid NdbRecord
   * specific checks
   */
  theHeadInlineRecAttr = anOp->getValue_impl(theColumn, theHeadInlineBuf.data);
  if (theHeadInlineRecAttr == NULL) {
    setErrorCode(anOp);
    DBUG_RETURN(-1);
  }
  if (userDefinedPartitioning)
  {
    /* For UserDefined partitioned tables, we ask for the partition
     * id of the main table row to use for the parts
     * Not technically needed for main table access via PK, which must
     * have partition id set for access, but we do it anyway and check
     * it's as expected.
     */
    thePartitionIdRecAttr = 
      anOp->getValue_impl(&NdbColumnImpl::getImpl(*NdbDictionary::Column::FRAGMENT));
    
    if (thePartitionIdRecAttr == NULL) {
      setErrorCode(anOp);
      DBUG_RETURN(-1);
    }
  }
  /*
   * If we get no data from this op then the operation is aborted
   * one way or other.  Following hack in 5.0 makes sure we don't read
   * garbage.  The proper fix exists only in version >= 5.1.
   */
  // 5.0 theHead->length = 0;
  memset(&theHead, 0, sizeof(theHead));
  packBlobHead();
  DBUG_RETURN(0);
}

void
NdbBlob::getHeadFromRecAttr()
{
  DBUG_ENTER("NdbBlob::getHeadFromRecAttr");
  assert(theHeadInlineRecAttr != NULL);
  theNullFlag = theHeadInlineRecAttr->isNULL();
  assert(theEventBlobVersion >= 0 || theNullFlag != -1);
  if (theNullFlag == 0) {
    unpackBlobHead();
    theLength = theHead.length;
  } else {
    theLength = 0;
  }
  if (theEventBlobVersion == -1) {
    if (userDefinedPartitioning)
    {
      /* Use main table fragment id as partition id
       * for blob parts table
       */
      Uint32 id = thePartitionIdRecAttr->u_32_value();
      DBUG_PRINT("info", ("table partition id: %u", id));
      if (thePartitionId == noPartitionId()) {
        DBUG_PRINT("info", ("discovered here"));
        thePartitionId = id;
      } else {
        assert(thePartitionId == id);
      }
    }
    else
    {
      assert(thePartitionIdRecAttr == NULL);
    }
  }

  DBUG_PRINT("info", ("theNullFlag=%d theLength=%llu",
                      theNullFlag, theLength));
  DBUG_VOID_RETURN;
}

void
NdbBlob::prepareSetHeadInlineValue()
{
  theHead.length = theLength;
  if (unlikely(theBlobVersion == NDB_BLOB_V1)) {
    if (theLength < theInlineSize)
      memset(theInlineData + theLength, 0, size_t(theInlineSize - theLength));
  } else {
    // the 2 length bytes are not counted in length
    if (theLength < theInlineSize)
      theHead.varsize = (theHeadSize - 2) + Uint32(theLength);
    else
      theHead.varsize = (theHeadSize - 2) + theInlineSize;
    theHead.pkid = 0; // wl3717_todo not yet
  }
  packBlobHead();
  theHeadInlineUpdateFlag = false;
  assert(theNullFlag != -1);
}

int
NdbBlob::setHeadInlineValue(NdbOperation* anOp)
{
  DBUG_ENTER("NdbBlob::setHeadInlineValue");
  prepareSetHeadInlineValue();
  const char* aValue = theNullFlag ? 0 : theHeadInlineBuf.data;
  if (anOp->setValue(theColumn, aValue) == -1) {
    setErrorCode(anOp);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

// getValue/setValue

int
NdbBlob::getValue(void* data, Uint32 bytes)
{
  DBUG_ENTER("NdbBlob::getValue");
  DBUG_PRINT("info", ("data=%p bytes=%u", data, bytes));
  if (! isReadOp() && ! isScanOp()) {
    setErrorCode(NdbBlobImpl::ErrCompat);
    DBUG_RETURN(-1);
  }
  if (theGetFlag || theState != Prepared) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  if (data == NULL && bytes != 0) {
    setErrorCode(NdbBlobImpl::ErrUsage);
    DBUG_RETURN(-1);
  }
  theGetFlag = true;
  theGetBuf = static_cast<char*>(data);
  theGetSetBytes = bytes;
  DBUG_RETURN(0);
}

int
NdbBlob::setValue(const void* data, Uint32 bytes)
{
  DBUG_ENTER("NdbBlob::setValue");
  DBUG_PRINT("info", ("data=%p bytes=%u", data, bytes));
  if (isReadOnlyOp()) {
    setErrorCode(NdbBlobImpl::ErrCompat);
    DBUG_RETURN(-1);
  }
  if (theSetFlag || theState != Prepared) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  if (data == NULL && bytes != 0) {
    setErrorCode(NdbBlobImpl::ErrUsage);
    DBUG_RETURN(-1);
  }
  theSetFlag = true;
  theSetBuf = static_cast<const char*>(data);
  theGetSetBytes = bytes;
  if (isInsertOp()) {
    // write inline part now
    if (theSetBuf != NULL) {
      Uint32 n = theGetSetBytes;
      if (n > theInlineSize)
        n = theInlineSize;
      assert(thePos == 0);
      if (writeDataPrivate(theSetBuf, n) == -1)
        DBUG_RETURN(-1);
    } else {
      theNullFlag = true;
      theLength = 0;
    }
    /*
     * In NdbRecAttr case, we set the value of the blob head here with
     * an extra setValue()
     * In NdbRecord case, this is done by adding a separate operation in 
     * preExecute() as we cannot modify the head-table NdbOperation.
     */
    if (!theNdbRecordFlag)
    {
      if (setHeadInlineValue(theNdbOp) == -1)
        DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

// activation hook

int
NdbBlob::setActiveHook(ActiveHook activeHook, void* arg)
{
  DBUG_ENTER("NdbBlob::setActiveHook");
  DBUG_PRINT("info", ("hook=%p arg=%p", (void*)&activeHook, arg));
  if (theState != Prepared) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  theActiveHook = activeHook;
  theActiveHookArg = arg;
  DBUG_RETURN(0);
}

// misc operations

int
NdbBlob::getDefined(int& isNull) // deprecated
{
  DBUG_ENTER("NdbBlob::getDefined");
  if (theState == Prepared && theSetFlag) {
    isNull = (theSetBuf == NULL);
    DBUG_RETURN(0);
  }
  isNull = theNullFlag;
  DBUG_RETURN(0);
}

int
NdbBlob::getNull(bool& isNull) // deprecated
{
  DBUG_ENTER("NdbBlob::getNull");
  if (theState == Prepared && theSetFlag) {
    isNull = (theSetBuf == NULL);
    DBUG_RETURN(0);
  }
  if (theNullFlag == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  isNull = theNullFlag;
  DBUG_RETURN(0);
}

int
NdbBlob::getNull(int& isNull)
{
  DBUG_ENTER("NdbBlob::getNull");
  if (theState == Prepared && theSetFlag) {
    isNull = (theSetBuf == NULL);
    DBUG_RETURN(0);
  }
  isNull = theNullFlag;
  if (isNull == -1 && theEventBlobVersion == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("isNull=%d", isNull));
  DBUG_RETURN(0);
}

int
NdbBlob::setNull()
{
  DBUG_ENTER("NdbBlob::setNull");
  if (isReadOnlyOp()) {
    setErrorCode(NdbBlobImpl::ErrCompat);
    DBUG_RETURN(-1);
  }
  if (theNullFlag == -1) {
    if (theState == Prepared) {
      DBUG_RETURN(setValue(0, 0));
    }
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  if (theNullFlag)
    DBUG_RETURN(0);
  if (deletePartsThrottled(0, getPartCount()) == -1)
    DBUG_RETURN(-1);
  theNullFlag = true;
  theLength = 0;
  theHeadInlineUpdateFlag = true;
  DBUG_RETURN(0);
}

int
NdbBlob::getLength(Uint64& len)
{
  DBUG_ENTER("NdbBlob::getLength");
  if (theState == Prepared && theSetFlag) {
    len = theGetSetBytes;
    DBUG_RETURN(0);
  }
  if (theNullFlag == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  len = theLength;
  DBUG_RETURN(0);
}

int
NdbBlob::truncate(Uint64 length)
{
  DBUG_ENTER("NdbBlob::truncate");
  DBUG_PRINT("info", ("length old=%llu new=%llu", theLength, length));
  if (isReadOnlyOp()) {
    setErrorCode(NdbBlobImpl::ErrCompat);
    DBUG_RETURN(-1);
  }
  if (theNullFlag == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  if (theLength > length) {
    if (length > theInlineSize) {
      Uint32 part1 = getPartNumber(length - 1);
      Uint32 part2 = getPartNumber(theLength - 1);
      assert(part2 >= part1);
      if (part2 > part1 && deletePartsThrottled(part1 + 1, part2 - part1) == -1)
      DBUG_RETURN(-1);
      Uint32 off = getPartOffset(length);
      if (off != 0) {
        assert(off < thePartSize);
        /* Ensure all previous writes to this blob are flushed so
         * that we can read their updates
         */
        if (executePendingBlobWrites() == -1)
          DBUG_RETURN(-1);
        Uint16 len = 0;
        if (readPart(thePartBuf.data, part1, len) == -1)
          DBUG_RETURN(-1);
        if (executePendingBlobReads() == -1)
          DBUG_RETURN(-1);
        assert(len != 0);
        DBUG_PRINT("info", ("part %u length old=%u new=%u",
                            part1, (Uint32)len, off));
        if (theFixedDataFlag)
          memset(thePartBuf.data + off, theFillChar, thePartSize - off);
        if (updatePart(thePartBuf.data, part1, off) == -1)
          DBUG_RETURN(-1);
      }
    } else {
      if (deletePartsThrottled(0, getPartCount()) == -1)
        DBUG_RETURN(-1);
    }
    theLength = length;
    theHeadInlineUpdateFlag = true;
    if (thePos > length)
      thePos = length;
  }
  DBUG_RETURN(0);
}

int
NdbBlob::getPos(Uint64& pos)
{
  DBUG_ENTER("NdbBlob::getPos");
  if (theNullFlag == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  pos = thePos;
  DBUG_RETURN(0);
}

int
NdbBlob::setPos(Uint64 pos)
{
  DBUG_ENTER("NdbBlob::setPos");
  DBUG_PRINT("info", ("this=%p pos=%llu", this, pos));
  if (theNullFlag == -1) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  if (pos > theLength) {
    setErrorCode(NdbBlobImpl::ErrSeek);
    DBUG_RETURN(-1);
  }
  thePos = pos;
  DBUG_RETURN(0);
}

// read/write

int
NdbBlob::readData(void* data, Uint32& bytes)
{
  DBUG_ENTER("NdbBlob::readData");
  if (unlikely(theState != Active)) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  char* buf = static_cast<char*>(data);
  int ret = readDataPrivate(buf, bytes);
  DBUG_RETURN(ret);
}

int
NdbBlob::readDataPrivate(char* buf, Uint32& bytes)
{
  DBUG_ENTER("NdbBlob::readDataPrivate");
  DBUG_PRINT("info", ("bytes=%u thePos=%u theLength=%u",
                      bytes, (Uint32)thePos, (Uint32)theLength));
  assert(thePos <= theLength);
  Uint64 pos = thePos;
  if (bytes > theLength - pos)
    bytes = Uint32(theLength - pos);
  Uint32 len = bytes;
  if (len > 0) {
    // inline part
    if (pos < theInlineSize) {
      Uint32 n = theInlineSize - Uint32(pos);
      if (n > len)
        n = len;
      memcpy(buf, theInlineData + pos, n);
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (unlikely(len > 0 && thePartSize == 0)) {
    setErrorCode(NdbBlobImpl::ErrSeek);
    DBUG_RETURN(-1);
  }
  if (len > 0) {
    assert(pos >= theInlineSize);
    Uint32 off = (pos - theInlineSize) % thePartSize;
    // partial first block
    if (off != 0) {
      DBUG_PRINT("info", ("partial first block pos=%llu len=%u", pos, len));
      Uint32 part = getPartNumber(pos);
      Uint16 sz = 0;
      if (readPart(thePartBuf.data, part, sz) == -1)
        DBUG_RETURN(-1);
      // need result now
      if (executePendingBlobReads() == -1)
        DBUG_RETURN(-1);
      assert(sz >= off);
      Uint32 n = sz - off;
      if (n > len)
        n = len;
      memcpy(buf, thePartBuf.data + off, n);
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (len > 0) {
    assert((pos - theInlineSize) % thePartSize == 0);
    // complete blocks in the middle
    if (len >= thePartSize) {
      Uint32 part = getPartNumber(pos);
      Uint32 count = len / thePartSize;
      do
      {
        /* How much quota left, avoiding underflow? */
        Uint32 partsThisTrip = count;
        if (theEventBlobVersion == -1)
        {
          /* Table read as opposed to event buffer read */
          const Uint32 remainingQuota = 
            theNdbCon->maxPendingBlobReadBytes - 
            MIN(theNdbCon->maxPendingBlobReadBytes, theNdbCon->pendingBlobReadBytes);
          const Uint32 maxPartsThisTrip = MAX(remainingQuota / thePartSize, 1); // always read one part
          partsThisTrip= MIN(count, maxPartsThisTrip);
        }
        
        if (readParts(buf, part, partsThisTrip) == -1)
          DBUG_RETURN(-1);
        Uint32 n = thePartSize * partsThisTrip;

        pos += n;
        buf += n;
        len -= n;
        part += partsThisTrip;
        count -= partsThisTrip;
        if (count != 0)
        {
          /* Execute this batch before defining next */
          if (executePendingBlobReads() == -1)
            DBUG_RETURN(-1);
        }
      } while (count != 0);
    }
  }
  if (len > 0) {
    // partial last block
    DBUG_PRINT("info", ("partial last block pos=%llu len=%u", pos, len));
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = getPartNumber(pos);
    Uint16 sz = 0;
    if (readPart(thePartBuf.data, part, sz) == -1)
      DBUG_RETURN(-1);
    // need result now
    if (executePendingBlobReads() == -1)
      DBUG_RETURN(-1);
    assert(len <= sz);
    memcpy(buf, thePartBuf.data, len);
    Uint32 n = len;
    pos += n;
    buf += n;
    len -= n;
  }
  assert(len == 0);
  thePos = pos;
  assert(thePos <= theLength);
  DBUG_RETURN(0);
}

int
NdbBlob::writeData(const void* data, Uint32 bytes)
{
  DBUG_ENTER("NdbBlob::writeData");
  if (unlikely(isReadOnlyOp())) {
    setErrorCode(NdbBlobImpl::ErrCompat);
    DBUG_RETURN(-1);
  }
  if (unlikely(theState != Active)) {
    setErrorCode(NdbBlobImpl::ErrState);
    DBUG_RETURN(-1);
  }
  const char* buf = static_cast<const char*>(data);
  int ret = writeDataPrivate(buf, bytes);
  DBUG_RETURN(ret);
}

int
NdbBlob::writeDataPrivate(const char* buf, Uint32 bytes)
{
  DBUG_ENTER("NdbBlob::writeDataPrivate");
  DBUG_PRINT("info", ("pos=%llu bytes=%u", thePos, bytes));
  assert(thePos <= theLength);
  Uint64 pos = thePos;
  Uint32 len = bytes;
  // any write makes blob not NULL
  if (theNullFlag) {
    theNullFlag = false;
    theHeadInlineUpdateFlag = true;
  }
  if (len > 0) {
    // inline part
    if (pos < theInlineSize) {
      Uint32 n = theInlineSize - Uint32(pos);
      if (n > len)
        n = len;
      memcpy(theInlineData + pos, buf, n);
      theHeadInlineUpdateFlag = true;
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (unlikely(len > 0 && thePartSize == 0)) {
    setErrorCode(NdbBlobImpl::ErrSeek);
    DBUG_RETURN(-1);
  }
  if (len > 0) {
    assert(pos >= theInlineSize);
    Uint32 off = (pos - theInlineSize) % thePartSize;
    // partial first block
    if (off != 0) {
      DBUG_PRINT("info", ("partial first block pos=%llu len=%u", pos, len));
      // flush writes to guarantee correct read
      if (executePendingBlobWrites() == -1)
        DBUG_RETURN(-1);
      Uint32 part = getPartNumber(pos);
      Uint16 sz = 0;
      if (readPart(thePartBuf.data, part, sz) == -1)
        DBUG_RETURN(-1);
      // need result now
      if (executePendingBlobReads() == -1)
        DBUG_RETURN(-1);
      DBUG_PRINT("info", ("part len=%u", (Uint32)sz));
      assert(sz >= off);
      Uint32 n = thePartSize - off;
      if (n > len)
        n = len;
      Uint16 newsz = sz;
      if (pos + n > theLength) {
        // this is last part and we are extending it
        newsz = off + n;
      }
      memcpy(thePartBuf.data + off, buf, n);
      if (updatePart(thePartBuf.data, part, newsz) == -1)
        DBUG_RETURN(-1);
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (len > 0) {
    assert((pos - theInlineSize) % thePartSize == 0);
    // complete blocks in the middle
    if (len >= thePartSize) {
      Uint32 part = getPartNumber(pos);
      Uint32 count = len / thePartSize;
      for (unsigned i = 0; i < count; i++) {
        if (part + i < getPartCount()) {
          if (updateParts(buf, part + i, 1) == -1)
            DBUG_RETURN(-1);
        } else {
          if (insertParts(buf, part + i, 1) == -1)
            DBUG_RETURN(-1);
        }
        Uint32 n = thePartSize;
        pos += n;
        buf += n;
        len -= n;
        if (theNdbCon->pendingBlobWriteBytes > 
            theNdbCon->maxPendingBlobWriteBytes)
        {
          /* Flush defined part ops */
          if (executePendingBlobWrites() == -1)
          {
            DBUG_RETURN(-1);
          }
        }
      }
    }
  }
  if (len > 0) {
    // partial last block
    DBUG_PRINT("info", ("partial last block pos=%llu len=%u", pos, len));
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = getPartNumber(pos);
    if (theLength > pos + len) {
      // flush writes to guarantee correct read
      if (executePendingBlobWrites() == -1)
        DBUG_RETURN(-1);
      Uint16 sz = 0;
      if (readPart(thePartBuf.data, part, sz) == -1)
        DBUG_RETURN(-1);
      // need result now
      if (executePendingBlobReads() == -1)
        DBUG_RETURN(-1);
      memcpy(thePartBuf.data, buf, len);
      // no length change
      if (updatePart(thePartBuf.data, part, sz) == -1)
        DBUG_RETURN(-1);
    } else {
      memcpy(thePartBuf.data, buf, len);
      if (theFixedDataFlag) {
        memset(thePartBuf.data + len, theFillChar, thePartSize - len);
      }
      Uint16 sz = len;
      if (part < getPartCount()) {
        if (updatePart(thePartBuf.data, part, sz) == -1)
          DBUG_RETURN(-1);
      } else {
        if (insertPart(thePartBuf.data, part, sz) == -1)
          DBUG_RETURN(-1);
      }
    }
    Uint32 n = len;
    pos += n;
    buf += n;
    len -= n;
  }
  assert(len == 0);
  if (theLength < pos) {
    theLength = pos;
    theHeadInlineUpdateFlag = true;
  }
  thePos = pos;
  assert(thePos <= theLength);
  DBUG_RETURN(0);
}

/*
 * Operations on parts.
 *
 * - multi-part read/write operates only on full parts
 * - single-part read/write uses length
 * - single-part read requires caller to exec pending ops
 *
 * In V1 parts are striped.  In V2 they are either striped
 * or use table row partition.  The latter case applies both
 * to default and user-defined partitioning.
 */

int
NdbBlob::readParts(char* buf, Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::readParts");
  DBUG_PRINT("info", ("part=%u count=%u", part, count));
  if (theEventBlobVersion == -1) {
    if (readTableParts(buf, part, count) == -1)
      DBUG_RETURN(-1);
  } else {
    if (readEventParts(buf, part, count) == -1)
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int
NdbBlob::readPart(char* buf, Uint32 part, Uint16& len)
{
  DBUG_ENTER("NdbBlob::readPart");
  DBUG_PRINT("info", ("part=%u", part));
  if (theEventBlobVersion == -1) {
    if (readTablePart(buf, part, len) == -1)
      DBUG_RETURN(-1);
  } else {
    if (readEventPart(buf, part, len) == -1)
      DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("part=%u len=%u", part, (Uint32)len));
  DBUG_RETURN(0);
}

int
NdbBlob::readTableParts(char* buf, Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::readTableParts");
  Uint32 n = 0;
  while (n < count) {
    // length is not checked but a non-stack buffer is needed
    if (readTablePart(buf + n * thePartSize, part + n, thePartLen) == -1)
      DBUG_RETURN(-1);
    n++;
  }
  DBUG_RETURN(0);
}

int
NdbBlob::readTablePart(char* buf, Uint32 part, Uint16& len)
{
  DBUG_ENTER("NdbBlob::readTablePart");
  NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
  if (tOp == NULL ||
      /*
       * This was committedRead() before.  However lock on main
       * table tuple does not fully protect blob parts since DBTUP
       * commits each tuple separately.
       */
      tOp->readTuple(NdbOperation::LM_SimpleRead) == -1 ||
      setPartKeyValue(tOp, part) == -1 ||
      getPartDataValue(tOp, buf, &len) == -1) {
    setErrorCode(tOp);
    DBUG_RETURN(-1);
  }

  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
  theNdbCon->pendingBlobReadBytes += len;
  DBUG_RETURN(0);
}

int
NdbBlob::readEventParts(char* buf, Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::readEventParts");
  // length not asked for - event code checks each part is full
  if (theEventOp->readBlobParts(buf, this, part, count, (Uint16*)0) == -1) {
    setErrorCode(theEventOp);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int
NdbBlob::readEventPart(char* buf, Uint32 part, Uint16& len)
{
  DBUG_ENTER("NdbBlob::readEventPart");
  if (theEventOp->readBlobParts(buf, this, part, 1, &len) == -1) {
    setErrorCode(theEventOp);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int
NdbBlob::insertParts(const char* buf, Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::insertParts");
  DBUG_PRINT("info", ("part=%u count=%u", part, count));
  Uint32 n = 0;
  while (n < count) {
    // use non-stack variable for safety
    thePartLen = thePartSize;
    if (insertPart(buf + n * thePartSize, part + n, thePartLen) == -1)
      DBUG_RETURN(-1);
    n++;
  }
  DBUG_RETURN(0);
}

int
NdbBlob::insertPart(const char* buf, Uint32 part, const Uint16& len)
{
  DBUG_ENTER("NdbBlob::insertPart");
  DBUG_PRINT("info", ("part=%u len=%u", part, (Uint32)len));
  NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
  if (tOp == NULL ||
      tOp->insertTuple() == -1 ||
      setPartKeyValue(tOp, part) == -1 ||
      setPartPkidValue(tOp, theHead.pkid) == -1 ||
      setPartDataValue(tOp, buf, len) == -1) {
    setErrorCode(tOp);
    DBUG_RETURN(-1);
  }

  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
  theNdbCon->pendingBlobWriteBytes += len;
  DBUG_RETURN(0);
}

int
NdbBlob::updateParts(const char* buf, Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::updateParts");
  DBUG_PRINT("info", ("part=%u count=%u", part, count));
  Uint32 n = 0;
  while (n < count) {
    // use non-stack variable for safety
    thePartLen = thePartSize;
    if (updatePart(buf + n * thePartSize, part + n, thePartLen) == -1)
      DBUG_RETURN(-1);
    n++;
  }
  DBUG_RETURN(0);
}

int
NdbBlob::updatePart(const char* buf, Uint32 part, const Uint16& len)
{
  DBUG_ENTER("NdbBlob::updatePart");
  DBUG_PRINT("info", ("part=%u len=%u", part, (Uint32)len));
  NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
  if (tOp == NULL ||
      tOp->updateTuple() == -1 ||
      setPartKeyValue(tOp, part) == -1 ||
      setPartPkidValue(tOp, theHead.pkid) == -1 ||
      setPartDataValue(tOp, buf, len) == -1) {
    setErrorCode(tOp);
    DBUG_RETURN(-1);
  }

  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
  theNdbCon->pendingBlobWriteBytes += len;
  DBUG_RETURN(0);
}

int
NdbBlob::deletePartsThrottled(Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::deletePartsThrottled");
  DBUG_PRINT("info", ("part=%u count=%u maxPendingBlobWriteBytes=%u", 
                      part, count, theNdbCon->maxPendingBlobWriteBytes));
  
  if (thePartSize)
  {
    do
    {
      /* How much quota left, avoiding underflow? */
      const Uint32 remainingQuota = 
        theNdbCon->maxPendingBlobWriteBytes - 
        MIN(theNdbCon->maxPendingBlobWriteBytes, theNdbCon->pendingBlobWriteBytes);
      const Uint32 maxPartsThisTrip = MAX(remainingQuota / thePartSize, 1); // always read one part
      const Uint32 partsThisTrip= MIN(count, maxPartsThisTrip);
      
      int rc = deleteParts(part, partsThisTrip);
      if (rc != 0)
        DBUG_RETURN(rc);
      
      part+= partsThisTrip;
      count-= partsThisTrip;
      
      if (count != 0)
      {
        /* Execute this batch before defining next */
        if (executePendingBlobWrites() == -1)
          DBUG_RETURN(-1);
      }
    } while (count != 0);
  }

  DBUG_RETURN(0);
}

int
NdbBlob::deleteParts(Uint32 part, Uint32 count)
{
  DBUG_ENTER("NdbBlob::deleteParts");
  DBUG_PRINT("info", ("part=%u count=%u", part, count));
  Uint32 n = 0;
  while (n < count) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
    if (tOp == NULL ||
        tOp->deleteTuple() == -1 ||
        setPartKeyValue(tOp, part + n) == -1) {
      setErrorCode(tOp);
      DBUG_RETURN(-1);
    }

    tOp->m_abortOption = NdbOperation::AbortOnError;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
    theNdbCon->pendingBlobWriteBytes += thePartSize; /* Assume full part */
  }
  DBUG_RETURN(0);
}

/*
 * Number of blob parts not known.  Used to check for race condition
 * when writeTuple is used for insert.  Deletes all parts found.
 */
int
NdbBlob::deletePartsUnknown(Uint32 part)
{
  DBUG_ENTER("NdbBlob::deletePartsUnknown");
  DBUG_PRINT("info", ("part=%u count=all", part));
  if (thePartSize == 0) // tinyblob
    DBUG_RETURN(0);
  static const unsigned maxbat = 256;
  static const unsigned minbat = 1;
  unsigned bat = minbat;
  NdbOperation* tOpList[maxbat];
  Uint32 count = 0;
  while (true) {
    Uint32 n;
    n = 0;
    /* How much quota left, avoiding underflow? */
    Uint32 remainingQuota = theNdbCon->maxPendingBlobWriteBytes - 
      MIN(theNdbCon->maxPendingBlobWriteBytes, theNdbCon->pendingBlobWriteBytes);
    Uint32 deleteQuota = MAX(remainingQuota / thePartSize, 1);
    bat = MIN(deleteQuota, bat);
    while (n < bat) {
      NdbOperation*& tOp = tOpList[n];  // ref
      tOp = theNdbCon->getNdbOperation(theBlobTable);
      if (tOp == NULL ||
          tOp->deleteTuple() == -1 ||
          setPartKeyValue(tOp, part + count + n) == -1) {
        setErrorCode(tOp);
        DBUG_RETURN(-1);
      }
      tOp->m_abortOption= NdbOperation::AO_IgnoreError;
      tOp->m_noErrorPropagation = true;
      theNdbCon->pendingBlobWriteBytes += thePartSize;
      n++;
    }
    DBUG_PRINT("info", ("bat=%u", bat));
    if (theNdbCon->executeNoBlobs(NdbTransaction::NoCommit) == -1)
      DBUG_RETURN(-1);
    n = 0;
    while (n < bat) {
      NdbOperation* tOp = tOpList[n];
      if (tOp->theError.code != 0) {
        if (tOp->theError.code != 626) {
          setErrorCode(tOp);
          DBUG_RETURN(-1);
        }
        // first non-existent part
        DBUG_PRINT("info", ("count=%u", count));
        DBUG_RETURN(0);
      }
      n++;
      count++;
    }
    bat *= 4;
    if (bat > maxbat)
      bat = maxbat;
  }
}

// pending ops

int
NdbBlob::executePendingBlobReads()
{
  DBUG_ENTER("NdbBlob::executePendingBlobReads");
  Uint8 flags = (1 << NdbOperation::ReadRequest);
  if (thePendingBlobOps & flags) {
    if (theNdbCon->executeNoBlobs(NdbTransaction::NoCommit) == -1)
      DBUG_RETURN(-1);
    thePendingBlobOps = 0;
    theNdbCon->thePendingBlobOps = 0;
  }
  DBUG_RETURN(0);
}

int
NdbBlob::executePendingBlobWrites()
{
  DBUG_ENTER("NdbBlob::executePendingBlobWrites");
  Uint8 flags = 0xFF & ~(1 << NdbOperation::ReadRequest);
  if (thePendingBlobOps & flags) {
    if (theNdbCon->executeNoBlobs(NdbTransaction::NoCommit) == -1)
      DBUG_RETURN(-1);
    thePendingBlobOps = 0;
    theNdbCon->thePendingBlobOps = 0;
  }
  DBUG_RETURN(0);
}

// callbacks

int
NdbBlob::invokeActiveHook()
{
  DBUG_ENTER("NdbBlob::invokeActiveHook");
  assert(theState == Active && theActiveHook != NULL);
  int ret = (*theActiveHook)(this, theActiveHookArg);
  if (ret != 0) {
    // no error is set on blob level
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

// blob handle maintenance

/*
 * Prepare blob handle linked to an operation.
 * This one for NdbRecAttr-based operation.
 *
 * For key operation fetches key data from signal data.
 */
int
NdbBlob::atPrepare(NdbTransaction* aCon, NdbOperation* anOp, const NdbColumnImpl* aColumn)
{
  DBUG_ENTER("NdbBlob::atPrepare");
  DBUG_PRINT("info", ("this=%p op=%p con=%p version=%d fixed data=%d",
                      this, theNdbOp, theNdbCon,
                      theBlobVersion, theFixedDataFlag));
  if (atPrepareCommon(aCon, anOp, aColumn) == -1)
    DBUG_RETURN(-1);

  /* For scans using the old RecAttr API, we internally use an
   * NdbRecord.
   * For PK and Index ops, we do not
   */
  theNdbRecordFlag= isScanOp();

  // handle different operation types
  bool supportedOp = false;
  if (isKeyOp()) {
    if (isTableOp()) {
      // get table key
      Uint32* data = (Uint32*)thePackKeyBuf.data;
      Uint32 size = theTable->m_keyLenInWords; // in-out
      if (theNdbOp->getKeyFromTCREQ(data, size) == -1) {
        setErrorCode(NdbBlobImpl::ErrUsage);
        DBUG_RETURN(-1);
      }
      thePackKeyBuf.size = 4 * size;
      thePackKeyBuf.zerorest();
      if (unpackKeyValue(theTable, theKeyBuf) == -1)
        DBUG_RETURN(-1);
    }
    if (isIndexOp()) {
      // get index key
      Uint32* data = (Uint32*)thePackKeyBuf.data;
      Uint32 size = theAccessTable->m_keyLenInWords; // in-out
      if (theNdbOp->getKeyFromTCREQ(data, size) == -1) {
        setErrorCode(NdbBlobImpl::ErrUsage);
        DBUG_RETURN(-1);
      }
      thePackKeyBuf.size = 4 * size;
      thePackKeyBuf.zerorest();
      if (unpackKeyValue(theAccessTable, theAccessKeyBuf) == -1)
        DBUG_RETURN(-1);
    }
    supportedOp = true;
  }
  if (isScanOp())
    supportedOp = true;

  if (! supportedOp) {
    setErrorCode(NdbBlobImpl::ErrUsage);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/*
 * Common prepare code for NdbRecAttr and NdbRecord operations.
 * Checks blob table. Allocates buffers.
 * For read operation adds read of head+inline.
 */
int
NdbBlob::atPrepareCommon(NdbTransaction* aCon, NdbOperation* anOp,
                         const NdbColumnImpl* aColumn)
{
  assert(theState == Idle);
  init();
  // ndb api stuff
  theNdb = anOp->theNdb;
  theNdbCon = aCon;     // for scan, this is the real transaction (m_transConnection)
  theNdbOp = anOp;
  theTable = anOp->m_currentTable;
  theAccessTable = anOp->m_accessTable;
  theColumn = aColumn;
  // prepare blob column and table
  if (prepareColumn() == -1)
    return -1;
  userDefinedPartitioning= (theTable->getFragmentType() == 
                            NdbDictionary::Object::UserDefined);
  /* UserDefined Partitioning
   * If user has set partitionId specifically, take it for
   * Blob head and part operations
   */
  if (userDefinedPartitioning && 
      theNdbOp->theDistrKeyIndicator_) {
    thePartitionId = theNdbOp->getPartitionId();
    DBUG_PRINT("info", ("op partition id: %u", thePartitionId));
  }
  // extra buffers
  theAccessKeyBuf.alloc(theAccessTable->m_keyLenInWords << 2);
  theHeadInlineCopyBuf.alloc(getHeadInlineSize());

  if (isKeyOp()) {
    if (isReadOp()) {
      // upgrade lock mode
      if ((theNdbOp->theLockMode == NdbOperation::LM_CommittedRead) ||
          (theNdbOp->theLockMode == NdbOperation::LM_SimpleRead))
      {
        assert(! theNdbOp->m_blob_lock_upgraded);
        theNdbOp->setReadLockMode(NdbOperation::LM_Read);
        theNdbOp->setReadCommittedBase();
        theNdbOp->m_blob_lock_upgraded = true;

        if (!isIndexOp())
        {
          assert(theNdbOp->theLockHandle == NULL);
          /* If the kernel supports it we'll ask for a lockhandle
           * to allow us to unlock the main table row when the
           * Blob handle is closed
           */
          if (likely(theNdb->getMinDbNodeVersion() >=
                     NDBD_UNLOCK_OP_SUPPORTED))
          {
            /* We've upgraded the lock from Committed/Simple to LM_Read
             * Now modify the read operation to request an NdbLockHandle
             * so that we can unlock the main table op on close()
             */
            if (theNdbOp->m_attribute_record)
            {
              /* NdbRecord op, need to set-up NdbLockHandle */
              int rc = theNdbOp->prepareGetLockHandleNdbRecord();
              if (rc != 0)
              {
                setErrorCode(rc, true);
                return -1;
              }
            }
            else
            {
              /* NdbRecAttr op, request lock handle read */
              int rc = theNdbOp->getLockHandleImpl();
              if (rc != 0)
              {
                setErrorCode(rc, true);
                return -1;
              }
            }
          }
        }        
      }
      // add read of head+inline in this op
      if (getHeadInlineValue(theNdbOp) == -1)
        return -1;
    }
    if (isInsertOp()) {
      // becomes NULL unless set before execute
      theNullFlag = true;
      theLength = 0;
    }
    if (isWriteOp()) {
      // becomes NULL unless set before execute
      theNullFlag = true;
      theLength = 0;
      theHeadInlineUpdateFlag = true;
    }
  }
  if (isScanOp()) {
    /* Upgrade lock mode
     * Unfortunately, this is a bit messy, depending on which
     * type of underlying scan we have
     */
    NdbScanOperation *sop= reinterpret_cast<NdbScanOperation*> (theNdbOp);

    if (sop->m_scanUsingOldApi)
    {
      /* Old Api scans only have saved lockmode state at this pre-finalisation
       * point, so it's easy to change the mode
       */ 
      if ((sop->m_savedLockModeOldApi == NdbOperation::LM_CommittedRead) ||
          (sop->m_savedLockModeOldApi == NdbOperation::LM_SimpleRead))
      {
        assert(! theNdbOp->m_blob_lock_upgraded);
        sop->m_savedLockModeOldApi= NdbOperation::LM_Read;
        theNdbOp->m_blob_lock_upgraded = true;
        theNdbOp->setReadCommittedBase();
      }
    }
    else
    {
      /* NdbRecord defined scans have had most signals built etc, so we need
       * to call the setReadLockMode method to do the right thing to change
       * the lockmode
       */
      if ((sop->theLockMode == NdbOperation::LM_CommittedRead) ||
          (sop->theLockMode == NdbOperation::LM_SimpleRead))
      {
        assert(! theNdbOp->m_blob_lock_upgraded);
        sop->setReadLockMode(NdbOperation::LM_Read);
        theNdbOp->m_blob_lock_upgraded = true;
        theNdbOp->setReadCommittedBase();
      }
    }

    // add read of head+inline in this op
    if (getHeadInlineValue(sop) == -1)
      return -1;
  }
  setState(Prepared);
  return 0;
}

/* Prepare blob handle for key operation, NdbRecord version. */
int
NdbBlob::atPrepareNdbRecord(NdbTransaction* aCon, NdbOperation* anOp,
                            const NdbColumnImpl* aColumn,
                            const NdbRecord *key_record, const char *key_row)
{
  int res;
  DBUG_ENTER("NdbBlob::atPrepareNdbRecord");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, anOp, aCon));

  theNdbRecordFlag= true;
  if (atPrepareCommon(aCon, anOp, aColumn) == -1)
    DBUG_RETURN(-1);

  assert(isKeyOp());

  if (isTableOp())
  {
    res= copyKeyFromRow(key_record, key_row, thePackKeyBuf, theKeyBuf);

    if (theNdbOp->theLockHandle)
    {
      /* Record in the lock handle that we have another
       * open Blob which must be closed before the 
       * main table operation can be unlocked.
       */
      theNdbOp->theLockHandle->m_openBlobCount++;
    }
  }
  else if (isIndexOp())
    res= copyKeyFromRow(key_record, key_row, thePackKeyBuf, theAccessKeyBuf);
  else
    res = -1;
  if (res == -1)
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}

int
NdbBlob::atPrepareNdbRecordTakeover(NdbTransaction* aCon, NdbOperation* anOp,
                                    const NdbColumnImpl* aColumn,
                                    const char *keyinfo, Uint32 keyinfo_bytes)
{
  DBUG_ENTER("NdbBlob::atPrepareNdbRecordTakeover");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, anOp, aCon));

  theNdbRecordFlag= true;
  if (atPrepareCommon(aCon, anOp, aColumn) == -1)
    DBUG_RETURN(-1);

  assert(isKeyOp());

  /* Get primary key. */
  if (keyinfo_bytes > thePackKeyBuf.maxsize)
  {
    assert(false);
    DBUG_RETURN(-1);
  }
  memcpy(thePackKeyBuf.data, keyinfo, keyinfo_bytes);
  thePackKeyBuf.size= keyinfo_bytes;
  thePackKeyBuf.zerorest();
  if (unpackKeyValue(theTable, theKeyBuf) == -1)
    DBUG_RETURN(-1);

  if (theNdbOp->theLockHandle)
  {
    /* Record in the lock handle that we have another
     * open Blob which must be closed before the 
     * main table operation can be unlocked.
     */
    theNdbOp->theLockHandle->m_openBlobCount++;
  }

  DBUG_RETURN(0);
}

/* Prepare blob handle for scan operation, NdbRecord version. */
int
NdbBlob::atPrepareNdbRecordScan(NdbTransaction* aCon, NdbOperation* anOp,
                                const NdbColumnImpl* aColumn)
{
  DBUG_ENTER("NdbBlob::atPrepareNdbRecordScan");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, anOp, aCon));

  theNdbRecordFlag= true;
  if (atPrepareCommon(aCon, anOp, aColumn) == -1)
    DBUG_RETURN(-1);

  assert(isScanOp());

  DBUG_RETURN(0);
}

int
NdbBlob::atPrepare(NdbEventOperationImpl* anOp, NdbEventOperationImpl* aBlobOp, const NdbColumnImpl* aColumn, int version)
{
  DBUG_ENTER("NdbBlob::atPrepare [event]");
  assert(theState == Idle);
  init();
  assert(version == 0 || version == 1);
  theEventBlobVersion = version;
  // ndb api stuff
  theNdb = anOp->m_ndb;
  theEventOp = anOp;
  theBlobEventOp = aBlobOp;
  theTable = anOp->m_eventImpl->m_tableImpl;
  theAccessTable = theTable;
  theColumn = aColumn;
  // prepare blob column and table
  if (prepareColumn() == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("this=%p main op=%p blob op=%p version=%d fixed data=%d",
                       this, anOp, aBlobOp,
                       theBlobVersion, theFixedDataFlag));
  // tinyblob sanity
  assert((theBlobEventOp == NULL) == (theBlobTable == NULL));
  // extra buffers
  theBlobEventDataBuf.alloc(theVarsizeBytes + thePartSize);
  // prepare receive of head+inline
  theHeadInlineRecAttr = theEventOp->getValue(aColumn, theHeadInlineBuf.data, version);
  if (theHeadInlineRecAttr == NULL) {
    setErrorCode(theEventOp);
    DBUG_RETURN(-1);
  }
  // prepare receive of blob part
  if (theBlobEventOp != NULL) {
    const NdbColumnImpl* bc;
    char* buf;
    // one must subscribe to all primary keys
    if (unlikely(theBlobVersion == NDB_BLOB_V1)) {
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnPk]);
      buf = thePackKeyBuf.data;
      theBlobEventPkRecAttr = theBlobEventOp->getValue(bc, buf, version);
      //
      assert(theStripeSize != 0);
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnDist]);
      buf = (char*)&theBlobEventDistValue;
      theBlobEventDistRecAttr = theBlobEventOp->getValue(bc, buf, version);
      //
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnPart]);
      buf = (char*)&theBlobEventPartValue;
      theBlobEventPartRecAttr = theBlobEventOp->getValue(bc, buf, version);
      //
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnData]);
      buf = theBlobEventDataBuf.data;
      theBlobEventDataRecAttr = theBlobEventOp->getValue(bc, buf, version);
      if (unlikely(
            theBlobEventPkRecAttr == NULL ||
            theBlobEventDistRecAttr == NULL ||
            theBlobEventPartRecAttr == NULL ||
            theBlobEventDataRecAttr == NULL
         )) {
        setErrorCode(theBlobEventOp);
        DBUG_RETURN(-1);
      }
    } else {
      const uint noOfKeys = theTable->m_noOfKeys;
      uint n = 0;
      uint i;
      for (i = 0; n < noOfKeys; i++) {
        assert(i < theTable->m_columns.size());
        const NdbColumnImpl* c = theTable->m_columns[i];
        assert(c != NULL);
        if (c->m_pk) {
          bc = theBlobTable->m_columns[n];
          assert(bc != NULL && bc->m_pk);
          NdbRecAttr* ra;
          ra = theBlobEventOp->getValue(bc, (char*)0, version);
          if (unlikely(ra == NULL)) {
            setErrorCode(theBlobEventOp);
            DBUG_RETURN(-1);
          }
          n++;
        }
      }
      if (theStripeSize != 0) {
        bc = theBlobTable->getColumn(theBtColumnNo[BtColumnDist]);
        buf = (char*)&theBlobEventDistValue;
        theBlobEventDistRecAttr = theBlobEventOp->getValue(bc, buf, version);
      }
      //
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnPart]);
      buf = (char*)&theBlobEventPartValue;
      theBlobEventPartRecAttr = theBlobEventOp->getValue(bc, buf, version);
      //
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnPkid]);
      buf = (char*)&theBlobEventPkidValue;
      theBlobEventPkidRecAttr = theBlobEventOp->getValue(bc, buf, version);
      //
      bc = theBlobTable->getColumn(theBtColumnNo[BtColumnData]);
      buf = theBlobEventDataBuf.data;
      theBlobEventDataRecAttr = theBlobEventOp->getValue(bc, buf, version);
      if (unlikely(
            (theStripeSize != 0 && theBlobEventDistRecAttr == NULL) ||
            theBlobEventPartRecAttr == NULL ||
            theBlobEventPkidRecAttr == NULL ||
            theBlobEventDataRecAttr == NULL
         )) {
        setErrorCode(theBlobEventOp);
        DBUG_RETURN(-1);
      }
    }
  }
  setState(Prepared);
  DBUG_RETURN(0);
}

int
NdbBlob::prepareColumn()
{
  DBUG_ENTER("prepareColumn");
  NdbDictionary::Column::Type partType = NdbDictionary::Column::Undefined;
  //
  theBlobVersion = theColumn->getBlobVersion();
  theInlineSize = theColumn->getInlineSize();
  thePartSize = theColumn->getPartSize();
  theStripeSize = theColumn->getStripeSize();
  //
  if (unlikely(theBlobVersion == NDB_BLOB_V1)) {
    theFixedDataFlag = true;
    theHeadSize = (NDB_BLOB_V1_HEAD_SIZE << 2);
    theVarsizeBytes = 0;
    switch (theColumn->getType()) {
    case NdbDictionary::Column::Blob:
      partType = NdbDictionary::Column::Binary;
      theFillChar = 0x0;
      break;
    case NdbDictionary::Column::Text:
      partType = NdbDictionary::Column::Char;
      theFillChar = 0x20;
      break;
    default:
      setErrorCode(NdbBlobImpl::ErrUsage);
      DBUG_RETURN(-1);
    }
    // in V1 stripe size is != 0 (except tinyblob)
    assert(!(thePartSize != 0 && theStripeSize == 0));
    theBtColumnNo[BtColumnPk] = 0;
    theBtColumnNo[BtColumnDist] = 1;
    theBtColumnNo[BtColumnPart] = 2;
    theBtColumnNo[BtColumnData] = 3;
  } else if (theBlobVersion == NDB_BLOB_V2) {
    const Uint32 storageType = (Uint32)theColumn->getStorageType();
    theFixedDataFlag = (storageType != NDB_STORAGETYPE_MEMORY);
    theHeadSize = (NDB_BLOB_V2_HEAD_SIZE << 2);
    theVarsizeBytes = 2;
    switch (theColumn->getType()) {
    case NdbDictionary::Column::Blob:
      if (theFixedDataFlag) {
        partType = NdbDictionary::Column::Binary;
        theFillChar = 0x0;
      } else
        partType = NdbDictionary::Column::Longvarbinary;
      break;
    case NdbDictionary::Column::Text:
      if (theFixedDataFlag) {
        partType = NdbDictionary::Column::Char;
        theFillChar = 0x20;
      } else
        partType = NdbDictionary::Column::Longvarchar;
      break;
    default:
      setErrorCode(NdbBlobImpl::ErrUsage);
      DBUG_RETURN(-1);
    }
    uint off = theTable->m_noOfKeys;
    if (theStripeSize != 0) {
      theBtColumnNo[BtColumnDist] = off;
      off += 1;
    }
    theBtColumnNo[BtColumnPart] = off + 0;
    theBtColumnNo[BtColumnPkid] = off + 1;
    theBtColumnNo[BtColumnData] = off + 2;
  } else {
      setErrorCode(NdbBlobImpl::ErrUsage);
      DBUG_RETURN(-1);
  }
  // sanity check
  assert(theColumn->m_attrSize * theColumn->m_arraySize == getHeadInlineSize());
  if (thePartSize > 0) {
    const NdbTableImpl* bt = NULL;
    const NdbColumnImpl* bc = NULL;
    if ((bt = theColumn->m_blobTable) == NULL ||
        (bc = bt->getColumn(theBtColumnNo[BtColumnData])) == NULL ||
        bc->getType() != partType ||
        bc->getLength() != (int)thePartSize) {
      setErrorCode(NdbBlobImpl::ErrTable);
      DBUG_RETURN(-1);
    }
    // blob table
    theBlobTable = &NdbTableImpl::getImpl(*bt);
  }
  // these buffers are always used
  theKeyBuf.alloc(theTable->m_keyLenInWords << 2);
  thePackKeyBuf.alloc(MAX(theTable->m_keyLenInWords, theAccessTable->m_keyLenInWords) << 2);
  theHeadInlineBuf.alloc(getHeadInlineSize());
  theInlineData = theHeadInlineBuf.data + theHeadSize;
  // no length bytes
  thePartBuf.alloc(thePartSize);
  DBUG_RETURN(0);
}

/*
 * Before execute of prepared operation.  
 * 
 * This method adds any extra operations required to perform the
 * requested Blob operations.
 * This can include : 
 *   Extra read operations added before the 'main table' operation
 *     Read Blob head + inline bytes
 *     Read original table key via access index
 *   Extra operations added after the 'main table' operation
 *     Update Blob head + inline bytes
 *     Insert Blob parts
 * 
 * Generally, operations are performed in preExecute() if possible,
 * and postExecute if not.
 *
 * If this method sets the batch parameter to true, then 
 *  - any remaining Blobs in the current user defined operation
 *    will have their preExecute() method called.
 *  - all operations up to the last one added will be executed with
 *    NoCommit BEFORE the next user-defined operation is executed.
 *  - NdbBlob::postExecute() will be called for all Blobs in the
 *    executed batch.
 *  - Processing will continue with the next user-defined operation
 *    (if any)
 * This control flow can be seen in NdbTransaction::execute().
 */
int
NdbBlob::preExecute(NdbTransaction::ExecType anExecType, 
                    bool& batch)
{
  DBUG_ENTER("NdbBlob::preExecute");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, theNdbOp, theNdbCon));
  DBUG_PRINT("info", ("optype=%d theGetSetBytes=%d theSetFlag=%d", 
                      theNdbOp->theOperationType,
                      theGetSetBytes,
                      theSetFlag));
  if (theState == Invalid)
    DBUG_RETURN(-1);
  assert(theState == Prepared);
  // handle different operation types
  assert(isKeyOp());

  /* Check that a non-nullable blob handle has had a value set 
   * before proceeding 
   */
  if (!theColumn->m_nullable && 
      (isInsertOp() || isWriteOp()) &&
      !theSetFlag)
  {
    /* Illegal null attribute */
    setErrorCode(839);
    DBUG_RETURN(-1);
  }

  if (isReadOp()) {
    if (theGetFlag && theGetSetBytes > theInlineSize) {
      /* Need blob head before proceeding
       * Not safe to do a speculative read of parts, as we do not
       * yet hold a lock on the blob head+inline
       */
      batch = true;
    }
  }
  if (isInsertOp() && theSetFlag) {
    /* If the main operation uses AbortOnError then
     * we can add operations to insert parts and update
     * the Blob head+inline here.
     * If the main operation uses IgnoreError then
     * we have to wait until we are sure that the main
     * insert succeeded before performing any other
     * operations (Otherwise we may perform duplicate insert,
     * and the transaction can fail on the AbortOnError 
     * part operations or corrupt the head with the 
     * post-update operation)
     *
     * Additionally, if the insert is large, we'll defer to
     * postExecute, where we can perform the writes at a more
     * leisurely pace.
     * We defer if we are writing more part data than we have
     * remaining quota for.
     */
    theSetValueInPreExecFlag =
      ((theNdbOp->m_abortOption == NdbOperation::AbortOnError) &&
       ((theGetSetBytes <= theInlineSize) ||   // Parts being written
        ((theGetSetBytes - theInlineSize) <=      // Total part size <=
         (theNdbCon->maxPendingBlobWriteBytes -   //  (Quota -
          MIN(theNdbCon->maxPendingBlobWriteBytes,
              theNdbCon->pendingBlobWriteBytes))  //   bytes_written)
         )));

    if (theSetValueInPreExecFlag)
    {
      DBUG_PRINT("info", 
                 ("Insert extra ops added in preExecute"));
      /* Add operations to insert parts and update the
       * Blob head+inline in the main tables
       */
      if (theGetSetBytes > theInlineSize) {
        // add ops to write rest of a setValue
        assert(theSetBuf != NULL);
        const char* buf = theSetBuf + theInlineSize;
        Uint32 bytes = theGetSetBytes - theInlineSize;
        assert(thePos == theInlineSize);
#ifndef NDEBUG
        Uint32 savePendingBlobWriteBytes = theNdbCon->pendingBlobWriteBytes;
#endif
        if (writeDataPrivate(buf, bytes) == -1)
          DBUG_RETURN(-1);
        /* Assert that we didn't execute inline there */
        assert(theNdbCon->pendingBlobWriteBytes > savePendingBlobWriteBytes);
      }
      
      if (theHeadInlineUpdateFlag)
      {
        NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
        if (tOp == NULL ||
            tOp->updateTuple() == -1 ||
            setTableKeyValue(tOp) == -1 ||
            setHeadInlineValue(tOp) == -1) {
          setErrorCode(NdbBlobImpl::ErrAbort);
          DBUG_RETURN(-1);
        }
        setHeadPartitionId(tOp);

        DBUG_PRINT("info", ("Insert : added op to update head+inline in preExecute"));
      }
    }
    else
    {
      DBUG_PRINT("info", 
                 ("Insert waiting for Blob head insert"));
    }

    /**
     * In both Insert cases (Parts Insert prepared before or after exec)
     * we need to force execution now.
     * This is to avoid potential adverse interactions with other
     * operations on the same blob row in the same batch observing
     * partially updated blob states.
     *
     * This defeats batching in many cases.
     *
     * A future optimisation would be to identify cases where the same
     * key is operated upon multiple times in a single batch and serialise
     * those specifically, allowing more batching in the more normal
     * case of disjoint keys.
     */
    batch= true;
  }

  if (isTableOp()) {
    if (isUpdateOp() || isWriteOp() || isDeleteOp()) {
      // add operation before main table op to read head+inline
      NdbOperation* tOp = theNdbCon->getNdbOperation(theTable, theNdbOp);
      /*
       * If main op is from take over scan lock, the added read is done
       * as committed read:
       *
       * In normal transactional case, the row is locked by us and
       * committed read returns same as normal read.
       *
       * In current TRUNCATE TABLE, the deleting trans is committed in
       * batches and then restarted with new trans id.  A normal read
       * would hang on the scan delete lock and then fail.
       */
      NdbOperation::LockMode lockMode =
        ! isTakeOverOp() ?
          NdbOperation::LM_Read : NdbOperation::LM_CommittedRead;
      if (tOp == NULL ||
          tOp->readTuple(lockMode) == -1 ||
          setTableKeyValue(tOp) == -1 ||
          getHeadInlineValue(tOp) == -1) {
        setErrorCode(tOp);
        DBUG_RETURN(-1);
      }
      setHeadPartitionId(tOp);

      if (isWriteOp()) {
        /* There may be no data currently, so ignore tuple not found etc. */
        tOp->m_abortOption = NdbOperation::AO_IgnoreError;
        tOp->m_noErrorPropagation = true;
      }
      theHeadInlineReadOp = tOp;
      // TODO : Could reuse this op for fetching other blob heads in 
      //        the request?
      //        Add their getHeadInlineValue() calls to this, rather
      //        than having separate ops?  (Similar to Index read below)
      // execute immediately
      // TODO : Why can't we continue with pre-execute of other user ops?
      //        Rationales that occur:
      //          - We're trying to keep user's op order consistent - 
      //            1 op completes before another starts.  
      //            - They probably shouldn't rely on this
      //            - Maybe it makes failure more atomic w.r.t. separate
      //              operations on Blobs
      //          - Or perhaps error handling is easier?
      batch = true;
      DBUG_PRINT("info", ("added op before to read head+inline"));
    }
  }
  if (isIndexOp()) {
    // add op before this one to read table key
    NdbBlob* tFirstBlob = theNdbOp->theBlobList;
    if (this == tFirstBlob) {
      // first blob does it for all
      if (g_ndb_blob_ok_to_read_index_table) {
        /* Cannot work for userDefinedPartitioning + write() op as
         * we need to read the 'main' partition Id
         * Maybe this branch should be removed?
         */
        assert(!userDefinedPartitioning);
        Uint32 pkAttrId = theAccessTable->getNoOfColumns() - 1;
        NdbOperation* tOp = theNdbCon->getNdbOperation(theAccessTable, theNdbOp);
        if (tOp == NULL ||
            tOp->readTuple() == -1 ||
            setAccessKeyValue(tOp) == -1 ||
            tOp->getValue(pkAttrId, thePackKeyBuf.data) == NULL) {
          setErrorCode(tOp);
          DBUG_RETURN(-1);
        }
      } else {
        NdbIndexOperation* tOp = theNdbCon->getNdbIndexOperation(theAccessTable->m_index, theTable, theNdbOp);
        if (tOp == NULL ||
            tOp->readTuple() == -1 ||
            setAccessKeyValue(tOp) == -1 ||
            getTableKeyValue(tOp) == -1) {
          setErrorCode(tOp);
          DBUG_RETURN(-1);
        }
        if (userDefinedPartitioning && isWriteOp())
        {
          /* Index Write op does not perform head read before deleting parts
           * as it cannot safely IgnoreErrors.
           * To get partitioning right we read partition id for main row
           * here.
           */
          thePartitionIdRecAttr = tOp->getValue_impl(&NdbColumnImpl::getImpl(*NdbDictionary::Column::FRAGMENT));
          
          if (thePartitionIdRecAttr == NULL) {
            setErrorCode(tOp);
            DBUG_RETURN(-1);
          }
        }
        if (isReadOp() && theNdbOp->getReadCommittedBase())
        {
          DBUG_PRINT("info", ("Set ReadCommittedBase on UI lookup"));
          tOp->setReadCommittedBase();
        }
      }
      DBUG_PRINT("info", ("Index op : added op before to read table key"));
    }
    if (isUpdateOp() || isDeleteOp()) {
      // add op before this one to read head+inline via index
      NdbIndexOperation* tOp = theNdbCon->getNdbIndexOperation(theAccessTable->m_index, theTable, theNdbOp);
      if (tOp == NULL ||
          tOp->readTuple() == -1 ||
          setAccessKeyValue(tOp) == -1 ||
          getHeadInlineValue(tOp) == -1) {
        setErrorCode(tOp);
        DBUG_RETURN(-1);
      }
      theHeadInlineReadOp = tOp;
      // execute immediately
      // TODO : Why execute immediately?  We could continue with other blobs
      // etc. here
      batch = true;
      DBUG_PRINT("info", ("added index op before to read head+inline"));
    }
    if (isWriteOp()) {
      // XXX until IgnoreError fixed for index op
      batch = true;
    }
  }
  if (isWriteOp()) {
    if (theSetFlag) {
      // write head+inline now
      theNullFlag = true;
      theLength = 0;
      /* Copy data into the headinline buffer */
      if (theSetBuf != NULL) {
        Uint32 n = theGetSetBytes;
        if (n > theInlineSize)
          n = theInlineSize;
        assert(thePos == 0);
        if (writeDataPrivate(theSetBuf, n) == -1)
          DBUG_RETURN(-1);
      }
      /*
       * We set the value of the blob head and inline data here if possible.
       * Note that the length is being set to max theInlineSize.  This will
       * be written with the correct length later if necessary.
       */
      if (!theNdbRecordFlag)
      {
        if (setHeadInlineValue(theNdbOp) == -1)
          DBUG_RETURN(-1);
      }
      else
      {
        /* For table based NdbRecord writes we can set the head+inline 
         * bytes here.  For index based writes, we need to wait until 
         * after the execute for the table key data to be available.
         * TODO : Is it worth doing this at all?
         */
        if (isTableOp())
        {
          /* NdbRecord - add an update operation after the main op */
          NdbOperation* tOp = 
            theNdbCon->getNdbOperation(theTable);
          if (tOp == NULL ||
              tOp->updateTuple() == -1 ||
              setTableKeyValue(tOp) == -1 ||
              setHeadInlineValue(tOp) == -1) {
            setErrorCode(NdbBlobImpl::ErrAbort);
            DBUG_RETURN(-1);
          }
          setHeadPartitionId(tOp);

          DBUG_PRINT("info", ("NdbRecord table write : added op to update head+inline"));
        }
      }
      /* Save the contents of the head inline buf for postExecute
       * It may get overwritten by the read operation injected
       * above
       */
      theHeadInlineCopyBuf.copyfrom(theHeadInlineBuf);
    }
  }
  if (theActiveHook != NULL) {
    // need blob head for callback
    batch = true;
  }
  DBUG_PRINT("info", ("batch=%u", batch));
  DBUG_RETURN(0);
}

/*
 * After execute, for each Blob in an operation.  If already Active, 
 * this routine has been done previously and is not rerun.  
 * Operations which requested a no-commit batch can add new operations 
 * after this one.  They are added before any remaining prepared user 
 * operations (See NdbTransaction::execute())
 *
 * This method has the following duties : 
 *  - Operation specific duties : 
 *    - Index based ops : Store main table key retrieved in preExecute
 *    - Read ops : Store read head+inline and read parts (inline execute)
 *    - Update ops : Store read head+inline and update parts (inline execute)
 *    - Table based write : Either store read head+inline and delete then 
 *                          insert parts and head+inline (inline execute) OR
 *                          Perform deletePartsUnknown() to avoid lockless
 *                          race with another transaction, then update head
 *                          and insert parts (inline execute)
 *    - Index based write : Always perform deletePartsUnknown based on 
 *                          fetched main table key then update head+inline
 *                          and insert parts (inline execute)
 *                          Rationale: Couldn't read head+inline safely as
 *                          Index ops don't support IgnoreError so could
 *                          cause Txn fail for write()?
 *    - Delete op : Store read head+inline info and use to delete parts
 *                  (inline execute)
 *  - Change Blob handle state to Active
 *  - Execute user's activeHook function if set
 *  - Add an operation to update the Blob's head+inline bytes if
 *    necesary 
 */
int
NdbBlob::postExecute(NdbTransaction::ExecType anExecType)
{
  DBUG_ENTER("NdbBlob::postExecute");
  DBUG_PRINT("info", ("this=%p op=%p con=%p anExecType=%u", this, theNdbOp, theNdbCon, anExecType));
  if (theState == Closed)
    DBUG_RETURN(0); // Nothing to do here 
  if (theState == Invalid)
    DBUG_RETURN(-1);
  if (theState == Active) {
    setState(anExecType == NdbTransaction::NoCommit ? Active : Closed);
    DBUG_PRINT("info", ("skip active"));
    DBUG_RETURN(0);
  }
  assert(theState == Prepared);
  setState(anExecType == NdbTransaction::NoCommit ? Active : Closed);
  assert(isKeyOp());
  if (isIndexOp()) {
    NdbBlob* tFirstBlob = theNdbOp->theBlobList;
    if (this == tFirstBlob) {
      packKeyValue(theTable, theKeyBuf);
    } else {
      // copy key from first blob
      theKeyBuf.copyfrom(tFirstBlob->theKeyBuf);
      thePackKeyBuf.copyfrom(tFirstBlob->thePackKeyBuf);
      thePackKeyBuf.zerorest();
    }
  }
  if (isReadOp()) {
    /*
      We injected a read of blob head into the operation, and need to
      set theLength and theNullFlag from it.
    */
    getHeadFromRecAttr();

    if (setPos(0) == -1)
      DBUG_RETURN(-1);
    if (theGetFlag) {
      assert(theGetSetBytes == 0 || theGetBuf != 0);
      assert(theGetSetBytes <= theInlineSize ||
	     anExecType == NdbTransaction::NoCommit);
      Uint32 bytes = theGetSetBytes;
      if (readDataPrivate(theGetBuf, bytes) == -1)
        DBUG_RETURN(-1);
    }
  }
  if (isInsertOp() && theSetFlag) {
    /* For Inserts where the main table operation is IgnoreError, 
     * we perform extra operations on the head and inline parts
     * now, as we know that the main table row was inserted 
     * successfully.
     *
     * Additionally, if the insert was large, we deferred writing
     * until now to better control the flow of part operations.
     * See preExecute()
     */
    if (! theSetValueInPreExecFlag)
    {
      DBUG_PRINT("info", ("Insert adding extra ops"));
      /* Check the main table op for an error (don't proceed if 
       * it failed) 
       */
      if (theNdbOp->theError.code == 0)
      {
        /* Add operations to insert parts and update the
         * Blob head+inline in the main table
         */
        if (theGetSetBytes > theInlineSize) {
          // add ops to write rest of a setValue
          assert(theSetBuf != NULL);
          const char* buf = theSetBuf + theInlineSize;
          Uint32 bytes = theGetSetBytes - theInlineSize;
          assert(thePos == theInlineSize);
          if (writeDataPrivate(buf, bytes) == -1)
            DBUG_RETURN(-1);
        }
        
        if (theHeadInlineUpdateFlag)
        {
          NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
          if (tOp == NULL ||
              tOp->updateTuple() == -1 ||
              setTableKeyValue(tOp) == -1 ||
              setHeadInlineValue(tOp) == -1) {
            setErrorCode(NdbBlobImpl::ErrAbort);
            DBUG_RETURN(-1);
          }
          setHeadPartitionId(tOp);

          DBUG_PRINT("info", ("Insert : added op to update head+inline"));

          /**
           * Force write back to ensure blob state stable for any subsequent
           * batched operation on the same key
           */
          thePendingBlobOps |= (1 << NdbOperation::WriteRequest);
          theNdbCon->thePendingBlobOps |= (1 << NdbOperation::WriteRequest);
          if (executePendingBlobWrites() != 0)
          {
            DBUG_PRINT("info", ("Failed flushing pending operations"));
            DBUG_RETURN(-1);
          }
        }
      }
      // NOTE : Could map IgnoreError insert error onto Blob here
    }
  }

  if (isUpdateOp()) {
    assert(anExecType == NdbTransaction::NoCommit);
    getHeadFromRecAttr();
    if (theSetFlag) {
      // setValue overwrites everything
      if (theSetBuf != NULL) {
        if (truncate(0) == -1)
          DBUG_RETURN(-1);
        assert(thePos == 0);
        if (writeDataPrivate(theSetBuf, theGetSetBytes) == -1)
          DBUG_RETURN(-1);
      } else {
        if (setNull() == -1)
          DBUG_RETURN(-1);
      }
    }
  }
  if (isWriteOp() && isTableOp()) {
    assert(anExecType == NdbTransaction::NoCommit);
    if (theHeadInlineReadOp->theError.code == 0) {
      int tNullFlag = theNullFlag;
      Uint64 tLength = theLength;
      Uint64 tPos = thePos;
      getHeadFromRecAttr();
      DBUG_PRINT("info", ("tuple found"));
      if (truncate(0) == -1)
        DBUG_RETURN(-1);
      // restore previous head+inline
      theHeadInlineBuf.copyfrom(theHeadInlineCopyBuf);
      theNullFlag = tNullFlag;
      theLength = tLength;
      thePos = tPos;
    } else {
      if (theHeadInlineReadOp->theError.code != 626) {
        setErrorCode(theHeadInlineReadOp);
        DBUG_RETURN(-1);
      }
      DBUG_PRINT("info", ("tuple not found"));
      /*
       * Read found no tuple but it is possible that a tuple was
       * created after the read by another transaction.  Delete all
       * blob parts which may exist.
       */
      if (deletePartsUnknown(0) == -1)
        DBUG_RETURN(-1);
    }
    if (theSetFlag && theGetSetBytes > theInlineSize) {
      assert(theSetBuf != NULL);
      const char* buf = theSetBuf + theInlineSize;
      Uint32 bytes = theGetSetBytes - theInlineSize;
      assert(thePos == theInlineSize);
      if (writeDataPrivate(buf, bytes) == -1)
          DBUG_RETURN(-1);
    }
  }
  if (isWriteOp() && isIndexOp()) {
    // XXX until IgnoreError fixed for index op
    if (userDefinedPartitioning)
    {
      /* For Index Write with UserDefined partitioning, we get the
       * partition id from the main table key read created in 
       * preExecute().
       * Extra complexity as only the first Blob does the read, other
       * Blobs grab result from first.
       */
      if (thePartitionIdRecAttr != NULL)
      {
        assert( this == theNdbOp->theBlobList );
        Uint32 id= thePartitionIdRecAttr->u_32_value();
        assert( id != noPartitionId() );
        DBUG_PRINT("info", ("Index write, setting partition id to %d", id));
        thePartitionId= id;
      }
      else
      {
        /* First Blob (not us) in this op got the partition Id */
        assert( theNdbOp->theBlobList );
        assert( this != theNdbOp->theBlobList );

        thePartitionId= theNdbOp->theBlobList->thePartitionId;

        assert(thePartitionId != noPartitionId());
      }
    }
    if (deletePartsUnknown(0) == -1)
      DBUG_RETURN(-1);
    if (theSetFlag && theGetSetBytes > theInlineSize) {
      assert(theSetBuf != NULL);
      const char* buf = theSetBuf + theInlineSize;
      Uint32 bytes = theGetSetBytes - theInlineSize;
      assert(thePos == theInlineSize);
      if (writeDataPrivate(buf, bytes) == -1)
          DBUG_RETURN(-1);
    }
  }
  if (isDeleteOp()) {
    assert(anExecType == NdbTransaction::NoCommit);
    getHeadFromRecAttr();
    if (deletePartsThrottled(0, getPartCount()) == -1)
      DBUG_RETURN(-1);
  }
  setState(anExecType == NdbTransaction::NoCommit ? Active : Closed);
  // activation callback
  if (theActiveHook != NULL) {
    if (invokeActiveHook() == -1)
      DBUG_RETURN(-1);
  }
  /* Cope with any changes to the head */
  if (anExecType == NdbTransaction::NoCommit && theHeadInlineUpdateFlag) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
    if (tOp == NULL ||
       tOp->updateTuple() == -1 ||
       setTableKeyValue(tOp) == -1 ||
       setHeadInlineValue(tOp) == -1) {
      setErrorCode(NdbBlobImpl::ErrAbort);
      DBUG_RETURN(-1);
    }
    setHeadPartitionId(tOp);

    tOp->m_abortOption = NdbOperation::AbortOnError;
    DBUG_PRINT("info", ("added op to update head+inline"));

    /**
     * Force write back to ensure blob state stable for any subsequent
     * batched operation on the same key
     */
    thePendingBlobOps |= (1 << NdbOperation::WriteRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::WriteRequest);
    if (executePendingBlobWrites() != 0)
    {
      DBUG_PRINT("info", ("Failed flushing pending operations"));
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

/*
 * Before commit of completed operation.  For write add operation to
 * update head+inline if necessary.  This code is the same as the
 * last part of postExecute()
 */
int
NdbBlob::preCommit()
{
  DBUG_ENTER("NdbBlob::preCommit");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, theNdbOp, theNdbCon));
  if (theState == Closed)
    DBUG_RETURN(0); // Nothing to do here
  if (theState == Invalid)
    DBUG_RETURN(-1);
  if (unlikely((theState == Prepared) && 
               (theNdbCon->commitStatus() == NdbTransaction::Aborted)))
  {
    /* execute(Commit) called after transaction aborted from kernel
     * Do nothing here - the call will fail later.
     */
    DBUG_RETURN(0);
  }
  assert(theState == Active);
  assert(isKeyOp());
  if (isInsertOp() || isUpdateOp() || isWriteOp()) {
    if (theHeadInlineUpdateFlag) {
        // add an operation to update head+inline
        NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
        if (tOp == NULL ||
            tOp->updateTuple() == -1 ||
            setTableKeyValue(tOp) == -1 ||
            setHeadInlineValue(tOp) == -1) {
          setErrorCode(NdbBlobImpl::ErrAbort);
          DBUG_RETURN(-1);
        }
        setHeadPartitionId(tOp);
        
        tOp->m_abortOption = NdbOperation::AbortOnError;
        DBUG_PRINT("info", ("added op to update head+inline"));
    }
  }
  DBUG_RETURN(0);
}

/*
  After next scan result.  Handle like read op above. NdbRecAttr version.
  Obtain the primary key from KEYINFO20.
 */
int
NdbBlob::atNextResult()
{
  DBUG_ENTER("NdbBlob::atNextResult");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, theNdbOp, theNdbCon));
  if (theState == Invalid)
    DBUG_RETURN(-1);
  assert(isScanOp());
  // get primary key
  { NdbScanOperation* tScanOp = (NdbScanOperation*)theNdbOp;
    Uint32* data = (Uint32*)thePackKeyBuf.data;
    unsigned size = theTable->m_keyLenInWords; // in-out
    if (tScanOp->getKeyFromKEYINFO20(data, size) == -1) {
      setErrorCode(NdbBlobImpl::ErrUsage);
      DBUG_RETURN(-1);
    }
    thePackKeyBuf.size = 4 * size;
    thePackKeyBuf.zerorest();
    if (unpackKeyValue(theTable, theKeyBuf) == -1)
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(atNextResultCommon());
}

/*
  After next scan result, NdbRecord version.
  For NdbRecord, the keyinfo is given as parameter.
*/
int
NdbBlob::atNextResultNdbRecord(const char *keyinfo, Uint32 keyinfo_bytes)
{
  DBUG_ENTER("NdbBlob::atNextResultNdbRecord");
  DBUG_PRINT("info", ("this=%p op=%p con=%p keyinfo_bytes=%lu",
                      this, theNdbOp, theNdbCon,
                      (unsigned long)keyinfo_bytes));
  if (theState == Invalid)
    DBUG_RETURN(-1);
  assert(isScanOp());
  /* Get primary key. */
  memcpy(thePackKeyBuf.data, keyinfo, keyinfo_bytes);
  thePackKeyBuf.size= keyinfo_bytes;
  thePackKeyBuf.zerorest();
  if (unpackKeyValue(theTable, theKeyBuf) == -1)
    DBUG_RETURN(-1);

  DBUG_RETURN(atNextResultCommon());
}

/* After next scan result. Stuff common to NdbRecAttr and NdbRecord case. */
int
NdbBlob::atNextResultCommon()
{
  DBUG_ENTER("NdbBlob::atNextResultCommon");
  // discard previous partition id before reading new one
  thePartitionId = noPartitionId();
  getHeadFromRecAttr();
  if (setPos(0) == -1)
    DBUG_RETURN(-1);
  if (theGetFlag) {
    assert(theGetSetBytes == 0 || theGetBuf != 0);
    Uint32 bytes = theGetSetBytes;
    if (readDataPrivate(theGetBuf, bytes) == -1)
      DBUG_RETURN(-1);
  }
  setState(Active);
  // activation callback
  if (theActiveHook != NULL) {
    if (invokeActiveHook() == -1)
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/*
 * After next event on main table.
 */
int
NdbBlob::atNextEvent()
{
  DBUG_ENTER("NdbBlob::atNextEvent");
  Uint32 optype = 
    SubTableData::getOperation(theEventOp->m_data_item->sdata->requestInfo);
  DBUG_PRINT("info", ("this=%p op=%p blob op=%p version=%d optype=%u", this, theEventOp, theBlobEventOp, theEventBlobVersion, optype));
  if (theState == Invalid)
    DBUG_RETURN(-1);
  assert(theEventBlobVersion >= 0);
  if (optype >= NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT)
    DBUG_RETURN(0);
  getHeadFromRecAttr();
  if (theNullFlag == -1) // value not defined
    DBUG_RETURN(0);
  if (setPos(0) == -1)
    DBUG_RETURN(-1);
  setState(Active);
  DBUG_RETURN(0);
}

// misc

const NdbDictionary::Column*
NdbBlob::getColumn()
{
  return theColumn;
}

// errors

void
NdbBlob::setErrorCode(int anErrorCode, bool invalidFlag)
{
  DBUG_ENTER("NdbBlob::setErrorCode");
  DBUG_PRINT("info", ("this=%p code=%u", this, anErrorCode));
  theError.code = anErrorCode;
  // conditionally copy error to operation level
  if (theNdbOp != NULL && theNdbOp->theError.code == 0)
    theNdbOp->setErrorCode(theError.code);
  if (invalidFlag)
    setState(Invalid);
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  if (NdbEnv_GetEnv("NDB_BLOB_ABORT_ON_ERROR", (char*)0, 0)) {
    abort();
  }
#endif
#endif
  DBUG_VOID_RETURN;
}

void
NdbBlob::setErrorCode(NdbOperation* anOp, bool invalidFlag)
{
  int code = 0;
  if (anOp != NULL && (code = anOp->theError.code) != 0)
    ;
  else if ((code = theNdbCon->theError.code) != 0)
    ;
  else if ((code = theNdb->theError.code) != 0)
    ;
  else
    code = NdbBlobImpl::ErrUnknown;
  setErrorCode(code, invalidFlag);
}

void
NdbBlob::setErrorCode(NdbEventOperationImpl* anOp, bool invalidFlag)
{
  int code = 0;
  if ((code = anOp->m_error.code) != 0)
    ;
  else
    code = NdbBlobImpl::ErrUnknown;
  setErrorCode(code, invalidFlag);
}

// info about all blobs in this operation

NdbBlob*
NdbBlob::blobsFirstBlob()
{
  return theNdbOp->theBlobList;
}

NdbBlob*
NdbBlob::blobsNextBlob()
{
  return theNext;
}

const NdbOperation*
NdbBlob::getNdbOperation() const
{
  return theNdbOp;
}

int 
NdbBlob::close(bool execPendingBlobOps)
{
  DBUG_ENTER("NdbBlob::close");
  DBUG_PRINT("info", ("this=%p state=%u", this, theState));

  /* A Blob can only be closed if it is in the Active state
   * with no pending operations
   */
  if (theState != Active)
  {
    /* NdbBlob can only be closed from Active state */
    setErrorCode(4554);
    DBUG_RETURN(-1);
  }

  if (execPendingBlobOps)
  {
    if (thePendingBlobOps != 0)
    {
      if (theNdbCon->executeNoBlobs(NdbTransaction::NoCommit) == -1)
        DBUG_RETURN(-1);
      thePendingBlobOps = 0;
      theNdbCon->thePendingBlobOps = 0;
    }
  } 
  else if (thePendingBlobOps != 0)
  {
    /* NdbBlob cannot be closed with pending operations */
    setErrorCode(4555);
    DBUG_RETURN(-1);
  }

  setState(Closed);

  if (theNdbOp->theLockHandle)
  {
    DBUG_PRINT("info", 
               ("Decrementing lockhandle Blob ref count to %d",
                theNdbOp->theLockHandle->m_openBlobCount -1));

    /* Reduce open blob ref count in main table 
     * operation's lock handle
     * The main table operation can only be unlocked when
     * the LockHandle's open blob refcount is zero.
     */
    assert(theNdbOp->theLockHandle->m_openBlobCount > 0);
    
    theNdbOp->theLockHandle->m_openBlobCount --;
  }

  if (theNdbOp->m_blob_lock_upgraded)
  {
    assert( theNdbOp->theLockMode == NdbOperation::LM_Read );
    
    /* In some upgrade scenarios, kernel may not support
     * unlock, so there will be no LockHandle
     * In that case we revert to the old behaviour - 
     * do nothing and the main table row stays locked until
     * commit / abort.
     */
    if (likely(theNdbOp->theLockHandle != NULL))
    {
      if (theNdbOp->theLockHandle->m_openBlobCount == 0)
      {
        DBUG_PRINT("info",
                   ("Upgraded -> LM_Read lock "
                    "now no longer required.  Issuing unlock "
                    " operation"));
        /* We can now issue an unlock operation for the main
         * table row - it was supposed to be LM_CommittedRead / LM_SimpleRead
         */
        const NdbOperation* op = theNdbCon->unlock(theNdbOp->theLockHandle,
                                                   NdbOperation::AbortOnError);
        
        if (unlikely(op == NULL))
        {
          /* setErrorCode will extract the error from the transaction... */
          setErrorCode((NdbOperation*) NULL, true); // Set Blob to invalid state
          DBUG_RETURN(-1);
        }
        
        thePendingBlobOps |= (1 << NdbOperation::UnlockRequest);
        theNdbCon->thePendingBlobOps |= (1 << NdbOperation::UnlockRequest);
        
        if (unlikely(theNdbCon->releaseLockHandle(theNdbOp->theLockHandle) != 0))
        {
          setErrorCode(theNdbCon->theError.code, true); // Set Blob to invalid state
          DBUG_RETURN(-1);
        }
      }
    }
  }
  
  /*
   * TODO : Release some other resources in the close() call to make it
   * worthwhile for more than unlocking.
   */
  
  DBUG_RETURN(0);
}
