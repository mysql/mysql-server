/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef FS_REF_H
#define FS_REF_H

#include <ndbd_exit_codes.h>
#include "SignalData.hpp"

#define JAM_FILE_ID 194

/**
 * FsRef - Common signal class for all REF signals sent from Ndbfs
 * GSN_FSCLOSEREF, GSN_FSOPENREF, GSN_FSWRITEREF, GSN_FSREADREF,
 * GSN_FSSYNCREF
 */

/**
 *
 * SENDER:  Ndbfs
 * RECIVER:
 */
struct FsRef {
  friend bool printFSREF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 receiverBlockNo);

  /**
   * Enum type for errorCode
   */
  static constexpr Uint32 FS_ERR_BIT = 0x8000;

  enum NdbfsErrorCodeType {
    fsErrNone = 0,
    fsErrEnvironmentError = NDBD_EXIT_AFS_ENVIRONMENT,
    fsErrTemporaryNotAccessible = NDBD_EXIT_AFS_TEMP_NO_ACCESS,
    fsErrNoSpaceLeftOnDevice = NDBD_EXIT_AFS_DISK_FULL,
    fsErrPermissionDenied = NDBD_EXIT_AFS_PERMISSION_DENIED,
    fsErrInvalidParameters = NDBD_EXIT_AFS_INVALID_PARAM,
    fsErrUnknown = NDBD_EXIT_AFS_UNKNOWN,
    fsErrNoMoreResources = NDBD_EXIT_AFS_NO_MORE_RESOURCES,
    fsErrFileDoesNotExist = NDBD_EXIT_AFS_NO_SUCH_FILE,
    fsErrReadUnderflow = NDBD_EXIT_AFS_READ_UNDERFLOW,
    fsErrFileExists = FS_ERR_BIT | 12,
    fsErrInvalidFileSize = FS_ERR_BIT | 13,
    fsErrOutOfMemory = FS_ERR_BIT | 14,
    fsErrSync = FS_ERR_BIT | 15,
    fsErrMax
  };
  /**
   * Length of signal
   */
  static constexpr Uint32 SignalLength = 4;

  /**
   * DATA VARIABLES
   */
  UintR userPointer;  // DATA 0
  UintR errorCode;    // DATA 1
  UintR osErrorCode;  // DATA 2
  UintR senderData;

  static NdbfsErrorCodeType getErrorCode(const UintR &errorcode);
  static void setErrorCode(UintR &errorcode, NdbfsErrorCodeType errorcodetype);
  static void setErrorCode(UintR &errorcode, UintR errorcodetype);
};

DECLARE_SIGNAL_SCOPE(GSN_FSOPENREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSCLOSEREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSREADREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSWRITEREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSSYNCREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSREMOVEREF, Local);
DECLARE_SIGNAL_SCOPE(GSN_FSAPPENDREF, Local);

inline FsRef::NdbfsErrorCodeType FsRef::getErrorCode(const UintR &errorcode) {
  return (NdbfsErrorCodeType)errorcode;
}

inline void FsRef::setErrorCode(UintR &errorcode,
                                NdbfsErrorCodeType errorcodetype) {
  ASSERT_MAX(errorcodetype, fsErrMax, "FsRef::setErrorCode");
  errorcode = (UintR)errorcodetype;
}

inline void FsRef::setErrorCode(UintR &errorcode, UintR errorcodetype) {
  ASSERT_MAX(errorcodetype, fsErrMax, "FsRef::setErrorCode");
  errorcode = errorcodetype;
}

#undef JAM_FILE_ID

#endif
