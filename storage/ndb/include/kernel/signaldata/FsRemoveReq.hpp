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

#ifndef FS_REMOVE_REQ_H
#define FS_REMOVE_REQ_H

#include "SignalData.hpp"
#include "FsOpenReq.hpp"

#define JAM_FILE_ID 62


/**
 * 
 * SENDER:  
 * RECIVER: Ndbfs
 */
class FsRemoveReq {
  /**
   * Reciver(s)
   */
  friend class Ndbfs;         // Reciver
  friend class AsyncFile;     // Uses FsOpenReq to decode file open flags
  friend class Filename;
  friend class VoidFs;
  friend class Restore;

  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Dbdict;
  friend class Dbacc;
  friend class Dbtup;
  friend class Ndbcntr;       // For initial start...

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 8 );

private:

  /**
   * DATA VARIABLES
   */

  UintR userReference;        // DATA 0
  UintR userPointer;          // DATA 1
  UintR fileNumber[4];        // DATA 2 - 5 // See FsOpen for interpretation

  /**
   * 0 = File -> rm file
   * 1 = Directory -> rm -r path
   */
  UintR directory;

  /**
   * If directory = 1
   *
   * 0 = remove only files/direcories in directory specified in fileNumber
   * 1 = remove directory specified in fileNumber
   */
  UintR ownDirectory;
};


#undef JAM_FILE_ID

#endif

