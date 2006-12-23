/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef FS_CONF_H
#define FS_CONF_H

#include "SignalData.hpp"

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
};



#endif
