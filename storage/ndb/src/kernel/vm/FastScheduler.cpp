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

#include "FastScheduler.hpp"
#include "ThreadConfig.hpp"
#include "RefConvert.hpp"

#include "Emulator.hpp"
#include "VMSignal.hpp"

#include <SignalLoggerManager.hpp>
#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <signaldata/EventReport.hpp>
#include "LongSignal.hpp"
#include <NdbTick.h>

#define JAM_FILE_ID 242


#define MIN_NUMBER_OF_SIG_PER_DO_JOB 64
#define MAX_NUMBER_OF_SIG_PER_DO_JOB 2048
#define EXTRA_SIGNALS_PER_DO_JOB 32

FastScheduler::FastScheduler()
{
   // These constants work for sun only, but they should be initated from
   // Emulator.C as soon as VMTime has been initiated.
   theJobBuffers[0].newBuffer(JBASIZE);
   theJobBuffers[1].newBuffer(JBBSIZE);
   theJobBuffers[2].newBuffer(JBCSIZE);
   theJobBuffers[3].newBuffer(JBDSIZE);
   clear();
}

FastScheduler::~FastScheduler()
{
}

void 
FastScheduler::clear()
{
  int i;
  // Make sure the restart signals are not sent too early
  // the prio is set back in 'main' using the 'ready' method.
  globalData.highestAvailablePrio = LEVEL_IDLE;
  globalData.sendPackedActivated = 0;
  globalData.activateSendPacked = 0;
  for (i = 0; i < JB_LEVELS; i++){
    theJobBuffers[i].clear();
  }
  globalData.JobCounter = 0;
  globalData.JobLap = 0;
  globalData.loopMax = 32;
  globalData.VMSignals[0].header.theSignalId = 0;
  
  theDoJobTotalCounter = 0;
  theDoJobCallCounter = 0;
}

void
FastScheduler::activateSendPacked()
{
  globalData.sendPackedActivated = 1;
  globalData.activateSendPacked = 0;
  globalData.loopMax = 2048;
}//FastScheduler::activateSendPacked()

//------------------------------------------------------------------------
// sendPacked is executed at the end of the loop.
// To ensure that we don't send any messages before executing all local
// packed signals we do another turn in the loop (unless we have already
// executed too many signals in the loop).
//------------------------------------------------------------------------
Uint32
FastScheduler::doJob(Uint32 loopStartCount)
{
  Uint32 loopCount = 0;
  Uint32 TminLoops = getBOccupancy() + EXTRA_SIGNALS_PER_DO_JOB;
  Uint32 TloopMax = (Uint32)globalData.loopMax;
  if (TminLoops < TloopMax) {
    TloopMax = TminLoops;
  }//if
  if (TloopMax < MIN_NUMBER_OF_SIG_PER_DO_JOB) {
    TloopMax = MIN_NUMBER_OF_SIG_PER_DO_JOB;
  }//if
  register Signal* signal = getVMSignals();
  register Uint32 tHighPrio= globalData.highestAvailablePrio;
  do{
    while ((tHighPrio < LEVEL_IDLE) && (loopCount < TloopMax)) {
#ifdef VM_TRACE
      /* Find reading / propagation of junk */
      signal->garbage_register();
#endif 
      if (unlikely(loopStartCount >
          MAX_SIGNALS_EXECUTED_BEFORE_ZERO_TIME_QUEUE_SCAN))
      {
        /**
         * This implements the bounded delay signal concept. This
         * means that we will never execute more than 160 signals
         * before getting the signals with delay 0 put into the
         * A-level job buffer.
         */
        loopStartCount = 0;
        globalEmulatorData.theThreadConfig->scanZeroTimeQueue();
      }
      // To ensure we find bugs quickly
      register Uint32 gsnbnr = theJobBuffers[tHighPrio].retrieve(signal);
      // also strip any instance bits since this is non-MT code
      register BlockNumber reg_bnr = gsnbnr & NDBMT_BLOCK_MASK;
      register GlobalSignalNumber reg_gsn = gsnbnr >> 16;
      globalData.incrementWatchDogCounter(1);
      if (reg_bnr > 0) {
        Uint32 tJobCounter = globalData.JobCounter;
        Uint64 tJobLap = globalData.JobLap;
        SimulatedBlock* b = globalData.getBlock(reg_bnr);
        theJobPriority[tJobCounter] = (Uint8)tHighPrio;
        globalData.JobCounter = (tJobCounter + 1) & 4095;
        globalData.JobLap = tJobLap + 1;
	
#ifdef VM_TRACE_TIME
	const NDB_TICKS t1 = NdbTick_getCurrentTicks();
	b->m_currentGsn = reg_gsn;
#endif
	
#ifdef VM_TRACE
        {
          if (globalData.testOn) {
	    signal->header.theVerId_signalNumber = reg_gsn;
	    signal->header.theReceiversBlockNumber = reg_bnr;
	    
            globalSignalLoggers.executeSignal(signal->header,
					      tHighPrio,
					      &signal->theData[0], 
					      globalData.ownId);
          }//if
        }
#endif
        b->jamBuffer()->markEndOfSigExec();
        b->executeFunction_async(reg_gsn, signal);
#ifdef VM_TRACE_TIME
	const NDB_TICKS t2 = NdbTick_getCurrentTicks();
        const Uint64 diff = NdbTick_Elapsed(t1,t2).microSec();
	b->addTime(reg_gsn, diff);
#endif
        tHighPrio = globalData.highestAvailablePrio;
      } else {
        tHighPrio++;
        globalData.highestAvailablePrio = tHighPrio;
      }//if
      loopCount++;
      loopStartCount++;
    }//while
    sendPacked();
    tHighPrio = globalData.highestAvailablePrio;
    if(getBOccupancy() > MAX_OCCUPANCY)
    {
      if(loopCount != TloopMax)
	abort();
      assert( loopCount == TloopMax );
      TloopMax += 512;
    }
  } while ((getBOccupancy() > MAX_OCCUPANCY) ||
           ((loopCount < TloopMax) &&
            (tHighPrio < LEVEL_IDLE)));

  theDoJobCallCounter ++;
  theDoJobTotalCounter += loopCount;
  if (theDoJobCallCounter == 8192) {
    reportDoJobStatistics(theDoJobTotalCounter >> 13);
    theDoJobCallCounter = 0;
    theDoJobTotalCounter = 0;
  }//if
  return loopStartCount;
}//FastScheduler::doJob()

void
FastScheduler::postPoll()
{
  Signal * signal = getVMSignals();
  SimulatedBlock* b_fs = globalData.getBlock(NDBFS);
  b_fs->executeFunction_async(GSN_SEND_PACKED, signal);
}

void FastScheduler::sendPacked()
{
  if (globalData.sendPackedActivated == 1) {
    SimulatedBlock* b_lqh = globalData.getBlock(DBLQH);
    SimulatedBlock* b_tc = globalData.getBlock(DBTC);
    SimulatedBlock* b_tup = globalData.getBlock(DBTUP);
    SimulatedBlock* b_fs = globalData.getBlock(NDBFS);
    Signal * signal = getVMSignals();
    b_lqh->executeFunction_async(GSN_SEND_PACKED, signal);
    b_tc->executeFunction_async(GSN_SEND_PACKED, signal);
    b_tup->executeFunction_async(GSN_SEND_PACKED, signal);
    b_fs->executeFunction_async(GSN_SEND_PACKED, signal);
    return;
  } else if (globalData.activateSendPacked == 0) {
    return;
  } else {
    activateSendPacked();
  }//if
  return;
}//FastScheduler::sendPacked()

Uint32
APZJobBuffer::retrieve(Signal* signal)
{              
  Uint32 tOccupancy = theOccupancy;
  Uint32 myRPtr = rPtr;
  BufferEntry& buf = buffer[myRPtr];
  Uint32 gsnbnr;
  Uint32 cond =  (++myRPtr == bufSize) - 1;
  Uint32 tRecBlockNo = buf.header.theReceiversBlockNumber;
  
  if (tOccupancy != 0) {
    if (tRecBlockNo != 0) {
      // Transform protocol to signal. 
      rPtr = myRPtr & cond;
      theOccupancy = tOccupancy - 1;
      gsnbnr = buf.header.theVerId_signalNumber << 16 | tRecBlockNo;
      
      Uint32 tSignalId = globalData.theSignalId;
      Uint32 tLength = buf.header.theLength;
      Uint32 tFirstData = buf.theDataRegister[0];
      signal->header = buf.header;
      
      // Recall our signal Id for restart purposes
      buf.header.theSignalId = tSignalId;  
      globalData.theSignalId = tSignalId + 1;
      
      Uint32* tDataRegPtr = &buf.theDataRegister[0];
      Uint32* tSigDataPtr = signal->getDataPtrSend();
      *tSigDataPtr = tFirstData;
      tDataRegPtr++;
      tSigDataPtr++;
      Uint32  tLengthCopied = 1;
      while (tLengthCopied < tLength) {
        Uint32 tData0 = tDataRegPtr[0];
        Uint32 tData1 = tDataRegPtr[1];
        Uint32 tData2 = tDataRegPtr[2];
        Uint32 tData3 = tDataRegPtr[3];
	
        tDataRegPtr += 4;
        tLengthCopied += 4;
	
        tSigDataPtr[0] = tData0;
        tSigDataPtr[1] = tData1;
        tSigDataPtr[2] = tData2;
        tSigDataPtr[3] = tData3;
        tSigDataPtr += 4;
      }//while
      
      tSigDataPtr = signal->m_sectionPtrI;
      tDataRegPtr = buf.theDataRegister + buf.header.theLength;
      Uint32 ptr0 = * tDataRegPtr ++;
      Uint32 ptr1 = * tDataRegPtr ++;
      Uint32 ptr2 = * tDataRegPtr ++;
      * tSigDataPtr ++ = ptr0;
      * tSigDataPtr ++ = ptr1;
      * tSigDataPtr ++ = ptr2;

      //---------------------------------------------------------
      // Prefetch of buffer[rPtr] is done here. We prefetch for
      // read both the first cache line and the next 64 byte
      // entry
      //---------------------------------------------------------
      NDB_PREFETCH_READ((void*)&buffer[rPtr]);
      NDB_PREFETCH_READ((void*)(((char*)&buffer[rPtr]) + 64));
      return gsnbnr;
    } else {
      bnr_error();
      return 0; // Will never come here, simply to keep GCC happy.
    }//if
  } else {
    //------------------------------------------------------------
    // The Job Buffer was empty, signal this by return zero.
    //------------------------------------------------------------
    return 0;
  }//if
}//APZJobBuffer::retrieve()

void 
APZJobBuffer::signal2buffer(Signal* signal,
			    BlockNumber bnr, GlobalSignalNumber gsn,
			    BufferEntry& buf)
{
  Uint32 tSignalId = globalData.theSignalId;
  Uint32 tFirstData = signal->theData[0];
  Uint32 tLength = signal->header.theLength + signal->header.m_noOfSections;
  Uint32 tSigId  = buf.header.theSignalId;
  
  buf.header = signal->header;
  buf.header.theVerId_signalNumber = gsn;
  buf.header.theReceiversBlockNumber = bnr;
  buf.header.theSendersSignalId = tSignalId - 1;
  buf.header.theSignalId = tSigId;
  buf.theDataRegister[0] = tFirstData;
  
  Uint32 tLengthCopied = 1;
  Uint32* tSigDataPtr = &signal->theData[1];
  Uint32* tDataRegPtr = &buf.theDataRegister[1];
  while (tLengthCopied < tLength) {
    Uint32 tData0 = tSigDataPtr[0];
    Uint32 tData1 = tSigDataPtr[1];
    Uint32 tData2 = tSigDataPtr[2];
    Uint32 tData3 = tSigDataPtr[3];
    
    tLengthCopied += 4;
    tSigDataPtr += 4;

    tDataRegPtr[0] = tData0;
    tDataRegPtr[1] = tData1;
    tDataRegPtr[2] = tData2;
    tDataRegPtr[3] = tData3;
    tDataRegPtr += 4;
  }//while
}//APZJobBuffer::signal2buffer()

void
APZJobBuffer::insert(const SignalHeader * const sh,
		     const Uint32 * const theData, const Uint32 secPtrI[3]){
  Uint32 tOccupancy = theOccupancy + 1;
  Uint32 myWPtr = wPtr;
  register BufferEntry& buf = buffer[myWPtr];
  
  if (tOccupancy < bufSize) {
    Uint32 cond =  (++myWPtr == bufSize) - 1;
    wPtr = myWPtr & cond;
    theOccupancy = tOccupancy;
    
    buf.header = * sh;
    const Uint32 len = buf.header.theLength;
    memcpy(buf.theDataRegister, theData, 4 * len);
    memcpy(&buf.theDataRegister[len], &secPtrI[0], 4 * 3);
    //---------------------------------------------------------
    // Prefetch of buffer[wPtr] is done here. We prefetch for
    // write both the first cache line and the next 64 byte
    // entry
    //---------------------------------------------------------
    NDB_PREFETCH_WRITE((void*)&buffer[wPtr]);
    NDB_PREFETCH_WRITE((void*)(((char*)&buffer[wPtr]) + 64));
  } else {
    jbuf_error();
  }//if
}
APZJobBuffer::APZJobBuffer()
  : bufSize(0), buffer(NULL), memRef(NULL)
{
  clear();
}

APZJobBuffer::~APZJobBuffer()
{
  delete [] buffer;
}

void
APZJobBuffer::newBuffer(int size)
{
  buffer = new BufferEntry[size + 1]; // +1 to support "overrrun"
  if(buffer){
#ifndef NDB_PURIFY
    ::memset(buffer, 0, (size * sizeof(BufferEntry)));
#endif
    bufSize = size;
  } else
    bufSize = 0;
}

void
APZJobBuffer::clear()
{
  rPtr = 0;
  wPtr = 0;
  theOccupancy = 0;
}

/**
 * Function prototype for print_restart
 *
 *   Defined later in this file
 */
void print_restart(FILE * output, Signal* signal, Uint32 aLevel);

void FastScheduler::dumpSignalMemory(Uint32 thr_no, FILE * output)
{
  SignalT<25> signalT;
  Signal * signal = new (&signalT) Signal(0);
  Uint32 ReadPtr[5];
  Uint32 tJob;
  Uint32 tLastJob;

  /* Single threaded ndbd scheduler, no threads. */
  assert(thr_no == 0);

  fprintf(output, "\n");
 
  if (globalData.JobLap > 4095) {
    if (globalData.JobCounter != 0)
      tJob = globalData.JobCounter - 1;
    else
      tJob = 4095;
    tLastJob = globalData.JobCounter;
  } else {
    if (globalData.JobCounter == 0)
      return; // No signals sent
    else {
      tJob = globalData.JobCounter - 1;
      tLastJob = 4095;
    }
  }
  ReadPtr[0] = theJobBuffers[0].getReadPtr();
  ReadPtr[1] = theJobBuffers[1].getReadPtr();
  ReadPtr[2] = theJobBuffers[2].getReadPtr();
  ReadPtr[3] = theJobBuffers[3].getReadPtr();
  
  do {
    unsigned char tLevel = theJobPriority[tJob];
    globalData.incrementWatchDogCounter(4);
    if (ReadPtr[tLevel] == 0)
      ReadPtr[tLevel] = theJobBuffers[tLevel].getBufSize() - 1;
    else
      ReadPtr[tLevel]--;
    
    theJobBuffers[tLevel].retrieveDump(signal, ReadPtr[tLevel]);
    // strip instance bits since this in non-MT code
    signal->header.theReceiversBlockNumber &= NDBMT_BLOCK_MASK;
    print_restart(output, signal, tLevel);
    
    if (tJob == 0)
      tJob = 4095;
    else
      tJob--;
    
  } while (tJob != tLastJob);
  fflush(output);
}

void
FastScheduler::prio_level_error()
{
  ERROR_SET(ecError, NDBD_EXIT_WRONG_PRIO_LEVEL, 
	    "Wrong Priority Level", "FastScheduler.C");
}

void 
jbuf_error()
{
  ERROR_SET(ecError, NDBD_EXIT_BLOCK_JBUFCONGESTION, 
	    "Job Buffer Full", "APZJobBuffer.C");
}

void 
bnr_error()
{
  ERROR_SET(ecError, NDBD_EXIT_BLOCK_BNR_ZERO, 
	    "Block Number Zero", "FastScheduler.C");
}

void
print_restart(FILE * output, Signal* signal, Uint32 aLevel)
{
  fprintf(output, "--------------- Signal ----------------\n");
  SignalLoggerManager::printSignalHeader(output, 
					 signal->header,
					 aLevel,
					 globalData.ownId, 
					 true);
  SignalLoggerManager::printSignalData  (output, 
					 signal->header,
					 &signal->theData[0]);
}

void
FastScheduler::traceDumpPrepare(NdbShutdownType&)
{
  /* No-operation in single-threaded ndbd. */
}

Uint32
FastScheduler::traceDumpGetNumThreads()
{
  return 1;                     // Single-threaded ndbd scheduler
}

int
FastScheduler::traceDumpGetCurrentThread()
{
  return -1;                     // Single-threaded ndbd scheduler
}

bool
FastScheduler::traceDumpGetJam(Uint32 thr_no,
                               const JamEvent * & thrdTheEmulatedJam,
                               Uint32 & thrdTheEmulatedJamIndex)
{
  /* Single threaded ndbd scheduler, no threads. */
  assert(thr_no == 0);

#ifdef NO_EMULATED_JAM
  thrdTheEmulatedJam = NULL;
  thrdTheEmulatedJamIndex = 0;
#else
  const EmulatedJamBuffer *jamBuffer = NDB_THREAD_TLS_JAM;
  thrdTheEmulatedJam = jamBuffer->theEmulatedJam;
  thrdTheEmulatedJamIndex = jamBuffer->theEmulatedJamIndex;
#endif
  return true;
}


/**
 * This method used to be a Cmvmi member function
 * but is now a "ordinary" function"
 *
 * See TransporterCallback.cpp for explanation
 */
void 
FastScheduler::reportDoJobStatistics(Uint32 tMeanLoopCount) {
  SignalT<2> signal;

  memset(&signal.header, 0, sizeof(signal.header));
  signal.header.theLength = 2;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, 0);  
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
  signal.header.theReceiversBlockNumber = CMVMI;

  signal.theData[0] = NDB_LE_JobStatistic;
  signal.theData[1] = tMeanLoopCount;

  Uint32 secPtr[3];  
  execute(&signal.header, JBA, signal.theData, secPtr);
}

void 
FastScheduler::reportThreadConfigLoop(Uint32 expired_time,
                                      Uint32 extra_constant,
                                      Uint32 *no_exec_loops,
                                      Uint32 *tot_exec_time,
                                      Uint32 *no_extra_loops,
                                      Uint32 *tot_extra_time)
{
  SignalT<6> signal;

  memset(&signal.header, 0, sizeof(signal.header));
  signal.header.theLength = 6;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, 0);  
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
  signal.header.theReceiversBlockNumber = CMVMI;

  signal.theData[0] = NDB_LE_ThreadConfigLoop;
  signal.theData[1] = expired_time;
  signal.theData[2] = extra_constant;
  signal.theData[3] = (*tot_exec_time)/(*no_exec_loops);
  signal.theData[4] = *no_extra_loops;
  if (*no_extra_loops > 0)
    signal.theData[5] = (*tot_extra_time)/(*no_extra_loops);
  else
    signal.theData[5] = 0;

  *no_exec_loops = 0;
  *tot_exec_time = 0;
  *no_extra_loops = 0;
  *tot_extra_time = 0;

  Uint32 secPtr[3];  
  execute(&signal.header, JBA, signal.theData, secPtr);
}

static NdbMutex g_mm_mutex;

void
mt_mem_manager_init()
{
  NdbMutex_Init(&g_mm_mutex);
}

void
mt_mem_manager_lock()
{
  NdbMutex_Lock(&g_mm_mutex);
}

void
mt_mem_manager_unlock()
{
  NdbMutex_Unlock(&g_mm_mutex);
}
