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

#include <Ndb.hpp>
#include <NdbDictionaryImpl.hpp>
#include <NdbConnection.hpp>
#include <NdbOperation.hpp>
#include <NdbIndexOperation.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include <NdbScanOperation.hpp>

#ifdef NDB_BLOB_DEBUG
#define DBG(x) \
  do { \
    static const char* p = getenv("NDB_BLOB_DEBUG"); \
    if (p == 0 || *p == 0 || *p == '0') break; \
    static char* prefix = "BLOB"; \
    const char* cname = theColumn == NULL ? "-" : theColumn->m_name.c_str(); \
    ndbout << prefix << " " << hex << (void*)this << " " << cname; \
    ndbout << " " << dec << __LINE__ << " " << x << " " << *this << endl; \
  } while (0)

static char*
ndb_blob_debug(const Uint32* data, unsigned size)
{
  static char buf[200]; // MT irrelevant
  buf[0] = 0;
  for (unsigned i = 0; i < size; i++) {
    unsigned n = strlen(buf);
    if (n + 10 < sizeof(buf))
      sprintf(buf + n, "%*s%08x", i != 0, "", data[i]);
  }
  return buf;
}

#else
#define DBG(x)
#endif

/*
 * Reading index table directly (as a table) is faster but there are
 * bugs or limitations.  Keep the code and make possible to choose.
 */
static const bool g_ndb_blob_ok_to_read_index_table = false;

// state (inline)

inline void
NdbBlob::setState(State newState)
{
  DBG("setState " << newState);
  theState = newState;
}

// define blob table

int
NdbBlob::getBlobTableName(char* btname, Ndb* anNdb, const char* tableName, const char* columnName)
{
  NdbTableImpl* t = anNdb->theDictionary->m_impl.getTable(tableName);
  if (t == NULL)
    return -1;
  NdbColumnImpl* c = t->getColumn(columnName);
  if (c == NULL)
    return -1;
  getBlobTableName(btname, t, c);
  return 0;
}

void
NdbBlob::getBlobTableName(char* btname, const NdbTableImpl* t, const NdbColumnImpl* c)
{
  assert(t != 0 && c != 0 && c->getBlobType());
  memset(btname, 0, BlobTableNameSize);
  sprintf(btname, "NDB$BLOB_%d_%d", (int)t->m_tableId, (int)c->m_attrId);
}

void
NdbBlob::getBlobTable(NdbTableImpl& bt, const NdbTableImpl* t, const NdbColumnImpl* c)
{
  char btname[BlobTableNameSize];
  getBlobTableName(btname, t, c);
  bt.setName(btname);
  bt.setLogging(t->getLogging());
  bt.setFragmentType(t->getFragmentType());
  { NdbDictionary::Column bc("PK");
    bc.setType(NdbDictionary::Column::Unsigned);
    assert(t->m_sizeOfKeysInWords != 0);
    bc.setLength(t->m_sizeOfKeysInWords);
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
    bt.addColumn(bc);
  }
}

// initialization

NdbBlob::NdbBlob()
{
  init();
}

void
NdbBlob::init()
{
  theState = Idle;
  theNdb = NULL;
  theNdbCon = NULL;
  theNdbOp = NULL;
  theTable = NULL;
  theAccessTable = NULL;
  theBlobTable = NULL;
  theColumn = NULL;
  theFillChar = 0;
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
  theHead = NULL;
  theInlineData = NULL;
  theHeadInlineRecAttr = NULL;
  theHeadInlineReadOp = NULL;
  theHeadInlineUpdateFlag = false;
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
NdbBlob::Buf::copyfrom(const NdbBlob::Buf& src)
{
  assert(size == src.size);
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

// computations (inline)

inline Uint32
NdbBlob::getPartNumber(Uint64 pos)
{
  assert(thePartSize != 0 && pos >= theInlineSize);
  return (pos - theInlineSize) / thePartSize;
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
  return (part / theStripeSize) % theStripeSize;
}

// getters and setters

int
NdbBlob::getTableKeyValue(NdbOperation* anOp)
{
  Uint32* data = (Uint32*)theKeyBuf.data;
  unsigned pos = 0;
  DBG("getTableKeyValue");
  for (unsigned i = 0; i < theTable->m_columns.size(); i++) {
    NdbColumnImpl* c = theTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (anOp->getValue_impl(c, (char*)&data[pos]) == NULL) {
        setErrorCode(anOp);
        return -1;
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
  return 0;
}

int
NdbBlob::setTableKeyValue(NdbOperation* anOp)
{
  const Uint32* data = (const Uint32*)theKeyBuf.data;
  DBG("setTableKeyValue key=" << ndb_blob_debug(data, theTable->m_sizeOfKeysInWords));
  const unsigned columns = theTable->m_columns.size();
  unsigned pos = 0;
  for (unsigned i = 0; i < columns; i++) {
    NdbColumnImpl* c = theTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (anOp->equal_impl(c, (const char*)&data[pos], len) == -1) {
        setErrorCode(anOp);
        return -1;
      }
      pos += (len + 3) / 4;
    }
  }
  assert(pos == theKeyBuf.size / 4);
  return 0;
}

int
NdbBlob::setAccessKeyValue(NdbOperation* anOp)
{
  const Uint32* data = (const Uint32*)theAccessKeyBuf.data;
  DBG("setAccessKeyValue key=" << ndb_blob_debug(data, theAccessTable->m_sizeOfKeysInWords));
  const unsigned columns = theAccessTable->m_columns.size();
  unsigned pos = 0;
  for (unsigned i = 0; i < columns; i++) {
    NdbColumnImpl* c = theAccessTable->m_columns[i];
    assert(c != NULL);
    if (c->m_pk) {
      unsigned len = c->m_attrSize * c->m_arraySize;
      if (anOp->equal_impl(c, (const char*)&data[pos], len) == -1) {
        setErrorCode(anOp);
        return -1;
      }
      pos += (len + 3) / 4;
    }
  }
  assert(pos == theAccessKeyBuf.size / 4);
  return 0;
}

int
NdbBlob::setPartKeyValue(NdbOperation* anOp, Uint32 part)
{
  Uint32* data = (Uint32*)theKeyBuf.data;
  unsigned size = theTable->m_sizeOfKeysInWords;
  DBG("setPartKeyValue dist=" << getDistKey(part) << " part=" << part << " key=" << ndb_blob_debug(data, size));
  // TODO use attr ids after compatibility with 4.1.7 not needed
  if (anOp->equal("PK", theKeyBuf.data) == -1 ||
      anOp->equal("DIST", getDistKey(part)) == -1 ||
      anOp->equal("PART", part) == -1) {
    setErrorCode(anOp);
    return -1;
  }
  return 0;
}

int
NdbBlob::getHeadInlineValue(NdbOperation* anOp)
{
  DBG("getHeadInlineValue");
  theHeadInlineRecAttr = anOp->getValue_impl(theColumn, theHeadInlineBuf.data);
  if (theHeadInlineRecAttr == NULL) {
    setErrorCode(anOp);
    return -1;
  }
  return 0;
}

void
NdbBlob::getHeadFromRecAttr()
{
  assert(theHeadInlineRecAttr != NULL);
  theNullFlag = theHeadInlineRecAttr->isNULL();
  assert(theNullFlag != -1);
  theLength = ! theNullFlag ? theHead->length : 0;
  DBG("getHeadFromRecAttr [out]");
}

int
NdbBlob::setHeadInlineValue(NdbOperation* anOp)
{
  DBG("setHeadInlineValue");
  theHead->length = theLength;
  if (theLength < theInlineSize)
    memset(theInlineData + theLength, 0, theInlineSize - theLength);
  assert(theNullFlag != -1);
  const char* aValue = theNullFlag ? 0 : theHeadInlineBuf.data;
  if (anOp->setValue(theColumn, aValue, theHeadInlineBuf.size) == -1) {
    setErrorCode(anOp);
    return -1;
  }
  theHeadInlineUpdateFlag = false;
  return 0;
}

// getValue/setValue

int
NdbBlob::getValue(void* data, Uint32 bytes)
{
  DBG("getValue data=" << hex << data << " bytes=" << dec << bytes);
  if (theGetFlag || theState != Prepared) {
    setErrorCode(ErrState);
    return -1;
  }
  if (! isReadOp() && ! isScanOp()) {
    setErrorCode(ErrUsage);
    return -1;
  }
  if (data == NULL && bytes != 0) {
    setErrorCode(ErrUsage);
    return -1;
  }
  theGetFlag = true;
  theGetBuf = static_cast<char*>(data);
  theGetSetBytes = bytes;
  return 0;
}

int
NdbBlob::setValue(const void* data, Uint32 bytes)
{
  DBG("setValue data=" << hex << data << " bytes=" << dec << bytes);
  if (theSetFlag || theState != Prepared) {
    setErrorCode(ErrState);
    return -1;
  }
  if (! isInsertOp() && ! isUpdateOp() && ! isWriteOp()) {
    setErrorCode(ErrUsage);
    return -1;
  }
  if (data == NULL && bytes != 0) {
    setErrorCode(ErrUsage);
    return -1;
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
        return -1;
    } else {
      theNullFlag = true;
      theLength = 0;
    }
    if (setHeadInlineValue(theNdbOp) == -1)
      return -1;
  }
  return 0;
}

// activation hook

int
NdbBlob::setActiveHook(ActiveHook activeHook, void* arg)
{
  DBG("setActiveHook hook=" << hex << (void*)activeHook << " arg=" << hex << arg);
  if (theState != Prepared) {
    setErrorCode(ErrState);
    return -1;
  }
  theActiveHook = activeHook;
  theActiveHookArg = arg;
  return 0;
}

// misc operations

int
NdbBlob::getNull(bool& isNull)
{
  DBG("getNull");
  if (theState == Prepared && theSetFlag) {
    isNull = (theSetBuf == NULL);
    return 0;
  }
  if (theNullFlag == -1) {
    setErrorCode(ErrState);
    return -1;
  }
  isNull = theNullFlag;
  return 0;
}

int
NdbBlob::setNull()
{
  DBG("setNull");
  if (theNullFlag == -1) {
    if (theState == Prepared) {
      return setValue(0, 0);
    }
    setErrorCode(ErrState);
    return -1;
  }
  if (theNullFlag)
    return 0;
  if (deleteParts(0, getPartCount()) == -1)
    return -1;
  theNullFlag = true;
  theLength = 0;
  theHeadInlineUpdateFlag = true;
  return 0;
}

int
NdbBlob::getLength(Uint64& len)
{
  DBG("getLength");
  if (theState == Prepared && theSetFlag) {
    len = theGetSetBytes;
    return 0;
  }
  if (theNullFlag == -1) {
    setErrorCode(ErrState);
    return -1;
  }
  len = theLength;
  return 0;
}

int
NdbBlob::truncate(Uint64 length)
{
  DBG("truncate [in] length=" << length);
  if (theNullFlag == -1) {
    setErrorCode(ErrState);
    return -1;
  }
  if (theLength > length) {
    if (length > theInlineSize) {
      Uint32 part1 = getPartNumber(length - 1);
      Uint32 part2 = getPartNumber(theLength - 1);
      assert(part2 >= part1);
      if (part2 > part1 && deleteParts(part1 + 1, part2 - part1) == -1)
        return -1;
    } else {
      if (deleteParts(0, getPartCount()) == -1)
        return -1;
    }
    theLength = length;
    theHeadInlineUpdateFlag = true;
    if (thePos > length)
      thePos = length;
  }
  DBG("truncate [out]");
  return 0;
}

int
NdbBlob::getPos(Uint64& pos)
{
  DBG("getPos");
  if (theNullFlag == -1) {
    setErrorCode(ErrState);
    return -1;
  }
  pos = thePos;
  return 0;
}

int
NdbBlob::setPos(Uint64 pos)
{
  DBG("setPos pos=" << pos);
  if (theNullFlag == -1) {
    setErrorCode(ErrState);
    return -1;
  }
  if (pos > theLength) {
    setErrorCode(ErrSeek);
    return -1;
  }
  thePos = pos;
  return 0;
}

// read/write

int
NdbBlob::readData(void* data, Uint32& bytes)
{
  if (theState != Active) {
    setErrorCode(ErrState);
    return -1;
  }
  char* buf = static_cast<char*>(data);
  return readDataPrivate(buf, bytes);
}

int
NdbBlob::readDataPrivate(char* buf, Uint32& bytes)
{
  DBG("readData [in] bytes=" << bytes);
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
  if (len > 0 && thePartSize == 0) {
    setErrorCode(ErrSeek);
    return -1;
  }
  if (len > 0) {
    assert(pos >= theInlineSize);
    Uint32 off = (pos - theInlineSize) % thePartSize;
    // partial first block
    if (off != 0) {
      DBG("partial first block pos=" << pos << " len=" << len);
      Uint32 part = (pos - theInlineSize) / thePartSize;
      if (readParts(thePartBuf.data, part, 1) == -1)
        return -1;
      // need result now
      if (executePendingBlobReads() == -1)
        return -1;
      Uint32 n = thePartSize - off;
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
        return -1;
      Uint32 n = thePartSize * count;
      pos += n;
      buf += n;
      len -= n;
    }
  }
  if (len > 0) {
    // partial last block
    DBG("partial last block pos=" << pos << " len=" << len);
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = (pos - theInlineSize) / thePartSize;
    if (readParts(thePartBuf.data, part, 1) == -1)
      return -1;
    // need result now
    if (executePendingBlobReads() == -1)
      return -1;
    memcpy(buf, thePartBuf.data, len);
    Uint32 n = len;
    pos += n;
    buf += n;
    len -= n;
  }
  assert(len == 0);
  thePos = pos;
  assert(thePos <= theLength);
  DBG("readData [out]");
  return 0;
}

int
NdbBlob::writeData(const void* data, Uint32 bytes)
{
  if (theState != Active) {
    setErrorCode(ErrState);
    return -1;
  }
  const char* buf = static_cast<const char*>(data);
  return writeDataPrivate(buf, bytes);
}

int
NdbBlob::writeDataPrivate(const char* buf, Uint32 bytes)
{
  DBG("writeData [in] bytes=" << bytes);
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
  if (len > 0 && thePartSize == 0) {
    setErrorCode(ErrSeek);
    return -1;
  }
  if (len > 0) {
    assert(pos >= theInlineSize);
    Uint32 off = (pos - theInlineSize) % thePartSize;
    // partial first block
    if (off != 0) {
      DBG("partial first block pos=" << pos << " len=" << len);
      // flush writes to guarantee correct read
      if (executePendingBlobWrites() == -1)
        return -1;
      Uint32 part = (pos - theInlineSize) / thePartSize;
      if (readParts(thePartBuf.data, part, 1) == -1)
        return -1;
      // need result now
      if (executePendingBlobReads() == -1)
        return -1;
      Uint32 n = thePartSize - off;
      if (n > len) {
        memset(thePartBuf.data + off + len, theFillChar, n - len);
        n = len;
      }
      memcpy(thePartBuf.data + off, buf, n);
      if (updateParts(thePartBuf.data, part, 1) == -1)
        return -1;
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
            return -1;
        } else {
          if (insertParts(buf, part + i, 1) == -1)
            return -1;
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
    DBG("partial last block pos=" << pos << " len=" << len);
    assert((pos - theInlineSize) % thePartSize == 0 && len < thePartSize);
    Uint32 part = (pos - theInlineSize) / thePartSize;
    if (theLength > pos + len) {
      // flush writes to guarantee correct read
      if (executePendingBlobWrites() == -1)
        return -1;
      if (readParts(thePartBuf.data, part, 1) == -1)
        return -1;
      // need result now
      if (executePendingBlobReads() == -1)
        return -1;
      memcpy(thePartBuf.data, buf, len);
      if (updateParts(thePartBuf.data, part, 1) == -1)
        return -1;
    } else {
      memcpy(thePartBuf.data, buf, len);
      memset(thePartBuf.data + len, theFillChar, thePartSize - len);
      if (part < getPartCount()) {
        if (updateParts(thePartBuf.data, part, 1) == -1)
          return -1;
      } else {
        if (insertParts(thePartBuf.data, part, 1) == -1)
          return -1;
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
  DBG("writeData [out]");
  return 0;
}

int
NdbBlob::readParts(char* buf, Uint32 part, Uint32 count)
{
  DBG("readParts [in] part=" << part << " count=" << count);
  Uint32 n = 0;
  while (n < count) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
    if (tOp == NULL ||
        tOp->committedRead() == -1 ||
        setPartKeyValue(tOp, part + n) == -1 ||
        tOp->getValue((Uint32)3, buf) == NULL) {
      setErrorCode(tOp);
      return -1;
    }
    tOp->m_abortOption = AbortOnError;
    buf += thePartSize;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::ReadRequest);
  }
  return 0;
}

int
NdbBlob::insertParts(const char* buf, Uint32 part, Uint32 count)
{
  DBG("insertParts [in] part=" << part << " count=" << count);
  Uint32 n = 0;
  while (n < count) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
    if (tOp == NULL ||
        tOp->insertTuple() == -1 ||
        setPartKeyValue(tOp, part + n) == -1 ||
        tOp->setValue((Uint32)3, buf) == -1) {
      setErrorCode(tOp);
      return -1;
    }
    tOp->m_abortOption = AbortOnError;
    buf += thePartSize;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::InsertRequest);
  }
  return 0;
}

int
NdbBlob::updateParts(const char* buf, Uint32 part, Uint32 count)
{
  DBG("updateParts [in] part=" << part << " count=" << count);
  Uint32 n = 0;
  while (n < count) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
    if (tOp == NULL ||
        tOp->updateTuple() == -1 ||
        setPartKeyValue(tOp, part + n) == -1 ||
        tOp->setValue((Uint32)3, buf) == -1) {
      setErrorCode(tOp);
      return -1;
    }
    tOp->m_abortOption = AbortOnError;
    buf += thePartSize;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::UpdateRequest);
  }
  return 0;
}

int
NdbBlob::deleteParts(Uint32 part, Uint32 count)
{
  DBG("deleteParts [in] part=" << part << " count=" << count);
  Uint32 n = 0;
  while (n < count) {
    NdbOperation* tOp = theNdbCon->getNdbOperation(theBlobTable);
    if (tOp == NULL ||
        tOp->deleteTuple() == -1 ||
        setPartKeyValue(tOp, part + n) == -1) {
      setErrorCode(tOp);
      return -1;
    }
    tOp->m_abortOption = AbortOnError;
    n++;
    thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
    theNdbCon->thePendingBlobOps |= (1 << NdbOperation::DeleteRequest);
  }
  return 0;
}

/*
 * Number of blob parts not known.  Used to check for race condition
 * when writeTuple is used for insert.  Deletes all parts found.
 */
int
NdbBlob::deletePartsUnknown(Uint32 part)
{
  DBG("deletePartsUnknown [in] part=" << part << " count=all");
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
        return -1;
      }
      tOp->m_abortOption = IgnoreError;
      n++;
    }
    DBG("deletePartsUnknown: executeNoBlobs [in] bat=" << bat);
    if (theNdbCon->executeNoBlobs(NoCommit) == -1)
      return -1;
    DBG("deletePartsUnknown: executeNoBlobs [out]");
    n = 0;
    while (n < bat) {
      NdbOperation* tOp = tOpList[n];
      if (tOp->theError.code != 0) {
        if (tOp->theError.code != 626) {
          setErrorCode(tOp);
          return -1;
        }
        // first non-existent part
        DBG("deletePartsUnknown [out] count=" << count);
        return 0;
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
  Uint8 flags = (1 << NdbOperation::ReadRequest);
  if (thePendingBlobOps & flags) {
    DBG("executePendingBlobReads: executeNoBlobs [in]");
    if (theNdbCon->executeNoBlobs(NoCommit) == -1)
      return -1;
    DBG("executePendingBlobReads: executeNoBlobs [out]");
    thePendingBlobOps = 0;
    theNdbCon->thePendingBlobOps = 0;
  }
  return 0;
}

int
NdbBlob::executePendingBlobWrites()
{
  Uint8 flags = 0xFF & ~(1 << NdbOperation::ReadRequest);
  if (thePendingBlobOps & flags) {
    DBG("executePendingBlobWrites: executeNoBlobs [in]");
    if (theNdbCon->executeNoBlobs(NoCommit) == -1)
      return -1;
    DBG("executePendingBlobWrites: executeNoBlobs [out]");
    thePendingBlobOps = 0;
    theNdbCon->thePendingBlobOps = 0;
  }
  return 0;
}

// callbacks

int
NdbBlob::invokeActiveHook()
{
  DBG("invokeActiveHook [in]");
  assert(theState == Active && theActiveHook != NULL);
  int ret = (*theActiveHook)(this, theActiveHookArg);
  DBG("invokeActiveHook [out] ret=" << ret);
  if (ret != 0) {
    // no error is set on blob level
    return -1;
  }
  return 0;
}

// blob handle maintenance

/*
 * Prepare blob handle linked to an operation.  Checks blob table.
 * Allocates buffers.  For key operation fetches key data from signal
 * data.  For read operation adds read of head+inline.
 */
int
NdbBlob::atPrepare(NdbConnection* aCon, NdbOperation* anOp, const NdbColumnImpl* aColumn)
{
  assert(theState == Idle);
  // ndb api stuff
  theNdb = anOp->theNdb;
  theNdbCon = aCon;     // for scan, this is the real transaction (m_transConnection)
  theNdbOp = anOp;
  theTable = anOp->m_currentTable;
  theAccessTable = anOp->m_accessTable;
  theColumn = aColumn;
  DBG("atPrepare [in]");
  NdbDictionary::Column::Type partType = NdbDictionary::Column::Undefined;
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
    setErrorCode(ErrUsage);
    return -1;
  }
  // sizes
  theInlineSize = theColumn->getInlineSize();
  thePartSize = theColumn->getPartSize();
  theStripeSize = theColumn->getStripeSize();
  // sanity check
  assert((NDB_BLOB_HEAD_SIZE << 2) == sizeof(Head));
  assert(theColumn->m_attrSize * theColumn->m_arraySize == sizeof(Head) + theInlineSize);
  if (thePartSize > 0) {
    const NdbDictionary::Table* bt = NULL;
    const NdbDictionary::Column* bc = NULL;
    if (theStripeSize == 0 ||
        (bt = theColumn->getBlobTable()) == NULL ||
        (bc = bt->getColumn("DATA")) == NULL ||
        bc->getType() != partType ||
        bc->getLength() != (int)thePartSize) {
      setErrorCode(ErrTable);
      return -1;
    }
    theBlobTable = &NdbTableImpl::getImpl(*bt);
  }
  // buffers
  theKeyBuf.alloc(theTable->m_sizeOfKeysInWords << 2);
  theAccessKeyBuf.alloc(theAccessTable->m_sizeOfKeysInWords << 2);
  theHeadInlineBuf.alloc(sizeof(Head) + theInlineSize);
  theHeadInlineCopyBuf.alloc(sizeof(Head) + theInlineSize);
  thePartBuf.alloc(thePartSize);
  theHead = (Head*)theHeadInlineBuf.data;
  theInlineData = theHeadInlineBuf.data + sizeof(Head);
  // handle different operation types
  bool supportedOp = false;
  if (isKeyOp()) {
    if (isTableOp()) {
      // get table key
      Uint32* data = (Uint32*)theKeyBuf.data;
      unsigned size = theTable->m_sizeOfKeysInWords;
      if (theNdbOp->getKeyFromTCREQ(data, size) == -1) {
        setErrorCode(ErrUsage);
        return -1;
      }
    }
    if (isIndexOp()) {
      // get index key
      Uint32* data = (Uint32*)theAccessKeyBuf.data;
      unsigned size = theAccessTable->m_sizeOfKeysInWords;
      if (theNdbOp->getKeyFromTCREQ(data, size) == -1) {
        setErrorCode(ErrUsage);
        return -1;
      }
    }
    if (isReadOp()) {
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
    supportedOp = true;
  }
  if (isScanOp()) {
    // add read of head+inline in this op
    if (getHeadInlineValue(theNdbOp) == -1)
      return -1;
    supportedOp = true;
  }
  if (! supportedOp) {
    setErrorCode(ErrUsage);
    return -1;
  }
  setState(Prepared);
  DBG("atPrepare [out]");
  return 0;
}

/*
 * Before execute of prepared operation.  May add new operations before
 * this one.  May ask that this operation and all before it (a "batch")
 * is executed immediately in no-commit mode.  In this case remaining
 * prepared operations are saved in a separate list.  They are added
 * back after postExecute.
 */
int
NdbBlob::preExecute(ExecType anExecType, bool& batch)
{
  DBG("preExecute [in]");
  if (theState == Invalid)
    return -1;
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
        return -1;
      if (theHeadInlineUpdateFlag) {
          // add an operation to update head+inline
          NdbOperation* tOp = theNdbCon->getNdbOperation(theTable);
          if (tOp == NULL ||
              tOp->updateTuple() == -1 ||
              setTableKeyValue(tOp) == -1 ||
              setHeadInlineValue(tOp) == -1) {
            setErrorCode(ErrAbort);
            return -1;
          }
          DBG("add op to update head+inline");
      }
    }
  }
  if (isTableOp()) {
    if (isUpdateOp() || isWriteOp() || isDeleteOp()) {
      // add operation before this one to read head+inline
      NdbOperation* tOp = theNdbCon->getNdbOperation(theTable, theNdbOp);
      if (tOp == NULL ||
          tOp->readTuple() == -1 ||
          setTableKeyValue(tOp) == -1 ||
          getHeadInlineValue(tOp) == -1) {
        setErrorCode(tOp);
        return -1;
      }
      if (isWriteOp()) {
        tOp->m_abortOption = IgnoreError;
      }
      theHeadInlineReadOp = tOp;
      // execute immediately
      batch = true;
      DBG("add op before to read head+inline");
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
            tOp->getValue(pkAttrId, theKeyBuf.data) == NULL) {
          setErrorCode(tOp);
          return -1;
        }
      } else {
        NdbIndexOperation* tOp = theNdbCon->getNdbIndexOperation(theAccessTable->m_index, theTable, theNdbOp);
        if (tOp == NULL ||
            tOp->readTuple() == -1 ||
            setAccessKeyValue(tOp) == -1 ||
            getTableKeyValue(tOp) == -1) {
          setErrorCode(tOp);
          return -1;
        }
      }
    }
    DBG("added op before to read table key");
    if (isUpdateOp() || isDeleteOp()) {
      // add op before this one to read head+inline via index
      NdbIndexOperation* tOp = theNdbCon->getNdbIndexOperation(theAccessTable->m_index, theTable, theNdbOp);
      if (tOp == NULL ||
          tOp->readTuple() == -1 ||
          setAccessKeyValue(tOp) == -1 ||
          getHeadInlineValue(tOp) == -1) {
        setErrorCode(tOp);
        return -1;
      }
      if (isWriteOp()) {
        tOp->m_abortOption = IgnoreError;
      }
      theHeadInlineReadOp = tOp;
      // execute immediately
      batch = true;
      DBG("added index op before to read head+inline");
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
          return -1;
      }
      if (setHeadInlineValue(theNdbOp) == -1)
        return -1;
      // the read op before us may overwrite
      theHeadInlineCopyBuf.copyfrom(theHeadInlineBuf);
    }
  }
  if (theActiveHook != NULL) {
    // need blob head for callback
    batch = true;
  }
  DBG("preExecute [out] batch=" << batch);
  return 0;
}

/*
 * After execute, for any operation.  If already Active, this routine
 * has been done previously.  Operations which requested a no-commit
 * batch can add new operations after this one.  They are added before
 * any remaining prepared operations.
 */
int
NdbBlob::postExecute(ExecType anExecType)
{
  DBG("postExecute [in] type=" << anExecType);
  if (theState == Invalid)
    return -1;
  if (theState == Active) {
    setState(anExecType == NoCommit ? Active : Closed);
    DBG("postExecute [skip]");
    return 0;
  }
  assert(theState == Prepared);
  setState(anExecType == NoCommit ? Active : Closed);
  assert(isKeyOp());
  if (isIndexOp()) {
    NdbBlob* tFirstBlob = theNdbOp->theBlobList;
    if (this != tFirstBlob) {
      // copy key from first blob
      assert(theKeyBuf.size == tFirstBlob->theKeyBuf.size);
      memcpy(theKeyBuf.data, tFirstBlob->theKeyBuf.data, tFirstBlob->theKeyBuf.size);
    }
  }
  if (isReadOp()) {
    getHeadFromRecAttr();
    if (setPos(0) == -1)
      return -1;
    if (theGetFlag) {
      assert(theGetSetBytes == 0 || theGetBuf != 0);
      assert(theGetSetBytes <= theInlineSize || anExecType == NoCommit);
      Uint32 bytes = theGetSetBytes;
      if (readDataPrivate(theGetBuf, bytes) == -1)
        return -1;
    }
  }
  if (isUpdateOp()) {
    assert(anExecType == NoCommit);
    getHeadFromRecAttr();
    if (theSetFlag) {
      // setValue overwrites everything
      if (theSetBuf != NULL) {
        if (truncate(0) == -1)
          return -1;
        assert(thePos == 0);
        if (writeDataPrivate(theSetBuf, theGetSetBytes) == -1)
          return -1;
      } else {
        if (setNull() == -1)
          return -1;
      }
    }
  }
  if (isWriteOp() && isTableOp()) {
    assert(anExecType == NoCommit);
    if (theHeadInlineReadOp->theError.code == 0) {
      int tNullFlag = theNullFlag;
      Uint64 tLength = theLength;
      Uint64 tPos = thePos;
      getHeadFromRecAttr();
      DBG("tuple found");
      if (truncate(0) == -1)
        return -1;
      // restore previous head+inline
      theHeadInlineBuf.copyfrom(theHeadInlineCopyBuf);
      theNullFlag = tNullFlag;
      theLength = tLength;
      thePos = tPos;
    } else {
      if (theHeadInlineReadOp->theError.code != 626) {
        setErrorCode(theHeadInlineReadOp);
        return -1;
      }
      DBG("tuple not found");
      /*
       * Read found no tuple but it is possible that a tuple was
       * created after the read by another transaction.  Delete all
       * blob parts which may exist.
       */
      if (deletePartsUnknown(0) == -1)
        return -1;
    }
    if (theSetFlag && theGetSetBytes > theInlineSize) {
      assert(theSetBuf != NULL);
      const char* buf = theSetBuf + theInlineSize;
      Uint32 bytes = theGetSetBytes - theInlineSize;
      assert(thePos == theInlineSize);
      if (writeDataPrivate(buf, bytes) == -1)
          return -1;
    }
  }
  if (isWriteOp() && isIndexOp()) {
    // XXX until IgnoreError fixed for index op
    if (deletePartsUnknown(0) == -1)
      return -1;
    if (theSetFlag && theGetSetBytes > theInlineSize) {
      assert(theSetBuf != NULL);
      const char* buf = theSetBuf + theInlineSize;
      Uint32 bytes = theGetSetBytes - theInlineSize;
      assert(thePos == theInlineSize);
      if (writeDataPrivate(buf, bytes) == -1)
          return -1;
    }
  }
  if (isDeleteOp()) {
    assert(anExecType == NoCommit);
    getHeadFromRecAttr();
    if (deleteParts(0, getPartCount()) == -1)
      return -1;
  }
  setState(anExecType == NoCommit ? Active : Closed);
  // activation callback
  if (theActiveHook != NULL) {
    if (invokeActiveHook() == -1)
      return -1;
  }
  DBG("postExecute [out]");
  return 0;
}

/*
 * Before commit of completed operation.  For write add operation to
 * update head+inline.
 */
int
NdbBlob::preCommit()
{
  DBG("preCommit [in]");
  if (theState == Invalid)
    return -1;
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
          setErrorCode(ErrAbort);
          return -1;
        }
        tOp->m_abortOption = AbortOnError;
        DBG("added op to update head+inline");
    }
  }
  DBG("preCommit [out]");
  return 0;
}

/*
 * After next scan result.  Handle like read op above.
 */
int
NdbBlob::atNextResult()
{
  DBG("atNextResult [in]");
  if (theState == Invalid)
    return -1;
  assert(isScanOp());
  // get primary key
  { Uint32* data = (Uint32*)theKeyBuf.data;
    unsigned size = theTable->m_sizeOfKeysInWords;
    if (((NdbScanOperation*)theNdbOp)->getKeyFromKEYINFO20(data, size) == -1) {
      setErrorCode(ErrUsage);
      return -1;
    }
  }
  getHeadFromRecAttr();
  if (setPos(0) == -1)
    return -1;
  if (theGetFlag) {
    assert(theGetSetBytes == 0 || theGetBuf != 0);
    Uint32 bytes = theGetSetBytes;
    if (readDataPrivate(theGetBuf, bytes) == -1)
      return -1;
  }
  setState(Active);
  // activation callback
  if (theActiveHook != NULL) {
    if (invokeActiveHook() == -1)
      return -1;
  }
  DBG("atNextResult [out]");
  return 0;
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
  DBG("setErrorCode code=" << anErrorCode);
  theError.code = anErrorCode;
  // conditionally copy error to operation level
  if (theNdbOp != NULL && theNdbOp->theError.code == 0)
    theNdbOp->setErrorCode(theError.code);
  if (invalidFlag)
    setState(Invalid);
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
    code = ErrUnknown;
  setErrorCode(code, invalidFlag);
}

void
NdbBlob::setErrorCode(NdbConnection* aCon, bool invalidFlag)
{
  int code = 0;
  if (theNdbCon != NULL && (code = theNdbCon->theError.code) != 0)
    ;
  else if ((code = theNdb->theError.code) != 0)
    ;
  else
    code = ErrUnknown;
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

// debug

#ifdef VM_TRACE
inline int
NdbBlob::getOperationType() const
{
  return theNdbOp != NULL ? theNdbOp->theOperationType : -1;
}

NdbOut&
operator<<(NdbOut& out, const NdbBlob& blob)
{
  ndbout << dec << "o=" << blob.getOperationType();
  ndbout << dec << " s=" << (Uint32) blob.theState;
  ndbout << dec << " n=" << blob.theNullFlag;;
  ndbout << dec << " l=" << blob.theLength;
  ndbout << dec << " p=" << blob.thePos;
  ndbout << dec << " u=" << (Uint32)blob.theHeadInlineUpdateFlag;
  ndbout << dec << " g=" << (Uint32)blob.theGetSetBytes;
  return out;
}
#endif
