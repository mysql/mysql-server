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

#ifndef UTIL_EXECUTE_HPP
#define UTIL_EXECUTE_HPP

#include "SignalData.hpp"
#include <SimpleProperties.hpp>

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
  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( HEADER_SECTION = 0 );
  STATIC_CONST( DATA_SECTION = 1 );
  STATIC_CONST( NoOfSections = 2 );

  GET_SET_SENDERREF
  GET_SET_SENDERDATA
  void setPrepareId(Uint32 pId) { prepareId = pId; }; // !! unsets release flag
  Uint32 getPrepareId() const { return prepareId & 0xFF; };
  void setReleaseFlag() { prepareId |= 0x100; };
  bool getReleaseFlag() const { return (prepareId & 0x100) != 0; };
private:
  Uint32 senderData; // MUST be no 1!
  Uint32 senderRef;
  Uint32 prepareId;     // Which prepared transaction to execute
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
  STATIC_CONST( SignalLength = 1 );

  GET_SET_SENDERDATA
private:
  Uint32 senderData; // MUST be no 1!
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
  STATIC_CONST( SignalLength = 3 );

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


#endif
