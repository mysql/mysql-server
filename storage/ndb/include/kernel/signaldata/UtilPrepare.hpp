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

#ifndef UTIL_PREPARE_REQ_HPP
#define UTIL_PREPARE_REQ_HPP

#include <SimpleProperties.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 18

/**
 * UTIL_PREPARE_REQ, UTIL_PREPARE_CONF, UTIL_PREPARE_REF
 */

/**
 * @class UtilPrepareReq
 * @brief Prepare transaction in Util block
 *
 * Data format:
 * - UTIL_PREPARE_REQ <NoOfOps> (<OperationType> <TableName> <AttrName>+)+
 */
class UtilPrepareReq {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;
  friend class Trix;
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printUTIL_PREPARE_REQ(FILE *output, const Uint32 *theData,
                                    Uint32 len, Uint16 receiverBlockNo);

 public:
  enum OperationTypeValue {
    Read = 0,
    Update = 1,
    Insert = 2,
    Delete = 3,
    Write = 4,
    Probe = 5  // check existence...
  };

  enum KeyValue {
    NoOfOperations = 1,  ///< No of operations in transaction
    OperationType = 2,   ///
    TableName = 3,       ///< String
    AttributeName = 4,   ///< String
    TableId = 5,
    AttributeId = 6,
    ScanTakeOverInd = 7,
    ReorgInd = 8
  };

  enum Flags { InternalOperation = 1 };

  // Signal constants
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 PROPERTIES_SECTION = 0;
  static constexpr Uint32 NoOfSections = 1;

  GET_SET_SENDERREF
  GET_SET_SENDERDATA
 private:
  Uint32 senderData;  // MUST be no 1!
  Uint32 senderRef;
  Uint32 schemaTransId;
  Uint32 flags;
};

/**
 * @class UtilPrepareConf
 *
 * Data format:
 * - UTIL_PREPARE_CONF <UtilPrepareId>
 */

class UtilPrepareConf {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;
  friend class Trix;

  /**
   * For printing
   */
  friend bool printUTIL_PREPARE_CONF(FILE *output, const Uint32 *theData,
                                     Uint32 len, Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 SignalLength = 2;

  GET_SET_SENDERDATA
  GET_SET_PREPAREID
 private:
  Uint32 senderData;  // MUST be no 1!
  Uint32 prepareId;
};

/**
 * @class UtilPrepareRef
 *
 * Data format:
 * - UTIL_PREPARE_REF
 */

class UtilPrepareRef {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class Dbdict;
  friend class DbUtil;
  friend class Trix;

  /**
   * For printing
   */
  friend bool printUTIL_PREPARE_REF(FILE *output, const Uint32 *theData,
                                    Uint32 len, Uint16 receiverBlockNo);

 public:
  enum ErrorCode {
    PREPARE_REF_NO_ERROR = 0,
    PREPARE_SEIZE_ERROR = 1,
    PREPARE_PAGES_SEIZE_ERROR = 2,
    PREPARED_OPERATION_SEIZE_ERROR = 3,
    DICT_TAB_INFO_ERROR = 4,
    MISSING_PROPERTIES_SECTION = 5
  };

  static constexpr Uint32 SignalLength = 3;

  GET_SET_SENDERDATA
  GET_SET_ERRORCODE
 private:
  Uint32 senderData;  // MUST be no 1!
  Uint32 errorCode;
  Uint32 dictErrCode;  // If errorCode == DICT_TAB_INFO_ERROR
};

#undef JAM_FILE_ID

#endif
