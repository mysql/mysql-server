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

#ifndef FastScheduler_H
#define FastScheduler_H

#include <kernel_types.h>
#include <portlib/NdbTick.h>
#include <portlib/ndb_prefetch.h>
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <Prio.hpp>
#include <SignalLoggerManager.hpp>
#include <SimulatedBlock.hpp>
#include <TransporterDefinitions.hpp>
#include <VMSignal.hpp>

#define JAM_FILE_ID 244

#define MAX_SIGNALS_EXECUTED_BEFORE_ZERO_TIME_QUEUE_SCAN 100
#define MAX_OCCUPANCY 1024

#define JBASIZE 1280  // Jobs which have dead lines to meet use this level
#define JBBSIZE 4096  // Most jobs use this level
#define JBCSIZE 64    // Only used by STTOR and STTORRY currently
#define JBDSIZE \
  4096  // Time Queue uses this level for storage, not supported
        // as priority level
void bnr_error();
void jbuf_error();
class Signal;
class Block;

class BufferEntry {
 public:
  SignalHeader header;
  Uint32 theDataRegister[25];
};

class APZJobBuffer {
 public:
  APZJobBuffer();
  ~APZJobBuffer();

  void newBuffer(int size);

  void insert(Signal25 *signal);
  void insert(const SignalHeader *const sh, const Uint32 *const theData,
              const Uint32 secPtrI[3]);
  void insert(Signal25 *signal, Uint32 myWPtr);

  Uint32 retrieve(Signal25 *signal);
  void retrieve(Signal25 *signal, Uint32 myRptr);

  /**
   * Used when dumping to trace file
   */
  void retrieveDump(Signal25 *signal, Uint32 myRptr);

  void clear();
  Uint32 getOccupancy() const;

  Uint32 getReadPtr() const;
  Uint32 getWritePtr() const;
  Uint32 getBufSize() const;

 private:
  void signal2buffer(Signal25 *signal, BufferEntry &buf);
  Uint32 rPtr;
  Uint32 wPtr;
  Uint32 theOccupancy;
  Uint32 bufSize;
  BufferEntry *buffer;
  BufferEntry *memRef;
};

class FastScheduler {
 public:
  FastScheduler();
  ~FastScheduler();

  Uint32 doJob(Uint32 loopStartCount);
  void postPoll();
  int checkDoJob();

  void activateSendPacked();

  void execute(Signal25 *signal, Priority prio);

  void execute(const SignalHeader *const sh, Uint8 prio,
               const Uint32 *const theData, const Uint32 secPtr[3]);

  void clear();
  Signal *getVMSignals();

  Priority highestAvailablePrio() const;
  Uint32 getBOccupancy() const;
  void sendPacked();

  void insertTimeQueue(Signal25 *aSignal, Uint32 aIndex);
  void scheduleTimeQueue(Uint32 aIndex);

  /*
    The following implement aspects of ErrorReporter that differs between
    singlethreaded and multithread ndbd.
  */

  /* Called before dumping, intended to stop any still running processing. */
  void traceDumpPrepare(NdbShutdownType &);
  /* Number of threads to create trace files for (thread id 0 .. N-1). */
  Uint32 traceDumpGetNumThreads();

  int traceDumpGetCurrentThread();  // returns -1 if not found

  /* Get jam() buffers etc. for specific thread. */
  bool traceDumpGetJam(Uint32 thr_no, const JamEvent *&thrdTheEmulatedJam,
                       Uint32 &thrdTheEmulatedJamIndex);
  /* Produce a signal dump. */
  void dumpSignalMemory(Uint32 thr_no, FILE *output);

  void reportThreadConfigLoop(Uint32 expired_time, Uint32 extra_constant,
                              Uint32 *no_exec_loops, Uint32 *tot_exec_time,
                              Uint32 *no_extra_loops, Uint32 *tot_extra_time);

  /* Get/Set high resolution timer in microseconds */
  NDB_TICKS getHighResTimer() { return curr_ticks; }
  const NDB_TICKS *getHighResTimerPtr() { return &curr_ticks; }
  void setHighResTimer(NDB_TICKS ticks) { curr_ticks = ticks; }

 private:
  void highestAvailablePrio(Priority prio);
  void reportJob(Priority aPriority);
  void prio_level_error();

  Uint32 theDoJobTotalCounter;
  Uint32 theDoJobCallCounter;
  NDB_TICKS curr_ticks;
  Uint8 theJobPriority[4096];
  APZJobBuffer theJobBuffers[JB_LEVELS];

  void reportDoJobStatistics(Uint32 meanLoopCount);
};

inline Uint32 FastScheduler::getBOccupancy() const {
  return theJobBuffers[JBB].getOccupancy();
}  // FastScheduler::getBOccupancy()

inline int FastScheduler::checkDoJob() {
  /*
   * Job buffer overload protetction
   * If the job buffer B is filled over a certain limit start
   * to execute the signals in the job buffer's
   */
  if (getBOccupancy() < MAX_OCCUPANCY) {
    return 0;
  } else {
    Uint32 loopStartCount = 0;
    doJob(loopStartCount);
    return 1;
  }  // if
}  // FastScheduler::checkDoJob()

inline void FastScheduler::reportJob(Priority aPriority) {
  Uint32 tJobCounter = globalData.JobCounter;
  Uint64 tJobLap = globalData.JobLap;
  theJobPriority[tJobCounter] = (Uint8)aPriority;
  globalData.JobCounter = (tJobCounter + 1) & 4095;
  globalData.JobLap = tJobLap + 1;
}

inline Priority FastScheduler::highestAvailablePrio() const {
  return (Priority)globalData.highestAvailablePrio;
}

inline void FastScheduler::highestAvailablePrio(Priority prio) {
  globalData.highestAvailablePrio = (Uint32)prio;
}

inline Signal *FastScheduler::getVMSignals() {
  return &globalData.VMSignals[0];
}

// Inserts of a protocol object into the Job Buffer.
inline void FastScheduler::execute(const SignalHeader *const sh, Uint8 prio,
                                   const Uint32 *const theData,
                                   const Uint32 secPtrI[3]) {
#ifdef VM_TRACE
  if (prio >= LEVEL_IDLE) prio_level_error();
#endif

  theJobBuffers[prio].insert(sh, theData, secPtrI);
  if (prio < (Uint8)highestAvailablePrio())
    highestAvailablePrio((Priority)prio);
}

inline void FastScheduler::execute(Signal25 *signal, Priority prio) {
#ifdef VM_TRACE
  if (prio >= LEVEL_IDLE) prio_level_error();
#endif
  theJobBuffers[prio].insert(signal);
  if (prio < highestAvailablePrio()) highestAvailablePrio(prio);
}

inline void FastScheduler::insertTimeQueue(Signal25 *signal, Uint32 aIndex) {
  theJobBuffers[3].insert(signal, aIndex);
}

inline void FastScheduler::scheduleTimeQueue(Uint32 aIndex) {
  Signal25 *signal = reinterpret_cast<Signal25 *>(getVMSignals());
  theJobBuffers[3].retrieve(signal, aIndex);
  theJobBuffers[0].insert(signal);
  if (highestAvailablePrio() > JBA) highestAvailablePrio(JBA);

  signal->header.m_noOfSections = 0;  // Or else sendPacked might pick it up
}

inline Uint32 APZJobBuffer::getWritePtr() const { return wPtr; }

inline Uint32 APZJobBuffer::getReadPtr() const { return rPtr; }

inline Uint32 APZJobBuffer::getOccupancy() const { return theOccupancy; }

inline Uint32 APZJobBuffer::getBufSize() const { return bufSize; }

inline void APZJobBuffer::retrieve(Signal25 *signal, Uint32 myRptr) {
  BufferEntry &buf = buffer[myRptr];

  buf.header.theSignalId = globalData.theSignalId++;

  signal->header = buf.header;

  Uint32 *from = (Uint32 *)&buf.theDataRegister[0];
  Uint32 *to = (Uint32 *)&signal->theData[0];
  Uint32 noOfWords = buf.header.theLength + buf.header.m_noOfSections;
  for (; noOfWords; noOfWords--) *to++ = *from++;
  // Copy sections references (copy all without if-statements)
  return;
}

inline void APZJobBuffer::retrieveDump(Signal25 *signal, Uint32 myRptr) {
  /**
   * Note that signal id is not taken from global data
   */

  BufferEntry &buf = buffer[myRptr];
  signal->header = buf.header;

  Uint32 *from = (Uint32 *)&buf.theDataRegister[0];
  Uint32 *to = (Uint32 *)&signal->theData[0];
  Uint32 noOfWords = buf.header.theLength;
  for (; noOfWords; noOfWords--) *to++ = *from++;
  return;
}

inline void APZJobBuffer::insert(Signal25 *signal) {
  Uint32 tOccupancy = theOccupancy + 1;
  Uint32 myWPtr = wPtr;
  if (tOccupancy < bufSize) {
    BufferEntry &buf = buffer[myWPtr];
    Uint32 cond = (++myWPtr == bufSize) - 1;
    wPtr = myWPtr & cond;
    theOccupancy = tOccupancy;
    signal2buffer(signal, buf);
    //---------------------------------------------------------
    // Prefetch of buffer[wPtr] is done here. We prefetch for
    // write both the first cache line and the next 64 byte
    // entry
    //---------------------------------------------------------
    NDB_PREFETCH_WRITE((void *)&buffer[wPtr]);
    NDB_PREFETCH_WRITE((void *)(((char *)&buffer[wPtr]) + 64));
  } else {
    jbuf_error();
  }  // if
}

inline void APZJobBuffer::insert(Signal25 *signal, Uint32 myWPtr) {
  BufferEntry &buf = buffer[myWPtr];
  signal2buffer(signal, buf);
}

#undef JAM_FILE_ID

#endif
