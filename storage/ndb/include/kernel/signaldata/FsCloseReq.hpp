/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
  friend class Dbtup;
  friend class Ndbcntr;

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
