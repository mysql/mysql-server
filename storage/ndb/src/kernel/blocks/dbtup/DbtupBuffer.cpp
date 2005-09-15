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

#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TransIdAI.hpp>

#define ljam() { jamLine(2000 + __LINE__); }
#define ljamEntry() { jamEntryLine(2000 + __LINE__); }

void Dbtup::execSEND_PACKED(Signal* signal)
{
  Uint16 hostId;
  Uint32 i;
  Uint32 TpackedListIndex = cpackedListIndex;
  ljamEntry();
  for (i = 0; i < TpackedListIndex; i++) {
    ljam();
    hostId = cpackedList[i];
    ndbrequire((hostId - 1) < (MAX_NODES - 1)); // Also check not zero
    Uint32 TpacketTA = hostBuffer[hostId].noOfPacketsTA;
    if (TpacketTA != 0) {
      ljam();
      BlockReference TBref = numberToRef(API_PACKED, hostId);
      Uint32 TpacketLen = hostBuffer[hostId].packetLenTA;
      MEMCOPY_NO_WORDS(&signal->theData[0],
                       &hostBuffer[hostId].packetBufferTA[0],
                       TpacketLen);
      sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
      hostBuffer[hostId].noOfPacketsTA = 0;
      hostBuffer[hostId].packetLenTA = 0;
    }//if
    hostBuffer[hostId].inPackedList = false;
  }//for
  cpackedListIndex = 0;
}//Dbtup::execSEND_PACKED()

void Dbtup::bufferTRANSID_AI(Signal* signal, BlockReference aRef,
                             Uint32 Tlen)
{
  if(Tlen == 3)
    return;
  
  Uint32 hostId = refToNode(aRef);
  Uint32 Theader = ((refToBlock(aRef) << 16)+(Tlen-3));
  
  ndbrequire(hostId < MAX_NODES);
  Uint32 TpacketLen = hostBuffer[hostId].packetLenTA;
  Uint32 TnoOfPackets = hostBuffer[hostId].noOfPacketsTA;
  Uint32 sig0 = signal->theData[0];
  Uint32 sig1 = signal->theData[1];
  Uint32 sig2 = signal->theData[2];

  BlockReference TBref = numberToRef(API_PACKED, hostId);

  if ((Tlen + TpacketLen + 1) <= 25) {
// ----------------------------------------------------------------
// There is still space in the buffer. We will copy it into the
// buffer.
// ----------------------------------------------------------------
    ljam();
    updatePackedList(signal, hostId);
  } else if (false && TnoOfPackets == 1) {
// ----------------------------------------------------------------
// The buffer is full and there was only one packet buffered. We
// will send this as a normal signal.
// ----------------------------------------------------------------
    Uint32 TnewRef = numberToRef((hostBuffer[hostId].packetBufferTA[0] >> 16),
                                 hostId);
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferTA[1],
                     TpacketLen - 1);
    sendSignal(TnewRef, GSN_TRANSID_AI, signal, (TpacketLen - 1), JBB);
    TpacketLen = 0;
    TnoOfPackets = 0;
  } else {
// ----------------------------------------------------------------
// The buffer is full but at least two packets. Send those in
// packed form.
// ----------------------------------------------------------------
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferTA[0],
                     TpacketLen);
    sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
    TpacketLen = 0;
    TnoOfPackets = 0;
  }//if
// ----------------------------------------------------------------
// Copy the signal into the buffer
// ----------------------------------------------------------------
  hostBuffer[hostId].packetBufferTA[TpacketLen + 0] = Theader;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 1] = sig0;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 2] = sig1;
  hostBuffer[hostId].packetBufferTA[TpacketLen + 3] = sig2;
  hostBuffer[hostId].noOfPacketsTA = TnoOfPackets + 1;
  hostBuffer[hostId].packetLenTA = Tlen + TpacketLen + 1;
  MEMCOPY_NO_WORDS(&hostBuffer[hostId].packetBufferTA[TpacketLen + 4],
                   &signal->theData[25],
                   Tlen - 3);
}//Dbtup::bufferTRANSID_AI()

void Dbtup::updatePackedList(Signal* signal, Uint16 hostId)
{
  if (hostBuffer[hostId].inPackedList == false) {
    Uint32 TpackedListIndex = cpackedListIndex;
    ljam();
    hostBuffer[hostId].inPackedList = true;
    cpackedList[TpackedListIndex] = hostId;
    cpackedListIndex = TpackedListIndex + 1;
  }//if
}//Dbtup::updatePackedList()

/* ---------------------------------------------------------------- */
/* ----------------------- SEND READ ATTRINFO --------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::sendReadAttrinfo(Signal* signal,
                             Uint32 ToutBufIndex,
                             const Operationrec * const regOperPtr)
{
  if(ToutBufIndex == 0)
    return;

  const BlockReference recBlockref = regOperPtr->recBlockref;
  const Uint32 sig0 = regOperPtr->tcOperationPtr;
  const Uint32 sig1 = regOperPtr->transid1;
  const Uint32 sig2 = regOperPtr->transid2;

  const Uint32 block = refToBlock(recBlockref);
  const Uint32 nodeId = refToNode(recBlockref);

  bool connectedToNode = getNodeInfo(nodeId).m_connected;
  const Uint32 type = getNodeInfo(nodeId).m_type;
  bool is_api = (type >= NodeInfo::API && type <= NodeInfo::MGM);
  bool old_dest = (getNodeInfo(nodeId).m_version < MAKE_VERSION(3,5,0));
  const Uint32 TpacketTA = hostBuffer[nodeId].noOfPacketsTA;
  const Uint32 TpacketLen = hostBuffer[nodeId].packetLenTA;
  
  if (ERROR_INSERTED(4006) && (nodeId != getOwnNodeId())){
    // Use error insert to turn routing on
    ljam();
    connectedToNode = false;    
  }

  TransIdAI * transIdAI =  (TransIdAI *)signal->getDataPtrSend();
  transIdAI->connectPtr = sig0;
  transIdAI->transId[0] = sig1;
  transIdAI->transId[1] = sig2;

  if (connectedToNode){
    /**
     * Own node -> execute direct
     */
    if(nodeId != getOwnNodeId()){
      ljam();
    
      /**
       * Send long sig
       */
      if(ToutBufIndex >= 22 && is_api && !old_dest) {
	ljam();
	/**
	 * Flush buffer so that order is maintained
	 */
	if (TpacketTA != 0) {
	  ljam();
	  BlockReference TBref = numberToRef(API_PACKED, nodeId);
	  MEMCOPY_NO_WORDS(&signal->theData[0],
			   &hostBuffer[nodeId].packetBufferTA[0],
			   TpacketLen);
	  sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
	  hostBuffer[nodeId].noOfPacketsTA = 0;
	  hostBuffer[nodeId].packetLenTA = 0;
	  transIdAI->connectPtr = sig0;
	  transIdAI->transId[0] = sig1;
	  transIdAI->transId[1] = sig2;
	}//if
	LinearSectionPtr ptr[3];
	ptr[0].p = &signal->theData[25];
	ptr[0].sz = ToutBufIndex;
	sendSignal(recBlockref, GSN_TRANSID_AI, signal, 3, JBB, ptr, 1);
	return;
      }
      
      /**
       * short sig + api -> buffer
       */
#ifndef NDB_NO_DROPPED_SIGNAL
      if (ToutBufIndex < 22 && is_api){
	ljam();
	bufferTRANSID_AI(signal, recBlockref, 3+ToutBufIndex);
	return;
      }//if
#endif      

      /**
       * rest -> old send sig
       */
      Uint32 * src = signal->theData+25;
      if(ToutBufIndex >= 22){
	do {
	  ljam();
	  MEMCOPY_NO_WORDS(&signal->theData[3], src, 22);
	  sendSignal(recBlockref, GSN_TRANSID_AI, signal, 25, JBB);
	  ToutBufIndex -= 22;
	  src += 22;
	} while(ToutBufIndex >= 22);
      }
      
      if(ToutBufIndex > 0){
	ljam();
	MEMCOPY_NO_WORDS(&signal->theData[3], src, ToutBufIndex);
	sendSignal(recBlockref, GSN_TRANSID_AI, signal, 3+ToutBufIndex, JBB);
      }
      return;
    }
    EXECUTE_DIRECT(block, GSN_TRANSID_AI, signal, 3 + ToutBufIndex);
    ljamEntry();
    return;
  }

  /** 
   * If this node does not have a direct connection 
   * to the receiving node we want to send the signals 
   * routed via the node that controls this read
   */
  Uint32 routeBlockref = regOperPtr->coordinatorTC;
  
  if(true){ // TODO is_api && !old_dest){
    ljam();
    transIdAI->attrData[0] = recBlockref;
    LinearSectionPtr ptr[3];
    ptr[0].p = &signal->theData[25];
    ptr[0].sz = ToutBufIndex;
    sendSignal(routeBlockref, GSN_TRANSID_AI_R, signal, 4, JBB, ptr, 1);
    return;
  }
  
  /**
   * Fill in a TRANSID_AI signal, use last word to store
   * final destination and send it to route node
   * as signal TRANSID_AI_R (R as in Routed)
   */ 
  Uint32 tot = ToutBufIndex;
  Uint32 sent = 0;
  Uint32 maxLen = TransIdAI::DataLength - 1;
  while (sent < tot) {
    ljam();      
    Uint32 dataLen = (tot - sent > maxLen) ? maxLen : tot - sent;
    Uint32 sigLen = dataLen + TransIdAI::HeaderLength + 1; 
    MEMCOPY_NO_WORDS(&transIdAI->attrData,
		     &signal->theData[25+sent],
		     dataLen);
    // Set final destination in last word
    transIdAI->attrData[dataLen] = recBlockref;
    
    sendSignal(routeBlockref, GSN_TRANSID_AI_R, 
	       signal, sigLen, JBB);
    sent += dataLen;
  }
}//Dbtup::sendReadAttrinfo()
