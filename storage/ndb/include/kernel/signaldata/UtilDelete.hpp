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

#ifndef UTIL_DELETE_HPP
#define UTIL_DELETE_HPP

#include <SimpleProperties.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 124

/**
 * UTIL_DELETE_REQ, UTIL_DELETE_CONF, UTIL_DELETE_REF
 */

/**
 * @class UtilDeleteReq
 * @brief Delete transaction in Util block
 *
 * Data format:
 * - UTIL_DELETE_REQ <prepareId> <ListOfAttributeHeaderValuePairs>
 */

class UtilDeleteReq {
  /** Sender(s) / Receiver(s) */
  friend class DbUtil;

  /** For printing */
  friend bool printUTIL_DELETE_REQ(FILE *output, const Uint32 *theData,
                                   Uint32 len, Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 DataLength = 22;
  static constexpr Uint32 HeaderLength = 3;

 private:
  Uint32 senderData;
  Uint32 prepareId;     // Which prepared transaction to execute
  Uint32 totalDataLen;  // Total length of attrData (including AttributeHeaders
                        // and possibly spanning over multiple signals)

  /**
   * Length in this = signal->length() - 3
   * Sender block ref = signal->senderBlockRef()
   */

  Uint32 attrData[DataLength];
};

/**
 * @class UtilDeleteConf
 *
 * Data format:
 * - UTIL_PREPARE_CONF <UtilPrepareId>
 */

class UtilDeleteConf {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;

  /**
   * For printing
   */
  friend bool printUTIL_DELETE_CONF(FILE *output, const Uint32 *theData,
                                    Uint32 len, Uint16 receiverBlockNo);

  static constexpr Uint32 SignalLength = 1;

 private:
  Uint32 senderData;  ///< The client data provided by the client sending
                      ///< UTIL_DELETE_REQ
};

/**
 * @class UtilDeleteRef
 *
 * Data format:
 * - UTIL_PREPARE_REF
 */

class UtilDeleteRef {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class DbUtil;

  /**
   * For printing
   */
  friend bool printUTIL_DELETE_REF(FILE *output, const Uint32 *theData,
                                   Uint32 len, Uint16 receiverBlockNo);

  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 senderData;
  Uint32 errorCode;  ///< See UtilExecuteRef::errorCode
  Uint32 TCErrorCode;
};

#undef JAM_FILE_ID

#endif
