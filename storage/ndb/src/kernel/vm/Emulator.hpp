/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef EMULATOR_H
#define EMULATOR_H

//===========================================================================
//
// .DESCRIPTION
//      This is the main fuction for the AXE VM emulator.
//      It contains some global objects and a run method.
//
//===========================================================================
#include <kernel_types.h>

#define JAM_FILE_ID 260


extern class  JobTable            globalJobTable;
extern class  TimeQueue           globalTimeQueue;
extern class  FastScheduler       globalScheduler;
extern class  TransporterRegistry globalTransporterRegistry;
extern struct GlobalData          globalData;

#ifdef VM_TRACE
extern class SignalLoggerManager globalSignalLoggers;
#endif

/* EMULATED_JAM_SIZE must be a power of two, so JAM_MASK will work. */
#ifdef NDEBUG
// Keep jam buffer small for optimized build to improve locality of reference.
#define EMULATED_JAM_SIZE 1024
#else
#define EMULATED_JAM_SIZE 32768
#endif
#define JAM_MASK (EMULATED_JAM_SIZE - 1)

/**
 * JamEvents are used for recording that control passes a given point int the
 * code, reperesented by a JAM_FILE_ID value (which uniquely identifies a 
 * source file, and a line number. The reason for using JAM_FILE_ID rather
 * than the predefined __FILE__ is that is faster to store a 16-bit integer
 * than a pointer. For a description of how to maintain and debug JAM_FILE_IDs,
 * please refer to the comments for jamFileNames in Emulator.cpp.
 */
class JamEvent
{
public:
  /**
   * This method is used for verifying that JAM_FILE_IDs matches the contents 
   * of the jamFileNames table. The file name may include driectory names, 
   * which will be ignored.
   * @returns: true if fileId and pathName matches the jamFileNames table.
   */
  static bool verifyId(Uint32 fileId, const char* pathName);

  explicit JamEvent()
    :m_jamVal(0x7fffffff){}

  explicit JamEvent(Uint32 fileId, Uint32 lineNo)
    :m_jamVal(fileId << 16 | lineNo){}

  Uint32 getFileId() const
  {
    return (m_jamVal >> 16) & 0x7fff;
  }

  // Get the name of the source file, or NULL if unknown.
  const char* getFileName() const;

  Uint32 getLineNo() const
  {
    return m_jamVal & 0xffff;
  }

  bool isEmpty() const
  {
    return m_jamVal == 0x7fffffff;
  }

  /*
    True if the next JamEvent is the first in the execution of an incomming 
    signal.
  */
  bool isEndOfSig() const
  {
    return (m_jamVal >> 31) == 1;
  }

  /*
    Mark this event as the last one before the execution of the next incomming 
    signal. (We mark the last event before a signal instead of the fist event
    in a signal since this makes jam() more efficient, by eliminating the need
    to preserve bit 31 in the event that it accesses.) 
  */
  void setEndOfSig(bool isEnd)
  {
    m_jamVal |= (isEnd << 31);
  }

private:
  /*
    Bit 0-15:  line number.
    Bit 16-30: JAM_FILE_ID.
    Bit 31:    True if next JamEvent is the beginning of an incomming signal.
  */
  Uint32 m_jamVal;
};

/***
 * This is a ring buffer of JamEvents for a thread.
 */
struct EmulatedJamBuffer
{
  // Index of the next entry.
  Uint32 theEmulatedJamIndex;
  JamEvent theEmulatedJam[EMULATED_JAM_SIZE];

  // Mark current JamEvent as the last one before processing a new signal.
  void markEndOfSigExec()
  {
    const Uint32 eventNo = (theEmulatedJamIndex - 1) & JAM_MASK;
    theEmulatedJam[eventNo].setEndOfSig(true);    
  }
};

struct EmulatorData {
  class Configuration * theConfiguration;
  class WatchDog      * theWatchDog;
  class ThreadConfig  * theThreadConfig;
  class SimBlockList  * theSimBlockList;
  class SocketServer  * m_socket_server;
  class Ndbd_mem_manager * m_mem_manager;

  /**
   * Constructor
   *
   *  Sets all the pointers to NULL
   */
  EmulatorData();
  
  /**
   * Create all the objects
   */
  void create();
  
  /**
   * Destroys all the objects
   */
  void destroy();
};

extern struct EmulatorData globalEmulatorData;

/**
 * Get number of extra send buffer pages to use
 */
Uint32 mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                                      Uint32 extra_mem_pages);

/**
 * Compute no of pages to be used as job-buffer
 */
Uint32 compute_jb_pages(struct EmulatorData* ed);


#undef JAM_FILE_ID

#endif 
