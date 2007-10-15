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

#include <ndb_global.h>
#include <NdbIndexOperation.hpp>
#include <Ndb.hpp>
#include <NdbTransaction.hpp>
#include "NdbApiSignal.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/IndxKeyInfo.hpp>
#include <signaldata/IndxAttrInfo.hpp>

NdbIndexOperation::NdbIndexOperation(Ndb* aNdb) :
  NdbOperation(aNdb, NdbOperation::UniqueIndexAccess),
  m_theIndex(NULL)
{
  m_tcReqGSN = GSN_TCINDXREQ;
  m_attrInfoGSN = GSN_INDXATTRINFO;
  m_keyInfoGSN = GSN_INDXKEYINFO;

  /**
   * Change receiver type
   */
  theReceiver.init(NdbReceiver::NDB_INDEX_OPERATION, this);
}

NdbIndexOperation::~NdbIndexOperation()
{
}

/*****************************************************************************
 * int indxInit();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/
int
NdbIndexOperation::indxInit(const NdbIndexImpl * anIndex,
			    const NdbTableImpl * aTable, 
			    NdbTransaction* myConnection)
{
  NdbOperation::init(aTable, myConnection);

  switch (anIndex->m_type) {
  case(NdbDictionary::Index::UniqueHashIndex):
    break;
  case(NdbDictionary::Index::Undefined):
  case(NdbDictionary::Index::OrderedIndex):
    setErrorCodeAbort(4003);
    return -1;
  default:
    DBUG_ASSERT(0);
    break;
  }
  m_theIndex = anIndex;
  m_accessTable = anIndex->m_table;
  theNoOfTupKeyLeft = m_accessTable->getNoOfPrimaryKeys();
  return 0;
}

int NdbIndexOperation::readTuple(NdbOperation::LockMode lm)
{ 
  switch(lm) {
  case LM_Read:
    return readTuple();
    break;
  case LM_Exclusive:
    return readTupleExclusive();
    break;
  case LM_CommittedRead:
    return readTuple();
    break;
  case LM_SimpleRead:
    return readTuple();
    break;
  default:
    return -1;
  };
}

int NdbIndexOperation::insertTuple()
{
  setErrorCode(4200);
  return -1;
}

int NdbIndexOperation::readTuple()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::readTupleExclusive()
{
  // First check that index is unique

  return NdbOperation::readTupleExclusive();
}

int NdbIndexOperation::simpleRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::dirtyRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::committedRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::updateTuple()
{
  // First check that index is unique

  return NdbOperation::updateTuple();
}

int NdbIndexOperation::deleteTuple()
{
  // First check that index is unique

  return NdbOperation::deleteTuple();
}

int NdbIndexOperation::dirtyUpdate()
{
  // First check that index is unique

  return NdbOperation::dirtyUpdate();
}

int NdbIndexOperation::interpretedUpdateTuple()
{
  // First check that index is unique

  return NdbOperation::interpretedUpdateTuple();
}

int NdbIndexOperation::interpretedDeleteTuple()
{
  // First check that index is unique

  return NdbOperation::interpretedDeleteTuple();
}

const NdbDictionary::Index*
NdbIndexOperation::getIndex() const
{
  return m_theIndex;
}

/***************************************************************************
int receiveTCINDXREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the TCINDXREF signal from TC.
Remark:         Handles the reception of the TCKEYREF signal.
***************************************************************************/
int
NdbIndexOperation::receiveTCINDXREF( NdbApiSignal* aSignal)
{
  return receiveTCKEYREF(aSignal);
}//NdbIndexOperation::receiveTCINDXREF()
