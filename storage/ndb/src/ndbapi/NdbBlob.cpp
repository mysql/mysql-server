/* Copyright (C) 2003 MySQL AB

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

#include <Ndb.hpp>
#include <NdbDictionaryImpl.hpp>
#include <NdbTransaction.hpp>
#include <NdbOperation.hpp>
#include <NdbIndexOperation.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include "NdbBlobImpl.hpp"
#include <NdbScanOperation.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <NdbEventOperationImpl.hpp>
#include <NdbEnv.h>
#include "NdbRecord.hpp"

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
    but may change the fragment type if it is UserDefined since it
    must be hash based so that the kernel can handle it on its own.
    It also uses the same tablespaces and it never uses any range or
    list arrays.
  */
  bt.m_primaryTableId = t->m_id;
  bt.m_fd.clear();
  bt.m_ts.clear();
  bt.m_range.clear();
  bt.setFragmentCount(t->getFragmentCount());
  bt.m_tablespace_id = t->m_tablespace_id;
  bt.m_tablespace_version = t->m_tablespace_version;
  switch (t->getFragmentType())
  {
    case NdbDictionary::Object::FragAllSmall:
    case NdbDictionary::Object::FragAllMedium:
    case NdbDictionary::Object::FragAllLarge:
    case NdbDictionary::Object::FragSingle:
      bt.setFragmentType(t->getFragmentType());
      break;
    case NdbDictionary::Object::DistrKeyLin:
    case NdbDictionary::Object::DistrKeyHash:
      bt.setFragmentType(t->getFragmentType());
      break;
    case NdbDictionary::Object::UserDefined:
      bt.setFragmentType(NdbDictionary::Object::DistrKeyHash);
      break;
    default:
      DBUG_ASSERT(0);
      break;
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
      const uint columns = t->m_columns.size();
      const uint noOfKeys = t->m_noOfKeys;
      uint n = 0;
      uint i;
      for (i = 0; n < noOfKeys; i++) {
        assert(i < columns);
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
  snprintf(bename, MAX_TAB_NAME_SIZE, "NDB$BLOBEVENT_%s_%d", e->m_name.c_str(), (int)c->m_column_no);
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
  return (pos - theInlineSize) / thePartSize;
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
  char buf[256];
  DBUG_ENTER("NdbBlob::copyKeyFromRow");

  bool index_flag= isInsertOp();

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

  packedBuf.size= packed - packedBuf.data;
  packedBuf.zerorest();
  assert(unpacked == unpackedBuf.data + unpackedBuf.size);
  DBUG_RETURN(0);
}

void
NdbBlob::getBlobHeadData(const char * & data, Uint32 & byteSize)
{
  prepareSetHeadInlineValue();
  if (theNullFlag)
  {
    data= NULL;
    byteSize= 0;
  }
  else
  {
    data= theHeadInlineBuf.data;
    if (unlikely(theBlobVersion == NDB_BLOB_V1))
      byteSize = theHeadInlineBuf.size;
    else
      byteSize = theHead.varsize + 2;
  }
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
  const unsigned columns = theTable->m_columns.size();
  uint n = 0;
  const uint noOfKeys = theTable->m_noOfKeys;
  unsigned pos = 0;
  for (unsigned i = 0;  n < noOfKeys; i++) {
    assert(i < columns);
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
  theHeadInlineRecAttr = anOp->getValue_impl(theColumn, theHeadInlineBuf.data);
  thePartitionIdRecAttr = anOp->getValue(NdbDictionary::Column::FRAGMENT);
  if (theHeadInlineRecAttr == NULL ||
      thePartitionIdRecAttr == NULL) {
    setErrorCode(anOp);
    DBUG_RETURN(-1);
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
    if (theEventBlobVersion == -1) {
      Uint32 id = thePartitionIdRecAttr->u_32_value();
      DBUG_PRINT("info", ("table partition id: %u", id));
      if (thePartitionId == noPartitionId()) {
        DBUG_PRINT("info", ("discovered here"));
        // setting even in non-partitioned case
        thePartitionId = id;
      } else {
        assert(thePartitionId == id);
      }
    }
  } else {
    theLength = 0;
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
      memset(theInlineData + theLength, 0, theInlineSize - theLength);
  } else {
    // the 2 length bytes are not counted in length
    if (theLength < theInlineSize)
      theHead.varsize = (theHeadSize - 2) + theLength;
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
      In NdbRecAttr case, we set the value of the blob head here.
      In NdbRecord case, this is done in NdbOperation::prepareSendNdbRecord().
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
  DBUG_PRINT("info", ("hook=%p arg=%p", (void*)activeHook, arg));
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
  if (deleteParts(0, getPartCount()) == -1)
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
      if (part2 > part1 && deleteParts(part1 + 1, part2 - part1) == -1)
      DBUG_RETURN(-1);
      Uint32 off = getPartOffset(length);
      if (off != 0) {
        assert(off < thePartSize);
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
      if (deleteParts(0, getPartCount()) == -1)
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
    bytes = theLength - pos;
  Uint32 len = bytes;
  if (len > 0) {
    // inline part
    if (pos < theInlineSize) {
      Uint32 n = theInlineSize - pos;
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
      Uint32 part = (pos - theInlineSize) / thePartSize;
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
      Uint32 part = (pos - theInlineSize) / thePartSize;
      Uint32 count = len / thePartSize;
      if (readParts(buf, part, count) == -1)
        DBUG_RETURN(-1);
      Uint32 n = thePartSize * count;
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (len > 0) {
    // partial last block
    DBUG_PRINT("info", ("partial last block pos=%llu len=%u", pos, len));
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = (pos - theInlineSize) / thePartSize;
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
      Uint32 n = theInlineSize - pos;
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
      Uint32 part = (pos - theInlineSize) / thePartSize;
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
      Uint32 part = (pos - theInlineSize) / thePartSize;
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
      }
    }
  }
  if (len > 0) {
    // partial last block
    DBUG_PRINT("info", ("partial last block pos=%llu len=%u", pos, len));
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = (pos - theInlineSize) / thePartSize;
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
  if (thePartitionId != noPartitionId() &&
      theStripeSize == 0) {
    tOp->setPartitionId(thePartitionId);
  }
  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
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
  if (thePartitionId != noPartitionId() &&
      theStripeSize == 0) {
    tOp->setPartitionId(thePartitionId);
  }
  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
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
  if (thePartitionId != noPartitionId() &&
      theStripeSize == 0) {
    tOp->setPartitionId(thePartitionId);
  }
  tOp->m_abortOption = NdbOperation::AbortOnError;
  thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
  theNdbCon->thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
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
    if (thePartitionId != noPartitionId() &&
        theStripeSize == 0) {
      tOp->setPartitionId(thePartitionId);
    }
    tOp->m_abortOption = NdbOperation::AbortOnError;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
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
    while (n < bat) {
      NdbOperation*& tOp = tOpList[n];  // ref
      tOp = theNdbCon->getNdbOperation(theBlobTable);
      if (tOp == NULL ||
          tOp->deleteTuple() == -1 ||
          setPartKeyValue(tOp, part + count + n) == -1) {
        setErrorCode(tOp);
        DBUG_RETURN(-1);
      }
      if (thePartitionId != noPartitionId() &&
          theStripeSize == 0) {
        tOp->setPartitionId(thePartitionId);
      }
      tOp->m_abortOption= NdbOperation::AO_IgnoreError;
      tOp->m_noErrorPropagation = true;
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
  theNdbRecordFlag= false;
  if (atPrepareCommon(aCon, anOp, aColumn) == -1)
    DBUG_RETURN(-1);

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
  // check if mysql or user has set partition id
  if (theNdbOp->theDistrKeyIndicator_) {
    thePartitionId = theNdbOp->getPartitionId();
    DBUG_PRINT("info", ("op partition id: %u", thePartitionId));
  }
  // extra buffers
  theAccessKeyBuf.alloc(theAccessTable->m_keyLenInWords << 2);
  theHeadInlineCopyBuf.alloc(getHeadInlineSize());

  if (isKeyOp()) {
    if (isReadOp()) {
      // upgrade lock mode
      if (theNdbOp->theLockMode == NdbOperation::LM_CommittedRead)
        theNdbOp->setReadLockMode(NdbOperation::LM_Read);
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
    // upgrade lock mode
    if (theNdbOp->theLockMode == NdbOperation::LM_CommittedRead)
      theNdbOp->setReadLockMode(NdbOperation::LM_Read);
    // add read of head+inline in this op
    if (getHeadInlineValue(theNdbOp) == -1)
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
    res= copyKeyFromRow(key_record, key_row, thePackKeyBuf, theKeyBuf);
  else if (isIndexOp())
    res= copyKeyFromRow(key_record, key_row, thePackKeyBuf, theAccessKeyBuf);
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
      const uint columns = theTable->m_columns.size();
      const uint noOfKeys = theTable->m_noOfKeys;
      uint n = 0;
      uint i;
      for (i = 0; n < noOfKeys; i++) {
        assert(i < columns);
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
  thePackKeyBuf.alloc(max(theTable->m_keyLenInWords, theAccessTable->m_keyLenInWords) << 2);
  theHeadInlineBuf.alloc(getHeadInlineSize());
  theInlineData = theHeadInlineBuf.data + theHeadSize;
  // no length bytes
  thePartBuf.alloc(thePartSize);
  DBUG_RETURN(0);
}

/*
 * Before execute of prepared operation.  May add new operations before
 * this one.  May ask that this operation and all before it (a "batch")
 * is executed immediately in no-commit mode.  In this case remaining
 * prepared operations are saved in a separate list.  They are added
 * back after postExecute.
 */
int
NdbBlob::preExecute(NdbTransaction::ExecType anExecType, bool& batch)
{
  DBUG_ENTER("NdbBlob::preExecute");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, theNdbOp, theNdbCon));
  if (theState == Invalid)
    DBUG_RETURN(-1);
  assert(theState == Prepared);
  // handle different operation types
  assert(isKeyOp());
  if (isReadOp()) {
    if (theGetFlag && theGetSetBytes > theInlineSize) {
      // need blob head before proceeding
      batch = true;
    }
  }
  if (isInsertOp()) {
    if (theSetFlag && theGetSetBytes > theInlineSize) {
      // add ops to write rest of a setValue
      assert(theSetBuf != NULL);
      const char* buf = theSetBuf + theInlineSize;
      Uint32 bytes = theGetSetBytes - theInlineSize;
      assert(thePos == theInlineSize);
      if (writeDataPrivate(buf, bytes) == -1)
        DBUG_RETURN(-1);
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
          if (thePartitionId != noPartitionId()) {
            tOp->setPartitionId(thePartitionId);
          }
          DBUG_PRINT("info", ("added op to update head+inline"));
      }
    }
  }
  if (isTableOp()) {
    if (isUpdateOp() || isWriteOp() || isDeleteOp()) {
      // add operation before this one to read head+inline
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
      if (thePartitionId != noPartitionId()) {
        tOp->setPartitionId(thePartitionId);
      }
      if (isWriteOp()) {
        tOp->m_abortOption = NdbOperation::AO_IgnoreError;
        tOp->m_noErrorPropagation = true;
      }
      theHeadInlineReadOp = tOp;
      // execute immediately
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
      }
    }
    DBUG_PRINT("info", ("added op before to read table key"));
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
      if (isWriteOp()) {
        tOp->m_abortOption = NdbOperation::AO_IgnoreError;
        tOp->m_noErrorPropagation = true;
      }
      theHeadInlineReadOp = tOp;
      // execute immediately
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
      if (theSetBuf != NULL) {
        Uint32 n = theGetSetBytes;
        if (n > theInlineSize)
          n = theInlineSize;
        assert(thePos == 0);
        if (writeDataPrivate(theSetBuf, n) == -1)
          DBUG_RETURN(-1);
      }
      /*
        For NdbRecAttr case, we need to set the value of the blob head here.
        For NdbRecord case, it is done in NdbOperation::prepareSendNdbRecord().
      */
      if (!theNdbRecordFlag)
      {
        if (setHeadInlineValue(theNdbOp) == -1)
          DBUG_RETURN(-1);
      }
      // the read op before us may overwrite
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
 * After execute, for any operation.  If already Active, this routine
 * has been done previously.  Operations which requested a no-commit
 * batch can add new operations after this one.  They are added before
 * any remaining prepared operations.
 */
int
NdbBlob::postExecute(NdbTransaction::ExecType anExecType)
{
  DBUG_ENTER("NdbBlob::postExecute");
  DBUG_PRINT("info", ("this=%p op=%p con=%p anExecType=%u", this, theNdbOp, theNdbCon, anExecType));
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
    if (deleteParts(0, getPartCount()) == -1)
      DBUG_RETURN(-1);
  }
  setState(anExecType == NdbTransaction::NoCommit ? Active : Closed);
  // activation callback
  if (theActiveHook != NULL) {
    if (invokeActiveHook() == -1)
      DBUG_RETURN(-1);
  }
  if (anExecType == NdbTransaction::NoCommit && theHeadInlineUpdateFlag) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
    if (tOp == NULL ||
       tOp->updateTuple() == -1 ||
       setTableKeyValue(tOp) == -1 ||
       setHeadInlineValue(tOp) == -1) {
      setErrorCode(NdbBlobImpl::ErrAbort);
      DBUG_RETURN(-1);
    }
    if (thePartitionId != noPartitionId()) {
      tOp->setPartitionId(thePartitionId);
    }
    tOp->m_abortOption = NdbOperation::AbortOnError;
    DBUG_PRINT("info", ("added op to update head+inline"));
  }
  DBUG_RETURN(0);
}

/*
 * Before commit of completed operation.  For write add operation to
 * update head+inline.
 */
int
NdbBlob::preCommit()
{
  DBUG_ENTER("NdbBlob::preCommit");
  DBUG_PRINT("info", ("this=%p op=%p con=%p", this, theNdbOp, theNdbCon));
  if (theState == Invalid)
    DBUG_RETURN(-1);
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
        if (thePartitionId != noPartitionId()) {
          tOp->setPartitionId(thePartitionId);
        }
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
  if (NdbEnv_GetEnv("NDB_BLOB_ABORT_ON_ERROR", (char*)0, 0)) {
    abort();
  }
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
