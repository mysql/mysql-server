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
 * Name:          NdbConnectionScan.cpp
 * Include:
 * Link:
 * Author:        UABRONM MikaelRonström UAB/M/MT       
 *                QABJKAM Jonas Kamf UAB/M/MT                  
 * Date:          2000-06-12
 * Version:       0.1
 * Description:   Interface between Application and NDB
 * Documentation:
 * Adjust:  2000-06-12  UABRONM   First version.
 ****************************************************************************/
#include <ndb_global.h>

#include <Ndb.hpp>
#include <NdbConnection.hpp>
#include <NdbOperation.hpp>
#include <NdbScanOperation.hpp>
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include "NdbUtil.hpp"
#include "API.hpp"
#include "NdbImpl.hpp"

#include <signaldata/ScanTab.hpp>

#include <NdbOut.hpp>


/***************************************************************************
 * int  receiveSCAN_TABREF(NdbApiSignal* aSignal)
 *
 *  This means the scan could not be started, set status(s) to indicate 
 *  the failure
 *
 ****************************************************************************/
int			
NdbConnection::receiveSCAN_TABREF(NdbApiSignal* aSignal){
  const ScanTabRef * ref = CAST_CONSTPTR(ScanTabRef, aSignal->getDataPtr());
  
  if(checkState_TransId(&ref->transId1)){
    theScanningOp->theError.code = ref->errorCode;
    theScanningOp->execCLOSE_SCAN_REP();
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
NdbConnection::receiveSCAN_TABCONF(NdbApiSignal* aSignal, 
				   const Uint32 * ops, Uint32 len)
{
  const ScanTabConf * conf = CAST_CONSTPTR(ScanTabConf, aSignal->getDataPtr());
  if(checkState_TransId(&conf->transId1)){
    
    if (conf->requestInfo == ScanTabConf::EndOfData) {
      theScanningOp->execCLOSE_SCAN_REP();
      return 0;
    }

    for(Uint32 i = 0; i<len; i += 3){
      Uint32 opCount, totalLen;
      Uint32 ptrI = * ops++;
      Uint32 tcPtrI = * ops++;
      Uint32 info = * ops++;
      opCount  = ScanTabConf::getRows(info);
      totalLen = ScanTabConf::getLength(info);
      
      void * tPtr = theNdb->int2void(ptrI);
      assert(tPtr); // For now
      NdbReceiver* tOp = theNdb->void2rec(tPtr);
      if (tOp && tOp->checkMagicNumber())
      {
	if (tcPtrI == RNIL && opCount == 0)
	  theScanningOp->receiver_completed(tOp);
	else if (tOp->execSCANOPCONF(tcPtrI, totalLen, opCount))
	  theScanningOp->receiver_delivered(tOp);
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
