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

#include <ndb_global.h>
#include <NdbConnection.hpp>
#include <NdbOperation.hpp>
#include "NdbApiSignal.hpp"
#include "NdbRecAttr.hpp"
#include "NdbUtil.hpp"
#include "NdbBlob.hpp"
#include "ndbapi_limits.h"
#include <signaldata/TcKeyReq.hpp>
#include "NdbDictionaryImpl.hpp"

#include "API.hpp"
#include <NdbOut.hpp>


/******************************************************************************
 * NdbOperation(Ndb* aNdb, Table* aTable);
 *
 * Return Value:  None
 * Parameters:    aNdb: Pointers to the Ndb object.
 *                aTable: Pointers to the Table object
 * Remark:        Creat an object of NdbOperation. 
 ****************************************************************************/
NdbOperation::NdbOperation(Ndb* aNdb) :
  theReceiver(aNdb),
  theErrorLine(0),
  theNdb(aNdb),
  //theTable(aTable),
  theNdbCon(NULL),
  theNext(NULL),
  theTCREQ(NULL),
  theFirstATTRINFO(NULL),
  theCurrentATTRINFO(NULL),
  theTotalCurrAI_Len(0),
  theAI_LenInCurrAI(0),
  theLastKEYINFO(NULL),

  theFirstLabel(NULL),
  theLastLabel(NULL),
  theFirstBranch(NULL),
  theLastBranch(NULL),
  theFirstCall(NULL),
  theLastCall(NULL),
  theFirstSubroutine(NULL),
  theLastSubroutine(NULL),
  theNoOfLabels(0),
  theNoOfSubroutines(0),

  m_currentTable(NULL), //theTableId(0xFFFF),
  m_accessTable(NULL), //theAccessTableId(0xFFFF),
  //theSchemaVersion(0), 
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
  theBlobList(NULL),
  m_abortOption(-1)
{
  theReceiver.init(NdbReceiver::NDB_OPERATION, this);
  theError.code = 0;
}
/*****************************************************************************
 * ~NdbOperation();
 *
 * Remark:         Delete tables for connection pointers (id).
 *****************************************************************************/
NdbOperation::~NdbOperation( )
{
}
/******************************************************************************
 *void setErrorCode(int anErrorCode);
 *
 * Remark:         Set an Error Code on operation and 
 *                 on connection set an error status.
 *****************************************************************************/
void
NdbOperation::setErrorCode(int anErrorCode)
{
  theError.code = anErrorCode;
  theNdbCon->theErrorLine = theErrorLine;
  theNdbCon->theErrorOperation = this;
  theNdbCon->setOperationErrorCode(anErrorCode);
}

/******************************************************************************
 * void setErrorCodeAbort(int anErrorCode);
 *
 * Remark:         Set an Error Code on operation and on connection set 
 *                 an error status.
 *****************************************************************************/
void
NdbOperation::setErrorCodeAbort(int anErrorCode)
{
  theError.code = anErrorCode;
  theNdbCon->theErrorLine = theErrorLine;
  theNdbCon->theErrorOperation = this;
  theNdbCon->setOperationErrorCodeAbort(anErrorCode);
}

/*****************************************************************************
 * int init();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/

int
NdbOperation::init(const NdbTableImpl* tab, NdbConnection* myConnection){
  NdbApiSignal* tSignal;
  theStatus		= Init;
  theError.code		= 0;
  theErrorLine		= 1;
  m_currentTable = m_accessTable = tab;
  
  theNdbCon = myConnection;
  for (Uint32 i=0; i<NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY; i++)
    for (int j=0; j<3; j++)
      theTupleKeyDefined[i][j] = 0;

  theFirstATTRINFO    = NULL;
  theCurrentATTRINFO  = NULL;
  theLastKEYINFO      = NULL;  
  

  theTupKeyLen	    = 0;
  theNoOfTupKeyLeft = tab->getNoOfPrimaryKeys();

  theTotalCurrAI_Len	= 0;
  theAI_LenInCurrAI	= 0;
  theStartIndicator	= 0;
  theCommitIndicator	= 0;
  theSimpleIndicator	= 0;
  theDirtyIndicator	= 0;
  theInterpretIndicator	= 0;
  theDistrKeyIndicator_  = 0;
  theScanInfo        	= 0;
  theTotalNrOfKeyWordInSignal = 8;
  theMagicNumber        = 0xABCDEF01;
  theBlobList = NULL;
  m_abortOption = -1;

  tSignal = theNdb->getSignal();
  if (tSignal == NULL)
  {
    setErrorCode(4000);
    return -1;
  }
  theTCREQ = tSignal;
  theTCREQ->setSignal(m_tcReqGSN);

  theAI_LenInCurrAI = 20;
  TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, theTCREQ->getDataPtrSend());
  tcKeyReq->scanInfo = 0;
  theKEYINFOptr = &tcKeyReq->keyInfo[0];
  theATTRINFOptr = &tcKeyReq->attrInfo[0];
  theReceiver.init(NdbReceiver::NDB_OPERATION, this);
  return 0;
}


/******************************************************************************
 * void release();
 *
 * Remark:        Release all objects connected to the operation object.
 *****************************************************************************/
void
NdbOperation::release()
{
  NdbApiSignal* tSignal;
  NdbApiSignal* tSaveSignal;
  NdbBranch*	tBranch;
  NdbBranch*	tSaveBranch;
  NdbLabel*	tLabel;
  NdbLabel*	tSaveLabel;
  NdbCall*	tCall;
  NdbCall*	tSaveCall;
  NdbSubroutine* tSubroutine;
  NdbSubroutine* tSaveSubroutine;
  NdbBlob* tBlob;
  NdbBlob* tSaveBlob;

  tSignal = theTCREQ;
  while (tSignal != NULL)
  {
    tSaveSignal = tSignal;
    tSignal = tSignal->next();
    theNdb->releaseSignal(tSaveSignal);
  }				
  theTCREQ = NULL;
  theLastKEYINFO = NULL;

  tSignal = theFirstATTRINFO;
  while (tSignal != NULL)
  {
    tSaveSignal = tSignal;
    tSignal = tSignal->next();
    theNdb->releaseSignal(tSaveSignal);
  }
  theFirstATTRINFO = NULL;
  theCurrentATTRINFO = NULL;

  if (theInterpretIndicator == 1)
  {
    tBranch = theFirstBranch;
    while (tBranch != NULL)
    {
      tSaveBranch = tBranch;
      tBranch = tBranch->theNext;
      theNdb->releaseNdbBranch(tSaveBranch);
    }
    tLabel = theFirstLabel;
    while (tLabel != NULL)
    {
      tSaveLabel = tLabel;
      tLabel = tLabel->theNext;
      theNdb->releaseNdbLabel(tSaveLabel);
    }
    tCall = theFirstCall;
    while (tCall != NULL)
    {
      tSaveCall = tCall;
      tCall = tCall->theNext;
      theNdb->releaseNdbCall(tSaveCall);
    }
    tSubroutine = theFirstSubroutine;
    while (tSubroutine != NULL)
    {
      tSaveSubroutine = tSubroutine;
      tSubroutine = tSubroutine->theNext;
      theNdb->releaseNdbSubroutine(tSaveSubroutine);
    }
  }
  tBlob = theBlobList;
  while (tBlob != NULL)
  {
    tSaveBlob = tBlob;
    tBlob = tBlob->theNext;
    theNdb->releaseNdbBlob(tSaveBlob);
  }
  theBlobList = NULL;
  theReceiver.release();
}

NdbRecAttr*
NdbOperation::getValue(const char* anAttrName, char* aValue)
{
  return getValue_impl(m_currentTable->getColumn(anAttrName), aValue);
}

NdbRecAttr*
NdbOperation::getValue(Uint32 anAttrId, char* aValue)
{
  return getValue_impl(m_currentTable->getColumn(anAttrId), aValue);
}

NdbRecAttr*
NdbOperation::getValue(const NdbDictionary::Column* col, char* aValue)
{
  return getValue_impl(&NdbColumnImpl::getImpl(*col), aValue);
}

int
NdbOperation::equal(const char* anAttrName, 
		    const char* aValuePassed, 
		    Uint32 aVariableKeyLen)
{
  return equal_impl(m_accessTable->getColumn(anAttrName), aValuePassed, aVariableKeyLen);
}

int
NdbOperation::equal(Uint32 anAttrId, 
		    const char* aValuePassed, 
		    Uint32 aVariableKeyLen)
{
  return equal_impl(m_accessTable->getColumn(anAttrId), aValuePassed, aVariableKeyLen);
}

int
NdbOperation::setValue( const char* anAttrName, 
			const char* aValuePassed, 
			Uint32 len)
{
  return setValue(m_currentTable->getColumn(anAttrName), aValuePassed, len);
}


int
NdbOperation::setValue( Uint32 anAttrId, 
			const char* aValuePassed, 
			Uint32 len)
{
  return setValue(m_currentTable->getColumn(anAttrId), aValuePassed, len);
}

NdbBlob*
NdbOperation::getBlobHandle(const char* anAttrName)
{
  return getBlobHandle(theNdbCon, m_currentTable->getColumn(anAttrName));
}

NdbBlob*
NdbOperation::getBlobHandle(Uint32 anAttrId)
{
  return getBlobHandle(theNdbCon, m_currentTable->getColumn(anAttrId));
}

int
NdbOperation::incValue(const char* anAttrName, Uint32 aValue)
{
  return incValue(m_currentTable->getColumn(anAttrName), aValue);
}

int
NdbOperation::incValue(const char* anAttrName, Uint64 aValue)
{
  return incValue(m_currentTable->getColumn(anAttrName), aValue);
}

int
NdbOperation::incValue(Uint32 anAttrId, Uint32 aValue)
{
  return incValue(m_currentTable->getColumn(anAttrId), aValue);
}

int
NdbOperation::incValue(Uint32 anAttrId, Uint64 aValue)
{
  return incValue(m_currentTable->getColumn(anAttrId), aValue);
}

int
NdbOperation::subValue( const char* anAttrName, Uint32 aValue)
{
  return subValue(m_currentTable->getColumn(anAttrName), aValue);
}

int
NdbOperation::subValue(Uint32 anAttrId, Uint32 aValue)
{
  return subValue(m_currentTable->getColumn(anAttrId), aValue);
}

int
NdbOperation::read_attr(const char* anAttrName, Uint32 RegDest)
{
  return read_attr(m_currentTable->getColumn(anAttrName), RegDest);
}

int
NdbOperation::read_attr(Uint32 anAttrId, Uint32 RegDest)
{
  return read_attr(m_currentTable->getColumn(anAttrId), RegDest);
}

int
NdbOperation::write_attr(const char* anAttrName, Uint32 RegDest)
{
  return write_attr(m_currentTable->getColumn(anAttrName), RegDest);
}

int
NdbOperation::write_attr(Uint32 anAttrId, Uint32 RegDest)
{
  return write_attr(m_currentTable->getColumn(anAttrId), RegDest);
}

const char*
NdbOperation::getTableName() const
{
  return m_currentTable->m_externalName.c_str();
}
