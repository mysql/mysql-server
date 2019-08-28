/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include "SignalLoggerManager.hpp"
#include "TransporterDefinitions.hpp"
#include <GlobalSignalNumbers.h>
#include <DebuggerNames.hpp>
#include <NdbTick.h>
#include <NdbEnv.h>

#ifdef VM_TRACE_TIME
static char* mytime()
{
  Uint64 t = NdbTick_CurrentMillisecond();
  uint s = (t / 1000) % 3600;
  uint ms = t % 1000;
  static char buf[100];
  sprintf(buf, "%u.%03u", s, ms);
  return buf;
}
#endif
SignalLoggerManager::SignalLoggerManager()
{
  for (int i = 0; i < NO_OF_BLOCKS; i++){
      logModes[i] = 0;
  }
  outputStream = 0;
  m_ownNodeId = 0;
  m_logDistributed = false;

  // using mutex avoids MT log mixups but has some serializing effect
  m_mutex = 0;

#ifdef NDB_USE_GET_ENV
  const char* p = NdbEnv_GetEnv("NDB_SIGNAL_LOG_MUTEX", (char*)0, 0);
  if (p != 0 && strchr("1Y", p[0]) != 0)
    m_mutex = NdbMutex_Create();
#endif
}

SignalLoggerManager::~SignalLoggerManager()
{
  if(outputStream != 0){
    fflush(outputStream);
    fclose(outputStream);
    outputStream = 0;
  }
  if (m_mutex != 0) {
    NdbMutex_Destroy(m_mutex);
    m_mutex = 0;
  }
}

FILE *
SignalLoggerManager::setOutputStream(FILE * output)
{
  if (outputStream != 0)
  {
    lock();
    fflush(outputStream);
    unlock();
  }

  FILE * out = outputStream;
  outputStream = output;
  return out;
}

FILE *
SignalLoggerManager::getOutputStream() const
{
  return outputStream;
}

void
SignalLoggerManager::flushSignalLog()
{
  if (outputStream != 0)
  {
    lock();
    fflush(outputStream);
    unlock();
  }
}

void
SignalLoggerManager::setTrace(unsigned long trace)
{
  traceId = trace;
}

unsigned long
SignalLoggerManager::getTrace() const
{
  return traceId;
}

void
SignalLoggerManager::setOwnNodeId(int nodeId){
  m_ownNodeId = nodeId;
}
  
void
SignalLoggerManager::setLogDistributed(bool val){
  m_logDistributed = val;
}

static int
getParameter(char *blocks[NO_OF_BLOCKS], const char * par, const char * line)
{
  const char * loc = strstr(line, par);
  if(loc == NULL)
    return 0;

  loc += strlen(par);

  int found = 0;

  char * copy = strdup(loc);
  char * tmp = copy;
  bool done = false;
  while(!done){
    int len = (int)strcspn(tmp, ", ;:\0");
    if(len == 0)
      done = true;
    else {
      if(* (tmp + len) != ',')
	done = true;
      * (tmp + len) = 0;
      blocks[found] = strdup(tmp);
      found ++;
      tmp += (len + 1);
    } 
  }
  free(copy);
  return found;
}


#define SLM_OFF    0
#define SLM_ON     1
#define SLM_TOGGLE 2

int
SignalLoggerManager::log(LogMode logMode, const char * params)
{
  char * blocks[NO_OF_BLOCKS];
  const int count = getParameter(blocks, "BLOCK=", params);
  
  int cnt = 0;
  if(count == 0 ||
     (count == 1 && !strcmp(blocks[0], "ALL")))
  {
    // Inform all blocks about the new log mode
    for (int number = 0; number < NO_OF_BLOCKS; ++number)
      cnt += log(SLM_ON, MIN_BLOCK_NO + number, logMode);
  }
  else
  {
    // Inform only specified blocks about the new log mode
    for (int i = 0; i < count; ++i)
    {
      BlockNumber bno = getBlockNo(blocks[i]);
      if (bno == 0)
      {
        // Could not find any block with matching name
        ndbout_c("Could not turn on signal logging for unknown block '%s'",
                 blocks[i]);
        continue;
      }
      cnt += log(SLM_ON, bno, logMode);
    }
  }
  for(int i = 0; i<count; i++){
    free(blocks[i]);
  }

  return cnt;
}

int
SignalLoggerManager::log(int cmd, BlockNumber bno, LogMode logMode)
{
  // Make sure bno is valid range
  assert(bno >= MIN_BLOCK_NO && bno <= MAX_BLOCK_NO);

  // Convert bno to index into logModes
  const size_t index = bno-MIN_BLOCK_NO;
  assert(index < NDB_ARRAY_SIZE(logModes));

  switch(cmd){
  case SLM_ON:
    logModes[index] |= logMode;
    return 1;
    break;
  case SLM_OFF:
    logModes[index] &= (~logMode);
    return 1;
    break;
  case SLM_TOGGLE:
    logModes[index] ^= logMode;
    return 1;
    break;
  }
  return 0;
}

int
SignalLoggerManager::logOn(bool allBlocks, BlockNumber bno, LogMode logMode)
{
  if(!allBlocks){
    return log(SLM_ON, bno, logMode);
  } 
  int cnt = 0;
  for(unsigned int i = MIN_BLOCK_NO; i <= MAX_BLOCK_NO; i++)
    cnt += log(SLM_ON, i, logMode);
  return cnt;
}

int
SignalLoggerManager::logOff(bool allBlocks, BlockNumber bno, LogMode logMode)
{
  if(!allBlocks){
    return log(SLM_OFF, bno, logMode);
  } 
  int cnt = 0;
  for(unsigned int i = MIN_BLOCK_NO; i <= MAX_BLOCK_NO; i++)
    cnt += log(SLM_OFF, i, logMode);
  return cnt;

}

int
SignalLoggerManager::logToggle(bool allBlocks, BlockNumber bno, LogMode logMode)
{
  if(!allBlocks){
    return log(SLM_TOGGLE, bno, logMode);
  } 
  int cnt = 0;
  for(unsigned int i = MIN_BLOCK_NO; i <= MAX_BLOCK_NO; i++)
    cnt += log(SLM_TOGGLE, i, logMode);
  return cnt;
}

void
SignalLoggerManager::executeDirect(const SignalHeader& sh, 
				   Uint8 prio,  // in-out flag
				   const Uint32 * theData, Uint32 node)
{
  Uint32 trace = sh.theTrace;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  Uint32 receiverBlockNo = sh.theReceiversBlockNumber;
  
  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(senderBlockNo, LogOut) || logMatch(receiverBlockNo, LogIn))){
    lock();
    const char* inOutStr = prio == 0 ? "In" : "Out";
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Direct --- Signal --- %s - %s ----\n", inOutStr, mytime());
#else
    fprintf(outputStream, "---- Direct --- Signal --- %s ----------------\n", inOutStr);
#endif
    // XXX pass in/out to print* function somehow
    printSignalHeader(outputStream, sh, 0, node, true);
    printSignalData(outputStream, sh, theData);
    unlock();
  }
}

/**
 * For input signals
 */
void
SignalLoggerManager::executeSignal(const SignalHeader& sh, Uint8 prio, 
				   const Uint32 * theData, Uint32 node,
                                   const SegmentedSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  //Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  Uint32 receiverBlockNo = sh.theReceiversBlockNumber;
  Uint32 senderNode = refToNode(sh.theSendersBlockRef);

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(receiverBlockNo, LogOut) ||
      (m_logDistributed && m_ownNodeId != senderNode))){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Received - Signal - %s ----\n", mytime());
#else
    fprintf(outputStream, "---- Received - Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, true);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
    unlock();
  }
}

void
SignalLoggerManager::executeSignal(const SignalHeader& sh, Uint8 prio, 
				   const Uint32 * theData, Uint32 node,
                                   const LinearSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  //Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  Uint32 receiverBlockNo = sh.theReceiversBlockNumber;
  Uint32 senderNode = refToNode(sh.theSendersBlockRef);

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(receiverBlockNo, LogOut) ||
      (m_logDistributed && m_ownNodeId != senderNode))){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Received - Signal - %s ----\n", mytime());
#else
    fprintf(outputStream, "---- Received - Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, true);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printLinearSection(outputStream, sh, ptr, i);
    unlock();
  }
}

/**
 * For output signals
 */
void
SignalLoggerManager::sendSignal(const SignalHeader& sh,
                                Uint8 prio,
				const Uint32 * theData, Uint32 node,
                                const LinearSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  //Uint32 receiverBlockNo = sh.theReceiversBlockNumber;

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(senderBlockNo, LogOut) ||
      (m_logDistributed && m_ownNodeId != node))){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Send ----- Signal - %s ----\n", mytime());
#else
    fprintf(outputStream, "---- Send ----- Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, false);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printLinearSection(outputStream, sh, ptr, i);
    unlock();
  }
}

/**
 * For output signals
 */
void
SignalLoggerManager::sendSignal(const SignalHeader& sh, Uint8 prio, 
				const Uint32 * theData, Uint32 node,
                                const SegmentedSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  //Uint32 receiverBlockNo = sh.theReceiversBlockNumber;

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(senderBlockNo, LogOut) ||
      (m_logDistributed && m_ownNodeId != node))){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Send ----- Signal - %s ----\n", mytime());
#else
    fprintf(outputStream, "---- Send ----- Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, false);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
    unlock();
  }
}

void
SignalLoggerManager::sendSignal(const SignalHeader& sh,
                                Uint8 prio,
				const Uint32 * theData, Uint32 node,
                                const GenericSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  //Uint32 receiverBlockNo = sh.theReceiversBlockNumber;

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     (logMatch(senderBlockNo, LogOut) ||
      (m_logDistributed && m_ownNodeId != node))){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Send ----- Signal - %s ----\n", mytime());
#else
    fprintf(outputStream, "---- Send ----- Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, false);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printGenericSection(outputStream, sh, ptr, i);
    unlock();
  }
}

void
SignalLoggerManager::sendSignalWithDelay(Uint32 delayInMilliSeconds,
					 const SignalHeader & sh, Uint8 prio, 
					 const Uint32 * theData, Uint32 node,
                                         const SegmentedSectionPtr ptr[3], Uint32 secs)
{
  Uint32 trace = sh.theTrace;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  //Uint32 receiverBlockNo = sh.theReceiversBlockNumber;

  if(outputStream != 0 && 
     (traceId == 0 || traceId == trace) &&
     logMatch(senderBlockNo, LogOut)){
    lock();
#ifdef VM_TRACE_TIME
    fprintf(outputStream, 
	    "---- Send ----- Signal (%d ms) %s\n", 
	    delayInMilliSeconds, 
	    mytime());
#else
    fprintf(outputStream, "---- Send delay Signal (%d ms) ----------\n", 
	    delayInMilliSeconds);
#endif

    printSignalHeader(outputStream, sh, prio, node, false);    
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
    unlock();
  }
}

/**
 * Generic messages in the signal log
 */
void
SignalLoggerManager::log(BlockNumber bno, const char * msg, ...)
{
  // Normalise blocknumber for use in logModes array
  const BlockNumber bno2 = bno - MIN_BLOCK_NO;
  assert(bno2<NO_OF_BLOCKS);

  if(outputStream != 0 &&
     logModes[bno2] != LogOff){
    lock();
    va_list ap;
    va_start(ap, msg);
    fprintf(outputStream, "%s: ", getBlockName(bno, "API"));
    vfprintf(outputStream, msg, ap);
    fprintf(outputStream, "\n");
    va_end(ap);
    unlock();
  }
}

static inline bool
isSysBlock(BlockNumber block, Uint32 gsn)
{
  if (block != 0)
    return false;
  switch (gsn) {
  case GSN_START_ORD:
    return true; // first sig
  case GSN_CONNECT_REP:
  case GSN_DISCONNECT_REP:
  case GSN_EVENT_REP:
    return true; // transporter
  case GSN_STOP_FOR_CRASH:
    return true; // mt scheduler
  }
  return false;
}

static inline bool
isApiBlock(BlockNumber block)
{
  return block >= 0x8000 || block == 4002 || block == 2047;
}

void 
SignalLoggerManager::printSignalHeader(FILE * output, 
				       const SignalHeader & sh,
				       Uint8 prio, 
				       Uint32 node,
				       bool printReceiversSignalId)
{
  const char* const dummy_block_name = "UUNET";

  bool receiverIsApi = isApiBlock(sh.theReceiversBlockNumber);
  Uint32 receiverBlockNo;
  Uint32 receiverInstanceNo;
  if (!receiverIsApi) {
    receiverBlockNo = blockToMain(sh.theReceiversBlockNumber);
    receiverInstanceNo = blockToInstance(sh.theReceiversBlockNumber);
  } else {
    receiverBlockNo = sh.theReceiversBlockNumber;
    receiverInstanceNo = 0;
  }
  Uint32 receiverProcessor = node;

  Uint32 gsn = sh.theVerId_signalNumber;

  Uint32 sbref = sh.theSendersBlockRef;
  bool senderIsSys = isSysBlock(refToBlock(sbref), gsn);
  bool senderIsApi = isApiBlock(refToBlock(sbref));
  Uint32 senderBlockNo;
  Uint32 senderInstanceNo;
  if (!senderIsSys && !senderIsApi) {
    senderBlockNo = refToMain(sbref);
    senderInstanceNo = refToInstance(sbref);
  } else {
    senderBlockNo = refToBlock(sbref);
    senderInstanceNo = 0;
  }
  Uint32 senderProcessor = refToNode(sbref);

  Uint32 length = sh.theLength;
  Uint32 trace = sh.theTrace;
  Uint32 rSigId = sh.theSignalId;
  Uint32 sSigId = sh.theSendersSignalId;

  const char * signalName = getSignalName(gsn);
  const char * rBlockName =
    receiverIsApi ? "API" :
    getBlockName(receiverBlockNo, dummy_block_name);
  const char * sBlockName =
    senderIsSys ? "SYS" :
    senderIsApi ? "API" :
    getBlockName(senderBlockNo, dummy_block_name);

  char rInstanceText[20];
  char sInstanceText[20];
  rInstanceText[0] = 0;
  sInstanceText[0] = 0;
  if (receiverInstanceNo != 0)
    sprintf(rInstanceText, "/%u", (uint)receiverInstanceNo);
  if (senderInstanceNo != 0)
    sprintf(sInstanceText, "/%u", (uint)senderInstanceNo);

  if (printReceiversSignalId)
    fprintf(output, 
	    "r.bn: %d%s \"%s\", r.proc: %d, r.sigId: %d gsn: %d \"%s\" prio: %d\n"
	    ,receiverBlockNo, rInstanceText, rBlockName, receiverProcessor,
            rSigId, gsn, signalName, prio);
  else 
    fprintf(output,
	    "r.bn: %d%s \"%s\", r.proc: %d, gsn: %d \"%s\" prio: %d\n",
	    receiverBlockNo, rInstanceText, rBlockName, receiverProcessor,
            gsn, signalName, prio);
  
  fprintf(output, 
	  "s.bn: %d%s \"%s\", s.proc: %d, s.sigId: %d length: %d trace: %d "
	  "#sec: %d fragInf: %d\n",
	  senderBlockNo, sInstanceText, sBlockName, senderProcessor,
          sSigId, length, trace, sh.m_noOfSections, sh.m_fragmentInfo);

  //assert(strcmp(rBlockName, dummy_block_name) != 0);
  //assert(strcmp(sBlockName, dummy_block_name) != 0);
}

void
SignalLoggerManager::printSignalData(FILE * output, 
				     const SignalHeader & sh,
				     const Uint32 * signalData)
{
  Uint32 len = sh.theLength;
  SignalDataPrintFunction printFunction = 
    findPrintFunction(sh.theVerId_signalNumber);
  
  bool ok = false;      // done with printing
  if(printFunction != 0){
    ok = (* printFunction)(output, signalData, len, sh.theReceiversBlockNumber);
  }
  if(!ok){
    while(len >= 7){
      fprintf(output, 
              " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
              signalData[0], signalData[1], signalData[2], signalData[3], 
              signalData[4], signalData[5], signalData[6]);
      len -= 7;
      signalData += 7;
    }
    if(len > 0){
      for(Uint32 i = 0; i<len; i++)
        fprintf(output, " H\'%.8x", signalData[i]);
      fprintf(output, "\n");
    }
  }
}

void
SignalLoggerManager::printLinearSection(FILE * output,
                                        const SignalHeader & sh,
                                        const LinearSectionPtr ptr[3],
                                        unsigned i)
{
  fprintf(output, "SECTION %u type=linear", i);
  if (i >= 3) {
    fprintf(output, " *** invalid ***\n");
    return;
  }
  const Uint32 len = ptr[i].sz;
  const Uint32 * data = ptr[i].p;
  Uint32 pos = 0;
  fprintf(output, " size=%u\n", (unsigned)len);
  while (pos < len) {
    printDataWord(output, pos, data[pos]);
  }
  if (len > 0)
    putc('\n', output);
}

void
SignalLoggerManager::printGenericSection(FILE * output,
                                         const SignalHeader & sh,
                                         const GenericSectionPtr ptr[3],
                                         unsigned i)
{
  fprintf(output, "SECTION %u type=generic", i);
  if (i >= 3) {
    fprintf(output, " *** invalid ***\n");
    return;
  }
  const Uint32 len = ptr[i].sz;
  Uint32 pos = 0;
  Uint32 chunksz = 0;
  fprintf(output, " size=%u\n", (unsigned)len);
  while (pos < len) {
    const Uint32* data= ptr[i].sectionIter->getNextWords(chunksz);
    Uint32 i=0;
    while (i < chunksz)
      printDataWord(output, pos, data[i++]);
  }
  if (len > 0)
    putc('\n', output);
}


void
SignalLoggerManager::printDataWord(FILE * output, Uint32 & pos, const Uint32 data)
{
  const char* const hex = "0123456789abcdef";
  if (pos > 0 && pos % 7 == 0)
    putc('\n', output);
  putc(' ', output);
  putc('H', output);
  putc('\'', output);
  for (int i = 7; i >= 0; i--)
    putc(hex[(data >> (i << 2)) & 0xf], output);
  pos++;
}
