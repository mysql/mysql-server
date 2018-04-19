/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FS_APPENDREQ_H
#define FS_APPENDREQ_H

#include "SignalData.hpp"

#define JAM_FILE_ID 136


/**
 * 
 * SENDER:  
 * RECIVER: Ndbfs
 */
class FsAppendReq {
  /**
   * Reciver(s)
   */
  friend class Ndbfs;
  friend class VoidFs;

  /**
   * Sender(s)
   */
  friend class Backup;

  friend bool printFSAPPENDREQ(FILE * output, const Uint32 * theData, 
			       Uint32 len, Uint16 receiverBlockNo);
public:
  STATIC_CONST( SignalLength = 7 );

private:

  /**
   * DATA VARIABLES
   */
  UintR filePointer;          // DATA 0
  UintR userReference;        // DATA 1
  UintR userPointer;          // DATA 2
  UintR varIndex;             // DATA 3
  UintR offset;               // DATA 4
  UintR size;                 // DATA 5
  UintR synch_flag;           // DATA 6
};


#undef JAM_FILE_ID

#endif
