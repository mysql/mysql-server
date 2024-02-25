/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef UTIL_EXECUTE_HPP
#define UTIL_EXECUTE_HPP

#include "SignalData.hpp"
#include <SimpleProperties.hpp>

#define JAM_FILE_ID 145


/**
 * UTIL_EXECUTE_REQ, UTIL_EXECUTE_CONF, UTIL_EXECUTE_REF
 */

/**
 * @class UtilExecuteReq
 * @brief Execute transaction in Util block
 *
 * Data format:
 * - UTIL_EXECUTE_REQ <prepareId> <ListOfAttributeHeaderValuePairs>
 */

class UtilExecuteReq {
  /** Sender(s) / Receiver(s) */
  friend class DbUtil;
  friend class Trix;

  /** For printing */
  friend bool printUTIL_EXECUTE_REQ(FILE * output, const Uint32 * theData, 
				    Uint32 len, Uint16 receiverBlockNo);
public:
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 HEADER_SECTION = 0;
  static constexpr Uint32 DATA_SECTION = 1;
  static constexpr Uint32 NoOfSections = 2;

  GET_SET_SENDERREF
  GET_SET_SENDERDATA
  void setPrepareId(Uint32 pId) { prepareId = pId; } // !! unsets release flag
  Uint32 getPrepareId() const { return prepareId & 0xFF; }
  void setReleaseFlag() { prepareId |= 0x100; }
  bool getReleaseFlag() const { return (prepareId & 0x100) != 0; }

  Uint32 senderData; // MUST be no 1!
  Uint32 senderRef;
  Uint32 prepareId;     // Which prepared transaction to execute
  Uint32 scanTakeOver;
};

/**
 * @class UtilExecuteConf
 *
 * Data format:
 * - UTIL_PREPARE_CONF <UtilPrepareId> 
 */

class UtilExecuteConf {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;
  friend class Trix;

  /**
   * For printing
   */
  friend bool printUTIL_EXECUTE_CONF(FILE * output, 
				     const Uint32 * theData, 
				     Uint32 len, 
				     Uint16 receiverBlockNo);
public:
  static constexpr Uint32 SignalLength = 3;

  GET_SET_SENDERDATA
private:
  Uint32 senderData; // MUST be no 1!
  Uint32 gci_hi;
  Uint32 gci_lo;
};


/**
 * @class UtilExecuteRef
 *
 * Data format:
 * - UTIL_PREPARE_REF 
 */

class UtilExecuteRef {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;
  friend class Trix;

  /**
   * For printing
   */
  friend bool printUTIL_EXECUTE_REF(FILE * output, 
				    const Uint32 * theData, 
				    Uint32 len, 
				    Uint16 receiverBlockNo);

public:
  static constexpr Uint32 SignalLength = 3;

  enum ErrorCode {
    IllegalKeyNumber = 1,
    IllegalAttrNumber = 2,
    TCError = 3,
    AllocationError = 5,
    MissingDataSection = 6,
    MissingData = 7
  };

  GET_SET_SENDERDATA
  GET_SET_ERRORCODE
  GET_SET_TCERRORCODE
private:
  Uint32 senderData; // MUST be no 1!
  Uint32 errorCode;
  Uint32 TCErrorCode;
};



#undef JAM_FILE_ID

#endif
