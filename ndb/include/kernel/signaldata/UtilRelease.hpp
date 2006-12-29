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

#ifndef UTIL_RELEASE_HPP
#define UTIL_PREPARE_HPP

#include "SignalData.hpp"

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
  STATIC_CONST( SignalLength = 2 );

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

  STATIC_CONST( SignalLength = 1 );

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
    NO_ERROR = 0,
    NO_SUCH_PREPARE_SEIZED = 1
  };

  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 senderData; // MUST be no 1!
  Uint32 prepareId;
  Uint32 errorCode;
};

#endif
