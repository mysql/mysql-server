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

#ifndef FastScheduler_H
#define FastScheduler_H

#include <VMSignal.hpp>
#include <kernel_types.h>
#include <Prio.hpp>
#include <SignalLoggerManager.hpp>
#include <SimulatedBlock.hpp>
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <TransporterDefinitions.hpp>
#include <prefetch.h>

#define MAX_OCCUPANCY 1024

#define JBASIZE   1280 // Jobs which have dead lines to meet use this level
#define JBBSIZE   4096 // Most jobs use this level
#define JBCSIZE   64   // Only used by STTOR and STTORRY currently
#define JBDSIZE   4096 // Time Queue uses this level for storage, not supported
                       // as priority level
void bnr_error();
void jbuf_error();
class Signal;
class Block;

class BufferEntry
{
public:
  SignalHeader header;
  Uint32 theDataRegister[25];
};

class APZJobBuffer
{
public:
  APZJobBuffer();
  ~APZJobBuffer();

  void newBuffer(int size);
  
  void insert(Signal* signal, BlockNumber bnr, GlobalSignalNumber gsn);   
  void insert(const SignalHeader * const sh, const Uint32 * const theData, const Uint32 secPtrI[3]);
  void insert(Signal* signal, BlockNumber bnr, GlobalSignalNumber gsn, 
	      Uint32 myWPtr);   
  
  Uint32 retrieve(Signal *signal);
  void retrieve(Signal *signal, Uint32 myRptr);
  
  /**
   * Used when dumping to trace file
   */
  void retrieveDump(Signal *signal, Uint32 myRptr);
  
  void clear();
  Uint32 getOccupancy() const;
  
  Uint32 getReadPtr() const;
  Uint32 getWritePtr() const;
  Uint32 getBufSize() const;
  
private:
  void signal2buffer(Signal* signal, BlockNumber bnr,
		     GlobalSignalNumber gsn, BufferEntry& buf);
  Uint32 rPtr;
  Uint32 wPtr;
  Uint32 theOccupancy;
  Uint32 bufSize;
  BufferEntry* buffer;
  BufferEntry* memRef;
};


class FastScheduler
{
public:
   FastScheduler();
   ~FastScheduler();

  void doJob();
  int checkDoJob();

  void activateSendPacked();
  
  void execute(Signal* signal, 
	       Priority prio,
	       BlockNumber bnr, 
	       GlobalSignalNumber gsn);
  
  void execute(const SignalHeader * const sh, 
	       Uint8 prio, const Uint32 * const theData, const Uint32 secPtr[3]);
  
  void clear();
  Signal* getVMSignals();
  
  void dumpSignalMemory(FILE * output);
  Priority highestAvailablePrio() const;
  Uint32 getBOccupancy() const;
  void sendPacked();
  
  void insertTimeQueue(Signal* aSignal, BlockNumber bnr,
		       GlobalSignalNumber gsn, Uint32 aIndex);
  void scheduleTimeQueue(Uint32 aIndex);
  
private:
  void highestAvailablePrio(Priority prio);
  void reportJob(Priority aPriority);
  void prio_level_error();

  Uint32 theDoJobTotalCounter;
  Uint32 theDoJobCallCounter;
  Uint8 theJobPriority[4096];
  APZJobBuffer theJobBuffers[JB_LEVELS];

  void reportDoJobStatistics(Uint32 meanLoopCount);
};

inline 
Uint32 
FastScheduler::getBOccupancy() const {
  return theJobBuffers[JBB].getOccupancy();
}//FastScheduler::getBOccupancy()

inline 
int 
FastScheduler::checkDoJob()
{
  /* 
   * Job buffer overload protetction 
   * If the job buffer B is filled over a certain limit start
   * to execute the signals in the job buffer's
   */
  if (getBOccupancy() < MAX_OCCUPANCY) {
    return 0;
  } else {
    doJob();
    return 1;
  }//if
}//FastScheduler::checkDoJob()

inline 
void 
FastScheduler::reportJob(Priority aPriority)
{
  Uint32 tJobCounter = globalData.JobCounter;
  Uint32 tJobLap = globalData.JobLap;
  theJobPriority[tJobCounter] = (Uint8)aPriority;
  globalData.JobCounter = (tJobCounter + 1) & 4095;
  globalData.JobLap = tJobLap + 1;
}

inline 
Priority 
FastScheduler::highestAvailablePrio() const
{
   return (Priority)globalData.highestAvailablePrio;
}

inline 
void 
FastScheduler::highestAvailablePrio(Priority prio)
{
   globalData.highestAvailablePrio = (Uint32)prio;
}

inline 
Signal* 
FastScheduler::getVMSignals()
{
  return &globalData.VMSignals[0];
}


// Inserts of a protocol object into the Job Buffer.
inline
void
FastScheduler::execute(const SignalHeader * const sh, Uint8 prio, 
		       const Uint32 * const theData, const Uint32 secPtrI[3]){
#ifdef VM_TRACE
  if (prio >= LEVEL_IDLE)
    prio_level_error();
#endif
  
  theJobBuffers[prio].insert(sh, theData, secPtrI);
  if (prio < (Uint8)highestAvailablePrio())
    highestAvailablePrio((Priority)prio);
}

inline 
void 
FastScheduler::execute(Signal* signal, Priority prio,
		       BlockNumber bnr, GlobalSignalNumber gsn)
{
#ifdef VM_TRACE
  if (prio >= LEVEL_IDLE)
    prio_level_error();
#endif
  theJobBuffers[prio].insert(signal, bnr, gsn);
  if (prio < highestAvailablePrio())
    highestAvailablePrio(prio);
}

inline 
void 
FastScheduler::insertTimeQueue(Signal* signal, BlockNumber bnr,
			       GlobalSignalNumber gsn, Uint32 aIndex)
{
  theJobBuffers[3].insert(signal, bnr, gsn, aIndex);
}

inline 
void 
FastScheduler::scheduleTimeQueue(Uint32 aIndex)
{
  Signal* signal = getVMSignals();
  theJobBuffers[3].retrieve(signal, aIndex);
  theJobBuffers[0].insert
    (signal,
     (BlockNumber)signal->header.theReceiversBlockNumber,
     (GlobalSignalNumber)signal->header.theVerId_signalNumber);
  if (highestAvailablePrio() > JBA)
    highestAvailablePrio(JBA);
}

inline
Uint32
APZJobBuffer::getWritePtr() const
{
  return wPtr;
}

inline 
Uint32 
APZJobBuffer::getReadPtr() const
{
  return rPtr;
}

inline 
Uint32 
APZJobBuffer::getOccupancy() const 
{
  return theOccupancy;
}

inline 
Uint32 
APZJobBuffer::getBufSize() const
{
  return bufSize;
}

inline
void
APZJobBuffer::retrieve(Signal* signal, Uint32 myRptr)
{              
  register BufferEntry& buf = buffer[myRptr];
  
  buf.header.theSignalId = globalData.theSignalId++;

  signal->header = buf.header;
  
  Uint32 *from = (Uint32*) &buf.theDataRegister[0];
  Uint32 *to   = (Uint32*) &signal->theData[0];
  Uint32 noOfWords = buf.header.theLength;
  for(; noOfWords; noOfWords--)
    *to++ = *from++;
  // Copy sections references (copy all without if-statements)
  SegmentedSectionPtr * tSecPtr = &signal->m_sectionPtr[0];
  tSecPtr[0].i = from[0];
  tSecPtr[1].i = from[1];
  tSecPtr[2].i = from[2];
  return;
}

inline
void
APZJobBuffer::retrieveDump(Signal* signal, Uint32 myRptr)
{              
  /**
   * Note that signal id is not taken from global data
   */
  
  register BufferEntry& buf = buffer[myRptr];
  signal->header = buf.header;
  
  Uint32 *from = (Uint32*) &buf.theDataRegister[0];
  Uint32 *to   = (Uint32*) &signal->theData[0];
  Uint32 noOfWords = buf.header.theLength;
  for(; noOfWords; noOfWords--)
    *to++ = *from++;
  return;
}

inline
void 
APZJobBuffer::insert(Signal* signal,
		     BlockNumber bnr, GlobalSignalNumber gsn)
{
  Uint32 tOccupancy = theOccupancy + 1;
  Uint32 myWPtr = wPtr;
  if (tOccupancy < bufSize) {
    register BufferEntry& buf = buffer[myWPtr];
    Uint32 cond =  (++myWPtr == bufSize) - 1;
    wPtr = myWPtr & cond;
    theOccupancy = tOccupancy;
    signal2buffer(signal, bnr, gsn, buf);
    //---------------------------------------------------------
    // Prefetch of buffer[wPtr] is done here. We prefetch for
    // write both the first cache line and the next 64 byte
    // entry
    //---------------------------------------------------------
    WRITEHINT((void*)&buffer[wPtr]);
    WRITEHINT((void*)(((char*)&buffer[wPtr]) + 64));
  } else {
    jbuf_error();
  }//if
}


inline
void
APZJobBuffer::insert(Signal* signal, BlockNumber bnr,
		     GlobalSignalNumber gsn, Uint32 myWPtr)
{
  register BufferEntry& buf = buffer[myWPtr];
  signal2buffer(signal, bnr, gsn, buf);
}

#endif
