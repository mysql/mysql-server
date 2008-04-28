/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

#include "SignalLoggerManager.hpp"
#include <LongSignal.hpp>

#include <DebuggerNames.hpp>

SignalLoggerManager::SignalLoggerManager()
{
  for (int i = 0; i < NO_OF_BLOCKS; i++){
      logModes[i] = 0;
  }
  outputStream = 0;
  m_ownNodeId = 0;
  m_logDistributed = false;
}

SignalLoggerManager::~SignalLoggerManager()
{
  if(outputStream != 0){
    fflush(outputStream);
    fclose(outputStream);
    outputStream = 0;
  }
}

FILE *
SignalLoggerManager::setOutputStream(FILE * output)
{
  if(outputStream != 0){
    fflush(outputStream);
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
  if(outputStream != 0)
    fflush(outputStream);
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

int
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
    int len = strcspn(tmp, ", ;:\0");
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
  if((count == 1 && !strcmp(blocks[0], "ALL")) ||
     count == 0){
    
    for (int number = 0; number < NO_OF_BLOCKS; ++number){
      cnt += log(SLM_ON, number, logMode);
    }
  } else {
    for (int i = 0; i < count; ++i){
      BlockNumber number = getBlockNo(blocks[i]);
      cnt += log(SLM_ON, number, logMode);
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
  // Normalise blocknumber for use in logModes array
  const BlockNumber bno2 = bno-MIN_BLOCK_NO;
  assert(bno2<NO_OF_BLOCKS);
  switch(cmd){
  case SLM_ON:
    logModes[bno2] |= logMode;
    return 1;
    break;
  case SLM_OFF:
    logModes[bno2] &= (~logMode);
    return 1;
    break;
  case SLM_TOGGLE:
    logModes[bno2] ^= logMode;
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
    const char* inOutStr = prio == 0 ? "In" : "Out";
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Direct --- Signal --- %s - %d ----\n", inOutStr, time(0));
#else
    fprintf(outputStream, "---- Direct --- Signal --- %s ----------------\n", inOutStr);
#endif
    // XXX pass in/out to print* function somehow
    printSignalHeader(outputStream, sh, 0, node, true);
    printSignalData(outputStream, sh, theData);
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
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Received - Signal - %d ----\n", time(0));
#else
    fprintf(outputStream, "---- Received - Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, true);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
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
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Received - Signal - %d ----\n", time(0));
#else
    fprintf(outputStream, "---- Received - Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, true);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printLinearSection(outputStream, sh, ptr, i);
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
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Send ----- Signal - %d ----\n", time(0));
#else
    fprintf(outputStream, "---- Send ----- Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, false);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printLinearSection(outputStream, sh, ptr, i);
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
#ifdef VM_TRACE_TIME
    fprintf(outputStream, "---- Send ----- Signal - %d ----\n", time(0));
#else
    fprintf(outputStream, "---- Send ----- Signal ----------------\n");
#endif

    printSignalHeader(outputStream, sh, prio, node, false);
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
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
#ifdef VM_TRACE_TIME
    fprintf(outputStream, 
	    "---- Send ----- Signal (%d ms) %d\n", 
	    delayInMilliSeconds, 
	    time(0));
#else
    fprintf(outputStream, "---- Send delay Signal (%d ms) ----------\n", 
	    delayInMilliSeconds);
#endif

    printSignalHeader(outputStream, sh, prio, node, false);    
    printSignalData(outputStream, sh, theData);
    for (unsigned i = 0; i < secs; i++)
      printSegmentedSection(outputStream, sh, ptr, i);
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
    va_list ap;
    va_start(ap, msg);
    fprintf(outputStream, "%s: ", getBlockName(bno, "API"));
    vfprintf(outputStream, msg, ap);
    fprintf(outputStream, "\n");
    va_end(ap);
  }
}


void 
SignalLoggerManager::printSignalHeader(FILE * output, 
				       const SignalHeader & sh,
				       Uint8 prio, 
				       Uint32 node,
				       bool printReceiversSignalId)
{
  Uint32 receiverBlockNo = sh.theReceiversBlockNumber;
  Uint32 receiverProcessor = node;
  Uint32 gsn = sh.theVerId_signalNumber;
  Uint32 senderBlockNo = refToBlock(sh.theSendersBlockRef);
  Uint32 senderProcessor = refToNode(sh.theSendersBlockRef);
  Uint32 length = sh.theLength;
  Uint32 trace = sh.theTrace;
  Uint32 rSigId = sh.theSignalId;
  Uint32 sSigId = sh.theSendersSignalId;

  const char * signalName = getSignalName(gsn);
  const char * rBlockName = getBlockName(receiverBlockNo, "API");
  const char * sBlockName = getBlockName(senderBlockNo, "API");
  
  if(printReceiversSignalId)
    fprintf(output, 
	    "r.bn: %d \"%s\", r.proc: %d, r.sigId: %d gsn: %d \"%s\" prio: %d\n"
	    ,receiverBlockNo, rBlockName, receiverProcessor, rSigId, 
	    gsn, signalName, prio);
  else 
    fprintf(output,
	    "r.bn: %d \"%s\", r.proc: %d, gsn: %d \"%s\" prio: %d\n",
	    receiverBlockNo, rBlockName, receiverProcessor, gsn, 
	    signalName, prio);
  
  fprintf(output, 
	  "s.bn: %d \"%s\", s.proc: %d, s.sigId: %d length: %d trace: %d "
	  "#sec: %d fragInf: %d\n",
	  senderBlockNo, sBlockName, senderProcessor, sSigId, length, trace,
	  sh.m_noOfSections, sh.m_fragmentInfo);
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
