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

#ifndef MANAGEMENTSERVER_HPP
#define MANAGEMENTSERVER_HPP

#include "SignalData.hpp"

/**
 * Request to lock configuration
 */
class MgmLockConfigReq {
  friend class MgmtSrvr;

public:
  STATIC_CONST( SignalLength = 1 );

private:
  Uint32 newConfigGeneration;
};

/**
 * Confirm configuration lock
 */
class MgmLockConfigRep {
  friend class MgmtSrvr;
public:
  STATIC_CONST( SignalLength = 1 );

  /* Error codes */
  enum ErrorCode {
    OK,
    UNKNOWN_ERROR,
    GENERATION_MISMATCH,
    ALREADY_LOCKED
  };

private:
  Uint32 errorCode;
};

/**
 * Unlock configuration
 */
class MgmUnlockConfigReq {
  friend class MgmtSrvr;

public:
  STATIC_CONST( SignalLength = 1 );

private:
  Uint32 commitConfig;
};

/**
 * Confirm config unlock
 */
class MgmUnlockConfigRep {
  friend class MgmtSrvr;
public:
  STATIC_CONST( SignalLength = 1 );

  /* Error codes */
  enum ErrorCode {
    OK,
    UNKNOWN_ERROR,
    NOT_LOCKED
  };

private:
  Uint32 errorCode;
};

#endif /* !MANAGEMENTSERVER_HPP */
