/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>

#include "API.hpp"

#include <signaldata/ScanTab.hpp>

#include <NdbOut.hpp>
#include <NdbQueryOperationImpl.hpp>

/***************************************************************************
 * int  receiveSCAN_TABREF(NdbApiSignal* aSignal)
 *
 *  This means the scan could not be started, set status(s) to indicate 
 *  the failure
 *
 ****************************************************************************/
int			
NdbTransaction::receiveSCAN_TABREF(const NdbApiSignal* aSignal){
  const ScanTabRef * ref = CAST_CONSTPTR(ScanTabRef, aSignal->getDataPtr());
  
  if (checkState_TransId(&ref->transId1)) {
    if (theScanningOp) {
      theScanningOp->execCLOSE_SCAN_REP();
      theScanningOp->setErrorCode(ref->errorCode);
      if(!ref->closeNeeded){
        return 0;
      }

      /**
       * Setup so that close_impl will actually perform a close
       *   and not "close scan"-optimze it away
       */
      theScanningOp->m_conf_receivers_count++;
      theScanningOp->m_conf_receivers[0] = theScanningOp->m_receivers[0];
      theScanningOp->m_conf_receivers[0]->m_tcPtrI = ~0;

    } else {
      assert (m_scanningQuery);
      m_scanningQuery->execCLOSE_SCAN_REP(ref->errorCode, ref->closeNeeded);
      if(!ref->closeNeeded){
        return 0;
      }
    }
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}

/*****************************************************************************
 * int  receiveSCAN_TABCONF(NdbApiSignal* aSignal)
 *
 * Receive SCAN_TABCONF
 * If scanStatus == 0 there is more records to read. Since signals may be 
 * received in any order we have to go through the lists with saved signals 
 * and check if all expected signals are there so that we can start to 
 * execute them.
 *
 * If scanStatus > 0 this indicates that the scan is finished and there are 
 * no more data to be read.
 * 
 *****************************************************************************/
int			
NdbTransaction::receiveSCAN_TABCONF(const NdbApiSignal* aSignal,
				   const Uint32 * ops, Uint32 len)
{
  const ScanTabConf * conf = CAST_CONSTPTR(ScanTabConf, aSignal->getDataPtr());

  if (checkState_TransId(&conf->transId1)) {
    
    /**
     * If EndOfData is set, close the scan.
     */
    if (conf->requestInfo == ScanTabConf::EndOfData) {
      if (theScanningOp) {
        theScanningOp->execCLOSE_SCAN_REP();
      } else {
        assert (m_scanningQuery);
        m_scanningQuery->execCLOSE_SCAN_REP(0, false);
      }
      return 1; // -> Finished
    }

    int retVal = -1;
    Uint32 words_per_op = theScanningOp ? 3 : 4;
    for(Uint32 i = 0; i<len; i += words_per_op)
    {
      Uint32 ptrI = * ops++;
      Uint32 tcPtrI = * ops++;
      Uint32 opCount;
      Uint32 totalLen;
      if (words_per_op == 3)
      {
        Uint32 info = * ops++;
        opCount  = ScanTabConf::getRows(info);
        totalLen = ScanTabConf::getLength(info);
      }
      else
      {
        opCount = * ops++;
        totalLen = * ops++;
      }

      void * tPtr = theNdb->int2void(ptrI);
      assert(tPtr); // For now
      NdbReceiver* tOp = theNdb->void2rec(tPtr);
      if (tOp && tOp->checkMagicNumber())
      {
        // Check if this is a linked operation.
        if (tOp->getType()==NdbReceiver::NDB_QUERY_OPERATION)
        {
          NdbQueryOperationImpl* queryOp = (NdbQueryOperationImpl*)tOp->m_owner;
          assert (&queryOp->getQuery() == m_scanningQuery);

          if (queryOp->execSCAN_TABCONF(tcPtrI, opCount, totalLen, tOp))
            retVal = 0; // We have result data, wakeup receiver
        }
        else
        {
          if (tcPtrI == RNIL && opCount == 0)
          {
            theScanningOp->receiver_completed(tOp);
            retVal = 0;
          }
          else if (tOp->execSCANOPCONF(tcPtrI, totalLen, opCount))
          {
            theScanningOp->receiver_delivered(tOp);
            retVal = 0;
          }
        }
      }
    } //for
    return retVal;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }
  
  return -1;
}
