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
    Uint32 TpacketRC = hostBuffer[hostId].noOfPacketsRC;
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
    if (TpacketRC != 0) {
      ljam();
      BlockReference TBref = numberToRef(API_PACKED, hostId);
      Uint32 TpacketLen = hostBuffer[hostId].packetLenRC;
      MEMCOPY_NO_WORDS(&signal->theData[0],
                       &hostBuffer[hostId].packetBufferRC[0],
                       TpacketLen);
      sendSignal(TBref, GSN_READCONF, signal, TpacketLen, JBB);
      hostBuffer[hostId].noOfPacketsRC = 0;
      hostBuffer[hostId].packetLenRC = 0;
    }//if
    hostBuffer[hostId].inPackedList = false;
  }//for
  cpackedListIndex = 0;
}//Dbtup::execSEND_PACKED()

void Dbtup::bufferREADCONF(Signal* signal, BlockReference aRef,
                           Uint32* buffer, Uint32 Tlen)
{
  Uint32 hostId = refToNode(aRef);
  Uint32 Theader = ((refToBlock(aRef) << 16) + (Tlen-3));

  ndbrequire(hostId < MAX_NODES);
  Uint32 TpacketLen = hostBuffer[hostId].packetLenRC;
  Uint32 TnoOfPackets = hostBuffer[hostId].noOfPacketsRC;
  Uint32 sig0 = signal->theData[0];
  Uint32 sig1 = signal->theData[1];
  Uint32 sig2 = signal->theData[2];
  Uint32 sig3 = signal->theData[3];

  BlockReference TBref = numberToRef(API_PACKED, hostId);

  if ((Tlen + TpacketLen + 1) <= 25) {
// ----------------------------------------------------------------
// There is still space in the buffer. We will copy it into the
// buffer.
// ----------------------------------------------------------------
    ljam();
    updatePackedList(signal, hostId);
  } else if (TnoOfPackets == 1) {
// ----------------------------------------------------------------
// The buffer is full and there was only one packet buffered. We
// will send this as a normal signal.
// ----------------------------------------------------------------
    Uint32 TnewRef = numberToRef((hostBuffer[hostId].packetBufferRC[0] >> 16), 
                                hostId);
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferRC[1],
                     TpacketLen - 1);
    sendSignal(TnewRef, GSN_READCONF, signal, (TpacketLen - 1), JBB);
    TpacketLen = 0;
    TnoOfPackets = 0;
  } else {
// ----------------------------------------------------------------
// The buffer is full but at least two packets. Send those in
// packed form.
// ----------------------------------------------------------------
    MEMCOPY_NO_WORDS(&signal->theData[0],
                     &hostBuffer[hostId].packetBufferRC[0],
                     TpacketLen);
    sendSignal(TBref, GSN_READCONF, signal, TpacketLen, JBB);
    TpacketLen = 0;
    TnoOfPackets = 0;
  }//if
// ----------------------------------------------------------------
// Copy the signal into the buffer
// ----------------------------------------------------------------
  hostBuffer[hostId].packetBufferRC[TpacketLen + 0] = Theader;
  hostBuffer[hostId].packetBufferRC[TpacketLen + 1] = sig0;
  hostBuffer[hostId].packetBufferRC[TpacketLen + 2] = sig1;
  hostBuffer[hostId].packetBufferRC[TpacketLen + 3] = sig2;
  hostBuffer[hostId].packetBufferRC[TpacketLen + 4] = sig3;
  hostBuffer[hostId].noOfPacketsRC = TnoOfPackets + 1;
  hostBuffer[hostId].packetLenRC = Tlen + TpacketLen + 1;
  MEMCOPY_NO_WORDS(&hostBuffer[hostId].packetBufferRC[TpacketLen + 5],
                   buffer,
                   Tlen - 4);
}//Dbtup::bufferREADCONF()

void Dbtup::bufferTRANSID_AI(Signal* signal, BlockReference aRef,
                             Uint32* buffer, Uint32 Tlen)
{
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
  } else if (TnoOfPackets == 1) {
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
                   buffer,
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
  const BlockReference recBlockref = regOperPtr->recBlockref;
  bool toOwnNode =  refToNode(recBlockref) == getOwnNodeId();
  bool connectedToNode = getNodeInfo(refToNode(recBlockref)).m_connected;
  const Uint32 type = getNodeInfo(refToNode(recBlockref)).m_type;
  bool is_api = (type >= NodeInfo::API && type <= NodeInfo::REP);
  
  if (ERROR_INSERTED(4006)){
    // Use error insert to turn routing on
    ljam();
    connectedToNode = false;    
  }

  if (!toOwnNode && !connectedToNode){
    /** 
     * If this node does not have a direct connection 
     * to the receiving node we want to send the signals 
     * routed via the node that controls this read
     */
    Uint32 routeBlockref = regOperPtr->coordinatorTC;

    /**
     * Fill in a TRANSID_AI signal, use last word to store
     * final destination and send it to route node
     * as signal TRANSID_AI_R (R as in Routed)
     */ 
    TransIdAI * const transIdAI =  (TransIdAI *)signal->getDataPtr();
    transIdAI->connectPtr = regOperPtr->tcOperationPtr;
    transIdAI->transId[0] = regOperPtr->transid1;
    transIdAI->transId[1] = regOperPtr->transid2;
      
    Uint32 tot = ToutBufIndex;
    Uint32 sent = 0;
    Uint32 maxLen = TransIdAI::DataLength - 1;
    while (sent < tot) {
      ljam();      
      Uint32 dataLen = (tot - sent > maxLen) ? maxLen : tot - sent;
      Uint32 sigLen = dataLen + TransIdAI::HeaderLength + 1; 
      MEMCOPY_NO_WORDS(&transIdAI->attrData,
		       &coutBuffer[sent],
		       dataLen);
      // Set final destination in last word
      transIdAI->attrData[dataLen] = recBlockref;

      sendSignal(routeBlockref, GSN_TRANSID_AI_R, 
		 signal, sigLen, JBB);
      sent += dataLen;
      
    }
    return;
  }

  Uint32 TbufIndex = 0;
  Uint32 sig0 = regOperPtr->tcOperationPtr;
  Uint32 sig1 = regOperPtr->transid1;
  Uint32 sig2 = regOperPtr->transid2;
  signal->theData[0] = sig0;
  signal->theData[1] = sig1;
  signal->theData[2] = sig2;

  while (ToutBufIndex > 21) {
    ljam();
    MEMCOPY_NO_WORDS(&signal->theData[3],
                     &coutBuffer[TbufIndex],
                     22);
    TbufIndex += 22;
    ToutBufIndex -= 22;
    const BlockReference sendBref = regOperPtr->recBlockref;
    if (refToNode(sendBref) != getOwnNodeId()) {
      ljam();
      sendSignal(sendBref, GSN_TRANSID_AI, signal, 25, JBB);
      ljam();
    } else {
      ljam();
      EXECUTE_DIRECT(refToBlock(sendBref), GSN_TRANSID_AI, signal, 25);
      ljamEntry();
    }//if
  }//while

  Uint32 TsigNumber;
  Uint32 TsigLen;
  Uint32 TdataIndex;
  if ((regOperPtr->opSimple == ZTRUE) &&
      (regOperPtr->optype == ZREAD)) {
                            /* DIRTY OPERATIONS ARE ALSO SIMPLE */
    ljam();
    Uint32 sig3 = regOperPtr->attroutbufLen;
    TdataIndex = 4;
    TsigLen = 4 + ToutBufIndex;
    TsigNumber = GSN_READCONF;
    signal->theData[3] = sig3;
    if ((TsigLen < 18) && is_api){
      bufferREADCONF(signal, regOperPtr->recBlockref,
                     &coutBuffer[TbufIndex], TsigLen);
      return;
    }//if
  } else if (ToutBufIndex > 0) {
    ljam();
    TdataIndex = 3;
    TsigLen = 3 + ToutBufIndex;
    TsigNumber = GSN_TRANSID_AI;
    if ((TsigLen < 18) && is_api){
      ljam();
      bufferTRANSID_AI(signal, regOperPtr->recBlockref,
                       &coutBuffer[TbufIndex], TsigLen);
      return;
    }//if
  } else {
    ljam();
    return;
  }//if
  MEMCOPY_NO_WORDS(&signal->theData[TdataIndex],
                   &coutBuffer[TbufIndex],
                   ToutBufIndex);
  const BlockReference sendBref = regOperPtr->recBlockref;
  if (refToNode(sendBref) != getOwnNodeId()) {
    ljam();
    sendSignal(sendBref, TsigNumber, signal, TsigLen, JBB);
  } else {
    EXECUTE_DIRECT(refToBlock(sendBref), GSN_TRANSID_AI, signal, TsigLen);
    ljamEntry();
  }//if
}//Dbtup::sendReadAttrinfo()
