/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "Packer.hpp"
#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include <RefConvert.hpp>
#include "BaseString.hpp"
#include "EventLogger.hpp"
#include "BlockNumbers.h"

#ifdef ERROR_INSERT
Uint32 MAX_RECEIVED_SIGNALS = 1024;
#else
#define MAX_RECEIVED_SIGNALS 1024
#endif

extern EventLogger* g_eventLogger;

void
TransporterRegistry::dump_and_report_bad_message(const char file[], unsigned line,
                          TransporterReceiveHandle & recvHandle,
                          Uint32 * readPtr,
                          size_t sizeInWords,
                          NodeId remoteNodeId,
                          IOState state,
                          TransporterError errorCode)
{
  report_error(remoteNodeId, errorCode);

  char msg[MAX_LOG_MESSAGE_SIZE];
  const size_t sz = sizeof(msg);
  Uint32 nextMsgOffset = Protocol6::getMessageLength(*readPtr);
  if (sizeInWords >= nextMsgOffset)
  {
    nextMsgOffset = 0;
  }

  {
    size_t offs = 0;
    ssize_t nb;
    nb = BaseString::snprintf(msg + offs, sz - offs, "%s: %u: ", file, line);
    if (nb < 0) goto log_it;
    offs += nb;

    // Get error message for errorCode
    LogLevel::EventCategory cat;
    Uint32 threshold;
    Logger::LoggerLevel severity;
    EventLogger::EventTextFunction textF;
    EventLoggerBase::event_lookup(NDB_LE_TransporterError,
                                  cat, threshold, severity, textF);
    Uint32 TE_words[3] = {0, remoteNodeId, (Uint32) errorCode};
    g_eventLogger->getText(msg + offs, sz - offs, textF, TE_words, 3);
    nb = strlen(msg + offs);
    if (nb < 0) goto log_it;
    offs += nb;

    const bool bad_data = recvHandle.m_bad_data_transporters.get(remoteNodeId);
    nb = BaseString::snprintf(msg + offs, sz - offs,
                  "\n"
                  "PerformState %u: IOState %u: bad_data %u\n"
                  "ptr %p: size %u bytes\n",
                  performStates[remoteNodeId], state, bad_data,
                  readPtr, (unsigned)(sizeInWords * 4));
    if (nb < 0) goto log_it;
    offs += nb;
    size_t reserve;
    if (!nextMsgOffset)
    {
      // If next message wont be dumped, print as much as possible
      // from start of buffer.
      reserve = 0;
    }
    else
    {
      // Keep some space for dumping next message, about 10 words
      // plus 6 preceding words.
      reserve = 200;
    }
    nb = BaseString::hexdump(msg + offs, sz - offs - reserve, readPtr, sizeInWords);
    if (nb < 0) goto log_it;
    offs += nb;
    if (nextMsgOffset)
    {
      // Always print some words preceding next message. But assume
      // at least 60 words will be printed for current message.
      if (nextMsgOffset > 60)
      {
        nb = BaseString::snprintf(msg + offs, sz - offs,
                  "Before next ptr %p\n",
                  readPtr + nextMsgOffset - 6);
        if (nb < 0) goto log_it;
        offs += nb;
        nb = BaseString::hexdump(msg + offs, sz - offs, readPtr + nextMsgOffset - 6, 6);
        offs += nb;
      }
      // Dump words for next message.
      nb = BaseString::snprintf(msg + offs, sz - offs,
                    "Next ptr %p\n",
                    readPtr + nextMsgOffset);
      if (nb < 0) goto log_it;
      offs += nb;
      nb = BaseString::hexdump(msg + offs, sz - offs, readPtr + nextMsgOffset, sizeInWords - nextMsgOffset);
      if (nb < 0) goto log_it;
      offs += nb;
    }
  }

log_it:
  g_eventLogger->error("%s", msg);
  recvHandle.m_bad_data_transporters.set(remoteNodeId);
}

static
inline
bool
unpack_one(Uint32* (&readPtr), Uint32* eodPtr,
           Uint8 (&prio), SignalHeader (&signalHeader), Uint32* (&signalData), LinearSectionPtr ptr[],
           TransporterError (&errorCode))
{
  Uint32 word1 = readPtr[0];
  Uint32 word2 = readPtr[1];
  Uint32 word3 = readPtr[2];

  if (unlikely(!Protocol6::verifyByteOrder(word1, MY_OWN_BYTE_ORDER)))
  {
    errorCode = TE_UNSUPPORTED_BYTE_ORDER;
    return false;
  }

  if (unlikely(Protocol6::getCompressed(word1))){
    errorCode = TE_COMPRESSED_UNSUPPORTED;
    return false;
  }

  const Uint16 messageLen32    = Protocol6::getMessageLength(word1);

  if(unlikely(messageLen32 == 0 ||
              messageLen32 > (MAX_RECV_MESSAGE_BYTESIZE >> 2))){
    DEBUG("Message Size = " << messageLen32 << " words");
    errorCode = TE_INVALID_MESSAGE_LENGTH;
    return false;
  }//if

  if (unlikely(eodPtr < readPtr + messageLen32)) {
    errorCode = TE_NO_ERROR;
    return false;
  }//if

  if(unlikely(Protocol6::getCheckSumIncluded(word1))){
    const Uint32 tmpLen = messageLen32 - 1;
    const Uint32 checkSumSent     = readPtr[tmpLen];
    const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);

    if(unlikely(checkSumComputed != checkSumSent)){
      errorCode = TE_INVALID_CHECKSUM;
      return false;
    }//if
  }//if

  signalData = &readPtr[3];

  readPtr     += messageLen32;

  Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);

  prio = Protocol6::getPrio(word1);

  if(Protocol6::getSignalIdIncluded(word1) == 0){
    signalHeader.theSendersSignalId = ~0;
  } else {
    signalHeader.theSendersSignalId = * signalData;
    signalData ++;
  }//if
  signalHeader.theSignalId= ~0;

  Uint32 * sectionPtr = signalData + signalHeader.theLength;
  Uint32 * sectionData = sectionPtr + signalHeader.m_noOfSections;
  for(Uint32 i = 0; i<signalHeader.m_noOfSections; i++){
    Uint32 sz = * sectionPtr;
    ptr[i].sz = sz;
    ptr[i].p = sectionData;

    sectionPtr ++;
    sectionData += sz;
  }

  if(Protocol6::getCheckSumIncluded(word1))
  {
    sectionData ++;
  }
  if (sectionData != readPtr)
  {
    readPtr -= messageLen32;
    errorCode = TE_INVALID_MESSAGE_LENGTH;
    return false;
  }

  /* check of next message if possible before delivery */
  if (eodPtr > readPtr)
  { // check next message word1
    Uint32 word1 = *readPtr;
    // check byte order
    if (unlikely(!Protocol6::verifyByteOrder(word1, MY_OWN_BYTE_ORDER)))
    {
      errorCode = TE_UNSUPPORTED_BYTE_ORDER;
      return false;
    }
    if (unlikely(Protocol6::getCompressed(word1)))
    {
      errorCode = TE_COMPRESSED_UNSUPPORTED;
      return false;
    }
    // check message size
    const Uint16 messageLen32    = Protocol6::getMessageLength(word1);
    if (unlikely(messageLen32 == 0 ||
                messageLen32 > (MAX_RECV_MESSAGE_BYTESIZE >> 2)))
    {
      DEBUG("Message Size = " << messageLenBytes);
      errorCode = TE_INVALID_MESSAGE_LENGTH;
      return false;
    }//if
  }

  return true;
}

Uint32
TransporterRegistry::unpack(TransporterReceiveHandle & recvHandle,
                            Uint32 * readPtr,
                            Uint32 sizeOfData,
                            NodeId remoteNodeId,
                            IOState state,
                            bool & stopReceiving)
{
  assert(stopReceiving == false);
  // If bad data detected in  previous run
  // skip all further data
  if (unlikely(recvHandle.m_bad_data_transporters.get(remoteNodeId)))
  {
    return sizeOfData;
  }

  Uint8 prio;
  SignalHeader signalHeader;
  Uint32* signalData;
  LinearSectionPtr ptr[3];
  TransporterError errorCode = TE_NO_ERROR;
  
  Uint32* const sodPtr = readPtr;
  Uint32* const eodPtr = readPtr + (sizeOfData >> 2);
  Uint32 loop_count = 0; 
  bool doStopReceiving = false;
 
  if(likely(state == NoHalt || state == HaltOutput)){
    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) &&
	   doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, prio, signalHeader, signalData, ptr, errorCode)) {
      loop_count++;
      
      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;
      
      doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio, signalData, ptr);
      
    }//while
  } else {
    /** state = HaltIO || state == HaltInput */

    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) &&
	   doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, prio, signalHeader, signalData, ptr, errorCode)) {
      loop_count++;

      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;

      if(rBlockNum == QMGR){
	Uint32 sBlockNum = signalHeader.theSendersBlockRef;
	sBlockNum = numberToRef(sBlockNum, remoteNodeId);
	signalHeader.theSendersBlockRef = sBlockNum;

	doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio, signalData, ptr);
      } else {
	DEBUG("prepareReceive(...) - Discarding message to block: "
	      << rBlockNum << " from Node: " << remoteNodeId);
      }//if
    }//while
  }//if

  if (errorCode != TE_NO_ERROR)
  {
    dump_and_report_bad_message(__FILE__, __LINE__,
            recvHandle, readPtr, eodPtr - readPtr, remoteNodeId, state,
            errorCode);
  }

  stopReceiving = doStopReceiving;
  return (readPtr - sodPtr) * sizeof(Uint32);
}

Uint32 *
TransporterRegistry::unpack(TransporterReceiveHandle & recvHandle,
                            Uint32 * readPtr,
                            Uint32 * eodPtr,
                            NodeId remoteNodeId,
                            IOState state,
                            bool & stopReceiving)
{
  assert(stopReceiving == false);

  // If bad data detected in previous run
  // skip all further data
  if (unlikely(recvHandle.m_bad_data_transporters.get(remoteNodeId)))
  {
    return eodPtr;
  }

  Uint8 prio;
  SignalHeader signalHeader;
  Uint32* signalData;
  LinearSectionPtr ptr[3];
  TransporterError errorCode = TE_NO_ERROR;
  
  Uint32 loop_count = 0; 
  bool doStopReceiving = false;
 
  if(likely(state == NoHalt || state == HaltOutput)){
    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) &&
	   doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, prio, signalHeader, signalData, ptr, errorCode)) {
      loop_count++;
      
      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;
      
      doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio, signalData, ptr);
      
    }//while
  } else {
    /** state = HaltIO || state == HaltInput */

    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) &&
	   doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, prio, signalHeader, signalData, ptr, errorCode)) {
      loop_count++;

      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;

      if(rBlockNum == 252){
	Uint32 sBlockNum = signalHeader.theSendersBlockRef;
	sBlockNum = numberToRef(sBlockNum, remoteNodeId);
	signalHeader.theSendersBlockRef = sBlockNum;

	doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio, signalData, ptr);
      } else {
	DEBUG("prepareReceive(...) - Discarding message to block: "
	      << rBlockNum << " from Node: " << remoteNodeId);
      }//if
    }//while
  }//if

  if (errorCode != TE_NO_ERROR)
  {
    dump_and_report_bad_message(__FILE__, __LINE__,
            recvHandle, readPtr, eodPtr - readPtr, remoteNodeId, state,
            errorCode);
  }

  stopReceiving = doStopReceiving;
  return readPtr;
}

Packer::Packer(bool signalId, bool checksum) {
  
  checksumUsed    = (checksum ? 1 : 0);
  signalIdUsed    = (signalId ? 1 : 0);
  
  // Set the priority

  preComputedWord1 = 0;
  Protocol6::setByteOrder(preComputedWord1, MY_OWN_BYTE_ORDER);
  Protocol6::setSignalIdIncluded(preComputedWord1, signalIdUsed);
  Protocol6::setCheckSumIncluded(preComputedWord1, checksumUsed);
  Protocol6::setCompressed(preComputedWord1, 0);
}

inline
void
import(Uint32 * & insertPtr, const LinearSectionPtr & ptr){
  const Uint32 sz = ptr.sz;
  memcpy(insertPtr, ptr.p, 4 * sz);
  insertPtr += sz;
}

inline
void
importGeneric(Uint32 * & insertPtr, const GenericSectionPtr & ptr){
  /* Use the section iterator to obtain the words in this section */
  Uint32 remain= ptr.sz;

  while (remain > 0)
  {
    Uint32 len= 0;
    const Uint32* next= ptr.sectionIter->getNextWords(len);

    assert(len <= remain);
    assert(next != NULL);

    memcpy(insertPtr, next, 4 * len);
    insertPtr+= len;
    remain-= len;
  }

  /* Check that there were no more words available from the
   * Signal iterator
   */
  assert(ptr.sectionIter->getNextWords(remain) == NULL);
}

void copy(Uint32 * & insertPtr, 
	  class SectionSegmentPool &, const SegmentedSectionPtr & ptr);

void
Packer::pack(Uint32 * insertPtr, 
	     Uint32 prio, 
	     const SignalHeader * header, 
	     const Uint32 * theData,
	     const LinearSectionPtr ptr[3]) const {
  Uint32 i;
  
  Uint32 dataLen32 = header->theLength;
  Uint32 no_segs = header->m_noOfSections;

  Uint32 len32 = 
    dataLen32 + no_segs + 
    checksumUsed + signalIdUsed + (sizeof(Protocol6)/4);
  

  for(i = 0; i<no_segs; i++){
    len32 += ptr[i].sz;
  }
  
  /**
   * Do insert of data
   */
  Uint32 word1 = preComputedWord1;
  Uint32 word2 = 0;
  Uint32 word3 = 0;
  
  Protocol6::setPrio(word1, prio);
  Protocol6::setMessageLength(word1, len32);
  Protocol6::createProtocol6Header(word1, word2, word3, header);

  insertPtr[0] = word1;
  insertPtr[1] = word2;
  insertPtr[2] = word3;
  
  Uint32 * tmpInserPtr = &insertPtr[3];
  
  if(signalIdUsed){
    * tmpInserPtr = header->theSignalId;
    tmpInserPtr++;
  }
  
  memcpy(tmpInserPtr, theData, 4 * dataLen32);

  tmpInserPtr += dataLen32;
  for(i = 0; i<no_segs; i++){
    tmpInserPtr[i] = ptr[i].sz;
  }

  tmpInserPtr += no_segs;
  for(i = 0; i<no_segs; i++){
    import(tmpInserPtr, ptr[i]);
  }
  
  if(checksumUsed){
    * tmpInserPtr = computeChecksum(&insertPtr[0], len32-1);
  }
}

void
Packer::pack(Uint32 * insertPtr, 
	     Uint32 prio, 
	     const SignalHeader * header, 
	     const Uint32 * theData,
	     class SectionSegmentPool & thePool,
	     const SegmentedSectionPtr ptr[3]) const {
  Uint32 i;
  
  Uint32 dataLen32 = header->theLength;
  Uint32 no_segs = header->m_noOfSections;

  Uint32 len32 = 
    dataLen32 + no_segs + 
    checksumUsed + signalIdUsed + (sizeof(Protocol6)/4);
  
  for(i = 0; i<no_segs; i++){
    len32 += ptr[i].sz;
  }
  
  /**
   * Do insert of data
   */
  Uint32 word1 = preComputedWord1;
  Uint32 word2 = 0;
  Uint32 word3 = 0;
  
  Protocol6::setPrio(word1, prio);
  Protocol6::setMessageLength(word1, len32);
  Protocol6::createProtocol6Header(word1, word2, word3, header);

  insertPtr[0] = word1;
  insertPtr[1] = word2;
  insertPtr[2] = word3;
  
  Uint32 * tmpInserPtr = &insertPtr[3];
  
  if(signalIdUsed){
    * tmpInserPtr = header->theSignalId;
    tmpInserPtr++;
  }
  
  memcpy(tmpInserPtr, theData, 4 * dataLen32);
  
  tmpInserPtr += dataLen32;
  for(i = 0; i<no_segs; i++){
    tmpInserPtr[i] = ptr[i].sz;
  }

  tmpInserPtr += no_segs;
  for(i = 0; i<no_segs; i++){
    copy(tmpInserPtr, thePool, ptr[i]);
  }
  
  if(checksumUsed){
    * tmpInserPtr = computeChecksum(&insertPtr[0], len32-1);
  }
}


void
Packer::pack(Uint32 * insertPtr, 
	     Uint32 prio, 
	     const SignalHeader * header, 
	     const Uint32 * theData,
	     const GenericSectionPtr ptr[3]) const {
  Uint32 i;
  
  Uint32 dataLen32 = header->theLength;
  Uint32 no_segs = header->m_noOfSections;

  Uint32 len32 = 
    dataLen32 + no_segs + 
    checksumUsed + signalIdUsed + (sizeof(Protocol6)/4);
  

  for(i = 0; i<no_segs; i++){
    len32 += ptr[i].sz;
  }
  
  /**
   * Do insert of data
   */
  Uint32 word1 = preComputedWord1;
  Uint32 word2 = 0;
  Uint32 word3 = 0;
  
  Protocol6::setPrio(word1, prio);
  Protocol6::setMessageLength(word1, len32);
  Protocol6::createProtocol6Header(word1, word2, word3, header);

  insertPtr[0] = word1;
  insertPtr[1] = word2;
  insertPtr[2] = word3;
  
  Uint32 * tmpInsertPtr = &insertPtr[3];
  
  if(signalIdUsed){
    * tmpInsertPtr = header->theSignalId;
    tmpInsertPtr++;
  }
  
  memcpy(tmpInsertPtr, theData, 4 * dataLen32);

  tmpInsertPtr += dataLen32;
  for(i = 0; i<no_segs; i++){
    tmpInsertPtr[i] = ptr[i].sz;
  }

  tmpInsertPtr += no_segs;
  for(i = 0; i<no_segs; i++){
    importGeneric(tmpInsertPtr, ptr[i]);
  }
  
  if(checksumUsed){
    * tmpInsertPtr = computeChecksum(&insertPtr[0], len32-1);
  }
}

/**
 * Find longest data size that does not exceed given maximum, and does not
 * cause individual signals to be split.
 *
 * Used by SHM_Transporter, as it is designed to send data in Signal chunks,
 * not bytes or words.
 */
Uint32
TransporterRegistry::unpack_length_words(const Uint32 *readPtr, Uint32 maxWords)
{
  Uint32 wordLength = 0;

  while (wordLength + 4 + sizeof(Protocol6) <= maxWords)
  {
    Uint32 word1 = readPtr[wordLength];
    Uint16 messageLen32 = Protocol6::getMessageLength(word1);
    if (wordLength + messageLen32 > maxWords)
      break;
    wordLength += messageLen32;
  }
  return wordLength;
}
