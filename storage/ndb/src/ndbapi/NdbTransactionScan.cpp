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

#include <ndb_global.h>

#include "API.hpp"

#include <signaldata/ScanTab.hpp>

#include <NdbOut.hpp>
#include <NdbQueryOperationImpl.hpp>

/***************************************************************************
 * int  receiveSCAN_TABREF(NdbApiSignal* aSignal)
 *
 *  TC indicates that the scan overall will fail.
 *  TC also indicates via closeNeeded whether :
 *    0 : It has already cleaned up the kernel-side scan state
 *    1 : It requires the API to send a SCAN_NEXTREQ(close) to clean up
 *        the kernel side state.
 *
 *  SCAN_TABREF and SCAN_TABCONF can arrive at any time, and can arrive
 *  while the referenced NdbScanOperation object is being operated upon
 *  by user code.
 *
 *  Some care is therefore needed to avoid races between setting and
 *  reading of common variables between signal reception code and user
 *  API side execution.
 *
 ****************************************************************************/
int NdbTransaction::receiveSCAN_TABREF(const NdbApiSignal *aSignal) {
  const ScanTabRef *ref = CAST_CONSTPTR(ScanTabRef, aSignal->getDataPtr());

  if (checkState_TransId(&ref->transId1)) {
    if (theScanningOp) {
      theScanningOp->execCLOSE_SCAN_REP(ref->errorCode, ref->closeNeeded);
    } else {
      assert(m_scanningQuery);
      m_scanningQuery->execCLOSE_SCAN_REP(ref->errorCode, ref->closeNeeded);
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
int NdbTransaction::receiveSCAN_TABCONF(const NdbApiSignal *aSignal,
                                        const Uint32 *ops, Uint32 len) {
  const ScanTabConf *conf = CAST_CONSTPTR(ScanTabConf, aSignal->getDataPtr());

  if (checkState_TransId(&conf->transId1)) {
    /**
     * If EndOfData is set, close the scan.
     */
    if (conf->requestInfo == ScanTabConf::EndOfData) {
      if (theScanningOp) {
        theScanningOp->execCLOSE_SCAN_REP(0, false);
      } else {
        assert(m_scanningQuery);
        m_scanningQuery->execCLOSE_SCAN_REP(0, false);
      }
      return 1;  // -> Finished
    }

    int retVal = -1;
    const Uint32 *const opsEnd = ops + len;
    while (ops < opsEnd) {
      const Uint32 ptrI = *ops++;
      const Uint32 tcPtrI = *ops++;
      void *tPtr = theNdb->theImpl->int2void(ptrI);
      assert(tPtr);  // For now
      NdbReceiver *tOp = NdbImpl::void2rec(tPtr);
      if (likely(tOp && tOp->checkMagicNumber())) {
        // Check if this is a linked operation.
        if (tOp->getType() == NdbReceiver::NDB_QUERY_OPERATION)  // A SPJ reply
        {
          const Uint32 rowCount = *ops++;
          const Uint32 moreMask = *ops++;

          // A 5'th 'activeMask' word was added as part of wl#7636 (SPJ outer
          // join). Version of connected TC node decide whether a 4/5 word conf
          // is returned.
          const Uint32 tcNodeId = getConnectedNodeId();
          const Uint32 nodeVersion =
              theNdb->theImpl->getNodeNdbVersion(tcNodeId);
          assert(nodeVersion != 0);
          const Uint32 activeMask =
              ndbd_send_active_bitmask(nodeVersion) ? *ops++ : 0;

          NdbQueryOperationImpl *queryOp =
              (NdbQueryOperationImpl *)tOp->m_owner;
          assert(&queryOp->getQuery() == m_scanningQuery);
          if (queryOp->execSCAN_TABCONF(tcPtrI, rowCount, moreMask, activeMask,
                                        tOp))
            retVal = 0;  // We have result data, wakeup receiver
        } else {
          const Uint32 info = *ops++;
          const Uint32 opCount = ScanTabConf::getRows(info);
          const Uint32 totalLen = ScanTabConf::getLength(info);
          if (tcPtrI == RNIL && opCount == 0) {
            theScanningOp->receiver_completed(tOp);
            retVal = 0;
          } else if (tOp->execSCANOPCONF(tcPtrI, totalLen, opCount)) {
            theScanningOp->receiver_delivered(tOp);
            retVal = 0;
          }
        }
      }
    }  // while
    return retVal;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}
