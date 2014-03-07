/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FS_CLOSE_REQ_H
#define FS_CLOSE_REQ_H

#include "SignalData.hpp"

#define JAM_FILE_ID 202


/**
 * 
 * SENDER:  
 * RECIVER: Ndbfs
 */
class FsCloseReq {
  /**
   * Reciver(s)
   */
  friend class Ndbfs;         // Reciver
  friend class VoidFs;
  friend class Lgman;
  friend class Tsman;

  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Dbdict;
  friend class Restore;

  /**
   * For printing
   */
  friend bool printFSCLOSEREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 4 );

private:

  /**
   * DATA VARIABLES
   */

  UintR filePointer;          // DATA 0
  UintR userReference;        // DATA 1
  UintR userPointer;          // DATA 2
  UintR fileFlag;             // DATA 3

  static bool  getRemoveFileFlag(const UintR & fileflag);
  static void setRemoveFileFlag(UintR & fileflag, bool removefile);

};


inline
bool 
FsCloseReq::getRemoveFileFlag(const UintR & fileflag){
  return (fileflag == 1);
}

inline
void
FsCloseReq::setRemoveFileFlag(UintR & fileflag, bool removefile){
//  ASSERT_BOOL(removefile, "FsCloseReq::setRemoveFileFlag");
  if (removefile)
    fileflag = 1;
  else
    fileflag = 0;
}



#undef JAM_FILE_ID

#endif
