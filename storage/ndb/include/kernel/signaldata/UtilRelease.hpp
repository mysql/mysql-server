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

#ifndef UTIL_RELEASE_HPP
#define UTIL_RELEASE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 119


/**
 * @class UtilReleaseReq
 * @brief Release Prepared transaction in Util block
 *
 * Data format:
 * - UTIL_PREPARE_RELEASE_REQ <UtilPrepareId>
 */
class UtilReleaseReq {
  friend class DbUtil;
  friend class Trix;
public:
  static constexpr Uint32 SignalLength = 2;

private:  
  Uint32 senderData; // MUST be no 1!
  Uint32 prepareId;
};


/**
 * @class UtilReleaseConf
 *
 * Data format:
 * - UTIL_PREPARE_CONF <UtilPrepareId> 
 */

class UtilReleaseConf {
  friend class DbUtil;
  friend class Trix;

  static constexpr Uint32 SignalLength = 1;

private:
  Uint32 senderData;  // MUST be no 1!
};


/**
 * @class UtilReleaseRef
 *
 * Data format:
 * - UTIL_PREPARE_RELEASE_REF 
 */

class UtilReleaseRef {
  friend class DbUtil;
  friend class Trix;

  enum ErrorCode {
    RELEASE_REF_NO_ERROR = 0,
    NO_SUCH_PREPARE_SEIZED = 1
  };

  static constexpr Uint32 SignalLength = 3;

private:
  Uint32 senderData; // MUST be no 1!
  Uint32 prepareId;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif
