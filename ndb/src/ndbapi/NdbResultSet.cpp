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

/*****************************************************************************
 * Name:          NdbResultSet.cpp
 * Include:
 * Link:
 * Author:        UABMASD Martin Sköld INN/V Alzato
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Cursor class
 * Documentation:
 * Adjust:  2002-04-01  UABMASD   First version.
 ****************************************************************************/

#include <Ndb.hpp>
#include <NdbConnection.hpp>
#include <NdbResultSet.hpp>

NdbResultSet::NdbResultSet(NdbCursorOperation *owner)
: m_operation(owner)
{
}
 
NdbResultSet::~NdbResultSet()
{
}

void  NdbResultSet::init()
{
}

int NdbResultSet::nextResult(bool fetchAllowed)
{
  return m_operation->nextResult(fetchAllowed);
}

void NdbResultSet::close()
{
  m_operation->closeScan();
}

NdbOperation* 
NdbResultSet::updateTuple(){
  if(m_operation->cursorType() != NdbCursorOperation::ScanCursor){
    m_operation->setErrorCode(4003);
    return 0;
  }
  
  NdbScanOperation * op = (NdbScanOperation*)(m_operation);
  return op->takeOverScanOp(NdbOperation::UpdateRequest, 
			    op->m_transConnection);
}

NdbOperation* 
NdbResultSet::updateTuple(NdbConnection* takeOverTrans){
  if(m_operation->cursorType() != NdbCursorOperation::ScanCursor){
    m_operation->setErrorCode(4003);
    return 0;
  }
  
  return m_operation->takeOverScanOp(NdbOperation::UpdateRequest, 
				     takeOverTrans);
}

int
NdbResultSet::deleteTuple(){
  if(m_operation->cursorType() != NdbCursorOperation::ScanCursor){
    m_operation->setErrorCode(4003);
    return 0;
  }
  
  NdbScanOperation * op = (NdbScanOperation*)(m_operation);
  void * res = op->takeOverScanOp(NdbOperation::DeleteRequest, 
				  op->m_transConnection);
  if(res == 0)
    return -1;
  return 0;
}

int
NdbResultSet::deleteTuple(NdbConnection * takeOverTrans){
  if(m_operation->cursorType() != NdbCursorOperation::ScanCursor){
    m_operation->setErrorCode(4003);
    return 0;
  }
  
  void * res = m_operation->takeOverScanOp(NdbOperation::DeleteRequest, 
					   takeOverTrans);
  if(res == 0)
    return -1;
  return 0;
}
