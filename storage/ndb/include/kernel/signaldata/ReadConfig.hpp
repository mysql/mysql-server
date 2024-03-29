/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#ifndef READ_CONFIG_HPP
#define READ_CONFIG_HPP

#include "kernel/signaldata/SignalData.hpp"

#define JAM_FILE_ID 75


/**
 */
class ReadConfigReq {
public:
  static constexpr Uint32 SignalLength = 3;
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfParameters; // 0 Means read all relevant for block
  Uint32 parameters[1];  // see mgmapi_config_parameters.h
};

DECLARE_SIGNAL_SCOPE(GSN_READ_CONFIG_REQ, Local);

class ReadConfigConf {
public:
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderRef;
  Uint32 senderData;
};

DECLARE_SIGNAL_SCOPE(GSN_READ_CONFIG_CONF, Local);

#undef JAM_FILE_ID

#endif
