/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_BUFFER_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TransIdAI.hpp>

#define JAM_FILE_ID 410


void Dbtup::execSEND_PACKED(Signal* signal)
{
  Uint16 hostId;
  Uint32 i;
  Uint32 TpackedListIndex= cpackedListIndex;
  bool present = false;
  for (i= 0; i < TpackedListIndex; i++) {
    jam();
    hostId= cpackedList[i];
    ndbrequire((hostId - 1) < (MAX_NODES - 1)); // Also check not zero
    HostBuffer* const buffer = &hostBuffer[hostId];
    Uint32 TpacketTA= buffer->noOfPacketsTA;
    if (TpacketTA != 0) {
      jamDebug();

      if (ERROR_INSERTED(4037))
      {
        /* Delay a SEND_PACKED signal for 10 calls to execSEND_PACKED */
        jam();
        if (!present)
        {
          /* First valid packed data in this pass */
          jamDebug();
          present = true;
          cerrorPackedDelay++;
          
          if ((cerrorPackedDelay % 10) != 0)
          {
            /* Skip it */
            jamDebug();
            return;
          }
        }
      }
      const BlockReference TBref= numberToRef(API_PACKED, hostId);
      const Uint32 TpacketLen= buffer->packetLenTA;
      MEMCOPY_NO_WORDS(&signal->theData[0],
                       &buffer->packetBufferTA[0],
                       TpacketLen);
      sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
      buffer->noOfPacketsTA= 0;
      buffer->packetLenTA= 0;
    }
    buffer->inPackedList= false;
  }//for
  cpackedListIndex= 0;
}

/**
 * Copy a TRANSID_AI signal, which alread has its header constructed in 'signal',
 * into a packed buffer structure.
 *
 * Prereq:
 *  - Signal should be sufficiently small to allow it to be 'packed'
 *  - Buffer should have sufficient free space for the signal.
 */
void Dbtup::bufferTRANSID_AI(Signal* signal, BlockReference aRef,
                             const Uint32 *dataBuf,
                             Uint32 lenOfData)
{
  ndbassert(lenOfData > 0);
  ndbassert(TransIdAI::HeaderLength+lenOfData+1 <= 25);

  const Uint32 hostId= refToNode(aRef);
  HostBuffer* const buffer = &hostBuffer[hostId];
  const Uint32 TpacketLen= buffer->packetLenTA;

  // ----------------------------------------------------------------
  // There should always be space in the buffer.
  // ----------------------------------------------------------------
  ndbassert((TpacketLen + 1+TransIdAI::HeaderLength+lenOfData) <= 25);

  // ----------------------------------------------------------------
  // Copy the header + TRANSID_AI signal into the buffer
  // ----------------------------------------------------------------
  Uint32* const packedBuffer = &buffer->packetBufferTA[TpacketLen];
  const Uint32 Theader= ((refToBlock(aRef) << 16)+lenOfData);
  packedBuffer[0] = Theader;

  MEMCOPY_NO_WORDS(&packedBuffer[1], signal->theData, TransIdAI::HeaderLength);
  MEMCOPY_NO_WORDS(&packedBuffer[1+TransIdAI::HeaderLength], dataBuf, lenOfData);

  buffer->packetLenTA= TpacketLen + 1+TransIdAI::HeaderLength+lenOfData;
  buffer->noOfPacketsTA++;
  updatePackedList(hostId);
}

void Dbtup::updatePackedList(Uint16 hostId)
{
  if (hostBuffer[hostId].inPackedList == false) {
    Uint32 TpackedListIndex= cpackedListIndex;
    jamDebug();
    hostBuffer[hostId].inPackedList= true;
    cpackedList[TpackedListIndex]= hostId;
    cpackedListIndex= TpackedListIndex + 1;
  }
}

/**
 * Send a TRANSID_AI signal to an API node. If sufficiently small, the signal is 
 * buffered for later being sent as a API_PACKED-signal. When required the 
 * packed buffer is flushed to the destination API-node.
 *
 * Prereq:
 *  - The destination node must be an API node.
 *  - We must be connected to the API node.
 */
void Dbtup::sendAPI_TRANSID_AI(Signal* signal,
                               Uint32 recBlockRef,
                               const Uint32 *dataBuf,
                               Uint32 lenOfData)
{
  const Uint32 nodeId= refToNode(recBlockRef);

  // Test prerequisites:
  ndbassert(getNodeInfo(nodeId).m_connected);
  ndbassert(getNodeInfo(nodeId).m_type >= NodeInfo::API && getNodeInfo(nodeId).m_type <= NodeInfo::MGM);

  ndbrequire(nodeId < MAX_NODES);
  HostBuffer* const buffer = &hostBuffer[nodeId];
  const Uint32 TpacketLen= buffer->packetLenTA;

  /**
   * Check if the packed buffers has to be flushed first.
   * Note that even if we will not use them for this (too large) signal,
   * it has to be flushed now in order to maintain the order of TRANSID_AIs
   */
  if (TpacketLen > 0 &&
      TpacketLen + 1+TransIdAI::HeaderLength+lenOfData > 25)
  {
    jamDebug();
    TransIdAI *transIdAI = (TransIdAI *)signal->getDataPtrSend();

    // Save prepare TRANSID_AI header
    const Uint32 sig0= transIdAI->connectPtr;
    const Uint32 sig1= transIdAI->transId[0];
    const Uint32 sig2= transIdAI->transId[1];

    if (dataBuf != &signal->theData[25])
    {
      jamDebug();
      /**
       * TUP incorrectly guessed that it could prepare the signal
       * to be EXECUTE_DIRECT'ly. Has to move it away for sendSignal()
       * needing the low 25 signal-words to send the packed buffers.
       * (Use memmove as src & dest may overlap)
       */
      memmove(&signal->theData[25], dataBuf, lenOfData*sizeof(Uint32));
      dataBuf = &signal->theData[25];
    }

    // Send already buffered TRANSID_AI(s) preceeding this TRANSID_AI
    const BlockReference TBref = numberToRef(API_PACKED, nodeId);
    MEMCOPY_NO_WORDS(&signal->theData[0], &buffer->packetBufferTA[0], TpacketLen);
    sendSignal(TBref, GSN_TRANSID_AI, signal, TpacketLen, JBB);
    buffer->noOfPacketsTA = 0;
    buffer->packetLenTA = 0;

    // Reconstruct the current TRANSID_AI header
    transIdAI->connectPtr = sig0;
    transIdAI->transId[0] = sig1;
    transIdAI->transId[1] = sig2;
  }

  if (lenOfData <= TransIdAI::DataLength)
  {
    /**
     * Short signal, buffer it, or send directly
     * 1) Buffer signal if we can pack at least
     *    this signal + another 1-word signal into buffers.
     * 2) else, short-signal is sent immediately.
     *
     * Note that the check for fitting a 1-word signal in addition
     * to this signal serves dual purposes:
     * - The 1-word signal is the smalles possible signal which
     *   can either be added later, or already is buffered.
     * - So failing to also add a 1-word signal implies that any
     *   previously buffered signals were flushed above.
     *   Thus, 'packetLenTA' is also known to be '== 0' in
     *   the non-buffered sendSignal further below.
     */
#ifndef NDB_NO_DROPPED_SIGNAL
    if (1+TransIdAI::HeaderLength + lenOfData +  // this TRANSID_AI
        1+TransIdAI::HeaderLength + 1 <= 25)     // 1 word TRANSID_AI
    {
      jamDebug();
      bufferTRANSID_AI(signal, recBlockRef, dataBuf, lenOfData);
    }
    else
#endif
    {
      jamDebug();
      ndbassert(buffer->packetLenTA == 0);
      if (dataBuf != &signal->theData[TransIdAI::HeaderLength])
      {
        MEMCOPY_NO_WORDS(&signal->theData[TransIdAI::HeaderLength], dataBuf, lenOfData);
      }
      sendSignal(recBlockRef, GSN_TRANSID_AI, signal,
                 TransIdAI::HeaderLength+lenOfData, JBB);
    }
  }
  else
  {
    jamDebug();
    /**
     * Send to API as a long signal.
     */
    LinearSectionPtr ptr[3];
    ptr[0].p= const_cast<Uint32*>(dataBuf);
    ptr[0].sz= lenOfData;
    sendSignal(recBlockRef, GSN_TRANSID_AI, signal,
               TransIdAI::HeaderLength, JBB, ptr, 1);
  }
}

/* ---------------------------------------------------------------- */
/* ----------------------- SEND READ ATTRINFO --------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::sendReadAttrinfo(Signal* signal,
                             KeyReqStruct *req_struct,
                             Uint32 ToutBufIndex)
{
  if(ToutBufIndex == 0)
    return;
  
  const BlockReference recBlockref= req_struct->rec_blockref;
  const Uint32 nodeId= refToNode(recBlockref);

  bool connectedToNode= getNodeInfo(nodeId).m_connected;
  const Uint32 type= getNodeInfo(nodeId).m_type;
  const bool is_api= (type >= NodeInfo::API && type <= NodeInfo::MGM);
  //const bool old_dest= (getNodeInfo(nodeId).m_version < MAKE_VERSION(6,4,0));

  if (ERROR_INSERTED(4006) && (nodeId != getOwnNodeId())){
    // Use error insert to turn routing on
    jam();
    connectedToNode= false;    
  }

  Uint32 sig0= req_struct->tc_operation_ptr;
  Uint32 sig1= req_struct->trans_id1;
  Uint32 sig2= req_struct->trans_id2;
  
  TransIdAI * transIdAI=  (TransIdAI *)signal->getDataPtrSend();
  transIdAI->connectPtr= sig0;
  transIdAI->transId[0]= sig1;
  transIdAI->transId[1]= sig2;
  
  const Uint32 routeBlockref= req_struct->TC_ref;
  /**
   * If we are not connected to the destination block, we may reach it 
   * indirectly by sending a TRANSID_AI_R signal to routeBlockref. Only
   * TC can handle TRANSID_AI_R signals. The 'ndbrequire' below should
   * check that there is no chance of sending TRANSID_AI_R to a block
   * that cannot handle it.
   */
  ndbassert (refToMain(routeBlockref) == DBTC || 
             /**
              * routeBlockref will point to SPJ for operations initiated by
              * that block. TRANSID_AI_R should not be sent to SPJ, as
              * SPJ will do its own internal error handling to compensate
              * for the lost TRANSID_AI signal.
              */
             refToMain(routeBlockref) == DBSPJ ||
             /** 
              * A node should always be connected to itself. So we should
              * never need to send TRANSID_AI_R in this case.
              */
             (nodeId == getOwnNodeId() && connectedToNode));

  /**
   * If a previous read_pseudo executed a 'FLUSH_AI', we may
   * already have sent a TRANSID_AI signal with the result row
   * to the API node. The result size was then already recorded
   * in 'read_length' and we should not add the size of this 
   * row as it is not part of the 'result' . 
   */
  if (req_struct->read_length != 0)
  {
    ndbassert(!is_api);  // API result already FLUSH_AI'ed
  }
  else
  {
    // No API-result produced yet, record this
    req_struct->read_length = ToutBufIndex;
  }

  if (connectedToNode){
    /**
     * Own node -> execute direct
     */
    if(nodeId != getOwnNodeId())
    {
      jamDebug();
    
      //Assume no prehistoric versions not supporting long TRANSID_AI signals anymore
      ndbassert(getNodeInfo(nodeId).m_version >= MAKE_VERSION(6,4,0));

      if (is_api)
      {
        sendAPI_TRANSID_AI(signal, recBlockref,
                           &signal->theData[25], ToutBufIndex);
      }

      /**
       * Send long signal if 'long' data.
       * Note that SPJ can *only* handle long signals
       */
      else if (ToutBufIndex > TransIdAI::DataLength ||
               refToMain(recBlockref) == DBSPJ)
      {
        jam();
        /**
         * Receiver block doesn't support packed 'short' signals.
         */
        LinearSectionPtr ptr[3];
        ptr[0].p= &signal->theData[25];
        ptr[0].sz= ToutBufIndex;
        sendSignal(recBlockref, GSN_TRANSID_AI, signal,
                   TransIdAI::HeaderLength, JBB, ptr, 1);
      }
      else
      {
        jam();
        ndbassert(ToutBufIndex <= TransIdAI::DataLength);
        /**
         * Data is 'short', send short signal
         */
        MEMCOPY_NO_WORDS(&signal->theData[TransIdAI::HeaderLength],
                         &signal->theData[25], ToutBufIndex);
        sendSignal(recBlockref, GSN_TRANSID_AI, signal,
                   TransIdAI::HeaderLength+ToutBufIndex, JBB);
      }
      return;
    } //nodeId != getOwnNodeId()
  
    /**
     * BACKUP, LQH & SUMA run in our thread, so we can EXECUTE_DIRECT().
     *
     * The UTIL/TC blocks are in another thread (in multi-threaded ndbd), so
     * must use sendSignal().
     *
     * In MT LQH only LQH and BACKUP are in same thread, and BACKUP only
     * in LCP case since user-backup uses single worker.
     */
    const bool sameInstance = refToInstance(recBlockref) == instance();
    const Uint32 blockNumber= refToMain(recBlockref);
    if (sameInstance &&
        (blockNumber == BACKUP ||
         blockNumber == DBLQH ||
         blockNumber == SUMA))
    {
      EXECUTE_DIRECT(blockNumber, GSN_TRANSID_AI, signal,
                     TransIdAI::HeaderLength + ToutBufIndex);
      jamEntryDebug();
    }
    else
    {
      jam();
      LinearSectionPtr ptr[3];
      ptr[0].p= &signal->theData[TransIdAI::HeaderLength];
      ptr[0].sz= ToutBufIndex;
      if (ERROR_INSERTED(4038))
      {
        /* Copy data to Seg-section for delayed send */
        Uint32 sectionIVal = RNIL;
        ndbrequire(appendToSection(sectionIVal, ptr[0].p, ptr[0].sz));
        SectionHandle sh(this, sectionIVal);
        
        sendSignalWithDelay(recBlockref, GSN_TRANSID_AI, signal, 10,
                            TransIdAI::HeaderLength, &sh);
      }
      else
      {
        /**
         * We are sending to the same node, it is important that we maintain
         * signal order with SCAN_FRAGCONF and other signals. So we make sure
         * that TRANSID_AI is sent at the same priority level as the
         * SCAN_FRAGCONF will be sent at.
         *
         * One case for this is Backups, the receiver is the first LDM thread
         * which could have raised priority of scan executions to Priority A.
         * To ensure that TRANSID_AI arrives there before SCAN_FRAGCONF we
         * send also TRANSID_AI on priority A if the signal is sent on prio A.
         */
        JobBufferLevel prioLevel = req_struct->m_prio_a_flag ? JBA : JBB;
        sendSignal(recBlockref,
                   GSN_TRANSID_AI,
                   signal,
                   TransIdAI::HeaderLength,
                   prioLevel,
                   ptr,
                   1);
      }
    }
    return;
  }

  /** 
   * If this node does not have a direct connection 
   * to the receiving node, we want to send the signals 
   * routed via the node that controls this read
   */
  // TODO is_api && !old_dest){
  if (refToNode(recBlockref) == refToNode(routeBlockref))
  {
    jam();
    /**
     * Signal's only alternative route is direct - cannot be delivered, 
     * drop it. (Expected behavior if recBlockRef is an SPJ block.)
     */
    return;
  }
  // Only TC can handle TRANSID_AI_R signals.
  ndbrequire(refToMain(routeBlockref) == DBTC);
  transIdAI->attrData[0]= recBlockref;
  LinearSectionPtr ptr[3];
  ptr[0].p= &signal->theData[25];
  ptr[0].sz= ToutBufIndex;
  sendSignal(routeBlockref, GSN_TRANSID_AI_R, signal,
             TransIdAI::HeaderLength+1, JBB, ptr, 1);
}
