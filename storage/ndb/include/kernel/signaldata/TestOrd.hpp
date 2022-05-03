/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef TEST_ORD_H
#define TEST_ORD_H

#include "SignalData.hpp"

#define JAM_FILE_ID 196


/**
 * Send by API to perform TEST ON / TEST OFF
 *
 * SENDER:  API
 * RECIVER: SimBlockCMCtrBlck
 */
class TestOrd {
  friend class Ndb;
  friend class Cmvmi;
  friend class MgmtSrvr;
public:

  enum Command {
    KeepUnchanged = 0,
    On            = 1,
    Off           = 2,
    Toggle        = 3,
    COMMAND_MASK  = 3
  };
  
  enum SignalLoggerSpecification {
    InputSignals       = 1,
    OutputSignals      = 2,
    InputOutputSignals = 3,
    LOG_MASK           = 3
  };

  enum TraceSpecification {
    TraceALL              = 0,
    TraceAPI              = 1,
    TraceGlobalCheckpoint = 2,
    TraceLocalCheckpoint  = 4,
    TraceDisconnect       = 8,
    TRACE_MASK            = 15
  };

private:
  static constexpr Uint32 SignalLength = 25;

  /**
   * Clear Signal
   */
  void clear();

  /**
   * Set/Get test command
   */
  void setTestCommand(Command);
  void getTestCommand(Command&) const;
  
  /**
   * Set trace command
   */
  void setTraceCommand(Command, TraceSpecification);
  
  /**
   * Get trace command
   */
  void getTraceCommand(Command&, TraceSpecification&) const;
  
  /**
   * Return no of signal logger commands 
   *
   * -1 Means apply command(0) to all blocks
   * 
   */
  UintR getNoOfSignalLoggerCommands() const;

  /**
   * Add a signal logger command to a specific block
   */
  void addSignalLoggerCommand(BlockNumber, Command, SignalLoggerSpecification);
  
  /**
   * Add a signal logger command to all blocks
   *
   * Note removes all previously added commands
   *
   */
  void addSignalLoggerCommand(Command, SignalLoggerSpecification);
			  
  /**
   * Get Signal logger command
   */
  void getSignalLoggerCommand(int no, BlockNumber&, Command&, SignalLoggerSpecification&) const;
  
  UintR testCommand;              // DATA 0 
  UintR traceCommand;             // DATA 1
  UintR noOfSignalLoggerCommands; // DATA 2
  UintR signalLoggerCommands[22]; // DATA 3 - 25
};

#define COMMAND_SHIFT  (0)
#define TRACE_SHIFT    (2)
#define LOG_SHIFT      (2)

#define BLOCK_NO_SHIFT (16)
#define BLOCK_NO_MASK  65535

/**
 * Clear Signal
 */
inline
void
TestOrd::clear(){
  setTestCommand(KeepUnchanged);
  setTraceCommand(KeepUnchanged, TraceAPI); // 
  noOfSignalLoggerCommands = 0;
}

/**
 * Set/Get test command
 */
inline
void
TestOrd::setTestCommand(Command cmd){
  ASSERT_RANGE(cmd, 0, COMMAND_MASK, "TestOrd::setTestCommand");
  testCommand = cmd;
}

inline
void
TestOrd::getTestCommand(Command & cmd) const{
  cmd = (Command)(testCommand >> COMMAND_SHIFT);
}

/**
 * Set trace command
 */
inline
void
TestOrd::setTraceCommand(Command cmd, TraceSpecification spec){
  ASSERT_RANGE(cmd, 0, COMMAND_MASK, "TestOrd::setTraceCommand");
  ASSERT_RANGE(spec, 0, TRACE_MASK, "TestOrd::setTraceCommand");
  traceCommand = (cmd << COMMAND_SHIFT) | (spec << TRACE_SHIFT);
}

/**
 * Get trace command
 */
inline
void
TestOrd::getTraceCommand(Command & cmd, TraceSpecification & spec) const{
  cmd  = (Command)((traceCommand >> COMMAND_SHIFT) & COMMAND_MASK);
  spec = (TraceSpecification)((traceCommand >> TRACE_SHIFT) & TRACE_MASK);
}

/**
 * Return no of signal logger commands 
 *
 * -1 Means apply command(0) to all blocks
 * 
 */
inline
UintR
TestOrd::getNoOfSignalLoggerCommands() const{
  return noOfSignalLoggerCommands;
}

/**
 * Add a signal logger command to a specific block
 */
inline
void
TestOrd::addSignalLoggerCommand(BlockNumber bnr, 
				Command cmd, SignalLoggerSpecification spec){
  ASSERT_RANGE(cmd, 0, COMMAND_MASK, "TestOrd::addSignalLoggerCommand");
  ASSERT_RANGE(spec, 0, LOG_MASK, "TestOrd::addSignalLoggerCommand");
  //ASSERT_MAX(bnr, BLOCK_NO_MASK, "TestOrd::addSignalLoggerCommand");

  signalLoggerCommands[noOfSignalLoggerCommands] =
    (bnr << BLOCK_NO_SHIFT) | (cmd << COMMAND_SHIFT) | (spec << LOG_SHIFT);
  noOfSignalLoggerCommands ++;
}

/**
 * Add a signal logger command to all blocks
 *
 * Note removes all previously added commands
 *
 */
inline
void
TestOrd::addSignalLoggerCommand(Command cmd, SignalLoggerSpecification spec){
  ASSERT_RANGE(cmd, 0, COMMAND_MASK, "TestOrd::addSignalLoggerCommand");
  ASSERT_RANGE(spec, 0, LOG_MASK, "TestOrd::addSignalLoggerCommand");

  noOfSignalLoggerCommands = ~0;
  signalLoggerCommands[0] = (cmd << COMMAND_SHIFT) | (spec << LOG_SHIFT);
}

/**
 * Get Signal logger command
 */
inline
void
TestOrd::getSignalLoggerCommand(int no, BlockNumber & bnr, 
				Command & cmd, 
				SignalLoggerSpecification & spec) const{
  bnr  = (BlockNumber)((signalLoggerCommands[no] >> BLOCK_NO_SHIFT) 
		       & BLOCK_NO_MASK);
  cmd  = (Command)((signalLoggerCommands[no] >> COMMAND_SHIFT)  
		   & COMMAND_MASK);
  spec = (SignalLoggerSpecification)((signalLoggerCommands[no] >> LOG_SHIFT) 
				     & LOG_MASK);
}


#undef JAM_FILE_ID

#endif
