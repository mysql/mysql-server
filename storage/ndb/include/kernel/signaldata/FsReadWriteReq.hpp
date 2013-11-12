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

#ifndef FS_READWRITEREQ_H
#define FS_READWRITEREQ_H

#include "SignalData.hpp"

#define JAM_FILE_ID 156


/**
 * FsReadWriteReq - Common signal class for FSWRITEREQ and FSREADREQ
 *
 */

/**
 * 
 * SENDER:  
 * RECIVER: Ndbfs
 */
class FsReadWriteReq {
  /**
   * Reciver(s)
   */
  friend class Ndbfs;
  friend class VoidFs;
  friend class AsyncFile;
  friend class PosixAsyncFile; // FIXME
  friend class Win32AsyncFile;

  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Lgman;
  friend class Tsman;
  friend class Pgman;
  friend class Restore;
  friend class Dblqh;

  /**
   * For printing
   */
  friend bool printFSREADWRITEREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
 * Enum type for errorCode
 */
  enum NdbfsFormatType {
    fsFormatListOfPairs=0,
    fsFormatArrayOfPages=1,
    fsFormatListOfMemPages=2,
    fsFormatGlobalPage=3,
    fsFormatSharedPage=4,
    fsFormatMax
  };
  
  /**
   * Length of signal
   */
  STATIC_CONST( FixedLength = 6 );

private:

  /**
   * DATA VARIABLES
   */
  UintR filePointer;          // DATA 0
  UintR userReference;        // DATA 1
  UintR userPointer;          // DATA 2
  UintR operationFlag;        // DATA 3
  UintR varIndex;             // DATA 4
  UintR numberOfPages;        // DATA 5  

//-------------------------------------------------------------
// Variable sized part. Those will contain 
// info about memory/file pages to read/write
//-------------------------------------------------------------  
  union {
    UintR pageData[16];        // DATA 6 - 21
    struct {
      Uint32 varIndex;   // In unit cluster size
      Uint32 fileOffset; // In unit page size
    } listOfPair[8];
    struct {
      Uint32 varIndex;
      Uint32 fileOffset;
    } arrayOfPages;
    struct {
      Uint32 varIndex[1]; // Size = numberOfPages
      Uint32 fileOffset;
    } listOfMemPages;
  } data;

  static Uint8 getSyncFlag(const UintR & opFlag);
  static void setSyncFlag(UintR & opFlag, Uint8 flag);

  static NdbfsFormatType getFormatFlag(const UintR & opFlag);
  static void setFormatFlag(UintR & opFlag, Uint8 flag);

  static Uint32 getPartialReadFlag(UintR opFlag);
  static void setPartialReadFlag(UintR & opFlag, Uint32 flag);
};

/**
 * Operation flag
 *
 f = Format of pageData       -  4 Bits -> max 15
 s = sync after write flag    -  1 Bit

           1111111111222222222233
 01234567890123456789012345678901
 ffffs
*/

#define SYNC_SHIFT (4)
#define SYNC_MASK  (0x01)

#define FORMAT_MASK (0x0F)

#define PARTIAL_READ_SHIFT (5)

inline
Uint8
FsReadWriteReq::getSyncFlag(const UintR & opFlag){
  return (Uint8)((opFlag >> SYNC_SHIFT) & SYNC_MASK);
}

inline
FsReadWriteReq::NdbfsFormatType
FsReadWriteReq::getFormatFlag(const UintR & opFlag){
  return (NdbfsFormatType)(opFlag & FORMAT_MASK);
}

inline
void 
FsReadWriteReq::setSyncFlag(UintR & opFlag, Uint8 flag){
  ASSERT_BOOL(flag, "FsReadWriteReq::setSyncFlag");
  opFlag |= (flag << SYNC_SHIFT);
}

inline
void 
FsReadWriteReq::setFormatFlag(UintR & opFlag, Uint8 flag){
  ASSERT_MAX(flag, fsFormatMax, "FsReadWriteReq::setSyncFlag");
  opFlag |= flag;
}

inline
void 
FsReadWriteReq::setPartialReadFlag(UintR & opFlag, Uint32 flag){
  ASSERT_BOOL(flag, "FsReadWriteReq::setSyncFlag");
  opFlag |= (flag << PARTIAL_READ_SHIFT);
}

inline
Uint32
FsReadWriteReq::getPartialReadFlag(UintR opFlag){
  return (opFlag >> PARTIAL_READ_SHIFT) & 1;
}

struct FsSuspendOrd
{
  UintR filePointer;          // DATA 0
  Uint32 milliseconds;

  STATIC_CONST(SignalLength = 2);
};


#undef JAM_FILE_ID

#endif
