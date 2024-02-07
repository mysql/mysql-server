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

#include <RefConvert.hpp>
#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include "BaseString.hpp"
#include "BlockNumbers.h"
#include "EventLogger.hpp"
#include "Packer.hpp"

#ifdef ERROR_INSERT
Uint32 MAX_RECEIVED_SIGNALS = 1024;
#else
#define MAX_RECEIVED_SIGNALS 1024
#endif

void TransporterRegistry::dump_and_report_bad_message(
    const char file[], unsigned line, TransporterReceiveHandle &recvHandle,
    Uint32 *readPtr, size_t sizeInWords, NodeId remoteNodeId, TrpId trpId,
    IOState state, TransporterError errorCode) {
  report_error(trpId, errorCode);

  char msg[MAX_LOG_MESSAGE_SIZE];
  const size_t sz = sizeof(msg);
  Uint32 nextMsgOffset = Protocol6::getMessageLength(*readPtr);
  if (sizeInWords >= nextMsgOffset) {
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
    if (EventLoggerBase::event_lookup(NDB_LE_TransporterError, cat, threshold,
                                      severity, textF) != 0)
      goto log_it;
    Uint32 TE_words[3] = {0, remoteNodeId, (Uint32)errorCode};
    g_eventLogger->getText(msg + offs, sz - offs, textF, TE_words, 3);
    nb = strlen(msg + offs);
    if (nb < 0) goto log_it;
    offs += nb;

    const bool bad_data = recvHandle.m_bad_data_transporters.get(trpId);
    nb = BaseString::snprintf(msg + offs, sz - offs,
                              "\n"
                              "PerformState %u: IOState %u: bad_data %u\n"
                              "ptr %p: size %u bytes\n",
                              performStates[trpId], state, bad_data, readPtr,
                              (unsigned)(sizeInWords * 4));
    if (nb < 0) goto log_it;
    offs += nb;
    size_t reserve;
    if (!nextMsgOffset) {
      // If next message won't be dumped, print as much as possible
      // from start of buffer.
      reserve = 0;
    } else {
      // Keep some space for dumping next message, about 10 words
      // plus 6 preceding words.
      reserve = 200;
    }
    nb = BaseString::hexdump(msg + offs, sz - offs - reserve, readPtr,
                             sizeInWords);
    if (nb < 0) goto log_it;
    offs += nb;
    if (nextMsgOffset) {
      // Always print some words preceding next message. But assume
      // at least 60 words will be printed for current message.
      if (nextMsgOffset > 60) {
        nb = BaseString::snprintf(msg + offs, sz - offs, "Before next ptr %p\n",
                                  readPtr + nextMsgOffset - 6);
        if (nb < 0) goto log_it;
        offs += nb;
        nb = BaseString::hexdump(msg + offs, sz - offs,
                                 readPtr + nextMsgOffset - 6, 6);
        offs += nb;
      }
      // Dump words for next message.
      nb = BaseString::snprintf(msg + offs, sz - offs, "Next ptr %p\n",
                                readPtr + nextMsgOffset);
      if (nb < 0) goto log_it;
      offs += nb;
      nb = BaseString::hexdump(msg + offs, sz - offs, readPtr + nextMsgOffset,
                               sizeInWords - nextMsgOffset);
      if (nb < 0) goto log_it;
      offs += nb;
    }
  }

log_it:
  g_eventLogger->error("%s", msg);
  recvHandle.m_bad_data_transporters.set(trpId);
}

static inline bool unpack_one(Uint32 *(&readPtr), Uint32 *eodPtr,
                              Uint32 *endPtr, Uint8(&prio),
                              SignalHeader(&signalHeader),
                              Uint32 *(&signalData), LinearSectionPtr ptr[],
                              TransporterError(&errorCode)) {
  Uint32 word1 = readPtr[0];
  Uint32 word2 = readPtr[1];
  Uint32 word3 = readPtr[2];

  if (unlikely(!Protocol6::verifyByteOrder(word1, MY_OWN_BYTE_ORDER))) {
    errorCode = TE_UNSUPPORTED_BYTE_ORDER;
    return false;
  }

  if (unlikely(Protocol6::getCompressed(word1))) {
    errorCode = TE_COMPRESSED_UNSUPPORTED;
    return false;
  }

  const Uint16 messageLen32 = Protocol6::getMessageLength(word1);

  if (unlikely(messageLen32 == 0 ||
               messageLen32 > (MAX_RECV_MESSAGE_BYTESIZE >> 2))) {
    DEBUG("Message Size = " << messageLen32 << " words");
    errorCode = TE_INVALID_MESSAGE_LENGTH;
    return false;
  }  // if

  if (unlikely(eodPtr < readPtr + messageLen32)) {
    errorCode = TE_NO_ERROR;
    return false;
  }  // if

  if (unlikely(Protocol6::getCheckSumIncluded(word1))) {
    const Uint32 tmpLen = messageLen32 - 1;
    const Uint32 checkSumSent = readPtr[tmpLen];
    const Uint32 checkSumComputed = computeChecksum(&readPtr[0], tmpLen);

    if (unlikely(checkSumComputed != checkSumSent)) {
      errorCode = TE_INVALID_CHECKSUM;
      return false;
    }  // if
  }    // if

  signalData = &readPtr[3];

  readPtr += messageLen32;

  Protocol6::createSignalHeader(&signalHeader, word1, word2, word3);

  prio = Protocol6::getPrio(word1);

  if (Protocol6::getSignalIdIncluded(word1) == 0) {
    signalHeader.theSendersSignalId = ~0;
  } else {
    signalHeader.theSendersSignalId = *signalData;
    signalData++;
  }  // if
  signalHeader.theSignalId = ~0;

  Uint32 *sectionPtr = signalData + signalHeader.theLength;
  Uint32 *sectionData = sectionPtr + signalHeader.m_noOfSections;
  for (Uint32 i = 0; i < signalHeader.m_noOfSections; i++) {
    Uint32 sz = *sectionPtr;
    ptr[i].sz = sz;
    ptr[i].p = sectionData;

    sectionPtr++;
    sectionData += sz;
  }

  if (Protocol6::getCheckSumIncluded(word1)) {
    sectionData++;
  }
  if (sectionData != readPtr) {
    readPtr -= messageLen32;
    errorCode = TE_INVALID_MESSAGE_LENGTH;
    return false;
  }

  /* check if next message is possible before delivery */
  if (endPtr > readPtr) {  // check next message word1
    Uint32 word1 = *readPtr;
    // check byte order
    if (unlikely(!Protocol6::verifyByteOrder(word1, MY_OWN_BYTE_ORDER))) {
      errorCode = TE_UNSUPPORTED_BYTE_ORDER;
      return false;
    }
    if (unlikely(Protocol6::getCompressed(word1))) {
      errorCode = TE_COMPRESSED_UNSUPPORTED;
      return false;
    }
    // check message size
    const Uint16 messageLen32 = Protocol6::getMessageLength(word1);
    if (unlikely(messageLen32 == 0 ||
                 messageLen32 > (MAX_RECV_MESSAGE_BYTESIZE >> 2))) {
      DEBUG("Message Size = " << messageLen32);
      errorCode = TE_INVALID_MESSAGE_LENGTH;
      return false;
    }  // if
  }

  return true;
}

Uint32 TransporterRegistry::unpack(TransporterReceiveHandle &recvHandle,
                                   Uint32 *readPtr, Uint32 sizeOfData,
                                   NodeId remoteNodeId, TrpId trpId,
                                   bool &stopReceiving) {
  assert(stopReceiving == false);
  const IOState state = ioStates[trpId];

  // If bad data detected in previous run
  // skip all further data
  if (unlikely(recvHandle.m_bad_data_transporters.get(trpId))) {
    return sizeOfData;
  }

  Uint8 prio;
  SignalHeader signalHeader;
  Uint32 *signalData;
  LinearSectionPtr ptr[3];
  TransporterError errorCode = TE_NO_ERROR;

  Uint32 *const sodPtr = readPtr;
  Uint32 *const eodPtr = readPtr + (sizeOfData >> 2);
  Uint32 loop_count = 0;
  bool doStopReceiving = false;

  if (likely(!(state & HaltInput))) {
    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) && doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, eodPtr, prio, signalHeader, signalData,
                      ptr, errorCode)) {
      loop_count++;

      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;
      doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio,
                                                  errorCode, signalData, ptr);

    }  // while
  } else {
    /** state has 'HaltInput' */

    while ((eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) && doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, eodPtr, prio, signalHeader, signalData,
                      ptr, errorCode)) {
      loop_count++;

      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;

      if (rBlockNum == QMGR) {  // QMGR==252
        Uint32 sBlockNum = signalHeader.theSendersBlockRef;
        sBlockNum = numberToRef(sBlockNum, remoteNodeId);
        signalHeader.theSendersBlockRef = sBlockNum;

        doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio,
                                                    errorCode, signalData, ptr);
      } else {
        DEBUG("prepareReceive(...) - Discarding message to block: "
              << rBlockNum << " from Node: " << remoteNodeId);
      }  // if
    }    // while
  }      // if

  if (errorCode != TE_NO_ERROR) {
    dump_and_report_bad_message(__FILE__, __LINE__, recvHandle, readPtr,
                                eodPtr - readPtr, remoteNodeId, trpId, state,
                                errorCode);
    g_eventLogger->info("Loop count:%u", loop_count);
  }

  stopReceiving = doStopReceiving;
  return (readPtr - sodPtr) * sizeof(Uint32);
}

Uint32 *TransporterRegistry::unpack(TransporterReceiveHandle &recvHandle,
                                    Uint32 *readPtr, Uint32 *eodPtr,
                                    Uint32 *endPtr, NodeId remoteNodeId,
                                    TrpId trpId, bool &stopReceiving) {
  assert(stopReceiving == false);
  const IOState state = ioStates[trpId];

  // If bad data detected in previous run
  // skip all further data
  if (unlikely(recvHandle.m_bad_data_transporters.get(trpId))) {
    return eodPtr;
  }

  Uint8 prio;
  SignalHeader signalHeader;
  Uint32 *signalData;
  LinearSectionPtr ptr[3];
  TransporterError errorCode = TE_NO_ERROR;

  Uint32 loop_count = 0;
  bool doStopReceiving = false;

  /**
   * We will read past the endPtr, but never beyond the eodPtr. We will only
   * read one signal beyond the end and then we stop.
   */
  if (likely(!(state & HaltInput))) {
    while ((readPtr < endPtr) &&
           (eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) && doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, endPtr, prio, signalHeader, signalData,
                      ptr, errorCode)) {
      loop_count++;

      Uint32 sBlockNum = signalHeader.theSendersBlockRef;
      sBlockNum = numberToRef(sBlockNum, remoteNodeId);
      signalHeader.theSendersBlockRef = sBlockNum;

      doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio,
                                                  errorCode, signalData, ptr);

    }  // while
  } else {
    /** state has 'HaltInput' */

    while ((readPtr < endPtr) &&
           (eodPtr >= readPtr + (1 + (sizeof(Protocol6) >> 2))) &&
           (loop_count < MAX_RECEIVED_SIGNALS) && doStopReceiving == false &&
           unpack_one(readPtr, eodPtr, endPtr, prio, signalHeader, signalData,
                      ptr, errorCode)) {
      loop_count++;

      Uint32 rBlockNum = signalHeader.theReceiversBlockNumber;

      if (rBlockNum == QMGR) {  // QMGR==252
        Uint32 sBlockNum = signalHeader.theSendersBlockRef;
        sBlockNum = numberToRef(sBlockNum, remoteNodeId);
        signalHeader.theSendersBlockRef = sBlockNum;

        doStopReceiving = recvHandle.deliver_signal(&signalHeader, prio,
                                                    errorCode, signalData, ptr);
      } else {
        DEBUG("prepareReceive(...) - Discarding message to block: "
              << rBlockNum << " from Node: " << remoteNodeId);
      }  // if
    }    // while
  }      // if

  if (errorCode != TE_NO_ERROR) {
    dump_and_report_bad_message(__FILE__, __LINE__, recvHandle, readPtr,
                                eodPtr - readPtr, remoteNodeId, trpId, state,
                                errorCode);
  }

  stopReceiving = doStopReceiving;
  return readPtr;
}

Packer::Packer(bool signalId, bool checksum)
    : preComputedWord1(0),
      checksumUsed(checksum ? 1 : 0),
      signalIdUsed(signalId ? 1 : 0) {
  Protocol6::setByteOrder(preComputedWord1, MY_OWN_BYTE_ORDER);
  Protocol6::setSignalIdIncluded(preComputedWord1, signalIdUsed);
  Protocol6::setCheckSumIncluded(preComputedWord1, checksumUsed);
  Protocol6::setCompressed(preComputedWord1, 0);
}

static inline void import(Uint32 *&insertPtr, const LinearSectionPtr &ptr) {
  const Uint32 sz = ptr.sz;
  memcpy(insertPtr, ptr.p, 4 * sz);
  insertPtr += sz;
}

static inline void importGeneric(Uint32 *&insertPtr,
                                 const GenericSectionPtr &ptr) {
  /* Use the section iterator to obtain the words in this section */
  Uint32 remain = ptr.sz;

  while (remain > 0) {
    Uint32 len = 0;
    const Uint32 *next = ptr.sectionIter->getNextWords(len);

    assert(len <= remain);
    assert(next != nullptr);

    memcpy(insertPtr, next, 4 * len);
    insertPtr += len;
    remain -= len;
  }

  /* Check that there were no more words available from the
   * Signal iterator
   */
  assert(ptr.sectionIter->getNextWords(remain) == nullptr);
}

void copy(Uint32 *&insertPtr, class SectionSegmentPool &,
          const SegmentedSectionPtr &ptr);

/**
 * Define the different variants of import of 'AnySectionArg'
 * which the Packer::pack() template may be called with.
 */
static inline void import(Uint32 *&insertPtr, Uint32 no_segs,
                          Packer::LinearSectionArg section) {
  Uint32 *szPtr = insertPtr;
  insertPtr += no_segs;

  for (Uint32 i = 0; i < no_segs; i++) {
    szPtr[i] = section.m_ptr[i].sz;
    import(insertPtr, section.m_ptr[i]);
  }
}

static inline void import(Uint32 *&insertPtr, Uint32 no_segs,
                          Packer::GenericSectionArg section) {
  Uint32 *szPtr = insertPtr;
  insertPtr += no_segs;

  for (Uint32 i = 0; i < no_segs; i++) {
    szPtr[i] = section.m_ptr[i].sz;
    importGeneric(insertPtr, section.m_ptr[i]);
  }
}

static inline void import(Uint32 *&insertPtr, Uint32 no_segs,
                          Packer::SegmentedSectionArg section) {
  Uint32 *szPtr = insertPtr;
  insertPtr += no_segs;

  for (Uint32 i = 0; i < no_segs; i++) {
    szPtr[i] = section.m_ptr[i].sz;
    copy(insertPtr, section.m_pool, section.m_ptr[i]);
  }
}

/////////////////

template <typename AnySectionArg>
inline void Packer::pack(Uint32 *insertPtr, Uint32 prio,
                         const SignalHeader *header, const Uint32 *theData,
                         AnySectionArg section) const {
  Uint32 i;

  const Uint32 dataLen32 = header->theLength;
  const Uint32 no_segs = header->m_noOfSections;

  Uint32 len32 = dataLen32 + no_segs + checksumUsed + signalIdUsed +
                 (sizeof(Protocol6) / 4);

  for (i = 0; i < no_segs; i++) {
    len32 += section.m_ptr[i].sz;
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

  Uint32 *tmpInsertPtr = &insertPtr[3];

  if (signalIdUsed) {
    *tmpInsertPtr = header->theSignalId;
    tmpInsertPtr++;
  }

  memcpy(tmpInsertPtr, theData, 4 * dataLen32);
  tmpInsertPtr += dataLen32;

  import(tmpInsertPtr, no_segs, section);

  /**
   * 'unlikely checksum' as we want a fast path for the default
   * of checksum not being used. If enabling checksum we are prepared
   * to pay the cost of the extra overhead.
   */
  if (unlikely(checksumUsed)) {
    *tmpInsertPtr = computeChecksum(&insertPtr[0], len32 - 1);
  }
}

// Instantiate the templated ::pack() variants
template void Packer::pack<Packer::LinearSectionArg>(
    Uint32 *insertPtr, Uint32 prio, const SignalHeader *header,
    const Uint32 *theData, LinearSectionArg section) const;

template void Packer::pack<Packer::GenericSectionArg>(
    Uint32 *insertPtr, Uint32 prio, const SignalHeader *header,
    const Uint32 *theData, GenericSectionArg section) const;

template void Packer::pack<Packer::SegmentedSectionArg>(
    Uint32 *insertPtr, Uint32 prio, const SignalHeader *header,
    const Uint32 *theData, SegmentedSectionArg section) const;

/**
 * Find longest data size that does not exceed given maximum, and does not
 * cause individual signals to be split.
 *
 * Used by SHM_Transporter, as it is designed to send data in Signal chunks,
 * not bytes or words.
 */
Uint32 TransporterRegistry::unpack_length_words(const Uint32 *readPtr,
                                                Uint32 maxWords,
                                                bool extra_signal) {
  Uint32 wordLength = 0;

  /**
   * We come here in a number of situations:
   * 1) extra_signal true, in this case maxWords refers to the boundary we
   *    are allowed to pass with last signal. So here we want to return
   *    with at least maxWords, never less.
   *
   * 2) extra_signal false AND maxWords == all data in segment.
   *    In this case we always expect to return maxWords.
   *
   * 3) extra_signal false AND maxWords == remaining buffer space.
   *    In this case we will return up to maxWords, never more,
   *    and sometimes less.
   *
   * We have no information about whether we are 2) or 3) here.
   */
  while (wordLength < maxWords) {
    Uint32 word1 = readPtr[wordLength];
    Uint16 messageLen32 = Protocol6::getMessageLength(word1);
    if (wordLength + messageLen32 > maxWords) {
      if (extra_signal) {
        wordLength += messageLen32;
      }
      break;
    }
    wordLength += messageLen32;
  }
  assert(((wordLength < maxWords) && !extra_signal) ||
         ((wordLength > maxWords) && extra_signal) || (wordLength == maxWords));
  return wordLength;
}
