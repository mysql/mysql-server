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

#include <ndb_global.h>

#include "Packer.hpp"
#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include <RefConvert.hpp>

Uint32
TransporterRegistry::unpack(Uint32 * readPtr,
			    Uint32 sizeOfData,
			    NodeId remoteNodeId,
			    IOState state) {
  SignalHeader signalHeader;
  LinearSectionPtr ptr[3];
  
  Uint32 usedData   = 0;
  
  if(state == NoHalt || state == HaltOutput){
    while(sizeOfData >= 4 + sizeof(Protocol6)){
      Uint32 word1 = readPtr[0];
      Uint32 word2 = readPtr[1];
      Uint32 word3 = readPtr[2];
      
#if 0
      if(Protocol6::getByteOrder(word1) != MY_OWN_BYTE_ORDER){
	//Do funky stuff
      }
#endif

      const Uint16 messageLen32    = Protocol6::getMessageLength(word1);
      const Uint32 messageLenBytes = ((Uint32)messageLen32) << 2;

      if(messageLen32 == 0 || messageLen32 > MAX_MESSAGE_SIZE){
        DEBUG("Message Size = " << messageLenBytes);
	reportError(callbackObj, remoteNodeId, TE_INVALID_MESSAGE_LENGTH);
        return usedData;
      }//if
      
      if (sizeOfData < messageLenBytes) {
	break;
      }//if
      
      if(Protocol6::getCheckSumIncluded(word1)){
	const Uint32 tmpLen = messageLen32 - 1;
	const Uint32 checkSumSent     = readPtr[tmpLen];
	const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);
	
	if(checkSumComputed != checkSumSent){
	  reportError(callbackObj, remoteNodeId, TE_INVALID_CHECKSUM);
          return usedData;
	}//if
      }//if
      
#if 0
      if(Protocol6::getCompressed(word1)){
	//Do funky stuff
      }//if
#endif
      
      Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);
      
      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;
      
      Uint8 prio = Protocol6::getPrio(word1);
      
      Uint32 * signalData = &readPtr[3];
      
      if(Protocol6::getSignalIdIncluded(word1) == 0){
	signalHeader.theSendersSignalId = ~0;
      } else {
	signalHeader.theSendersSignalId = * signalData;
	signalData ++;
      }//if
      
      Uint32 * sectionPtr = signalData + signalHeader.theLength;
      Uint32 * sectionData = sectionPtr + signalHeader.m_noOfSections;
      for(Uint32 i = 0; i<signalHeader.m_noOfSections; i++){
	Uint32 sz = * sectionPtr;
	ptr[i].sz = sz;
	ptr[i].p = sectionData;
	
	sectionPtr ++;
	sectionData += sz;
      }

      execute(callbackObj, &signalHeader, prio, signalData, ptr);
      
      readPtr     += messageLen32;
      sizeOfData  -= messageLenBytes;
      usedData    += messageLenBytes;
    }//while
    
    return usedData;
  } else {
    /** state = HaltIO || state == HaltInput */

    while(sizeOfData >= 4 + sizeof(Protocol6)){
      Uint32 word1 = readPtr[0];
      Uint32 word2 = readPtr[1];
      Uint32 word3 = readPtr[2];
      
#if 0
      if(Protocol6::getByteOrder(word1) != MY_OWN_BYTE_ORDER){
	//Do funky stuff
      }//if
#endif
      
      const Uint16 messageLen32    = Protocol6::getMessageLength(word1);
      const Uint32 messageLenBytes = ((Uint32)messageLen32) << 2;
      if(messageLen32 == 0 || messageLen32 > MAX_MESSAGE_SIZE){
	DEBUG("Message Size = " << messageLenBytes);
	reportError(callbackObj, remoteNodeId, TE_INVALID_MESSAGE_LENGTH);
        return usedData;
      }//if
      
      if (sizeOfData < messageLenBytes) {
	break;
      }//if
      
      if(Protocol6::getCheckSumIncluded(word1)){
	const Uint32 tmpLen = messageLen32 - 1;
	const Uint32 checkSumSent     = readPtr[tmpLen];
	const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);
	
	if(checkSumComputed != checkSumSent){
	  
	  //theTransporters[remoteNodeId]->disconnect();
	  reportError(callbackObj, remoteNodeId, TE_INVALID_CHECKSUM);
          return usedData;
	}//if
      }//if
      
#if 0
      if(Protocol6::getCompressed(word1)){
	//Do funky stuff
      }//if
#endif
      
      Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);
      
      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;

      if(rBlockNum == 252){
	Uint32 sBlockNum = signalHeader.theSendersBlockRef;
	sBlockNum = numberToRef(sBlockNum, remoteNodeId);
	signalHeader.theSendersBlockRef = sBlockNum;
	
	Uint8 prio = Protocol6::getPrio(word1);
	
	Uint32 * signalData = &readPtr[3];
	
	if(Protocol6::getSignalIdIncluded(word1) == 0){
	  signalHeader.theSendersSignalId = ~0;
	} else {
	  signalHeader.theSendersSignalId = * signalData;
	  signalData ++;
	}//if
	
	Uint32 * sectionPtr = signalData + signalHeader.theLength;
	Uint32 * sectionData = sectionPtr + signalHeader.m_noOfSections;
	for(Uint32 i = 0; i<signalHeader.m_noOfSections; i++){
	  Uint32 sz = * sectionPtr;
	  ptr[i].sz = sz;
	  ptr[i].p = sectionData;
	  
	  sectionPtr ++;
	  sectionData += sz;
	}

	execute(callbackObj, &signalHeader, prio, signalData, ptr);
      } else {
	DEBUG("prepareReceive(...) - Discarding message to block: "
	      << rBlockNum << " from Node: " << remoteNodeId);
      }//if
      
      readPtr     += messageLen32;
      sizeOfData  -= messageLenBytes;
      usedData    += messageLenBytes;
    }//while
    

    return usedData;
  }//if
}

Uint32 *
TransporterRegistry::unpack(Uint32 * readPtr,
			    Uint32 * eodPtr,
			    NodeId remoteNodeId,
			    IOState state) {
  static SignalHeader signalHeader;
  static LinearSectionPtr ptr[3];
  if(state == NoHalt || state == HaltOutput){
    while(readPtr < eodPtr){
      Uint32 word1 = readPtr[0];
      Uint32 word2 = readPtr[1];
      Uint32 word3 = readPtr[2];
      
#if 0
      if(Protocol6::getByteOrder(word1) != MY_OWN_BYTE_ORDER){
	//Do funky stuff
      }
#endif
      
      const Uint16 messageLen32    = Protocol6::getMessageLength(word1);
      
      if(messageLen32 == 0 || messageLen32 > MAX_MESSAGE_SIZE){
        DEBUG("Message Size(words) = " << messageLen32);
	reportError(callbackObj, remoteNodeId, TE_INVALID_MESSAGE_LENGTH);
        return readPtr;
      }//if
      
      if(Protocol6::getCheckSumIncluded(word1)){
	const Uint32 tmpLen = messageLen32 - 1;
	const Uint32 checkSumSent     = readPtr[tmpLen];
	const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);
	
	if(checkSumComputed != checkSumSent){
	  reportError(callbackObj, remoteNodeId, TE_INVALID_CHECKSUM);
	  return readPtr;
	}//if
      }//if
      
#if 0
      if(Protocol6::getCompressed(word1)){
	//Do funky stuff
      }//if
#endif
      
      Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);
      
      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;
      
      Uint8 prio = Protocol6::getPrio(word1);
      
      Uint32 * signalData = &readPtr[3];
      
      if(Protocol6::getSignalIdIncluded(word1) == 0){
	signalHeader.theSendersSignalId = ~0;
      } else {
	signalHeader.theSendersSignalId = * signalData;
	signalData ++;
      }//if

      Uint32 * sectionPtr = signalData + signalHeader.theLength;
      Uint32 * sectionData = sectionPtr + signalHeader.m_noOfSections;
      for(Uint32 i = 0; i<signalHeader.m_noOfSections; i++){
	Uint32 sz = * sectionPtr;
	ptr[i].sz = sz;
	ptr[i].p = sectionData;
	
	sectionPtr ++;
	sectionData += sz;
      }
      
      execute(callbackObj, &signalHeader, prio, signalData, ptr);
      
      readPtr += messageLen32;
    }//while
  } else {
    /** state = HaltIO || state == HaltInput */

    while(readPtr < eodPtr){
      Uint32 word1 = readPtr[0];
      Uint32 word2 = readPtr[1];
      Uint32 word3 = readPtr[2];
      
#if 0
      if(Protocol6::getByteOrder(word1) != MY_OWN_BYTE_ORDER){
	//Do funky stuff
      }//if
#endif
      
      const Uint16 messageLen32    = Protocol6::getMessageLength(word1);
      if(messageLen32 == 0 || messageLen32 > MAX_MESSAGE_SIZE){
	DEBUG("Message Size(words) = " << messageLen32);
	reportError(callbackObj, remoteNodeId, TE_INVALID_MESSAGE_LENGTH);
        return readPtr;
      }//if
      
      if(Protocol6::getCheckSumIncluded(word1)){
	const Uint32 tmpLen = messageLen32 - 1;
	const Uint32 checkSumSent     = readPtr[tmpLen];
	const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);
	
	if(checkSumComputed != checkSumSent){
	  
	  //theTransporters[remoteNodeId]->disconnect();
	  reportError(callbackObj, remoteNodeId, TE_INVALID_CHECKSUM);
	  return readPtr;
	}//if
      }//if
      
#if 0
      if(Protocol6::getCompressed(word1)){
	//Do funky stuff
      }//if
#endif
      
      Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);
      
      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;
      
      if(rBlockNum == 252){
	Uint32 sBlockNum = signalHeader.theSendersBlockRef;
	sBlockNum = numberToRef(sBlockNum, remoteNodeId);
	signalHeader.theSendersBlockRef = sBlockNum;
	
	Uint8 prio = Protocol6::getPrio(word1);
	
	Uint32 * signalData = &readPtr[3];
	
	if(Protocol6::getSignalIdIncluded(word1) == 0){
	  signalHeader.theSendersSignalId = ~0;
	} else {
	  signalHeader.theSendersSignalId = * signalData;
	  signalData ++;
	}//if
	
	Uint32 * sectionPtr = signalData + signalHeader.theLength;
	Uint32 * sectionData = sectionPtr + signalHeader.m_noOfSections;
	for(Uint32 i = 0; i<signalHeader.m_noOfSections; i++){
	  Uint32 sz = * sectionPtr;
	  ptr[i].sz = sz;
	  ptr[i].p = sectionData;
	  
	  sectionPtr ++;
	  sectionData += sz;
	}

	execute(callbackObj, &signalHeader, prio, signalData, ptr);
      } else {
	DEBUG("prepareReceive(...) - Discarding message to block: "
	      << rBlockNum << " from Node: " << remoteNodeId);
      }//if
      
      readPtr += messageLen32;
    }//while
  }//if
  return readPtr;
}

Packer::Packer(bool signalId, bool checksum) {
  
  checksumUsed    = (checksum ? 1 : 0);
  signalIdUsed    = (signalId ? 1 : 0);
  
  // Set the priority

  preComputedWord1 = 0;
  Protocol6::setByteOrder(preComputedWord1, 0);
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
