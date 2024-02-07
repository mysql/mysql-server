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

#include "NdbApiSignal.hpp"
#include "API.hpp"

/**
 * The following include includes
 *  definitions directly from the kernel
 *
 * Definitions that is shared between kernel and the API
 */
#include <signaldata/AttrInfo.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/IndxAttrInfo.hpp>
#include <signaldata/IndxKeyInfo.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/TcHbRep.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/TestOrd.hpp>

#include <NdbOut.hpp>

/******************************************************************************
NdbApiSignal();

Return Value:   None
Remark:         Creates a NdbApiSignal object.
******************************************************************************/
NdbApiSignal::NdbApiSignal(BlockReference ref) {
  theVerId_signalNumber = 0;    // 4 bit ver id - 16 bit gsn
  theReceiversBlockNumber = 0;  // Only 16 bit blocknum
  theSendersBlockRef = refToBlock(ref);
  theLength = 0;
  theSendersSignalId = 0;
  theSignalId = 0;
  theTrace = 0;
  m_noOfSections = 0;
  m_fragmentInfo = 0;
  for (int i = 0; i < 25; i++) theData[i] = 0x13579753;

  setDataPtr(&theData[0]);
  theNextSignal = nullptr;
}

NdbApiSignal::NdbApiSignal(Ndb *ndb) {
  theVerId_signalNumber = 0;    // 4 bit ver id - 16 bit gsn
  theReceiversBlockNumber = 0;  // Only 16 bit blocknum
  theSendersBlockRef = ndb->theNdbBlockNumber;
  theLength = 0;
  theSendersSignalId = 0;
  theSignalId = 0;
  theTrace = 0;
  m_noOfSections = 0;
  m_fragmentInfo = 0;
  for (int i = 0; i < 25; i++) theData[i] = 0x13579753;

  assert(theSendersBlockRef != 0);

  setDataPtr(&theData[0]);
  theNextSignal = nullptr;
}

/**
 * Copy constructor
 */
NdbApiSignal::NdbApiSignal(const NdbApiSignal &src) { copyFrom(&src); }
/******************************************************************************
~NdbApiSignal();

Return Value:   None
Remark:         Delete a NdbApiSignal object.
******************************************************************************/
NdbApiSignal::~NdbApiSignal() {}
/******************************************************************************
int setSignal(NdbSignalType aNdbSignalType);

Return Value:   Return 0 : setSignal was successful.
                Return tErrorCode In all other case.
Parameters:     aNdbSignalType: Type of signal.
Remark:         Set signal header and allocate 128 byte.
******************************************************************************/
int NdbApiSignal::setSignal(int aNdbSignalType, Uint32 receiversBlockNo) {
  theSendersSignalId = 0;
  switch (aNdbSignalType) {
    case GSN_DIHNDBTAMPER: {
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDIH;
      theVerId_signalNumber = GSN_DIHNDBTAMPER;
      theLength = 3;
    } break;

    case GSN_TCSEIZEREQ: {
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber = GSN_TCSEIZEREQ;
      theLength = 3;
    } break;

    case GSN_TCKEYREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TCKEYREQ;
      theLength = TcKeyReq::SignalLength;
    } break;

    case GSN_TCRELEASEREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TCRELEASEREQ;
      theLength = 3;
    } break;

    case GSN_ATTRINFO: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_ATTRINFO;
      theLength = AttrInfo::MaxSignalLength;
    } break;

    case GSN_KEYINFO: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_KEYINFO;
      theLength = KeyInfo::MaxSignalLength;
    } break;

    case GSN_TCROLLBACKREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TCROLLBACKREQ;
      theLength = 3;
    } break;

    case GSN_TC_HBREP: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TC_HBREP;
      theLength = TcHbRep::SignalLength;
    } break;

    case GSN_TC_COMMITREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TC_COMMITREQ;
      theLength = 3;
    } break;

    case GSN_SCAN_TABREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_SCAN_TABREQ;
      theLength = ScanTabReq::StaticLength;
    } break;

    case GSN_SCAN_NEXTREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_SCAN_NEXTREQ;
      theLength = ScanNextReq::SignalLength;
    } break;

    case GSN_CREATE_INDX_REQ: {
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDICT;
      theVerId_signalNumber = GSN_CREATE_INDX_REQ;
      theLength = CreateIndxReq::SignalLength;
    } break;

    case GSN_DROP_INDX_REQ: {
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDICT;
      theVerId_signalNumber = GSN_DROP_INDX_REQ;
      theLength = DropIndxReq::SignalLength;
    } break;

    case GSN_TCINDXREQ: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_TCINDXREQ;
      theLength = TcKeyReq::SignalLength;
    } break;

    case GSN_INDXKEYINFO: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_INDXKEYINFO;
      theLength = IndxKeyInfo::MaxSignalLength;
    } break;

    case GSN_INDXATTRINFO: {
      assert(blockToMain(receiversBlockNo) == DBTC);
      theTrace = TestOrd::TraceAPI;
      theReceiversBlockNumber = receiversBlockNo;
      theVerId_signalNumber = GSN_INDXATTRINFO;
      theLength = IndxAttrInfo::MaxSignalLength;
    } break;

    default: {
      return -1;
    }
  }
  return 0;
}

void NdbApiSignal::set(Uint8 trace, Uint16 receiversBlockNumber,
                       Uint16 signalNumber, Uint32 length) {
  theTrace = trace;
  theReceiversBlockNumber = receiversBlockNumber;
  theVerId_signalNumber = signalNumber;
  theLength = length;
}

void NdbApiSignal::copyFrom(const NdbApiSignal *src) {
  theSignalId = src->theSignalId;
  theVerId_signalNumber = src->theVerId_signalNumber;
  theReceiversBlockNumber = src->theReceiversBlockNumber;
  theSendersBlockRef = src->theSendersBlockRef;
  theLength = src->theLength;
  theTrace = src->theTrace;
  m_noOfSections = src->m_noOfSections;
  m_fragmentInfo = src->m_fragmentInfo;

  Uint32 *dstData = getDataPtrSend();
  const Uint32 *srcData = src->getDataPtr();
  for (Uint32 i = 0; i < theLength; i++) dstData[i] = srcData[i];

  setDataPtr(dstData);
}
