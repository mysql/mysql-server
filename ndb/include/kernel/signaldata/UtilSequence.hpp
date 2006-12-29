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

#ifndef UTIL_SEQUENCE_HPP
#define UTIL_SEQUENCE_HPP

#include "SignalData.hpp"

class UtilSequenceReq {
  
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class Backup;
  friend class Suma;

  friend bool printUTIL_SEQUENCE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
  enum RequestType {
    NextVal = 1, // Return uniq value
    CurrVal = 2, // Read
    Create  = 3  // Create a sequence
  };
private:
  Uint32 senderData;  
  Uint32 sequenceId;  // Number of sequence variable
  Uint32 requestType;
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
  STATIC_CONST( SignalLength = 5 );
  
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
  STATIC_CONST( SignalLength = 5 );
  
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

#endif
