/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef UTIL_SEQUENCE_HPP
#define UTIL_SEQUENCE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 186


class UtilSequenceReq {
  
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class Backup;
  friend class BackupProxy;
  friend class Suma;

  friend bool printUTIL_SEQUENCE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;
  
  enum RequestType {
    NextVal = 1, // Return uniq value
    CurrVal = 2, // Read
    Create  = 3,  // Create a sequence
    SetVal  = 4  // Set a new sequence
  };
private:
  Uint32 senderData;  
  Uint32 sequenceId;  // Number of sequence variable
  Uint32 requestType;
  Uint32 value;
};

class UtilSequenceConf {
  
  /**
   * Receiver
   */
  friend class Backup;
  friend class Suma;  
  /**
   * Sender
   */
  friend class DbUtil;

  friend bool printUTIL_SEQUENCE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 5;
  
private:
  Uint32 senderData;
  Uint32 sequenceId;
  Uint32 requestType;
  Uint32 sequenceValue[2];
};

class UtilSequenceRef {
  
  /**
   * Reciver
   */
  friend class Backup;
  friend class Suma;
  /**
   * Sender
   */
  friend class DbUtil;
  
  friend bool printUTIL_SEQUENCE_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 5;
  
  enum ErrorCode {
    NoSuchSequence = 1,
    TCError = 2
  };
private:
  Uint32 senderData;
  Uint32 sequenceId;
  Uint32 requestType;
  Uint32 errorCode;
  Uint32 TCErrorCode;
};


#undef JAM_FILE_ID

#endif
