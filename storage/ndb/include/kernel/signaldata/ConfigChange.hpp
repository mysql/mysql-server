/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef CONFIG_CHANGE_H
#define CONFIG_CHANGE_H

#include "SignalData.hpp"

struct ConfigChangeReq
{
  /**
   * Sender
   */
  friend class MgmtSrvr;

  /**
   * Receiver
   */
  friend class ConfigManager;

  STATIC_CONST( SignalLength = 1 );

  Uint32 length; // Length of the config data in long signal
};


struct ConfigChangeConf 
{
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

  STATIC_CONST( SignalLength = 1 );

  Uint32 unused;
};


struct ConfigChangeRef 
{
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
    ConfigNotOk             = 4,
    InternalError           = 5,
    PrepareFailed           = 6,
    IllegalConfigChange     = 7,
    FailedToUnpack          = 8,
    InvalidGeneration       = 9,
    InvalidConfigName       = 10,
    IllegalInitialState     = 11,
    IllegalInitialGeneration = 12,
    DifferentInitial        = 13,
    NotAllStarted           = 14,
    NotPrimaryMgmNode       = 15,
    SendFailed              = 16
  };

  STATIC_CONST( SignalLength = 1 );

  static const char* errorMessage(Uint32 error) {
    switch (error){
    case ConfigChangeOnGoing:
      return "Config change ongoing";
    case NotMaster:
      return "Not the config change master";
    case NoConfigData:
      return "No config data in signal";
    case ConfigNotOk:
      return "Config is not ok";
    case InternalError:
      return "ConfigChangeRef, internal error";
    case PrepareFailed:
      return "Prepare of config change failed";
    case IllegalConfigChange:
      return "Illegal configuration change";
    case FailedToUnpack:
      return "Failed to unpack the configuration";
    case InvalidGeneration:
      return "Invalid generation in configuration";
    case InvalidConfigName:
      return "Invalid configuration name in configuration";
    case IllegalInitialState:
      return "Initial config change not allowed in this state";
    case IllegalInitialGeneration:
      return "Initial config change with generation not 0";
    case DifferentInitial:
      return "Different initial config files";
    case NotAllStarted:
      return " Not all mgm nodes are started";
    case NotPrimaryMgmNode:
      return "Not primary mgm node for configuration";
    case SendFailed:
      return "Failed to send signal to other node";

    default:
      return "ConfigChangeRef, unknown error";
    }
  }

  Uint32 errorCode;
};


struct ConfigChangeImplReq
{
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

  enum RequestType {
    Prepare,
    Commit,
    Abort
  };

  STATIC_CONST( SignalLength = 3 );

  Uint32 requestType;
  Uint32 initial; // Valid when requestType = Prepare
  Uint32 length; // Length of the config data in long signal

};


struct ConfigChangeImplConf
{
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

  STATIC_CONST( SignalLength = 1 );

  Uint32 requestType;
};


struct ConfigChangeImplRef
{
  /**
   * Receiver and sender
   */
  friend class ConfigManager;

  STATIC_CONST( SignalLength = 1 );

  Uint32 errorCode;
};


struct ConfigCheckReq
{
  /**
   * Sender
   */
  friend class MgmtSrvr;

  /**
   * Receiver
   */
  friend class ConfigManager;

  STATIC_CONST( SignalLengthBeforeChecksum = 2 );
  STATIC_CONST( SignalLength = 3 );

  Uint32 state;
  Uint32 generation;
  Uint32 checksum;
};


struct ConfigCheckConf
{
  /**
   * Sender
   */
  friend class ConfigManager;

  /**
   * Receiver
   */
  friend class MgmtSrvr;

  STATIC_CONST( SignalLength = 2 );

  Uint32 state;
  Uint32 generation;
};


struct ConfigCheckRef
{
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
    WrongGeneration            = 2,
    WrongChecksum              = 3
  };

  static const char* errorMessage(Uint32 error) {
    switch (error){
    case WrongState:
      return "Wrong state";
    case WrongGeneration:
      return "Wrong generation";
    case WrongChecksum:
      return "Wrong checksum";

    default:
      return "ConfigCheckRef, unknown error";
    }
  }

  STATIC_CONST( SignalLength = 5 );
  STATIC_CONST( SignalLengthWithConfig = 6 );

  Uint32 error;
  Uint32 generation;
  Uint32 expected_generation;
  Uint32 state;
  Uint32 expected_state;
  Uint32 length; // Length of the config data in long signal
};

#endif
