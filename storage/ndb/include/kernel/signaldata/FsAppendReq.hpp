/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef FS_APPENDREQ_H
#define FS_APPENDREQ_H

#include "SignalData.hpp"

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

#endif
