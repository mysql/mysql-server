/* Copyright (C) 2008 Sun Microsystems, Inc.

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

#ifndef CONFIG_CHANGE_H
#define CONFIG_CHANGE_H

#include "SignalData.hpp"

class ConfigChangeReq {
  /**
   * Sender
   */
  friend class MgmtSrvr;

  /**
   * Receiver
   */
  friend class ConfigManager;

public:
  STATIC_CONST( SignalLength = 1 );

private:
  Uint32 length; // Length of the config data in long signal
};


class ConfigChangeConf {
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

public:
  STATIC_CONST( SignalLength = 1 );

private:

  Uint32 unused;
};


class ConfigChangeRef {
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;


  enum ErrorCode {
    OK                      = 0,
    ConfigChangeOnGoing     = 1,
    NotMaster               = 2,
    NoConfigData            = 3,
    ConfigChangeAborted     = 4,
    ConfigNotOk             = 5,

    InternalError           = 10,
    PrepareFailed           = 11,
    IllegalConfigChange     = 13,
    FailedToUnpack          = 14,
    InvalidGeneration       = 15,
    InvalidConfigName       = 16,
    IllegalState            = 17,
    IllegalInitialGeneration = 18,
    DifferentInitial        = 19,
    NotAllStarted           = 20,
    NotPrimaryMgmNode       = 21
  } ;

public:
  STATIC_CONST( SignalLength = 1 );

  static const char* errorMessage(Uint32 error) {
    switch (error){
    case NoConfigData:
      return "No config data in signal";
    case ConfigChangeAborted:
      return "Config change was aborted";
    case FailedToUnpack:
      return "Failed to unpack the configuration";
    case IllegalConfigChange:
      return "Illegal config change";

    case InternalError:
      return "ConfigChangeRef, internal error";

    default:
      return "ConfigChangeRef, unknown error";
    }
  }

private:

  Uint32 errorCode;
};


class ConfigChangeImplReq {
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

  enum RequestType {
    Prepare,
    Commit,
    Abort
  };

public:
  STATIC_CONST( SignalLength = 3 );

private:

  Uint32 requestType;
  Uint32 initial; // Valid when requestType = Prepare
  Uint32 length; // Length of the config data in long signal
};


class ConfigChangeImplConf  {
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

public:
  STATIC_CONST( SignalLength = 1 );

private:

  Uint32 requestType;
};


class ConfigChangeImplRef  {
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

public:
  STATIC_CONST( SignalLength = 1 );

private:

  Uint32 errorCode;
};


class ConfigCheckReq  {
  /**
   * Sender
   */
  friend class MgmtSrvr;

  /**
   * Receiver
   */
  friend class ConfigManager;

public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 state;
  Uint32 generation;
};


class ConfigCheckConf {
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

public:
  STATIC_CONST( SignalLength = 2 );

private:

  Uint32 state;
  Uint32 generation;
};


class ConfigCheckRef  {
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

  enum ErrorCode {
    WrongState                 = 1,
    WrongGeneration            = 2
  };

  static const char* errorMessage(Uint32 error) {
    switch (error){
    case WrongState:
      return "Wrong state";
    case WrongGeneration:
      return "Wrong generation";

    default:
      return "ConfigCheckRef, unknown error";
    }
  }

public:
  STATIC_CONST( SignalLength = 5 );
private:
  Uint32 error;
  Uint32 generation;
  Uint32 expected_generation;
  Uint32 state;
  Uint32 expected_state;
};


#endif
