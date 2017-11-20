/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef LOCAL_SYSFILE_H
#define LOCAL_SYSFILE_H

#include "SignalData.hpp"

#define JAM_FILE_ID 498

/**
 * 
 * SENDER: NDBCNTR, QMGR, DBLQH
 * RECEIVER: NDBCNTR 
 */
class ReadLocalSysfileReq
{
  friend class Ndbcntr;
  friend class Qmgr;
  friend class Dblqh;
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 2 );
  STATIC_CONST( NODE_RESTORABLE_ON_ITS_OWN = 0 );
  STATIC_CONST( NODE_NOT_RESTORABLE_ON_ITS_OWN = 1 );
  STATIC_CONST( NODE_REQUIRE_INITIAL_RESTART = 2 );

  /**
   * DATA VARIABLES
   */
  UintR userPointer;              // DATA 0
  UintR userReference;            // DATA 1
};

/**
 * 
 * SENDER: NDBCNTR 
 * RECEIVER: NDBCNTR, QMGR, DBLQH
 */
class ReadLocalSysfileConf
{
  friend class Ndbcntr;
  friend class Qmgr;
  friend class Dblqh;

  friend bool printREAD_LOCAL_SYSFILE_CONF(FILE * output,
                                           const Uint32 * theData,
                                           Uint32 len,
                                           Uint16 receiverBlockNo);

  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 3 );

  /**
   * DATA VARIABLES
   */
  UintR userPointer;              // DATA 0
  UintR nodeRestorableOnItsOwn;   // DATA 1
  UintR maxGCIRestorable;         // DATA 2
};


/**
 * 
 * SENDER: DBLQH, NDBCNTR
 * RECEIVER: NDBCNTR 
 */
class WriteLocalSysfileReq
{
  friend class Ndbcntr;
  friend class Dblqh;
  friend class Qmgr;

  friend bool printWRITE_LOCAL_SYSFILE_REQ(FILE * output,
                                           const Uint32 * theData,
                                           Uint32 len,
                                           Uint16 receiverBlockNo);

  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 5 );

  /**
   * DATA VARIABLES
   */
  Uint32 userPointer;              // DATA 0
  Uint32 userReference;            // DATA 1
  Uint32 nodeRestorableOnItsOwn;   // DATA 2
  Uint32 maxGCIRestorable;         // DATA 3
  Uint32 lastWrite;                // DATA 4
};

/**
 * 
 * SENDER: NDBCNTR 
 * RECEIVER: DBLQH, NDBCNTR
 */
class WriteLocalSysfileConf
{
  friend class Ndbcntr;
  friend class Dblqh;
  friend class Qmgr;

  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 1 );

  /**
   * DATA VARIABLES
   */
  UintR userPointer;              // DATA 0
};

#undef JAM_FILE_ID

#endif
