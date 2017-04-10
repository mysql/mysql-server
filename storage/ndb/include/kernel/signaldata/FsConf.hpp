/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FS_CONF_H
#define FS_CONF_H

#include "SignalData.hpp"

#define JAM_FILE_ID 206


/**
 * FsConf - Common signal class for all CONF signals sent from Ndbfs
 * GSN_FSCLOSECONF, GSN_FSOPENCONF, GSN_FSWRITECONF, GSN_FSREADCONF,
 * GSN_FSSYNCCONF, GSN_FSREMOVECONF
 */

/**
 * 
 * SENDER:  Ndbfs
 * RECIVER: 
 */
class FsConf {
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbdict;
  friend class Lgman;
  friend class Tsman;
  friend class Pgman;
  friend class Restore;
  /**
   * Sender(s)
   */
  friend class Ndbfs;
  friend class VoidFs;

  /**
   * For printing
   */
  friend bool printFSCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  /**
   *  FSOPENCONF: static const UintR SignalLength = 2; 
   *  FSCLOSECONF, FSREADCONF, FSWRITECONF, FSSYNCCONF: static const UintR SignalLength = 2; 
   */

private:

  /**
   * DATA VARIABLES
   */
  UintR userPointer;          // DATA 0

  // Data 1
  union {
    UintR filePointer;          // FSOPENCONF
    Uint32 bytes_read;          // FSREADCONF (when allow partial read)      
  };

  // debug info for trace log
  Uint32 fileInfo;
  Uint32 file_size_hi;
  Uint32 file_size_lo;
};




#undef JAM_FILE_ID

#endif
