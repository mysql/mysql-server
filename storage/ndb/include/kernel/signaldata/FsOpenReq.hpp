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

#ifndef FS_OPEN_REQ_H
#define FS_OPEN_REQ_H

#include "SignalData.hpp"

#define JAM_FILE_ID 148


/**
 * 
 * SENDER:  
 * RECIVER: Ndbfs
 */
class FsOpenReq {
  /**
   * Reciver(s)
   */
  friend class Ndbfs;         // Reciver
  friend class AsyncFile;     // Uses FsOpenReq to decode file open flags
  friend class PosixAsyncFile; // FIXME
  friend class Win32AsyncFile; // FIXME
  friend class Filename;
  friend class VoidFs;
  friend class AsyncIoThread;

  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Dbdict;
  friend class Ndbcntr;       // For initial start...
  friend class Dbdih;
  friend class Lgman;
  friend class Tsman;
  friend class Restore;
  friend class Dblqh;

  friend class Dbtup;

  friend class Cmvmi;

  /**
   * For printing
   */
  friend bool printFSOPENREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 11 );
  SECTION( FILENAME = 0 );
  
private:

  /**
   * DATA VARIABLES
   */

  UintR userReference;        // DATA 0
  UintR userPointer;          // DATA 1
  UintR fileNumber[4];        // DATA 2 - 5
  UintR fileFlags;            // DATA 6
  Uint32 page_size;
  Uint32 file_size_hi;
  Uint32 file_size_lo;
  Uint32 auto_sync_size; // In bytes
  
  STATIC_CONST( OM_READONLY  = 0 );
  STATIC_CONST( OM_WRITEONLY = 1 );
  STATIC_CONST( OM_READWRITE = 2 );

  STATIC_CONST( OM_APPEND    = 0x8   ); // Not Implemented on W2k
  STATIC_CONST( OM_SYNC      = 0x10  );
  STATIC_CONST( OM_CREATE    = 0x100 );
  STATIC_CONST( OM_TRUNCATE  = 0x200 );
  STATIC_CONST( OM_AUTOSYNC  = 0x400 ); 

  STATIC_CONST( OM_CREATE_IF_NONE = 0x0800 );
  STATIC_CONST( OM_INIT           = 0x1000 ); // 
  STATIC_CONST( OM_CHECK_SIZE     = 0x2000 );
  STATIC_CONST( OM_DIRECT         = 0x4000 );
  STATIC_CONST( OM_GZ             = 0x8000 );
  STATIC_CONST( OM_THREAD_POOL    = 0x10000 );
  STATIC_CONST( OM_WRITE_BUFFER   = 0x20000 );
  STATIC_CONST( OM_READ_SIZE      = 0x40000 );
  STATIC_CONST( OM_DIRECT_SYNC    = 0x80000 );
  
  enum Suffixes {
    S_DATA = 0,
    S_FRAGLOG = 1,
    S_LOGLOG = 2,
    S_FRAGLIST = 3,
    S_TABLELIST = 4,
    S_SCHEMALOG = 5,
    S_SYSFILE = 6,
    S_LOG = 7,
    S_CTL = 8
  };

  enum BasePathSpec
  {
    BP_FS = 0,     // FileSystemPath
    BP_BACKUP = 1, // BackupDataDir
    BP_DD_DF = 2,  // FileSystemPathDataFiles
    BP_DD_UF = 3,  // FileSystemPathUndoFiles
    BP_MAX = 4
  };
  
  static Uint32 getVersion(const Uint32 fileNumber[]);
  static Uint32 getSuffix(const Uint32 fileNumber[]);

  static void setVersion(Uint32 fileNumber[], Uint8 val);
  static void setSuffix(Uint32 fileNumber[], Uint8 val);
  
  /**
   * V1
   */
  static Uint32 v1_getDisk(const Uint32 fileNumber[]);
  static Uint32 v1_getTable(const Uint32 fileNumber[]);
  static Uint32 v1_getFragment(const Uint32 fileNumber[]);
  static Uint32 v1_getS(const Uint32 fileNumber[]);
  static Uint32 v1_getP(const Uint32 fileNumber[]);

  static void v1_setDisk(Uint32 fileNumber[], Uint8 val);
  static void v1_setTable(Uint32 fileNumber[], Uint32 val);
  static void v1_setFragment(Uint32 fileNumber[], Uint32 val);
  static void v1_setS(Uint32 fileNumber[], Uint32 val);
  static void v1_setP(Uint32 fileNumber[], Uint8 val);

  /**
   * V2 - Backup
   */
  static Uint32 v2_getSequence(const Uint32 fileNumber[]);
  static Uint32 v2_getNodeId(const Uint32 fileNumber[]);
  static Uint32 v2_getPartNum(const Uint32 fileNumber[]);
  static Uint32 v2_getCount(const Uint32 fileNumber[]);
  static Uint32 v2_getTotalParts(const Uint32 fileNumber[]);

  static void v2_setSequence(Uint32 fileNumber[], Uint32 no);
  static void v2_setNodeId(Uint32 fileNumber[], Uint32 no);
  static void v2_setPartNum(Uint32 fileNumber[], Uint32 no);
  static void v2_setCount(Uint32 fileNumber[], Uint32 no);
  static void v2_setTotalParts(Uint32 fileNumber[], Uint32 no);

  /**
   * V4 - Specified filename
   */
  static Uint32 v4_getBasePath(const Uint32 fileNumber[]) {
    return v5_getLcpNo(fileNumber);
  }
  static void v4_setBasePath(Uint32 fileNumber[], Uint32 no) {
    v5_setLcpNo(fileNumber, no);
  }

  /**
   * V5 - LCP
   */
  static Uint32 v5_getLcpNo(const Uint32 fileNumber[]);
  static Uint32 v5_getTableId(const Uint32 fileNumber[]);
  static Uint32 v5_getFragmentId(const Uint32 fileNumber[]);

  static void v5_setLcpNo(Uint32 fileNumber[], Uint32 no);
  static void v5_setTableId(Uint32 fileNumber[], Uint32 no);
  static void v5_setFragmentId(Uint32 fileNumber[], Uint32 no);
};

/**
 * File flags (set according to solaris standard)
 *
 o = Open mode                -  2 Bits -> max 3
 c = create new file          -  1 Bit 
 t = truncate existing        -  1 Bit

           1111111111222222222233
 01234567890123456789012345678901
 oo      ct
*/


/**
 * -- v1 --
 * File number[0] = Table
 * File number[1] = Fragment
 * File number[2] = S-value
 * File number[3] =
 *   p = v1_P       0 - 7
 *   d = v1_disk    8 - 15
 *   s = v1_suffix 16 - 23
 *   v = version   24 - 31
 * 
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * ppppppppddddddddssssssssvvvvvvvv
 *
 * -- v2 --
 * File number[0] = Backup Sequence Number
 * File number[1] = Node Id
 * File number[3] =
 *   v = version   24 - 31
 *   s = v1_suffix 16 - 23
 * 
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *                 ssssssssvvvvvvvv
 *
 * -- v5 --
 * File number[0] = Table
 * File number[1] = LcpNo
 * File number[2] = Fragment Id
 *   v = version   24 - 31
 *   s = v1_suffix 16 - 23
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *                 ssssssssvvvvvvvv
 */ 
inline 
Uint32 FsOpenReq::getVersion(const Uint32 fileNumber[]){
  return (fileNumber[3] >> 24) & 0xff;
}

inline
void FsOpenReq::setVersion(Uint32 fileNumber[], Uint8 val){
  const Uint32 t = fileNumber[3];
  fileNumber[3] = (t & 0x00FFFFFF) | (((Uint32)val) << 24);
}

inline 
Uint32 FsOpenReq::getSuffix(const Uint32 fileNumber[]){
  return (fileNumber[3] >> 16)& 0xff;
}

inline
void FsOpenReq::setSuffix(Uint32 fileNumber[], Uint8 val){
  const Uint32 t = fileNumber[3];
  fileNumber[3] = (t & 0xFF00FFFF) | (((Uint32)val) << 16);
}

inline 
Uint32 FsOpenReq::v1_getDisk(const Uint32 fileNumber[]){
  return  (fileNumber[3]>>8) & 0xff;
}

inline
void FsOpenReq::v1_setDisk(Uint32 fileNumber[], Uint8 val){
  const Uint32 t = fileNumber[3];
  fileNumber[3] = (t & 0xFFFF00FF) | (((Uint32)val) << 8);
}

inline 
Uint32 FsOpenReq::v1_getTable(const Uint32 fileNumber[]){
  return fileNumber[0];
}

inline
void FsOpenReq::v1_setTable(Uint32 fileNumber[], Uint32 val){
  fileNumber[0] = val;
}

inline 
Uint32 FsOpenReq::v1_getFragment(const Uint32 fileNumber[]){
  return fileNumber[1];
}

inline
void FsOpenReq::v1_setFragment(Uint32 fileNumber[], Uint32 val){
  fileNumber[1] = val;
}

inline
Uint32 FsOpenReq::v1_getS(const Uint32 fileNumber[]){
  return fileNumber[2];
}

inline
void FsOpenReq::v1_setS(Uint32 fileNumber[], Uint32 val){
  fileNumber[2] = val;
}

inline
Uint32 FsOpenReq::v1_getP(const Uint32 fileNumber[]){
  return fileNumber[3] & 0xff;
}

inline
void FsOpenReq::v1_setP(Uint32 fileNumber[], Uint8 val){
  const Uint32 t = fileNumber[3];
  fileNumber[3] = (t & 0xFFFFFF00) | val;
}

/****************/
inline 
Uint32 FsOpenReq::v2_getSequence(const Uint32 fileNumber[]){
  return fileNumber[0];
}

inline
void FsOpenReq::v2_setSequence(Uint32 fileNumber[], Uint32 val){
  fileNumber[0] = val;
}

inline 
Uint32 FsOpenReq::v2_getNodeId(const Uint32 fileNumber[]){
  return (fileNumber[1] & 0x0000FFFF);
}

inline
void FsOpenReq::v2_setNodeId(Uint32 fileNumber[], Uint32 val){
  const Uint32 t = fileNumber[1];
  fileNumber[1] = (t & 0xFFFF0000) | (((Uint32)val) & 0x0000FFFF);
}

inline 
Uint32 FsOpenReq::v2_getPartNum(const Uint32 fileNumber[]){
  return ((fileNumber[1] >> 16) & 0x0000FFFF);
}

inline
void FsOpenReq::v2_setPartNum(Uint32 fileNumber[], Uint32 val){
  Uint32 t = fileNumber[1] ;
  fileNumber[1] = (t & 0x0000FFFF) | ((val << 16) & 0xFFFF0000);
}

inline
Uint32 FsOpenReq::v2_getCount(const Uint32 fileNumber[]){
  return (fileNumber[2] & 0x0000FFFF);
}

inline
void FsOpenReq::v2_setCount(Uint32 fileNumber[], Uint32 val){
  const Uint32 t = fileNumber[2];
  fileNumber[2] = (t & 0xFFFF0000) | (((Uint32)val) & 0x0000FFFF);
}

inline
Uint32 FsOpenReq::v2_getTotalParts(const Uint32 fileNumber[]){
  return ((fileNumber[2] >> 16) & 0x0000FFFF);
}

inline
void FsOpenReq::v2_setTotalParts(Uint32 fileNumber[], Uint32 val){
  Uint32 t = fileNumber[2] ;
  fileNumber[2] = (t & 0x0000FFFF) | ((val << 16) & 0xFFFF0000);
}

/****************/
inline 
Uint32 FsOpenReq::v5_getTableId(const Uint32 fileNumber[]){
  return fileNumber[0];
}

inline
void FsOpenReq::v5_setTableId(Uint32 fileNumber[], Uint32 val){
  fileNumber[0] = val;
}

inline 
Uint32 FsOpenReq::v5_getLcpNo(const Uint32 fileNumber[]){
  return fileNumber[1];
}

inline
void FsOpenReq::v5_setLcpNo(Uint32 fileNumber[], Uint32 val){
  fileNumber[1] = val;
}

inline 
Uint32 FsOpenReq::v5_getFragmentId(const Uint32 fileNumber[]){
  return fileNumber[2];
}

inline
void FsOpenReq::v5_setFragmentId(Uint32 fileNumber[], Uint32 val){
  fileNumber[2] = val;
}


#undef JAM_FILE_ID

#endif

