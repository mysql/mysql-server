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
#include <NdbBlob.hpp>

NdbResultSet::NdbResultSet(NdbScanOperation *owner)
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
  int res;
  if ((res = m_operation->nextResult(fetchAllowed)) == 0) {
    // handle blobs
    NdbBlob* tBlob = m_operation->theBlobList;
    while (tBlob != 0) {
      if (tBlob->atNextResult() == -1)
        return -1;
      tBlob = tBlob->theNext;
    }
    /*
     * Flush blob part ops on behalf of user because
     * - nextResult is analogous to execute(NoCommit)
     * - user is likely to want blob value before next execute
     */
    if (m_operation->m_transConnection->executePendingBlobOps() == -1)
      return -1;
    return 0;
  }
  return res;
}

void NdbResultSet::close()
{
  m_operation->closeScan();
}

NdbOperation* 
NdbResultSet::updateTuple(){
  return updateTuple(m_operation->m_transConnection);
}

NdbOperation* 
NdbResultSet::updateTuple(NdbConnection* takeOverTrans){
  return m_operation->takeOverScanOp(NdbOperation::UpdateRequest, 
				     takeOverTrans);
}

int
NdbResultSet::deleteTuple(){
  return deleteTuple(m_operation->m_transConnection);
}

int
NdbResultSet::deleteTuple(NdbConnection * takeOverTrans){
  void * res = m_operation->takeOverScanOp(NdbOperation::DeleteRequest, 
					   takeOverTrans);
  if(res == 0)
    return -1;
  return 0;
}

int
NdbResultSet::restart(){
  return m_operation->restart();
}
