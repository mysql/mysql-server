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


#include "API.hpp"
#include "NdbApiSignal.hpp"

/**
 * The following include includes
 *  definitions directly from the kernel
 *
 * Definitions that is shared between kernel and the API
 */
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/IndxKeyInfo.hpp>
#include <signaldata/IndxAttrInfo.hpp>
#include <signaldata/TcHbRep.hpp>
#include <signaldata/ScanTab.hpp>

#include <NdbOut.hpp>

/******************************************************************************
NdbApiSignal();

Return Value:   None  
Remark:         Creates a NdbApiSignal object. 
******************************************************************************/
NdbApiSignal::NdbApiSignal(BlockReference ref)
{
  theVerId_signalNumber   = 0;    // 4 bit ver id - 16 bit gsn
  theReceiversBlockNumber = 0;  // Only 16 bit blocknum  
  theSendersBlockRef      = refToBlock(ref);
  theLength               = 0;
  theSendersSignalId      = 0;
  theSignalId             = 0;
  theTrace                = 0;
  m_noOfSections          = 0;
  m_fragmentInfo          = 0;
  for (int i = 0; i < 25; i++)
    theData[i] = 0x13579753;
  
  setDataPtr(&theData[0]);
  theNextSignal = 0;
}

NdbApiSignal::NdbApiSignal(Ndb* ndb)
{
  BlockReference ref = ndb->theMyRef;
  theVerId_signalNumber   = 0;    // 4 bit ver id - 16 bit gsn
  theReceiversBlockNumber = 0;  // Only 16 bit blocknum  
  theSendersBlockRef      = refToBlock(ref);
  theLength               = 0;
  theSendersSignalId      = 0;
  theSignalId             = 0;
  theTrace                = 0;
  m_noOfSections          = 0;
  m_fragmentInfo          = 0;
  for (int i = 0; i < 25; i++)
    theData[i] = 0x13579753;
  
  setDataPtr(&theData[0]);
  theNextSignal = 0;
}

/**
 * Copy constructor
 */
NdbApiSignal::NdbApiSignal(const NdbApiSignal &src) {
  copyFrom(&src);
}
/******************************************************************************
~NdbApiSignal();

Return Value:   None  
Remark:         Delete a NdbApiSignal object.
******************************************************************************/
NdbApiSignal::~NdbApiSignal()
{ 
}
/******************************************************************************
int setSignal(NdbSignalType aNdbSignalType);

Return Value:   Return 0 : setSignal was successful.
                Return tErrorCode In all other case. 
Parameters:     aNdbSignalType: Type of signal.
Remark:         Set signal header and allocate 128 byte. 
******************************************************************************/
int 
NdbApiSignal::setSignal(int aNdbSignalType)
{ 	
  theSendersSignalId = 0;
  switch (aNdbSignalType)
  {
    case GSN_DIHNDBTAMPER:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDIH;
      theVerId_signalNumber   = GSN_DIHNDBTAMPER;
      theLength               = 3;
    }
    break;

    case GSN_TCSEIZEREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TCSEIZEREQ;
      theLength               = 2;
    }
    break;

    case GSN_TCKEYREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TCKEYREQ;
      theLength               = TcKeyReq::SignalLength;
    }
    break;

    case GSN_TCRELEASEREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TCRELEASEREQ;
      theLength               = 3;
    }
    break;

    case GSN_ATTRINFO:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_ATTRINFO;
      theLength               = AttrInfo::MaxSignalLength;
    }
    break;
    
    case GSN_KEYINFO:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber	= DBTC;
      theVerId_signalNumber = GSN_KEYINFO;
      theLength               = KeyInfo::MaxSignalLength; 
    }
    break;

    case GSN_TCROLLBACKREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TCROLLBACKREQ;
      theLength               = 3;
    }
    break;

    case GSN_TC_HBREP:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TC_HBREP;
      theLength               = TcHbRep::SignalLength;
    }
    break;

    case GSN_TC_COMMITREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TC_COMMITREQ;
      theLength               = 3;
    }
    break;

    case GSN_SCAN_TABREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_SCAN_TABREQ;
      theLength               = ScanTabReq::StaticLength;      
    }
    break;

    case GSN_SCAN_NEXTREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_SCAN_NEXTREQ;
      theLength               = ScanNextReq::SignalLength;
    }
    break;

    case GSN_CREATE_INDX_REQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDICT;
      theVerId_signalNumber   = GSN_CREATE_INDX_REQ;
      theLength               = CreateIndxReq::SignalLength;   
    }
    break;

    case GSN_DROP_INDX_REQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBDICT;
      theVerId_signalNumber   = GSN_DROP_INDX_REQ;
      theLength               = DropIndxReq::SignalLength;   
    }
    break;

    case GSN_TCINDXREQ:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_TCINDXREQ;
      theLength               = TcIndxReq::SignalLength;
    }
    break;

    case GSN_INDXKEYINFO:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_INDXKEYINFO;
      theLength               = IndxKeyInfo::MaxSignalLength; 
    }
    break;

    case GSN_INDXATTRINFO:
    {
      theTrace                = TestOrd::TraceAPI;
      theReceiversBlockNumber = DBTC;
      theVerId_signalNumber   = GSN_INDXATTRINFO;
      theLength               = IndxAttrInfo::MaxSignalLength;
    }
    break;
    
    default: 
    {
      return -1;
    }
  }
  return 0;
}

void
NdbApiSignal::set(Uint8  trace,
		  Uint16 receiversBlockNumber,
		  Uint16 signalNumber,
		  Uint32 length){
  
  theTrace                = trace;
  theReceiversBlockNumber = receiversBlockNumber;
  theVerId_signalNumber   = signalNumber;
  theLength               = length;
}

void
NdbApiSignal::copyFrom(const NdbApiSignal * src){
  theVerId_signalNumber   = src->theVerId_signalNumber;
  theReceiversBlockNumber = src->theReceiversBlockNumber;
  theSendersBlockRef      = src->theSendersBlockRef;
  theLength               = src->theLength;
  theTrace                = src->theTrace;
  
  Uint32 * dstData = getDataPtrSend();
  const Uint32 * srcData = src->getDataPtr();
  for (Uint32 i = 0; i < theLength; i++)
    dstData[i] = srcData[i];

  setDataPtr(dstData);
  
  /**
   * NOTE that theSignalId is used as data ptr
   *   and should not be copied
   * NOTE that theSendersSignalId is used as next pointer
   *   and should not be copied
   */
}
