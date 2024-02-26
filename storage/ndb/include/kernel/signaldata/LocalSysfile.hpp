/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
  static constexpr Uint32 SignalLength = 2;
  static constexpr Uint32 NODE_RESTORABLE_ON_ITS_OWN = 0;
  static constexpr Uint32 NODE_NOT_RESTORABLE_ON_ITS_OWN = 1;
  static constexpr Uint32 NODE_REQUIRE_INITIAL_RESTART = 2;

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
  static constexpr Uint32 SignalLength = 3;

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
  static constexpr Uint32 SignalLength = 5;

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
  static constexpr Uint32 SignalLength = 1;

  /**
   * DATA VARIABLES
   */
  UintR userPointer;              // DATA 0
};

#undef JAM_FILE_ID

#endif
