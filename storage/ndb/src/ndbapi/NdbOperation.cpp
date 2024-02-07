/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <signaldata/TcKeyReq.hpp>
#include "API.hpp"

/******************************************************************************
 * NdbOperation(Ndb* aNdb, Table* aTable);
 *
 * Return Value:  None
 * Parameters:    aNdb: Pointers to the Ndb object.
 *                aTable: Pointers to the Table object
 * Remark:        Create an object of NdbOperation.
 ****************************************************************************/
NdbOperation::NdbOperation(Ndb *aNdb, NdbOperation::Type aType)
    : m_type(aType),
      theReceiver(aNdb),
      theErrorLine(0),
      theNdb(aNdb),
      // theTable(aTable),
      theNdbCon(nullptr),
      theNext(nullptr),
      theTCREQ(nullptr),
      theFirstATTRINFO(nullptr),
      theCurrentATTRINFO(nullptr),
      theTotalCurrAI_Len(0),
      theAI_LenInCurrAI(0),
      theLastKEYINFO(nullptr),

      theFirstLabel(nullptr),
      theLastLabel(nullptr),
      theFirstBranch(nullptr),
      theLastBranch(nullptr),
      theFirstCall(nullptr),
      theLastCall(nullptr),
      theFirstSubroutine(nullptr),
      theLastSubroutine(nullptr),
      theNoOfLabels(0),
      theNoOfSubroutines(0),

      m_currentTable(nullptr),  // theTableId(0xFFFF),
      m_accessTable(nullptr),   // theAccessTableId(0xFFFF),
      // theSchemaVersion(0),
      theTotalNrOfKeyWordInSignal(8),
      theTupKeyLen(0),
      theNoOfTupKeyLeft(0),
      theOperationType(NotDefined),
      theStatus(Init),
      theMagicNumber(0xFE11D0),
      theScanInfo(0),
      m_tcReqGSN(GSN_TCKEYREQ),
      m_keyInfoGSN(GSN_KEYINFO),
      m_attrInfoGSN(GSN_ATTRINFO),
      theBlobList(nullptr),
      m_abortOption(-1),
      m_noErrorPropagation(false),
      theLockHandle(nullptr),
      m_blob_lock_upgraded(false) {
  theReceiver.init(NdbReceiver::NDB_OPERATION, this);
  theError.code = 0;
  m_customData = nullptr;
}
/*****************************************************************************
 * ~NdbOperation();
 *
 * Remark:         Delete tables for connection pointers (id).
 *****************************************************************************/
NdbOperation::~NdbOperation() {
  assert(theRequest == nullptr);  // The same as theTCREQ
}
/******************************************************************************
 *void setErrorCode(int anErrorCode);
 *
 * Remark:         Set an Error Code on operation and
 *                 on connection set an error status.
 *****************************************************************************/
void NdbOperation::setErrorCode(int anErrorCode) const {
  /* Setting an error is considered to be a const
     operation, hence the nasty cast here */
  NdbOperation *pnonConstThis = const_cast<NdbOperation *>(this);

  pnonConstThis->theError.code = anErrorCode;
  theNdbCon->theErrorLine = theErrorLine;
  theNdbCon->theErrorOperation = pnonConstThis;
  if (!(m_abortOption == AO_IgnoreError && m_noErrorPropagation))
    theNdbCon->setOperationErrorCode(anErrorCode);
}

/******************************************************************************
 * void setErrorCodeAbort(int anErrorCode);
 *
 * Remark:         Set an Error Code on operation and on connection set
 *                 an error status.
 *****************************************************************************/
void NdbOperation::setErrorCodeAbort(int anErrorCode) const {
  /* Setting an error is considered to be a const
     operation, hence the nasty cast here */
  NdbOperation *pnonConstThis = const_cast<NdbOperation *>(this);

  pnonConstThis->theError.code = anErrorCode;
  theNdbCon->theErrorLine = theErrorLine;
  theNdbCon->theErrorOperation = pnonConstThis;
  // ignore m_noErrorPropagation
  theNdbCon->setOperationErrorCodeAbort(anErrorCode);
}

/*****************************************************************************
 * int init();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/

int NdbOperation::init(const NdbTableImpl *tab, NdbTransaction *myConnection) {
  theStatus = Init;
  theError.code = 0;
  theErrorLine = 1;
  m_currentTable = m_accessTable = tab;

  theNdbCon = myConnection;
  for (Uint32 i = 0; i < NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY; i++)
    for (int j = 0; j < 3; j++) theTupleKeyDefined[i][j] = 0;

  theFirstATTRINFO = nullptr;
  theCurrentATTRINFO = nullptr;
  theLastKEYINFO = nullptr;

  theTupKeyLen = 0;
  theNoOfTupKeyLeft = tab->getNoOfPrimaryKeys();

  theTotalCurrAI_Len = 0;
  theAI_LenInCurrAI = 0;
  theStartIndicator = 0;
  theCommitIndicator = 0;
  theSimpleIndicator = 0;
  theDirtyIndicator = 0;
  theReadCommittedBaseIndicator = 0;
  theInterpretIndicator = 0;
  theDistrKeyIndicator_ = 0;
  theScanInfo = 0;
  theTotalNrOfKeyWordInSignal = 8;
  theMagicNumber = getMagicNumber();
  m_attribute_record = nullptr;
  theBlobList = nullptr;
  m_abortOption = -1;
  m_noErrorPropagation = false;
  m_flags = 0;
  m_flags |= OF_NO_DISK;
  m_interpreted_code = nullptr;
  m_extraSetValues = nullptr;
  m_numExtraSetValues = 0;
  m_customData = nullptr;

  if (theReceiver.init(NdbReceiver::NDB_OPERATION, this)) {
    // theReceiver sets the error code of its owner
    return -1;
  }

  NdbApiSignal *tSignal = theNdb->getSignal();
  if (tSignal == nullptr) {
    setErrorCode(4000);
    return -1;
  }
  theTCREQ = tSignal;
  theTCREQ->setSignal(m_tcReqGSN, refToBlock(theNdbCon->m_tcRef));

  theAI_LenInCurrAI = 20;
  TcKeyReq *const tcKeyReq = CAST_PTR(TcKeyReq, theTCREQ->getDataPtrSend());
  tcKeyReq->scanInfo = 0;
  theKEYINFOptr = &tcKeyReq->keyInfo[0];
  theATTRINFOptr = &tcKeyReq->attrInfo[0];

  if (theNdb->theImpl->get_ndbapi_config_parameters().m_default_queue_option)
    m_flags |= OF_QUEUEABLE;

  return 0;
}

/******************************************************************************
 * void release();
 *
 * Remark:        Release all objects connected to the operation object.
 *****************************************************************************/
void NdbOperation::release() {
  NdbBlob *tBlob;
  NdbBlob *tSaveBlob;

  /* In case we didn't execute... */
  postExecuteRelease();

  tBlob = theBlobList;
  while (tBlob != nullptr) {
    tSaveBlob = tBlob;
    tBlob = tBlob->theNext;
    theNdb->releaseNdbBlob(tSaveBlob);
  }
  theBlobList = nullptr;
  theReceiver.release();

  theLockHandle = nullptr;
  m_blob_lock_upgraded = false;

#ifndef NDEBUG
  // Poison members to detect late usage
  m_accessTable = m_currentTable = (NdbTableImpl *)0x1;
  theNdbCon = (NdbTransaction *)0x1;
  m_key_record = m_attribute_record = (NdbRecord *)0x1;
#endif
}

void NdbOperation::postExecuteRelease() {
  NdbApiSignal *tSignal;
  NdbApiSignal *tSaveSignal;
  NdbBranch *tBranch;
  NdbBranch *tSaveBranch;
  NdbLabel *tLabel;
  NdbLabel *tSaveLabel;
  NdbCall *tCall;
  NdbCall *tSaveCall;
  NdbSubroutine *tSubroutine;
  NdbSubroutine *tSaveSubroutine;

  tSignal = theRequest; /* TCKEYREQ/TCINDXREQ/SCANTABREQ */
  while (tSignal != nullptr) {
    tSaveSignal = tSignal;
    tSignal = tSignal->next();
    theNdb->releaseSignal(tSaveSignal);
  }
  theRequest = nullptr;
  theLastKEYINFO = nullptr;
#ifdef TODO
  /**
   * Compute correct #cnt signals between theFirstATTRINFO/theCurrentATTRINFO
   */
  if (theFirstATTRINFO) {
    theNdb->releaseSignals(1, theFirstATTRINFO, theCurrentATTRINFO);
  }
#else
  tSignal = theFirstATTRINFO;
  while (tSignal != nullptr) {
    tSaveSignal = tSignal;
    tSignal = tSignal->next();
    theNdb->releaseSignal(tSaveSignal);
  }
#endif
  theFirstATTRINFO = nullptr;
  theCurrentATTRINFO = nullptr;

  if (theInterpretIndicator == 1) {
    tBranch = theFirstBranch;
    while (tBranch != nullptr) {
      tSaveBranch = tBranch;
      tBranch = tBranch->theNext;
      theNdb->releaseNdbBranch(tSaveBranch);
    }
    tLabel = theFirstLabel;
    while (tLabel != nullptr) {
      tSaveLabel = tLabel;
      tLabel = tLabel->theNext;
      theNdb->releaseNdbLabel(tSaveLabel);
    }
    tCall = theFirstCall;
    while (tCall != nullptr) {
      tSaveCall = tCall;
      tCall = tCall->theNext;
      theNdb->releaseNdbCall(tSaveCall);
    }
    tSubroutine = theFirstSubroutine;
    while (tSubroutine != nullptr) {
      tSaveSubroutine = tSubroutine;
      tSubroutine = tSubroutine->theNext;
      theNdb->releaseNdbSubroutine(tSaveSubroutine);
    }
  }
}

NdbRecAttr *NdbOperation::getValue(const char *anAttrName, char *aValue) {
  return getValue_impl(m_currentTable->getColumn(anAttrName), aValue);
}

NdbRecAttr *NdbOperation::getValue(Uint32 anAttrId, char *aValue) {
  return getValue_impl(m_currentTable->getColumn(anAttrId), aValue);
}

NdbRecAttr *NdbOperation::getValue(const NdbDictionary::Column *col,
                                   char *aValue) {
  if (theStatus != UseNdbRecord)
    return getValue_impl(&NdbColumnImpl::getImpl(*col), aValue);

  setErrorCodeAbort(4508);
  /* GetValue not allowed for NdbRecord defined operation */
  return nullptr;
}

int NdbOperation::equal(const char *anAttrName, const char *aValuePassed) {
  const NdbColumnImpl *col = m_accessTable->getColumn(anAttrName);
  if (col == nullptr) {
    setErrorCode(4004);
    return -1;
  } else {
    return equal_impl(col, aValuePassed);
  }
}

int NdbOperation::equal(Uint32 anAttrId, const char *aValuePassed) {
  const NdbColumnImpl *col = m_accessTable->getColumn(anAttrId);
  if (col == nullptr) {
    setErrorCode(4004);
    return -1;
  } else {
    return equal_impl(col, aValuePassed);
  }
}

int NdbOperation::setValue(const char *anAttrName, const char *aValuePassed) {
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrName);
  if (col == nullptr) {
    setErrorCode(4004);
    return -1;
  } else {
    return setValue(col, aValuePassed);
  }
}

int NdbOperation::setValue(Uint32 anAttrId, const char *aValuePassed) {
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrId);
  if (col == nullptr) {
    setErrorCode(4004);
    return -1;
  } else {
    return setValue(col, aValuePassed);
  }
}

NdbBlob *NdbOperation::getBlobHandle(const char *anAttrName) {
  // semantics differs from overloaded 'getBlobHandle(const char*) const'
  // by delegating to the non-const variant of internal getBlobHandle(...),
  // which may create a new BlobHandle
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrName);
  if (col == nullptr) {
    setErrorCode(4004);
    return nullptr;
  } else {
    return getBlobHandle(theNdbCon, col);
  }
}

NdbBlob *NdbOperation::getBlobHandle(Uint32 anAttrId) {
  // semantics differs from overloaded 'getBlobHandle(Uint32) const'
  // by delegating to the non-const variant of internal getBlobHandle(...),
  // which may create a new BlobHandle
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrId);
  if (col == nullptr) {
    setErrorCode(4004);
    return nullptr;
  } else {
    return getBlobHandle(theNdbCon, col);
  }
}

NdbBlob *NdbOperation::getBlobHandle(const char *anAttrName) const {
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrName);
  if (col == nullptr) {
    setErrorCode(4004);
    return nullptr;
  } else {
    return getBlobHandle(theNdbCon, col);
  }
}

NdbBlob *NdbOperation::getBlobHandle(Uint32 anAttrId) const {
  const NdbColumnImpl *col = m_currentTable->getColumn(anAttrId);
  if (col == nullptr) {
    setErrorCode(4004);
    return nullptr;
  } else {
    return getBlobHandle(theNdbCon, col);
  }
}

int NdbOperation::incValue(const char *anAttrName, Uint32 aValue) {
  return incValue(m_currentTable->getColumn(anAttrName), aValue);
}

int NdbOperation::incValue(const char *anAttrName, Uint64 aValue) {
  return incValue(m_currentTable->getColumn(anAttrName), aValue);
}

int NdbOperation::incValue(Uint32 anAttrId, Uint32 aValue) {
  return incValue(m_currentTable->getColumn(anAttrId), aValue);
}

int NdbOperation::incValue(Uint32 anAttrId, Uint64 aValue) {
  return incValue(m_currentTable->getColumn(anAttrId), aValue);
}

int NdbOperation::subValue(const char *anAttrName, Uint32 aValue) {
  return subValue(m_currentTable->getColumn(anAttrName), aValue);
}

int NdbOperation::subValue(const char *anAttrName, Uint64 aValue) {
  return subValue(m_currentTable->getColumn(anAttrName), aValue);
}

int NdbOperation::subValue(Uint32 anAttrId, Uint32 aValue) {
  return subValue(m_currentTable->getColumn(anAttrId), aValue);
}

int NdbOperation::subValue(Uint32 anAttrId, Uint64 aValue) {
  return subValue(m_currentTable->getColumn(anAttrId), aValue);
}

int NdbOperation::read_attr(const char *anAttrName, Uint32 RegDest) {
  return read_attr(m_currentTable->getColumn(anAttrName), RegDest);
}

int NdbOperation::read_attr(Uint32 anAttrId, Uint32 RegDest) {
  return read_attr(m_currentTable->getColumn(anAttrId), RegDest);
}

int NdbOperation::write_attr(const char *anAttrName, Uint32 RegDest) {
  return write_attr(m_currentTable->getColumn(anAttrName), RegDest);
}

int NdbOperation::write_attr(Uint32 anAttrId, Uint32 RegDest) {
  return write_attr(m_currentTable->getColumn(anAttrId), RegDest);
}

const char *NdbOperation::getTableName() const {
  return m_currentTable->m_externalName.c_str();
}

const NdbDictionary::Table *NdbOperation::getTable() const {
  return m_currentTable;
}

NdbTransaction *NdbOperation::getNdbTransaction() const { return theNdbCon; }

int NdbOperation::getLockHandleImpl() {
  assert(!theLockHandle);

  if (likely(((theOperationType == ReadRequest) ||
              (theOperationType == ReadExclusive)) &&
             (m_type == PrimaryKeyAccess) &&
             ((theLockMode == LM_Read) | (theLockMode == LM_Exclusive)))) {
    theLockHandle = theNdbCon->getLockHandle();
    if (!theLockHandle) {
      return 4000;
    }

    /* Now operation has a LockHandle - it'll be
     * filled-in when the operation is prepared prior
     * to execution.
     */
    assert(theLockHandle->m_state == NdbLockHandle::ALLOCATED);
    assert(!theLockHandle->isLockRefValid());

    return 0;
  } else {
    /* getLockHandle only supported for primary key read with a lock */
    return 4549;
  }
}

const NdbLockHandle *NdbOperation::getLockHandle() {
  if (likely(!m_blob_lock_upgraded)) {
    if (theLockHandle == nullptr) {
      int rc = getLockHandleImpl();

      if (likely(rc == 0))
        return theLockHandle;
      else {
        setErrorCode(rc);
        return nullptr;
      }
    }
    /* Return existing LockHandle */
    return theLockHandle;
  } else {
    /* Not allowed to call getLockHandle() on a Blob-upgraded
     * read
     */
    setErrorCode(4549);
    return nullptr;
  }
}

const NdbLockHandle *NdbOperation::getLockHandle() const {
  /* NdbRecord / handle already exists variant */
  return theLockHandle;
}
