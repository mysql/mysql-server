/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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

//****************************************************************************
//
// .NAME
//      SignalLoggerManager - Handle signal loggers
//
//****************************************************************************
#ifndef SignalLoggerManager_H
#define SignalLoggerManager_H


#include <kernel_types.h>
#include <BlockNumbers.h>
#include <RefConvert.hpp>
#include <NdbMutex.h>
#include "portlib/ndb_compiler.h"

struct SignalHeader;

class SignalLoggerManager
{
public:
  SignalLoggerManager();
  virtual ~SignalLoggerManager();

  /**
   * Sets output
   * @Returns old output stream
   */
  FILE * setOutputStream(FILE * output);
  
  /**
   * Gets current output
   */
  FILE * getOutputStream() const;

  void flushSignalLog();
  
  /**
   * For direct signals
   * @See also SimulatedBlock EXECUTE_DIRECT
   */   
  void executeDirect(const SignalHeader&, 
		     Uint8 prio, const Uint32 * theData, Uint32 node);
  
  /**
   * For input signals
   */
  void executeSignal(const SignalHeader& sh, Uint8 prio,
		     const Uint32 * theData, Uint32 node) {
    executeSignal(sh, prio, theData, node, (LinearSectionPtr*)nullptr, 0);
  }

  void executeSignal(const SignalHeader&, Uint8 prio,
                     const Uint32 * theData, Uint32 node,
                     const SegmentedSectionPtr ptr[3], Uint32 secs);

  void executeSignal(const SignalHeader&, Uint8 prio,
                     const Uint32 * theData, Uint32 node,
                     const LinearSectionPtr ptr[3], Uint32 secs);

  /**
   * For output signals
   */
  void sendSignal(const SignalHeader& sh, Uint8 prio,
		  const Uint32 * theData, Uint32 node) {
    sendSignal(sh, prio, theData, node, (LinearSectionPtr*)nullptr, 0);
  }

  void sendSignal(const SignalHeader&, Uint8 prio,
		  const Uint32 * theData, Uint32 node,
                  const SegmentedSectionPtr ptr[3], Uint32 secs);

  void sendSignal(const SignalHeader&, Uint8 prio, 
		  const Uint32 * theData, Uint32 node,
                  const LinearSectionPtr ptr[3], Uint32 secs);

  void sendSignal(const SignalHeader&, Uint8 prio,
                  const Uint32 * theData, Uint32 node,
                  const GenericSectionPtr ptr[3], Uint32 secs);
  
  /**
   * For output signals
   */
  void sendSignalWithDelay(Uint32 delayInMilliSeconds, 
			   const SignalHeader& sh,
			   Uint8 prio, const Uint32 * data, Uint32 node){
    sendSignalWithDelay(delayInMilliSeconds, sh, prio, data, node,
			(SegmentedSectionPtr*)nullptr, 0);
  }

  void sendSignalWithDelay(Uint32 delayInMilliSeconds,
			   const SignalHeader&,
			   Uint8 prio, const Uint32 * data, Uint32 node,
                           const SegmentedSectionPtr ptr[3], Uint32 secs);
  
  /**
   * Generic messages in the signal log
   */
  void log(BlockNumber bno, const char * msg, ...)
    ATTRIBUTE_FORMAT(printf, 3, 4);
  
  /**
   * LogModes
   */
  enum LogMode {
    LogOff   = 0,
    LogIn    = 1,
    LogOut   = 2,
    LogInOut = 3
  };

  /**
   * Returns no of loggers affected
   */
  int log(LogMode logMode, const char * params);
  int logOn(bool allBlocks, BlockNumber bno, LogMode logMode);
  int logOff(bool allBlocks, BlockNumber bno, LogMode logMode);
  int logToggle(bool allBlocks, BlockNumber bno, LogMode logMode);
  
  void setTrace(unsigned long trace);   
  unsigned long getTrace() const;

  void setOwnNodeId(int nodeId);
  void setLogDistributed(bool val);

  /**
   * Print header
   */
  static void printSignalHeader(FILE * output, 
				const SignalHeader & sh,
				Uint8 prio, 
				Uint32 node,
				bool printReceiversSignalId);
  
  /**
   * Function for printing the Signal Data
   */
  static void printSignalData(FILE * out, 
			      const SignalHeader & sh, const Uint32 *);

  /**
   * Print linear section.
   */
  static void printLinearSection(FILE * output,
                                 const SignalHeader & sh,
                                 const LinearSectionPtr ptr[3],
                                 unsigned i);

  /**
   * Print segmented section.
   */
  static void printSegmentedSection(FILE * output,
                                    const SignalHeader & sh,
                                    const SegmentedSectionPtr ptr[3],
                                    unsigned i);

  /**
   * Print generic section.
   */
  static void printGenericSection(FILE * output,
                                  const SignalHeader & sh,
                                  const GenericSectionPtr ptr[3],
                                  unsigned i);

  /**
   * Print data word in hex.  Adds line break before the word
   * when pos > 0 && pos % 7 == 0.  Increments pos.
   */
  static void printDataWord(FILE * output, Uint32 & pos, const Uint32 data);

private:
  bool m_logDistributed;
  Uint32 m_ownNodeId;

  FILE * outputStream;
  int log(int cmd, BlockNumber bno, LogMode logMode);
  
  Uint32        traceId;
  Uint8         logModes[NO_OF_BLOCKS];

  NdbMutex* m_mutex;

public:
  void lock() { if (m_mutex != nullptr) NdbMutex_Lock(m_mutex); }
  void unlock() { if (m_mutex != nullptr) NdbMutex_Unlock(m_mutex); }
 
  inline bool
  logMatch(BlockNumber bno, LogMode mask)
  {
    // extract main block number
    bno = blockToMain(bno);
    // avoid addressing outside logModes
    return
      bno < MIN_BLOCK_NO || bno > MAX_BLOCK_NO ||
      (logModes[bno-MIN_BLOCK_NO] & mask);
  }
};

#endif // SignalLoggerManager_H

